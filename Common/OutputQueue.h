/*
 *   Copyright (C) 2011,2014 by Jonathan Naylor G4KLX
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

#ifndef	OutputQueue_H
#define	OutputQueue_H

#include "RingBuffer.h"
#include "HeaderData.h"

/*
 * FIFO queue for outbound D-Star frames with type/length/data encoding.
 *
 * Each frame is stored in the ring buffer as a 2-byte header followed by the
 * frame payload:
 *   byte 0: end flag — 1 if this is the last frame of the transmission, else 0
 *   byte 1: payload length in bytes
 *   [payload bytes]
 *
 * A separate m_header pointer holds the pending CHeaderData for the next
 * transmission.  Playback is gated by a threshold: neither the header nor data
 * frames are released to the transmitter until m_count >= m_threshold, which
 * ensures there is a small buffer of frames pre-queued before TX begins (to
 * absorb jitter in frame delivery from the network).  Receiving an end-of-
 * stream frame immediately raises the count to the threshold so the remaining
 * frames drain promptly.
 */
class COutputQueue {
public:
	COutputQueue(unsigned int space, unsigned int threshold);
	~COutputQueue();

	// Takes ownership of header; replaces any previously pending header.
	void setHeader(CHeaderData* header);
	// Returns the pending header and relinquishes ownership (caller must delete).
	CHeaderData* getHeader();

	unsigned int getData(unsigned char* data, unsigned int length, bool& end);
	unsigned int addData(const unsigned char* data, unsigned int length, bool end);

	// True when a header is queued and the pre-fill threshold has been reached.
	bool headerReady() const;
	// True when data frames are queued and no header is pending (mid-stream).
	bool dataReady() const;

	bool isEmpty();

	void reset();

	void setThreshold(unsigned int threshold);

private:
	CRingBuffer<unsigned char> m_data;
	unsigned int               m_threshold;
	CHeaderData*               m_header;
	unsigned int               m_count;   // Frames added since the last header; triggers playback start.
};

#endif
