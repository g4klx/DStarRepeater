/*
 *   Copyright (C) 2011-2015 by Jonathan Naylor G4KLX
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

#ifndef	DStarRepeaterTRXThread_H
#define	DStarRepeaterTRXThread_H

#include "DStarRepeaterStatusData.h"
#include "RepeaterProtocolHandler.h"
#include "AnnouncementCallback.h"
#include "DStarRepeaterThread.h"
#include "DStarRepeaterDefs.h"
#include "DVTOOLFileWriter.h"
#include "AnnouncementUnit.h"
#include "SlowDataDecoder.h"
#include "SlowDataEncoder.h"
#include "BeaconCallback.h"
#include "CallsignList.h"
#include "OutputQueue.h"
#include "BeaconUnit.h"
#include "HeaderData.h"
#include "AMBEFEC.h"
#include "Timer.h"
#include "Utils.h"
#if defined(MQTT)
#include "MQTTPublisher.h"
#endif

#include "StdCompat.h"
#include <atomic>
#include <regex>
#include <chrono>

// CDStarRepeaterTRXThread — the primary repeater thread used for DUPLEX,
// SIMPLEX, and GATEWAY modes.  It owns both the receive path (radio → network)
// and the transmit path (network → radio) and is the only thread variant that
// implements the full repeater state machine, DTMF blanking, ack/status
// transmissions, beacons, announcements, and DTMF-triggered control commands.
//
// Implements IBeaconCallback and IAnnouncementCallback so that CBeaconUnit and
// CAnnouncementUnit can push pre-built audio frames back into the local queue.
class CDStarRepeaterTRXThread : public IDStarRepeaterThread, public IBeaconCallback, public IAnnouncementCallback {
public:
	CDStarRepeaterTRXThread(const std::string& type);
	virtual ~CDStarRepeaterTRXThread();

	virtual void setCallsign(const std::string& callsign, const std::string& gateway, DSTAR_MODE mode, ACK_TYPE ack, bool restriction, bool rpt1Validation, bool dtmfBlanking, bool errorReply);
	virtual void setProtocolHandler(CRepeaterProtocolHandler* handler, bool local);
	virtual void setModem(CModem* modem);
	virtual void setController(CExternalController* controller, unsigned int activeHangTime);
	virtual void setTimes(unsigned int timeout, unsigned int ackTime);
	virtual void setBeacon(unsigned int time, const std::string& text, bool voice, TEXT_LANG language);
	virtual void setAnnouncement(bool enabled, unsigned int time, const std::string& recordRPT1, const std::string& recordRPT2, const std::string& deleteRPT1, const std::string& deleteRPT2);

	virtual void setControl(bool enabled, const std::string& rpt1Callsign,
		const std::string& rpt2Callsign, const std::string& shutdown,
		const std::string& startup, const std::vector<std::string>& command,
		const std::vector<std::string>& commandLine,
		const std::vector<std::string>& status, const std::vector<std::string>& outputs
	);

	virtual void setOutputs(bool out1, bool out2, bool out3, bool out4);
	virtual void setLogging(bool logging, const std::string& dir);
	virtual void setWhiteList(CCallsignList* list);
	virtual void setBlackList(CCallsignList* list);
	virtual void setGreyList(CCallsignList* list);

	virtual void shutdown();
	virtual void startup();

	virtual CDStarRepeaterStatusData* getStatus();

	virtual void entry();

	virtual void kill();

	virtual void transmitBeaconHeader();
	virtual void transmitBeaconData(const unsigned char* data, unsigned int length, bool end);

	virtual void transmitAnnouncementHeader(CHeaderData* header);
	virtual void transmitAnnouncementData(const unsigned char* data, unsigned int length, bool end);

private:
	std::string                m_type;              // Modem type string (e.g. "MMDVM"), used to special-case DVAP status
	CModem*                    m_modem;
	CRepeaterProtocolHandler*  m_protocolHandler;   // UDP link to ircDDB/DStarGateway; null when no gateway is configured
	CExternalController*       m_controller;        // PTT / active-indicator hardware
	std::string                m_rptCallsign;       // Our 8-char RPT1 callsign (padded with spaces)
	std::string                m_gwyCallsign;       // Gateway callsign (RPT2); defaults to rpt+G suffix
	CBeaconUnit*               m_beacon;            // Synthesises periodic ID transmissions; null if beacons are off
	CAnnouncementUnit*         m_announcement;      // Plays back a pre-recorded audio clip periodically; null if disabled
	std::string                m_recordRPT1;        // RPT1/RPT2 callsign pair that triggers announcement recording
	std::string                m_recordRPT2;
	std::string                m_deleteRPT1;        // RPT1/RPT2 pair that triggers announcement deletion
	std::string                m_deleteRPT2;
	CHeaderData*               m_rxHeader;          // Header from the currently active transmission (RF or network)

	// Three output queues feed the modem.  Priority: radio > local > network.
	COutputQueue               m_localQueue;        // Beacon / announcement / ack / status transmissions
	COutputQueue               m_radioQueue;        // RF → RF retransmit (duplex mode only)
	COutputQueue**             m_networkQueue;      // Double-buffered network → RF queue (NETWORK_QUEUE_COUNT slots)
	unsigned int               m_writeNum;          // Index of the queue slot currently being written by receiveNetwork()
	unsigned int               m_readNum;           // Index of the slot currently being drained to the modem

	unsigned char              m_radioSeqNo;        // Frame sequence counter (0..20) for sync regeneration on the RF path
	unsigned char              m_networkSeqNo;      // Frame sequence counter for the network path (used for gap filling)
	unsigned char              m_lastSlowDataType;  // Tracks the previous slow-data type byte (gateway header substitution)

	// Timers — all clocked via clock() every ~9 ms
	CTimer                     m_timeoutTimer;      // Maximum on-air time per transmission; fires → DSRS_TIMEOUT
	CTimer                     m_watchdogTimer;     // Network data watchdog; fires if gateway stops sending frames
	CTimer                     m_pollTimer;         // Periodic gateway keepalive poll (60 s)
	CTimer                     m_ackTimer;          // Delay between end-of-transmission and the ack/status reply (0.5 s)

	CTimer                     m_statusAnnounceTimer[5]; // Per-status short delay before transmitting a user-status reply

	CTimer                     m_beaconTimer;       // Interval between beacon transmissions (configurable, default 10 min)
	CTimer                     m_announcementTimer; // Interval between announcement playbacks

	CTimer                     m_statusTimer;       // Throttle for modem space/TX-state polling (100 ms)
	CTimer                     m_heartbeatTimer;    // 1 s pulse to drive the controller heartbeat output

	// Repeater state machine — the two axes of state
	DSTAR_RPT_STATE            m_rptState;  // Overall repeater state: LISTENING / VALID / TIMEOUT / NETWORK / SHUTDOWN / …
	DSTAR_RX_STATE             m_rxState;   // Radio receive state: LISTENING / PROCESS_SLOW_DATA / PROCESS_DATA

	CSlowDataDecoder           m_slowDataDecoder;   // Extracts embedded header from slow data (no fast-data header case)
	CSlowDataEncoder           m_ackEncoder;        // Encodes BER / link text into the ack slow-data stream
	CSlowDataEncoder           m_linkEncoder;       // Encodes link/error status for error-reply transmissions
	CSlowDataEncoder           m_headerEncoder;     // Re-encodes the modified header into the gateway slow-data stream

	CSlowDataEncoder           m_status1Encoder;    // Encoders for the five user-defined status messages
	CSlowDataEncoder           m_status2Encoder;
	CSlowDataEncoder           m_status3Encoder;
	CSlowDataEncoder           m_status4Encoder;
	CSlowDataEncoder           m_status5Encoder;

	std::vector<std::string>   m_statusText;        // Five status strings received from the gateway

	bool                       m_tx;               // Cached modem TX state (refreshed every 100 ms via m_statusTimer)
	unsigned int               m_space;            // Cached modem buffer space (frames available to write)
	std::atomic<bool>          m_killed;           // Set by kill() to break the entry() loop
	DSTAR_MODE                 m_mode;             // DUPLEX / SIMPLEX / GATEWAY — affects relay and ack behaviour
	ACK_TYPE                   m_ack;              // AT_NONE / AT_BER / AT_TEXT — what to send after a valid QSO
	bool                       m_restriction;      // When true, only callsigns matching the repeater prefix are allowed
	bool                       m_rpt1Validation;   // When false, simplex headers are re-addressed as repeater headers
	bool                       m_errorReply;       // When true, send an error-status reply to rejected transmissions
	bool                       m_controlEnabled;   // DTMF/YSFSF control is active
	std::string                m_controlRPT1;      // RPT1 callsign that must match for a control command to be accepted
	std::string                m_controlRPT2;      // RPT2 callsign that must match
	std::string                m_controlShutdown;  // YOUR callsign that triggers a software shutdown
	std::string                m_controlStartup;   // YOUR callsign that cancels a software shutdown

	std::vector<std::string>   m_controlStatus;     // YOUR callsigns that trigger status announcements (up to 5)
	std::vector<std::string>   m_controlCommand;    // YOUR callsigns that trigger shell commands (up to 6)
	std::vector<std::string>   m_controlCommandLine;// Shell command strings paired with m_controlCommand entries

	std::vector<std::string>   m_controlOutput;     // YOUR callsigns that toggle physical output lines (up to 4)

	bool                       m_output[4];         // Current state of the four configurable output lines

	CTimer                     m_activeHangTimer;   // Keeps the active indicator asserted briefly after the QSO ends
	bool                       m_shutdown;          // Set by shutdown() / cleared by startup() via DTMF or UI
	bool                       m_disable;           // Mirrors the hardware disable input from the controller
	CDVTOOLFileWriter*         m_logging;           // Optional per-QSO DVTOOL frame log; null when logging is disabled

	// AMBE BER tracking — accumulated per transmission, reset on each new header
	unsigned char*             m_lastData;          // Copy of the most recent network frame (used to fill gaps)
	CAMBEFEC                   m_ambe;              // AMBE FEC regenerator / bit-error counter
	unsigned int               m_ambeFrames;        // Total voice frames received in this transmission
	unsigned int               m_ambeSilence;       // Number of frames that were silence (null AMBE)
	unsigned int               m_ambeBits;          // Cumulative FEC bits examined (denominator for BER)
	unsigned int               m_ambeErrors;        // Cumulative FEC bit errors (numerator for BER)
	unsigned int               m_lastAMBEBits;      // Snapshot taken at the last getStatus() call (for incremental BER)
	unsigned int               m_lastAMBEErrors;

	std::string                m_ackText;           // Slow-data text received from the gateway (e.g. reflector name)
	std::string                m_tempAckText;       // One-shot override ack text; cleared after use
	LINK_STATUS                m_linkStatus;        // Current reflector link state (used to format the BER ack string)
	std::string                m_reflector;         // Name of the linked reflector, if any

	// Callsign validation regex: matches standard amateur callsigns (e.g. G4KLX, M0XYZ, VK3ABC)
	std::regex                 m_regEx;

	// Packet timing for network gap detection — used to insert silence frames when UDP packets are late
	std::chrono::steady_clock::time_point m_headerTime;   // When the current network header arrived
	std::chrono::steady_clock::time_point m_packetTime;   // When the last network data packet arrived
	unsigned int               m_packetCount;             // Total frames processed in this network transmission
	unsigned int               m_packetSilence;           // Frames inserted as silence due to loss or gaps

	CCallsignList*             m_whiteList;   // If set, only callsigns in the list may access the repeater
	CCallsignList*             m_blackList;   // If set, callsigns in the list are always rejected
	CCallsignList*             m_greyList;    // If set, matching callsigns access RF but are blocked from the network
	bool                       m_blocked;     // True when the current user is on the grey list (local only)
	bool                       m_busyData;    // True when the repeater is in NETWORK state and an RF user is transmitting
	bool                       m_blanking;    // When true, DTMF tones in the audio stream are muted before retransmission
	bool                       m_recording;   // True while recording an announcement from the current RF transmission
	bool                       m_deleting;    // True while absorbing (discarding) a delete-announcement transmission

#if defined(MQTT)
	CTimer                     m_mqttStatusTimer;   // 1 s timer driving MQTT status/BER publication
#endif

	void receiveHeader(CHeaderData* header);
	void receiveRadioData(unsigned char* data, unsigned int length);
	void receiveSlowData(unsigned char* data, unsigned int length);
	void transmitRadioHeader(CHeaderData* header);
	void transmitLocalHeader(CHeaderData* header);
	void transmitNetworkHeader(CHeaderData* header);
	void transmitStatus();
	void transmitErrorStatus();
	void transmitUserStatus(unsigned int n);
	void transmitLocalData();
	void transmitRadioData();
	void transmitNetworkData();
	void transmitLocalHeader();
	void transmitRadioHeader();
	void transmitNetworkHeader();

	void repeaterStateMachine();
	void receiveModem();
	void receiveNetwork();
	bool processRadioHeader(CHeaderData* header);
	void processNetworkHeader(CHeaderData* header);
	void processRadioFrame(unsigned char* data, FRAME_TYPE type);
	unsigned int processNetworkFrame(unsigned char* data, unsigned int length, unsigned char seqNo);
	void endOfRadioData();
	void endOfNetworkData();
	void setRadioState(DSTAR_RX_STATE state);
	bool setRepeaterState(DSTAR_RPT_STATE state);
	bool checkControl(const CHeaderData& header);
	bool checkAnnouncements(const CHeaderData& header);
	TRISTATE checkHeader(CHeaderData& header);
	unsigned int countBits(unsigned char byte);
	void clock(unsigned int ms);
	void blankDTMF(unsigned char* data);
};

#endif
