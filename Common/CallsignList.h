/*
 *   Copyright (C) 2011,2013 by Jonathan Naylor G4KLX
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

#ifndef	CallsignList_H
#define	CallsignList_H

#include "StdCompat.h"

/*
 * Loads a callsign access list (whitelist, blacklist, or greylist) from a
 * plain-text file, one callsign per line.
 *
 * Each entry is normalised on load: converted to uppercase and padded/truncated
 * to exactly LONG_CALLSIGN_LENGTH (8) characters with trailing spaces.  This
 * matches the fixed-width format used in D-Star radio headers, so isInList()
 * can do a simple string comparison without any normalisation at call time.
 */
class CCallsignList {
public:
	CCallsignList(const std::string& filename);
	~CCallsignList();

	bool load();

	unsigned int getCount() const;

	// Returns true if callsign (already padded to 8 chars) is in the list.
	bool isInList(const std::string& callsign) const;

private:
	std::string              m_filename;
	std::vector<std::string> m_callsigns;
};

#endif
