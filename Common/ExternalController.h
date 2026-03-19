/*
 *	Copyright (C) 2009,2010,2013,2014 by Jonathan Naylor, G4KLX
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

/*
 * CExternalController wraps an IHardwareController (serial port, Arduino,
 * GPIO board, etc.) and drives it from a dedicated background thread.
 *
 * The repeater thread calls the set* methods to express the desired output
 * state (PTT, heartbeat LED, active indicator, auxiliary outputs).  The
 * internal thread polls the hardware at half the D-Star frame rate (every
 * 10 ms) and applies those values to the physical I/O pins.  Input pins
 * (the "disable" line) are read on the same schedule and stored atomically
 * so the repeater thread can check them without blocking.
 *
 * PTT inversion
 * -------------
 * Some hardware asserts PTT by pulling a line low rather than high.  When
 * pttInvert is true the radioTX sense is flipped before being written to the
 * hardware, and the idle state is initialised to the inverted (asserted) value
 * so the line is de-asserted once the thread starts.
 *
 * Shutdown sequence
 * -----------------
 * close() sets m_kill and joins the thread.  The thread then de-asserts PTT,
 * waits three frame periods (60 ms) to let the transmitter drop, asserts the
 * final idle state once more, then calls m_controller->close().
 */

#ifndef	ExternalController_H
#define	ExternalController_H

#include "HardwareController.h"
#include "StdCompat.h"

#include <atomic>
#include <thread>

class CExternalController {
public:
	CExternalController(IHardwareController* controller, bool pttInvert);
	virtual ~CExternalController();

	// Open the hardware device and start the polling thread.
	virtual bool open();

	// True when the hardware disable input is asserted (repeater should not TX).
	virtual bool getDisable() const;

	// Set the desired PTT state; inverted before applying if pttInvert is set.
	virtual void setRadioTransmit(bool value);
	// Toggle the heartbeat output each call (caller drives the toggle rate).
	virtual void setHeartbeat();
	// Assert or de-assert the "active" status output.
	virtual void setActive(bool value);
	virtual void setOutput1(bool value);
	virtual void setOutput2(bool value);
	virtual void setOutput3(bool value);
	virtual void setOutput4(bool value);

	// Signal the polling thread to stop, then close the hardware device.
	virtual void close();

private:
	void entry();  // Polling thread body

	IHardwareController* m_controller;   // Concrete hardware driver (owned)
	bool                 m_pttInvert;    // True if PTT sense is active-low

	// Desired output states written by the repeater thread and read by entry().
	// All are atomic so no mutex is needed for the shared flag pattern.
	std::atomic<bool>    m_disable;    // Latched from the hardware disable input
	std::atomic<bool>    m_heartbeat;  // Heartbeat/LED toggle
	std::atomic<bool>    m_active;     // Repeater-active indicator
	std::atomic<bool>    m_radioTX;    // PTT output (post-inversion)
	std::atomic<bool>    m_out1;       // Auxiliary outputs 1–4
	std::atomic<bool>    m_out2;
	std::atomic<bool>    m_out3;
	std::atomic<bool>    m_out4;
	std::atomic<bool>    m_kill;       // Set by close() to stop the poll loop
	std::thread          m_thread;
};

#endif
