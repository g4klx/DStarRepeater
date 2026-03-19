/*
 *	Copyright (C) 2009,2014 by Jonathan Naylor, G4KLX
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

#ifndef	Utils_H
#define	Utils_H

#include "StdCompat.h"

enum TRISTATE {
	STATE_FALSE,
	STATE_TRUE,
	STATE_UNKNOWN
};

/*
 * General utility class.
 *
 * Currently contains only dump(), a hex-dump helper used throughout the
 * codebase to print raw protocol bytes to stdout for debugging.  Output
 * format is classic hex+ASCII: 16 bytes per line, address on the left,
 * printable characters on the right.
 */
class CUtils {
public:
	static void dump(const char* title, const unsigned char* data, unsigned int length);

private:
};

#endif
