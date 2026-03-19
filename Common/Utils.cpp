/*
 *	Copyright (C) 2009,2014 Jonathan Naylor, G4KLX
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

#include "Utils.h"

#include <cassert>
#include <cctype>
#include <cstdio>
#include <string>

void CUtils::dump(const char* title, const unsigned char* data, unsigned int length)
{
	assert(title != nullptr);
	assert(data != nullptr);

	::fprintf(stdout, "%s\n", title);

	unsigned int offset = 0U;

	while (length > 0U) {
		std::string output;

		unsigned int bytes = (length > 16U) ? 16U : length;

		char temp[8];
		for (unsigned i = 0U; i < bytes; i++) {
			::snprintf(temp, sizeof(temp), "%02X ", data[offset + i]);
			output += temp;
		}

		for (unsigned int i = bytes; i < 16U; i++)
			output += "   ";

		output += "   *";

		for (unsigned i = 0U; i < bytes; i++) {
			unsigned char c = data[offset + i];

			if (::isprint(c))
				output += char(c);
			else
				output += '.';
		}

		output += '*';

		::fprintf(stdout, "%04X:  %s\n", offset, output.c_str());

		offset += 16U;

		if (length >= 16U)
			length -= 16U;
		else
			length = 0U;
	}
}
