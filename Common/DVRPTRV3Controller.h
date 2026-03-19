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

#ifndef	DVRPTRV3Controller_H
#define	DVRPTRV3Controller_H

#include "SerialDataController.h"
#include "TCPReaderWriter.h"
#include "DStarDefines.h"
#include "RingBuffer.h"
#include "Modem.h"
#include "Utils.h"
#include "StdCompat.h"

#include <string>

enum RESP_TYPE_V3 {
	RT3_TIMEOUT,
	RT3_ERROR,
	RT3_UNKNOWN,
	RT3_SPACE,
	RT3_QUERY,
	RT3_CONFIG,
	RT3_HEADER,
	RT3_DATA
};

/*
 * CDVRPTRV3Controller - Driver for DV-RPTR V3 boards.
 *
 * Functionally identical to CDVRPTRV2Controller; V3 uses the same ASCII "HEAD"
 * protocol and the same CT_USB / CT_NETWORK connection type switching.
 * The code is a parallel implementation maintained separately for V3 firmware
 * compatibility.  See CDVRPTRV2Controller for full protocol documentation.
 */
class CDVRPTRV3Controller : public CModem {
public:
	// USB serial connection.
	CDVRPTRV3Controller(const std::string& port, const std::string& path, bool txInvert, unsigned int modLevel, bool duplex, const std::string& callsign, unsigned int txDelay);
	// TCP network connection.
	CDVRPTRV3Controller(const std::string& address, unsigned int port, bool txInvert, unsigned int modLevel, bool duplex, const std::string& callsign, unsigned int txDelay);
	virtual ~CDVRPTRV3Controller();

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

	RESP_TYPE_V3 getResponse(unsigned char* buffer, unsigned int& length);

	bool findPort();
	bool findPath();

	bool findModem();
	bool openModem();

	int readModem(unsigned char* buffer, unsigned int length);
	bool writeModem(const unsigned char* buffer, unsigned int length);
	void closeModem();
};

#endif
