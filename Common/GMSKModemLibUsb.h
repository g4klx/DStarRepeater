/*
 *   Copyright (C) 2010-2014 by Jonathan Naylor G4KLX
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

#ifndef	GMSKModemLibUsb_H
#define	GMSKModemLibUsb_H

#include "GMSKModem.h"
#include "Utils.h"

#include <libusb-1.0/libusb.h>

/*
 * CGMSKModemLibUsb - libusb implementation of IGMSKModem for GMSK USB modems.
 *
 * USB vendor ID: 0x04D8 (Microchip).  The address parameter is the USB product
 * ID used to distinguish between multiple modems on the same host.
 *
 * All modem operations use USB control transfers (libusb_control_transfer).
 * Request direction encoding:
 *   0xC0 = device-to-host (IN):  GET_VERSION, GET_HEADER, GET_DATA,
 *                                GET_AD_STATUS, GET_REMAINSPACE
 *   0x40 = host-to-device (OUT): SET_AD_INIT, SET_PTT, PUT_DATA,
 *                                SET_MyCALL, SET_YourCALL, SET_RPT1CALL,
 *                                SET_RPT2CALL, SET_FLAGS, SET_MyCALL2
 *
 * The io() helper retries each control transfer up to 4 times on transient
 * errors, returning -ENODEV (-19) immediately on device disconnect.
 *
 * DUTCH*Star firmware quirk (m_brokenSpace):
 *   Some versions report an incorrect remaining-space value from GET_REMAINSPACE.
 *   When detected by the firmware version string, the space check is bypassed
 *   by treating hasSpace() as always returning STATE_TRUE.
 *
 * Data is transferred in GMSK_MODEM_DATA_LENGTH chunks.  Frames larger than
 * one chunk are split across two sequential PUT_DATA transfers with a 3ms gap
 * to give libusb recovery time between transfers.
 */
class CGMSKModemLibUsb : public IGMSKModem {
public:
	CGMSKModemLibUsb(unsigned int address);
	virtual ~CGMSKModemLibUsb();

	virtual bool open();

	virtual bool readHeader(unsigned char* header, unsigned int length);
	virtual int  readData(unsigned char* data, unsigned int length, bool& end);

	virtual TRISTATE getPTT();
	virtual void     setPTT(bool on);

	virtual TRISTATE hasSpace();

	virtual void writeHeader(unsigned char* data, unsigned int length);
	virtual int  writeData(unsigned char* data, unsigned int length);

	virtual void close();

private:
	unsigned int             m_address;
	libusb_context*          m_context;
	libusb_device_handle*    m_dev;

	// Executes a USB control transfer, retrying up to 4 times on transient errors.
	// requestType: 0xC0 = device-to-host, 0x40 = host-to-device.
	// Returns byte count on success, negative libusb error code on failure.
	int io(uint8_t requestType, uint8_t request, uint16_t value, uint16_t index, unsigned char* data, uint16_t length, unsigned int timeout);

	bool                     m_brokenSpace;

	bool openModem();
};

#endif
