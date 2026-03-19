/*
 *   Copyright (C) 2009,2013 by Jonathan Naylor G4KLX
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

#ifndef	DVTOOLFileWriter_H
#define DVTOOLFileWriter_H

#include "HeaderData.h"

#include "StdCompat.h"
#include <cstdio>
#include <cstdint>

/*
 * Sequential writer for .dvtool recording files (see DVTOOLFileReader.h for
 * the file format description).
 *
 * Usage:
 *   open()  — writes the "DVTOOL" signature and a placeholder record count,
 *              then writes the header record.
 *   write() — appends each DV data frame as a DSVT data record.
 *   close() — writes the trailer record, then seeks back to the placeholder
 *             and fills in the final record count (big-endian uint32).
 *
 * Two open() overloads are provided:
 *   open(filename, header) — uses a caller-supplied base filename.
 *   open(header)           — auto-generates a filename from a timestamp and
 *                            the callsigns embedded in the header.
 *
 * setDirectory() is a class-level (static) setting that prepends a directory
 * to all generated filenames.
 */
class CDVTOOLFileWriter {
public:
	CDVTOOLFileWriter();
	~CDVTOOLFileWriter();

	// Sets the output directory used by all CDVTOOLFileWriter instances.
	static void setDirectory(const std::string& dirName);

	std::string getFileName() const;

	bool open(const CHeaderData& header);
	bool open(const std::string& filename, const CHeaderData& header);
	bool write(const unsigned char* buffer, unsigned int length);
	void close();

private:
	static std::string m_dirName;

	std::string  m_fileName;
	FILE*        m_file;
	uint32_t     m_count;      // Number of DSVT records written (header + data + trailer).
	unsigned int m_sequence;   // Per-frame sequence counter (0–20, mirrors DSRP wire format).
	long         m_offset;     // File offset of the record-count field, for back-patching in close().

	bool writeHeader(const CHeaderData& header);
	bool writeTrailer();
};

#endif
