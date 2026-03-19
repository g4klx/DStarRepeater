/*
 *	Copyright (C) 2009,2010 by Jonathan Naylor, G4KLX
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 */

#include "K8055Controller.h"

#include <cassert>
#include <cstdio>

#if !defined(_WIN32)

const unsigned int VELLEMAN_VENDOR_ID  = 0x10CFU;
const unsigned int VELLEMAN_PRODUCT_ID = 0x5500U;

const char REPORT_ID              = 0x00U;

const char CMD_RESET              = 0x00U;
const char CMD_SET_ANALOG_DIGITAL = 0x05U;

const char IN_PORT1 = 0x10U;
const char IN_PORT2 = 0x20U;
const char IN_PORT3 = 0x01U;
const char IN_PORT4 = 0x40U;
const char IN_PORT5 = 0x80U;

const char OUT_PORT1 = 0x01U;
const char OUT_PORT2 = 0x02U;
const char OUT_PORT3 = 0x04U;
const char OUT_PORT4 = 0x08U;
const char OUT_PORT5 = 0x10U;
const char OUT_PORT6 = 0x20U;
const char OUT_PORT7 = 0x40U;
const char OUT_PORT8 = 0x80U;

const unsigned int VELLEMAN_HID_INTERFACE = 0U;

const unsigned int USB_OUTPUT_ENDPOINT = 0x01U;
const unsigned int USB_INPUT_ENDPOINT  = 0x81U;

const int USB_BUFSIZE = 8;

const int USB_TIMEOUT = 5000;

CK8055Controller::CK8055Controller(unsigned int address) :
m_address(address),
m_outp1(false),
m_outp2(false),
m_outp3(false),
m_outp4(false),
m_outp5(false),
m_outp6(false),
m_outp7(false),
m_outp8(false),
m_context(nullptr),
m_handle(nullptr)
{
	::libusb_init(&m_context);
}

CK8055Controller::~CK8055Controller()
{
	assert(m_context != nullptr);

	::libusb_exit(m_context);
}

bool CK8055Controller::open()
{
	assert(m_context != nullptr);
	assert(m_handle == nullptr);

	m_handle = ::libusb_open_device_with_vid_pid(m_context, VELLEMAN_VENDOR_ID, VELLEMAN_PRODUCT_ID + m_address);
	if (m_handle == nullptr) {
		::fprintf(stderr, "Could not open the Velleman K8055\n");
		return false;
	}

	int res = ::libusb_claim_interface(m_handle, VELLEMAN_HID_INTERFACE);
	if (res != 0) {
		res = ::libusb_detach_kernel_driver(m_handle, VELLEMAN_HID_INTERFACE);
		if (res != 0) {
			::fprintf(stderr, "Error from libusb_detach_kernel_driver: err=%d\n", res);
			::libusb_close(m_handle);
			m_handle = nullptr;
			return false;
		}

		res = ::libusb_claim_interface(m_handle, VELLEMAN_HID_INTERFACE);
		if (res != 0) {
			::fprintf(stderr, "Error from libusb_claim_interface: err=%d\n", res);
			::libusb_close(m_handle);
			m_handle = nullptr;
			return false;
		}
	}

	::libusb_set_configuration(m_handle, 1);

	setDigitalOutputs(false, false, false, false, false, false, false, false);

	return true;
}

