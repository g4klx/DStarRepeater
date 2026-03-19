/*
 *	Copyright (C) 2009,2010 by Jonathan Naylor, G4KLX
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

#ifndef	SlowDataDecoder_H
#define	SlowDataDecoder_H

#include "HeaderData.h"

/*
 * Decodes D-Star slow data embedded in the 3-byte data field of each DV frame.
 *
 * D-Star slow data is multiplexed with voice at 1200 bps by stealing 3 bytes
 * from every 12-byte DV frame.  These 3 bytes are XOR-scrambled before
 * transmission using a repeating 3-byte key (SCRAMBLER_BYTE1/2/3 from
 * DStarDefines.h) to keep the bit pattern from looking like sync patterns.
 *
 * Two consecutive 3-byte slow-data fields are collected into a 6-byte block.
 * The first byte of each block is a type/length tag; the remaining 5 bytes
 * carry payload.  Blocks can carry a re-transmitted copy of the radio header
 * (type SLOW_DATA_TYPE_HEADER) or free-text (type SLOW_DATA_TYPE_TEXT).
 *
 * addData() is called once per DV frame with the 3-byte slow-data field (after
 * stripping the AMBE bytes).  It alternates between SDD_FIRST and SDD_SECOND
 * states to accumulate complete 6-byte blocks.  When a valid header checksum
 * is found, the result is available via getHeaderData().
 */

enum SDD_STATE {
	SDD_FIRST,   // Waiting for the first 3-byte half of a slow-data block.
	SDD_SECOND   // Waiting for the second half to complete the block.
};

class CSlowDataDecoder {
public:
	CSlowDataDecoder();
	~CSlowDataDecoder();

	// Feed one 3-byte slow-data field (raw, still scrambled).
	void addData(const unsigned char* data);

	// Returns a newly allocated CHeaderData if a valid header was decoded,
	// or nullptr if none has been found yet.  Caller takes ownership.
	CHeaderData* getHeaderData();

	// Called on each data-sync frame to re-align the two-phase accumulator.
	void sync();
	void reset();

private:
	unsigned char* m_buffer;      // Accumulates the current 6-byte block.
	SDD_STATE      m_state;
	unsigned char* m_headerData;  // Circular buffer of decoded payload bytes.
	unsigned int   m_headerPtr;
	CHeaderData*   m_header;      // Decoded header, held until getHeaderData() is called.

	void processHeader();
	bool processHeader(const unsigned char* bytes);
};

#endif
