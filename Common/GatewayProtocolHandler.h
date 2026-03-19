/*
 *   Copyright (C) 2009-2014 by Jonathan Naylor G4KLX
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

#ifndef	GatewayProtocolHander_H
#define	GatewayProtocolHander_H

#include "UDPReaderWriter.h"
#include "DStarDefines.h"
#include "StdCompat.h"

/*
 * Implements the D-Star Repeater Protocol (DSRP) from the gateway side.
 *
 * This is the mirror of CRepeaterProtocolHandler — it is used by the gateway
 * process (ircDDBGateway/DStarGateway) to communicate with one or more
 * repeater daemons.  Unlike the repeater side, there is no fixed peer address:
 * each packet carries the sender's address and port, which are passed through
 * to the caller so it can route replies back to the correct repeater.
 *
 * The same DSRP packet types are used as on the repeater side (0x20 header,
 * 0x21 data, 0x0B register).  The key difference is that the gateway sends
 * headers four times for redundancy (vs two on the repeater side).
 */
class CGatewayProtocolHandler {
public:
	CGatewayProtocolHandler(const std::string& localAddress, unsigned int localPort);
	~CGatewayProtocolHandler();

	bool open();

	// Sends the header packet four times to reduce the impact of UDP packet loss.
	bool writeHeader(const unsigned char* header, uint16_t id, const in_addr& address, unsigned int port);
	bool writeData(const unsigned char* data, unsigned int length, uint16_t id, uint8_t seqNo, const in_addr& address, unsigned int port);

	// Drains pending datagrams; fills address/port with the sender for routing.
	NETWORK_TYPE read(uint16_t& id, in_addr& address, unsigned int& port);
	unsigned int readHeader(unsigned char* data, unsigned int length);
	unsigned int readData(unsigned char* data, unsigned int length, uint8_t& seqNo, unsigned int& errors);
	// Reads a Register packet; name is the repeater's self-reported name string.
	unsigned int readRegister(std::string& name);

	void close();

private:
	CUDPReaderWriter m_socket;
	NETWORK_TYPE     m_type;
	unsigned char*   m_buffer;
	unsigned int     m_length;

	bool readPackets(uint16_t& id, in_addr& address, unsigned int& port);
};

#endif
