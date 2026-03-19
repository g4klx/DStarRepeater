/*
 *	Copyright (C) 2009,2011 by Jonathan Naylor, G4KLX
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

#ifndef	CCITTChecksum_H
#define	CCITTChecksum_H

#include <cstdint>

/*
 * CCITT-16 (CRC-16/CCITT) checksum in standard bit order.
 *
 * update() feeds bytes into the running CRC.  result() writes the two-byte
 * checksum into the provided buffer.  check() compares a stored two-byte
 * checksum against the computed value.  reset() resets to the initial state.
 *
 * See also CCCITTChecksumReverse, which uses a bit-reversed variant required
 * by the D-Star radio header and DSRP wire formats.
 */
class CCCITTChecksum {
public:
	CCCITTChecksum();
	~CCCITTChecksum();

	void update(const unsigned char* data, unsigned int length);

	void result(unsigned char* data);

	bool check(const unsigned char* data);

	void reset();

private:
	union {
		uint16_t m_crc16;
		uint8_t  m_crc8[2U];
	};
};

#endif
