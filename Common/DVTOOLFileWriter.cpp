/*
 *   Copyright (C) 2009,2011,2013 by Jonathan Naylor G4KLX
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

#include "CCITTChecksumReverse.h"
#include "DVTOOLFileWriter.h"
#include "DStarDefines.h"

#include <cstdio>
#include <cstring>
#include <cassert>
#include <ctime>
#include "EndianCompat.h"

static const char        DVTOOL_SIGNATURE[] = "DVTOOL";
static unsigned int DVTOOL_SIGNATURE_LENGTH = 6U;

static const char        DSVT_SIGNATURE[] = "DSVT";
static unsigned int DSVT_SIGNATURE_LENGTH = 4U;

static const unsigned char HEADER_FLAG   = 0x10;
static const unsigned char DATA_FLAG     = 0x20;

static const unsigned char FIXED_DATA[]  = {0x00, 0x81, 0x00, 0x20, 0x00, 0x01, 0x02, 0xC0, 0xDE};
static unsigned int    FIXED_DATA_LENGTH = 9U;

static const unsigned char TRAILER_DATA[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned int   TRAILER_DATA_LENGTH = 12U;

static const unsigned char HEADER_MASK   = 0x80;
static const unsigned char TRAILER_MASK  = 0x40;

std::string CDVTOOLFileWriter::m_dirName = std::string();

CDVTOOLFileWriter::CDVTOOLFileWriter() :
m_fileName(),
m_file(nullptr),
m_count(0U),
m_sequence(0U),
m_offset(0)
{
}

CDVTOOLFileWriter::~CDVTOOLFileWriter()
{
}

void CDVTOOLFileWriter::setDirectory(const std::string& dirName)
{
	m_dirName = dirName;
}

std::string CDVTOOLFileWriter::getFileName() const
{
	return m_fileName;
}

bool CDVTOOLFileWriter::open(const std::string& filename, const CHeaderData& header)
{
	if (m_file != nullptr)
		close();

	std::string name = filename;
	// Replace spaces with underscores
	size_t pos = 0;
	while ((pos = name.find(' ', pos)) != std::string::npos) {
		name.replace(pos, 1, "_");
		pos += 1;
	}

	m_fileName = m_dirName + "/" + name + ".dvtool";

	m_file = ::fopen(m_fileName.c_str(), "wb");
	if (m_file == nullptr)
		return false;

	size_t n = ::fwrite(DVTOOL_SIGNATURE, 1U, DVTOOL_SIGNATURE_LENGTH, m_file);
	if (n != DVTOOL_SIGNATURE_LENGTH) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	m_offset = ::ftell(m_file);

	uint32_t dummy = 0U;
	n = ::fwrite(&dummy, 1U, sizeof(uint32_t), m_file);
	if (n != sizeof(uint32_t)) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	m_sequence = 0U;
	m_count = 0U;

	bool res = writeHeader(header);
	if (!res) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	return true;
}

bool CDVTOOLFileWriter::open(const CHeaderData& header)
{
	if (m_file != nullptr)
		close();

	// Format timestamp
	time_t now = ::time(nullptr);
#if defined(_WIN32)
	struct tm tm_buf;
	localtime_s(&tm_buf, &now);
	struct tm* tm_info = &tm_buf;
#else
	struct tm tm_buf;
	struct tm* tm_info = localtime_r(&now, &tm_buf);
#endif
	char timeBuf[32];
	::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d-%H%M%S-", tm_info);

	std::string name = std::string(timeBuf);
	name += header.getRptCall1();
	name += header.getRptCall2();
	name += header.getYourCall();
	name += header.getMyCall1();
	name += header.getMyCall2();

	// Replace spaces with underscores
	size_t pos = 0;
	while ((pos = name.find(' ', pos)) != std::string::npos) {
		name.replace(pos, 1, "_");
		pos += 1;
	}

	// Sanitize the filename to prevent path traversal from crafted callsigns
	for (size_t i = 0; i < name.size(); i++) {
		if (name[i] == '/' || name[i] == '\\')
			name[i] = '-';
	}
	// Remove any ".." sequences
	while ((pos = name.find("..")) != std::string::npos)
		name.replace(pos, 2, "__");

	m_fileName = m_dirName + "/" + name + ".dvtool";

	m_file = ::fopen(m_fileName.c_str(), "wb");
	if (m_file == nullptr)
		return false;

	size_t n = ::fwrite(DVTOOL_SIGNATURE, 1U, DVTOOL_SIGNATURE_LENGTH, m_file);
	if (n != DVTOOL_SIGNATURE_LENGTH) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	m_offset = ::ftell(m_file);

	uint32_t dummy = 0U;
	n = ::fwrite(&dummy, 1U, sizeof(uint32_t), m_file);
	if (n != sizeof(uint32_t)) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	m_sequence = 0U;
	m_count = 0U;

	bool res = writeHeader(header);
	if (!res) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	return true;
}

bool CDVTOOLFileWriter::write(const unsigned char* buffer, unsigned int length)
{
	assert(buffer != nullptr);
	assert(length > 0U);

	// wxUINT16_SWAP_ON_BE produces a little-endian value
	uint16_t len = htole16(length + 15U);
	size_t n = ::fwrite(&len, 1U, sizeof(uint16_t), m_file);
	if (n != sizeof(uint16_t)) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	n = ::fwrite(DSVT_SIGNATURE, 1U, DSVT_SIGNATURE_LENGTH, m_file);
	if (n != DSVT_SIGNATURE_LENGTH) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	char byte = DATA_FLAG;
	n = ::fwrite(&byte, 1U, 1U, m_file);
	if (n != 1U) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	n = ::fwrite(FIXED_DATA, 1U, FIXED_DATA_LENGTH, m_file);
	if (n != FIXED_DATA_LENGTH) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	byte = (char)m_sequence;
	n = ::fwrite(&byte, 1U, 1U, m_file);
	if (n != 1U) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	n = ::fwrite(buffer, 1U, length, m_file);
	if (n != length) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	m_count++;
	m_sequence++;
	if (m_sequence >= 0x15U)
		m_sequence = 0U;

	return true;
}

void CDVTOOLFileWriter::close()
{
	writeTrailer();

	::fseek(m_file, m_offset, SEEK_SET);

	// wxUINT32_SWAP_ON_LE produces a big-endian value
	uint32_t count = htobe32(m_count);
	::fwrite(&count, 1U, sizeof(uint32_t), m_file);

	::fclose(m_file);
	m_file = nullptr;
}

bool CDVTOOLFileWriter::writeHeader(const CHeaderData& header)
{
	unsigned char buffer[RADIO_HEADER_LENGTH_BYTES];

	buffer[0] = header.getFlag1();
	buffer[1] = header.getFlag2();
	buffer[2] = header.getFlag3();

	for (unsigned int i = 0U; i < LONG_CALLSIGN_LENGTH; i++)
		buffer[3 + i] = header.getRptCall1()[i];

	for (unsigned int i = 0U; i < LONG_CALLSIGN_LENGTH; i++)
		buffer[11 + i] = header.getRptCall2()[i];

	for (unsigned int i = 0U; i < LONG_CALLSIGN_LENGTH; i++)
		buffer[19 + i] = header.getYourCall()[i];

	for (unsigned int i = 0U; i < LONG_CALLSIGN_LENGTH; i++)
		buffer[27 + i] = header.getMyCall1()[i];

	for (unsigned int i = 0U; i < SHORT_CALLSIGN_LENGTH; i++)
		buffer[35 + i] = header.getMyCall2()[i];

	// Get the checksum for the header
	CCCITTChecksumReverse csum;
	csum.update(buffer, RADIO_HEADER_LENGTH_BYTES - 2U);
	csum.result(buffer + 39U);

	uint16_t len = htole16(RADIO_HEADER_LENGTH_BYTES + 15U);
	size_t n = ::fwrite(&len, 1U, sizeof(uint16_t), m_file);
	if (n != sizeof(uint16_t)) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	n = ::fwrite(DSVT_SIGNATURE, 1U, DSVT_SIGNATURE_LENGTH, m_file);
	if (n != DSVT_SIGNATURE_LENGTH) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	char byte = HEADER_FLAG;
	n = ::fwrite(&byte, 1U, 1U, m_file);
	if (n != 1U) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	n = ::fwrite(FIXED_DATA, 1U, FIXED_DATA_LENGTH, m_file);
	if (n != FIXED_DATA_LENGTH) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	byte = HEADER_MASK;
	n = ::fwrite(&byte, 1U, 1U, m_file);
	if (n != 1U) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	n = ::fwrite(buffer, 1U, RADIO_HEADER_LENGTH_BYTES, m_file);
	if (n != RADIO_HEADER_LENGTH_BYTES) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	m_count++;

	return true;
}

bool CDVTOOLFileWriter::writeTrailer()
{
	uint16_t len = htole16(27U);
	size_t n = ::fwrite(&len, 1U, sizeof(uint16_t), m_file);
	if (n != sizeof(uint16_t)) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	n = ::fwrite(DSVT_SIGNATURE, 1U, DSVT_SIGNATURE_LENGTH, m_file);
	if (n != DSVT_SIGNATURE_LENGTH) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	char byte = DATA_FLAG;
	n = ::fwrite(&byte, 1U, 1U, m_file);
	if (n != 1U) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	n = ::fwrite(FIXED_DATA, 1U, FIXED_DATA_LENGTH, m_file);
	if (n != FIXED_DATA_LENGTH) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	byte = (char)(TRAILER_MASK | m_sequence);
	n = ::fwrite(&byte, 1U, 1U, m_file);
	if (n != 1U) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	n = ::fwrite(TRAILER_DATA, 1U, TRAILER_DATA_LENGTH, m_file);
	if (n != TRAILER_DATA_LENGTH) {
		::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	return true;
}
