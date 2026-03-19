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

#ifndef	DStarRepeaterTXThread_H
#define	DStarRepeaterTXThread_H

#include "DStarRepeaterThread.h"
#include "OutputQueue.h"
#include "HeaderData.h"
#include "AMBEFEC.h"
#include "Timer.h"
#if defined(MQTT)
#include "MQTTPublisher.h"
#endif

#include "StdCompat.h"
#include <atomic>
#include <chrono>

// CDStarRepeaterTXThread — transmit-only repeater thread (MODE_TXONLY).
//
// Accepts audio streams from the gateway and drives the modem to transmit
// them over RF.  There is no radio receive path.  The modem's read() is called
// each cycle only to drain any unexpected events; its output is ignored.
//
// Uses the same double-buffered network queue and sequence-number gap-filling
// logic as TRXThread.  The state machine is a two-state subset: LISTENING and
// NETWORK.  No ack, timeout, beacon, announcement, or control commands.
class CDStarRepeaterTXThread : public IDStarRepeaterThread {
public:
	CDStarRepeaterTXThread(const std::string& type);
	virtual ~CDStarRepeaterTXThread();

	virtual void setCallsign(const std::string& callsign, const std::string& gateway, DSTAR_MODE mode, ACK_TYPE ack, bool restriction, bool rpt1Validation, bool dtmfBlanking, bool errorReply);
	virtual void setProtocolHandler(CRepeaterProtocolHandler* handler, bool local);
	virtual void setModem(CModem* modem);
	virtual void setController(CExternalController* controller, unsigned int activeHangTime);
	virtual void setTimes(unsigned int timeout, unsigned int ackTime);
	virtual void setBeacon(unsigned int time, const std::string& text, bool voice, TEXT_LANG language);
	virtual void setAnnouncement(bool enabled, unsigned int time, const std::string& recordRPT1, const std::string& recordRPT2, const std::string& deleteRPT1, const std::string& deleteRPT2);
	virtual void setOutputs(bool out1, bool out2, bool out3, bool out4);
	virtual void setLogging(bool logging, const std::string& dir);
	virtual void setWhiteList(CCallsignList* list);
	virtual void setBlackList(CCallsignList* list);
	virtual void setGreyList(CCallsignList* list);

	virtual void shutdown();
	virtual void startup();

	virtual CDStarRepeaterStatusData* getStatus();

	virtual void kill();

	virtual void entry();

private:
	std::string                m_type;
	CModem*                    m_modem;
	CRepeaterProtocolHandler*  m_protocolHandler;
	std::string                m_rptCallsign;
	CHeaderData*               m_txHeader;
	COutputQueue**             m_networkQueue;
	unsigned int               m_writeNum;
	unsigned int               m_readNum;
	unsigned char              m_networkSeqNo;
	CTimer                     m_watchdogTimer;
	CTimer                     m_registerTimer;
	CTimer                     m_statusTimer;
	DSTAR_RPT_STATE            m_state;
	bool                       m_tx;
	unsigned int               m_space;
	std::atomic<bool>          m_killed;
	unsigned char*             m_lastData;
	CAMBEFEC                   m_ambe;
	std::chrono::steady_clock::time_point m_headerTime;
	std::chrono::steady_clock::time_point m_packetTime;
	unsigned int               m_packetCount;
	unsigned int               m_packetSilence;

#if defined(MQTT)
	CTimer                     m_mqttStatusTimer;
#endif

	void transmitNetworkHeader(CHeaderData* header);
	void transmitNetworkData();
	void transmitNetworkHeader();

	void receiveModem();
	void receiveNetwork();
	void processNetworkHeader(CHeaderData* header);
	unsigned int processNetworkFrame(unsigned char* data, unsigned int length, unsigned char seqNo);
	void endOfNetworkData();
	void clock(unsigned int ms);
};

#endif
