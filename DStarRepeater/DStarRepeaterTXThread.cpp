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
#include "DStarRepeaterTXThread.h"
#include "DStarDefines.h"
#include "HeaderData.h"
#include "Version.h"
#include "Logger.h"

#include <chrono>

using namespace std::chrono;

const unsigned int MAX_DATA_SYNC_BIT_ERRS  = 2U;

const unsigned int NETWORK_QUEUE_COUNT = 2U;

const unsigned int SILENCE_THRESHOLD = 2U;

const unsigned int CYCLE_TIME  = 9U;

CDStarRepeaterTXThread::CDStarRepeaterTXThread(const std::string& type) :
m_type(type),
m_modem(nullptr),
m_protocolHandler(nullptr),
m_rptCallsign(),
m_txHeader(nullptr),
m_networkQueue(nullptr),
m_writeNum(0U),
m_readNum(0U),
m_networkSeqNo(0U),
m_watchdogTimer(1000U, NETWORK_TIMEOUT),
m_registerTimer(1000U),
m_statusTimer(1000U, 0U, 100U),		// 100ms
m_state(DSRS_LISTENING),
m_tx(false),
m_space(0U),
m_killed(false),
m_lastData(nullptr),
m_ambe(),
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
}

CDStarRepeaterTXThread::~CDStarRepeaterTXThread()
{
	for (unsigned int i = 0U; i < NETWORK_QUEUE_COUNT; i++)
		delete m_networkQueue[i];
	delete[] m_networkQueue;
	delete[] m_lastData;
	delete   m_txHeader;
	delete   m_modem;
	delete   m_protocolHandler;
}

