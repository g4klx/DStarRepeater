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
#include "DStarRepeaterRXThread.h"
#include "DVAPController.h"
#include "DStarDefines.h"
#include "HeaderData.h"
#include "Version.h"
#include "Logger.h"
#include "Utils.h"

#include <chrono>

using namespace std::chrono;

const unsigned int MAX_DATA_SYNC_BIT_ERRS  = 2U;

const unsigned int CYCLE_TIME = 9U;

CDStarRepeaterRXThread::CDStarRepeaterRXThread(const std::string& type) :
m_type(type),
m_modem(nullptr),
m_protocolHandler(nullptr),
m_rxHeader(nullptr),
m_radioSeqNo(0U),
m_registerTimer(1000U),
m_rptState(DSRS_LISTENING),
m_rxState(DSRXS_LISTENING),
m_slowDataDecoder(),
m_killed(false),
m_ambe(),
m_ambeFrames(0U),
m_ambeSilence(0U),
m_ambeBits(1U),
m_ambeErrors(0U),
m_lastAMBEBits(0U),
m_lastAMBEErrors(0U)
#if defined(MQTT)
,m_mqttStatusTimer(1000U, 1U)		// 1s
#endif
{
	setRadioState(DSRXS_LISTENING);
}

CDStarRepeaterRXThread::~CDStarRepeaterRXThread()
{
	delete m_rxHeader;
	delete m_modem;
	delete m_protocolHandler;
}

// ---------------------------------------------------------------------------
// entry() — receive-only thread body.
//
// Requires modem and protocol handler before starting.  Main loop (~9 ms):
//   1. receiveModem()   — drain RF events; forward headers and AMBE to the network.
//   2. receiveNetwork() — drain and discard any inbound network packets (RX only).
//   3. Service the register timer (sends a keepalive to the gateway every 30 s).
//   4. MQTT status publication (1 s, if built with MQTT=1).
// ---------------------------------------------------------------------------
void CDStarRepeaterRXThread::entry()
{
	// Wait here until we have the essentials to run
	while (!m_killed && (m_modem == nullptr  || m_protocolHandler == nullptr))
		std::this_thread::sleep_for(std::chrono::milliseconds(500));		// 1/2 sec

	if (m_killed)
		return;

	m_registerTimer.start(10U);
#if defined(MQTT)
	m_mqttStatusTimer.start();
#endif

	wxLogMessage("Starting the D-Star receiver thread");

	auto stopWatch = steady_clock::now();

	try {
		while (!m_killed) {
			stopWatch = steady_clock::now();

			receiveModem();

			receiveNetwork();

			// Send the register packet if needed and restart the timer
			if (m_registerTimer.hasExpired()) {
				m_protocolHandler->writeRegister();
				m_registerTimer.start(30U);
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

	wxLogMessage("Stopping the D-Star receiver thread");

	m_modem->stop();
	delete m_modem;
	m_modem = nullptr;

	m_protocolHandler->close();
	delete m_protocolHandler;
	m_protocolHandler = nullptr;
}

void CDStarRepeaterRXThread::kill()
{
	m_killed = true;
}

void CDStarRepeaterRXThread::setCallsign(const std::string&, const std::string&, DSTAR_MODE, ACK_TYPE, bool, bool, bool, bool)
{
}

void CDStarRepeaterRXThread::setProtocolHandler(CRepeaterProtocolHandler* handler, bool local)
{
	assert(handler != nullptr);

	m_protocolHandler = handler;
}

void CDStarRepeaterRXThread::setModem(CModem* modem)
{
	assert(modem != nullptr);

	m_modem = modem;
}

void CDStarRepeaterRXThread::setTimes(unsigned int, unsigned int)
{
}

void CDStarRepeaterRXThread::setBeacon(unsigned int, const std::string&, bool, TEXT_LANG)
{
}

void CDStarRepeaterRXThread::setAnnouncement(bool, unsigned int, const std::string&, const std::string&, const std::string&, const std::string&)
{
}

void CDStarRepeaterRXThread::setController(CExternalController*, unsigned int)
{
}

void CDStarRepeaterRXThread::setOutputs(bool, bool, bool, bool)
{
}

void CDStarRepeaterRXThread::setLogging(bool, const std::string&)
{
}

void CDStarRepeaterRXThread::setWhiteList(CCallsignList* list) { delete list; }

void CDStarRepeaterRXThread::setBlackList(CCallsignList* list) { delete list; }

void CDStarRepeaterRXThread::setGreyList(CCallsignList* list) { delete list; }

void CDStarRepeaterRXThread::receiveModem()
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

void CDStarRepeaterRXThread::receiveHeader(CHeaderData* header)
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

void CDStarRepeaterRXThread::receiveSlowData(unsigned char* data, unsigned int)
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

void CDStarRepeaterRXThread::receiveRadioData(unsigned char* data, unsigned int)
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

// Drain and discard all inbound network packets — this thread cannot transmit.
void CDStarRepeaterRXThread::receiveNetwork()
{
	NETWORK_TYPE type;

	do {
		type = m_protocolHandler->read();
	} while (type != NETWORK_NONE);
}

void CDStarRepeaterRXThread::setRadioState(DSTAR_RX_STATE state)
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

bool CDStarRepeaterRXThread::processRadioHeader(CHeaderData* header)
{
	assert(header != nullptr);

	// We don't handle DD data packets
	if (header->isDataPacket()) {
		wxLogMessage("Received a DD packet, ignoring");
		delete header;
		return false;
	}

	m_rptState = DSRS_VALID;

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

	return true;
}

void CDStarRepeaterRXThread::processRadioFrame(unsigned char* data, FRAME_TYPE type)
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

void CDStarRepeaterRXThread::endOfRadioData()
{
	wxLogMessage("AMBE for %s  Frames: %.1fs, Silence: %.1f%%, BER: %.1f%%", m_rxHeader->getMyCall1().c_str(), float(m_ambeFrames) / 50.0F, float(m_ambeSilence * 100U) / float(m_ambeFrames), float(m_ambeErrors * 100U) / float(m_ambeBits));

	m_rptState = DSRS_LISTENING;

	m_protocolHandler->reset();
}

CDStarRepeaterStatusData* CDStarRepeaterRXThread::getStatus()
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
					std::string(), 0x00, 0x00, 0x00, false, m_rxState, m_rptState, 0U, 0U, 0U, 0U, 0U, 0U, 0.0F,
					std::string(), std::string(), std::string(), std::string(), std::string(), std::string());
	else
		status = new CDStarRepeaterStatusData(m_rxHeader->getMyCall1(), m_rxHeader->getMyCall2(),
					m_rxHeader->getYourCall(), m_rxHeader->getRptCall1(), m_rxHeader->getRptCall2(),
					m_rxHeader->getFlag1(), m_rxHeader->getFlag2(), m_rxHeader->getFlag3(), false, m_rxState,
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

void CDStarRepeaterRXThread::clock(unsigned int ms)
{
	m_registerTimer.clock(ms);
#if defined(MQTT)
	m_mqttStatusTimer.clock(ms);
#endif
}

void CDStarRepeaterRXThread::shutdown()
{
}

void CDStarRepeaterRXThread::startup()
{
}

unsigned int CDStarRepeaterRXThread::countBits(unsigned char byte)
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
