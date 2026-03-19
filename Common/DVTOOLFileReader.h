/*
 *   Copyright (C) 2009 by Jonathan Naylor G4KLX
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

#ifndef	DVTOOLFileReader_H
#define DVTOOLFileReader_H

#include "HeaderData.h"

/*
 * .dvtool file format (compatible with the DVTool PC application):
 *
 *   [0..5]   "DVTOOL"                    — 6-byte magic signature
 *   [6..9]   record count (big-endian)   — total number of DSVT records
 *
 *   Each record:
 *     [0..1]   total record length (little-endian, includes 15-byte fixed header)
 *     [2..5]   "DSVT"                    — record signature
 *     [6]      0x10 = header frame, 0x20 = data/trailer frame
 *     [7..15]  fixed 9-byte field (stream flags, sequence info)
 *     [16]     flags: bit 7 = header record, bit 6 = trailer (end-of-stream)
 *     [17..]   payload (radio header bytes, or DV frame data)
 */

enum DVTFR_TYPE {
	DVTFR_NONE,    // No record read yet, or end of file.
	DVTFR_HEADER,  // Current record contains a radio header.
	DVTFR_DATA     // Current record contains a DV voice/data frame.
};

#include "StdCompat.h"
#include <cstdio>

/*
 * Sequential reader for .dvtool recording files.
 *
 * Call open() to validate the signature and read the record count, then loop
 * calling read() to advance to each record, followed by readHeader() or
 * readData() to extract the payload.  close() releases the file handle.
 */
class CDVTOOLFileReader {
public:
	CDVTOOLFileReader();
	~CDVTOOLFileReader();

	std::string  getFileName() const;
	unsigned int getRecords() const;

	bool         open(const std::string& fileName);
	// Advances to the next record; returns its type or DVTFR_NONE at EOF.
	DVTFR_TYPE   read();
	// Returns a newly allocated CHeaderData; caller takes ownership.
	CHeaderData* readHeader();
	// Copies DV frame payload into buffer; sets end=true on the trailer record.
	unsigned int readData(unsigned char* buffer, unsigned int length, bool& end);
	void         close();

private:
	std::string    m_fileName;
	FILE*          m_file;
	uint32_t       m_records;   // Total record count from the file header.
	DVTFR_TYPE     m_type;
	unsigned char* m_buffer;
	unsigned int   m_length;    // Payload length of the current record.
	bool           m_end;       // True if the trailer bit was set in the current record.
};

#endif
