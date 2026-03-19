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

#ifndef	DVRPTRV2Controller_H
#define	DVRPTRV2Controller_H

#include "SerialDataController.h"
#include "TCPReaderWriter.h"
#include "DStarDefines.h"
#include "RingBuffer.h"
#include "Modem.h"
#include "Utils.h"
#include "StdCompat.h"

#include <string>

enum RESP_TYPE_V2 {
	RT2_TIMEOUT,
	RT2_ERROR,
	RT2_UNKNOWN,
	RT2_SPACE,
	RT2_QUERY,
	RT2_CONFIG,
	RT2_HEADER,
	RT2_DATA
};

/*
 * CDVRPTRV2Controller - Driver for DV-RPTR V2 boards.
 *
 * Supports two physical connection types, selected by constructor:
 *   CT_USB     - USB CDC-ACM serial port (same as V1).
 *   CT_NETWORK - TCP socket (address + port); used for networked / remote modems.
 *
 * The CONNECTION_TYPE flag (m_connection) controls which of the two backing
 * objects (m_usb / m_network) is used; readModem(), writeModem(), closeModem()
 * dispatch through it uniformly so the rest of the logic is connection-agnostic.
 *
 * Protocol: ASCII-framed "HEAD" protocol (distinct from the binary DVRPTR protocol
 * used by V1).  Each frame starts with the 4-byte magic "HEAD" followed by a
 * 1-byte type character:
 *   'X' - 105-byte control/header frame  (HEADX)
 *   'Y' - 10-byte  space/status frame    (HEADY)
 *   'Z' - 20-byte  voice data frame      (HEADZ)
 * Frame subtypes are identified by 4 ASCII digits at bytes [5..8].
 *
 * On startup the driver sends a HEADX/9000 query to get the hardware serial,
 * then HEADX/9001 to configure the modem (callsign, duplex, modLevel, txDelay).
 * During operation the driver polls HEADY/9011 every 250ms to get TX buffer space.
 *
 * The header frame (HEADX/0002) also contains the first DV frame of the
 * transmission (at byte offset 51), so the repeater receives both the D-Star
 * header and the first voice frame in a single network packet.
 *
 * Reconnection works identically to V1: findPort() -> openModem() retried every 2s.
 */
class CDVRPTRV2Controller : public CModem {
public:
	// USB serial connection.
	CDVRPTRV2Controller(const std::string& port, const std::string& path, bool txInvert, unsigned int modLevel, bool duplex, const std::string& callsign, unsigned int txDelay);
	// TCP network connection.
	CDVRPTRV2Controller(const std::string& address, unsigned int port, bool txInvert, unsigned int modLevel, bool duplex, const std::string& callsign, unsigned int txDelay);
	virtual ~CDVRPTRV2Controller();

	virtual bool start();

	virtual unsigned int getSpace();
	virtual bool isTXReady();

	virtual bool writeHeader(const CHeaderData& header);
	virtual bool writeData(const unsigned char* data, unsigned int length, bool end);

	virtual std::string getPath() const;

private:
	void entry();

	CONNECTION_TYPE            m_connection;
	std::string                m_usbPort;
	std::string                m_usbPath;
	std::string                m_address;
	unsigned int               m_port;
	bool                       m_txInvert;
	unsigned int               m_modLevel;
	bool                       m_duplex;
	std::string                m_callsign;
	unsigned int               m_txDelay;
	CSerialDataController*     m_usb;
	CTCPReaderWriter*          m_network;
	unsigned char*             m_buffer;
	CRingBuffer<unsigned char> m_txData;
	bool                       m_rx;

	bool readSerial();
	bool setConfig();
	bool readSpace();

	RESP_TYPE_V2 getResponse(unsigned char* buffer, unsigned int& length);

	bool findPort();
	bool findPath();

	bool findModem();
	bool openModem();

	int readModem(unsigned char* buffer, unsigned int length);
	bool writeModem(const unsigned char* buffer, unsigned int length);
	void closeModem();
};

#endif
