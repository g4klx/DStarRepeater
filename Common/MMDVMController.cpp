/*
 *   Copyright (C) 2011-2016,2018 by Jonathan Naylor G4KLX
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
#include "MMDVMController.h"
#include "DStarDefines.h"
#include "Logger.h"
#include "Timer.h"

#include <chrono>
#include <thread>
#include <cassert>
#include <cstring>
#if !defined(_WIN32)
#include <dirent.h>
#include <unistd.h>
#endif

const unsigned char MMDVM_FRAME_START  = 0xE0U;

const unsigned char MMDVM_GET_VERSION  = 0x00U;
const unsigned char MMDVM_GET_STATUS   = 0x01U;
const unsigned char MMDVM_SET_CONFIG   = 0x02U;

const unsigned char MMDVM_DSTAR_HEADER = 0x10U;
const unsigned char MMDVM_DSTAR_DATA   = 0x11U;
const unsigned char MMDVM_DSTAR_LOST   = 0x12U;
const unsigned char MMDVM_DSTAR_EOT    = 0x13U;

const unsigned char MMDVM_ACK          = 0x70U;
const unsigned char MMDVM_NAK          = 0x7FU;

const unsigned char MMDVM_DUMP         = 0xF0U;
const unsigned char MMDVM_DEBUG1       = 0xF1U;
const unsigned char MMDVM_DEBUG2       = 0xF2U;
const unsigned char MMDVM_DEBUG3       = 0xF3U;
const unsigned char MMDVM_DEBUG4       = 0xF4U;
const unsigned char MMDVM_DEBUG5       = 0xF5U;

const unsigned int MAX_RESPONSES = 30U;

const unsigned int BUFFER_LENGTH = 200U;

CMMDVMController::CMMDVMController(const std::string& port, const std::string& path, bool rxInvert, bool txInvert, bool pttInvert, unsigned int txDelay, unsigned int rxLevel, unsigned int txLevel) :
CModem(),
m_port(port),
m_path(path),
m_rxInvert(rxInvert),
m_txInvert(txInvert),
m_pttInvert(pttInvert),
m_txDelay(txDelay),
m_rxLevel(rxLevel),
m_txLevel(txLevel),
m_serial(port, SERIAL_115200, true),
m_buffer(nullptr),
m_txData(1000U),
m_rx(false)
{
	assert(!port.empty());

	m_buffer = new unsigned char[BUFFER_LENGTH];
}

CMMDVMController::~CMMDVMController()
{
	delete[] m_buffer;
}

bool CMMDVMController::start()
{
	findPort();

	bool ret = openModem();
	if (!ret)
		return false;

	findPath();

	m_thread = std::thread(&CMMDVMController::entry, this);

	return true;
}

void CMMDVMController::entry()
{
	wxLogMessage("Starting MMDVM Controller thread");

	// Clock every 5ms-ish
	CTimer pollTimer(200U, 0U, 100U);
	pollTimer.start();

	unsigned char  writeType   = DSMTT_NONE;
	unsigned char  writeLength = 0U;
	unsigned char* writeBuffer = new unsigned char[BUFFER_LENGTH];

	unsigned int space = 0U;

	while (!m_stopped) {
		// Poll the modem status every 100ms
		if (pollTimer.hasExpired()) {
			bool ret = readStatus();
			if (!ret) {
				ret = findModem();
				if (!ret) {
					wxLogError("Stopping MMDVM Controller thread");
					delete[] writeBuffer;
					return;
				}
			}

			pollTimer.start();
		}

		unsigned int length;
		RESP_TYPE_MMDVM type = getResponse(m_buffer, length);

		switch (type) {
			case RTDVM_TIMEOUT:
				break;

			case RTDVM_ERROR: {
					bool ret = findModem();
					if (!ret) {
						wxLogError("Stopping MMDVM Controller thread");
						delete[] writeBuffer;
						return;
					}
				}
				break;

			case RTDVM_DSTAR_HEADER: {
					// CUtils::dump("RT_DSTAR_HEADER", m_buffer, length);
					std::lock_guard<std::mutex> lock(m_mutex);

					unsigned char data[2U];
					data[0U] = DSMTT_HEADER;
					data[1U] = RADIO_HEADER_LENGTH_BYTES;
					m_rxData.addData(data, 2U);

					m_rxData.addData(m_buffer + 3U, RADIO_HEADER_LENGTH_BYTES);

					m_rx = true;
				}
				break;

			case RTDVM_DSTAR_DATA: {
					// CUtils::dump("RT_DSTAR_DATA", m_buffer, length);
					std::lock_guard<std::mutex> lock(m_mutex);

					unsigned char data[2U];
					data[0U] = DSMTT_DATA;
					data[1U] = DV_FRAME_LENGTH_BYTES;
					m_rxData.addData(data, 2U);

					m_rxData.addData(m_buffer + 3U, DV_FRAME_LENGTH_BYTES);

					m_rx = true;
				}
				break;

			case RTDVM_DSTAR_EOT: {
					// wxLogMessage("RT_DSTAR_EOT");
					std::lock_guard<std::mutex> lock(m_mutex);

					unsigned char data[2U];
					data[0U] = DSMTT_EOT;
					data[1U] = 0U;
					m_rxData.addData(data, 2U);

					m_rx = false;
				}
				break;

			case RTDVM_DSTAR_LOST: {
					// wxLogMessage("RT_DSTAR_LOST");
					std::lock_guard<std::mutex> lock(m_mutex);

					unsigned char data[2U];
					data[0U] = DSMTT_LOST;
					data[1U] = 0U;
					m_rxData.addData(data, 2U);

					m_rx = false;
				}
				break;

			case RTDVM_GET_STATUS: {
					bool dstar = (m_buffer[3U] & 0x01U) == 0x01U;
					if (!dstar) {
						wxLogError("D-Star not enabled in the MMDVM!!!");
						wxLogError("Stopping MMDVM Controller thread");
						delete[] writeBuffer;
						return;
					}

					m_tx  = (m_buffer[5U] & 0x01U) == 0x01U;

					bool adcOverflow = (m_buffer[5U] & 0x02U) == 0x02U;
					if (adcOverflow)
						wxLogWarning("MMDVM ADC levels have overflowed");

					space = m_buffer[6U];
					// CUtils::dump("GET_STATUS", m_buffer, length);
					// wxLogMessage("PTT=%d space=%u", int(m_tx), space);
				}
				break;

			// These should not be received in this loop, but don't complain if we do
			case RTDVM_GET_VERSION:
			case RTDVM_ACK:
				break;

			case RTDVM_NAK: {
					switch (m_buffer[3U]) {
						case MMDVM_DSTAR_HEADER:
							wxLogWarning("Received a header NAK from the MMDVM, reason = %u", m_buffer[4U]);
							break;
						case MMDVM_DSTAR_DATA:
							wxLogWarning("Received a data NAK from the MMDVM, reason = %u", m_buffer[4U]);
							break;
						case MMDVM_DSTAR_EOT:
							wxLogWarning("Received an EOT NAK from the MMDVM, reason = %u", m_buffer[4U]);
							break;
						default:
							wxLogWarning("Received a NAK from the MMDVM, command = 0x%02X, reason = %u", m_buffer[3U], m_buffer[4U]);
							break;
					}
				}
				break;

			case RTDVM_DUMP:
				CUtils::dump("Modem dump", m_buffer + 3U, length - 3U);
				break;

			case RTDVM_DEBUG1:
			case RTDVM_DEBUG2:
			case RTDVM_DEBUG3:
			case RTDVM_DEBUG4:
			case RTDVM_DEBUG5:
				printDebug();
				break;

			default:
				wxLogMessage("Unknown message, type: %02X", m_buffer[2U]);
				CUtils::dump("Buffer dump", m_buffer, length);
				break;
		}

		if (writeType == DSMTT_NONE && m_txData.hasData()) {
			std::lock_guard<std::mutex> lock(m_mutex);

			m_txData.getData(&writeType, 1U);
			m_txData.getData(&writeLength, 1U);
			m_txData.getData(writeBuffer, writeLength);
		}

		if (space > 4U && writeType == DSMTT_HEADER) {
			// CUtils::dump("Write Header", writeBuffer, writeLength);

			int ret = m_serial.write(writeBuffer, writeLength);
			if (ret != int(writeLength))
				wxLogWarning("Error when writing the header to the MMDVM");

			writeType = DSMTT_NONE;
			space -= 4U;
		}

		if (space > 1U && (writeType == DSMTT_DATA || writeType == DSMTT_EOT)) {
			// CUtils::dump("Write Data", writeBuffer, writeLength);

			int ret = m_serial.write(writeBuffer, writeLength);
			if (ret != int(writeLength))
				wxLogWarning("Error when writing data to the MMDVM");

			writeType = DSMTT_NONE;
			space--;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(5));

		pollTimer.clock();
	}

	wxLogMessage("Stopping MMDVM Controller thread");

	delete[] writeBuffer;

	m_serial.close();
}

bool CMMDVMController::writeHeader(const CHeaderData& header)
{
	bool ret = m_txData.hasSpace(46U);
	if (!ret) {
		wxLogWarning("No space to write the header");
		return false;
	}

	unsigned char buffer[50U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = RADIO_HEADER_LENGTH_BYTES + 3U;
	buffer[2U] = MMDVM_DSTAR_HEADER;

	::memset(buffer + 3U, ' ', RADIO_HEADER_LENGTH_BYTES);

	buffer[3U] = header.getFlag1();
	buffer[4U] = header.getFlag2();
	buffer[5U] = header.getFlag3();

	std::string rpt2 = header.getRptCall2();
	for (unsigned int i = 0U; i < rpt2.size() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer[i + 6U]  = rpt2[i];

	std::string rpt1 = header.getRptCall1();
	for (unsigned int i = 0U; i < rpt1.size() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer[i + 14U] = rpt1[i];

	std::string your = header.getYourCall();
	for (unsigned int i = 0U; i < your.size() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer[i + 22U] = your[i];

	std::string my1 = header.getMyCall1();
	for (unsigned int i = 0U; i < my1.size() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer[i + 30U] = my1[i];

	std::string my2 = header.getMyCall2();
	for (unsigned int i = 0U; i < my2.size() && i < SHORT_CALLSIGN_LENGTH; i++)
		buffer[i + 38U] = my2[i];

	CCCITTChecksumReverse cksum1;
	cksum1.update(buffer + 3U, RADIO_HEADER_LENGTH_BYTES - 2U);
	cksum1.result(buffer + 42U);

	std::lock_guard<std::mutex> lock(m_mutex);

	unsigned char type = DSMTT_HEADER;
	m_txData.addData(&type, 1U);

	unsigned char len = RADIO_HEADER_LENGTH_BYTES + 3U;
	m_txData.addData(&len, 1U);

	m_txData.addData(buffer, RADIO_HEADER_LENGTH_BYTES + 3U);

	return true;
}

bool CMMDVMController::writeData(const unsigned char* data, unsigned int length, bool end)
{
	bool ret = m_txData.hasSpace(17U);
	if (!ret) {
		wxLogWarning("No space to write data");
		return false;
	}

	unsigned char buffer[20U];

	if (end) {
		buffer[0U] = MMDVM_FRAME_START;
		buffer[1U] = 3U;
		buffer[2U] = MMDVM_DSTAR_EOT;

		std::lock_guard<std::mutex> lock(m_mutex);

		unsigned char type = DSMTT_EOT;
		m_txData.addData(&type, 1U);

		unsigned char len = 3U;
		m_txData.addData(&len, 1U);

		m_txData.addData(buffer, 3U);

		return true;
	}

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = DV_FRAME_LENGTH_BYTES + 3U;
	buffer[2U] = MMDVM_DSTAR_DATA;
	::memcpy(buffer + 3U, data, DV_FRAME_LENGTH_BYTES);

	std::lock_guard<std::mutex> lock(m_mutex);

	unsigned char type = DSMTT_DATA;
	m_txData.addData(&type, 1U);

	unsigned char len = DV_FRAME_LENGTH_BYTES + 3U;
	m_txData.addData(&len, 1U);

	m_txData.addData(buffer, DV_FRAME_LENGTH_BYTES + 3U);

	return true;
}

unsigned int CMMDVMController::getSpace()
{
	return m_txData.freeSpace() / (DV_FRAME_LENGTH_BYTES + 5U);
}

bool CMMDVMController::isTXReady()
{
	if (m_tx)
		return false;

	return m_txData.isEmpty();
}

// Waits 2s for the MMDVM firmware to finish booting, then retries GET_VERSION
// up to 6 times (with 1s between attempts) before giving up.  The 2s initial
// wait is required because the Arduino bootloader holds the serial port for
// approximately that period after a USB connection is established.
bool CMMDVMController::readVersion()
{
	std::this_thread::sleep_for(std::chrono::seconds(2));

	for (unsigned int i = 0U; i < 6U; i++) {
		unsigned char buffer[3U];

		buffer[0U] = MMDVM_FRAME_START;
		buffer[1U] = 3U;
		buffer[2U] = MMDVM_GET_VERSION;

		// CUtils::dump("Written", buffer, 3U);

		int ret = m_serial.write(buffer, 3U);
		if (ret != 3)
			return false;

		for (unsigned int count = 0U; count < MAX_RESPONSES; count++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			unsigned int length;
			RESP_TYPE_MMDVM resp = getResponse(m_buffer, length);
			if (resp == RTDVM_GET_VERSION) {
				std::string description((char*)(m_buffer + 4U), length - 4U);

				wxLogInfo("MMDVM protocol version: %u, description: %s", m_buffer[3U], description.c_str());

				return true;
			}
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	wxLogError("Unable to read the firmware version after six attempts");

	return false;
}

bool CMMDVMController::readStatus()
{
	unsigned char buffer[3U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = 3U;
	buffer[2U] = MMDVM_GET_STATUS;

	return m_serial.write(buffer, 3U) == 3;
}

// Sends SET_CONFIG (20-byte frame) to configure the MMDVM for D-Star only.
// Key fields:
//   byte[3] invert flags: bit0=rxInvert, bit1=txInvert, bit2=pttInvert
//   byte[4] mode mask:    0x01 = D-Star enabled, all other modes off
//   byte[5] txDelay:      value / 10ms units (e.g. txDelay=100 -> 10)
//   byte[6] initial state: 1 = STATE_DSTAR
//   byte[7] rxLevel, byte[8] txLevel: 0-100% scaled to 0-255
//   bytes[12-15,18] txLevel repeated for DMR/YSF/P25/NXDN (unused but required)
bool CMMDVMController::setConfig()
{
	unsigned char buffer[25U];

	buffer[0U] = MMDVM_FRAME_START;

	buffer[1U] = 20U;

	buffer[2U] = MMDVM_SET_CONFIG;

	buffer[3U] = 0x00U;
	if (m_rxInvert)
		buffer[3U] |= 0x01U;
	if (m_txInvert)
		buffer[3U] |= 0x02U;
	if (m_pttInvert)
		buffer[3U] |= 0x04U;

	buffer[4U] = 0x01U;				// D-Star only

	buffer[5U] = m_txDelay / 10U;	// In 10ms units

	buffer[6U] = 1U;				// STATE_DSTAR

	buffer[7U] = (m_rxLevel * 255U) / 100U;
	buffer[8U] = (m_txLevel * 255U) / 100U;

	buffer[9U] = 0U;				// DMR Color Code

	buffer[10U] = 0U;				// DMR Delay

	buffer[11U] = 0U;				// Osc. Offset

	buffer[12U] = (m_txLevel * 255U) / 100U;
	buffer[13U] = (m_txLevel * 255U) / 100U;
	buffer[14U] = (m_txLevel * 255U) / 100U;
	buffer[15U] = (m_txLevel * 255U) / 100U;

	buffer[16U] = 128U;
	buffer[17U] = 128U;

	buffer[18U] = (m_txLevel * 255U) / 100U;

	buffer[19U] = 0U;

	// CUtils::dump("Written", buffer, 20U);

	int ret = m_serial.write(buffer, 20U);
	if (ret != 20U)
		return false;

	unsigned int count = 0U;
	unsigned int length;
	RESP_TYPE_MMDVM resp;
	do {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		resp = getResponse(m_buffer, length);

		if (resp != RTDVM_ACK && resp != RTDVM_NAK) {
			count++;
			if (count >= MAX_RESPONSES) {
				wxLogError("The MMDVM is not responding to the SET_CONFIG command");
				return false;
			}
		}
	} while (resp != RTDVM_ACK && resp != RTDVM_NAK);

	// CUtils::dump("Response", m_buffer, length);

	if (resp == RTDVM_NAK) {
		wxLogError("Received a NAK to the SET_CONFIG command from the modem");
		return false;
	}

	return true;
}

RESP_TYPE_MMDVM CMMDVMController::getResponse(unsigned char *buffer, unsigned int& length)
{
	// Get the start of the frame or nothing at all
	int ret = m_serial.read(buffer + 0U, 1U);
	if (ret < 0) {
		wxLogError("Error when reading from the MMDVM");
		return RTDVM_ERROR;
	}

	if (ret == 0)
		return RTDVM_TIMEOUT;

	if (buffer[0U] != MMDVM_FRAME_START)
		return RTDVM_TIMEOUT;

	ret = m_serial.read(buffer + 1U, 1U);
	if (ret < 0) {
		wxLogError("Error when reading from the MMDVM");
		return RTDVM_ERROR;
	}

	if (ret == 0)
		return RTDVM_TIMEOUT;

	length = buffer[1U];

	if (length >= BUFFER_LENGTH) {
		wxLogError("Invalid data received from the MMDVM");
		CUtils::dump("Data", buffer, 2U);
		return RTDVM_TIMEOUT;
	}

	unsigned int offset = 2U;

	while (offset < length) {
		int ret = m_serial.read(buffer + offset, length - offset);
		if (ret < 0) {
			wxLogError("Error when reading from the MMDVM");
			return RTDVM_ERROR;
		}

		if (ret > 0)
			offset += ret;

		if (ret == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			if (m_stopped)
				return RTDVM_TIMEOUT;
		}
	}

	// CUtils::dump("Received", buffer, length);

	switch (buffer[2U]) {
		case MMDVM_GET_STATUS:
			return RTDVM_GET_STATUS;
		case MMDVM_GET_VERSION:
			return RTDVM_GET_VERSION;
		case MMDVM_DSTAR_HEADER:
			return RTDVM_DSTAR_HEADER;
		case MMDVM_DSTAR_DATA:
			return RTDVM_DSTAR_DATA;
		case MMDVM_DSTAR_EOT:
			return RTDVM_DSTAR_EOT;
		case MMDVM_DSTAR_LOST:
			return RTDVM_DSTAR_LOST;
		case MMDVM_ACK:
			return RTDVM_ACK;
		case MMDVM_NAK:
			return RTDVM_NAK;
		case MMDVM_DUMP:
			return RTDVM_DUMP;
		case MMDVM_DEBUG1:
			return RTDVM_DEBUG1;
		case MMDVM_DEBUG2:
			return RTDVM_DEBUG2;
		case MMDVM_DEBUG3:
			return RTDVM_DEBUG3;
		case MMDVM_DEBUG4:
			return RTDVM_DEBUG4;
		case MMDVM_DEBUG5:
			return RTDVM_DEBUG5;
		default:
			return RTDVM_UNKNOWN;
	}
}

std::string CMMDVMController::getPath() const
{
	return m_path;
}

bool CMMDVMController::findPort()
{
#if !defined(_WIN32)
	if (m_path.empty())
		return false;

	DIR* dir = ::opendir("/sys/class/tty");
	if (dir == nullptr) {
		wxLogError("Cannot open directory /sys/class/tty");
		return false;
	}

	struct dirent* entry;
	while ((entry = ::readdir(dir)) != nullptr) {
		std::string fileName(entry->d_name);

		// Match ttyACM* entries
		if (fileName.substr(0, 6) != "ttyACM")
			continue;

		std::string path = "/sys/class/tty/" + fileName;

		char cpath[255U];
		::strncpy(cpath, path.c_str(), sizeof(cpath) - 1);
		cpath[sizeof(cpath) - 1] = '\0';

		char symlink[255U];
		int ret2 = ::readlink(cpath, symlink, sizeof(symlink) - 1);
		if (ret2 < 0) {
			::strncat(cpath, "/device", sizeof(cpath) - ::strlen(cpath) - 1);
			ret2 = ::readlink(cpath, symlink, sizeof(symlink) - 1);
			if (ret2 < 0) {
				wxLogError("Error from readlink()");
				::closedir(dir);
				return false;
			}
			symlink[ret2] = '\0';
			path = std::string(symlink, ret2);
		} else {
			symlink[ret2] = '\0';
			std::string fullPath(symlink, ret2);
			size_t pos = fullPath.rfind('/');
			path = (pos != std::string::npos) ? fullPath.substr(0, pos) : fullPath;
		}

		if (path == m_path) {
			m_port = "/dev/" + fileName;

			wxLogMessage("Found modem port of %s based on the path", m_port.c_str());

			::closedir(dir);
			return true;
		}
	}

	::closedir(dir);
	return false;
#else
	return true;
#endif
}

bool CMMDVMController::findPath()
{
#if !defined(_WIN32)
	std::string path = "/sys/class/tty/" + m_port.substr(5U);

	char cpath[255U];
	::strncpy(cpath, path.c_str(), sizeof(cpath) - 1);
	cpath[sizeof(cpath) - 1] = '\0';

	char symlink[255U];
	int ret = ::readlink(cpath, symlink, sizeof(symlink) - 1);
	if (ret < 0) {
		::strncat(cpath, "/device", sizeof(cpath) - ::strlen(cpath) - 1);
		ret = ::readlink(cpath, symlink, sizeof(symlink) - 1);
		if (ret < 0) {
			wxLogError("Error from readlink()");
			return false;
		}
		symlink[ret] = '\0';
		path = std::string(symlink, ret);
	} else {
		symlink[ret] = '\0';
		std::string fullPath(symlink, ret);
		size_t pos = fullPath.rfind('/');
		path = (pos != std::string::npos) ? fullPath.substr(0, pos) : fullPath;
	}

	if (m_path.empty())
		wxLogMessage("Found modem path of %s", path.c_str());

	m_path = path;

	return true;
#else
	return true;
#endif
}

bool CMMDVMController::findModem()
{
	m_serial.close();

	// Tell the repeater that the signal has gone away
	if (m_rx) {
		std::lock_guard<std::mutex> lock(m_mutex);

		unsigned char data[2U];
		data[0U] = DSMTT_EOT;
		data[1U] = 0U;
		m_rxData.addData(data, 2U);

		m_rx = false;
	}

	unsigned int count = 0U;

	// Purge the transmit buffer every 500ms to avoid overflow, but only try and reopen the modem every 2s
	while (!m_stopped) {
		count++;
		if (count >= 4U) {
			wxLogMessage("Trying to reopen the modem");

			bool ret = findPort();
			if (ret) {
				ret = openModem();
				if (ret)
					return true;
			}

			count = 0U;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	return false;
}

bool CMMDVMController::openModem()
{
	bool ret = m_serial.open();
	if (!ret)
		return false;

	ret = readVersion();
	if (!ret) {
		m_serial.close();
		return false;
	}

	ret = setConfig();
	if (!ret) {
		m_serial.close();
		return false;
	}

	return true;
}

void CMMDVMController::printDebug()
{
	unsigned int length = m_buffer[1U];
	if (m_buffer[2U] == 0xF1U && length >= 4U) {
		std::string message((char*)(m_buffer + 3U), length - 3U);
		wxLogMessage("Debug: %s", message.c_str());
	} else if (m_buffer[2U] == 0xF2U && length >= 5U) {
		std::string message((char*)(m_buffer + 3U), length - 5U);
		short val1 = (m_buffer[length - 2U] << 8) | m_buffer[length - 1U];
		wxLogMessage("Debug: %s %d", message.c_str(), val1);
	} else if (m_buffer[2U] == 0xF3U && length >= 7U) {
		std::string message((char*)(m_buffer + 3U), length - 7U);
		short val1 = (m_buffer[length - 4U] << 8) | m_buffer[length - 3U];
		short val2 = (m_buffer[length - 2U] << 8) | m_buffer[length - 1U];
		wxLogMessage("Debug: %s %d %d", message.c_str(), val1, val2);
	} else if (m_buffer[2U] == 0xF4U && length >= 9U) {
		std::string message((char*)(m_buffer + 3U), length - 9U);
		short val1 = (m_buffer[length - 6U] << 8) | m_buffer[length - 5U];
		short val2 = (m_buffer[length - 4U] << 8) | m_buffer[length - 3U];
		short val3 = (m_buffer[length - 2U] << 8) | m_buffer[length - 1U];
		wxLogMessage("Debug: %s %d %d %d", message.c_str(), val1, val2, val3);
	} else if (m_buffer[2U] == 0xF5U && length >= 11U) {
		std::string message((char*)(m_buffer + 3U), length - 11U);
		short val1 = (m_buffer[length - 8U] << 8) | m_buffer[length - 7U];
		short val2 = (m_buffer[length - 6U] << 8) | m_buffer[length - 5U];
		short val3 = (m_buffer[length - 4U] << 8) | m_buffer[length - 3U];
		short val4 = (m_buffer[length - 2U] << 8) | m_buffer[length - 1U];
		wxLogMessage("Debug: %s %d %d %d %d", message.c_str(), val1, val2, val3, val4);
	}
}