// ---------------------------------------------------------------------------
// entry() — transmit-only thread body.
//
// Requires modem, protocol handler, and callsign before starting.
// Main loop (~9 ms):
//   1. Refresh modem space/TX state every 100 ms.
//   2. receiveNetwork() — drain and queue gateway audio frames; handle gap filling.
//   3. receiveModem()   — drain modem events and discard them (TX-only).
//   4. Network watchdog: if DSRS_NETWORK and no frames for NETWORK_TIMEOUT, force EOT.
//   5. Gateway register timer (keepalive every 30 s).
//   6. Drain the network queue to the modem when space is available.
//   7. MQTT status publication (1 s).
// ---------------------------------------------------------------------------
void CDStarRepeaterTXThread::entry()
{
	// Wait here until we have the essentials to run
	while (!m_killed && (m_modem == nullptr  || m_protocolHandler == nullptr || m_rptCallsign.empty() || m_rptCallsign == "        "))
		std::this_thread::sleep_for(std::chrono::milliseconds(500));		// 1/2 sec

	if (m_killed)
		return;

	m_registerTimer.start(10U);
	m_statusTimer.start();
#if defined(MQTT)
	m_mqttStatusTimer.start();
#endif

	wxLogMessage("Starting the D-Star transmitter thread");

	auto stopWatch = steady_clock::now();

	try {
		while (!m_killed) {
			stopWatch = steady_clock::now();

			if (m_statusTimer.hasExpired() || m_space == 0U) {
				m_space = m_modem->getSpace();
				m_tx    = m_modem->isTX();
				m_statusTimer.start();
			}

			receiveNetwork();

			receiveModem();

			if (m_state == DSRS_NETWORK) {
				if (m_watchdogTimer.hasExpired()) {
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

			// Send the register packet if needed and restart the timer
			if (m_registerTimer.hasExpired()) {
				m_protocolHandler->writeRegister();
				m_registerTimer.start(30U);
			}

			if (m_networkQueue[m_readNum]->dataReady())
				transmitNetworkData();
			else if (m_networkQueue[m_readNum]->headerReady())
				transmitNetworkHeader();

#if defined(MQTT)
			// Publish status to MQTT every second
			if (m_mqttStatusTimer.hasExpired()) {
				if (g_mqtt != nullptr) {
					CDStarRepeaterStatusData* status = getStatus();
					std::string json = status->toJSON();
					g_mqtt->publish("status", json.c_str());
					delete status;
				}
				m_mqttStatusTimer.start();
			}
#endif

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

	wxLogMessage("Stopping the D-Star transmitter thread");

	m_modem->stop();
	delete m_modem;
	m_modem = nullptr;

	m_protocolHandler->close();
	delete m_protocolHandler;
	m_protocolHandler = nullptr;
}

void CDStarRepeaterTXThread::kill()
{
	m_killed = true;
}

void CDStarRepeaterTXThread::setCallsign(const std::string& callsign, const std::string&, DSTAR_MODE, ACK_TYPE, bool, bool, bool, bool)
{
	// Pad the callsign up to eight characters
	m_rptCallsign = callsign;
	m_rptCallsign.resize(LONG_CALLSIGN_LENGTH, ' ');
}

void CDStarRepeaterTXThread::setProtocolHandler(CRepeaterProtocolHandler* handler, bool local)
{
	assert(handler != nullptr);

	m_protocolHandler = handler;

	if (local) {
		wxLogInfo("Reducing transmit buffering because of local connection");

		for (unsigned int i = 0U; i < NETWORK_QUEUE_COUNT; i++)
			m_networkQueue[i]->setThreshold(LOCAL_RUN_FRAME_COUNT);
	}
}

void CDStarRepeaterTXThread::setModem(CModem* modem)
{
	assert(modem != nullptr);

	m_modem = modem;
}

void CDStarRepeaterTXThread::setTimes(unsigned int, unsigned int)
{
}

void CDStarRepeaterTXThread::setBeacon(unsigned int, const std::string&, bool, TEXT_LANG)
{
}

void CDStarRepeaterTXThread::setAnnouncement(bool, unsigned int, const std::string&, const std::string&, const std::string&, const std::string&)
{
}

void CDStarRepeaterTXThread::setController(CExternalController*, unsigned int)
{
}

void CDStarRepeaterTXThread::setOutputs(bool, bool, bool, bool)
{
}

void CDStarRepeaterTXThread::setLogging(bool, const std::string&)
{
}

void CDStarRepeaterTXThread::setWhiteList(CCallsignList* list) { delete list; }

void CDStarRepeaterTXThread::setBlackList(CCallsignList* list) { delete list; }

void CDStarRepeaterTXThread::setGreyList(CCallsignList* list) { delete list; }

// Drain and discard all modem events — this thread does not receive from RF.
void CDStarRepeaterTXThread::receiveModem()
{
	for (;;) {
		DSMT_TYPE type = m_modem->read();
		if (type == DSMTT_NONE)
			return;
	}
}

void CDStarRepeaterTXThread::receiveNetwork()
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
			if (length != 0U) {
				::memcpy(m_lastData, data, length);
				m_watchdogTimer.start();
				m_packetCount += processNetworkFrame(data, length, seqNo);
			}
		}
	}

	// Have we missed any data frames?
	long long packetMs = duration_cast<milliseconds>(steady_clock::now() - m_packetTime).count();
	if (m_state == DSRS_NETWORK && packetMs > 200L) {
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

void CDStarRepeaterTXThread::transmitNetworkHeader(CHeaderData* header)
{
	wxLogMessage("Transmitting to - My: %s/%s  Your: %s  Rpt1: %s  Rpt2: %s  Flags: %02X %02X %02X", header->getMyCall1().c_str(), header->getMyCall2().c_str(), header->getYourCall().c_str(), header->getRptCall1().c_str(), header->getRptCall2().c_str(), header->getFlag1(), header->getFlag2(), header->getFlag3());

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
	m_networkQueue[m_writeNum]->setHeader(header);
}

void CDStarRepeaterTXThread::transmitNetworkHeader()
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

void CDStarRepeaterTXThread::transmitNetworkData()
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

void CDStarRepeaterTXThread::processNetworkHeader(CHeaderData* header)
{
	assert(header != nullptr);

	wxLogMessage("Network header received - My: %s/%s  Your: %s  Rpt1: %s  Rpt2: %s  Flags: %02X %02X %02X", header->getMyCall1().c_str(), header->getMyCall2().c_str(), header->getYourCall().c_str(), header->getRptCall1().c_str(), header->getRptCall2().c_str(), header->getFlag1(), header->getFlag2(), header->getFlag3());

	// Is it for us?
	if (header->getRptCall2() != m_rptCallsign) {
		wxLogMessage("Invalid network RPT2 value, ignoring");
		delete header;
		return;
	}

	m_state = DSRS_NETWORK;
	m_networkSeqNo = 0U;
	m_watchdogTimer.start();

	delete m_txHeader;
	m_txHeader = header;

#if defined(MQTT)
	mqttPublishDStarStart(m_txHeader->getMyCall1(), m_txHeader->getMyCall2(),
		m_txHeader->getYourCall(), m_txHeader->getRptCall2(), "net");
#endif

	transmitNetworkHeader(new CHeaderData(*header));
}

unsigned int CDStarRepeaterTXThread::processNetworkFrame(unsigned char* data, unsigned int length, unsigned char seqNo)
{
	assert(data != nullptr);
	assert(length > 0U);

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

void CDStarRepeaterTXThread::endOfNetworkData()
{
	float loss = 0.0F;
	if (m_packetCount != 0U)
		loss = float(m_packetSilence) / float(m_packetCount);

	wxLogMessage("Stats for %s  Frames: %.1fs, Loss: %.1f%%, Packets: %u/%u", m_txHeader->getMyCall1().c_str(), float(m_packetCount) / 50.0F, loss * 100.0F, m_packetSilence, m_packetCount);

	m_state = DSRS_LISTENING;
	m_watchdogTimer.stop();
	m_protocolHandler->reset();

	m_writeNum++;
	if (m_writeNum >= NETWORK_QUEUE_COUNT)
		m_writeNum = 0U;
}

CDStarRepeaterStatusData* CDStarRepeaterTXThread::getStatus()
{
	float loss = 0.0F;
	if (m_packetCount != 0U)
		loss = float(m_packetSilence) / float(m_packetCount);

	if (m_state == DSRS_LISTENING)
		return new CDStarRepeaterStatusData(std::string(), std::string(), std::string(), std::string(),
				std::string(), 0x00, 0x00, 0x00, m_tx, DSRXS_LISTENING, m_state, 0U, 0U, 0U, 0U, 0U, 0U, 0.0F,
				std::string(), std::string(), std::string(), std::string(), std::string(), std::string());
	else
		return new CDStarRepeaterStatusData(m_txHeader->getMyCall1(), m_txHeader->getMyCall2(),
				m_txHeader->getYourCall(), m_txHeader->getRptCall1(), m_txHeader->getRptCall2(),
				m_txHeader->getFlag1(), m_txHeader->getFlag2(), m_txHeader->getFlag3(), m_tx, DSRXS_LISTENING,
				m_state, 0U, 0U, 0U, 0U, 0U, 0U, loss * 100.0F, std::string(), std::string(), std::string(), std::string(),
				std::string(), std::string());
}

void CDStarRepeaterTXThread::clock(unsigned int ms)
{
	m_registerTimer.clock(ms);
	m_watchdogTimer.clock(ms);
	m_statusTimer.clock(ms);
#if defined(MQTT)
	m_mqttStatusTimer.clock(ms);
#endif
}

void CDStarRepeaterTXThread::shutdown()
{
}

void CDStarRepeaterTXThread::startup()
{
}

