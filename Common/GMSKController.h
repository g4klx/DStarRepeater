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

#ifndef	GMSKController_H
#define	GMSKController_H

#include "DStarDefines.h"
#include "RingBuffer.h"
#include "GMSKModem.h"
#include "Modem.h"
#include "Utils.h"
#include "StdCompat.h"

/*
 * CGMSKController - CModem driver that bridges the repeater to an IGMSKModem.
 *
 * Owns an IGMSKModem* (currently always a CGMSKModemLibUsb instance) and
 * presents the standard CModem interface to the repeater thread.
 *
 * entry() loop (runs ~15ms cycle):
 *   Receive side: polls readHeader() every 100ms when idle.  On a valid header,
 *   switches to data mode and calls readData() every loop until end is signalled.
 *   Received bytes are accumulated in a local buffer until a full DV_FRAME_LENGTH
 *   (12 bytes) is available, then forwarded to m_rxData.
 *
 *   Transmit side: reads type/length/data from m_txData ring buffer.  For headers,
 *   waits until the hardware PTT is deasserted before calling writeHeader() +
 *   setPTT(true).  For data frames, waits 100ms after the header (dataTimer) then
 *   polls hasSpace() before each writeData().  On EOT, calls setPTT(false).
 *
 *   duplex: when true, RX and TX can occur simultaneously.  When false, TX
 *   suppresses RX polling and vice versa.
 *
 * reopenModem(): called on any negative return from an IGMSKModem method.
 *   Closes the modem, clears m_txData, and retries open() every 1s indefinitely.
 */
class CGMSKController : public CModem {
public:
	CGMSKController(USB_INTERFACE iface, unsigned int address, bool duplex);
	virtual ~CGMSKController();

	virtual bool start();

	virtual unsigned int getSpace();
	virtual bool isTXReady();

	virtual bool writeHeader(const CHeaderData& header);
	virtual bool writeData(const unsigned char* data, unsigned int length, bool end);

private:
	void entry();

	IGMSKModem*                m_modem;
	bool                       m_duplex;
	unsigned char*             m_buffer;
	CRingBuffer<unsigned char> m_txData;

	bool reopenModem();
};

#endif
