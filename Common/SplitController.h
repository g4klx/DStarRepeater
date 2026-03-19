/*
 *   Copyright (C) 2012-2014 by Jonathan Naylor G4KLX
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

#ifndef	SplitController_H
#define	SplitController_H

#include "GatewayProtocolHandler.h"
#include "DStarDefines.h"
#include "RingBuffer.h"
#include "Timer.h"
#include "Modem.h"
#include "Utils.h"
#include "StdCompat.h"

#include <string>
#include <vector>
#include <cstdint>

/*
 * CAMBESlot - Holds one sequence-number slot's worth of AMBE data from all receivers.
 *
 * D-Star transmits 21 frames per super-frame (one data sync + 20 voice frames).
 * CSplitController maintains 21 CAMBESlot objects, indexed by the D-Star sequence
 * number.  When AMBE data arrives from multiple receivers for the same slot, the
 * copy with the fewest AMBE error bits (lowest m_errors) is kept as the best copy.
 *
 * m_valid[n] - true when receiver n has contributed data for this slot.
 * m_end[n]   - true when receiver n flagged end-of-transmission for this slot.
 * m_best     - index of the receiver whose data is currently stored in m_ambe.
 * m_timer    - deadline by which the slot must be forwarded to the repeater,
 *              even if not all receivers have responded.
 */
class CAMBESlot {
public:
	CAMBESlot(unsigned int rxCount);
	~CAMBESlot();

	void reset();

	// Returns true when no receiver has deposited data yet (m_length == 0).
	bool isFirst() const;

	bool*          m_valid;
	unsigned int   m_errors;
	unsigned int   m_best;
	unsigned char* m_ambe;
	unsigned int   m_length;
	bool*          m_end;
	CTimer         m_timer;

private:
	unsigned int   m_rxCount;
};

/*
 * CSplitController - Multi-receiver split-site repeater controller using UDP.
 *
 * Allows multiple geographically separated receive sites to feed a single
 * repeater.  Each receiver and transmitter connects to the controller via UDP
 * using the CGatewayProtocolHandler (ircDDB gateway wire protocol).
 *
 * Network protocol:
 *   Receivers send NETWORK_HEADER and NETWORK_DATA packets, each containing
 *   the stream ID, sequence number, AMBE payload, and an error count.
 *   Transmitters receive NETWORK_HEADER and NETWORK_DATA forwarded by the
 *   controller's transmit() method.
 *   All remotes register by sending NETWORK_REGISTER with their name string;
 *   the controller maps names to IP:port and runs a REGISTRATION_TIMEOUT timer
 *   that expires if a remote goes silent.
 *
 * AMBE selection (best-copy diversity):
 *   For each D-Star sequence number slot (0-20), AMBE frames from all registered
 *   receivers are buffered in the corresponding CAMBESlot.  The frame with the
 *   fewest AMBE error bits wins and is forwarded to the repeater when the slot
 *   timer expires.  If no receiver provides data for a slot, a silence frame
 *   (NULL_FRAME_DATA_BYTES) is substituted.
 *
 * Resequencing:
 *   m_inSeqNo tracks the next expected sequence number from any receiver.
 *   m_outSeqNo tracks the next slot to be forwarded to the repeater.
 *   Out-of-order packets that are more than 18 slots ahead are discarded.
 *
 * Header forwarding (sendHeader()):
 *   Deferred until the first data slot is ready to be forwarded.  This ensures
 *   the repeater receives header + data without a gap, and means m_headerSent
 *   guards against duplicate header injection.
 *
 * timeout: per-slot wait time in milliseconds added to each slot's timer on
 *   top of the expected frame arrival time.  Increasing timeout improves
 *   diversity at the cost of added latency.
 */
class CSplitController : public CModem {
public:
	CSplitController(const std::string& localAddress, unsigned int localPort, const std::vector<std::string>& transmitterNames, const std::vector<std::string>& receiverNames, unsigned int timeout);
	virtual ~CSplitController();

	virtual bool start();

	virtual unsigned int getSpace();
	virtual bool isTXReady();

	virtual bool writeHeader(const CHeaderData& header);
	virtual bool writeData(const unsigned char* data, unsigned int length, bool end);

private:
	void entry();

	CGatewayProtocolHandler    m_handler;
	std::vector<std::string>   m_transmitterNames;
	std::vector<std::string>   m_receiverNames;
	unsigned int               m_timeout;
	unsigned int               m_txCount;
	unsigned int               m_rxCount;
	in_addr*                   m_txAddresses;
	unsigned int*              m_txPorts;
	CTimer**                   m_txTimers;
	in_addr*                   m_rxAddresses;
	unsigned int*              m_rxPorts;
	CTimer**                   m_rxTimers;
	CRingBuffer<unsigned char> m_txData;
	uint16_t                   m_outId;
	uint8_t                    m_outSeq;
	CTimer                     m_endTimer;
	bool                       m_listening;
	uint8_t                    m_inSeqNo;
	uint8_t                    m_outSeqNo;
	unsigned char*             m_header;
	uint16_t*                  m_id;
	bool*                      m_valid;
	CAMBESlot**                m_slots;
	bool                       m_headerSent;
	unsigned int*              m_packets;
	unsigned int*              m_best;
	unsigned int*              m_missed;
	unsigned int               m_silence;

	// Drains m_txData and forwards header/data/EOT to all registered transmitters.
	void transmit();
	// Reads all pending UDP packets and dispatches them to processHeader/processAMBE
	// or updates remote registrations.
	void receive();
	// Advances all timers by ms; expires slot timers and forwards data to m_rxData.
	void timers(unsigned int ms);
	// Records a header from receiver n.  First header starts the slot timers and
	// initialises per-receiver tracking; subsequent headers from other receivers
	// are validated by memcmp against the first.
	void processHeader(unsigned int n, uint16_t id, const unsigned char* header, unsigned int length);
	// Deposits an AMBE frame from receiver n into the appropriate sequence slot.
	// Keeps the frame with the lowest error count; discards frames > 18 slots ahead.
	void processAMBE(unsigned int n, uint16_t id, const unsigned char* ambe, unsigned int length, uint8_t seqNo, unsigned char errors);
	// Returns true when all valid receivers have signalled end-of-transmission
	// and none are still active in this slot.
	bool isEnd(const CAMBESlot& slot) const;
	// Queues the buffered header into m_rxData; must be called with m_mutex held.
	void sendHeader();
	// Logs per-receiver packet counts, best-copy proportions, and missed frames.
	void printStats() const;
};

#endif
