/*
 *   Copyright (C) 2011 by Jonathan Naylor G4KLX
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

#include "CallsignList.h"
#include "DStarDefines.h"

#include <fstream>
#include <algorithm>

CCallsignList::CCallsignList(const std::string& filename) :
m_filename(filename),
m_callsigns()
{
}


CCallsignList::~CCallsignList()
{
}

bool CCallsignList::load()
{
	std::ifstream file(m_filename);
	if (!file.is_open())
		return false;

	std::string callsign;
	while (std::getline(file, callsign)) {
		// Strip trailing CR so CRLF files (Windows line endings) are handled correctly
		if (!callsign.empty() && callsign.back() == '\r')
			callsign.pop_back();

		// Convert to uppercase
		std::transform(callsign.begin(), callsign.end(), callsign.begin(), ::toupper);

		// Pad to LONG_CALLSIGN_LENGTH with spaces, then truncate
		callsign.append(8U, ' ');
		callsign.resize(LONG_CALLSIGN_LENGTH);

		m_callsigns.push_back(callsign);
	}

	file.close();

	return true;
}

unsigned int CCallsignList::getCount() const
{
	return m_callsigns.size();
}

bool CCallsignList::isInList(const std::string& callsign) const
{
	for (const auto& cs : m_callsigns) {
		if (cs == callsign)
			return true;
	}
	return false;
}
