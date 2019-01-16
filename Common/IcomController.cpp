/*
 *   Copyright (C) 2011-2015,2018,2019 by Jonathan Naylor G4KLX
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

#include "IcomController.h"
#include "DStarDefines.h"
#include "Timer.h"
#include "Utils.h"

#if defined(__WINDOWS__)
#include <setupapi.h>
#else
#include <wx/dir.h>
#endif

enum STATE_ICOM {
	SI_NONE,
	SI_HEADER,
	SI_DATA
};

const unsigned int MAX_RESPONSES = 30U;

const unsigned int BUFFER_LENGTH = 200U;

CIcomController::CIcomController(const wxString& port) :
CModem(),
m_port(port),
m_serial(port, SERIAL_38400, true),
m_txData(2000U),
m_txCounter(0U),
m_pktCounter(0U)
{
	wxASSERT(!port.IsEmpty());
}

CIcomController::~CIcomController()
{
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

	CTimer retryTimer(200U, 0U, 50U);

	CTimer lostTimer(200U, 5U);
	lostTimer.start();
	
	unsigned char storeData[BUFFER_LENGTH];
	unsigned int storeLength = 0U;
	
	unsigned int pollCount = 0U;
	
	bool connected = false;
	
	STATE_ICOM    state   = SI_NONE;
	unsigned char seqNo   = 0U;
	bool          txSpace = true;

	while (!m_stopped) {
		if (pollTimer.hasExpired()) {
			if (pollCount == 18U) {
				pollTimer.setTimeout(1U);
				writePing();
			} else if (pollCount > 18U) {
				writePing();
			} else {
				writePoll();
			}

			pollTimer.start();
			pollCount++;
		}

		unsigned char buffer[BUFFER_LENGTH];
		unsigned int length = 0U;
		RESP_TYPE_ICOM type = getResponse(buffer, length);

		switch (type) {
		case RTI_TIMEOUT:
			break;

		case RTI_ERROR:
			wxLogMessage(wxT("Stopping Icom Controller thread"));
			return NULL;

		case RTI_HEADER: {
				// CUtils::dump(wxT("RTI_HEADER"), buffer, length);

				wxMutexLocker locker(m_mutex);

				unsigned char data[2U];
				data[0U] = DSMTT_HEADER;
				data[1U] = RADIO_HEADER_LENGTH_BYTES;
				m_rxData.addData(data, 2U);

				m_rxData.addData(buffer + 2U, RADIO_HEADER_LENGTH_BYTES);

				lostTimer.start();
				txSpace = true;
			}
			break;

		case RTI_DATA: {
				// CUtils::dump(wxT("RTI_DATA"), buffer, length);

				wxMutexLocker locker(m_mutex);

				unsigned char data[2U];
				data[0U] = DSMTT_DATA;
				data[1U] = DV_FRAME_LENGTH_BYTES;
				m_rxData.addData(data, 2U);

				m_rxData.addData(buffer + 4U, DV_FRAME_LENGTH_BYTES);

				lostTimer.start();
				txSpace = true;
			}
			break;

		case RTI_EOT: {
				// wxLogMessage(wxT("RTI_EOT"));

				wxMutexLocker locker(m_mutex);

				unsigned char data[2U];
				data[0U] = DSMTT_EOT;
				data[1U] = 0U;
				m_rxData.addData(data, 2U);

				lostTimer.start();
				txSpace = true;
			}
			break;

		case RTI_PONG:
			// wxLogMessage(wxT("RTI_PONG"));
			if (!connected)
				wxLogMessage(wxT("Connected to the Icom radio"));
			lostTimer.start();
			connected = true;
			break;

		case RTI_HEADER_ACK:
			if (buffer[2U] == 0x00U) {
				// wxLogMessage(wxT("RTI_HEADER_ACK"));
				if (state == SI_HEADER) {
					storeLength = 0U;
					retryTimer.stop();
					txSpace = true;
				}
			} else {
				wxLogMessage(wxT("RTI_HEADER_NAK"));
			}

			lostTimer.start();
			break;

		case RTI_DATA_ACK:
			if (buffer[3U] == 0x00U) {
				// wxLogMessage(wxT("RTI_DATA_ACK - %02X"), buffer[2U]);
				if (state == SI_DATA && seqNo == buffer[2U]) {
					storeLength = 0U;
					retryTimer.stop();
					txSpace = true;
				}
			} else {
				wxLogMessage(wxT("RTI_DATA_NAK - %02X"), buffer[2U]);
			}

			lostTimer.start();
			break;

		default:
			wxLogMessage(wxT("Unknown message, type: %02X"), buffer[1U]);
			CUtils::dump(wxT("Buffer dump"), buffer, length);
			break;
		}
		
		if (retryTimer.isRunning() && retryTimer.hasExpired()) {
			assert(storeLength > 0U);

			// CUtils::dump(wxT("Re-Sending"), storeData, storeLength + 1U);

			int ret = m_serial.write(storeData, storeLength + 1U);
			if (ret != int(storeLength + 1U))
				wxLogWarning(wxT("Error when writing to the Icom radio"));

			retryTimer.start();
			pollTimer.start();
		}

		if (txSpace && !m_txData.isEmpty()) {
			m_txData.getData(storeData, 1U);

			storeLength = storeData[0U];
			m_txData.getData(storeData + 1U, storeLength);

			// CUtils::dump(wxT("Sending"), storeData, storeLength + 1U);

			if (storeData[1U] == 0x20U) {
				state = SI_HEADER;
				seqNo = 0U;
			} else {
				state = SI_DATA;
				seqNo = storeData[2U];
			}

			int ret = m_serial.write(storeData, storeLength + 1U);
			if (ret != int(storeLength + 1U))
				wxLogWarning(wxT("Error when writing to the Icom radio"));

			retryTimer.start();
			pollTimer.start();

			txSpace = false;
		}

		Sleep(5UL);

		pollTimer.clock();
		lostTimer.clock();
		retryTimer.clock();
		
		if (lostTimer.hasExpired()) {
			if (connected)
				wxLogWarning(wxT("Lost connection to the Icom radio"));

			pollTimer.setTimeout(0U, 100U);
			pollTimer.start();
			pollCount = 0U;

			retryTimer.stop();
			storeLength = 0U;

			state = SI_NONE;
			seqNo = 0U;
			txSpace = true;

			lostTimer.start();
			connected = false;
		}
	}

	m_serial.close();

	wxLogMessage(wxT("Stopping Icom Controller thread"));

	return NULL;
}

bool CIcomController::writeHeader(const CHeaderData& header)
{
	bool ret = m_txData.hasSpace(43U);
	if (!ret) {
		wxLogWarning(wxT("No space to write the header"));
		return false;
	}

	unsigned char buffer[50U];

	buffer[0U] = 0x29U;

	buffer[1U] = 0x20U;

	::memset(buffer + 2U, ' ', RADIO_HEADER_LENGTH_BYTES);

	buffer[2U] = header.getFlag1();
	buffer[3U] = header.getFlag2();
	buffer[4U] = header.getFlag3();

	wxString rpt2 = header.getRptCall2();
	for (unsigned int i = 0U; i < rpt2.Len() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer[i + 5U]  = rpt2.GetChar(i);

	wxString rpt1 = header.getRptCall1();
	for (unsigned int i = 0U; i < rpt1.Len() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer[i + 13U] = rpt1.GetChar(i);

	wxString your = header.getYourCall();
	for (unsigned int i = 0U; i < your.Len() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer[i + 21U] = your.GetChar(i);

	wxString my1 = header.getMyCall1();
	for (unsigned int i = 0U; i < my1.Len() && i < LONG_CALLSIGN_LENGTH; i++)
		buffer[i + 29U] = my1.GetChar(i);

	wxString my2 = header.getMyCall2();
	for (unsigned int i = 0U; i < my2.Len() && i < SHORT_CALLSIGN_LENGTH; i++)
		buffer[i + 37U] = my2.GetChar(i);

	buffer[41U] = 0xFFU;

	m_txCounter  = 0U;
	m_pktCounter = 0U;

	wxMutexLocker locker(m_mutex);

	m_txData.addData(buffer, 42U);

	return true;
}

bool CIcomController::writeData(const unsigned char* data, unsigned int, bool end)
{
	bool ret = m_txData.hasSpace(18U);
	if (!ret) {
		wxLogWarning(wxT("No space to write data"));
		return false;
	}

	unsigned char buffer[20U];

	if (end) {
		buffer[0U] = 0x10U;

		buffer[1U] = 0x22U;

		buffer[2U] = m_txCounter;
		buffer[3U] = m_pktCounter | 0x40U;

		::memcpy(buffer + 4U, END_PATTERN_BYTES, DV_FRAME_LENGTH_BYTES);

		buffer[16U] = 0xFFU;

		wxMutexLocker locker(m_mutex);

		m_txData.addData(buffer, 17U);

		return true;
	}

	buffer[0U] = 0x10U;

	buffer[1U] = 0x22U;

	buffer[2U] = m_txCounter;
	buffer[3U] = m_pktCounter;

	m_txCounter++;
	m_pktCounter++;
	if (::memcmp(buffer + VOICE_FRAME_LENGTH_BYTES, DATA_SYNC_BYTES, DATA_FRAME_LENGTH_BYTES) == 0 || m_pktCounter == 21U)
		m_pktCounter = 0U;

	::memcpy(buffer + 4U, data, DV_FRAME_LENGTH_BYTES);

	buffer[16U] = 0xFFU;

	wxMutexLocker locker(m_mutex);

	m_txData.addData(buffer, 17U);

	return true;
}

unsigned int CIcomController::getSpace()
{
	return m_txData.freeSpace() / 18U;
}

bool CIcomController::isTXReady()
{
	return true;
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
		wxLogError(wxT("Error when reading the length from the Icom radio"));
		return RTI_ERROR;
	}

	if (ret == 0)
		return RTI_TIMEOUT;

	if (buffer[0U] == 0xFFU)
		return RTI_TIMEOUT;

	length = buffer[0U];

	// Validate the message lengths
	if (buffer[0U] != 0x03U && buffer[0U] != 0x04U && buffer[0U] != 0x10U && buffer[0U] != 0x2CU) {
		wxLogError(wxT("Invalid data length received from the Icom radio - 0x%02X"), length);
		return RTI_TIMEOUT;
	}

	ret = m_serial.read(buffer + 1U, 1U, 40U);
	if (ret < 0) {
		wxLogError(wxT("Error when reading the type from the Icom radio"));
		return RTI_ERROR;
	}

	if (ret == 0)
		return RTI_TIMEOUT;

	// Validate the message types
	if (buffer[1U] != 0x03U && buffer[1U] != 0x10U && buffer[1U] != 0x12U && buffer[1U] != 0x21U && buffer[1U] != 0x23U) {
		wxLogError(wxT("Invalid data type received from the Icom radio - 0x%02X"), buffer[1U]);
		return RTI_TIMEOUT;
	}

	unsigned int offset = 2U;

	while (offset < length) {
		ret = m_serial.read(buffer + offset, length - offset, 40U);
		if (ret < 0) {
			wxLogError(wxT("Error when reading data from the Icom radio"));
			return RTI_ERROR;
		}

		if (ret > 0)
			offset += ret;

		if (ret == 0) {
			// CUtils::dump(wxT("Receive timed out"), buffer, offset);
			return RTI_TIMEOUT;
		}
	}

	// CUtils::dump(wxT("Received"), buffer, length);

	switch (buffer[1U]) {
		case 0x03U:
			return RTI_PONG;
		case 0x10U:
			return RTI_HEADER;
		case 0x12U:
			if ((buffer[3U] & 0x40U) == 0x40U)
				return RTI_EOT;
			else
				return RTI_DATA;
		case 0x21U:
			return RTI_HEADER_ACK;
		case 0x23U:
			return RTI_DATA_ACK;
		default:
			return RTI_UNKNOWN;
	}
}

bool CIcomController::writePoll()
{
	return m_serial.write((unsigned char*)"\xFF\xFF\xFF", 3U) == 3;
}

bool CIcomController::writePing()
{
	return m_serial.write((unsigned char*)"\x02\x02\xFF", 3U) == 3;
}
