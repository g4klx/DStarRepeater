/*
 *   Copyright (C) 2002,2003,2009,2011,2012,2019 by Jonathan Naylor G4KLX
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

#include "Logger.h"

#include <cstdarg>
#include <ctime>
#include <cstdlib>

static struct tm* gmtime_safe(const time_t* t, struct tm* result) {
#if defined(_WIN32)
    gmtime_s(result, t);
#else
    gmtime_r(t, result);
#endif
    return result;
}

#if defined(MQTT)
#include "MQTTConnection.h"
CMQTTConnection* g_mqtt = nullptr;
unsigned int g_mqttLevel = 2U;
#endif

std::atomic<CLogger*> CLogger::s_instance{nullptr};

CLogger::CLogger(const std::string& directory, const std::string& name,
                 unsigned int fileLevel, unsigned int displayLevel) :
m_name(name),
m_directory(directory),
m_file(nullptr),
m_day(0),
m_mutex(),
m_fileLevel(fileLevel),
m_displayLevel(displayLevel)
{
	if (m_fileLevel == 0U)
		return;  // File logging disabled; do not create a log file.

	time_t timestamp;
	::time(&timestamp);
	struct tm tmBuf;
	struct tm* tm = gmtime_safe(&timestamp, &tmBuf);

	m_day = tm->tm_yday;
	openFile(tm);
}

CLogger::~CLogger()
{
	if (m_file != nullptr) {
		::fclose(m_file);
		m_file = nullptr;
	}
}

CLogger* CLogger::getInstance()
{
	return s_instance;
}

void CLogger::setInstance(CLogger* logger)
{
	s_instance = logger;
}

void CLogger::openFile(const struct tm* tm)
{
	if (m_file != nullptr)
		::fclose(m_file);

	char filename[512];
	::snprintf(filename, sizeof(filename), "%s/%s-%04d-%02d-%02d.log",
		m_directory.c_str(), m_name.c_str(),
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

	m_file = ::fopen(filename, "a+t");
	if (m_file == nullptr)
		::fprintf(stderr, "Cannot open %s for appending\n", filename);
}

void CLogger::log(LogLevel level, const char* fmt, ...)
{
	// Fast-path: drop the message if neither sink would accept it.
	const unsigned int numLevel = static_cast<unsigned int>(level);
	const bool toFile    = (m_fileLevel    != 0U && numLevel >= m_fileLevel);
	const bool toDisplay = (m_displayLevel != 0U && numLevel >= m_displayLevel);
#if defined(MQTT)
	const bool toMQTT    = (g_mqtt != nullptr && g_mqttLevel != 0U && numLevel >= g_mqttLevel);
#else
	const bool toMQTT    = false;
#endif
	if (!toFile && !toDisplay && !toMQTT && level != LOG_FATAL)
		return;

	char buffer[1024];

	va_list ap;
	va_start(ap, fmt);
	::vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	time_t timestamp;
	::time(&timestamp);

	const char* letter;
	switch (level) {
		case LOG_FATAL:   letter = "F"; break;
		case LOG_ERROR:   letter = "E"; break;
		case LOG_WARNING: letter = "W"; break;
		case LOG_INFO:    letter = "I"; break;
		case LOG_MESSAGE: letter = "M"; break;
		case LOG_DEBUG:   letter = "D"; break;
		default:          letter = "U"; break;
	}

	struct tm tmBuf;
	struct tm* tm = gmtime_safe(&timestamp, &tmBuf);

	char message[1280];
	::snprintf(message, sizeof(message), "%s: %04d-%02d-%02d %02d:%02d:%02d: %s\n",
		letter,
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec,
		buffer);

	if (toFile || toDisplay) {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (toFile)
			writeLog(message, timestamp);
		if (toDisplay) {
			::fputs(message, stdout);
			::fflush(stdout);
		}
	}

	// MQTT publish happens outside the lock: CMQTTConnection has its own
	// internal locking and mosquitto_publish() must not be called while
	// holding m_mutex (risk of priority inversion / deadlock).
#if defined(MQTT)
	if (toMQTT)
		g_mqtt->publish("log", message);
#endif

	if (level == LOG_FATAL)
		::abort();
}

void CLogger::writeLog(const char* msg, time_t timestamp)
{
	struct tm tmBuf;
	struct tm* tm = gmtime_safe(&timestamp, &tmBuf);

	int day = tm->tm_yday;
	if (day != m_day) {
		m_day = day;
		openFile(tm);
	}

	if (m_file != nullptr) {
		::fputs(msg, m_file);
		::fflush(m_file);
	}
}
