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
 * Display-Driver-compatible MQTT JSON publishing for DStarRepeater.
 *
 * Publishes event-driven JSON messages to the "json" topic in the format
 * expected by Display-Driver, enabling LCD/OLED display output.
 *
 * These are separate from the periodic status messages on the "status"
 * topic — the display needs discrete start/end events, not continuous
 * state snapshots.
 */

#include "MQTTConnection.h"
#include "Logger.h"

#include <wx/wx.h>

#include <cstdio>

// Publish a D-Star call start event (RF or network)
static inline void mqttPublishDStarStart(const wxString& myCall1, const wxString& myCall2,
	const wxString& yourCall, const wxString& rptCall2, const char* source)
{
	if (g_mqtt == NULL)
		return;

	char buf[512];
	::snprintf(buf, sizeof(buf),
		"{\"D-Star\":{\"action\":\"start\","
		"\"source_cs\":\"%s\",\"source_ext\":\"%s\","
		"\"destination_cs\":\"%s\",\"reflector\":\"%s\","
		"\"source\":\"%s\"}}",
		(const char*)myCall1.mb_str(),
		(const char*)myCall2.mb_str(),
		(const char*)yourCall.mb_str(),
		(const char*)rptCall2.mb_str(),
		source);

	g_mqtt->publish("json", buf);
}

// Publish a D-Star call end event (normal termination)
static inline void mqttPublishDStarEnd()
{
	if (g_mqtt == NULL)
		return;

	g_mqtt->publish("json", "{\"D-Star\":{\"action\":\"end\"}}");
}

// Publish a D-Star call lost event (watchdog timeout / abnormal)
static inline void mqttPublishDStarLost()
{
	if (g_mqtt == NULL)
		return;

	g_mqtt->publish("json", "{\"D-Star\":{\"action\":\"lost\"}}");
}

// Publish idle mode (no traffic)
static inline void mqttPublishIdle()
{
	if (g_mqtt == NULL)
		return;

	g_mqtt->publish("json", "{\"MMDVM\":{\"mode\":\"idle\"}}");
}

// Publish D-Star BER value
static inline void mqttPublishBER(float ber)
{
	if (g_mqtt == NULL)
		return;

	char buf[128];
	::snprintf(buf, sizeof(buf),
		"{\"BER\":{\"mode\":\"D-Star\",\"value\":%.1f}}",
		ber);

	g_mqtt->publish("json", buf);
}

// Publish D-Star slow data text
static inline void mqttPublishText(const wxString& text)
{
	if (g_mqtt == NULL)
		return;

	char buf[256];
	::snprintf(buf, sizeof(buf),
		"{\"Text\":{\"mode\":\"D-Star\",\"value\":\"%s\"}}",
		(const char*)text.mb_str());

	g_mqtt->publish("json", buf);
}

#endif

#endif
