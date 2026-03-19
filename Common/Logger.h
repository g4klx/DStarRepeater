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

/*
 * Singleton, thread-safe file logger with daily log rotation.
 *
 * One CLogger instance is created at startup and registered via setInstance().
 * All threads call the static getInstance() accessor or, more commonly, use the
 * wxLogXxx macros below.  The macros are compatibility shims that preserve the
 * original wxWidgets wxLogMessage/wxLogError call sites without pulling in the
 * wxWidgets library.
 *
 * Log levels follow the MMDVMHost convention so that config file values map
 * directly to enum values without translation:
 *
 *   Config value 0 — no logging (disabled)
 *   Config value 1 — LOG_DEBUG   (most verbose)
 *   Config value 2 — LOG_MESSAGE
 *   Config value 3 — LOG_INFO
 *   Config value 4 — LOG_WARNING
 *   Config value 5 — LOG_ERROR
 *   Config value 6 — LOG_FATAL   (least verbose)
 *
 * Both fileLevel and displayLevel use these values: a message is written to
 * the respective sink only when its level >= the configured threshold.  Setting
 * a threshold to 0 disables that sink entirely; 6 passes only LOG_FATAL.
 *
 * When built with MQTT=1, log lines are also forwarded to the MQTT broker on
 * the "{name}/log" topic via the global g_mqtt connection, filtered by the
 * minimum level stored in g_mqttLevel.
 */

#ifndef	Logger_H
#define	Logger_H

#include <atomic>
#include <string>
#include <cstdio>
#include <mutex>

#if defined(MQTT)
class CMQTTConnection;
extern CMQTTConnection* g_mqtt;         // Global MQTT connection (set in main())
extern unsigned int g_mqttLevel;        // Minimum LogLevel to forward over MQTT
#endif

// Ordered to match MMDVMHost: enum value == config file level.
// Level 0 ("no logging") has no corresponding enum constant.
enum LogLevel {
	LOG_DEBUG   = 1,
	LOG_MESSAGE = 2,
	LOG_INFO    = 3,
	LOG_WARNING = 4,
	LOG_ERROR   = 5,
	LOG_FATAL   = 6
};

class CLogger {
public:
	// directory  — where daily log files are written; may be empty when fileLevel == 0.
	// name       — base filename stem (e.g. "dstarrepeaterd").
	// fileLevel  — minimum LogLevel to write to the log file (0 = disabled).
	// displayLevel — minimum LogLevel to write to stdout (0 = disabled).
	CLogger(const std::string& directory, const std::string& name,
	        unsigned int fileLevel = 2U, unsigned int displayLevel = 2U);
	~CLogger();

	void log(LogLevel level, const char* fmt, ...) __attribute__((format(printf, 3, 4)));

	static CLogger* getInstance();
	static void setInstance(CLogger* logger);

private:
	std::string  m_name;          // Base filename stem (e.g. "dstarrepeater")
	std::string  m_directory;     // Directory where daily log files are written
	FILE*        m_file;          // Currently open log file handle (nullptr when disabled)
	int          m_day;           // Day-of-year when m_file was opened; triggers rotation
	std::mutex   m_mutex;         // Serialises concurrent log() calls from multiple threads
	unsigned int m_fileLevel;     // Minimum level to write to file (0 = disabled)
	unsigned int m_displayLevel;  // Minimum level to write to stdout (0 = disabled)

	static std::atomic<CLogger*> s_instance;  // The singleton logger

	void openFile(const struct tm* tm);
	void writeLog(const char* msg, time_t timestamp);
};

// Compatibility macros matching the wxWidgets wxLogXxx API.  Existing call
// sites throughout the codebase use these names and can remain unchanged.
#define wxLogFatalError(fmt, ...) do { if (CLogger::getInstance()) CLogger::getInstance()->log(LOG_FATAL, fmt, ##__VA_ARGS__); } while(0)
#define wxLogError(fmt, ...)     do { if (CLogger::getInstance()) CLogger::getInstance()->log(LOG_ERROR, fmt, ##__VA_ARGS__); } while(0)
#define wxLogWarning(fmt, ...)   do { if (CLogger::getInstance()) CLogger::getInstance()->log(LOG_WARNING, fmt, ##__VA_ARGS__); } while(0)
#define wxLogMessage(fmt, ...)   do { if (CLogger::getInstance()) CLogger::getInstance()->log(LOG_MESSAGE, fmt, ##__VA_ARGS__); } while(0)
#define wxLogInfo(fmt, ...)      do { if (CLogger::getInstance()) CLogger::getInstance()->log(LOG_INFO, fmt, ##__VA_ARGS__); } while(0)

#endif
