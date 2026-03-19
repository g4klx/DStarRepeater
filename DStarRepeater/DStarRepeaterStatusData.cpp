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

#include "DStarRepeaterStatusData.h"

#if defined(MQTT)
#include <cstdio>
#include <string>

// Escape a string for safe embedding in a JSON value.
// Identical to the copy in MQTTPublisher.h — kept local to avoid a header
// dependency between DStarRepeater/ and Common/.
static std::string jsonEscape(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for (char c : s) {
		if      (c == '"')  out += "\\\"";
		else if (c == '\\') out += "\\\\";
		else if (c == '\n') out += "\\n";
		else if (c == '\r') out += "\\r";
		else if (c == '\t') out += "\\t";
		else if (c >= 0x20) out += c;
		// drop other non-printable control characters
	}
	return out;
}
#endif

CDStarRepeaterStatusData::CDStarRepeaterStatusData(const std::string& myCall1, const std::string& myCall2,
													 const std::string& yourCall, const std::string& rptCall1,
													 const std::string& rptCall2, unsigned char flag1,
													 unsigned char flag2, unsigned char flag3, bool tx,
													 DSTAR_RX_STATE rxState, DSTAR_RPT_STATE rptState,
													 unsigned int timeoutTimer, unsigned int timeoutExpiry,
													 unsigned int beaconTimer, unsigned int beaconExpiry,
													 unsigned int announceTimer, unsigned int announceExpiry,
													 float percent, const std::string& text, const std::string& status1,
													 const std::string& status2, const std::string& status3,
													 const std::string& status4, const std::string& status5) :
m_myCall1(myCall1),
m_myCall2(myCall2),
m_yourCall(yourCall),
m_rptCall1(rptCall1),
m_rptCall2(rptCall2),
m_flag1(flag1),
m_flag2(flag2),
m_flag3(flag3),
m_tx(tx),
m_rxState(rxState),
m_rptState(rptState),
m_timeoutTimer(timeoutTimer),
m_timeoutExpiry(timeoutExpiry),
m_beaconTimer(beaconTimer),
m_beaconExpiry(beaconExpiry),
m_announceTimer(announceTimer),
m_announceExpiry(announceExpiry),
m_percent(percent),
m_text(text),
m_status1(status1),
m_status2(status2),
m_status3(status3),
m_status4(status4),
m_status5(status5),
m_squelch(false),
m_signal(0)
{
}

CDStarRepeaterStatusData::~CDStarRepeaterStatusData()
{
}

void CDStarRepeaterStatusData::setDVAP(bool squelch, int signal)
{
	m_squelch = squelch;
	m_signal  = signal;
}

std::string CDStarRepeaterStatusData::getMyCall1() const
{
	return m_myCall1;
}

std::string CDStarRepeaterStatusData::getMyCall2() const
{
	return m_myCall2;
}

std::string CDStarRepeaterStatusData::getYourCall() const
{
	return m_yourCall;
}

std::string CDStarRepeaterStatusData::getRptCall1() const
{
	return m_rptCall1;
}

std::string CDStarRepeaterStatusData::getRptCall2() const
{
	return m_rptCall2;
}

unsigned char CDStarRepeaterStatusData::getFlag1() const
{
	return m_flag1;
}

unsigned char CDStarRepeaterStatusData::getFlag2() const
{
	return m_flag2;
}

unsigned char CDStarRepeaterStatusData::getFlag3() const
{
	return m_flag3;
}

bool CDStarRepeaterStatusData::getTX() const
{
	return m_tx;
}

bool CDStarRepeaterStatusData::getSquelch() const
{
	return m_squelch;
}

int CDStarRepeaterStatusData::getSignal() const
{
	return m_signal;
}

DSTAR_RX_STATE CDStarRepeaterStatusData::getRxState() const
{
	return m_rxState;
}

DSTAR_RPT_STATE CDStarRepeaterStatusData::getRptState() const
{
	return m_rptState;
}

unsigned int CDStarRepeaterStatusData::getTimeoutTimer() const
{
	return m_timeoutTimer;
}

unsigned int CDStarRepeaterStatusData::getTimeoutExpiry() const
{
	return m_timeoutExpiry;
}

