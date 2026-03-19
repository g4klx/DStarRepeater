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

#ifndef	IcomController_H
#define	IcomController_H

#include "SerialDataController.h"
#include "RingBuffer.h"
#include "Modem.h"
#include "Utils.h"
#include "StdCompat.h"

#include <string>

enum RESP_TYPE_ICOM {
	RTI_TIMEOUT,
	RTI_ERROR,
	RTI_UNKNOWN,
	RTI_HEADER,
	RTI_DATA,
	RTI_EOT,
	RTI_HEADER_ACK,
	RTI_DATA_ACK,
	RTI_PONG
};

/*
 * CIcomController - Driver for Icom radios in terminal/access-point mode.
 *
 * Hardware interface: Serial at 38400 baud with RTS/CTS flow control.
 *
 * Protocol: Length-prefixed binary frames.  The first byte of each frame is
 * the payload length; valid lengths are 0x03, 0x04, 0x10, and 0x2C.
 * The second byte is the message type:
 *   0x03 - PONG (keepalive response)
 *   0x10 - D-Star header received (44 bytes payload)
 *   0x12 - D-Star data frame received (12 bytes payload)
 *          bit 6 of byte[3] set = end-of-transmission
 *   0x21 - Header ACK/NAK (byte[2] == 0x00 is ACK)
 *   0x23 - Data ACK/NAK   (byte[2] = sequence, byte[3] == 0x00 is ACK)
 *
 * Packet counters:
 *   m_txCounter  - monotonically incrementing TX stream ID (rolls at 255 -> 1).
 *   m_pktCounter - per-packet sequence within a stream, resets on data-sync
 *                  pattern (0x55 0x2D 0x16) or at 21.  The radio uses this to
 *                  detect lost frames and request retransmission.
 *
 * Connection management:
 *   The driver sends poll frames (0xFF 0xFF 0xFF) initially and ping frames
 *   (0x02 0x02 0xFF) after the 18th poll.  A 5-second lostTimer triggers
 *   reconnection if no response (PONG or data) is received.
 *
 * ACK/retry:
 *   Each transmitted packet is held in storeData until the matching ACK is
 *   received (matched by sequence number for data frames).  A 50ms retryTimer
 *   re-sends the packet if no ACK arrives.  txSpace gates the send queue to
 *   ensure only one unacknowledged packet is in flight at a time.
 *
 * getPath() returns an empty string - Icom radios do not use the sysfs USB
 * path tracking used by the MMDVM/DV-RPTR family.
 */
class CIcomController : public CModem {
public:
	CIcomController(const std::string& port);
	virtual ~CIcomController();

	virtual bool start();

	virtual unsigned int getSpace();
	virtual bool isTXReady();

	virtual bool writeHeader(const CHeaderData& header);
	virtual bool writeData(const unsigned char* data, unsigned int length, bool end);

	virtual std::string getPath() const;

private:
	void entry();

	std::string                m_port;
	CSerialDataController      m_serial;
	CRingBuffer<unsigned char> m_txData;
	unsigned char              m_txCounter;
	unsigned char              m_pktCounter;

	RESP_TYPE_ICOM getResponse(unsigned char* buffer, unsigned int& length);
	// Sends 0xFF 0xFF 0xFF - null keepalive before the radio responds to pings.
	bool writePoll();
	// Sends 0x02 0x02 0xFF - ping request; elicits a PONG response from the radio.
	bool writePing();
};

#endif
