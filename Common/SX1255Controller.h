/*
 *   Copyright (C) 2025 by Jonathan Naylor G4KLX and contributors
 *   SX1255 support: Karel Kadlec (OK1LBC) <xkk@post.cz>
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

#ifndef	SX1255Controller_H
#define	SX1255Controller_H

#include "SoundCardController.h"

#include <atomic>
#include <string>
#include <thread>

/*
 * CSX1255Controller - D-Star modem using a Semtech SX1255 direct-I/Q HAT.
 *
 * The SX1255 is a 410-510 MHz transceiver that exposes zero/low-IF I/Q over an
 * I2S ALSA sound card (the "genericstereoaudiocodec" overlay) and is configured
 * over Linux SPI.  The D-Star protocol engine (GMSK modulation, frame sync, FEC,
 * interleaving and scrambling) is inherited from CSoundCardController and is
 * therefore identical to the existing Sound Card modem.
 *
 * This class replaces only the audio transport:
 *
 *   RX: 125 kHz S32_LE interleaved I/Q -> DC block -> complex mixer
 *       (RXShift) -> LPF + decimate by 5 -> FM discriminator -> fractional
 *       resample to 48 kHz -> CSoundCardController baseband demodulator.
 *
 *   TX: CSoundCardController GMSK baseband at 48 kHz -> FM modulator ->
 *       fractional upsample to 125 kHz -> S32_LE interleaved I/Q.
 *
 * SPI is used to program the RX/TX frequencies, the chip configuration
 * registers and the RX/TX enable mode register (0x00).  The TX front-end is
 * only enabled while D-Star data is actually being transmitted so that an idle
 * repeater does not radiate an unmodulated carrier.
 */
class CSX1255Controller : public CSoundCardController {
public:
	CSX1255Controller(const std::string& rxDevice, const std::string& txDevice,
	                  const std::string& spiDevice,
	                  unsigned int rxFrequency, unsigned int txFrequency,
	                  float rxShiftHz, float txDeviationHz,
	                  bool rxInvert, bool txInvert,
	                  float rxLevel, float txLevel,
	                  unsigned int txDelay, unsigned int txTail,
	                  unsigned int spiSpeed, int resetGpio,
	                  const std::string& resetChip, unsigned int resetLine,
	                  const std::string& pttChip, unsigned int pttLine, bool pttInvert);
	virtual ~CSX1255Controller();

	virtual bool start() override;
	virtual bool isTX() override;
	virtual void stop() override;

private:
	void rxThread();
	void txThread();

	bool spiOpen();
	void spiClose();
	bool spiReset();
	bool spiInitChip();
	bool spiSetFrequency(bool tx, unsigned int freqHz);
	bool spiSetMode(unsigned char mode);

	bool alsaOpen();
	void alsaClose();

	bool pttOpen();
	void pttClose();
	void pttSet(bool active);

	// Configuration
	std::string  m_rxDevice;
	std::string  m_txDevice;
	std::string  m_spiDevice;
	unsigned int m_rxFrequency;
	unsigned int m_txFrequency;
	float        m_rxShiftHz;
	float        m_txDeviationHz;
	unsigned int m_spiSpeed;
	int          m_resetGpio;
	std::string  m_resetChip;
	unsigned int m_resetLine;
	std::string  m_pttChip;
	unsigned int m_pttLine;
	bool         m_pttInvert;

	// Runtime state
	int                 m_spiFd;
	snd_pcm_t*          m_pcmRx;
	snd_pcm_t*          m_pcmTx;
	std::thread         m_rxThread;
	std::thread         m_txThread;
	std::atomic<bool>   m_txEnabled;   // SX1255 TX front-end enable state
	std::atomic<bool>   m_txActive;    // True while D-Star data is flowing out
	void*               m_pttChipHandle; // gpiod_chip*
	void*               m_pttRequest;    // gpiod_line_request*
	bool                m_pttAsserted;
};

#endif
