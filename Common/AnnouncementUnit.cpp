/*
 *   Copyright (C) 2013 by Jonathan Naylor G4KLX
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

#include "AnnouncementUnit.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#if defined(_WIN32)
#include <io.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

static const std::string GLOBAL_FILE_NAME = "Announce";

CAnnouncementUnit::CAnnouncementUnit(IAnnouncementCallback* handler, const std::string& callsign) :
m_handler(handler),
m_localFileName(),
m_reader(),
m_writer(),
m_time(),
m_out(0U),
m_sending(false)
{
	assert(handler != nullptr);

	m_localFileName = "Announce " + callsign;

	// Replace spaces with underscores in the filename
	size_t pos = 0;
	while ((pos = m_localFileName.find(' ', pos)) != std::string::npos) {
		m_localFileName.replace(pos, 1, "_");
		pos += 1;
	}

#if defined(_WIN32)
	const char* home = getenv("USERPROFILE");
#else
	const char* home = getenv("HOME");
#endif
	m_writer.setDirectory(std::string(home != nullptr ? home : ""));
}

CAnnouncementUnit::~CAnnouncementUnit()
{
}

bool CAnnouncementUnit::writeHeader(const CHeaderData& header)
{
	return m_writer.open(m_localFileName, header);
}

bool CAnnouncementUnit::writeData(const unsigned char* data, unsigned int length, bool end)
{
	bool ret = m_writer.write(data, length);
	if (!ret)
		return false;

	if (end)
		m_writer.close();

	return true;
}

void CAnnouncementUnit::deleteAnnouncement()
{
#if defined(_WIN32)
	const char* home = getenv("USERPROFILE");
#else
	const char* home = getenv("HOME");
#endif
	std::string filePath = std::string(home != nullptr ? home : "") + "/" + m_localFileName + ".dvtool";

	if (access(filePath.c_str(), F_OK) == 0)
		::remove(filePath.c_str());
}

void CAnnouncementUnit::startAnnouncement()
{
#if defined(_WIN32)
	const char* home = getenv("USERPROFILE");
#else
	const char* home = getenv("HOME");
#endif
	std::string filePath1 = std::string(home != nullptr ? home : "") + "/" + m_localFileName + ".dvtool";
	std::string filePath2 = std::string(home != nullptr ? home : "") + "/" + GLOBAL_FILE_NAME + ".dvtool";

	if (access(filePath1.c_str(), F_OK) == 0) {
		bool ret = m_reader.open(filePath1);
		if (!ret)
			return;
	} else if (access(filePath2.c_str(), F_OK) == 0) {
		bool ret = m_reader.open(filePath2);
		if (!ret)
			return;
	} else {
		return;
	}

	DVTFR_TYPE type = m_reader.read();
	if (type != DVTFR_HEADER) {
		m_reader.close();
		return;
	}

	CHeaderData* header = m_reader.readHeader();
	if (header == nullptr) {
		m_reader.close();
		return;
	}

	// Remove the repeater bit
	header->setFlag1(header->getFlag1() & ~REPEATER_MASK);

	m_handler->transmitAnnouncementHeader(header);

	m_time = std::chrono::steady_clock::now();

	m_out = 0U;
	m_sending = true;
}

void CAnnouncementUnit::clock()
{
	if (!m_sending)
		return;

	unsigned int needed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_time).count() / DSTAR_FRAME_TIME_MS;

	while (m_out < needed) {
		DVTFR_TYPE type = m_reader.read();
		if (type != DVTFR_DATA) {
			m_handler->transmitAnnouncementData(END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES, true);
			m_reader.close();
			m_sending = false;
			m_out = 0U;
			return;
		}

		unsigned char data[DV_FRAME_MAX_LENGTH_BYTES];
		bool end;
		m_reader.readData(data, DV_FRAME_MAX_LENGTH_BYTES, end);

		m_handler->transmitAnnouncementData(data, DV_FRAME_LENGTH_BYTES, false);
		m_out++;

		if (end) {
			m_handler->transmitAnnouncementData(END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES, true);
			m_reader.close();
			m_sending = false;
			m_out = 0U;
			return;
		}
	}
}
