/*
 *   Copyright (C) 2022,2023,2025 by Jonathan Naylor G4KLX
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

#if !defined(MQTTCONNECTION_H)
#define	MQTTCONNECTION_H

#if defined(MQTT)

#include <mosquitto.h>

#include <atomic>
#include <vector>
#include <string>

/*
 * Thin wrapper around the libmosquitto client for publish-only MQTT telemetry.
 *
 * open() initialises the mosquitto client, registers callbacks, and starts the
 * internal mosquitto network loop thread (mosquitto_loop_start).  After open()
 * returns, publish() can be called from any thread; mosquitto handles its own
 * thread safety internally.
 *
 * Topics without a '/' are automatically prefixed with the configured name
 * (e.g. "log" becomes "MyRepeater/log").  Topics containing '/' are used as-is.
 *
 * Optional subscriptions can be provided via the subs vector; each entry pairs
 * a topic string with a callback function invoked on incoming messages.
 *
 * This class is compiled only when MQTT=1 is passed to make.
 */

enum class MQTT_QOS : int {
	AT_MODE_ONCE  = 0,
	AT_LEAST_ONCE = 1,
	EXACTLY_ONCE  = 2
};

class CMQTTConnection {
public:
	CMQTTConnection(const std::string& host, unsigned short port, const std::string& name, const bool authEnabled, const std::string& username, const std::string& password, const std::vector<std::pair<std::string, void (*)(const unsigned char*, unsigned int)>>& subs, unsigned int keepalive, MQTT_QOS qos = MQTT_QOS::EXACTLY_ONCE);
	~CMQTTConnection();

	bool open();

	bool publish(const char* topic, const char* text);
	bool publish(const char* topic, const std::string& text);
	bool publish(const char* topic, const unsigned char* data, unsigned int len);

	void close();

private:
	std::string    m_host;
	unsigned short m_port;
	std::string    m_name;        // Prefix prepended to bare topic strings.
	bool           m_authEnabled;
	std::string    m_username;
	std::string    m_password;
	std::vector<std::pair<std::string, void (*)(const unsigned char*, unsigned int)>> m_subs;
	unsigned int   m_keepalive;
	MQTT_QOS       m_qos;
	mosquitto*     m_mosq;
	std::atomic<bool> m_connected;   // Set true in onConnect, false in onDisconnect.

	// Mosquitto event callbacks (called on the internal mosquitto thread).
	static void onConnect(mosquitto* mosq, void* obj, int rc);
	static void onSubscribe(mosquitto* mosq, void* obj, int mid, int qosCount, const int* grantedQOS);
	static void onMessage(mosquitto* mosq, void* obj, const mosquitto_message* message);
	static void onDisconnect(mosquitto* mosq, void* obj, int rc);
};

#endif

#endif
