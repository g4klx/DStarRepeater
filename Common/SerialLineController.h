/*
 *   Copyright (C) 2002-2004,2007-2009,2011-2013,2015 by Jonathan Naylor G4KLX
 *   Copyright (C) 1999-2001 by Thomas Sailor HB9JNX
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

#ifndef SerialLineController_H
#define SerialLineController_H

#include "HardwareController.h"
#include "StdCompat.h"

#if defined(_WIN32)
#include <windows.h>
#endif

// Serial modem control lines used for PTT / COR signalling.
enum SERIALPIN {
	SERIAL_CD,        // Carrier Detect — input, indicates received signal.
	SERIAL_CTS,       // Clear To Send — input.
	SERIAL_DSR,       // Data Set Ready — input.
	SERIAL_DTR,       // Data Terminal Ready — output, used for PTT on some hardware.
	SERIAL_RTS,       // Request To Send — output, most common PTT line.
	SERIAL_ECHOLINK   // EchoLink-compatible pin assignment variant.
};

const unsigned int MAX_DEVICE_NAME = 255U;

/*
 * Serial port line-level controller for PTT and COR/COS I/O.
 *
 * Implements IHardwareController using the modem-control lines of an RS-232
 * (or USB-serial) port.  RTS and/or DTR are used as PTT outputs; CD, CTS,
 * and DSR are used as COR/squelch inputs.  The config parameter selects which
 * pin assignment scheme is used (see the SERIALPIN enum and the controller
 * config documentation).
 *
 * This controller does not transfer any data — it only toggles the control
 * lines.  For serial data transfer use CSerialDataController.
 */
class CSerialLineController : public IHardwareController {
public:
	CSerialLineController(const std::string& device, unsigned int config = 1U);
	virtual ~CSerialLineController();

	virtual bool open();

	virtual bool setRTS(bool set);
	virtual bool setDTR(bool set);

	virtual bool getCD() const;
	virtual bool getCTS() const;
	virtual bool getDSR() const;

	virtual void getDigitalInputs(bool& inp1, bool& inp2, bool& inp3, bool& inp4, bool& inp5);
	virtual void setDigitalOutputs(bool outp1, bool outp2, bool outp3, bool outp4, bool outp5, bool outp6, bool outp7, bool outp8);

	virtual void close();

private:
	std::string  m_device;
	unsigned int m_config;   // Pin assignment scheme index.
	bool         m_rts;
	bool         m_dtr;
#if defined(_WIN32)
	HANDLE       m_handle;
#else
	int          m_fd;
#endif
};

#endif
