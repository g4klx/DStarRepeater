/*
 *   Copyright (C) 2010,2014 by Jonathan Naylor G4KLX
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

#ifndef	AMBEFEC_H
#define	AMBEFEC_H

#include "Golay.h"

/*
 * Forward error correction for AMBE codec frames.
 *
 * D-Star uses AMBE+2 (72-bit frames) with a Golay(23,12) FEC scheme applied
 * to the most significant bits of each frame.  regenerate() attempts to repair
 * bit errors in the three Golay codewords that make up one AMBE frame, and
 * returns the number of errors that were corrected.  count() returns the number
 * of uncorrectable errors without modifying the data.
 */
class CAMBEFEC {
public:
	CAMBEFEC();
	~CAMBEFEC();

	// Attempts in-place error correction; returns number of errors corrected.
	unsigned int regenerate(unsigned char* bytes) const;
	// Counts bit errors without modifying the data.
	unsigned int count(const unsigned char* bytes) const;

private:
	unsigned int regenerate(unsigned int& a, unsigned int& b, unsigned int& c) const;
	unsigned int count(unsigned int a, unsigned int b, unsigned int c) const;
};

#endif
