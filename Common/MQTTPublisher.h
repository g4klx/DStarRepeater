/*
 *   Copyright (C) 2025 by Andy Taylor MW0MWZ
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

#ifndef	MQTTPublisher_H
#define	MQTTPublisher_H

#if defined(MQTT)

/*
 * Inline helper functions that format and publish D-Star events as JSON to the
 * MQTT broker via the global g_mqtt (CMQTTConnection*) instance.
 *
 * All helpers are no-ops when g_mqtt is nullptr (MQTT not configured) or when
 * the broker is not connected.  They publish to the "json" sub-topic, which is
 * automatically prefixed with the configured repeater name by CMQTTConnection.
 *
 * Display-Driver-compatible MQTT JSON publishing for DStarRepeater:
 * Publishes event-driven JSON messages to the "json" topic in the format
 * expected by Display-Driver, enabling LCD/OLED display output.
 *
 * These are separate from the periodic status messages on the "status"
 * topic — the display needs discrete start/end events, not continuous
 * state snapshots.
 */

#include "MQTTConnection.h"
#include "Logger.h"

#include <string>
#include <cstdio>

// Escape a string for safe embedding in a JSON value.
// Handles the characters that would break JSON string syntax or silently
// corrupt the document: backslash, double-quote, and the common C0 controls.
// Non-printable control characters other than \n/\r/\t are dropped entirely
// so that malformed radio headers cannot produce invalid JSON.
static inline std::string jsonEscape(const std::string& s)
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

// Publish a D-Star call start event (RF or network)
static inline void mqttPublishDStarStart(const std::string& myCall1, const std::string& myCall2,
	const std::string& yourCall, const std::string& rptCall2, const char* source)
{
	if (g_mqtt == nullptr)
		return;

	// Escape all callsign strings: over-the-air headers are untrusted input
	// and may contain characters (e.g. '"' or '\') that break JSON syntax.
	// 'source' is a string literal supplied by our own code, not radio data.
	const std::string cs1 = jsonEscape(myCall1);
	const std::string cs2 = jsonEscape(myCall2);
	const std::string yc  = jsonEscape(yourCall);
	const std::string rc2 = jsonEscape(rptCall2);

	char buf[512];
	::snprintf(buf, sizeof(buf),
		"{\"D-Star\":{\"action\":\"start\","
		"\"source_cs\":\"%s\",\"source_ext\":\"%s\","
		"\"destination_cs\":\"%s\",\"reflector\":\"%s\","
		"\"source\":\"%s\"}}",
		cs1.c_str(),
		cs2.c_str(),
		yc.c_str(),
		rc2.c_str(),
		source);

	g_mqtt->publish("json", buf);
}

// Publish a D-Star call end event (normal termination)
static inline void mqttPublishDStarEnd()
{
	if (g_mqtt == nullptr)
		return;

	g_mqtt->publish("json", "{\"D-Star\":{\"action\":\"end\"}}");
}

// Publish a D-Star call lost event (watchdog timeout / abnormal)
static inline void mqttPublishDStarLost()
{
	if (g_mqtt == nullptr)
		return;

	g_mqtt->publish("json", "{\"D-Star\":{\"action\":\"lost\"}}");
}

// Publish idle mode (no traffic)
static inline void mqttPublishIdle()
{
	if (g_mqtt == nullptr)
		return;

	g_mqtt->publish("json", "{\"MMDVM\":{\"mode\":\"idle\"}}");
}

// Publish D-Star RSSI value (DVAP signal strength in dBm)
static inline void mqttPublishRSSI(int rssi)
{
	if (g_mqtt == nullptr)
		return;

	char buf[128];
	::snprintf(buf, sizeof(buf),
		"{\"RSSI\":{\"mode\":\"D-Star\",\"value\":%d}}",
		rssi);

	g_mqtt->publish("json", buf);
}

// Publish D-Star BER value
static inline void mqttPublishBER(float ber)
{
	if (g_mqtt == nullptr)
		return;

	char buf[128];
	::snprintf(buf, sizeof(buf),
		"{\"BER\":{\"mode\":\"D-Star\",\"value\":%.1f}}",
		ber);

	g_mqtt->publish("json", buf);
}

// Publish D-Star slow data text
static inline void mqttPublishText(const std::string& text)
{
	if (g_mqtt == nullptr)
		return;

	// Slow-data text arrives over the air and is untrusted; escape it so
	// that a crafted transmission cannot produce malformed JSON.
	const std::string escaped = jsonEscape(text);

	char buf[256];
	::snprintf(buf, sizeof(buf),
		"{\"Text\":{\"mode\":\"D-Star\",\"value\":\"%s\"}}",
		escaped.c_str());

	g_mqtt->publish("json", buf);
}

#endif

#endif
