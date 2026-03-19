/*
 *   Copyright (C) 2013 by Jonathan Naylor G4KLX
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

#ifndef	AnnouncementUnit_H
#define	AnnouncementUnit_H

#include "AnnouncementCallback.h"
#include "DVTOOLFileWriter.h"
#include "DVTOOLFileReader.h"
#include "DStarDefines.h"
#include "HeaderData.h"

#include "StdCompat.h"
#include <chrono>

/*
 * Records and plays back user announcements stored as .dvtool files.
 *
 * Lifecycle:
 *   Record  — The repeater thread calls writeHeader() then writeData() as
 *             audio arrives over RF.  The stream is saved to a per-callsign
 *             file ("Announce_<callsign>.dvtool") in the user's home directory.
 *
 *   Playback — startAnnouncement() opens the callsign-specific file if it
 *              exists, falling back to the global "Announce.dvtool".  It reads
 *              the header and signals the repeater thread to key up.  clock()
 *              then releases DV frames at air rate (same wall-clock pacing as
 *              CBeaconUnit) until the end-of-transmission marker is reached.
 *
 *   Delete  — deleteAnnouncement() removes the callsign-specific file so the
 *             global announcement (or silence) takes effect on the next play.
 */
class CAnnouncementUnit {
public:
	CAnnouncementUnit(IAnnouncementCallback* handler, const std::string& callsign);
	~CAnnouncementUnit();

	bool writeHeader(const CHeaderData& header);
	bool writeData(const unsigned char* data, unsigned int length, bool end);

	void deleteAnnouncement();

	void startAnnouncement();

	// Called every repeater tick; releases frames to the transmitter at air rate.
	void clock();

private:
	IAnnouncementCallback* m_handler;
	std::string            m_localFileName;  // Callsign-specific filename (no extension).
	CDVTOOLFileReader      m_reader;
	CDVTOOLFileWriter      m_writer;
	std::chrono::steady_clock::time_point m_time;
	unsigned int           m_out;      // Frames dispatched so far during playback.
	bool                   m_sending;
};

#endif
