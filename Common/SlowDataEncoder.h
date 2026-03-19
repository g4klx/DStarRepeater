/*
 *	Copyright (C) 2010 by Jonathan Naylor, G4KLX
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

#ifndef	SlowDataEncoder_H
#define	SlowDataEncoder_H

#include "HeaderData.h"

#include <string>

/*
 * Encodes D-Star slow data into the 3-byte data field of each DV frame.
 *
 * Two independent payloads can be encoded:
 *   Header — a copy of the radio header, split across 8 tagged 6-byte blocks
 *            with a CCITT-16 checksum appended.  Used by the repeater to
 *            re-broadcast the header to late-joining receivers.
 *   Text   — a 20-character free-text message split across 4 tagged blocks.
 *
 * Each block is 6 bytes: 1-byte type/sequence tag followed by 5 payload bytes.
 * getHeaderData() and getTextData() each return 3 bytes (half a block) per
 * call, cycling back to the start after all blocks have been emitted.
 *
 * Before writing to the DV frame the 3 bytes are XOR-scrambled with the
 * repeating key SCRAMBLER_BYTE1/SCRAMBLER_BYTE2/SCRAMBLER_BYTE3.  sync()
 * resets the read pointer to the beginning of the sequence so that the
 * encoder is aligned with the data-sync frame position in the stream.
 */
class CSlowDataEncoder {
public:
	CSlowDataEncoder();
	~CSlowDataEncoder();

	void setHeaderData(const CHeaderData& header);
	void setTextData(const std::string& text);

	// Returns the next 3 scrambled bytes of the header slow-data sequence.
	void getHeaderData(unsigned char* data);
	// Returns the next 3 scrambled bytes of the text slow-data sequence.
	void getTextData(unsigned char* data);

	void reset();
	// Resets read pointers to the start of the sequence (call on each sync frame).
	void sync();

private:
	unsigned char* m_headerData;  // Pre-packed, unscrambled header slow-data bytes.
	unsigned char* m_textData;    // Pre-packed, unscrambled text slow-data bytes.
	unsigned int   m_headerPtr;   // Current read offset into m_headerData.
	unsigned int   m_textPtr;     // Current read offset into m_textData.
};

#endif
