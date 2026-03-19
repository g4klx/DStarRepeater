/*
 *   Copyright (C) 2011-2014 by Jonathan Naylor G4KLX
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

#ifndef	DVRPTRV1Controller_H
#define	DVRPTRV1Controller_H

#include "SerialDataController.h"
#include "RingBuffer.h"
#include "Modem.h"
#include "Utils.h"
#include "StdCompat.h"

#include <string>

enum RESP_TYPE_V1 {
	RT1_TIMEOUT,
	RT1_ERROR,
	RT1_UNKNOWN,
	RT1_GET_STATUS,
	RT1_GET_VERSION,
	RT1_GET_SERIAL,
	RT1_GET_CONFIG,
	RT1_SET_CONFIG,
	RT1_RXPREAMBLE,
	RT1_START,
	RT1_HEADER,
	RT1_RXSYNC,
	RT1_DATA,
	RT1_EOT,
	RT1_RXLOST,
	RT1_SET_TESTMDE
};

/*
 * CDVRPTRV1Controller - Driver for DV-RPTR V1 boards via serial (USB CDC-ACM, 115200 baud).
 *
 * Uses the same DVRPTR binary framing protocol as CDVMegaController:
 *   [0xD0] [len_lo] [len_hi] [type] [txCounter] [pktCounter] [payload...] [cksum]
 *
 * Hardware-specific parameters passed to SET_CONFIG (physical layer):
 *   channel   - selects which of the two physical input channels to use (bit 0x04
 *               in the config byte).  Some V1 boards have two discriminator inputs.
 *   modLevel  - output modulation level, 0-100%, scaled to 0-255 in the wire frame.
 *               Sets the DAC drive level for the TX audio path.
 *   txDelay   - PTT-to-data delay in milliseconds (little-endian uint16_t in frame).
 *               Allows time for the radio to reach full output power before
 *               transmitting the D-Star preamble.
 *
 * Reconnection and sysfs path tracking work identically to CDVMegaController.
 */
class CDVRPTRV1Controller : public CModem {
public:
	CDVRPTRV1Controller(const std::string& port, const std::string& path, bool rxInvert, bool txInvert, bool channel, unsigned int modLevel, unsigned int txDelay);
	virtual ~CDVRPTRV1Controller();

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
	bool                       m_channel;
	unsigned int               m_modLevel;
	unsigned int               m_txDelay;
	CSerialDataController      m_serial;
	unsigned char*             m_buffer;
	CRingBuffer<unsigned char> m_txData;
	unsigned char              m_txCounter;
	unsigned char              m_pktCounter;
	bool                       m_rx;
	unsigned int               m_txSpace;
	bool                       m_txEnabled;
	bool                       m_checksum;

	bool readVersion();
	bool readStatus();
	bool setConfig();
	bool setEnabled(bool enable);

	RESP_TYPE_V1 getResponse(unsigned char* buffer, unsigned int& length);

	bool findPort();
	bool findPath();

	bool findModem();
	bool openModem();
};

#endif