unsigned int CDStarRepeaterStatusData::getBeaconTimer() const
{
	return m_beaconTimer;
}

unsigned int CDStarRepeaterStatusData::getBeaconExpiry() const
{
	return m_beaconExpiry;
}

unsigned int CDStarRepeaterStatusData::getAnnounceTimer() const
{
	return m_announceTimer;
}

unsigned int CDStarRepeaterStatusData::getAnnounceExpiry() const
{
	return m_announceExpiry;
}

float CDStarRepeaterStatusData::getPercent() const
{
	return m_percent;
}

std::string CDStarRepeaterStatusData::getText() const
{
	return m_text;
}

std::string CDStarRepeaterStatusData::getStatus1() const
{
	return m_status1;
}

std::string CDStarRepeaterStatusData::getStatus2() const
{
	return m_status2;
}

std::string CDStarRepeaterStatusData::getStatus3() const
{
	return m_status3;
}

std::string CDStarRepeaterStatusData::getStatus4() const
{
	return m_status4;
}

std::string CDStarRepeaterStatusData::getStatus5() const
{
	return m_status5;
}

// toJSON() serialises the snapshot to a JSON string for MQTT publication.
// The output format is consumed by Display-Driver and similar monitoring tools.
#if defined(MQTT)
static const char* rptStateToString(DSTAR_RPT_STATE state)
{
	switch (state) {
		case DSRS_SHUTDOWN:     return "shutdown";
		case DSRS_LISTENING:    return "listening";
		case DSRS_VALID:        return "valid";
		case DSRS_VALID_WAIT:   return "valid_wait";
		case DSRS_INVALID:      return "invalid";
		case DSRS_INVALID_WAIT: return "invalid_wait";
		case DSRS_TIMEOUT:      return "timeout";
		case DSRS_TIMEOUT_WAIT: return "timeout_wait";
		case DSRS_NETWORK:      return "network";
		default:                return "unknown";
	}
}

static const char* rxStateToString(DSTAR_RX_STATE state)
{
	switch (state) {
		case DSRXS_LISTENING:         return "listening";
		case DSRXS_PROCESS_DATA:      return "process_data";
		case DSRXS_PROCESS_SLOW_DATA: return "process_slow_data";
		default:                      return "unknown";
	}
}

std::string CDStarRepeaterStatusData::toJSON() const
{
	// Escape every field that originates from over-the-air data or gateway
	// text to prevent JSON injection.  The state-machine strings and the
	// boolean are produced by our own code and need no escaping.
	const std::string mc1 = jsonEscape(m_myCall1);
	const std::string mc2 = jsonEscape(m_myCall2);
	const std::string yc  = jsonEscape(m_yourCall);
	const std::string rc1 = jsonEscape(m_rptCall1);
	const std::string rc2 = jsonEscape(m_rptCall2);
	const std::string txt = jsonEscape(m_text);
	const std::string s1  = jsonEscape(m_status1);
	const std::string s2  = jsonEscape(m_status2);
	const std::string s3  = jsonEscape(m_status3);
	const std::string s4  = jsonEscape(m_status4);
	const std::string s5  = jsonEscape(m_status5);

	char buffer[2048];
	::snprintf(buffer, sizeof(buffer),
		"{\"myCall1\":\"%s\",\"myCall2\":\"%s\","
		"\"yourCall\":\"%s\",\"rptCall1\":\"%s\",\"rptCall2\":\"%s\","
		"\"tx\":%s,\"rxState\":\"%s\",\"rptState\":\"%s\","
		"\"ber\":%.1f,"
		"\"text\":\"%s\","
		"\"status1\":\"%s\",\"status2\":\"%s\",\"status3\":\"%s\","
		"\"status4\":\"%s\",\"status5\":\"%s\"}",
		mc1.c_str(),
		mc2.c_str(),
		yc.c_str(),
		rc1.c_str(),
		rc2.c_str(),
		m_tx ? "true" : "false",
		rxStateToString(m_rxState),
		rptStateToString(m_rptState),
		m_percent,
		txt.c_str(),
		s1.c_str(),
		s2.c_str(),
		s3.c_str(),
		s4.c_str(),
		s5.c_str());

	return std::string(buffer);
}
#endif
