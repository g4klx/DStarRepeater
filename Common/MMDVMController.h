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

#ifndef	MMDVMController_H
#define	MMDVMController_H

#include "SerialDataController.h"
#include "RingBuffer.h"
#include "Modem.h"
#include "Utils.h"
#include "StdCompat.h"

#include <string>

enum RESP_TYPE_MMDVM {
	RTDVM_TIMEOUT,
	RTDVM_ERROR,
	RTDVM_UNKNOWN,
	RTDVM_GET_STATUS,
	RTDVM_GET_VERSION,
	RTDVM_DSTAR_HEADER,
	RTDVM_DSTAR_DATA,
	RTDVM_DSTAR_EOT,
	RTDVM_DSTAR_LOST,
	RTDVM_ACK,
	RTDVM_NAK,
	RTDVM_DUMP,
	RTDVM_DEBUG1,
	RTDVM_DEBUG2,
	RTDVM_DEBUG3,
	RTDVM_DEBUG4,
	RTDVM_DEBUG5
};

/*
 * CMMDVMController - Driver for MMDVM (Multi-Mode Digital Voice Modem) boards.
 *
 * Hardware interface: USB CDC-ACM serial at 115200 baud with RTS/CTS flow control.
 *
 * Protocol: Binary, 3-byte minimum frame.
 *   byte[0] = 0xE0 (MMDVM_FRAME_START, always)
 *   byte[1] = total frame length (including this 3-byte header)
 *   byte[2] = frame type (command/response code)
 *   byte[3..] = payload
 *
 * Frame types used for D-Star:
 *   0x00 GET_VERSION   - request firmware version string
 *   0x01 GET_STATUS    - request modem status (TX state, space, ADC overflow)
 *   0x02 SET_CONFIG    - configure modem (invert flags, levels, txDelay, mode)
 *   0x10 DSTAR_HEADER  - D-Star header (TX from host, RX from modem)
 *   0x11 DSTAR_DATA    - D-Star voice frame
 *   0x12 DSTAR_LOST    - RX sync lost (modem -> host)
 *   0x13 DSTAR_EOT     - end of transmission
 *   0x70 ACK           - positive acknowledgement
 *   0x7F NAK           - negative acknowledgement (with reason byte)
 *   0xF0 DUMP          - raw hex dump from firmware
 *   0xF1-0xF5 DEBUG1-5 - firmware debug messages with 0-4 int16 parameters
 *
 * Startup sequence (readVersion waits 2s for the firmware to boot):
 *   readVersion() -> setConfig() -> launch entry() thread
 *
 * During operation (entry() thread):
 *   - Sends GET_STATUS every 100ms (readStatus()), which triggers a status
 *     response containing the current TX flag and remaining TX buffer space.
 *   - Buffer space is used to gate outbound header and data writes:
 *       space > 4 required to send a header (4 frames reserved)
 *       space > 1 required to send a data frame
 *
 * pttInvert: inverts the PTT output polarity (useful when the radio uses
 * active-low PTT keying).
 * rxLevel / txLevel: 0-100%, scaled to 0-255 for the modem's ADC/DAC ranges.
 *
 * Auto-detection (findPort / findPath): same sysfs USB path mechanism as
 * CDVMegaController; allows the modem to be found after a USB re-enumeration.
 */
class CMMDVMController : public CModem {
public:
	CMMDVMController(const std::string& port, const std::string& path, bool rxInvert, bool txInvert, bool pttInvert, unsigned int txDelay, unsigned int rxLevel, unsigned int txLevel);
	virtual ~CMMDVMController();

	virtual bool start();

	virtual unsigned int getSpace();
	virtual bool isTXReady();

	virtual bool writeHeader(const CHeaderData& header);
	virtual bool writeData(const unsigned char* data, unsigned int length, bool end);

	virtual std::string getPath() const;

private:
	void entry();

	std::string                m_port;
	std::string                m_path;
	bool                       m_rxInvert;
	bool                       m_txInvert;
	bool                       m_pttInvert;
	unsigned int               m_txDelay;
	unsigned int               m_rxLevel;
	unsigned int               m_txLevel;
	CSerialDataController      m_serial;
	unsigned char*             m_buffer;
	CRingBuffer<unsigned char> m_txData;
	bool                       m_rx;

	bool readVersion();
	bool readStatus();
	bool setConfig();

	RESP_TYPE_MMDVM getResponse(unsigned char* buffer, unsigned int& length);

	bool findPort();
	bool findPath();

	bool findModem();
	bool openModem();

	// Decodes and logs a DEBUG1-DEBUG5 firmware debug frame.
	// DEBUG1 carries a string; DEBUG2-5 append 1-4 big-endian int16 values.
	void printDebug();
};

#endif
