/*
 *   Copyright (C) 2011-2016,2018 by Jonathan Naylor G4KLX
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
#include "DStarRepeaterTRXThread.h"
#include "DVAPController.h"
#include "DStarDefines.h"
#include "HeaderData.h"
#include "Version.h"
#include "Logger.h"

#include <chrono>
#include <cstdio>
#include <regex>

using namespace std::chrono;

// Bit-mask and expected signature for AMBE frames that contain DTMF tones.
// Applied in blankDTMF() to detect and mute DTMF before retransmission.
const unsigned char DTMF_MASK[] = {0x82U, 0x08U, 0x20U, 0x82U, 0x00U, 0x00U, 0x82U, 0x00U, 0x00U};
const unsigned char DTMF_SIG[]  = {0x82U, 0x08U, 0x20U, 0x82U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};

// Fuzzy sync detection: allow up to 2 bit errors when matching the data-sync pattern.
const unsigned int MAX_DATA_SYNC_BIT_ERRS  = 2U;

// Double-buffer the network output queue so a new transmission can be staged
// while the previous one is still being transmitted.
const unsigned int NETWORK_QUEUE_COUNT = 2U;

// If a gap is 2 frames or fewer, repeat the last frame rather than inserting silence.
const unsigned int SILENCE_THRESHOLD = 2U;

// Target loop period in milliseconds.  The main loop sleeps for any remaining
// time after its work is done, then clocks all timers with the actual elapsed ms.
const unsigned int CYCLE_TIME = 9U;

// ---------------------------------------------------------------------------
// Constructor — initialise all members to safe defaults.
// The two output queues (local, radio) are sized for 1 s of audio.
// The network queue array is heap-allocated as a double buffer (4 s each).
// Both state machines start in LISTENING.
// ---------------------------------------------------------------------------
CDStarRepeaterTRXThread::CDStarRepeaterTRXThread(const std::string& type) :
m_type(type),
m_modem(nullptr),
m_protocolHandler(nullptr),
m_controller(nullptr),
m_rptCallsign(),
m_gwyCallsign(),
m_beacon(nullptr),
m_announcement(nullptr),
m_recordRPT1(),
m_recordRPT2(),
m_deleteRPT1(),
m_deleteRPT2(),
m_rxHeader(nullptr),
m_localQueue((DV_FRAME_LENGTH_BYTES + 2U) * 50U, LOCAL_RUN_FRAME_COUNT),			// 1s worth of data
m_radioQueue((DV_FRAME_LENGTH_BYTES + 2U) * 50U, RADIO_RUN_FRAME_COUNT),			// 1s worth of data
m_networkQueue(nullptr),
m_writeNum(0U),
m_readNum(0U),
m_radioSeqNo(0U),
m_networkSeqNo(0U),
m_lastSlowDataType(0x00U),
m_timeoutTimer(1000U, 180U),		// 180s
m_watchdogTimer(1000U, NETWORK_TIMEOUT),
m_pollTimer(1000U, 60U),			// 60s
m_ackTimer(1000U, 0U, 500U),		// 0.5s
m_beaconTimer(1000U, 600U),			// 10 mins
m_announcementTimer(1000U, 0U),		// not running
m_statusTimer(1000U, 0U, 100U),		// 100ms
m_heartbeatTimer(1000U, 1U),		// 1s
m_rptState(DSRS_LISTENING),
m_rxState(DSRXS_LISTENING),
m_slowDataDecoder(),
m_ackEncoder(),
m_linkEncoder(),
m_headerEncoder(),
m_status1Encoder(),
m_status2Encoder(),
m_status3Encoder(),
m_status4Encoder(),
m_status5Encoder(),
m_tx(false),
m_space(0U),
m_killed(false),
m_mode(MODE_DUPLEX),
m_ack(AT_BER),
m_restriction(false),
m_rpt1Validation(true),
m_errorReply(true),
m_controlEnabled(false),
m_controlRPT1(),
m_controlRPT2(),
m_controlShutdown(),
m_controlStartup(),
m_activeHangTimer(1000U),
m_shutdown(false),
m_disable(false),
m_logging(nullptr),
m_lastData(nullptr),
m_ambe(),
m_ambeFrames(0U),
m_ambeSilence(0U),
m_ambeBits(1U),
m_ambeErrors(0U),
m_lastAMBEBits(0U),
m_lastAMBEErrors(0U),
m_ackText(),
m_tempAckText(),
m_linkStatus(LS_NONE),
m_reflector(),
m_regEx("^[A-Z0-9]{1}[A-Z0-9]{0,1}[0-9]{1,2}[A-Z]{1,4} {0,4}[ A-Z]{1}$"),
m_headerTime(),
m_packetTime(),
m_packetCount(0U),
m_packetSilence(0U),
m_whiteList(nullptr),
m_blackList(nullptr),
m_greyList(nullptr),
m_blocked(false),
m_busyData(false),
m_blanking(true),
m_recording(false),
m_deleting(false)
#if defined(MQTT)
,m_mqttStatusTimer(1000U, 1U)		// 1s
#endif
{
	for(int i = 0; i < 5; ++i)
		m_statusAnnounceTimer[i] = CTimer(1000U, 3U);

	m_statusText.resize(5);

	m_networkQueue = new COutputQueue*[NETWORK_QUEUE_COUNT];
	for (unsigned int i = 0U; i < NETWORK_QUEUE_COUNT; i++)
		m_networkQueue[i] = new COutputQueue((DV_FRAME_LENGTH_BYTES + 2U) * 200U, NETWORK_RUN_FRAME_COUNT);		// 4s worth of data);

	m_lastData = new unsigned char[DV_FRAME_MAX_LENGTH_BYTES];

	setRepeaterState(DSRS_LISTENING);
	setRadioState(DSRXS_LISTENING);
}

CDStarRepeaterTRXThread::~CDStarRepeaterTRXThread()
{
	for (unsigned int i = 0U; i < NETWORK_QUEUE_COUNT; i++)
		delete m_networkQueue[i];
	delete[] m_networkQueue;
	delete[] m_lastData;
	delete   m_rxHeader;
	delete   m_modem;
	delete   m_controller;
	delete   m_protocolHandler;
	delete   m_beacon;
	delete   m_announcement;
	delete   m_whiteList;
	delete   m_blackList;
	delete   m_greyList;
	delete   m_logging;
}

// ---------------------------------------------------------------------------
// entry() — the repeater thread body.
//
// Startup barrier: spin until the modem, controller, and callsign are all
// configured (set by the main thread via the set*() methods).
//
// Main loop (~9 ms cycle):
//   1. Refresh modem space/TX state every 100 ms.
//   2. receiveModem()   — drain the modem event queue; feed the state machine.
//   3. receiveNetwork() — drain the gateway event queue; fill the network output queue.
//   4. repeaterStateMachine() — handle timer-driven state transitions (timeout, watchdog, ack delay).
//   5. Service periodic timers: gateway poll, beacon, announcement, status announces, heartbeat, MQTT.
//   6. Drive the active-indicator output and handle shutdown/startup transitions.
//   7. Feed the highest-priority queue to the modem (radio > local > network).
//   8. Sleep for the remainder of the 9 ms cycle and clock all timers.
//
// Shutdown: on kill(), the loop exits and all subsystems are torn down in order.
// ---------------------------------------------------------------------------
void CDStarRepeaterTRXThread::entry()
{
	// Wait here until we have the essentials to run
	while (!m_killed && (m_modem == nullptr || m_controller == nullptr || m_rptCallsign.empty() || m_rptCallsign == "        "))
		std::this_thread::sleep_for(std::chrono::milliseconds(500));		// 1/2 sec

	if (m_killed)
		return;

	m_beaconTimer.start();
	m_announcementTimer.start();
	m_controller->setActive(false);
	m_controller->setRadioTransmit(false);
	m_statusTimer.start();
	m_heartbeatTimer.start();
#if defined(MQTT)
	m_mqttStatusTimer.start();
#endif

	if (m_protocolHandler != nullptr)
		m_pollTimer.start();

	std::string hardware = m_type;
	size_t n = hardware.find(' ');
	if (n != std::string::npos)
		hardware = m_type.substr(0, n);

	char pollBuf[128];
	::snprintf(pollBuf, sizeof(pollBuf), "linux_%s-%s", hardware.c_str(), VERSION.c_str());
	std::string pollText(pollBuf);
	// Replace spaces with dashes
	{
		size_t pos = 0;
		while ((pos = pollText.find(' ', pos)) != std::string::npos) {
			pollText.replace(pos, 1, "-");
			pos += 1;
		}
	}
	// Make lower case
	for (char& c : pollText)
		c = (char)tolower((unsigned char)c);

	wxLogMessage("Poll text set to \"%s\"", pollText.c_str());

	wxLogMessage("Starting the D-Star repeater thread");

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

			// Send the network poll if needed and restart the timer
			if (m_pollTimer.hasExpired()) {
				m_protocolHandler->writePoll(pollText);
				m_pollTimer.start();
			}

			// Send the beacon and restart the timer
			if (m_beaconTimer.isRunning() && m_beaconTimer.hasExpired()) {
				m_beacon->sendBeacon();
				m_beaconTimer.start();
			}

			// Send the announcement and restart the timer
			if (m_announcementTimer.isRunning() && m_announcementTimer.hasExpired()) {
				m_announcement->startAnnouncement();
				m_announcementTimer.start();
			}

			// A status announce timer fires 3 s after a DTMF trigger.
			// Only transmit if the repeater is back to LISTENING by then.
			for(int i = 0; i < 5; ++ i) {
				if(m_statusAnnounceTimer[i].isRunning() &&
				   m_statusAnnounceTimer[i].hasExpired()) {
				   	m_statusAnnounceTimer[i].stop();
				   	if(m_rptState == DSRS_LISTENING)
				   		transmitUserStatus(i);
				}
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

					// Publish BER and RSSI for Display-Driver during active RF
					if (m_rptState == DSRS_VALID && m_ambeBits > 0U) {
						float ber = float(m_ambeErrors * 100U) / float(m_ambeBits);
						mqttPublishBER(ber);
					}

					// Publish DVAP signal strength as RSSI during active RF.
					// No m_ambeBits guard needed — the DVAP continuously updates
					// m_signal from hardware regardless of voice frame arrival.
					if (m_rptState == DSRS_VALID && m_type == "DVAP" && m_modem != nullptr) {
						CDVAPController* dvap = static_cast<CDVAPController*>(m_modem);
						mqttPublishRSSI(dvap->getSignal());
					}
				}
				m_mqttStatusTimer.start();
			}
#endif

			// Set the output state
			if (m_rptState == DSRS_VALID      || m_rptState == DSRS_INVALID      || m_rptState == DSRS_TIMEOUT      ||
				m_rptState == DSRS_VALID_WAIT || m_rptState == DSRS_INVALID_WAIT || m_rptState == DSRS_TIMEOUT_WAIT ||
				m_rptState == DSRS_NETWORK    || (m_activeHangTimer.isRunning() && !m_activeHangTimer.hasExpired())) {
				m_controller->setActive(true);
			} else {
				m_controller->setActive(false);
				m_activeHangTimer.stop();
			}

			// Check the shutdown state, state changes are done here to bypass the state machine which is
			// frozen when m_disable or m_shutdown are asserted
			m_disable = m_controller->getDisable();
			if (m_disable || m_shutdown) {
				if (m_rptState != DSRS_SHUTDOWN) {
					m_timeoutTimer.stop();
					m_watchdogTimer.stop();
					m_activeHangTimer.stop();
					m_ackTimer.stop();
					m_beaconTimer.stop();
					m_announcementTimer.stop();
					m_localQueue.reset();
					m_radioQueue.reset();
					for (unsigned int i = 0U; i < NETWORK_QUEUE_COUNT; i++)
						m_networkQueue[i]->reset();
					m_controller->setActive(false);
					m_controller->setRadioTransmit(false);
					m_rptState = DSRS_SHUTDOWN;
				}
			} else {
				if (m_rptState == DSRS_SHUTDOWN) {
					m_timeoutTimer.stop();
					m_watchdogTimer.stop();
					m_ackTimer.stop();
					m_beaconTimer.start();
					m_announcementTimer.start();
					m_rptState = DSRS_LISTENING;
					if (m_protocolHandler != nullptr)	// Tell the protocol handler
						m_protocolHandler->reset();
				}
			}

			if (m_radioQueue.dataReady())
				transmitRadioData();
			else if (m_localQueue.dataReady())
				transmitLocalData();
			else if (m_networkQueue[m_readNum]->dataReady())
				transmitNetworkData();
			else if (m_radioQueue.headerReady())
				transmitRadioHeader();
			else if (m_localQueue.headerReady())
				transmitLocalHeader();
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

	wxLogMessage("Stopping the D-Star repeater thread");

	m_modem->stop();
	delete m_modem;
	m_modem = nullptr;

	if (m_logging != nullptr) {
		m_logging->close();
		delete m_logging;
		m_logging = nullptr;
	}

	delete m_beacon;
	m_beacon = nullptr;
	delete m_announcement;
	m_announcement = nullptr;

	delete m_whiteList;
	m_whiteList = nullptr;
	delete m_blackList;
	m_blackList = nullptr;
	delete m_greyList;
	m_greyList = nullptr;

	m_controller->setActive(false);
	m_controller->setRadioTransmit(false);
	m_controller->close();
	delete m_controller;
	m_controller = nullptr;

	if (m_protocolHandler != nullptr) {
		m_protocolHandler->close();
		delete m_protocolHandler;
		m_protocolHandler = nullptr;
	}
}

void CDStarRepeaterTRXThread::kill()
{
	m_killed = true;
}

void CDStarRepeaterTRXThread::setCallsign(const std::string& callsign, const std::string& gateway, DSTAR_MODE mode, ACK_TYPE ack, bool restriction, bool rpt1Validation, bool dtmfBlanking, bool errorReply)
{
	// Pad the callsign up to eight characters
	m_rptCallsign = callsign;
	m_rptCallsign.resize(LONG_CALLSIGN_LENGTH, ' ');

	if (gateway.empty()) {
		m_gwyCallsign = callsign;
		m_gwyCallsign.resize(LONG_CALLSIGN_LENGTH - 1U, ' ');
		m_gwyCallsign += "G";
	} else {
		m_gwyCallsign = gateway;
		m_gwyCallsign.resize(LONG_CALLSIGN_LENGTH, ' ');
	}

	m_mode           = mode;
	m_ack            = ack;
	m_restriction    = restriction;
	m_rpt1Validation = rpt1Validation;
	m_blanking       = dtmfBlanking;
	m_errorReply     = errorReply;
}

void CDStarRepeaterTRXThread::setProtocolHandler(CRepeaterProtocolHandler* handler, bool local)
{
	assert(handler != nullptr);

	m_protocolHandler = handler;
}

void CDStarRepeaterTRXThread::setModem(CModem* modem)
{
	assert(modem != nullptr);

	m_modem = modem;
}

void CDStarRepeaterTRXThread::setTimes(unsigned int timeout, unsigned int ackTime)
{
	m_timeoutTimer.setTimeout(timeout);
	m_ackTimer.setTimeout(0U, ackTime);
}

void CDStarRepeaterTRXThread::setBeacon(unsigned int time, const std::string& text, bool voice, TEXT_LANG language)
{
	m_beaconTimer.setTimeout(time);

	if (time > 0U)
		m_beacon = new CBeaconUnit(this, m_rptCallsign, text, voice, language);
}

void CDStarRepeaterTRXThread::setAnnouncement(bool enabled, unsigned int time, const std::string& recordRPT1, const std::string& recordRPT2, const std::string& deleteRPT1, const std::string& deleteRPT2)
{
	if (enabled && time > 0U) {
		m_announcement = new CAnnouncementUnit(this, m_rptCallsign);

		m_announcementTimer.setTimeout(time);

		m_recordRPT1 = recordRPT1;
		m_recordRPT2 = recordRPT2;
		m_deleteRPT1 = deleteRPT1;
		m_deleteRPT2 = deleteRPT2;

		m_recordRPT1.append(LONG_CALLSIGN_LENGTH, ' ');
		m_recordRPT2.append(LONG_CALLSIGN_LENGTH, ' ');
		m_deleteRPT1.append(LONG_CALLSIGN_LENGTH, ' ');
		m_deleteRPT2.append(LONG_CALLSIGN_LENGTH, ' ');

		m_recordRPT1.resize(LONG_CALLSIGN_LENGTH);
		m_recordRPT2.resize(LONG_CALLSIGN_LENGTH);
		m_deleteRPT1.resize(LONG_CALLSIGN_LENGTH);
		m_deleteRPT2.resize(LONG_CALLSIGN_LENGTH);
	}
}

void CDStarRepeaterTRXThread::setController(CExternalController* controller, unsigned int activeHangTime)
{
	assert(controller != nullptr);

	m_controller = controller;
	m_activeHangTimer.setTimeout(activeHangTime);
}

void CDStarRepeaterTRXThread::setControl(bool enabled,
	const std::string& rpt1Callsign, const std::string& rpt2Callsign,
	const std::string& shutdown, const std::string& startup,
	const std::vector<std::string>& command, const std::vector<std::string>& commandLine,
	const std::vector<std::string>& status, const std::vector<std::string>& outputs)
{
	m_controlEnabled      = enabled;

	m_controlRPT1         = rpt1Callsign;
	m_controlRPT2         = rpt2Callsign;

	m_controlShutdown     = shutdown;
	m_controlStartup      = startup;

	m_controlStatus       = status;
	m_controlCommand      = command;
	m_controlCommandLine  = commandLine;
	m_controlOutput       = outputs;

	for (size_t i = 0; i < m_controlCommand.size(); ++i) {
		m_controlCommand[i].append(LONG_CALLSIGN_LENGTH, ' ');
		m_controlCommand[i].resize(LONG_CALLSIGN_LENGTH);
	}

	for (size_t i = 0; i < m_controlStatus.size(); ++i) {
		m_controlStatus[i].append(LONG_CALLSIGN_LENGTH, ' ');
		m_controlStatus[i].resize(LONG_CALLSIGN_LENGTH);
	}

	for (size_t i = 0; i < m_controlOutput.size(); ++i) {
		m_controlOutput[i].append(LONG_CALLSIGN_LENGTH, ' ');
		m_controlOutput[i].resize(LONG_CALLSIGN_LENGTH);
	}

	m_controlRPT1.append(LONG_CALLSIGN_LENGTH, ' ');
	m_controlRPT2.append(LONG_CALLSIGN_LENGTH, ' ');
	m_controlShutdown.append(LONG_CALLSIGN_LENGTH, ' ');
	m_controlStartup.append(LONG_CALLSIGN_LENGTH, ' ');

	m_controlRPT1.resize(LONG_CALLSIGN_LENGTH);
	m_controlRPT2.resize(LONG_CALLSIGN_LENGTH);
	m_controlShutdown.resize(LONG_CALLSIGN_LENGTH);
	m_controlStartup.resize(LONG_CALLSIGN_LENGTH);
}

void CDStarRepeaterTRXThread::setOutputs(bool out1, bool out2, bool out3, bool out4)
{
	if (m_controller == nullptr)
		return;

	m_output[0] = out1;
	m_output[1] = out2;
	m_output[2] = out3;
	m_output[3] = out4;

	m_controller->setOutput1(m_output[0]);
	m_controller->setOutput2(m_output[1]);
	m_controller->setOutput3(m_output[2]);
	m_controller->setOutput4(m_output[3]);
}

void CDStarRepeaterTRXThread::setLogging(bool logging, const std::string& dir)
{
	if (logging && m_logging == nullptr) {
		m_logging = new CDVTOOLFileWriter;
		m_logging->setDirectory(dir);
		return;
	}

	if (!logging && m_logging != nullptr) {
		delete m_logging;
		m_logging = nullptr;
		return;
	}
}

void CDStarRepeaterTRXThread::setWhiteList(CCallsignList* list)
{
	assert(list != nullptr);

	m_whiteList = list;
}

void CDStarRepeaterTRXThread::setBlackList(CCallsignList* list)
{
	assert(list != nullptr);

	m_blackList = list;
}

void CDStarRepeaterTRXThread::setGreyList(CCallsignList* list)
{
	assert(list != nullptr);

	m_greyList = list;
}

// ---------------------------------------------------------------------------
// receiveModem() — drain all pending events from the modem and dispatch them
// through the RX state machine.
//
// State: LISTENING
//   HEADER event → validate and process the radio header.
//   DATA event without a prior header → enter PROCESS_SLOW_DATA to attempt
//     header recovery from the embedded slow-data stream.
//
// State: PROCESS_SLOW_DATA
//   DATA events → feed the slow-data decoder looking for a late/embedded header.
//   EOT / LOST → give up and return to LISTENING.
//
// State: PROCESS_DATA
//   DATA events → process AMBE frames, relay to radio queue and network.
//   EOT / LOST → synthesise a FRAME_END, call endOfRadioData(), return to LISTENING.
// ---------------------------------------------------------------------------
void CDStarRepeaterTRXThread::receiveModem()
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

void CDStarRepeaterTRXThread::receiveHeader(CHeaderData* header)
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

void CDStarRepeaterTRXThread::receiveSlowData(unsigned char* data, unsigned int)
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

void CDStarRepeaterTRXThread::receiveRadioData(unsigned char* data, unsigned int)
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

// ---------------------------------------------------------------------------
// receiveNetwork() — drain all pending packets from the gateway and dispatch them.
//
// NETWORK_HEADER: validates RPT2 callsign, calls processNetworkHeader() which
//   transitions to DSRS_NETWORK and enqueues the header for transmission.
// NETWORK_DATA: feeds processNetworkFrame() which handles sequence-number gap
//   filling (silence or last-frame repeat) and queues frames for the modem.
// NETWORK_TEXT / TEMPTEXT: gateway-supplied slow-data text for the ack message.
// NETWORK_STATUS1..5: five user-status strings pushed by the gateway.
//
// After the read loop, checks whether any frames have arrived late (>200 ms gap)
// and inserts silence padding to keep the output stream continuous.
// ---------------------------------------------------------------------------
void CDStarRepeaterTRXThread::receiveNetwork()
{
	if (m_protocolHandler == nullptr)
		return;

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
		} else if (type == NETWORK_TEXT) {			// Slow data text for the Ack
			m_protocolHandler->readText(m_ackText, m_linkStatus, m_reflector);
			m_linkEncoder.setTextData(m_ackText);
			wxLogMessage("Slow data set to \"%s\"", m_ackText.c_str());
		} else if (type == NETWORK_TEMPTEXT) {			// Temporary slow data text for the Ack
			m_protocolHandler->readTempText(m_tempAckText);
			wxLogMessage("Temporary slow data set to \"%s\"", m_tempAckText.c_str());
		} else if (type == NETWORK_STATUS1) {		// Status 1 data text
			m_statusText[0] = m_protocolHandler->readStatus1();
			m_status1Encoder.setTextData(m_statusText[0]);
			wxLogMessage("Status 1 data set to \"%s\"", m_statusText[0].c_str());
		} else if (type == NETWORK_STATUS2) {		// Status 2 data text
			m_statusText[1] = m_protocolHandler->readStatus2();
			m_status2Encoder.setTextData(m_statusText[1]);
			wxLogMessage("Status 2 data set to \"%s\"", m_statusText[1].c_str());
		} else if (type == NETWORK_STATUS3) {		// Status 3 data text
			m_statusText[2] = m_protocolHandler->readStatus3();
			m_status3Encoder.setTextData(m_statusText[2]);
			wxLogMessage("Status 3 data set to \"%s\"", m_statusText[2].c_str());
		} else if (type == NETWORK_STATUS4) {		// Status 4 data text
			m_statusText[3] = m_protocolHandler->readStatus4();
			m_status4Encoder.setTextData(m_statusText[3]);
			wxLogMessage("Status 4 data set to \"%s\"", m_statusText[3].c_str());
		} else if (type == NETWORK_STATUS5) {		// Status 5 data text
			m_statusText[4] = m_protocolHandler->readStatus5();
			m_status5Encoder.setTextData(m_statusText[4]);
			wxLogMessage("Status 5 data set to \"%s\"", m_statusText[4].c_str());
		}
	}

	// Have we missed any data frames?
	long long packetMs = duration_cast<milliseconds>(steady_clock::now() - m_packetTime).count();
	if (m_rptState == DSRS_NETWORK && packetMs > 200L) {
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

void CDStarRepeaterTRXThread::transmitLocalHeader(CHeaderData* header)
{
	wxLogMessage("Transmitting to - My: %s/%s  Your: %s  Rpt1: %s  Rpt2: %s  Flags: %02X %02X %02X", header->getMyCall1().c_str(), header->getMyCall2().c_str(), header->getYourCall().c_str(), header->getRptCall1().c_str(), header->getRptCall2().c_str(), header->getFlag1(), header->getFlag2(), header->getFlag3());

	m_headerEncoder.setHeaderData(*header);

	m_localQueue.reset();
	m_localQueue.setHeader(header);
}

void CDStarRepeaterTRXThread::transmitBeaconHeader()
{
	CHeaderData* header = new CHeaderData(m_rptCallsign, "RPTR", "CQCQCQ  ", m_gwyCallsign, m_rptCallsign);
	transmitLocalHeader(header);
}

void CDStarRepeaterTRXThread::transmitBeaconData(const unsigned char* data, unsigned int length, bool end)
{
	m_localQueue.addData(data, length, end);
}

void CDStarRepeaterTRXThread::transmitAnnouncementHeader(CHeaderData* header)
{
	header->setRptCall1(m_gwyCallsign);
	header->setRptCall2(m_rptCallsign);

	transmitLocalHeader(header);
}

void CDStarRepeaterTRXThread::transmitAnnouncementData(const unsigned char* data, unsigned int length, bool end)
{
	m_localQueue.addData(data, length, end);
}

void CDStarRepeaterTRXThread::transmitRadioHeader(CHeaderData* header)
{
	wxLogMessage("Transmitting to - My: %s/%s  Your: %s  Rpt1: %s  Rpt2: %s  Flags: %02X %02X %02X", header->getMyCall1().c_str(), header->getMyCall2().c_str(), header->getYourCall().c_str(), header->getRptCall1().c_str(), header->getRptCall2().c_str(), header->getFlag1(), header->getFlag2(), header->getFlag3());

	m_headerEncoder.setHeaderData(*header);

	m_radioQueue.reset();
	m_radioQueue.setHeader(header);
}

void CDStarRepeaterTRXThread::transmitNetworkHeader(CHeaderData* header)
{
	wxLogMessage("Transmitting to - My: %s/%s  Your: %s  Rpt1: %s  Rpt2: %s  Flags: %02X %02X %02X", header->getMyCall1().c_str(), header->getMyCall2().c_str(), header->getYourCall().c_str(), header->getRptCall1().c_str(), header->getRptCall2().c_str(), header->getFlag1(), header->getFlag2(), header->getFlag3());

	m_headerEncoder.setHeaderData(*header);

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

void CDStarRepeaterTRXThread::transmitStatus()
{
	CHeaderData* header = new CHeaderData(m_rptCallsign, "    ", m_rxHeader->getMyCall1(), m_gwyCallsign, m_rptCallsign, RELAY_UNAVAILABLE);
	transmitLocalHeader(header);

	// Filler data
	for (unsigned int i = 0U; i < 21U; i++) {
		unsigned char buffer[DV_FRAME_LENGTH_BYTES];

		if (i == 0U) {
			m_ackEncoder.sync();

			::memcpy(buffer + 0U, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, DATA_SYNC_BYTES, DATA_FRAME_LENGTH_BYTES);
		} else {
			unsigned char text[DATA_FRAME_LENGTH_BYTES];
			m_ackEncoder.getTextData(text);

			::memcpy(buffer + 0U, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, text, DATA_FRAME_LENGTH_BYTES);
		}

		m_localQueue.addData(buffer, DV_FRAME_LENGTH_BYTES, false);
	}

	m_localQueue.addData(END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES, true);
}

void CDStarRepeaterTRXThread::transmitErrorStatus()
{
	CHeaderData* header = new CHeaderData(m_rptCallsign, "    ", m_rxHeader->getMyCall1(), m_rptCallsign, m_rptCallsign, RELAY_UNAVAILABLE);
	transmitLocalHeader(header);

	// Filler data
	for (unsigned int i = 0U; i < 21U; i++) {
		unsigned char buffer[DV_FRAME_LENGTH_BYTES];

		if (i == 0U) {
			m_linkEncoder.sync();

			::memcpy(buffer + 0U, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, DATA_SYNC_BYTES, DATA_FRAME_LENGTH_BYTES);
		} else {
			unsigned char text[DATA_FRAME_LENGTH_BYTES];
			m_linkEncoder.getTextData(text);

			::memcpy(buffer + 0U, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, text, DATA_FRAME_LENGTH_BYTES);
		}

		m_localQueue.addData(buffer, DV_FRAME_LENGTH_BYTES, false);
	}

	m_localQueue.addData(END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES, true);
}

void CDStarRepeaterTRXThread::transmitUserStatus(unsigned int n)
{
	CSlowDataEncoder* encoder = nullptr;
	CHeaderData* header = nullptr;
	switch (n) {
		case 0U:
			header = new CHeaderData(m_rptCallsign, "    ", "STATUS 1", m_gwyCallsign, m_rptCallsign);
			encoder = &m_status1Encoder;
			break;
		case 1U:
			header = new CHeaderData(m_rptCallsign, "    ", "STATUS 2", m_gwyCallsign, m_rptCallsign);
			encoder = &m_status2Encoder;
			break;
		case 2U:
			header = new CHeaderData(m_rptCallsign, "    ", "STATUS 3", m_gwyCallsign, m_rptCallsign);
			encoder = &m_status3Encoder;
			break;
		case 3U:
			header = new CHeaderData(m_rptCallsign, "    ", "STATUS 4", m_gwyCallsign, m_rptCallsign);
			encoder = &m_status4Encoder;
			break;
		case 4U:
			header = new CHeaderData(m_rptCallsign, "    ", "STATUS 5", m_gwyCallsign, m_rptCallsign);
			encoder = &m_status5Encoder;
			break;
		default:
			wxLogWarning("Invalid status number - %u", n);
			return;
	}

	transmitLocalHeader(header);

	// Filler data
	for (unsigned int i = 0U; i < 21U; i++) {
		unsigned char buffer[DV_FRAME_LENGTH_BYTES];

		if (i == 0U) {
			encoder->sync();

			::memcpy(buffer + 0U, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, DATA_SYNC_BYTES, DATA_FRAME_LENGTH_BYTES);
		} else {
			unsigned char text[DATA_FRAME_LENGTH_BYTES];
			encoder->getTextData(text);

			::memcpy(buffer + 0U, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, text, DATA_FRAME_LENGTH_BYTES);
		}

		m_localQueue.addData(buffer, DV_FRAME_LENGTH_BYTES, false);
	}

	m_localQueue.addData(END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES, true);
}

void CDStarRepeaterTRXThread::transmitLocalHeader()
{
	// Don't send a header until the modem is ready
	bool ready = m_modem->isTXReady();
	if (!ready)
		return;

	CHeaderData* header = m_localQueue.getHeader();
	if (header == nullptr)
		return;

	m_modem->writeHeader(*header);
	delete header;
}

void CDStarRepeaterTRXThread::transmitLocalData()
{
	if (m_space == 0U)
		return;

	unsigned char buffer[DV_FRAME_LENGTH_BYTES];
	bool end;
	unsigned int length = m_localQueue.getData(buffer, DV_FRAME_LENGTH_BYTES, end);

	if (length == 0U)
		return;

	m_modem->writeData(buffer, length, end);
	m_space--;

	if (end)
		m_localQueue.reset();
}

void CDStarRepeaterTRXThread::transmitRadioHeader()
{
	// Don't send a header until the modem is ready
	bool ready = m_modem->isTXReady();
	if (!ready)
		return;

	CHeaderData* header = m_radioQueue.getHeader();
	if (header == nullptr)
		return;

	m_modem->writeHeader(*header);
	delete header;
}

void CDStarRepeaterTRXThread::transmitRadioData()
{
	if (m_space == 0U)
		return;

	unsigned char buffer[DV_FRAME_LENGTH_BYTES];
	bool end;
	unsigned int length = m_radioQueue.getData(buffer, DV_FRAME_LENGTH_BYTES, end);

	if (length == 0U)
		return;

	m_modem->writeData(buffer, length, end);
	m_space--;

	if (end)
		m_radioQueue.reset();
}

void CDStarRepeaterTRXThread::transmitNetworkHeader()
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

void CDStarRepeaterTRXThread::transmitNetworkData()
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

// ---------------------------------------------------------------------------
// repeaterStateMachine() — timer-driven state transitions called once per loop.
//
// DSRS_VALID:       timeout expiry → DSRS_TIMEOUT (user held PTT too long).
// DSRS_VALID_WAIT:  ack timer expiry → transmit BER/status, return to LISTENING.
// DSRS_INVALID_WAIT:ack timer expiry → transmit error-status if configured, LISTENING.
// DSRS_TIMEOUT_WAIT:ack timer expiry → transmit status, return to LISTENING.
// DSRS_NETWORK:     watchdog expiry → forcibly end the network transmission.
//
// The *_WAIT states exist so that the ack/status reply is deferred until the
// modem has fully stopped transmitting and PTT has dropped.
// ---------------------------------------------------------------------------
void CDStarRepeaterTRXThread::repeaterStateMachine()
{
	switch (m_rptState) {
		case DSRS_VALID:
			if (m_timeoutTimer.isRunning() && m_timeoutTimer.hasExpired()) {
				wxLogMessage("User has timed out");
				setRepeaterState(DSRS_TIMEOUT);
			}
			break;

		case DSRS_VALID_WAIT:
			if (m_ackTimer.hasExpired()) {
				if (m_mode != MODE_GATEWAY)
					transmitStatus();
				setRepeaterState(DSRS_LISTENING);
				m_activeHangTimer.start();
			}
			break;

		case DSRS_INVALID_WAIT:
			if (m_ackTimer.hasExpired()) {
				if (m_mode != MODE_GATEWAY && m_errorReply)
					transmitErrorStatus();
				setRepeaterState(DSRS_LISTENING);
				m_activeHangTimer.start();
			}
			break;

		case DSRS_TIMEOUT_WAIT:
			if (m_ackTimer.hasExpired()) {
				if (m_mode != MODE_GATEWAY)
					transmitStatus();
				setRepeaterState(DSRS_LISTENING);
				m_activeHangTimer.start();
			}
			break;

		case DSRS_NETWORK:
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
			break;

		default:
			break;
	}
}

void CDStarRepeaterTRXThread::setRadioState(DSTAR_RX_STATE state)
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

// ---------------------------------------------------------------------------
// setRepeaterState() — transition the repeater state machine.
//
// Returns false without changing state if:
//   - the repeater is disabled/shutdown, or
//   - the requested transition is not allowed from the current state
//     (e.g. DSRS_VALID is only reachable from LISTENING or VALID_WAIT).
//
// On exit from LISTENING the beacon timer is stopped (no beacons mid-QSO).
// On entry to LISTENING the beacon timer restarts and the gateway is reset.
// On entry to DSRS_VALID the timeout timer starts (or the ack timer stops
//   if we are re-keying from VALID_WAIT).
// On entry to DSRS_NETWORK the watchdog starts to guard against gateway silence.
// ---------------------------------------------------------------------------
bool CDStarRepeaterTRXThread::setRepeaterState(DSTAR_RPT_STATE state)
{
	// Can't change state when shutdown
	if (m_disable || m_shutdown)
		return false;

	// The "from" state
	switch (m_rptState) {
		case DSRS_SHUTDOWN:
			m_beaconTimer.start();
			m_announcementTimer.start();
			break;

		case DSRS_LISTENING:
			m_beaconTimer.stop();
			break;

		default:
			break;
	}

	// The "to" state
	switch (state) {
		case DSRS_SHUTDOWN:
			m_timeoutTimer.stop();
			m_watchdogTimer.stop();
			m_activeHangTimer.stop();
			m_ackTimer.stop();
			m_beaconTimer.stop();
			m_announcementTimer.stop();
			m_controller->setActive(false);
			m_controller->setRadioTransmit(false);
			m_rptState = DSRS_SHUTDOWN;
			break;

		case DSRS_LISTENING:
			m_timeoutTimer.stop();
			m_watchdogTimer.stop();
			m_ackTimer.stop();
			m_beaconTimer.start();
			m_rptState = DSRS_LISTENING;
			if (m_protocolHandler != nullptr)	// Tell the protocol handler
				m_protocolHandler->reset();
			break;

		case DSRS_VALID:
			if (m_rptState != DSRS_LISTENING && m_rptState != DSRS_VALID_WAIT)
				return false;

			if (m_rptState == DSRS_LISTENING)
				m_timeoutTimer.start();
			else
				m_ackTimer.stop();

			m_activeHangTimer.stop();
			m_rptState = DSRS_VALID;
			break;

		case DSRS_VALID_WAIT:
			m_ackTimer.start();
			m_rptState = DSRS_VALID_WAIT;
			break;

		case DSRS_TIMEOUT:
			// Send end of transmission data to the radio and the network
			if (m_mode == MODE_DUPLEX)
				m_radioQueue.addData(END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES, true);

			if (!m_blocked && m_protocolHandler != nullptr) {
				unsigned char bytes[DV_FRAME_MAX_LENGTH_BYTES];
				::memcpy(bytes, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
				::memcpy(bytes + VOICE_FRAME_LENGTH_BYTES, END_PATTERN_BYTES, END_PATTERN_LENGTH_BYTES);
				m_protocolHandler->writeData(bytes, DV_FRAME_MAX_LENGTH_BYTES, 0U, true);
			}

			m_timeoutTimer.stop();
			m_rptState = DSRS_TIMEOUT;
			break;

		case DSRS_TIMEOUT_WAIT:
			m_ackTimer.start();
			m_timeoutTimer.stop();
			m_rptState = DSRS_TIMEOUT_WAIT;
			break;

		case DSRS_INVALID:
			if (m_rptState != DSRS_LISTENING)
				return false;

			m_activeHangTimer.stop();
			m_timeoutTimer.stop();
			m_rptState = DSRS_INVALID;
			break;

		case DSRS_INVALID_WAIT:
			m_ackTimer.start();
			m_timeoutTimer.stop();
			m_rptState = DSRS_INVALID_WAIT;
			break;

		case DSRS_NETWORK:
			if (m_rptState != DSRS_LISTENING && m_rptState != DSRS_VALID_WAIT && m_rptState != DSRS_INVALID_WAIT && m_rptState != DSRS_TIMEOUT_WAIT)
				return false;

			m_rptState = DSRS_NETWORK;
			m_networkSeqNo = 0U;
			m_timeoutTimer.stop();
			m_watchdogTimer.start();
			m_activeHangTimer.stop();
			m_ackTimer.stop();
			break;
	}

	return true;
}

// ---------------------------------------------------------------------------
// processRadioHeader() — validate and dispatch an incoming radio header.
//
// Processing order:
//   1. checkControl()       — DTMF control commands (shutdown, startup, status, etc.)
//   2. checkAnnouncements() — announcement record/delete triggers.
//   3. Shutdown guard       — drop silently when in DSRS_SHUTDOWN.
//   4. White-list check     — reject if callsign not in the white list.
//   5. Black-list check     — reject if callsign is in the black list.
//   6. Grey-list check      — allow locally, but suppress network forwarding.
//   7. Gateway-mode flag-2  — detect and drop our own retransmitted headers.
//   8. DD packet check      — reject D-Data (non-voice) frames.
//   9. checkHeader()        — structural and callsign format validation.
//  10. DSRS_NETWORK guard   — repeater busy: send a busy header to the network if appropriate.
//  11. setRepeaterState(DSRS_VALID) — accept the transmission; forward header to network and RF.
//
// Returns true if the header was consumed (even if ultimately rejected), false
// only for DD packets (the caller uses this to stay in LISTENING).
// ---------------------------------------------------------------------------
bool CDStarRepeaterTRXThread::processRadioHeader(CHeaderData* header)
{
	assert(header != nullptr);

	// Check control messages
	bool res = checkControl(*header);
	if (res) {
		delete header;
		return true;
	}

	// Check announcement messages
	res = checkAnnouncements(*header);
	if (res) {
		bool res = setRepeaterState(DSRS_INVALID);
		if (res) {
			delete m_rxHeader;
			m_rxHeader = header;
		} else {
			delete header;
		}
		return true;
	}

	// If shutdown we ignore incoming headers
	if (m_rptState == DSRS_SHUTDOWN) {
		delete header;
		return true;
	}

	if (m_whiteList != nullptr) {
		bool res = m_whiteList->isInList(header->getMyCall1());
		if (!res) {
			wxLogMessage("%s rejected due to not being in the white list", header->getMyCall1().c_str());
			delete header;
			return true;
		}
	}

	if (m_blackList != nullptr) {
		bool res = m_blackList->isInList(header->getMyCall1());
		if (res) {
			wxLogMessage("%s rejected due to being in the black list", header->getMyCall1().c_str());
			delete header;
			return true;
		}
	}

	m_blocked = false;
	if (m_greyList != nullptr) {
		bool res = m_greyList->isInList(header->getMyCall1());
		if (res) {
			wxLogMessage("%s blocked from the network due to being in the grey list", header->getMyCall1().c_str());
			m_blocked = true;
		}
	}

	// Check for receiving our own gateway data, and ignore it
	if (m_mode == MODE_GATEWAY) {
		if (header->getFlag2() == 0x01U) {
			wxLogMessage("Receiving a gateway header, ignoring");
			delete header;
			return true;
		}

		header->setFlag2(0x00U);
	}

	// We don't handle DD data packets
	if (header->isDataPacket()) {
		wxLogMessage("Received a DD packet, ignoring");
		delete header;
		return false;
	}

	TRISTATE valid = checkHeader(*header);
	switch (valid) {
		case STATE_FALSE: {
				bool res = setRepeaterState(DSRS_INVALID);
				if (res) {
					delete m_rxHeader;
					m_rxHeader = header;
				} else {
					delete header;
				}
			}
			return true;

		case STATE_UNKNOWN:
			delete header;
			return true;

		case STATE_TRUE:
			break;
	}

	// If we're in network mode, send the header as a busy header to the gateway in case it's an unlink command
	if (m_rptState == DSRS_NETWORK) {
		// Only send on the network if the user isn't blocked we have one and RPT2 is not blank or the repeater callsign
		if (header->getRptCall2() != "        " && header->getRptCall2() != m_rptCallsign) {
			if (!m_blocked && m_protocolHandler != nullptr) {
				CHeaderData netHeader(*header);
				netHeader.setRptCall1(header->getRptCall2());
				netHeader.setRptCall2(header->getRptCall1());
				netHeader.setFlag1(header->getFlag1() & ~REPEATER_MASK);
				m_protocolHandler->writeBusyHeader(netHeader);
			}

			m_busyData = true;
		}

		delete header;

		return true;
	}

	// Send the valid header to the gateway if we are accepted
	res = setRepeaterState(DSRS_VALID);
	if (res) {
		delete m_rxHeader;
		m_rxHeader = header;

#if defined(MQTT)
		mqttPublishDStarStart(m_rxHeader->getMyCall1(), m_rxHeader->getMyCall2(),
			m_rxHeader->getYourCall(), m_rxHeader->getRptCall2(), "rf");
#endif

		if (m_logging != nullptr)
			m_logging->open(*m_rxHeader);

		// Only send on the network if the user isn't blocked and we have one and RPT2 is not blank or the repeater callsign
		if (!m_blocked && m_protocolHandler != nullptr && m_rxHeader->getRptCall2() != "        " && m_rxHeader->getRptCall2() != m_rptCallsign) {
			CHeaderData netHeader(*m_rxHeader);
			netHeader.setRptCall1(m_rxHeader->getRptCall2());
			netHeader.setRptCall2(m_rxHeader->getRptCall1());
			netHeader.setFlag1(m_rxHeader->getFlag1() & ~REPEATER_MASK);
			m_protocolHandler->writeHeader(netHeader);
		}

		// Create the new radio header but only in duplex mode
		if (m_mode == MODE_DUPLEX) {
			CHeaderData* rfHeader = new CHeaderData(*m_rxHeader);
			rfHeader->setRptCall1(m_rptCallsign);
			rfHeader->setRptCall2(m_rptCallsign);
			rfHeader->setFlag1(m_rxHeader->getFlag1() & ~REPEATER_MASK);
			transmitRadioHeader(rfHeader);
		}
	}

	return true;
}

// ---------------------------------------------------------------------------
// processNetworkHeader() — handle a header arriving from the gateway.
//
// Validates that RPT2 matches our callsign, then transitions to DSRS_NETWORK.
// In GATEWAY mode the header is modified before transmission: the repeater bit
// is set, flag2 is set to 0x01 (marks gateway origin), and RPT1/RPT2 are
// swapped to the local pair.  In other modes the header is forwarded unchanged.
// ---------------------------------------------------------------------------
void CDStarRepeaterTRXThread::processNetworkHeader(CHeaderData* header)
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

	bool res = setRepeaterState(DSRS_NETWORK);
	if (!res) {
		delete header;
		return;
	}

	delete m_rxHeader;
	m_rxHeader = header;

#if defined(MQTT)
	mqttPublishDStarStart(m_rxHeader->getMyCall1(), m_rxHeader->getMyCall2(),
		m_rxHeader->getYourCall(), m_rxHeader->getRptCall2(), "net");
#endif

	if (m_mode == MODE_GATEWAY) {
		// If in gateway mode, set the repeater bit, set flag 2 to 0x01,
		// and change RPT1 & RPT2 just for transmission
		CHeaderData* header = new CHeaderData(*m_rxHeader);
		header->setRepeaterMode(true);
		header->setFlag2(0x01U);
		header->setRptCall1(m_rptCallsign);
		header->setRptCall2(m_gwyCallsign);
		transmitNetworkHeader(header);
	} else {
		CHeaderData* header = new CHeaderData(*m_rxHeader);
		transmitNetworkHeader(header);
	}
}

void CDStarRepeaterTRXThread::processRadioFrame(unsigned char* data, FRAME_TYPE type)
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
			errors = m_ambe.regenerate(data);

		m_ambeErrors += errors;
		m_ambeBits   += 48U;		// Only count the bits with FEC added
	}

	if (::memcmp(data, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES) == 0)
		m_ambeSilence++;

	// If this is deleting an announcement, ignore the audio
	if (m_deleting) {
		if (type == FRAME_END) {
			m_deleting  = false;
			m_recording = false;
		}
		return;
	}

	// If this is recording an announcement, send the audio to the announcement unit and then stop
	if (m_recording) {
		m_announcement->writeData(data, DV_FRAME_LENGTH_BYTES, type == FRAME_END);
		if (type == FRAME_END) {
			m_deleting  = false;
			m_recording = false;
		}
		return;
	}

	// Pass background AMBE data to the network
	if (m_busyData) {
		if (type == FRAME_END) {
			if (!m_blocked && m_protocolHandler != nullptr) {
				unsigned char bytes[DV_FRAME_MAX_LENGTH_BYTES];
				::memcpy(bytes, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
				::memcpy(bytes + VOICE_FRAME_LENGTH_BYTES, END_PATTERN_BYTES, END_PATTERN_LENGTH_BYTES);
				m_protocolHandler->writeBusyData(bytes, DV_FRAME_MAX_LENGTH_BYTES, 0U, true);
			}

			m_busyData = false;
		} else {
			if (!m_blocked && m_protocolHandler != nullptr)
				m_protocolHandler->writeBusyData(data, DV_FRAME_LENGTH_BYTES, errors, false);
		}

		return;
	}

	// Don't pass through the frame of an invalid transmission
	if (m_rptState != DSRS_VALID)
		return;

	if (type == FRAME_END) {
		if (m_logging != nullptr)
			m_logging->close();

		// Transmit the end sync on the radio, but only in duplex mode
		if (m_mode == MODE_DUPLEX)
			m_radioQueue.addData(END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES, true);

		// Send null data and the end marker over the network, and the statistics
		if (!m_blocked && m_protocolHandler != nullptr) {
			unsigned char bytes[DV_FRAME_MAX_LENGTH_BYTES];
			::memcpy(bytes, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
			::memcpy(bytes + VOICE_FRAME_LENGTH_BYTES, END_PATTERN_BYTES, END_PATTERN_LENGTH_BYTES);
			m_protocolHandler->writeData(bytes, DV_FRAME_MAX_LENGTH_BYTES, 0U, true);
		}
	} else {
		if (m_logging != nullptr)
			m_logging->write(data, DV_FRAME_LENGTH_BYTES);

		// Send the data to the network
		if (!m_blocked && m_protocolHandler != nullptr)
			m_protocolHandler->writeData(data, DV_FRAME_LENGTH_BYTES, errors, false);

		// Send the data for transmission, but only in duplex mode
		if (m_mode == MODE_DUPLEX) {
			if (m_blanking)
				blankDTMF(data);
			m_radioQueue.addData(data, DV_FRAME_LENGTH_BYTES, false);
		}
	}
}

unsigned int CDStarRepeaterTRXThread::processNetworkFrame(unsigned char* data, unsigned int length, unsigned char seqNo)
{
	assert(data != nullptr);
	assert(length > 0U);

	if (m_rptState != DSRS_NETWORK)
		return 0U;

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
			if (!m_rxHeader->isDataPacket())
				m_ambe.regenerate(buffer);
			blankDTMF(buffer);
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
	if (m_networkSeqNo == 0U) {
		::memcpy(data + VOICE_FRAME_LENGTH_BYTES, DATA_SYNC_BYTES, DATA_FRAME_LENGTH_BYTES);
		m_headerEncoder.sync();
	} else {
		// If in Gateway mode, replace the slow data with our modified header when the slow data contains the
		// original header
		if (m_mode == MODE_GATEWAY) {
			unsigned char dataType = 0U;
			if ((m_networkSeqNo & 0x01U) == 0x01U)
				dataType = (data[VOICE_FRAME_LENGTH_BYTES] ^ SCRAMBLER_BYTE1) & SLOW_DATA_TYPE_MASK;

			if (dataType == SLOW_DATA_TYPE_HEADER || m_lastSlowDataType == SLOW_DATA_TYPE_HEADER) {
				unsigned char slowData[DATA_FRAME_LENGTH_BYTES];
				m_headerEncoder.getHeaderData(slowData);
				::memcpy(data + VOICE_FRAME_LENGTH_BYTES, slowData, DATA_FRAME_LENGTH_BYTES);
			}

			if ((m_networkSeqNo & 0x01U) == 0x01U)
				m_lastSlowDataType = dataType;
		}
	}

	packetCount++;
	m_networkSeqNo++;
	if (m_networkSeqNo >= 21U)
		m_networkSeqNo = 0U;

	// Data packets have no AMBE FEC
	if (!m_rxHeader->isDataPacket())
		m_ambe.regenerate(data);

	blankDTMF(data);

	m_networkQueue[m_writeNum]->addData(data, DV_FRAME_LENGTH_BYTES, false);

	return packetCount;
}

// ---------------------------------------------------------------------------
// endOfRadioData() — called when the radio transmission ends (EOT or LOST).
//
// Logs the AMBE statistics (duration, silence %, BER).  Depending on state:
//   DSRS_VALID:   formats the ack text (BER or custom text), transitions to
//                 DSRS_VALID_WAIT (which later sends the ack and returns to LISTENING).
//   DSRS_INVALID: transitions to DSRS_INVALID_WAIT for a possible error reply.
//   DSRS_TIMEOUT: same ack logic as VALID, then DSRS_TIMEOUT_WAIT.
// If AT_NONE is configured, skips the wait states and goes straight to LISTENING.
// ---------------------------------------------------------------------------
void CDStarRepeaterTRXThread::endOfRadioData()
{
	switch (m_rptState) {
		case DSRS_VALID:
			wxLogMessage("AMBE for %s  Frames: %.1fs, Silence: %.1f%%, BER: %.1f%%", m_rxHeader->getMyCall1().c_str(), float(m_ambeFrames) / 50.0F, float(m_ambeSilence * 100U) / float(m_ambeFrames), float(m_ambeErrors * 100U) / float(m_ambeBits));

			if (m_tempAckText.empty()) {
				if (m_ack == AT_BER) {
					// Create the ack text with the linked reflector and BER
					char ackBuf[64];
					if (m_linkStatus == LS_LINKED_DEXTRA || m_linkStatus == LS_LINKED_DPLUS || m_linkStatus == LS_LINKED_DCS || m_linkStatus == LS_LINKED_CCS || m_linkStatus == LS_LINKED_LOOPBACK)
						::snprintf(ackBuf, sizeof(ackBuf), "%-8s  BER: %.1f%%   ", m_reflector.c_str(), float(m_ambeErrors * 100U) / float(m_ambeBits));
					else
						::snprintf(ackBuf, sizeof(ackBuf), "BER: %.1f%%            ", float(m_ambeErrors * 100U) / float(m_ambeBits));
					m_ackEncoder.setTextData(std::string(ackBuf));
				} else {
					m_ackEncoder.setTextData(m_ackText);
				}
			} else {
				m_ackEncoder.setTextData(m_tempAckText);
				m_tempAckText.clear();
			}

			if (m_ack != AT_NONE || m_mode == MODE_GATEWAY) {
				setRepeaterState(DSRS_VALID_WAIT);
			} else {
				setRepeaterState(DSRS_LISTENING);
				m_activeHangTimer.start();
			}
			break;

		case DSRS_INVALID:
			wxLogMessage("AMBE for %s  Frames: %.1fs, Silence: %.1f%%, BER: %.1f%%", m_rxHeader->getMyCall1().c_str(), float(m_ambeFrames) / 50.0F, float(m_ambeSilence * 100U) / float(m_ambeFrames), float(m_ambeErrors * 100U) / float(m_ambeBits));

			if (m_ack != AT_NONE || m_mode == MODE_GATEWAY) {
				setRepeaterState(DSRS_INVALID_WAIT);
			} else {
				setRepeaterState(DSRS_LISTENING);
				m_activeHangTimer.start();
			}
			break;

		case DSRS_TIMEOUT:
			wxLogMessage("AMBE for %s  Frames: %.1fs, Silence: %.1f%%, BER: %.1f%%", m_rxHeader->getMyCall1().c_str(), float(m_ambeFrames) / 50.0F, float(m_ambeSilence * 100U) / float(m_ambeFrames), float(m_ambeErrors * 100U) / float(m_ambeBits));

			if (m_tempAckText.empty()) {
				if (m_ack == AT_BER) {
					// Create the ack text with the linked reflector and BER
					char ackBuf[64];
					if (m_linkStatus == LS_LINKED_DEXTRA || m_linkStatus == LS_LINKED_DPLUS || m_linkStatus == LS_LINKED_DCS || m_linkStatus == LS_LINKED_CCS || m_linkStatus == LS_LINKED_LOOPBACK)
						::snprintf(ackBuf, sizeof(ackBuf), "%-8s  BER: %.1f%%   ", m_reflector.c_str(), float(m_ambeErrors * 100U) / float(m_ambeBits));
					else
						::snprintf(ackBuf, sizeof(ackBuf), "BER: %.1f%%            ", float(m_ambeErrors * 100U) / float(m_ambeBits));
					m_ackEncoder.setTextData(std::string(ackBuf));
				} else {
					m_ackEncoder.setTextData(m_ackText);
				}
			} else {
				m_ackEncoder.setTextData(m_tempAckText);
				m_tempAckText.clear();
			}

			if (m_ack != AT_NONE || m_mode == MODE_GATEWAY) {
				setRepeaterState(DSRS_TIMEOUT_WAIT);
			} else {
				setRepeaterState(DSRS_LISTENING);
				m_activeHangTimer.start();
			}
			break;

		default:
			break;
	}
}

// ---------------------------------------------------------------------------
// endOfNetworkData() — called when a network transmission ends (end-of-frame
// flag received, or watchdog expiry).
//
// Logs packet-loss statistics, returns to DSRS_LISTENING, starts the active-
// hang timer, resets the gateway protocol handler, and advances the write-side
// of the double-buffered network queue to the next slot.
// ---------------------------------------------------------------------------
void CDStarRepeaterTRXThread::endOfNetworkData()
{
	float loss = 0.0F;
	if (m_packetCount != 0U)
		loss = float(m_packetSilence) / float(m_packetCount);

	if (m_rxHeader != nullptr)
		wxLogMessage("Stats for %s  Frames: %.1fs, Loss: %.1f%%, Packets: %u/%u", m_rxHeader->getMyCall1().c_str(), float(m_packetCount) / 50.0F, loss * 100.0F, m_packetSilence, m_packetCount);
	else
		wxLogMessage("Stats for Network  Frames: %.1fs, Loss: %.1f%%, Packets: %u/%u", float(m_packetCount) / 50.0F, loss * 100.0F, m_packetSilence, m_packetCount);

	setRepeaterState(DSRS_LISTENING);
	m_activeHangTimer.start();

	m_writeNum++;
	if (m_writeNum >= NETWORK_QUEUE_COUNT)
		m_writeNum = 0U;
}

CDStarRepeaterStatusData* CDStarRepeaterTRXThread::getStatus()
{
	CDStarRepeaterStatusData* status;
	if (m_rptState == DSRS_SHUTDOWN || m_rptState == DSRS_LISTENING) {
		status = new CDStarRepeaterStatusData(std::string(), std::string(), std::string(), std::string(),
					std::string(), 0x00, 0x00, 0x00, m_tx, m_rxState, m_rptState, m_timeoutTimer.getTimer(),
					m_timeoutTimer.getTimeout(), m_beaconTimer.getTimer(), m_beaconTimer.getTimeout(),
					m_announcementTimer.getTimer(), m_announcementTimer.getTimeout(), 0.0F,
					m_ackText, m_statusText[0], m_statusText[1], m_statusText[2], m_statusText[3], m_statusText[4]);
	} else if (m_rptState == DSRS_NETWORK) {
		float loss = 0.0F;
		if (m_packetCount != 0U)
			loss = float(m_packetSilence) / float(m_packetCount);

		status = new CDStarRepeaterStatusData(m_rxHeader->getMyCall1(), m_rxHeader->getMyCall2(),
					m_rxHeader->getYourCall(), m_rxHeader->getRptCall1(), m_rxHeader->getRptCall2(),
					m_rxHeader->getFlag1(), m_rxHeader->getFlag2(), m_rxHeader->getFlag3(), m_tx, m_rxState,
					m_rptState, m_timeoutTimer.getTimer(), m_timeoutTimer.getTimeout(), m_beaconTimer.getTimer(),
					m_beaconTimer.getTimeout(), m_announcementTimer.getTimer(), m_announcementTimer.getTimeout(),
					loss * 100.0F, m_ackText, m_statusText[0], m_statusText[1], m_statusText[2], m_statusText[3], m_statusText[4]);
	} else {
		float   bits = float(m_ambeBits - m_lastAMBEBits);
		float errors = float(m_ambeErrors - m_lastAMBEErrors);
		if (bits == 0.0F)
			bits = 1.0F;

		m_lastAMBEBits   = m_ambeBits;
		m_lastAMBEErrors = m_ambeErrors;

		status = new CDStarRepeaterStatusData(m_rxHeader->getMyCall1(), m_rxHeader->getMyCall2(),
					m_rxHeader->getYourCall(), m_rxHeader->getRptCall1(), m_rxHeader->getRptCall2(),
					m_rxHeader->getFlag1(), m_rxHeader->getFlag2(), m_rxHeader->getFlag3(), m_tx, m_rxState,
					m_rptState, m_timeoutTimer.getTimer(), m_timeoutTimer.getTimeout(), m_beaconTimer.getTimer(),
					m_beaconTimer.getTimeout(), m_announcementTimer.getTimer(), m_announcementTimer.getTimeout(),
					(errors * 100.0F) / bits, m_ackText, m_statusText[0], m_statusText[1], m_statusText[2], m_statusText[3],
					m_statusText[4]);
	}

	if (m_type == "DVAP" && m_modem != nullptr) {
		CDVAPController* dvap = static_cast<CDVAPController*>(m_modem);
		bool squelch = dvap->getSquelch();
		int signal   = dvap->getSignal();
		status->setDVAP(squelch, signal);
	}

	return status;
}

void CDStarRepeaterTRXThread::clock(unsigned int ms)
{
	m_pollTimer.clock(ms);
	m_timeoutTimer.clock(ms);
	m_watchdogTimer.clock(ms);
	m_activeHangTimer.clock(ms);
	m_ackTimer.clock(ms);

	for(int i = 0; i < 5; ++i)
		m_statusAnnounceTimer[i].clock(ms);

	m_beaconTimer.clock(ms);
	m_announcementTimer.clock(ms);
	m_statusTimer.clock(ms);
	m_heartbeatTimer.clock(ms);
#if defined(MQTT)
	m_mqttStatusTimer.clock(ms);
#endif
	if (m_beacon != nullptr)
		m_beacon->clock();
	if (m_announcement != nullptr)
		m_announcement->clock();
}

void CDStarRepeaterTRXThread::shutdown()
{
	m_shutdown = true;
}

void CDStarRepeaterTRXThread::startup()
{
	m_shutdown = false;
}

// ---------------------------------------------------------------------------
// checkControl() — test the header's YOUR call against configured control
// destinations.  Returns true (and consumes the transmission) if any match.
//
// Matches are checked in priority order:
//   1. Command slots (up to 6): execute the configured shell command via system().
//   2. Status slots (up to 5): start a 3 s timer; on expiry transmit a status announcement.
//   3. Output slots (up to 4): toggle a controller output line.
//   4. Shutdown / Startup:     change the m_shutdown flag.
// The RPT1/RPT2 pair must also match m_controlRPT1/RPT2 for any check to proceed.
// ---------------------------------------------------------------------------
bool CDStarRepeaterTRXThread::checkControl(const CHeaderData& header)
{
	if (!m_controlEnabled)
		return false;

	if (m_controlRPT1 != header.getRptCall1() || m_controlRPT2 != header.getRptCall2())
		return false;

	for (size_t i = 0; i < m_controlCommand.size(); ++i) {
		if (m_controlCommand[i] == header.getYourCall()) {
			if (i < m_controlCommandLine.size() && !m_controlCommandLine[i].empty()) {
				wxLogMessage("Executing command %u (%s) requested by %s/%s",
					(unsigned)i + 1U, m_controlCommandLine[i].c_str(),
					header.getMyCall1().c_str(), header.getMyCall2().c_str());
				int ret = ::system(m_controlCommandLine[i].c_str());
				if (ret != 0)
					wxLogWarning("Command %u exited with status %d", (unsigned)i + 1U, ret);
			} else {
				wxLogMessage("Command %u requested by %s/%s (no command line configured)",
					(unsigned)i + 1U, header.getMyCall1().c_str(), header.getMyCall2().c_str());
			}
			return true;
		}
	}

	for (size_t i = 0; i < m_controlStatus.size(); ++i) {
		if (m_controlStatus[i] == header.getYourCall()) {
			wxLogMessage("Status %d requested by %s/%s",
				(int)i, header.getMyCall1().c_str(),
				header.getMyCall2().c_str());
			m_statusAnnounceTimer[i].start();
			return true;
		}
	}

	for (size_t i = 0; i < m_controlOutput.size(); ++i) {
		if (m_controlOutput[i] == header.getYourCall()) {
			wxLogMessage("Output %d requested by %s/%s", (int)i,
				header.getMyCall1().c_str(),
				header.getMyCall2().c_str());
			m_output[i] = !m_output[i];

			//  XXX These should be fixed in the controller code!
			m_controller->setOutput1(m_output[0]);
			m_controller->setOutput2(m_output[1]);
			m_controller->setOutput3(m_output[2]);
			m_controller->setOutput4(m_output[3]);
			return true;
		}
	}

	if (m_controlShutdown == header.getYourCall()) {
		wxLogMessage("Shutdown requested by %s/%s", header.getMyCall1().c_str(), header.getMyCall2().c_str());
		shutdown();
	} else if (m_controlStartup == header.getYourCall()) {
		wxLogMessage("Startup requested by %s/%s", header.getMyCall1().c_str(), header.getMyCall2().c_str());
		startup();
	} else {
		wxLogMessage("Invalid command of %s sent by %s/%s", header.getYourCall().c_str(), header.getMyCall1().c_str(), header.getMyCall2().c_str());
	}

	return true;
}

// ---------------------------------------------------------------------------
// checkAnnouncements() — test the header's RPT1/RPT2 pair against the
// configured announcement record and delete callsign pairs.
//
// Record match: sets m_recording = true; subsequent audio frames are written
//   to the CAnnouncementUnit rather than forwarded.
// Delete match: sets m_deleting = true; audio frames are silently discarded
//   and the existing announcement is deleted.
// Returns true if either match fires (the transmission is consumed by this path).
// ---------------------------------------------------------------------------
bool CDStarRepeaterTRXThread::checkAnnouncements(const CHeaderData& header)
{
	if (m_announcement == nullptr)
		return false;

	if (m_recordRPT1 == header.getRptCall1() && m_recordRPT2 == header.getRptCall2()) {
		wxLogMessage("Announcement creation requested by %s/%s", header.getMyCall1().c_str(), header.getMyCall2().c_str());
		m_announcement->writeHeader(header);
		m_recording = true;
		return true;
	}

	if (m_deleteRPT1 == header.getRptCall1() && m_deleteRPT2 == header.getRptCall2()) {
		wxLogMessage("Announcement deletion requested by %s/%s", header.getMyCall1().c_str(), header.getMyCall2().c_str());
		m_announcement->deleteAnnouncement();
		m_deleting = true;
		return true;
	}

	return false;
}

// ---------------------------------------------------------------------------
// checkHeader() — structural and callsign-format validation.  Returns:
//   STATE_TRUE    — header is acceptable; proceed with the transmission.
//   STATE_FALSE   — header is structurally invalid (wrong RPT1, non-repeater
//                   mode, etc.); transition to DSRS_INVALID and send error reply.
//   STATE_UNKNOWN — header looks ill-formed but silently drop (ack suppressed).
//
// Checks performed:
//   - RPT1 validation / simplex→repeater conversion (m_rpt1Validation flag).
//   - Repeater-mode bit must be set outside of GATEWAY mode.
//   - Gateway mode: reject acks and our own callsigns in YOUR.
//   - MYCALL must not be empty, "NOCALL", "N0CALL", or the repeater callsign.
//   - STN* prefix is exempted from MYCALL checks (used by some terminal modes).
//   - French class-3 novice (F0xxx) and Australian foundation (VKnFxxx) callsigns
//     are rejected because they are not licensed for digital repeater operation.
//   - MYCALL must match the standard amateur callsign regex.
//   - RPT1 must match our own callsign.
//   - Callsign restriction: MYCALL prefix must match the repeater prefix.
// ---------------------------------------------------------------------------
TRISTATE CDStarRepeaterTRXThread::checkHeader(CHeaderData& header)
{
	// If not in RPT1 validation mode, then a simplex header is converted to a proper repeater header
	if (!m_rpt1Validation) {
		if (!header.isRepeaterMode()) {
			// Convert to a properly addressed repeater packet
			header.setRepeaterMode(true);
			header.setRptCall1(m_rptCallsign);
			header.setRptCall2(m_gwyCallsign);
		}
	}

	// The repeater bit must be set when not in gateway mode
	if (m_mode != MODE_GATEWAY) {
		if (!header.isRepeaterMode()) {
			wxLogMessage("Received a non-repeater packet, ignoring");
			return STATE_FALSE;
		}
	} else {
		// Quietly reject acks when in gateway mode
		if (header.isAck() || header.isNoResponse() || header.isRelayUnavailable())
			return STATE_UNKNOWN;

		// As a second check, reject on own UR call
		std::string ur = header.getYourCall();
		if (ur == m_rptCallsign || ur == m_gwyCallsign)
			return STATE_UNKNOWN;

		// Change RPT2 to be the gateway callsign in gateway mode
		header.setRptCall2(m_gwyCallsign);
	}

	std::string my = header.getMyCall1();

	// Make sure MyCall is not empty, a silly value, or the repeater or gateway callsigns, STN* is a special case
	if (my.compare(0, 3, "STN") != 0) {
		if (my == m_rptCallsign ||
			my == m_gwyCallsign ||
			my == "        " ||
			my.compare(0, 6, "NOCALL") == 0 ||
			my.compare(0, 6, "N0CALL") == 0 ||
			my.compare(0, 6, "MYCALL") == 0) {
			wxLogMessage("Invalid MYCALL value of %s, ignoring", my.c_str());
			return STATE_UNKNOWN;
		}
	}

	// Check for a French class 3 novice callsign, and reject
	// Of the form F0xxx
	if (my.compare(0, 2, "F0") == 0) {
		wxLogMessage("French novice class licence callsign found, %s, ignoring", my.c_str());
		return STATE_UNKNOWN;
	}

	// Check for an Australian foundation class licence callsign, and reject
	// Of the form VKnFxxx
	if (my.compare(0, 2, "VK") == 0 && my.length() > 6 && my[3] == 'F' && my[6] != ' ') {
		wxLogMessage("Australian foundation class licence callsign found, %s, ignoring", my.c_str());
		return STATE_UNKNOWN;
	}

	// Check the MyCall value against the regular expression
	bool ok = std::regex_search(my, m_regEx);
	if (!ok) {
		wxLogMessage("Invalid MYCALL value of %s, ignoring", my.c_str());
		return STATE_UNKNOWN;
	}

	// Is it for us?
	if (header.getRptCall1() != m_rptCallsign) {
		wxLogMessage("Invalid RPT1 value %s, ignoring", header.getRptCall1().c_str());
		return STATE_FALSE;
	}

	// If using callsign restriction, validate the my callsign
	if (m_restriction) {
		if (my.substr(0, LONG_CALLSIGN_LENGTH - 1U) != m_rptCallsign.substr(0, LONG_CALLSIGN_LENGTH - 1U)) {
			wxLogMessage("Unauthorised user %s tried to access the repeater", my.c_str());
			return STATE_UNKNOWN;
		}
	}

	return STATE_TRUE;
}

unsigned int CDStarRepeaterTRXThread::countBits(unsigned char byte)
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

// ---------------------------------------------------------------------------
// blankDTMF() — replace the voice portion of an AMBE frame with silence if
// the frame contains a DTMF tone signature.
//
// D-Star embeds DTMF detection info in specific bit positions of the AMBE
// frame.  Applying DTMF_MASK and comparing against DTMF_SIG identifies tones.
// When detected, the 9-byte voice portion is replaced with NULL_AMBE_DATA_BYTES
// (the AMBE-encoded silence pattern), preventing DTMF from being heard on air
// or forwarded to the network.  The 3-byte slow-data field is left intact.
// ---------------------------------------------------------------------------
void CDStarRepeaterTRXThread::blankDTMF(unsigned char* data)
{
	assert(data != nullptr);

	// DTMF begins with these byte values
	if ((data[0] & DTMF_MASK[0]) == DTMF_SIG[0] && (data[1] & DTMF_MASK[1]) == DTMF_SIG[1] &&
		(data[2] & DTMF_MASK[2]) == DTMF_SIG[2] && (data[3] & DTMF_MASK[3]) == DTMF_SIG[3] &&
		(data[4] & DTMF_MASK[4]) == DTMF_SIG[4] && (data[5] & DTMF_MASK[5]) == DTMF_SIG[5] &&
		(data[6] & DTMF_MASK[6]) == DTMF_SIG[6] && (data[7] & DTMF_MASK[7]) == DTMF_SIG[7] &&
		(data[8] & DTMF_MASK[8]) == DTMF_SIG[8])
		::memcpy(data, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
}
