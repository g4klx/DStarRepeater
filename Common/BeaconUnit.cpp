/*
 *   Copyright (C) 2011,2012 by Jonathan Naylor G4KLX
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

#include "BeaconUnit.h"

#include <fstream>
#include <sstream>
#include <cstdio>
#include <cassert>
#include <cstring>
#if defined(_WIN32)
#include <io.h>
#define access _access
#define F_OK 0
#define R_OK 4
#else
#include <unistd.h>
#endif

// Maximum beacon duration: 60 seconds at the D-Star frame rate.
const unsigned int MAX_FRAMES = 60U * DSTAR_FRAMES_PER_SEC;

// Number of silent AMBE frames prepended to the AMBE buffer and mapped to " ".
// These create a short pause before and after each word/character.
const unsigned int SILENCE_LENGTH = 10U;

CBeaconUnit::CBeaconUnit(IBeaconCallback* handler, const std::string& callsign, const std::string& text, bool voice, TEXT_LANG language) :
m_ambe(nullptr),
m_ambeLength(0U),
m_data(nullptr),
m_dataLength(0U),
m_index(),
m_language(language),
m_handler(handler),
m_callsign(callsign),
m_encoder(),
m_in(0U),
m_out(0U),
m_seqNo(0U),
m_time(),
m_sending(false)
{
	assert(handler != nullptr);

	std::string slowData = text;
	slowData.resize(20U, ' ');
	m_encoder.setTextData(slowData);

	m_data = new unsigned char[MAX_FRAMES * DV_FRAME_LENGTH_BYTES];
	::memset(m_data, 0x00U, MAX_FRAMES * DV_FRAME_LENGTH_BYTES);

	if (!voice)
		return;

	std::string ambeFileName;
	std::string indxFileName;

	switch (m_language) {
		case TL_DEUTSCH:
			ambeFileName = "de_DE.ambe";
			indxFileName = "de_DE.indx";
			break;
		case TL_DANSK:
			ambeFileName = "dk_DK.ambe";
			indxFileName = "dk_DK.indx";
			break;
		case TL_ITALIANO:
			ambeFileName = "it_IT.ambe";
			indxFileName = "it_IT.indx";
			break;
		case TL_FRANCAIS:
			ambeFileName = "fr_FR.ambe";
			indxFileName = "fr_FR.indx";
			break;
		case TL_ESPANOL:
			ambeFileName = "es_ES.ambe";
			indxFileName = "es_ES.indx";
			break;
		case TL_SVENSKA:
			ambeFileName = "se_SE.ambe";
			indxFileName = "se_SE.indx";
			break;
		case TL_POLSKI:
			ambeFileName = "pl_PL.ambe";
			indxFileName = "pl_PL.indx";
			break;
		case TL_ENGLISH_US:
			ambeFileName = "en_US.ambe";
			indxFileName = "en_US.indx";
			break;
		case TL_NORSK:
			ambeFileName = "no_NO.ambe";
			indxFileName = "no_NO.indx";
			break;
//		case TL_NEDERLANDS_NL:
//			ambeFileName = "nl_NL.ambe";
//			indxFileName = "nl_NL.indx";
//			break;
//		case TL_NEDERLANDS_BE:
//			ambeFileName = "nl_BE.ambe";
//			indxFileName = "nl_BE.indx";
//			break;
		default:
			ambeFileName = "en_GB.ambe";
			indxFileName = "en_GB.indx";
			break;
	}

	bool ret = readAMBE(ambeFileName);
	if (!ret)
		return;

	readIndex(indxFileName);
}

CBeaconUnit::~CBeaconUnit()
{
	for (CIndexList_t::iterator it = m_index.begin(); it != m_index.end(); ++it)
		delete it->second;

	delete[] m_ambe;
	delete[] m_data;
}

void CBeaconUnit::sendBeacon()
{
	m_handler->transmitBeaconHeader();

	m_sending = true;

	m_time = std::chrono::steady_clock::now();

	m_in         = 0U;
	m_out        = 0U;
	m_seqNo      = 0U;
	m_dataLength = 0U;

	if (m_ambe == nullptr) {
		for (unsigned int i = 0U; i < 21U; i++) {
			unsigned char buffer[DV_FRAME_LENGTH_BYTES];

			if (i == 0U) {
				m_encoder.sync();

				::memcpy(buffer + 0U, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
				::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, DATA_SYNC_BYTES, DATA_FRAME_LENGTH_BYTES);
			} else {
				unsigned char text[DATA_FRAME_LENGTH_BYTES];
				m_encoder.getTextData(text);

				::memcpy(buffer + 0U, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);
				::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, text, DATA_FRAME_LENGTH_BYTES);
			}

			::memcpy(m_data + m_dataLength, buffer, DV_FRAME_LENGTH_BYTES);
			m_dataLength += DV_FRAME_LENGTH_BYTES;
			m_in++;
		}
	} else {
		lookup(" ");
		lookup(" ");
		lookup(" ");
		lookup(" ");

		spellCallsign(m_callsign);

		lookup(" ");
		lookup(" ");
		lookup(" ");
		lookup(" ");
	}
}

void CBeaconUnit::clock()
{
	if (!m_sending)
		return;

	// Calculate how many frames should have been sent by now based on elapsed
	// wall-clock time, then dispatch any that haven't been sent yet.  This
	// paces output at exactly the D-Star air rate without sleeping.
	unsigned int needed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_time).count() / DSTAR_FRAME_TIME_MS;

	while (m_out < needed) {
		m_handler->transmitBeaconData(m_data + m_out * DV_FRAME_LENGTH_BYTES, DV_FRAME_LENGTH_BYTES, false);
		m_out++;

		if (m_in == m_out) {
			m_handler->transmitBeaconData(END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES, true);
			m_sending = false;
			m_in  = 0U;
			m_out = 0U;
			return;
		}
	}
}

bool CBeaconUnit::lookup(const std::string& id)
{
	CIndexList_t::iterator it = m_index.find(id);
	if (it == m_index.end() || it->second == nullptr) {
		// Cannot find the AMBE index for this id
		return false;
	}

	CIndexRecord* info = it->second;

	unsigned int  start = info->getStart();
	unsigned int length = info->getLength();

	for (unsigned int i = 0U; i < length; i++) {
		unsigned char* dataIn = m_ambe + (start + i) * VOICE_FRAME_LENGTH_BYTES;

		unsigned char buffer[DV_FRAME_LENGTH_BYTES];
		::memcpy(buffer + 0U, dataIn, VOICE_FRAME_LENGTH_BYTES);

		// Insert sync bytes when the sequence number is zero, slow data otherwise
		if (m_seqNo == 0U) {
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, DATA_SYNC_BYTES, DATA_FRAME_LENGTH_BYTES);
			m_encoder.sync();
		} else {
			m_encoder.getTextData(buffer + VOICE_FRAME_LENGTH_BYTES);
		}

		::memcpy(m_data + m_dataLength, buffer, DV_FRAME_LENGTH_BYTES);
		m_dataLength += DV_FRAME_LENGTH_BYTES;
		m_in++;

		m_seqNo++;
		if (m_seqNo == 21U)
			m_seqNo = 0U;
	}

	return true;
}

void CBeaconUnit::spellCallsign(const std::string& callsign)
{
	unsigned int length = callsign.length();

	for (unsigned int i = 0U; i < (length - 1U); i++) {
		std::string c = callsign.substr(i, 1U);

		if (c != " ")
			lookup(c);
	}

	char c = callsign[length - 1U];

	switch (c) {
		case 'A':
			lookup("alpha");
			break;
		case 'B':
			lookup("bravo");
			break;
		case 'C':
			lookup("charlie");
			break;
		case 'D':
			lookup("delta");
			break;
		default:
			lookup(std::string(1, c));
			break;
	}
}

bool CBeaconUnit::readAMBE(const std::string& name)
{
#if defined(_WIN32)
	const char* home = getenv("USERPROFILE");
#else
	const char* home = getenv("HOME");
#endif
	std::string homePath = std::string(home != nullptr ? home : "") + "/" + name;
	std::string filePath;

	if (access(homePath.c_str(), R_OK) == 0) {
		filePath = homePath;
	} else {
		std::string dataPath = std::string(DATA_DIR) + "/" + name;
		if (access(dataPath.c_str(), R_OK) == 0) {
			filePath = dataPath;
		} else {
			return false;
		}
	}

	FILE* file = ::fopen(filePath.c_str(), "rb");
	if (file == nullptr)
		return false;

	unsigned char buffer[VOICE_FRAME_LENGTH_BYTES];

	size_t n = ::fread(buffer, 1U, 4U, file);
	if (n != 4U) {
		::fclose(file);
		return false;
	}

	if (::memcmp(buffer, "AMBE", 4U) != 0) {
		::fclose(file);
		return false;
	}

	// Determine file length minus the 4-byte header
	::fseek(file, 0L, SEEK_END);
	long fileSize = ::ftell(file);
	::fseek(file, 4L, SEEK_SET);
	unsigned int length = (unsigned int)(fileSize - 4L);

	// Hold the file data plus silence at the end
	m_ambe = new unsigned char[length + SILENCE_LENGTH * VOICE_FRAME_LENGTH_BYTES];
	m_ambeLength = length / VOICE_FRAME_LENGTH_BYTES;

	// Add silence to the beginning of the buffer
	unsigned char* p = m_ambe;
	for (unsigned int i = 0U; i < SILENCE_LENGTH; i++, p += VOICE_FRAME_LENGTH_BYTES)
		::memcpy(p, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);

	n = ::fread(p, 1U, length, file);
	if (n != length) {
		::fclose(file);
		delete[] m_ambe;
		m_ambe = nullptr;
		return false;
	}

	::fclose(file);

	return true;
}

bool CBeaconUnit::readIndex(const std::string& name)
{
#if defined(_WIN32)
	const char* home = getenv("USERPROFILE");
#else
	const char* home = getenv("HOME");
#endif
	std::string homePath = std::string(home != nullptr ? home : "") + "/" + name;
	std::string filePath;

	if (access(homePath.c_str(), R_OK) == 0) {
		filePath = homePath;
	} else {
		std::string dataPath = std::string(DATA_DIR) + "/" + name;
		if (access(dataPath.c_str(), R_OK) == 0) {
			filePath = dataPath;
		} else {
			return false;
		}
	}

	std::ifstream file(filePath);
	if (!file.is_open())
		return false;

	// Add a silence entry at the beginning
	m_index[" "] = new CIndexRecord(" ", 0U, SILENCE_LENGTH);

	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty() && line[0] != '#') {
			std::istringstream iss(line);
			std::string entryName, startTxt, lengthTxt;
			iss >> entryName >> startTxt >> lengthTxt;

			if (!entryName.empty() && !startTxt.empty() && !lengthTxt.empty()) {
				unsigned long start  = std::stoul(startTxt);
				unsigned long length = std::stoul(lengthTxt);

				if (start >= m_ambeLength || (start + length) >= m_ambeLength) {
					// Out of range entry — skip silently
				} else {
					m_index[entryName] = new CIndexRecord(entryName, start + SILENCE_LENGTH, length);
				}
			}
		}
	}

	file.close();

	return true;
}