void CK8055Controller::getDigitalInputs(bool& inp1, bool& inp2, bool& inp3, bool& inp4, bool& inp5)
{
	assert(m_handle != nullptr);

	unsigned char buffer[USB_BUFSIZE];
	buffer[0] = 0x00;
	buffer[1] = 0x00;
	buffer[2] = 0x00;
	buffer[3] = 0x00;
	buffer[4] = 0x00;
	buffer[5] = 0x00;
	buffer[6] = 0x00;
	buffer[7] = 0x00;

	int written;
	int res = ::libusb_interrupt_transfer(m_handle, USB_INPUT_ENDPOINT, buffer, USB_BUFSIZE, &written, USB_TIMEOUT);
	if (res != 0) {
		::fprintf(stderr, "Error from libusb_interrupt_transfer: err=%d\n", res);
		return;
	}

	if (written != USB_BUFSIZE)
		return;

	// Do we have data?
	if (buffer[1] != 0x00) {
		inp1 = (buffer[0] & IN_PORT1) == IN_PORT1;
		inp2 = (buffer[0] & IN_PORT2) == IN_PORT2;
		inp3 = (buffer[0] & IN_PORT3) == IN_PORT3;
		inp4 = (buffer[0] & IN_PORT4) == IN_PORT4;
		inp5 = (buffer[0] & IN_PORT5) == IN_PORT5;
	}
}

void CK8055Controller::setDigitalOutputs(bool outp1, bool outp2, bool outp3, bool outp4, bool outp5, bool outp6, bool outp7, bool outp8)
{
	assert(m_handle != nullptr);

	if (outp1 == m_outp1 && outp2 == m_outp2 && outp3 == m_outp3 && outp4 == m_outp4 &&
		outp5 == m_outp5 && outp6 == m_outp6 && outp7 == m_outp7 && outp8 == m_outp8)
		return;

	unsigned char buffer[USB_BUFSIZE];
	buffer[0] = CMD_SET_ANALOG_DIGITAL;
	buffer[1] = 0x00;
	buffer[2] = 0x00;
	buffer[3] = 0x00;
	buffer[4] = 0x00;
	buffer[5] = 0x00;
	buffer[6] = 0x00;
	buffer[7] = 0x00;

	if (outp1)
		buffer[1] |= OUT_PORT1;
	if (outp2)
		buffer[1] |= OUT_PORT2;
	if (outp3)
		buffer[1] |= OUT_PORT3;
	if (outp4)
		buffer[1] |= OUT_PORT4;
	if (outp5)
		buffer[1] |= OUT_PORT5;
	if (outp6)
		buffer[1] |= OUT_PORT6;
	if (outp7)
		buffer[1] |= OUT_PORT7;
	if (outp8)
		buffer[1] |= OUT_PORT8;

	int written;
	int res = ::libusb_interrupt_transfer(m_handle, USB_OUTPUT_ENDPOINT, buffer, USB_BUFSIZE, &written, USB_TIMEOUT);
	if (res != 0) {
		::fprintf(stderr, "Error from libusb_interrupt_transfer: err=%d\n", res);
		return;
	}

	if (written != USB_BUFSIZE)
		return;

	m_outp1 = outp1;
	m_outp2 = outp2;
	m_outp3 = outp3;
	m_outp4 = outp4;
	m_outp5 = outp5;
	m_outp6 = outp6;
	m_outp7 = outp7;
	m_outp8 = outp8;
}

void CK8055Controller::close()
{
	assert(m_handle != nullptr);

	setDigitalOutputs(false, false, false, false, false, false, false, false);

	::libusb_release_interface(m_handle, VELLEMAN_HID_INTERFACE);

	::libusb_close(m_handle);
	m_handle = nullptr;
}

#else

// Windows stub implementation

CK8055Controller::CK8055Controller(unsigned int address) :
m_address(address)
{
}

CK8055Controller::~CK8055Controller()
{
}

bool CK8055Controller::open()
{
	::fprintf(stderr, "K8055 not supported on Windows without Velleman DLL\n");
	return false;
}

void CK8055Controller::getDigitalInputs(bool& inp1, bool& inp2, bool& inp3, bool& inp4, bool& inp5)
{
	inp1 = inp2 = inp3 = inp4 = inp5 = false;
}

void CK8055Controller::setDigitalOutputs(bool, bool, bool, bool, bool, bool, bool, bool)
{
}

void CK8055Controller::close()
{
}

#endif
