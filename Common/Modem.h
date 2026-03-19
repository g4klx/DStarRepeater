/*
 *   Copyright (C) 2013,2014 by Jonathan Naylor G4KLX
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

/*
 * CModem is the abstract base class for all hardware modem drivers
 * (DVAP, MMDVM, DVMega, GMSK USB, etc.).
 *
 * Threading model
 * ---------------
 * Each concrete driver runs its own I/O thread (started by start(), stopped by
 * stop()).  That thread reads received frames from the hardware and pushes them
 * into the m_rxData ring buffer.  The repeater thread then calls read() /
 * readHeader() / readData() to drain the buffer.  The mutex guards the ring
 * buffer against concurrent access between the two threads.
 *
 * Read protocol
 * -------------
 * The repeater thread calls read() each tick.  If a message is available,
 * read() returns the DSMT_TYPE tag and latches the payload into an internal
 * scratch buffer.  The caller then calls the matching accessor:
 *   - DSMTT_HEADER -> readHeader()   (returns a heap-allocated CHeaderData)
 *   - DSMTT_DATA   -> readData()     (copies bytes into the caller's buffer)
 * Other types (START, EOT, LOST) carry no payload and need no further call.
 */

#ifndef	Modem_H
#define	Modem_H

#include "HeaderData.h"
#include "RingBuffer.h"
#include "StdCompat.h"

#include <atomic>
#include <thread>
#include <mutex>

// Message type tags pushed by the driver thread into the ring buffer.
// Each message is prefixed with [type:1][length:1][payload:length] bytes.
enum DSMT_TYPE {
	DSMTT_NONE,    // No message waiting
	DSMTT_START,   // Hardware has started (carrier / sync detected)
	DSMTT_HEADER,  // A valid D-Star header has been decoded from the air
	DSMTT_DATA,    // One DV frame of voice + slow-data payload
	DSMTT_EOT,     // End-of-transmission detected on air
	DSMTT_LOST,    // Signal lost mid-frame (carrier dropped unexpectedly)
};

class CModem {
public:
	CModem();
	virtual ~CModem();

	// Start the driver I/O thread and open the hardware device.
	virtual bool start() = 0;

	// Transmit a D-Star header (called before the first writeData()).
	virtual bool writeHeader(const CHeaderData& header) = 0;
	// Transmit one DV frame; set end=true for the final frame of a transmission.
	virtual bool writeData(const unsigned char* data, unsigned int length, bool end) = 0;

	// Number of DV frames the TX buffer can still accept.
	virtual unsigned int getSpace() = 0;
	// True when the hardware is ready to accept TX frames.
	virtual bool isTXReady() = 0;
	// True while the hardware is actively transmitting.
	virtual bool isTX();

	// Drain the next message from the ring buffer; returns its type tag.
	// Must be called before readHeader() or readData().
	virtual DSMT_TYPE read();
	// Returns the header decoded by the most recent read() == DSMTT_HEADER call.
	// Caller owns the returned object.
	virtual CHeaderData* readHeader();
	// Copies DV frame bytes from the most recent read() == DSMTT_DATA call.
	virtual unsigned int readData(unsigned char* data, unsigned int length);

	// Signal the driver thread to exit and wait for it to join.
	virtual void stop();

protected:
	// SPSC ring buffer: driver thread writes, repeater thread reads.
	CRingBuffer<unsigned char> m_rxData;
	// Guards m_rxData against concurrent access.
	std::mutex                 m_mutex;
	std::atomic<bool>          m_tx;       // Current TX state of the hardware
	std::atomic<bool>          m_stopped;  // Set by stop() to terminate the thread
	std::thread                m_thread;   // The driver I/O thread

private:
	// Scratch state populated by read() and consumed by readHeader()/readData().
	DSMT_TYPE                  m_readType;
	unsigned int               m_readLength;
	unsigned char*             m_readBuffer;
};

#endif
