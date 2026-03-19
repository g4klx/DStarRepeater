/*
 *   Copyright (C) 2011-2014 by Jonathan Naylor G4KLX
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

#ifndef	DVAPController_H
#define	DVAPController_H

#define	DVAP_DUMP

#include "SerialDataController.h"
#include "RingBuffer.h"
#include "HeaderData.h"
#include "Modem.h"
#include "Utils.h"
#include "StdCompat.h"

#include <string>
#include <cstdint>

enum RESP_TYPE {
	RT_TIMEOUT,
	RT_ERROR,
	RT_UNKNOWN,
	RT_NAME,
	RT_SERIAL,
	RT_FIRMWARE,
	RT_START,
	RT_STOP,
	RT_MODULATION,
	RT_MODE,
	RT_SQUELCH,
	RT_POWER,
	RT_FREQUENCY,
	RT_FREQLIMITS,
	RT_STATE,
	RT_PTT,
	RT_ACK,
	RT_HEADER,
	RT_HEADER_ACK,
	RT_GMSK_DATA,
	RT_FM_DATA
};

/*
 * CDVAPController - Driver for the DVAP USB dongle (Digital Voice Access Point).
 *
 * Hardware interface: USB CDC-ACM serial at 230400 baud.
 *
 * Protocol: Binary, little-endian framing.
 *   Every frame begins with a 2-byte header:
 *     byte[0]      = total frame length (low byte)
 *     byte[1]      = flags in upper 3 bits; length high bits in lower 5
 *   The command/type is encoded in bytes[2..3].
 *
 * Startup sequence (enforced in start()):
 *   getName() -> getFirmware() -> getSerial() ->
 *   setModulation() -> setMode() -> setSquelch() -> setPower() -> setFrequency() ->
 *   startDVAP()
 *
 * During operation (entry() thread):
 *   - Polls every ~2s by sending a 3-byte ACK (writePoll()).
 *   - The DVAP sends RT_STATE packets every ~20ms containing RSSI, squelch
 *     state, and TX buffer space; these are used to clock outbound data writes.
 *   - Received headers and GMSK voice frames are forwarded to m_rxData (the
 *     CModem shared RX ring buffer) as DSMTT_HEADER / DSMTT_DATA / DSMTT_EOT
 *     typed records.
 *   - If the DVAP spontaneously sends an FM_DATA packet it has lost GMSK lock;
 *     the driver restarts modulation to recover.
 *
 * Power: -12 to +10 dBm (int16_t, little-endian in wire frame).
 * Squelch: -128 to -45 (int8_t, signed dBm threshold).
 * Frequency: 32-bit Hz, validated against dongle-reported hardware limits.
 *
 * DVAP_DUMP build flag enables a circular packet trace buffer (last 30 frames)
 * that is dumped to the log on any error or unexpected FM mode event.
 */
class CDVAPController : public CModem {
public:
	CDVAPController(const std::string& port, unsigned int frequency, int power, int squelch);
	virtual ~CDVAPController();

	virtual bool start();

	virtual unsigned int getSpace();
	virtual bool isTXReady();

	virtual bool getSquelch() const;
	virtual int  getSignal() const;

	virtual bool writeHeader(const CHeaderData& header);
	virtual bool writeData(const unsigned char* data, unsigned int length, bool end);

private:
	void entry();

	CSerialDataController      m_serial;
	uint32_t                   m_frequency;
	int16_t                    m_power;
	int8_t                     m_squelch;
	std::atomic<bool>          m_squelchOpen;
	std::atomic<int>           m_signal;
	unsigned char*             m_buffer;
	uint16_t                   m_streamId;
	uint8_t                    m_framePos;
	uint8_t                    m_seq;
	CRingBuffer<unsigned char> m_txData;
#if defined(DVAP_DUMP)
	unsigned char**            m_dvapData;
	unsigned int*              m_dvapLength;
	unsigned int               m_dvapIndex;
#endif

	bool getName();
	bool getFirmware();
	bool getSerial();

	bool setModulation();
	bool setMode();

	bool setSquelch();
	bool setPower();
	bool setFrequency();

	bool startDVAP();
	bool stopDVAP();

	// Sends a 3-byte keepalive ACK; DVAP requires periodic traffic to stay active.
	void writePoll();

	// Resynchronises the byte stream after a framing error by discarding bytes
	// one at a time until a complete RT_STATE (status) header is found.
	// Called when getResponse() receives an unrecognisable or oversized frame.
	void resync();

	// Reads one complete binary frame from the serial port into buffer.
	// Returns a RESP_TYPE identifying the frame kind, or RT_TIMEOUT if no
	// data is available and RT_ERROR on a read failure.
	RESP_TYPE getResponse(unsigned char* buffer, unsigned int& length);

#if defined(DVAP_DUMP)
	void storePacket(const unsigned char* data, unsigned int length);
	void dumpPackets();
#endif
};

#endif
