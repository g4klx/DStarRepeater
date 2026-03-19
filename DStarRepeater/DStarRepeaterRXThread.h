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

#ifndef	DStarRepeaterRXThread_H
#define	DStarRepeaterRXThread_H

#include "DStarRepeaterThread.h"
#include "SlowDataDecoder.h"
#include "HeaderData.h"
#include "AMBEFEC.h"
#include "Timer.h"
#if defined(MQTT)
#include "MQTTPublisher.h"
#endif

#include "StdCompat.h"
#include <atomic>

// CDStarRepeaterRXThread — receive-only repeater thread (MODE_RXONLY).
//
// Listens for RF transmissions, validates the header, counts AMBE BER, and
// forwards every frame to the gateway via the protocol handler.  There is no
// local transmit queue, no ack, no beacon, and no controller PTT.  The gateway
// link is required (a protocol handler must be set before entry() will run).
//
// Compared with TRXThread the state machine is trivial: LISTENING ↔ VALID
// with no timeout, ack-wait, or network states.
class CDStarRepeaterRXThread : public IDStarRepeaterThread {
public:
	CDStarRepeaterRXThread(const std::string& type);
	virtual ~CDStarRepeaterRXThread();

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
	CHeaderData*               m_rxHeader;
	unsigned char              m_radioSeqNo;
	CTimer                     m_registerTimer;
	DSTAR_RPT_STATE            m_rptState;
	DSTAR_RX_STATE             m_rxState;
	CSlowDataDecoder           m_slowDataDecoder;
	std::atomic<bool>          m_killed;
	CAMBEFEC                   m_ambe;
	unsigned int               m_ambeFrames;
	unsigned int               m_ambeSilence;
	unsigned int               m_ambeBits;
	unsigned int               m_ambeErrors;
	unsigned int               m_lastAMBEBits;
	unsigned int               m_lastAMBEErrors;

#if defined(MQTT)
	CTimer                     m_mqttStatusTimer;
#endif

	void receiveHeader(CHeaderData* header);
	void receiveRadioData(unsigned char* data, unsigned int length);
	void receiveSlowData(unsigned char* data, unsigned int length);

	void receiveModem();
	void receiveNetwork();
	bool processRadioHeader(CHeaderData* header);
	void processRadioFrame(unsigned char* data, FRAME_TYPE type);
	void endOfRadioData();
	void setRadioState(DSTAR_RX_STATE state);
	unsigned int countBits(unsigned char byte);
	void clock(unsigned int ms);
};

#endif
