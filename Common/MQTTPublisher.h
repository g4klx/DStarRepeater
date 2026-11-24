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
 * The messages are the ones MMDVM-Host publishes for D-Star (same keys,
 * values and timestamps), so MMDVM-Display and anything else written for
 * MMDVM-Host shows DStarRepeater's traffic as it is.
 *
 * These are separate from the periodic status messages on the "status"
 * topic: a display needs discrete start/end events, not continuous
 * state snapshots.
 */

#include "MQTTConnection.h"
#include "Logger.h"

#include <string>
#include <cstdio>
#include <ctime>
#include <sys/time.h>

// Escape a string for safe embedding in a JSON value.
// Callsigns arrive over the air and are untrusted. Quote, backslash and the
// common control characters are escaped; other control characters are
// dropped; DEL and bytes 0x80 and above are replaced with '?', so that a
// malformed header can't produce invalid JSON or invalid UTF-8 (which
// JSON parsers such as MMDVM-Display's reject, dropping the whole message).
static inline std::string jsonEscape(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for (const char ch : s) {
		const unsigned char c = static_cast<unsigned char>(ch);
		if      (c == '"')  out += "\\\"";
		else if (c == '\\') out += "\\\\";
		else if (c == '\n') out += "\\n";
		else if (c == '\r') out += "\\r";
		else if (c == '\t') out += "\\t";
		else if (c >= 0x7FU) out += '?';
		else if (c >= 0x20U) out += ch;
		// drop other non-printable control characters
	}
	return out;
}

// The time now, as MMDVM-Host writes it: UTC, to the millisecond
// ("2026-11-24T15:27:06.314Z")
static inline std::string mqttTimestamp()
{
	struct timeval now;
	::gettimeofday(&now, nullptr);

	struct tm tm;
	::gmtime_r(&now.tv_sec, &tm);

	char buf[64];
	size_t n = ::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
	::snprintf(buf + n, sizeof(buf) - n, ".%03dZ", int(now.tv_usec / 1000));

	return buf;
}

// A callsign without its trailing padding, as MMDVM-Host sends it; leading
// spaces stay ("       E" for echo)
static inline std::string mqttCallsign(const std::string& callsign)
{
	size_t pos = callsign.find_last_not_of(' ');
	if (pos == std::string::npos)
		return std::string();

	return jsonEscape(callsign.substr(0U, pos + 1U));
}

// The call the display has been told about: every start is closed by
// exactly one end or lost (and idle), and only by one for the same source,
// so a rejected RF header during a network call can't end it. Only the
// repeater thread publishes calls. One call is tracked: where RF and
// network calls can overlap (the TX/RX thread), the later start replaces
// the earlier, whose end is then not published.
enum class MQTT_CALL {
	NONE,
	RF,
	NETWORK
};

inline MQTT_CALL g_mqttCall = MQTT_CALL::NONE;

// Publish a mode change ("D-Star", "idle")
static inline void mqttPublishMode(const char* mode)
{
	char buf[128];
	::snprintf(buf, sizeof(buf),
		"{\"MMDVM\":{\"timestamp\":\"%s\",\"mode\":\"%s\"}}",
		mqttTimestamp().c_str(), mode);

	g_mqtt->publish("json", buf);
}

// Publish the start of a D-Star call, from RF or the network: the mode
// change to D-Star (a display switches to its D-Star screen), then the
// call. The reflector is sent empty: MMDVM-Host sends the reflector the
// gateway reports as linked, which the repeater does not know.
static inline void mqttPublishDStarStart(const std::string& myCall1, const std::string& myCall2,
	const std::string& yourCall, MQTT_CALL source)
{
	if (g_mqtt == nullptr)
		return;

	g_mqttCall = source;

	mqttPublishMode("D-Star");

	char buf[512];
	::snprintf(buf, sizeof(buf),
		"{\"D-Star\":{\"timestamp\":\"%s\","
		"\"src_callsign\":\"%s\",\"src_ext\":\"%s\",\"dst_callsign\":\"%s\","
		"\"source\":\"%s\",\"action\":\"start\",\"reflector\":\"\"}}",
		mqttTimestamp().c_str(),
		mqttCallsign(myCall1).c_str(),
		mqttCallsign(myCall2).c_str(),
		mqttCallsign(yourCall).c_str(),
		source == MQTT_CALL::RF ? "rf" : "network");

	g_mqtt->publish("json", buf);
}

// Publish the end of a D-Star call ("end" or "lost"), then idle, if the
// call open is from that source. value is the BER (RF) or the loss
// (network) as a percentage, duration in seconds.
static inline void mqttPublishDStarStop(MQTT_CALL source, const char* action, float duration, float value)
{
	if (g_mqtt == nullptr || g_mqttCall != source)
		return;

	g_mqttCall = MQTT_CALL::NONE;

	// Room for any float: "%.1f" can be up to 312 characters
	char buf[1024];
	::snprintf(buf, sizeof(buf),
		"{\"D-Star\":{\"timestamp\":\"%s\",\"duration\":%.1f,\"%s\":%.1f,\"action\":\"%s\"}}",
		mqttTimestamp().c_str(), double(duration), source == MQTT_CALL::RF ? "ber" : "loss", double(value), action);

	g_mqtt->publish("json", buf);

	mqttPublishMode("idle");
}

// A call received over RF has ended, cleanly or lost
static inline void mqttPublishDStarEndRF(float duration, float ber)
{
	mqttPublishDStarStop(MQTT_CALL::RF, "end", duration, ber);
}

static inline void mqttPublishDStarLostRF(float duration, float ber)
{
	mqttPublishDStarStop(MQTT_CALL::RF, "lost", duration, ber);
}

// A call from the network has ended, cleanly or lost (watchdog timeout)
static inline void mqttPublishDStarEndNet(float duration, float loss)
{
	mqttPublishDStarStop(MQTT_CALL::NETWORK, "end", duration, loss);
}

static inline void mqttPublishDStarLostNet(float duration, float loss)
{
	mqttPublishDStarStop(MQTT_CALL::NETWORK, "lost", duration, loss);
}

// Publish D-Star RSSI value (DVAP signal strength in dBm)
static inline void mqttPublishRSSI(int rssi)
{
	if (g_mqtt == nullptr)
		return;

	char buf[128];
	::snprintf(buf, sizeof(buf),
		"{\"RSSI\":{\"timestamp\":\"%s\",\"mode\":\"D-Star\",\"value\":%d}}",
		mqttTimestamp().c_str(), rssi);

	g_mqtt->publish("json", buf);
}

// Publish D-Star BER value, over the last second as MMDVM-Host does
static inline void mqttPublishBER(float ber)
{
	if (g_mqtt == nullptr)
		return;

	// Room for any float: "%.1f" can be up to 312 characters
	char buf[512];
	::snprintf(buf, sizeof(buf),
		"{\"BER\":{\"timestamp\":\"%s\",\"mode\":\"D-Star\",\"value\":%.1f}}",
		mqttTimestamp().c_str(), double(ber));

	g_mqtt->publish("json", buf);
}

#endif

#endif
