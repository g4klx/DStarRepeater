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

#ifndef	HeaderData_H
#define	HeaderData_H

#include <ctime>
#include <string>

/*
 * Represents a D-Star radio header — the block transmitted before every voice
 * or data transmission that identifies the participants and routing.
 *
 * Wire format (41 bytes, all callsigns space-padded to fixed width):
 *   [0]      Flag 1   — repeater/data/control/urgent/BK/EMR bits
 *   [1]      Flag 2   — reserved (always 0x00 in practice)
 *   [2]      Flag 3   — reserved (always 0x00 in practice)
 *   [3..10]  RPT2     — outgoing repeater callsign (8 chars, LONG_CALLSIGN_LENGTH)
 *   [11..18] RPT1     — incoming repeater callsign (8 chars)
 *   [19..26] YOUR     — destination callsign or "CQCQCQ  " for a general call
 *   [27..34] MY1      — originating station callsign (8 chars)
 *   [35..38] MY2      — 4-character callsign suffix (SHORT_CALLSIGN_LENGTH)
 *   [39..40] Checksum — CCITT-16 (reversed bit order) over bytes [0..38]
 *
 * The checksum is computed by CCCITTChecksumReverse.  A stored checksum of
 * 0xFFFF is treated as "unchecked" and is accepted without verification; this
 * is used when the header arrives from a trusted local source.
 *
 * Flag 1 bit meanings (see DStarDefines.h for mask constants):
 *   Bit 6  Repeater mode  — set when relayed through a repeater
 *   Bit 5  Data packet    — DV data vs. voice
 *   Bit 4  Interrupted    — previous transmission was interrupted (BK)
 *   Bit 2  Control signal — auto-control frame
 *   Bit 0  Urgent         — emergency (EMR)
 */
class CHeaderData {
public:
	CHeaderData();
	CHeaderData(const CHeaderData& header);
	// Deserialise from raw wire bytes; set check=true to validate the checksum.
	CHeaderData(const unsigned char* data, unsigned int length, bool check);
	CHeaderData(const std::string& myCall1,  const std::string& myCall2, const std::string& yourCall,
				const std::string& rptCall1, const std::string& rptCall2, unsigned char flag1 = 0x00,
				unsigned char flag2 = 0x00, unsigned char flag3 = 0x00);
	~CHeaderData();

	time_t      getTime() const;
	std::string getMyCall1() const;
	std::string getMyCall2() const;
	std::string getYourCall() const;
	std::string getRptCall1() const;
	std::string getRptCall2() const;

	unsigned char getFlag1() const;
	unsigned char getFlag2() const;
	unsigned char getFlag3() const;

	bool isAck() const;
	bool isNoResponse() const;
	bool isRelayUnavailable() const;
	bool isRepeaterMode() const;
	bool isDataPacket() const;
	bool isInterrupted() const;
	bool isControlSignal() const;
	bool isUrgent() const;
	unsigned char getRepeaterFlags() const;

	void setFlag1(unsigned char flag);
	void setFlag2(unsigned char flag);
	void setFlag3(unsigned char flag);

	void setMyCall1(const std::string& callsign);
	void setMyCall2(const std::string& callsign);
	void setYourCall(const std::string& callsign);
	void setRptCall1(const std::string& callsign);
	void setRptCall2(const std::string& callsign);

	void setRepeaterMode(bool set);
	void setDataPacket(bool set);
	void setInterrupted(bool set);
	void setControlSignal(bool set);
	void setUrgent(bool set);
	void setRepeaterFlags(unsigned char set);

	// Returns false if the checksum was checked and failed during deserialisation.
	bool isValid() const;

	void reset();

	CHeaderData& operator=(const CHeaderData& header);

private:
	time_t        m_time;
	std::string   m_myCall1;
	std::string   m_myCall2;
	std::string   m_yourCall;
	std::string   m_rptCall1;
	std::string   m_rptCall2;
	unsigned char m_flag1;
	unsigned char m_flag2;
	unsigned char m_flag3;
	bool          m_valid;   // Set to false if checksum validation failed.
};

#endif
