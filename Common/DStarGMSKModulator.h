/*
 *	Copyright (C) 2009,2013 by Jonathan Naylor, G4KLX
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

#ifndef	DStarGMSKModulator_H
#define	DStarGMSKModulator_H

#include "FIRFilter.h"

/*
 * GMSK modulator for D-Star 4800 bps transmissions.
 *
 * Converts a single NRZ bit into a sequence of float audio samples using a
 * Gaussian FIR pulse-shaping filter (BT=0.5).  The output sample buffer must
 * be pre-allocated by the caller; code() returns the number of samples written.
 * setInvert() reverses the polarity of the modulated signal for hardware that
 * requires the opposite sense.
 */
class CDStarGMSKModulator {
public:
	CDStarGMSKModulator();
	~CDStarGMSKModulator();

	unsigned int code(bool bit, float* buffer, unsigned int length);

	void setInvert(bool set);

private:
	CFIRFilter m_filter;
	bool       m_invert;
};

#endif
