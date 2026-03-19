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

#ifndef	DStarGMSKDemodulator_H
#define	DStarGMSKDemodulator_H

#include "DStarDefines.h"
#include "FIRFilter.h"
#include "Utils.h"

/*
 * GMSK demodulator for D-Star 4800 bps received audio.
 *
 * Processes audio samples one at a time via decode().  Internally maintains a
 * software PLL to track symbol timing, and a FIR low-pass filter to remove
 * out-of-band noise before bit decisions are made.
 *
 * Returns TRISTATE:
 *   STATE_TRUE    — a '1' bit was clocked out at this sample
 *   STATE_FALSE   — a '0' bit was clocked out at this sample
 *   STATE_UNKNOWN — mid-symbol; no bit decision yet
 *
 * lock() can be used to freeze PLL adaptation once a stable signal is detected,
 * preventing the PLL from wandering during long runs of the same bit value.
 */
class CDStarGMSKDemodulator {
public:
	CDStarGMSKDemodulator();
	~CDStarGMSKDemodulator();

	TRISTATE decode(float val);

	void setInvert(bool set);

	void reset();

	// Freezes (on=true) or enables (on=false) PLL frequency adaptation.
	void lock(bool on);

private:
	CFIRFilter   m_filter;
	bool         m_invert;
	unsigned int m_pll;      // Current PLL phase accumulator.
	bool         m_prev;     // Previous bit value, used for GMSK differential decoding.
	unsigned int m_inc;      // PLL phase increment per sample.
	bool         m_locked;
	float        m_offset;
	float        m_accum;
	unsigned int m_count;
};

#endif
