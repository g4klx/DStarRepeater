/*
 *   Copyright (C) 2011,2012 by Jonathan Naylor G4KLX
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

#ifndef	BeaconUnit_H
#define	BeaconUnit_H

#include "SlowDataEncoder.h"
#include "BeaconCallback.h"
#include "DStarDefines.h"

#include "StdCompat.h"
#include <chrono>
#include <unordered_map>

/*
 * Maps a word or single character (e.g. "alpha", "B", " ") to a slice of the
 * pre-recorded AMBE audio buffer.  The index file (.indx) contains one entry
 * per line: <name> <start_frame> <frame_count>.
 */
class CIndexRecord {
public:
	CIndexRecord(const std::string& name, unsigned int start, unsigned int length) :
	m_name(name),
	m_start(start),
	m_length(length)
	{
	}

	std::string getName() const
	{
		return m_name;
	}

	unsigned int getStart() const
	{
		return m_start;
	}

	unsigned int getLength() const
	{
		return m_length;
	}

private:
	std::string  m_name;
	unsigned int m_start;   // Frame offset into the AMBE buffer.
	unsigned int m_length;  // Number of AMBE frames for this word/character.
};

typedef std::unordered_map<std::string, CIndexRecord*> CIndexList_t;

/*
 * Generates a voice ID beacon by assembling pre-recorded AMBE audio frames.
 *
 * On construction the .ambe file (raw VOICE_FRAME_LENGTH_BYTES chunks) and
 * its companion .indx file are loaded for the configured language.  When
 * sendBeacon() is called, spellCallsign() looks up each character of the
 * callsign in the index and copies the corresponding AMBE frames into an
 * internal playback buffer.  The last character is spoken phonetically
 * (e.g. "A" -> "alpha") to follow amateur radio convention.
 *
 * clock() is called every DSTAR_FRAME_TIME_MS (20 ms) by the repeater thread.
 * It compares elapsed wall-clock time against m_out (frames dispatched) to
 * pace frame delivery to the transmitter without requiring a dedicated thread.
 *
 * If no voice files are available, a silent beacon (null AMBE frames) with
 * slow-data text is still transmitted.
 */
class CBeaconUnit {
public:
	CBeaconUnit(IBeaconCallback* handler, const std::string& callsign, const std::string& text, bool voice, TEXT_LANG language);
	~CBeaconUnit();

	// Assembles the beacon frames and signals the repeater thread to begin TX.
	void sendBeacon();

	// Called every repeater tick; releases frames to the transmitter at air rate.
	void clock();

private:
	unsigned char*   m_ambe;        // Raw AMBE frames loaded from the .ambe file.
	unsigned int     m_ambeLength;  // Total frames available in m_ambe.
	unsigned char*   m_data;        // Assembled DV frames ready for transmission.
	unsigned int     m_dataLength;  // Bytes written into m_data so far.
	CIndexList_t     m_index;       // Word/character -> AMBE frame mapping.
	TEXT_LANG        m_language;
	IBeaconCallback* m_handler;
	std::string      m_callsign;
	CSlowDataEncoder m_encoder;     // Encodes the beacon text into slow-data fields.
	unsigned int     m_in;          // Total frames queued into m_data.
	unsigned int     m_out;         // Frames dispatched to the transmitter so far.
	unsigned int     m_seqNo;       // Slow-data sequence counter (0–20).
	std::chrono::steady_clock::time_point m_time;  // Wall-clock start time of the beacon.
	bool             m_sending;

	// Appends AMBE frames for a word/character from the index into m_data.
	bool lookup(const std::string& id);
	// Spells out the callsign character by character; the last character uses
	// the phonetic alphabet (A→alpha, B→bravo, C→charlie, D→delta).
	void spellCallsign(const std::string& callsign);

	bool readAMBE(const std::string& name);
	bool readIndex(const std::string& name);
};

#endif
