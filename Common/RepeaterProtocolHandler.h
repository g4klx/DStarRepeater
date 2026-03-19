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

#ifndef	RepeaterProtocolHander_H
#define	RepeaterProtocolHander_H

#include "UDPReaderWriter.h"
#include "DStarDefines.h"
#include "HeaderData.h"
#include "StdCompat.h"

/*
 * Implements the D-Star Repeater Protocol (DSRP) from the repeater side.
 *
 * DSRP is the UDP-based protocol used between the repeater daemon and
 * ircDDBGateway/DStarGateway.  All packets begin with the four-byte magic
 * "DSRP", followed by a one-byte type code:
 *
 *   0x0A  Poll        — keepalive with a human-readable text string
 *   0x0B  Register    — sent on startup to name this repeater to the gateway
 *   0x20  Header      — D-Star radio header (callsigns + flags)
 *   0x21  Data        — one DV frame (voice + slow-data)
 *   0x22  Busy Header — header received while repeater is already busy
 *   0x23  Busy Data   — data received while repeater is already busy
 *
 * The repeater daemon calls write*() to push outbound audio toward the
 * gateway, and calls read() each timer tick to drain inbound packets.
 */
class CRepeaterProtocolHandler {
public:
	CRepeaterProtocolHandler(const std::string& gatewayAddress, unsigned int gatewayPort, const std::string& localAddress, unsigned int localPort, const std::string& name);
	~CRepeaterProtocolHandler();

	bool open();

	// Sends the header packet twice to reduce the impact of UDP packet loss.
	bool writeHeader(const CHeaderData& header);
	bool writeBusyHeader(const CHeaderData& header);
	bool writeData(const unsigned char* data, unsigned int length, unsigned int errors, bool end);
	bool writeBusyData(const unsigned char* data, unsigned int length, unsigned int errors, bool end);
	bool writePoll(const std::string& text);
	bool writeRegister();

	// Drains all pending UDP datagrams; returns the type of the last useful one.
	NETWORK_TYPE read();
	void         readText(std::string& text, LINK_STATUS& status, std::string& reflector);
	void         readTempText(std::string& text);
	std::string  readStatus1();
	std::string  readStatus2();
	std::string  readStatus3();
	std::string  readStatus4();
	std::string  readStatus5();
	CHeaderData* readHeader();
	unsigned int readData(unsigned char* data, unsigned int length, unsigned char& seqNo);

	// Clears the current inbound session ID so a new stream can be accepted.
	void reset();

	void close();

private:
	CUDPReaderWriter m_socket;
	in_addr          m_address;   // Resolved gateway address; packets from any other source are dropped.
	unsigned int     m_port;
	std::string      m_name;
	uint16_t         m_outId;     // Random session ID assigned at the start of each transmission.
	uint8_t          m_outSeq;    // Per-frame sequence number (0–20, resets on each sync frame).
	NETWORK_TYPE     m_type;
	uint16_t         m_inId;      // Session ID of the stream currently being received (0 = idle).
	unsigned char*   m_buffer;
	unsigned int     m_length;

	// Reads one UDP datagram and sets m_type; returns true if more may be waiting.
	bool readPackets();
};

#endif
