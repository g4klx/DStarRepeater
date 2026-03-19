/*
 *	Copyright (C) 2009,2011 by Jonathan Naylor, G4KLX
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

#ifndef	URIUSBController_H
#define	URIUSBController_H

#include "HardwareController.h"

#if !defined(_WIN32)

#include <libusb-1.0/libusb.h>

class CURIUSBController : public IHardwareController {
public:
	CURIUSBController(unsigned int address, bool checkInput);
	virtual ~CURIUSBController();

	virtual bool open();

	virtual void getDigitalInputs(bool& inp1, bool& inp2, bool& inp3, bool& inp4, bool& inp5);

	virtual void setDigitalOutputs(bool outp1, bool outp2, bool outp3, bool outp4, bool outp5, bool outp6, bool outp7, bool outp8);

	virtual void close();

private:
	unsigned int          m_address;
	bool                  m_checkInput;
	bool                  m_outp1;
	bool                  m_outp3;
	bool                  m_outp5;
	bool                  m_outp6;
	libusb_context*       m_context;
	libusb_device_handle* m_handle;
};

#else

// Windows stub — URI USB (CM108-based) controller requires libusb which is not
// available in this build configuration.  open() will log an error and return false.

class CURIUSBController : public IHardwareController {
public:
	CURIUSBController(unsigned int address, bool checkInput);
	virtual ~CURIUSBController();

	virtual bool open();

	virtual void getDigitalInputs(bool& inp1, bool& inp2, bool& inp3, bool& inp4, bool& inp5);

	virtual void setDigitalOutputs(bool outp1, bool outp2, bool outp3, bool outp4, bool outp5, bool outp6, bool outp7, bool outp8);

	virtual void close();

private:
	unsigned int m_address;
	bool         m_checkInput;
};

#endif

#endif
