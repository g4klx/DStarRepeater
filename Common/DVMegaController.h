/*
 *   Copyright (C) 2011-2015 by Jonathan Naylor G4KLX
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

#ifndef	DVMegaController_H
#define	DVMegaController_H

#include "SerialDataController.h"
#include "RingBuffer.h"
#include "Modem.h"
#include "Utils.h"
#include "StdCompat.h"

#include <string>

enum RESP_TYPE_MEGA {
	RTM_TIMEOUT,
	RTM_ERROR,
	RTM_UNKNOWN,
	RTM_GET_STATUS,
	RTM_GET_VERSION,
	RTM_GET_SERIAL,
	RTM_GET_CONFIG,
	RTM_SET_CONFIG,
	RTM_RXPREAMBLE,
	RTM_START,
	RTM_HEADER,
	RTM_RXSYNC,
	RTM_DATA,
	RTM_EOT,
	RTM_RXLOST,
	RTM_SET_TESTMDE
};

/*
 * CDVMegaController - Driver for DV-Mega boards (modem and radio variants).
 *
 * Hardware interface: USB CDC-ACM serial at 115200 baud.
 *
 * Two hardware variants, selected by constructor:
 *   1. Modem variant  (port, path, rxInvert, txInvert, txDelay)
 *      The board is a bare GMSK modem; the host radio provides the RF.
 *      rxInvert/txInvert flip the baseband polarity to match the radio's
 *      discriminator/modulator wiring.
 *   2. Radio variant  (port, path, txDelay, rxFrequency, txFrequency, power)
 *      The board includes a UHF/VHF transceiver.  Frequency and power are
 *      programmed at startup via a second SET_CONFIG (RF layer) command.
 *      rxFrequency != 0 is the distinguishing condition checked in openModem().
 *
 * Protocol: DVRPTR binary framing (shared with DV-RPTR V1).
 *   Frame structure:  [0xD0] [len_lo] [len_hi] [type] [txCount] [pktCount]
 *                     [payload...] [checksum_lo] [checksum_hi]
 *   The type byte has bit 7 set in responses from the board (stripped with & 0x7F).
 *   Checksum is either CCITT-16 (when m_checksum is true, reported in GET_STATUS)
 *   or a fixed sentinel 0x000B when the firmware does not support checksums.
 *
 * Reconnection (findModem()):
 *   On any serial error the port is closed.  An in-progress RX stream is
 *   terminated with a synthetic DSMTT_EOT.  The driver then polls every 2s
 *   (4 x 500ms sleep) searching for the device by USB sysfs path before
 *   reopening and re-initialising.
 *
 * Sysfs path tracking (findPort / findPath):
 *   m_path is the stable USB device path (e.g. /sys/devices/platform/.../1-1.2).
 *   This survives port renumbering across reconnects.  findPort() walks
 *   /sys/class/tty/ttyACM* to find which /dev node currently maps to that path.
 */
class CDVMegaController : public CModem {
public:
	// Modem-only variant: no integrated RF, host radio provides the signal.
	CDVMegaController(const std::string& port, const std::string& path, bool rxInvert, bool txInvert, unsigned int txDelay);
	// Radio variant: integrated transceiver; frequency in Hz, power in percent.
	CDVMegaController(const std::string& port, const std::string& path, unsigned int txDelay, unsigned int rxFrequency, unsigned int txFrequency, unsigned int power);
	virtual ~CDVMegaController();

	virtual bool start();

	virtual unsigned int getSpace();
	virtual bool isTXReady();

	virtual bool writeHeader(const CHeaderData& header);
	virtual bool writeData(const unsigned char* data, unsigned int length, bool end);

	virtual std::string getPath() const;

private:
	void entry();

	std::string                m_port;
	std::string                m_path;
	bool                       m_rxInvert;
	bool                       m_txInvert;
	unsigned int               m_txDelay;
	unsigned int               m_rxFrequency;
	unsigned int               m_txFrequency;
	unsigned int               m_power;
	CSerialDataController      m_serial;
	unsigned char*             m_buffer;
	CRingBuffer<unsigned char> m_txData;
	unsigned char              m_txCounter;
	unsigned char              m_pktCounter;
	bool                       m_rx;
	unsigned int               m_txSpace;
	bool                       m_txEnabled;
	bool                       m_checksum;

	bool readVersion();
	bool readStatus();
	bool setConfig();
	bool setFrequencyAndPower();
	bool setEnabled(bool enable);

	RESP_TYPE_MEGA getResponse(unsigned char* buffer, unsigned int& length);

	bool findPort();
	bool findPath();

	// Closes the port, signals EOT if mid-stream, then polls every 2s until
	// findPort() + openModem() succeed or m_stopped is set.
	bool findModem();
	// Opens the serial port, sends GET_VERSION, SET_CONFIG (physical layer),
	// optionally SET_CONFIG (RF layer for radio variant), then setEnabled(true).
	bool openModem();
};

#endif
