/*
 *   Copyright (C) 2009-2015 by Jonathan Naylor G4KLX
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

#ifndef	SoundCardController_H
#define	SoundCardController_H

#include "SoundCardReaderWriter.h"
#include "DStarGMSKDemodulator.h"
#include "DStarGMSKModulator.h"
#include "AudioCallback.h"
#include "RingBuffer.h"
#include "Modem.h"
#include "Utils.h"
#include "StdCompat.h"

#include <string>
#include <cstdint>

enum DSRSCC_STATE {
	DSRSCCS_NONE,
	DSRSCCS_HEADER,
	DSRSCCS_DATA
};

/*
 * CSoundCardController - D-Star modem using a sound card as the RF interface.
 *
 * Implements both CModem (repeater-facing) and IAudioCallback (audio I/O facing).
 * The sound card carries the baseband GMSK signal: the discriminator output of the
 * radio feeds the RX input, and the TX output drives the radio's microphone input.
 *
 * Audio path (IAudioCallback):
 *   readCallback()  - called by CSoundCardReaderWriter with captured float samples.
 *                     Appends to m_rxAudio ring buffer; entry() drains it.
 *   writeCallback() - called when the audio output device needs a new block.
 *                     Drains m_txAudio into the provided output buffer.
 *
 * Demodulation (entry() thread, processes m_rxAudio bit by bit):
 *   m_demodulator converts float samples to a recovered bit stream.
 *   A 32-bit shift register (m_patternBuffer) is used to detect the D-Star
 *   frame sync pattern (FRAME_SYNC_DATA) and data sync (DATA_SYNC_DATA).
 *   State machine (m_rxState): NONE -> HEADER -> DATA
 *     processNone():   hunts for the frame sync pattern (up to MAX_SYNC_BITS).
 *     processHeader(): accumulates FEC_SECTION_LENGTH_BITS (660) bits, then calls
 *                      rxHeader() which runs the Viterbi decoder and validates
 *                      the CCITT checksum before queuing a DSMTT_HEADER record.
 *     processData():   accumulates DV_FRAME_LENGTH_BITS per frame; detects end
 *                      pattern and data sync for sequence tracking.
 *
 * Modulation (writeCallback()):
 *   writeHeader() / writeData() push into m_txAudio.
 *   The modulator (m_modulator) converts binary frames to float samples.
 *   txDelay pads silence before the first header bit; txTail pads after EOT.
 *
 * FEC and interleaving:
 *   The header section is rate-1/2 convolutional coded + interleaved.
 *   Viterbi decoding state is held in m_pathMetric, m_pathMemory0-3, m_fecOutput.
 *   countBits() uses a 4-bit lookup table (NIBBLE_BITS) for Hamming weight.
 */
class CSoundCardController : public CModem, public IAudioCallback {
public:
	CSoundCardController(const std::string& rxDevice, const std::string& txDevice, bool rxInvert, bool txInvert, float rxLevel, float txLevel, unsigned int txDelay, unsigned int txTail);
	virtual ~CSoundCardController();

	virtual bool start();

	virtual unsigned int getSpace();
	virtual bool isTXReady();
	virtual bool isTX();

	virtual bool writeHeader(const CHeaderData& header);
	virtual bool writeData(const unsigned char* data, unsigned int length, bool end);

	virtual void readCallback(const float* input, unsigned int n, int id);
	virtual void writeCallback(float* output, int& n, int id);

private:
	void entry();

	CSoundCardReaderWriter     m_sound;
	float                      m_rxLevel;
	float                      m_txLevel;
	unsigned int               m_txDelay;
	unsigned int               m_txTail;
	CRingBuffer<float>         m_txAudio;
	CRingBuffer<float>         m_rxAudio;
	DSRSCC_STATE               m_rxState;
	uint32_t                   m_patternBuffer;
	CDStarGMSKDemodulator      m_demodulator;
	CDStarGMSKModulator        m_modulator;
	unsigned char*             m_rxBuffer;
	unsigned int               m_rxBufferBits;
	unsigned int               m_dataBits;
	unsigned int               m_mar;
	int*                       m_pathMetric;
	unsigned int*              m_pathMemory0;
	unsigned int*              m_pathMemory1;
	unsigned int*              m_pathMemory2;
	unsigned int*              m_pathMemory3;
	unsigned char*             m_fecOutput;

	void processNone(bool bit);
	void processHeader(bool bit);
	void processData(bool bit);

	bool rxHeader(unsigned char* in, unsigned char* out);
	void acs(int* metric);
	void viterbiDecode(int* data);
	void traceBack();

	void txHeader(const unsigned char* in, unsigned char* out);
	void writeBits(unsigned char c);

	unsigned int countBits(uint32_t num);
};

#endif
