/*
 *   Copyright (C) 2011-2016 by Jonathan Naylor G4KLX
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "DStarRepeaterStatusData.h"
#include "DStarRepeaterTXRXThread.h"
#include "DVAPController.h"
#include "DStarDefines.h"
#include "HeaderData.h"
#include "Version.h"
#include "Logger.h"
#include "Utils.h"

#include <chrono>

using namespace std::chrono;

const unsigned int MAX_DATA_SYNC_BIT_ERRS  = 2U;

const unsigned int NETWORK_QUEUE_COUNT = 2U;

const unsigned int SILENCE_THRESHOLD = 2U;

const unsigned int CYCLE_TIME = 9U;

CDStarRepeaterTXRXThread::CDStarRepeaterTXRXThread(const std::string& type) :
m_type(type),
m_modem(nullptr),
m_protocolHandler(nullptr),
m_controller(nullptr),
m_rptCallsign(),
m_rxHeader(nullptr),
m_txHeader(nullptr),
m_networkQueue(nullptr),
m_writeNum(0U),
m_readNum(0U),
m_radioSeqNo(0U),
m_networkSeqNo(0U),
m_watchdogTimer(1000U, NETWORK_TIMEOUT),
m_registerTimer(1000U),
m_statusTimer(1000U, 0U, 100U),		// 100ms
m_heartbeatTimer(1000U, 1U),		// 1s
m_rptState(DSRS_LISTENING),
m_rxState(DSRXS_LISTENING),
m_slowDataDecoder(),
m_tx(false),
m_transmitting(false),
m_space(0U),
m_killed(false),
m_activeHangTimer(1000U),
m_disable(false),
m_lastData(nullptr),
m_ambe(),
m_ambeFrames(0U),
m_ambeSilence(0U),
m_ambeBits(1U),
m_ambeErrors(0U),
m_lastAMBEBits(0U),
m_lastAMBEErrors(0U),
m_headerTime(),
m_packetTime(),
m_packetCount(0U),
m_packetSilence(0U)
#if defined(MQTT)
,m_mqttStatusTimer(1000U, 1U)		// 1s
#endif
{
	m_networkQueue = new COutputQueue*[NETWORK_QUEUE_COUNT];
	for (unsigned int i = 0U; i < NETWORK_QUEUE_COUNT; i++)
		m_networkQueue[i] = new COutputQueue((DV_FRAME_LENGTH_BYTES + 2U) * 200U, NETWORK_RUN_FRAME_COUNT);		// 4s worth of data);

	m_lastData = new unsigned char[DV_FRAME_MAX_LENGTH_BYTES];

	setRepeaterState(DSRS_LISTENING);
	setRadioState(DSRXS_LISTENING);
}

CDStarRepeaterTXRXThread::~CDStarRepeaterTXRXThread()
{
	for (unsigned int i = 0U; i < NETWORK_QUEUE_COUNT; i++)
		delete m_networkQueue[i];
	delete[] m_networkQueue;
	delete[] m_lastData;
	delete   m_rxHeader;
	delete   m_txHeader;
	delete   m_modem;
	delete   m_controller;
	delete   m_protocolHandler;
}

// ---------------------------------------------------------------------------
// entry() — split-site TX+RX thread body.
//
// Requires modem, controller, protocol handler, and callsign before starting.
// Main loop (~9 ms):
//   1. Refresh modem space/TX state every 100 ms.
//   2. receiveModem()        — forward RF audio to the network (no local retransmit).
//   3. receiveNetwork()      — queue gateway audio for RF transmit.
//   4. repeaterStateMachine()— network watchdog timeout → force EOT.
//   5. Gateway register timer (keepalive every 30 s).
//   6. Heartbeat output to the controller (1 s).
//   7. MQTT status (1 s).
//   8. Active-indicator: follows m_tx or the active-hang timer.
//   9. Disable/shutdown handling (hardware disable line only; no software shutdown).
//  10. Drain the network queue to the modem.
// ---------------------------------------------------------------------------
void CDStarRepeaterTXRXThread::entry()
{
	// Wait here until we have the essentials to run
	while (!m_killed && (m_modem == nullptr  || m_controller == nullptr || m_protocolHandler == nullptr || m_rptCallsign.empty() || m_rptCallsign == "        "))
		std::this_thread::sleep_for(std::chrono::milliseconds(500));		// 1/2 sec

	if (m_killed)
		return;

	m_controller->setActive(false);
	m_controller->setRadioTransmit(false);

	m_heartbeatTimer.start();
	m_statusTimer.start();
	m_registerTimer.start(10U);
#if defined(MQTT)
	m_mqttStatusTimer.start();
#endif

	wxLogMessage("Starting the D-Star transmitter and receiver thread");

	auto stopWatch = steady_clock::now();

	try {
		while (!m_killed) {
			stopWatch = steady_clock::now();

			if (m_statusTimer.hasExpired() || m_space == 0U) {
				m_space = m_modem->getSpace();
				m_tx    = m_modem->isTX();
				m_statusTimer.start();
			}

			receiveModem();

			receiveNetwork();

			repeaterStateMachine();

			// Send the register packet if needed and restart the timer
			if (m_registerTimer.hasExpired()) {
				m_protocolHandler->writeRegister();
				m_registerTimer.start(30U);
			}

			// Clock the heartbeat output every one second
			if (m_heartbeatTimer.hasExpired()) {
				m_controller->setHeartbeat();
				m_heartbeatTimer.start();
			}

#if defined(MQTT)
			// Publish status to MQTT every second
			if (m_mqttStatusTimer.hasExpired()) {
				if (g_mqtt != nullptr) {
					CDStarRepeaterStatusData* status = getStatus();
					std::string json = status->toJSON();
					g_mqtt->publish("status", json.c_str());
					delete status;

					// Publish BER for Display-Driver during active RF
					if (m_rptState == DSRS_VALID && m_ambeBits > 0U) {
						float ber = float(m_ambeErrors * 100U) / float(m_ambeBits);
						mqttPublishBER(ber);
					}
				}
				m_mqttStatusTimer.start();
			}
#endif

			// Set the output state
			if (m_tx || (m_activeHangTimer.isRunning() && !m_activeHangTimer.hasExpired())) {
				m_controller->setActive(true);
			} else {
				m_controller->setActive(false);
				m_activeHangTimer.stop();
			}

			// Check the shutdown state, state changes are done here to bypass the state machine which is
			// frozen when m_disable is asserted
			m_disable = m_controller->getDisable();
			if (m_disable) {
				if (m_rptState != DSRS_SHUTDOWN) {
					m_watchdogTimer.stop();
					m_activeHangTimer.stop();
					for (unsigned int i = 0U; i < NETWORK_QUEUE_COUNT; i++)
						m_networkQueue[i]->reset();
					m_controller->setActive(false);
					m_controller->setRadioTransmit(false);
					m_rptState = DSRS_SHUTDOWN;
					m_transmitting = false;
				}
			} else {
				if (m_rptState == DSRS_SHUTDOWN) {
					m_watchdogTimer.stop();
					m_rptState = DSRS_LISTENING;
					m_protocolHandler->reset();
					m_transmitting = false;
				}
			}

			if (m_networkQueue[m_readNum]->dataReady())
				transmitNetworkData();
			else if (m_networkQueue[m_readNum]->headerReady())
				transmitNetworkHeader();

			m_controller->setRadioTransmit(m_tx);

			long long ms = duration_cast<milliseconds>(steady_clock::now() - stopWatch).count();
			if (ms < CYCLE_TIME) {
				std::this_thread::sleep_for(std::chrono::milliseconds(CYCLE_TIME - ms));
				clock(CYCLE_TIME);
			} else {
				clock((unsigned int)ms);
			}
		}
	}
	catch (std::exception& e) {
		wxLogError("Exception raised - \"%s\"", e.what());
	}
	catch (...) {
		wxLogError("Unknown exception raised");
	}

	wxLogMessage("Stopping the D-Star transmitter and receiver thread");

	m_modem->stop();
	delete m_modem;
	m_modem = nullptr;

	m_controller->setActive(false);
	m_controller->setRadioTransmit(false);
	m_controller->close();
	delete m_controller;
	m_controller = nullptr;

	m_protocolHandler->close();
	delete m_protocolHandler;
	m_protocolHandler = nullptr;
}

void CDStarRepeaterTXRXThread::kill()
{
	m_killed = true;
}

void CDStarRepeaterTXRXThread::setCallsign(const std::string& callsign, const std::string&, DSTAR_MODE, ACK_TYPE, bool, bool, bool, bool)
{
	// Pad the callsign up to eight characters
	m_rptCallsign = callsign;
	m_rptCallsign.resize(LONG_CALLSIGN_LENGTH, ' ');
}

void CDStarRepeaterTXRXThread::setProtocolHandler(CRepeaterProtocolHandler* handler, bool local)
{
	assert(handler != nullptr);

	m_protocolHandler = handler;

	if (local) {
		wxLogInfo("Reducing transmit buffering because of local connection");

		for (unsigned int i = 0U; i < NETWORK_QUEUE_COUNT; i++)
			m_networkQueue[i]->setThreshold(LOCAL_RUN_FRAME_COUNT);
	}
}

void CDStarRepeaterTXRXThread::setModem(CModem* modem)
{
	assert(modem != nullptr);

	m_modem = modem;
}

void CDStarRepeaterTXRXThread::setTimes(unsigned int, unsigned int)
{
}

void CDStarRepeaterTXRXThread::setBeacon(unsigned int, const std::string&, bool, TEXT_LANG)
{
}

void CDStarRepeaterTXRXThread::setAnnouncement(bool, unsigned int, const std::string&, const std::string&, const std::string&, const std::string&)
{
}

void CDStarRepeaterTXRXThread::setController(CExternalController* controller, unsigned int activeHangTime)
{
	assert(controller != nullptr);

	m_controller = controller;
	m_activeHangTimer.setTimeout(activeHangTime);
}

void CDStarRepeaterTXRXThread::setOutputs(bool, bool, bool, bool)
{
}

void CDStarRepeaterTXRXThread::setLogging(bool, const std::string&)
{
}

void CDStarRepeaterTXRXThread::setWhiteList(CCallsignList* list) { delete list; }

void CDStarRepeaterTXRXThread::setBlackList(CCallsignList* list) { delete list; }

void CDStarRepeaterTXRXThread::setGreyList(CCallsignList* list) { delete list; }

void CDStarRepeaterTXRXThread::receiveModem()
{
	for (;;) {
		DSMT_TYPE type = m_modem->read();
		if (type == DSMTT_NONE)
			return;

		switch (m_rxState) {
			case DSRXS_LISTENING:
				if (type == DSMTT_HEADER) {
					CHeaderData* header = m_modem->readHeader();
					receiveHeader(header);
				} else if (type == DSMTT_DATA) {
					unsigned char data[20U];
					unsigned int length = m_modem->readData(data, 20U);
					setRadioState(DSRXS_PROCESS_SLOW_DATA);
					receiveSlowData(data, length);
				}
				break;

			case DSRXS_PROCESS_SLOW_DATA:
				if (type == DSMTT_DATA) {
					unsigned char data[20U];
					unsigned int length = m_modem->readData(data, 20U);
					receiveSlowData(data, length);
				} else if (type == DSMTT_EOT || type == DSMTT_LOST) {
					setRadioState(DSRXS_LISTENING);
				}
				break;

			case DSRXS_PROCESS_DATA:
				if (type == DSMTT_DATA) {
					unsigned char data[20U];
					unsigned int length = m_modem->readData(data, 20U);
					receiveRadioData(data, length);
				} else if (type == DSMTT_EOT || type == DSMTT_LOST) {
					unsigned char data[20U];
					::memcpy(data, END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES);
					processRadioFrame(data, FRAME_END);
					setRadioState(DSRXS_LISTENING);
#if defined(MQTT)
					mqttPublishDStarEnd();
					mqttPublishIdle();
#endif
					endOfRadioData();
				}
				break;
		}
	}
}

void CDStarRepeaterTXRXThread::receiveHeader(CHeaderData* header)
{
	assert(header != nullptr);

	wxLogMessage("Radio header decoded - My: %s/%s  Your: %s  Rpt1: %s  Rpt2: %s  Flags: %02X %02X %02X", header->getMyCall1().c_str(), header->getMyCall2().c_str(), header->getYourCall().c_str(), header->getRptCall1().c_str(), header->getRptCall2().c_str(), header->getFlag1(), header->getFlag2(), header->getFlag3());

	bool res = processRadioHeader(header);
	if (res) {
		// A valid header and is a DV packet
		m_radioSeqNo = 20U;
		setRadioState(DSRXS_PROCESS_DATA);
	} else {
		// This is a DD packet or some other problem
		// wxLogMessage("Invalid header");
	}
}

void CDStarRepeaterTXRXThread::receiveSlowData(unsigned char* data, unsigned int)
{
	unsigned int errs;
	errs  = countBits(data[VOICE_FRAME_LENGTH_BYTES + 0U] ^ DATA_SYNC_BYTES[0U]);
	errs += countBits(data[VOICE_FRAME_LENGTH_BYTES + 1U] ^ DATA_SYNC_BYTES[1U]);
	errs += countBits(data[VOICE_FRAME_LENGTH_BYTES + 2U] ^ DATA_SYNC_BYTES[2U]);

	// The data sync has been seen, a fuzzy match is used, two bit errors or less
	if (errs <= MAX_DATA_SYNC_BIT_ERRS) {
		// wxLogMessage("Found data sync at frame %u, errs: %u", m_radioSeqNo, errs);
		m_radioSeqNo     = 0U;
		m_slowDataDecoder.sync();
	} else if (m_radioSeqNo == 20U) {
		// wxLogMessage("Assuming data sync");
		m_radioSeqNo = 0U;
		m_slowDataDecoder.sync();
	} else {
		m_radioSeqNo++;
		m_slowDataDecoder.addData(data + VOICE_FRAME_LENGTH_BYTES);

		CHeaderData* header = m_slowDataDecoder.getHeaderData();
		if (header == nullptr)
			return;

		wxLogMessage("Radio header from slow data - My: %s/%s  Your: %s  Rpt1: %s  Rpt2: %s  Flags: %02X %02X %02X  BER: 0%%", header->getMyCall1().c_str(), header->getMyCall2().c_str(), header->getYourCall().c_str(), header->getRptCall1().c_str(), header->getRptCall2().c_str(), header->getFlag1(), header->getFlag2(), header->getFlag3());

		bool res = processRadioHeader(header);
		if (res) {
			// A valid header and is a DV packet, go to normal data relaying
			setRadioState(DSRXS_PROCESS_DATA);
		} else {
			// This is a DD packet or some other problem
			// wxLogMessage("Invalid header");
		}
	}
}

void CDStarRepeaterTXRXThread::receiveRadioData(unsigned char* data, unsigned int)
{
	unsigned int errs;
	errs  = countBits(data[VOICE_FRAME_LENGTH_BYTES + 0U] ^ DATA_SYNC_BYTES[0U]);
	errs += countBits(data[VOICE_FRAME_LENGTH_BYTES + 1U] ^ DATA_SYNC_BYTES[1U]);
	errs += countBits(data[VOICE_FRAME_LENGTH_BYTES + 2U] ^ DATA_SYNC_BYTES[2U]);

	// The data sync has been seen, a fuzzy match is used, two bit errors or less
	if (errs <= MAX_DATA_SYNC_BIT_ERRS) {
		// wxLogMessage("Found data sync at frame %u, errs: %u", m_radioSeqNo, errs);
		m_radioSeqNo = 0U;
		processRadioFrame(data, FRAME_SYNC);
	} else if (m_radioSeqNo == 20U) {
		// wxLogMessage("Regenerating data sync");
		m_radioSeqNo = 0U;
		processRadioFrame(data, FRAME_SYNC);
	} else {
		m_radioSeqNo++;
		processRadioFrame(data, FRAME_NORMAL);
	}
}

void CDStarRepeaterTXRXThread::receiveNetwork()
{
	NETWORK_TYPE type;

	for (;;) {
		type = m_protocolHandler->read();

		// Get the data from the network
		if (type == NETWORK_NONE) {					// Nothing received
			break;
		} else if (type == NETWORK_HEADER) {		// A header
			CHeaderData* header = m_protocolHandler->readHeader();
			if (header != nullptr) {
				::memcpy(m_lastData, NULL_FRAME_DATA_BYTES, DV_FRAME_LENGTH_BYTES);

				processNetworkHeader(header);

				m_headerTime = steady_clock::now();
				m_packetTime = steady_clock::now();
				m_packetCount   = 0U;
				m_packetSilence = 0U;
			}
		} else if (type == NETWORK_DATA) {			// AMBE data and slow data
			unsigned char data[2U * DV_FRAME_MAX_LENGTH_BYTES];
			::memset(data, 0x00U, 2U * DV_FRAME_MAX_LENGTH_BYTES);

			unsigned char seqNo;
			unsigned int length = m_protocolHandler->readData(data, DV_FRAME_MAX_LENGTH_BYTES, seqNo);
			if (length != 0U && m_transmitting) {
				::memcpy(m_lastData, data, length);
				m_watchdogTimer.start();
				m_packetCount += processNetworkFrame(data, length, seqNo);
			}
		}
	}

	// Have we missed any data frames?
	long long packetMs = duration_cast<milliseconds>(steady_clock::now() - m_packetTime).count();
	if (m_transmitting && packetMs > 200L) {
		long long headerMs = duration_cast<milliseconds>(steady_clock::now() - m_headerTime).count();
		unsigned int packetsNeeded = (unsigned int)(headerMs / DSTAR_FRAME_TIME_MS);

		if (packetsNeeded > m_packetCount) {
			unsigned int count = packetsNeeded - m_packetCount;

			if (count > 5U) {
				count -= 2U;

				// Create silence frames
				for (unsigned int i = 0U; i < count; i++) {
					unsigned char data[DV_FRAME_LENGTH_BYTES];
					::memcpy(data, NULL_FRAME_DATA_BYTES, DV_FRAME_LENGTH_BYTES);
					m_packetCount += processNetworkFrame(data, DV_FRAME_LENGTH_BYTES, m_networkSeqNo);
					m_packetSilence++;
				}
			}
		}

		m_packetTime = steady_clock::now();
	}
}

void CDStarRepeaterTXRXThread::transmitNetworkHeader(const CHeaderData& header)
{
	wxLogMessage("Transmitting to - My: %s/%s  Your: %s  Rpt1: %s  Rpt2: %s  Flags: %02X %02X %02X", header.getMyCall1().c_str(), header.getMyCall2().c_str(), header.getYourCall().c_str(), header.getRptCall1().c_str(), header.getRptCall2().c_str(), header.getFlag1(), header.getFlag2(), header.getFlag3());

	bool empty = m_networkQueue[m_readNum]->isEmpty();
	if (!empty) {
		bool headerReady = m_networkQueue[m_readNum]->headerReady();
		if (headerReady) {
			// Transmission has never started, so just purge the queue
			m_networkQueue[m_readNum]->reset();

			m_readNum++;
			if (m_readNum >= NETWORK_QUEUE_COUNT)
				m_readNum = 0U;
		} else {
			// Append an end of stream
			m_networkQueue[m_readNum]->reset();
			m_networkQueue[m_readNum]->addData(END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES, true);
		}
	}

	m_networkQueue[m_writeNum]->reset();
	m_networkQueue[m_writeNum]->setHeader(new CHeaderData(header));
}

void CDStarRepeaterTXRXThread::transmitNetworkHeader()
{
	// Don't send a header until the modem is ready
	bool ready = m_modem->isTXReady();
	if (!ready)
		return;

	CHeaderData* header = m_networkQueue[m_readNum]->getHeader();
	if (header == nullptr)
		return;

	m_modem->writeHeader(*header);
	delete header;
}

void CDStarRepeaterTXRXThread::transmitNetworkData()
{
	if (m_space == 0U)
		return;

	unsigned char buffer[DV_FRAME_LENGTH_BYTES];
	bool end;
	unsigned int length = m_networkQueue[m_readNum]->getData(buffer, DV_FRAME_LENGTH_BYTES, end);

	if (length == 0U)
		return;

	m_modem->writeData(buffer, length, end);
	m_space--;

	if (end) {
		m_networkQueue[m_readNum]->reset();

		m_readNum++;
		if (m_readNum >= NETWORK_QUEUE_COUNT)
			m_readNum = 0U;
	}
}

void CDStarRepeaterTXRXThread::repeaterStateMachine()
{
	if (m_watchdogTimer.isRunning() && m_watchdogTimer.hasExpired()) {
		wxLogMessage("Network watchdog has expired");
		// Send end of transmission data to the radio
		m_networkQueue[m_writeNum]->addData(END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES, true);
#if defined(MQTT)
		mqttPublishDStarLost();
		mqttPublishIdle();
#endif
		endOfNetworkData();
	}
}

void CDStarRepeaterTXRXThread::setRadioState(DSTAR_RX_STATE state)
{
	// This is the to state
	switch (state) {
		case DSRXS_LISTENING:
			m_rxState = DSRXS_LISTENING;
			break;

		case DSRXS_PROCESS_DATA:
			m_ambeFrames     = 0U;
			m_ambeSilence    = 0U;
			m_ambeBits       = 1U;
			m_ambeErrors     = 0U;
			m_lastAMBEBits   = 0U;
			m_lastAMBEErrors = 0U;
			m_rxState        = DSRXS_PROCESS_DATA;
			break;

		case DSRXS_PROCESS_SLOW_DATA:
			m_radioSeqNo = 0U;
			m_slowDataDecoder.reset();
			m_rxState = DSRXS_PROCESS_SLOW_DATA;
			break;
	}
}

bool CDStarRepeaterTXRXThread::setRepeaterState(DSTAR_RPT_STATE state)
{
	// Can't change state when shutdown
	if (m_disable)
		return false;

	// The "to" state
	switch (state) {
		case DSRS_SHUTDOWN:
			m_watchdogTimer.stop();
			m_activeHangTimer.stop();
			m_controller->setActive(false);
			m_controller->setRadioTransmit(false);
			m_rptState = DSRS_SHUTDOWN;
			m_transmitting = false;
			break;

		case DSRS_LISTENING:
			m_rptState = DSRS_LISTENING;
			break;

		case DSRS_VALID:
			if (m_rptState != DSRS_LISTENING)
				return false;

			m_rptState = DSRS_VALID;
			break;

		default:
			break;
	}

	return true;
}

bool CDStarRepeaterTXRXThread::processRadioHeader(CHeaderData* header)
{
	assert(header != nullptr);

	// We don't handle DD data packets
	if (header->isDataPacket()) {
		wxLogMessage("Received a DD packet, ignoring");
		delete header;
		return false;
	}

	setRepeaterState(DSRS_VALID);

	if (m_rptState == DSRS_VALID) {
		// Send the valid header to the gateway if we are accepted
		delete m_rxHeader;
		m_rxHeader = header;

#if defined(MQTT)
		mqttPublishDStarStart(m_rxHeader->getMyCall1(), m_rxHeader->getMyCall2(),
			m_rxHeader->getYourCall(), m_rxHeader->getRptCall2(), "rf");
#endif

		CHeaderData netHeader(*m_rxHeader);
		netHeader.setRptCall1(m_rxHeader->getRptCall2());
		netHeader.setRptCall2(m_rxHeader->getRptCall1());

		m_protocolHandler->writeHeader(netHeader);
	} else {
		delete header;
	}

	return true;
}

void CDStarRepeaterTXRXThread::processNetworkHeader(CHeaderData* header)
{
	assert(header != nullptr);

	// If shutdown we ignore incoming headers
	if (m_rptState == DSRS_SHUTDOWN) {
		delete header;
		return;
	}

	wxLogMessage("Network header received - My: %s/%s  Your: %s  Rpt1: %s  Rpt2: %s  Flags: %02X %02X %02X", header->getMyCall1().c_str(), header->getMyCall2().c_str(), header->getYourCall().c_str(), header->getRptCall1().c_str(), header->getRptCall2().c_str(), header->getFlag1(), header->getFlag2(), header->getFlag3());

	// Is it for us?
	if (header->getRptCall2() != m_rptCallsign) {
		wxLogMessage("Invalid network RPT2 value, ignoring");
		delete header;
		return;
	}

	delete m_txHeader;
	m_txHeader = header;

#if defined(MQTT)
	mqttPublishDStarStart(m_txHeader->getMyCall1(), m_txHeader->getMyCall2(),
		m_txHeader->getYourCall(), m_txHeader->getRptCall2(), "net");
#endif

	m_networkSeqNo = 0U;
	m_transmitting = true;
	m_watchdogTimer.start();
	m_activeHangTimer.stop();

	transmitNetworkHeader(*header);
}

void CDStarRepeaterTXRXThread::processRadioFrame(unsigned char* data, FRAME_TYPE type)
{
	m_ambeFrames++;

	// If a sync frame, regenerate the sync bytes
	if (type == FRAME_SYNC)
		::memcpy(data + VOICE_FRAME_LENGTH_BYTES, DATA_SYNC_BYTES, DATA_FRAME_LENGTH_BYTES);

	// Only regenerate the AMBE on received radio data
	unsigned int errors = 0U;
	if (type != FRAME_END) {
		// Data packets have no AMBE FEC
		if (!m_rxHeader->isDataPacket())
			errors = m_ambe.count(data);

		m_ambeErrors += errors;
		m_ambeBits   += 48U;		// Only count the bits with FEC added
	}

	if (::memcmp(data, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES) == 0)
		m_ambeSilence++;

	// Don't pass through the frame of an invalid transmission
	if (m_rptState != DSRS_VALID)
		return;

	if (type == FRAME_END) {
		// Send null data and the end marker over the network, and the statistics
		unsigned char bytes[DV_FRAME_MAX_LENGTH_BYTES];
		::memcpy(bytes, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
		::memcpy(bytes + VOICE_FRAME_LENGTH_BYTES, END_PATTERN_BYTES, END_PATTERN_LENGTH_BYTES);
		m_protocolHandler->writeData(bytes, DV_FRAME_MAX_LENGTH_BYTES, 0U, true);
	} else {
		// Send the data to the network
		m_protocolHandler->writeData(data, DV_FRAME_LENGTH_BYTES, errors, false);
	}
}

unsigned int CDStarRepeaterTXRXThread::processNetworkFrame(unsigned char* data, unsigned int length, unsigned char seqNo)
{
	assert(data != nullptr);
	assert(length > 0U);

	// If shutdown we ignore incoming data
	if (m_rptState == DSRS_SHUTDOWN)
		return 1U;

	bool end = (seqNo & 0x40U) == 0x40U;
	if (end) {
		m_networkQueue[m_writeNum]->addData(END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES, true);
#if defined(MQTT)
		mqttPublishDStarEnd();
		mqttPublishIdle();
#endif
		endOfNetworkData();
		return 1U;
	}

	// Mask out the control bits of the sequence number
	seqNo &= 0x1FU;

	// Count the number of silence frames to insert
	unsigned int tempSeqNo = m_networkSeqNo;
	unsigned int count = 0U;
	while (seqNo != tempSeqNo) {
		count++;

		tempSeqNo++;
		if (tempSeqNo >= 21U)
			tempSeqNo = 0U;
	}

	// If the number is too high, then it probably means an old out-of-order frame, ignore it
	if (count > 18U)
		return 0U;

	unsigned int packetCount = 0U;

	// Insert missing frames
	while (seqNo != m_networkSeqNo) {
		unsigned char buffer[DV_FRAME_LENGTH_BYTES];
		if (count > SILENCE_THRESHOLD) {
			::memcpy(buffer, NULL_FRAME_DATA_BYTES, DV_FRAME_LENGTH_BYTES);
		} else {
			::memcpy(buffer, m_lastData, DV_FRAME_LENGTH_BYTES);
			// Data packets have no AMBE FEC
			if (!m_txHeader->isDataPacket())
				m_ambe.regenerate(buffer);
		}

		if (m_networkSeqNo == 0U)
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, DATA_SYNC_BYTES, DATA_FRAME_LENGTH_BYTES);

		m_networkQueue[m_writeNum]->addData(buffer, DV_FRAME_LENGTH_BYTES, false);

		packetCount++;
		m_networkSeqNo++;
		m_packetSilence++;
		if (m_networkSeqNo >= 21U)
			m_networkSeqNo = 0U;
	}

	// Regenerate the sync bytes
	if (m_networkSeqNo == 0U)
		::memcpy(data + VOICE_FRAME_LENGTH_BYTES, DATA_SYNC_BYTES, DATA_FRAME_LENGTH_BYTES);

	packetCount++;
	m_networkSeqNo++;
	if (m_networkSeqNo >= 21U)
		m_networkSeqNo = 0U;

	// Data packets have no AMBE FEC
	if (!m_txHeader->isDataPacket())
		m_ambe.regenerate(data);

	m_networkQueue[m_writeNum]->addData(data, DV_FRAME_LENGTH_BYTES, false);

	return packetCount;
}

void CDStarRepeaterTXRXThread::endOfRadioData()
{
	wxLogMessage("AMBE for %s  Frames: %.1fs, Silence: %.1f%%, BER: %.1f%%", m_rxHeader->getMyCall1().c_str(), float(m_ambeFrames) / 50.0F, float(m_ambeSilence * 100U) / float(m_ambeFrames), float(m_ambeErrors * 100U) / float(m_ambeBits));

	setRepeaterState(DSRS_LISTENING);
}

void CDStarRepeaterTXRXThread::endOfNetworkData()
{
	float loss = 0.0F;
	if (m_packetCount != 0U)
		loss = float(m_packetSilence) / float(m_packetCount);

	wxLogMessage("Stats for %s  Frames: %.1fs, Loss: %.1f%%, Packets: %u/%u", m_txHeader->getMyCall1().c_str(), float(m_packetCount) / 50.0F, loss * 100.0F, m_packetSilence, m_packetCount);

	m_watchdogTimer.stop();
	m_activeHangTimer.start();
	m_protocolHandler->reset();
	m_transmitting = false;

	m_writeNum++;
	if (m_writeNum >= NETWORK_QUEUE_COUNT)
		m_writeNum = 0U;
}

CDStarRepeaterStatusData* CDStarRepeaterTXRXThread::getStatus()
{
	float   bits = float(m_ambeBits - m_lastAMBEBits);
	float errors = float(m_ambeErrors - m_lastAMBEErrors);
	if (bits == 0.0F)
		bits = 1.0F;

	m_lastAMBEBits   = m_ambeBits;
	m_lastAMBEErrors = m_ambeErrors;

	CDStarRepeaterStatusData* status;
	if (m_rptState == DSRS_SHUTDOWN || m_rptState == DSRS_LISTENING)
		status = new CDStarRepeaterStatusData(std::string(), std::string(), std::string(), std::string(),
					std::string(), 0x00, 0x00, 0x00, m_tx, m_rxState, m_rptState, 0U, 0U, 0U, 0U, 0U, 0U, 0.0F,
					std::string(), std::string(), std::string(), std::string(), std::string(), std::string());
	else
		status = new CDStarRepeaterStatusData(m_rxHeader->getMyCall1(), m_rxHeader->getMyCall2(),
					m_rxHeader->getYourCall(), m_rxHeader->getRptCall1(), m_rxHeader->getRptCall2(),
					m_rxHeader->getFlag1(), m_rxHeader->getFlag2(), m_rxHeader->getFlag3(), m_tx, m_rxState,
					m_rptState, 0U, 0U, 0U, 0U, 0U, 0U, (errors * 100.0F) / bits, std::string(), std::string(),
					std::string(), std::string(), std::string(), std::string());

	if (m_type == "DVAP" && m_modem != nullptr) {
		CDVAPController* dvap = static_cast<CDVAPController*>(m_modem);
		bool squelch = dvap->getSquelch();
		int signal   = dvap->getSignal();
		status->setDVAP(squelch, signal);
	}

	return status;
}

void CDStarRepeaterTXRXThread::clock(unsigned int ms)
{
	m_registerTimer.clock(ms);
	m_watchdogTimer.clock(ms);
	m_activeHangTimer.clock(ms);
	m_statusTimer.clock(ms);
	m_heartbeatTimer.clock(ms);
#if defined(MQTT)
	m_mqttStatusTimer.clock(ms);
#endif
}

void CDStarRepeaterTXRXThread::shutdown()
{
}

void CDStarRepeaterTXRXThread::startup()
{
}

unsigned int CDStarRepeaterTXRXThread::countBits(unsigned char byte)
{
	unsigned int bits = 0U;

	if ((byte & 0x01U) == 0x01U)
		bits++;
	if ((byte & 0x02U) == 0x02U)
		bits++;
	if ((byte & 0x04U) == 0x04U)
		bits++;
	if ((byte & 0x08U) == 0x08U)
		bits++;
	if ((byte & 0x10U) == 0x10U)
		bits++;
	if ((byte & 0x20U) == 0x20U)
		bits++;
	if ((byte & 0x40U) == 0x40U)
		bits++;
	if ((byte & 0x80U) == 0x80U)
		bits++;

	return bits;
}
