/*
 *   Copyright (C) 2010 by Jonathan Naylor G4KLX
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

#ifndef Golay_H
#define Golay_H

/*
 * Golay error-correcting code used by the AMBE FEC layer.
 *
 * Provides two variants:
 *   Golay(23,12) — encodes 12 data bits into 23 bits; corrects up to 3 errors.
 *   Golay(24,12) — adds a parity bit to the above for even-parity detection.
 *
 * encode*() returns the full codeword (data + parity bits).
 * decode*() returns the corrected 12-bit data word, or ~0U on uncorrectable error.
 */
class CGolay {
public:
	static unsigned int encode23127(unsigned int data);
	static unsigned int encode24128(unsigned int data);

	static unsigned int decode23127(unsigned int code);
	static unsigned int decode24128(unsigned int code);
};

#endif
