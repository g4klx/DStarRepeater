/*
 *   Copyright (C) 2011-2015,2018 by Jonathan Naylor G4KLX
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
#include "IcomController.h"
#include "CCITTChecksum.h"
#include "DStarDefines.h"
#include "Timer.h"
#include "Utils.h"

#if defined(__WINDOWS__)
#include <setupapi.h>
#else
#include <wx/dir.h>
#endif

const unsigned char ICOM_FRAME_START = 0xFFU;

const unsigned int MAX_RESPONSES = 30U;

const unsigned int BUFFER_LENGTH = 200U;

CIcomController::CIcomController(const wxString& port) :
CModem(),
m_port(port),
m_serial(port, SERIAL_38400, true),
m_buffer(NULL),
m_txData(1000U),
m_txCounter(0U),
m_pktCounter(0U),
m_rx(false),
m_txSpace(0U),
m_txEnabled(false),
m_checksum(false)
{
	wxASSERT(!port.IsEmpty());

	m_buffer = new unsigned char[BUFFER_LENGTH];
}

CIcomController::~CIcomController()
{
	delete[] m_buffer;
}

bool CIcomController::start()
{
	bool ret = m_serial.open();
	if (!ret)
		return false;

	Create();
	SetPriority(100U);
	Run();

	return true;
}

void* CIcomController::Entry()
{
	wxLogMessage(wxT("Starting Icom Controller thread"));

	// Clock every 5ms-ish
	CTimer pollTimer(200U, 0U, 100U);
	pollTimer.start();

	while (!m_stopped) {
		// Poll the modem status every 100ms
		if (pollTimer.hasExpired()) {
			m_serial.write((unsigned char*)"\xFF\xFF\xFF", 3U);
			pollTimer.start();
		}

		unsigned int length;
		RESP_TYPE_ICOM type = getResponse(m_buffer, length);

		switch (type) {
		case RTI_TIMEOUT:
			break;

		case RTI_ERROR: {
			wxLogMessage(wxT("Stopping Icom Controller thread"));
			return NULL;

		case RTI_HEADER: {
				// CUtils::dump(wxT("RT_HEADER"), m_buffer, length);
				wxMutexLocker locker(m_mutex);

				unsigned char data[2U];
				data[0U] = DSMTT_HEADER;
				data[1U] = RADIO_HEADER_LENGTH_BYTES;
				m_rxData.addData(data, 2U);

				m_rxData.addData(m_buffer + 3U, RADIO_HEADER_LENGTH_BYTES);

				m_rx = true;
			}
			break;

		case RTI_DATA: {
				// CUtils::dump(wxT("RT_DATA"), m_buffer, length);
				wxMutexLocker locker(m_mutex);

				unsigned char data[2U];
				data[0U] = DSMTT_DATA;
				data[1U] = DV_FRAME_LENGTH_BYTES;
				m_rxData.addData(data, 2U);

				m_rxData.addData(m_buffer + 5U, DV_FRAME_LENGTH_BYTES);

				m_rx = true;
			}
			break;

		case RTI_EOT: {
				// wxLogMessage(wxT("RT_EOT"));
				wxMutexLocker locker(m_mutex);

				unsigned char data[2U];
				data[0U] = DSMTT_EOT;
				data[1U] = 0U;
				m_rxData.addData(data, 2U);

				m_rx = false;
			}
			break;

		default:
			wxLogMessage(wxT("Unknown message, type: %02X"), m_buffer[3U]);
			CUtils::dump(wxT("Buffer dump"), m_buffer, length);
			break;
		}

#ifdef notdef
		if (space > 0U) {
			if (writeType == DSMTT_NONE && m_txData.hasData()) {
				wxMutexLocker locker(m_mutex);

				m_txData.getData(&writeType, 1U);
				m_txData.getData(&writeLength, 1U);
				m_txData.getData(writeBuffer, writeLength);
			}

			// Only send the start when the TX is off
			if (!m_tx && writeType == DSMTT_START) {
				// CUtils::dump(wxT("Write Header"), writeBuffer, writeLength);

				int ret = m_serial.write(writeBuffer, writeLength);
				if (ret != int(writeLength))
					wxLogWarning(wxT("Error when writing the header to the Icom"));

				writeType = DSMTT_NONE;
				space--;
			}

			if (space > 4U && writeType == DSMTT_HEADER) {
				// CUtils::dump(wxT("Write Header"), writeBuffer, writeLength);

				int ret = m_serial.write(writeBuffer, writeLength);
				if (ret != int(writeLength))
					wxLogWarning(wxT("Error when writing the header to the Icom"));

				writeType = DSMTT_NONE;
				space -= 4U;
			}

			if (writeType == DSMTT_DATA || writeType == DSMTT_EOT) {
				// CUtils::dump(wxT("Write Data"), writeBuffer, writeLength);

				int ret = m_serial.write(writeBuffer, writeLength);
				if (ret != int(writeLength))
					wxLogWarning(wxT("Error when writing data to the Icom"));

				writeType = DSMTT_NONE;
				space--;
			}
#endif
		}

		Sleep(5UL);

		pollTimer.clock();
	}

	m_serial.close();

	wxLogMessage(wxT("Stopping Icom Controller thread"));

	return NULL;
}

#ifdef notdef
void* CIcomController::Entry()
{
	wxLogMessage(wxT("Starting Icom Controller thread"));

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
					wxLogMessage(wxT("Stopping Icom Controller thread"));
					return NULL;
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
						wxLogMessage(wxT("Stopping Icom Controller thread"));
						return NULL;
					}
				}
				break;

			case RTM_RXPREAMBLE:
				// wxLogMessage(wxT("RT_PREAMBLE"));
				break;

			case RTM_START:
				// wxLogMessage(wxT("RT_START"));
				break;

			case RTM_HEADER:
				CUtils::dump(wxT("RT_HEADER"), m_buffer, length);
				if (length == 7U) {
					if (m_buffer[4U] == DVRPTR_NAK)
						wxLogWarning(wxT("Received a header NAK from the Icom"));
				} else {
					bool correct = (m_buffer[5U] & 0x80U) == 0x00U;
					if (correct) {
						wxMutexLocker locker(m_mutex);

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
				// wxLogMessage(wxT("RT_RXSYNC"));
				break;

			case RTM_DATA:
				CUtils::dump(wxT("RT_DATA"), m_buffer, length);
				if (length == 7U) {
					if (m_buffer[4U] == DVRPTR_NAK)
						wxLogWarning(wxT("Received a data NAK from the Icom"));
				} else {
					wxMutexLocker locker(m_mutex);

					unsigned char data[2U];
					data[0U] = DSMTT_DATA;
					data[1U] = DV_FRAME_LENGTH_BYTES;
					m_rxData.addData(data, 2U);

					m_rxData.addData(m_buffer + 8U, DV_FRAME_LENGTH_BYTES);

					m_rx = true;
				}
				break;

			case RTM_EOT: {
					wxLogMessage(wxT("RT_EOT"));
					wxMutexLocker locker(m_mutex);

					unsigned char data[2U];
					data[0U] = DSMTT_EOT;
					data[1U] = 0U;
					m_rxData.addData(data, 2U);

					m_rx = false;
				}
				break;

			case RTM_RXLOST: {
					wxLogMessage(wxT("RT_LOST"));
					wxMutexLocker locker(m_mutex);

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
					// CUtils::dump(wxT("GET_STATUS"), m_buffer, length);
					// wxLogMessage(wxT("PTT=%d tx=%u space=%u cksum=%d, tx enabled=%d"), int(m_tx), m_txSpace, space, int(m_checksum), int(m_txEnabled));
				}
				break;

			// These should not be received in this loop, but don't complain if we do
			case RTM_GET_VERSION:
			case RTM_GET_SERIAL:
			case RTM_GET_CONFIG:
				break;

			default:
				wxLogMessage(wxT("Unknown message, type: %02X"), m_buffer[3U]);
				CUtils::dump(wxT("Buffer dump"), m_buffer, length);
				break;
		}

		if (space > 0U) {
			if (writeType == DSMTT_NONE && m_txData.hasData()) {
				wxMutexLocker locker(m_mutex);

				m_txData.getData(&writeType, 1U);
				m_txData.getData(&writeLength, 1U);
				m_txData.getData(writeBuffer, writeLength);
			}

			// Only send the start when the TX is off
			if (!m_tx && writeType == DSMTT_START) {
				// CUtils::dump(wxT("Write Header"), writeBuffer, writeLength);

				int ret = m_serial.write(writeBuffer, writeLength);
				if (ret != int(writeLength))
					wxLogWarning(wxT("Error when writing the header to the Icom"));

				writeType = DSMTT_NONE;
				space--;
			}

			if (space > 4U && writeType == DSMTT_HEADER) {
				// CUtils::dump(wxT("Write Header"), writeBuffer, writeLength);

				int ret = m_serial.write(writeBuffer, writeLength);
				if (ret != int(writeLength))
					wxLogWarning(wxT("Error when writing the header to the Icom"));

				writeType = DSMTT_NONE;
				space -= 4U;
			}

			if (writeType == DSMTT_DATA || writeType == DSMTT_EOT) {
				// CUtils::dump(wxT("Write Data"), writeBuffer, writeLength);

				int ret = m_serial.write(writeBuffer, writeLength);
				if (ret != int(writeLength))
					wxLogWarning(wxT("Error when writing data to the Icom"));

				writeType = DSMTT_NONE;
				space--;
			}
		}

		Sleep(5UL);

		pollTimer.clock();
	}

	wxLogMessage(wxT("Stopping Icom Controller thread"));

	setEnabled(false);

	delete[] writeBuffer;

	m_serial.close();

	return NULL;
}
#endif

bool CIcomController::writeHeader(const CHeaderData& header)
{
#ifdef notdef
	if (!m_txEnabled)
		return false;

	bool ret = m_txData.hasSpace(64U);
	if (!ret) {
		wxLogWarning(wxT("No space to write the header"));
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

	wxString rpt2 = header.getRptCall2();
	for (unsigned int i = 0U; i < rpt2.Len() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer2[i + 11U]  = rpt2.GetChar(i);

	wxString rpt1 = header.getRptCall1();
	for (unsigned int i = 0U; i < rpt1.Len() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer2[i + 19U] = rpt1.GetChar(i);

	wxString your = header.getYourCall();
	for (unsigned int i = 0U; i < your.Len() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer2[i + 27U] = your.GetChar(i);

	wxString my1 = header.getMyCall1();
	for (unsigned int i = 0U; i < my1.Len() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer2[i + 35U] = my1.GetChar(i);

	wxString my2 = header.getMyCall2();
	for (unsigned int i = 0U; i < my2.Len() && i < SHORT_CALLSIGN_LENGTH; i++)
		buffer2[i + 43U] = my2.GetChar(i);

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

	wxMutexLocker locker(m_mutex);

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
#endif
	return true;
}

bool CIcomController::writeData(const unsigned char* data, unsigned int, bool end)
{
#ifdef notdef
	if (!m_txEnabled)
		return false;

	bool ret = m_txData.hasSpace(26U);
	if (!ret) {
		wxLogWarning(wxT("No space to write data"));
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

		wxMutexLocker locker(m_mutex);

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

	wxMutexLocker locker(m_mutex);

	unsigned char type = DSMTT_DATA;
	m_txData.addData(&type, 1U);

	unsigned char len = 24U;
	m_txData.addData(&len, 1U);

	m_txData.addData(buffer, 24U);
#endif
	return true;
}

unsigned int CIcomController::getSpace()
{
	// return m_txData.freeSpace() / 26U;
	return true;
}

bool CIcomController::isTXReady()
{
	return true;

#ifdef notdef
	if (m_tx)
		return false;

	return m_txData.isEmpty();
#endif
}

wxString CIcomController::getPath() const
{
	return wxEmptyString;
}

RESP_TYPE_ICOM CIcomController::getResponse(unsigned char *buffer, unsigned int& length)
{
	// Get the start of the frame or nothing at all
	int ret = m_serial.read(buffer, 1U);
	if (ret < 0) {
		wxLogError(wxT("Error when reading from the Icom radio"));
		return RTI_ERROR;
	}

	if (ret == 0)
		return RTI_TIMEOUT;

	if (buffer[0U] != ICOM_FRAME_START)
		return RTI_TIMEOUT;

	do {
		ret = m_serial.read(buffer + 1U, 1U);
		if (ret < 0) {
			wxLogError(wxT("Error when reading from the Icom radio"));
			return RTI_ERROR;
		}
		if (ret == 0)
			Sleep(5UL);
	} while (ret == 0);

	if (buffer[1U] == ICOM_FRAME_START)
		return RTI_TIMEOUT;

	length = buffer[1U] + 1U;

	if (length >= 100U) {
		wxLogError(wxT("Invalid data received from the Icom radio"));
		return RTI_ERROR;
	}

	unsigned int offset = 2U;

	while (offset < length) {
		ret = m_serial.read(buffer + offset, length - offset);
		if (ret < 0) {
			wxLogError(wxT("Error when reading from the Icom radio"));
			return RTI_ERROR;
		}

		if (ret > 0)
			offset += ret;

		if (ret == 0)
			Sleep(5UL);
	}

	// CUtils::dump(wxT("Received"), buffer, length);

	switch (length) {
		case 45U:
			return RTI_HEADER;
		case 17U:
			if ((buffer[4U] & 0x40U) == 0x40U)
				return RTI_EOT;
			else
				return RTI_DATA;
		default:
			return RTI_UNKNOWN;
	}
}
