/*
 *   Copyright (C) 2011,2012,2013 by Jonathan Naylor G4KLX
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

#ifndef	DStarRepeaterStatusData_H
#define	DStarRepeaterStatusData_H

#include "DStarRepeaterDefs.h"
#include "DStarDefines.h"

#include "StdCompat.h"

// CDStarRepeaterStatusData — an immutable snapshot of repeater state captured
// at a single point in time.
//
// Created by each thread's getStatus() method and consumed by:
//   - The GUI display panel (polling via a timer).
//   - The MQTT publisher (toJSON(), published every 1 s to {name}/status).
//
// Contains the D-Star header fields of the current transmission, the TX flag,
// both state-machine states, timer values for the timeout/beacon/announcement
// countdown displays, the current BER or loss percentage, the gateway-supplied
// ack text, and the five status strings.
//
// For DVAP modems, squelch and signal-strength fields are added via setDVAP().
class CDStarRepeaterStatusData {
public:
	CDStarRepeaterStatusData(const std::string& myCall1, const std::string& myCall2, const std::string& yourCall,
							  const std::string& rptCall1, const std::string& rptCall2, unsigned char flag1,
							  unsigned char flag2, unsigned char flag3, bool tx, DSTAR_RX_STATE rxState,
							  DSTAR_RPT_STATE rptState, unsigned int timeoutTimer, unsigned int timeoutExpiry,
							  unsigned int beaconTimer, unsigned int beaconExpiry, unsigned int announceTimer,
							  unsigned int announceExpiry, float percent, const std::string& text,
							  const std::string& status1, const std::string& status2, const std::string& status3,
							  const std::string& status4, const std::string& status5);
	~CDStarRepeaterStatusData();

	void setDVAP(bool squelch, int signal);

	std::string   getMyCall1() const;
	std::string   getMyCall2() const;
	std::string   getYourCall() const;
	std::string   getRptCall1() const;
	std::string   getRptCall2() const;
	unsigned char getFlag1() const;
	unsigned char getFlag2() const;
	unsigned char getFlag3() const;

	bool          getTX() const;
	bool          getSquelch() const;
	int           getSignal() const;

	DSTAR_RPT_STATE getRptState() const;
	DSTAR_RX_STATE  getRxState() const;

	unsigned int  getTimeoutTimer() const;
	unsigned int  getTimeoutExpiry() const;

	unsigned int  getBeaconTimer() const;
	unsigned int  getBeaconExpiry() const;

	unsigned int  getAnnounceTimer() const;
	unsigned int  getAnnounceExpiry() const;

	float         getPercent() const;

	std::string   getText() const;
	std::string   getStatus1() const;
	std::string   getStatus2() const;
	std::string   getStatus3() const;
	std::string   getStatus4() const;
	std::string   getStatus5() const;

#if defined(MQTT)
	std::string   toJSON() const;
#endif

private:
	std::string     m_myCall1;
	std::string     m_myCall2;
	std::string     m_yourCall;
	std::string     m_rptCall1;
	std::string     m_rptCall2;
	unsigned char   m_flag1;
	unsigned char   m_flag2;
	unsigned char   m_flag3;
	bool            m_tx;
	DSTAR_RX_STATE  m_rxState;
	DSTAR_RPT_STATE m_rptState;
	unsigned int    m_timeoutTimer;
	unsigned int    m_timeoutExpiry;
	unsigned int    m_beaconTimer;
	unsigned int    m_beaconExpiry;
	unsigned int    m_announceTimer;
	unsigned int    m_announceExpiry;
	float           m_percent;
	std::string     m_text;
	std::string     m_status1;
	std::string     m_status2;
	std::string     m_status3;
	std::string     m_status4;
	std::string     m_status5;

	// DVAP
	bool            m_squelch;
	int             m_signal;
};

#endif
