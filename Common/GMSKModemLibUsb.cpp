/*
 *   Copyright (C) 2010-2014,2018 by Jonathan Naylor G4KLX
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

#include "GMSKModemLibUsb.h"
#include "DStarDefines.h"
#include "Logger.h"

#include <cassert>
#include <chrono>
#include <thread>
#include <string>

const unsigned int VENDOR_ID = 0x04D8U;

const int USB_TIMEOUT     = 1000;

const int SET_AD_INIT     = 0x00;
const int SET_PTT         = 0x05;
const int PUT_DATA        = 0x10;
const int GET_DATA        = 0x11;
const int GET_HEADER      = 0x21;
const int GET_AD_STATUS   = 0x30;
const int SET_MyCALL      = 0x40;
const int SET_MyCALL2     = 0x41;
const int SET_YourCALL    = 0x42;
const int SET_RPT1CALL    = 0x43;
const int SET_RPT2CALL    = 0x44;
const int SET_FLAGS       = 0x45;
const int GET_REMAINSPACE = 0x50;
const int GET_VERSION     = 0xFF;

const char COS_OnOff  = 0x02;
const char CRC_ERROR  = 0x04;
const char LAST_FRAME = 0x08;
const char PTT_OnOff  = 0x20;

const int PTT_ON  = 1;
const int PTT_OFF = 0;

static void libUsbLogError(int ret, const char *message) {
	std::string errorText(libusb_error_name(ret));
	wxLogMessage("%s, ret: %d, err=%s", message, ret, errorText.c_str());
}

CGMSKModemLibUsb::CGMSKModemLibUsb(unsigned int address) :
m_address(address),
m_context(nullptr),
m_dev(nullptr),
m_brokenSpace(false)
{
	::libusb_init(&m_context);
}


CGMSKModemLibUsb::~CGMSKModemLibUsb()
{
	assert(m_context != nullptr);
	::libusb_exit(m_context);
}

bool CGMSKModemLibUsb::open()
{
	assert(m_context != nullptr);
	assert(m_dev == nullptr);

	bool ret1 = openModem();
	if (!ret1) {
		wxLogError("Cannot find the GMSK Modem with address: 0x%04X", m_address);
		return false;
	}

	wxLogInfo("Found the GMSK Modem with address: 0x%04X", m_address);

	std::string version;

	int ret2;
	do {
		unsigned char buffer[GMSK_MODEM_DATA_LENGTH];
		ret2 = io(0xC0, GET_VERSION, 0, 0, buffer, GMSK_MODEM_DATA_LENGTH, USB_TIMEOUT);
		if (ret2 > 0) {
			version.append((char*)buffer, ret2);
		} else if (ret2 < 0) {
			::libUsbLogError(ret2, "GET_VERSION");
			close();
			return false;
		}
	} while (ret2 == int(GMSK_MODEM_DATA_LENGTH));

	wxLogInfo("Firmware version: %s", version.c_str());

	// Trap firmware version 0.1.00 of DUTCH*Star and complain loudly
	if (version.find("DUTCH*Star") != std::string::npos && version.find("0.1.00") != std::string::npos) {
		wxLogWarning("This modem firmware is not supported by the repeater");
		wxLogWarning("Please upgrade to a newer version");
		close();
		return false;
	}

	// DUTCH*Star firmware has a broken concept of free space
	if (version.find("DUTCH*Star") != std::string::npos)
		m_brokenSpace = true;

	return true;
}

// Polls the modem for a received D-Star header via repeated GET_HEADER transfers.
// Each transfer returns up to 8 bytes.  The loop accumulates bytes until
// RADIO_HEADER_LENGTH_BYTES have been received.
// If GET_HEADER returns 0 bytes mid-header and the COS (carrier-operated squelch)
// status bit is still set, the header counter is reset to zero because the signal
// has restarted (e.g. a quick key-up/key-down) and the modem's header buffer has
// been overwritten.  Returns false on CRC error (GET_AD_STATUS CRC_ERROR bit set).
bool CGMSKModemLibUsb::readHeader(unsigned char* header, unsigned int length)
{
	assert(header != nullptr);
	assert(length > (RADIO_HEADER_LENGTH_BYTES * 2U));

	unsigned int offset = 0U;

	while (offset < RADIO_HEADER_LENGTH_BYTES) {
		int ret = io(0xC0, GET_HEADER, 0, 0, (header + offset), 8, USB_TIMEOUT);
		if (ret < 0) {
			::libUsbLogError(ret, "GET_HEADER");

			if (ret == -19)				// -ENODEV
				return false;

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		} else if (ret == 0) {
			if (offset == 0U)
				return false;

			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			unsigned char status;
			int ret = io(0xC0, GET_AD_STATUS, 0, 0, &status, 1, USB_TIMEOUT);
			if (ret < 0) {
				::libUsbLogError(ret, "GET_COS");

				if (ret == -19)			// -ENODEV
					return false;

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			} else if (ret > 0) {
				if ((status & COS_OnOff) == COS_OnOff)
					offset = 0U;
			}
		} else {
			offset += ret;
		}
	}

	unsigned char status;
	int ret = io(0xC0, GET_AD_STATUS, 0, 0, &status, 1, USB_TIMEOUT);
	if (ret < 0) {
		::libUsbLogError(ret, "GET_CRC");
		return false;
	}

	if ((status & CRC_ERROR) == CRC_ERROR) {
		wxLogMessage("Header - CRC Error");
		return false;
	}

	return true;
}

int CGMSKModemLibUsb::readData(unsigned char* data, unsigned int length, bool& end)
{
	assert(data != nullptr);
	assert(length > 0U);

	end = false;

	int ret = io(0xC0, GET_DATA, 0, 0, data, GMSK_MODEM_DATA_LENGTH, USB_TIMEOUT);
	if (ret < 0) {
		if (ret == -19) {		// -ENODEV
			::libUsbLogError(ret, "GET_DATA");
			return ret;
		}

		return 0;
	} else if (ret == 0) {
		unsigned char status;
		int ret = io(0xC0, GET_AD_STATUS, 0, 0, &status, 1, USB_TIMEOUT);
		if (ret < 0) {
			::libUsbLogError(ret, "LAST_FRAME");

			if (ret == -19)		// -ENODEV
				return ret;

			return 0;
		}

		if ((status & LAST_FRAME) == LAST_FRAME)
			end = true;
	}

	return ret;
}

void CGMSKModemLibUsb::writeHeader(unsigned char* header, unsigned int length)
{
	assert(header != nullptr);
	assert(length >= (RADIO_HEADER_LENGTH_BYTES - 2U));

	io(0x40, SET_MyCALL2,  0, 0, (header + 35U), SHORT_CALLSIGN_LENGTH, USB_TIMEOUT);
	io(0x40, SET_MyCALL,   0, 0, (header + 27U), LONG_CALLSIGN_LENGTH,  USB_TIMEOUT);
	io(0x40, SET_YourCALL, 0, 0, (header + 19U), LONG_CALLSIGN_LENGTH,  USB_TIMEOUT);
	io(0x40, SET_RPT1CALL, 0, 0, (header + 11U), LONG_CALLSIGN_LENGTH,  USB_TIMEOUT);
	io(0x40, SET_RPT2CALL, 0, 0, (header + 3U),  LONG_CALLSIGN_LENGTH,  USB_TIMEOUT);
	io(0x40, SET_FLAGS,    0, 0, (header + 0U),  3U, USB_TIMEOUT);
}

TRISTATE CGMSKModemLibUsb::hasSpace()
{
	unsigned char space;
	int rc = io(0xC0, GET_REMAINSPACE, 0, 0, &space, 1, USB_TIMEOUT);
	if (rc != 1) {
		::libUsbLogError(rc, "GET_REMAINSPACE");
		return STATE_UNKNOWN;
	}

	if (space >= DV_FRAME_LENGTH_BYTES)
		return STATE_TRUE;
	else
		return STATE_FALSE;
}

TRISTATE CGMSKModemLibUsb::getPTT()
{
	unsigned char status;
	int rc = io(0xC0, GET_AD_STATUS, 0, 0, &status, 1, USB_TIMEOUT);
	if (rc < 1) {
		::libUsbLogError(rc, "GET_PTT");
		return STATE_UNKNOWN;
	}

	if ((status & PTT_OnOff) == PTT_OnOff)
		return STATE_TRUE;
	else
		return STATE_FALSE;
}

void CGMSKModemLibUsb::setPTT(bool on)
{
	unsigned char c;
	io(0x40, SET_PTT, on ? PTT_ON : PTT_OFF, 0, &c, 0, USB_TIMEOUT);
}

int CGMSKModemLibUsb::writeData(unsigned char* data, unsigned int length)
{
	assert(data != nullptr);
	assert(length > 0U && length <= DV_FRAME_LENGTH_BYTES);

	if (length > GMSK_MODEM_DATA_LENGTH) {
		int ret = io(0x40, PUT_DATA, 0, 0, data, GMSK_MODEM_DATA_LENGTH, USB_TIMEOUT);
		if (ret < 0) {
			if (ret == -19) {		// -ENODEV
				::libUsbLogError(ret, "PUT_DATA 1");
				return ret;
			}

			return 0;
		}

		// Give libUSB some recovery time
		std::this_thread::sleep_for(std::chrono::milliseconds(3));

		ret = io(0x40, PUT_DATA, 0, 0, (data + GMSK_MODEM_DATA_LENGTH), length - GMSK_MODEM_DATA_LENGTH, USB_TIMEOUT);
		if (ret < 0) {
			if (ret == -19) {		// -ENODEV
				::libUsbLogError(ret, "PUT_DATA 2");
				return ret;
			}

			return int(GMSK_MODEM_DATA_LENGTH);
		}

		return length;
	} else {
		int ret = io(0x40, PUT_DATA, 0, 0, data, length, USB_TIMEOUT);
		if (ret < 0) {
			if (ret == -19) {			// -ENODEV
				::libUsbLogError(ret, "PUT_DATA");
				return ret;
			}

			return 0;
		}

		return length;
	}
}

void CGMSKModemLibUsb::close()
{
	assert(m_dev != nullptr);

	libusb_close(m_dev);
	m_dev = nullptr;
}

bool CGMSKModemLibUsb::openModem()
{
	m_dev = ::libusb_open_device_with_vid_pid(m_context, VENDOR_ID, m_address);
	if (m_dev == nullptr)
		return false;

	::libusb_set_configuration(m_dev, 1);

	unsigned char c;
	io(0x40, SET_AD_INIT, 0, 0, &c, 0, USB_TIMEOUT);

	setPTT(false);

	return true;
}

int CGMSKModemLibUsb::io(uint8_t requestType, uint8_t request, uint16_t value,
                         uint16_t index, unsigned char* data, uint16_t length,
                         unsigned int timeout)
{
	assert(m_dev != nullptr);
	assert(data != nullptr);

	int ret = 0;
	for (unsigned int i = 0U; i < 4U; i++) {
		ret = ::libusb_control_transfer(m_dev, requestType, request, value, index, data, length, timeout);
		if (ret >= 0)
			return ret;

		if (ret == -19)		// ENODEV
			return ret;

		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	return ret;
}
