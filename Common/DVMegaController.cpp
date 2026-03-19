/*
 *   Copyright (C) 2011-2015 by Jonathan Naylor G4KLX
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
#include "DVMegaController.h"
#include "CCITTChecksum.h"
#include "DStarDefines.h"
#include "Logger.h"
#include "Timer.h"
#include "Utils.h"

#include <chrono>
#include <thread>
#include <cassert>
#include <cstring>
#include <cstdio>
#include "EndianCompat.h"
#if !defined(_WIN32)
#include <dirent.h>
#include <unistd.h>
#endif

const unsigned char DVRPTR_HEADER_LENGTH = 5U;

const unsigned char DVRPTR_FRAME_START = 0xD0U;

const unsigned char DVRPTR_GET_STATUS  = 0x10U;
const unsigned char DVRPTR_GET_VERSION = 0x11U;
const unsigned char DVRPTR_GET_SERIAL  = 0x12U;
const unsigned char DVRPTR_GET_CONFIG  = 0x13U;
const unsigned char DVRPTR_SET_CONFIG  = 0x14U;
const unsigned char DVRPTR_RXPREAMBLE  = 0x15U;
const unsigned char DVRPTR_START       = 0x16U;
const unsigned char DVRPTR_HEADER      = 0x17U;
const unsigned char DVRPTR_RXSYNC      = 0x18U;
const unsigned char DVRPTR_DATA        = 0x19U;
const unsigned char DVRPTR_EOT         = 0x1AU;
const unsigned char DVRPTR_RXLOST      = 0x1BU;
const unsigned char DVRPTR_MSG_RSVD1   = 0x1CU;
const unsigned char DVRPTR_MSG_RSVD2   = 0x1DU;
const unsigned char DVRPTR_MSG_RSVD3   = 0x1EU;
const unsigned char DVRPTR_SET_TESTMDE = 0x1FU;

const unsigned char DVRPTR_ACK = 0x06U;
const unsigned char DVRPTR_NAK = 0x15U;

const unsigned int MAX_RESPONSES = 30U;

const unsigned int BUFFER_LENGTH = 200U;

CDVMegaController::CDVMegaController(const std::string& port, const std::string& path, bool rxInvert, bool txInvert, unsigned int txDelay) :
CModem(),
m_port(port),
m_path(path),
m_rxInvert(rxInvert),
m_txInvert(txInvert),
m_txDelay(txDelay),
m_rxFrequency(0U),
m_txFrequency(0U),
m_power(0U),
m_serial(port, SERIAL_115200, true),
m_buffer(nullptr),
m_txData(1000U),
m_txCounter(0U),
m_pktCounter(0U),
m_rx(false),
m_txSpace(0U),
m_txEnabled(false),
m_checksum(false)
{
	assert(!port.empty());

	m_buffer = new unsigned char[BUFFER_LENGTH];
}

CDVMegaController::CDVMegaController(const std::string& port, const std::string& path, unsigned int txDelay, unsigned int rxFrequency, unsigned int txFrequency, unsigned int power) :
CModem(),
m_port(port),
m_path(path),
m_rxInvert(false),
m_txInvert(false),
m_txDelay(txDelay),
m_rxFrequency(rxFrequency),
m_txFrequency(txFrequency),
m_power(power),
m_serial(port, SERIAL_115200, true),
m_buffer(nullptr),
m_txData(1000U),
m_txCounter(0U),
m_pktCounter(0U),
m_rx(false),
m_txSpace(0U),
m_txEnabled(false),
m_checksum(false)
{
	assert(!port.empty());
	assert((rxFrequency >= 144000000U && rxFrequency <= 148000000U) ||
		   (rxFrequency >= 420000000U && rxFrequency <= 450000000U));
	assert((txFrequency >= 144000000U && txFrequency <= 148000000U) ||
		   (txFrequency >= 420000000U && txFrequency <= 450000000U));

	m_buffer = new unsigned char[BUFFER_LENGTH];
}

CDVMegaController::~CDVMegaController()
{
	delete[] m_buffer;
}

bool CDVMegaController::start()
{
	findPort();

	bool ret = openModem();
	if (!ret)
		return false;

	findPath();

	m_thread = std::thread(&CDVMegaController::entry, this);

	return true;
}

void CDVMegaController::entry()
{
	wxLogMessage("Starting DVMEGA Controller thread");

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
					wxLogMessage("Stopping DVMEGA Controller thread");
					delete[] writeBuffer;
					return;
				}
			}

			pollTimer.start();
		}

		unsigned int length;
		RESP_TYPE_MEGA type = getResponse(m_buffer, length);

		switch (type) {
			case RTM_TIMEOUT:
				break;

			case RTM_ERROR: {
					bool ret = findModem();
					if (!ret) {
						wxLogMessage("Stopping DVMEGA Controller thread");
						delete[] writeBuffer;
						return;
					}
				}
				break;

			case RTM_RXPREAMBLE:
				// wxLogMessage("RT_PREAMBLE");
				break;

			case RTM_START:
				// wxLogMessage("RT_START");
				break;

			case RTM_HEADER:
				CUtils::dump("RT_HEADER", m_buffer, length);
				if (length == 7U) {
					if (m_buffer[4U] == DVRPTR_NAK)
						wxLogWarning("Received a header NAK from the DVMEGA");
				} else {
					bool correct = (m_buffer[5U] & 0x80U) == 0x00U;
					if (correct) {
						std::lock_guard<std::mutex> lock(m_mutex);

						unsigned char data[2U];
						data[0U] = DSMTT_HEADER;
						data[1U] = RADIO_HEADER_LENGTH_BYTES;
						m_rxData.addData(data, 2U);

						m_rxData.addData(m_buffer + 8U, RADIO_HEADER_LENGTH_BYTES);

						m_rx = true;
					}
				}
				break;

			case RTM_RXSYNC:
				// wxLogMessage("RT_RXSYNC");
				break;

			case RTM_DATA:
				CUtils::dump("RT_DATA", m_buffer, length);
				if (length == 7U) {
					if (m_buffer[4U] == DVRPTR_NAK)
						wxLogWarning("Received a data NAK from the DVMEGA");
				} else {
					std::lock_guard<std::mutex> lock(m_mutex);

					unsigned char data[2U];
					data[0U] = DSMTT_DATA;
					data[1U] = DV_FRAME_LENGTH_BYTES;
					m_rxData.addData(data, 2U);

					m_rxData.addData(m_buffer + 8U, DV_FRAME_LENGTH_BYTES);

					m_rx = true;
				}
				break;

			case RTM_EOT: {
					// wxLogMessage("RT_EOT");
					std::lock_guard<std::mutex> lock(m_mutex);

					unsigned char data[2U];
					data[0U] = DSMTT_EOT;
					data[1U] = 0U;
					m_rxData.addData(data, 2U);

					m_rx = false;
				}
				break;

			case RTM_RXLOST: {
					// wxLogMessage("RT_LOST");
					std::lock_guard<std::mutex> lock(m_mutex);

					unsigned char data[2U];
					data[0U] = DSMTT_LOST;
					data[1U] = 0U;
					m_rxData.addData(data, 2U);

					m_rx = false;
				}
				break;

			case RTM_GET_STATUS: {
					m_txEnabled = (m_buffer[4U] & 0x02U) == 0x02U;
					m_checksum  = (m_buffer[4U] & 0x08U) == 0x08U;
					m_tx        = (m_buffer[5U] & 0x02U) == 0x02U;
					m_txSpace   = m_buffer[8U];
					space       = m_txSpace - m_buffer[9U];
					// CUtils::dump("GET_STATUS", m_buffer, length);
					// wxLogMessage("PTT=%d tx=%u space=%u cksum=%d, tx enabled=%d", int(m_tx), m_txSpace, space, int(m_checksum), int(m_txEnabled));
				}
				break;

			// These should not be received in this loop, but don't complain if we do
			case RTM_GET_VERSION:
			case RTM_GET_SERIAL:
			case RTM_GET_CONFIG:
				break;

			default:
				wxLogMessage("Unknown message, type: %02X", m_buffer[3U]);
				CUtils::dump("Buffer dump", m_buffer, length);
				break;
		}

		if (space > 0U) {
			if (writeType == DSMTT_NONE && m_txData.hasData()) {
				std::lock_guard<std::mutex> lock(m_mutex);

				m_txData.getData(&writeType, 1U);
				m_txData.getData(&writeLength, 1U);
				m_txData.getData(writeBuffer, writeLength);
			}

			// Only send the start when the TX is off
			if (!m_tx && writeType == DSMTT_START) {
				// CUtils::dump("Write Header", writeBuffer, writeLength);

				int ret = m_serial.write(writeBuffer, writeLength);
				if (ret != int(writeLength))
					wxLogWarning("Error when writing the header to the DVMEGA");

				writeType = DSMTT_NONE;
				space--;
			}

			if (space > 4U && writeType == DSMTT_HEADER) {
				// CUtils::dump("Write Header", writeBuffer, writeLength);

				int ret = m_serial.write(writeBuffer, writeLength);
				if (ret != int(writeLength))
					wxLogWarning("Error when writing the header to the DVMEGA");

				writeType = DSMTT_NONE;
				space -= 4U;
			}

			if (writeType == DSMTT_DATA || writeType == DSMTT_EOT) {
				// CUtils::dump("Write Data", writeBuffer, writeLength);

				int ret = m_serial.write(writeBuffer, writeLength);
				if (ret != int(writeLength))
					wxLogWarning("Error when writing data to the DVMEGA");

				writeType = DSMTT_NONE;
				space--;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(5));

		pollTimer.clock();
	}

	wxLogMessage("Stopping DVMEGA Controller thread");

	setEnabled(false);

	delete[] writeBuffer;

	m_serial.close();
}

bool CDVMegaController::writeHeader(const CHeaderData& header)
{
	if (!m_txEnabled)
		return false;

	bool ret = m_txData.hasSpace(64U);
	if (!ret) {
		wxLogWarning("No space to write the header");
		return false;
	}

	m_txCounter++;
	if (m_txCounter == 0U)
		m_txCounter = 1U;

	unsigned char buffer1[10U];

	buffer1[0U] = DVRPTR_FRAME_START;

	buffer1[1U] = 0x03U;
	buffer1[2U] = 0x00U;

	buffer1[3U] = DVRPTR_START;

	buffer1[4U] = m_txCounter;
	buffer1[5U] = 0x00U;

	if (m_checksum) {
		CCCITTChecksum cksum;
		cksum.update(buffer1 + 0U, 6U);
		cksum.result(buffer1 + 6U);
	} else {
		buffer1[6U] = 0x00U;
		buffer1[7U] = 0x0BU;
	}

	unsigned char buffer2[60U];

	buffer2[0U] = DVRPTR_FRAME_START;

	buffer2[1U] = 0x2FU;
	buffer2[2U] = 0x00U;

	buffer2[3U] = DVRPTR_HEADER;

	buffer2[4U] = m_txCounter;
	buffer2[5U] = 0x00U;

	buffer2[6U] = 0x00U;
	buffer2[7U] = 0x00U;

	::memset(buffer2 + 8U, ' ', RADIO_HEADER_LENGTH_BYTES);

	buffer2[8U]  = header.getFlag1();
	buffer2[9U]  = header.getFlag2();
	buffer2[10U] = header.getFlag3();

	std::string rpt2 = header.getRptCall2();
	for (unsigned int i = 0U; i < rpt2.size() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer2[i + 11U]  = rpt2[i];

	std::string rpt1 = header.getRptCall1();
	for (unsigned int i = 0U; i < rpt1.size() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer2[i + 19U] = rpt1[i];

	std::string your = header.getYourCall();
	for (unsigned int i = 0U; i < your.size() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer2[i + 27U] = your[i];

	std::string my1 = header.getMyCall1();
	for (unsigned int i = 0U; i < my1.size() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer2[i + 35U] = my1[i];

	std::string my2 = header.getMyCall2();
	for (unsigned int i = 0U; i < my2.size() && i < SHORT_CALLSIGN_LENGTH; i++)
		buffer2[i + 43U] = my2[i];

	CCCITTChecksumReverse cksum1;
	cksum1.update(buffer2 + 8U, RADIO_HEADER_LENGTH_BYTES - 2U);
	cksum1.result(buffer2 + 47U);

	buffer2[49U] = 0x00U;

	if (m_checksum) {
		CCCITTChecksum cksum;
		cksum.update(buffer2 + 0U, 50U);
		cksum.result(buffer2 + 50U);
	} else {
		buffer2[50U] = 0x00U;
		buffer2[51U] = 0x0BU;
	}

	m_pktCounter = 0U;

	std::lock_guard<std::mutex> lock(m_mutex);

	unsigned char type1 = DSMTT_START;
	m_txData.addData(&type1, 1U);

	unsigned char len1 = 8U;
	m_txData.addData(&len1, 1U);

	m_txData.addData(buffer1, 8U);

	unsigned char type2 = DSMTT_HEADER;
	m_txData.addData(&type2, 1U);

	unsigned char len2 = 52U;
	m_txData.addData(&len2, 1U);

	m_txData.addData(buffer2, 52U);

	return true;
}

bool CDVMegaController::writeData(const unsigned char* data, unsigned int, bool end)
{
	if (!m_txEnabled)
		return false;

	bool ret = m_txData.hasSpace(26U);
	if (!ret) {
		wxLogWarning("No space to write data");
		return false;
	}

	unsigned char buffer[30U];

	if (end) {
		buffer[0U] = DVRPTR_FRAME_START;

		buffer[1U] = 0x03U;
		buffer[2U] = 0x00U;

		buffer[3U] = DVRPTR_EOT;

		buffer[4U] = m_txCounter;
		buffer[5U] = 0xFFU;

		if (m_checksum) {
			CCCITTChecksum cksum;
			cksum.update(buffer + 0U, 6U);
			cksum.result(buffer + 6U);
		} else {
			buffer[6U] = 0x00U;
			buffer[7U] = 0x0BU;
		}

		std::lock_guard<std::mutex> lock(m_mutex);

		unsigned char type = DSMTT_EOT;
		m_txData.addData(&type, 1U);

		unsigned char len = 8U;
		m_txData.addData(&len, 1U);

		m_txData.addData(buffer, 8U);

		return true;
	}

	buffer[0U] = DVRPTR_FRAME_START;

	buffer[1U] = 0x13U;
	buffer[2U] = 0x00U;

	buffer[3U] = DVRPTR_DATA;

	buffer[4U] = m_txCounter;
	buffer[5U] = m_pktCounter;

	m_pktCounter++;
	if (m_pktCounter >= m_txSpace)
		m_pktCounter = 0U;

	buffer[6U] = 0x00U;
	buffer[7U] = 0x00U;

	::memcpy(buffer + 8U, data, DV_FRAME_LENGTH_BYTES);

	buffer[20U] = 0x00U;
	buffer[21U] = 0x00U;

	if (m_checksum) {
		CCCITTChecksum cksum;
		cksum.update(buffer + 0U, 22U);
		cksum.result(buffer + 22U);
	} else {
		buffer[22U] = 0x00U;
		buffer[23U] = 0x0BU;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	unsigned char type = DSMTT_DATA;
	m_txData.addData(&type, 1U);

	unsigned char len = 24U;
	m_txData.addData(&len, 1U);

	m_txData.addData(buffer, 24U);

	return true;
}

unsigned int CDVMegaController::getSpace()
{
	return m_txData.freeSpace() / 26U;
}

bool CDVMegaController::isTXReady()
{
	if (m_tx)
		return false;

	return m_txData.isEmpty();
}

bool CDVMegaController::readVersion()
{
	for (unsigned int i = 0U; i < 6U; i++) {
		unsigned char buffer[10U];

		buffer[0U] = DVRPTR_FRAME_START;

		buffer[1U] = 0x01U;
		buffer[2U] = 0x00U;

		buffer[3U] = DVRPTR_GET_VERSION;

		if (m_checksum) {
			CCCITTChecksum cksum;
			cksum.update(buffer + 0U, 4U);
			cksum.result(buffer + 4U);
		} else {
			buffer[4U] = 0x00U;
			buffer[5U] = 0x0BU;
		}

		// CUtils::dump("Written", buffer, 6U);

		int ret = m_serial.write(buffer, 6U);
		if (ret != 6)
			return false;

		for (unsigned int count = 0U; count < MAX_RESPONSES; count++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			unsigned int length;
			RESP_TYPE_MEGA resp = getResponse(m_buffer, length);
			if (resp == RTM_GET_VERSION) {
				char firmware[32];
				if ((m_buffer[4U] & 0x0FU) > 0x00U)
					::snprintf(firmware, sizeof(firmware), "%u.%u%u%c",
						(m_buffer[5U] & 0xF0U) >> 4,
						m_buffer[5U] & 0x0FU,
						(m_buffer[4U] & 0xF0U) >> 4,
						(m_buffer[4U] & 0x0FU) + 'a' - 1U);
				else
					::snprintf(firmware, sizeof(firmware), "%u.%u%u",
						(m_buffer[5U] & 0xF0U) >> 4,
						m_buffer[5U] & 0x0FU,
						(m_buffer[4U] & 0xF0U) >> 4);

				std::string hardware((char*)(m_buffer + 6U), length - DVRPTR_HEADER_LENGTH - 3U);

				wxLogInfo("DVMEGA Firmware version: %s, hardware: %s", firmware, hardware.c_str());

				return true;
			}
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	wxLogError("Unable to read the firmware version after six attempts");

	return false;
}

bool CDVMegaController::readStatus()
{
	unsigned char buffer[10U];

	buffer[0U] = DVRPTR_FRAME_START;

	buffer[1U] = 0x01U;
	buffer[2U] = 0x00U;

	buffer[3U] = DVRPTR_GET_STATUS;

	if (m_checksum) {
		CCCITTChecksum cksum;
		cksum.update(buffer + 0U, 4U);
		cksum.result(buffer + 4U);
	} else {
		buffer[4U] = 0x00U;
		buffer[5U] = 0x0BU;
	}

	return m_serial.write(buffer, 6U) == 6;
}

bool CDVMegaController::setConfig()
{
	unsigned char buffer[20U];

	buffer[0U] = DVRPTR_FRAME_START;

	buffer[1U] = 0x07U;
	buffer[2U] = 0x00U;

	buffer[3U] = DVRPTR_SET_CONFIG;

	buffer[4U] = 0xC0U;		// Physical layer

	buffer[5U] = 0x04U;		// Block length

	buffer[6U] = 0x00U;
	if (m_rxInvert)
		buffer[6U] |= 0x01U;
	if (m_txInvert)
		buffer[6U] |= 0x02U;

	uint16_t txDelay = htole16((uint16_t)m_txDelay);
	::memcpy(buffer + 8U, &txDelay, sizeof(uint16_t));

	if (m_checksum) {
		CCCITTChecksum cksum;
		cksum.update(buffer + 0U, 10U);
		cksum.result(buffer + 10U);
	} else {
		buffer[10U] = 0x00U;
		buffer[11U] = 0x0BU;
	}

	// CUtils::dump("Written", buffer, 12U);

	int ret = m_serial.write(buffer, 12U);
	if (ret != 12)
		return false;

	unsigned int count = 0U;
	unsigned int length;
	RESP_TYPE_MEGA resp;
	do {

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		resp = getResponse(m_buffer, length);

		if (resp != RTM_SET_CONFIG) {
			count++;
			if (count >= MAX_RESPONSES) {
				wxLogError("The DVMEGA is not responding to the SET_CONFIG command");
				return false;
			}
		}
	} while (resp != RTM_SET_CONFIG);

	// CUtils::dump("Response", m_buffer, length);

	unsigned char type = m_buffer[4U];
	if (type != DVRPTR_ACK) {
		wxLogError("Received a NAK to the SET_CONFIG command from the modem");
		return false;
	}

	return true;
}

bool CDVMegaController::setFrequencyAndPower()
{
	unsigned char buffer[25U];

	::memset(buffer, 0x00U, 21U);

	buffer[0U] = DVRPTR_FRAME_START;

	buffer[1U] = 0x10U;
	buffer[2U] = 0x00U;

	buffer[3U] = DVRPTR_SET_CONFIG;

	buffer[4U] = 0xC1U;		// RF layer

	buffer[5U] = 0x0CU;		// Block length

	uint32_t rxFreq = htole32((uint32_t)m_rxFrequency);
	uint32_t txFreq = htole32((uint32_t)m_txFrequency);

	::memcpy(buffer + 7U,  &rxFreq, sizeof(uint32_t));
	::memcpy(buffer + 11U, &txFreq, sizeof(uint32_t));

	buffer[16U] = (m_power * 64U) / 100U;

	if (m_checksum) {
		CCCITTChecksum cksum;
		cksum.update(buffer + 0U, 19U);
		cksum.result(buffer + 19U);
	} else {
		buffer[19U] = 0x00U;
		buffer[20U] = 0x0BU;
	}

	// CUtils::dump("Written", buffer, 21U);

	int ret = m_serial.write(buffer, 21U);
	if (ret != 21)
		return false;

	unsigned int count = 0U;
	unsigned int length;
	RESP_TYPE_MEGA resp;
	do {

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		resp = getResponse(m_buffer, length);

		if (resp != RTM_SET_CONFIG) {
			count++;
			if (count >= MAX_RESPONSES) {
				wxLogError("The DVMEGA is not responding to the SET_CONFIG command");
				return false;
			}
		}
	} while (resp != RTM_SET_CONFIG);

	// CUtils::dump("Response", m_buffer, length);

	unsigned char type = m_buffer[4U];
	if (type != DVRPTR_ACK) {
		wxLogError("Received a NAK to the SET_CONFIG command from the modem");
		return false;
	}

	return true;
}

bool CDVMegaController::setEnabled(bool enable)
{
	unsigned char buffer[10U];

	buffer[0U] = DVRPTR_FRAME_START;

	buffer[1U] = 0x02U;
	buffer[2U] = 0x00U;

	buffer[3U] = DVRPTR_GET_STATUS;

	// Enable RX, TX, and Watchdog
	if (enable)
		buffer[4U] = 0x01U | 0x02U | 0x04U;
	else
		buffer[4U] = 0x00U;

	if (m_checksum) {
		CCCITTChecksum cksum;
		cksum.update(buffer + 0U, 5U);
		cksum.result(buffer + 5U);
	} else {
		buffer[5U] = 0x00U;
		buffer[6U] = 0x0BU;
	}

	// CUtils::dump("Written", buffer, 7U);

	int ret = m_serial.write(buffer, 7U);
	if (ret != 7)
		return false;

	unsigned int count = 0U;
	unsigned int length;
	RESP_TYPE_MEGA resp;
	do {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		resp = getResponse(m_buffer, length);

		if (resp != RTM_GET_STATUS) {
			count++;
			if (count >= MAX_RESPONSES) {
				wxLogError("The DVMEGA is not responding to the SET_STATUS command");
				return false;
			}
		}
	} while (resp != RTM_GET_STATUS);

	// CUtils::dump("Response", m_buffer, length);

	unsigned char type = m_buffer[4U];
	if (type != DVRPTR_ACK) {
		wxLogError("Received a NAK to the SET_STATUS command from the modem");
		return false;
	}

	return true;
}

// Reads one complete DVRPTR frame.
//   Frame header (5 bytes): [0xD0] [len_lo] [len_hi] [type] [...]
//   length = header bytes 1+2 (payload length, not including header itself).
//   type has bit 7 set for board responses; strip it before dispatching.
// Returns RTM_TIMEOUT if no start byte is available, RTM_ERROR on I/O failure.
RESP_TYPE_MEGA CDVMegaController::getResponse(unsigned char *buffer, unsigned int& length)
{
	// Get the start of the frame or nothing at all
	int ret = m_serial.read(buffer, 1U);
	if (ret < 0) {
		wxLogError("Error when reading from the DVMEGA");
		return RTM_ERROR;
	}

	if (ret == 0)
		return RTM_TIMEOUT;

	if (buffer[0U] != DVRPTR_FRAME_START)
		return RTM_TIMEOUT;

	unsigned int offset = 1U;

	while (offset < DVRPTR_HEADER_LENGTH) {
		ret = m_serial.read(buffer + offset, DVRPTR_HEADER_LENGTH - offset);
		if (ret < 0) {
			wxLogError("Error when reading from the DVMEGA");
			return RTM_ERROR;
		}

		if (ret > 0)
			offset += ret;

		if (ret == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			if (m_stopped)
				return RTM_TIMEOUT;
		}
	}

	length = buffer[1U] + buffer[2U] * 256U;

	if (length >= 100U) {
		wxLogError("Invalid data received from the DVMEGA");
		return RTM_ERROR;
	}

	// Remove the response bit
	unsigned int type = buffer[3U] & 0x7FU;

	offset = 0U;

	while (offset < length) {
		ret = m_serial.read(buffer + offset + DVRPTR_HEADER_LENGTH, length - offset);
		if (ret < 0) {
			wxLogError("Error when reading from the DVMEGA");
			return RTM_ERROR;
		}

		if (ret > 0)
			offset += ret;

		if (ret == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			if (m_stopped)
				return RTM_TIMEOUT;
		}
	}

	length += DVRPTR_HEADER_LENGTH;

	// CUtils::dump("Received", buffer, length);

	switch (type) {
		case DVRPTR_GET_STATUS:
			return RTM_GET_STATUS;
		case DVRPTR_GET_VERSION:
			return RTM_GET_VERSION;
		case DVRPTR_GET_SERIAL:
			return RTM_GET_SERIAL;
		case DVRPTR_GET_CONFIG:
			return RTM_GET_CONFIG;
		case DVRPTR_SET_CONFIG:
			return RTM_SET_CONFIG;
		case DVRPTR_RXPREAMBLE:
			return RTM_RXPREAMBLE;
		case DVRPTR_START:
			return RTM_START;
		case DVRPTR_HEADER:
			return RTM_HEADER;
		case DVRPTR_RXSYNC:
			return RTM_RXSYNC;
		case DVRPTR_DATA:
			return RTM_DATA;
		case DVRPTR_EOT:
			return RTM_EOT;
		case DVRPTR_RXLOST:
			return RTM_RXLOST;
		case DVRPTR_SET_TESTMDE:
			return RTM_SET_TESTMDE;
		default:
			return RTM_UNKNOWN;
	}
}

std::string CDVMegaController::getPath() const
{
	return m_path;
}

bool CDVMegaController::findPort()
{
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
			// Get all but the last section
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
}

bool CDVMegaController::findPath()
{
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
}

bool CDVMegaController::findModem()
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

bool CDVMegaController::openModem()
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

	if (m_rxFrequency != 0U && m_txFrequency != 0U) {
		ret = setFrequencyAndPower();
		if (!ret) {
			m_serial.close();
			return false;
		}
	}

	ret = setEnabled(true);
	if (!ret) {
		m_serial.close();
		return false;
	}

	return true;
}
