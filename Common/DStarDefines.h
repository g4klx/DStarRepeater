/*
 *	Copyright (C) 2009-2015 by Jonathan Naylor, G4KLX
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 */

/*
 * Central D-Star digital voice protocol constants. These values are mandated by
 * the D-Star specification and must not be changed without breaking on-air
 * compatibility.  They cover the physical layer (GMSK modulation), frame
 * structure, slow-data encoding, flag fields, callsign formats, timing, and
 * network linking ports.
 */

#ifndef	DStarDefines_H
#define	DStarDefines_H

#include "StdCompat.h"

// --- Physical / modulation layer ---

// D-Star uses GMSK at 4800 bps with a Gaussian BT product of 0.5.
const unsigned int DSTAR_GMSK_SYMBOL_RATE = 4800U;
const float        DSTAR_GMSK_BT          = 0.5F;

// --- Frame structure ---

// Three-byte pattern that marks the start of a DV data frame within a stream.
inline const unsigned char DATA_SYNC_BYTES[] = {0x55, 0x2D, 0x16};

// Sent after the last DV frame to signal end-of-transmission on air.
// Only the first END_PATTERN_LENGTH_BYTES bytes are transmitted; the rest are padding.
inline const unsigned char END_PATTERN_BYTES[] = {0x55, 0x55, 0x55, 0x55, 0xC8, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const unsigned int END_PATTERN_LENGTH_BYTES = 6U;

// AMBE codec output for a frame of silence (used to fill gaps).
inline const unsigned char NULL_AMBE_DATA_BYTES[] = {0x9E, 0x8D, 0x32, 0x88, 0x26, 0x1A, 0x3F, 0x61, 0xE8};

// Slow-data bytes representing an empty payload.
// Note that these are already scrambled, 0x66 0x66 0x66 otherwise
inline const unsigned char NULL_SLOW_DATA_BYTES[] = {0x16, 0x29, 0xF5};

// Combined null DV frame: NULL_AMBE_DATA_BYTES followed by NULL_SLOW_DATA_BYTES.
inline const unsigned char NULL_FRAME_DATA_BYTES[] = {0x9E, 0x8D, 0x32, 0x88, 0x26, 0x1A, 0x3F, 0x61, 0xE8, 0x16, 0x29, 0xF5};

// Each DV frame carries 9 bytes of AMBE-compressed voice (72 bits at 3600 bps).
const unsigned int VOICE_FRAME_LENGTH_BYTES  = 9U;

// Each DV frame also carries 3 bytes of slow data (24 bits at 1200 bps).
const unsigned int DATA_FRAME_LENGTH_BYTES   = 3U;

// Total wire size of one DV frame: voice + slow-data interleaved.
const unsigned int DV_FRAME_LENGTH_BYTES     = VOICE_FRAME_LENGTH_BYTES + DATA_FRAME_LENGTH_BYTES;

// The length of the end frame, three bytes extra
// Maximum DV frame buffer size, accommodating the end-of-transmission extra bytes.
const unsigned int DV_FRAME_MAX_LENGTH_BYTES = DV_FRAME_LENGTH_BYTES + 3U;

// Length of the FEC (forward error correction) section in the radio header.
const unsigned int FEC_SECTION_LENGTH_BYTES = 83U;

// On-air D-Star header size: 41 bytes covering callsigns, flags, and FEC.
const unsigned int RADIO_HEADER_LENGTH_BYTES = 41U;

// 21 DV frames form one slow-data block (enough to carry the full header in-band).
const unsigned int DATA_BLOCK_SIZE_BYTES = 21U * DV_FRAME_LENGTH_BYTES;

// --- Audio / DSP sizing ---

// Baseband sample rate used by modem drivers that process raw I/Q or audio.
const unsigned int DSTAR_RADIO_SAMPLE_RATE = 48000U;
// Samples per processing block handed to the modem (20 ms at 48 kHz).
const unsigned int DSTAR_RADIO_BLOCK_SIZE  = 960U;
// Samples per AMBE codec frame (20 ms at 8 kHz narrowband).
const unsigned int DSTAR_AUDIO_BLOCK_SIZE  = 160U;

// --- Callsign / locator field widths ---

// Maidenhead grid locator string length (e.g. "IO91wm").
const unsigned int LOCATOR_LENGTH        = 6U;
// D-Star callsign field is always 8 characters, right-padded with spaces.
const unsigned int LONG_CALLSIGN_LENGTH  = 8U;
// Suffix / RPT2 suffix field (e.g. " G" for gateway, " A" for port A).
const unsigned int SHORT_CALLSIGN_LENGTH = 4U;

// --- Slow-data field encoding ---

// Upper nibble of the slow-data type byte identifies the payload type.
const unsigned char SLOW_DATA_TYPE_MASK    = 0xF0U;
const unsigned char SLOW_DATA_TYPE_GPSDATA = 0x30U;  // APRS/GPS position string
const unsigned char SLOW_DATA_TYPE_TEXT    = 0x40U;  // Free-text message (TX message)
const unsigned char SLOW_DATA_TYPE_HEADER  = 0x50U;  // Repeated radio header
const unsigned char SLOW_DATA_TYPE_SQUELCH = 0xC0U;  // Squelch-tail / keep-alive
// Lower nibble gives the number of valid data bytes in this slow-data chunk.
const unsigned char SLOW_DATA_LENGTH_MASK  = 0x0FU;

// Maximum length of a slow-data text message in characters.
const unsigned int  SLOW_DATA_TEXT_LENGTH  = 20U;

// --- Scrambler ---

// XOR pattern applied to each slow-data byte pair to decorrelate bit patterns
// on air.  Applied as byte[0] ^= BYTE1, byte[1] ^= BYTE2, byte[2] ^= BYTE3.
const unsigned char SCRAMBLER_BYTE1 = 0x70U;
const unsigned char SCRAMBLER_BYTE2 = 0x4FU;
const unsigned char SCRAMBLER_BYTE3 = 0x93U;

// --- Header flag byte 1 (RF_HEADER.flag[0]) ---

// Set when the frame carries data rather than voice.
const unsigned char DATA_MASK           = 0x80U;
// Set when a repeater is involved in the QSO.
const unsigned char REPEATER_MASK       = 0x40U;
// Set when the transmission was interrupted (incomplete header received).
const unsigned char INTERRUPTED_MASK    = 0x20U;
// Set for control-channel signalling frames.
const unsigned char CONTROL_SIGNAL_MASK = 0x10U;
// Set to request priority / urgent handling.
const unsigned char URGENT_MASK         = 0x08U;

// --- Header flag byte 1 lower nibble: repeater control codes ---

const unsigned char REPEATER_CONTROL_MASK = 0x07U;
const unsigned char REPEATER_CONTROL      = 0x07U;  // Normal repeater frame
const unsigned char AUTO_REPLY            = 0x06U;  // Automatic acknowledgement
const unsigned char RESEND_REQUESTED      = 0x04U;  // Request retransmission
const unsigned char ACK_FLAG              = 0x03U;  // Positive acknowledgement
const unsigned char NO_RESPONSE           = 0x02U;  // Destination not responding
const unsigned char RELAY_UNAVAILABLE     = 0x01U;  // Requested relay is busy/offline

// --- Timing ---

// Number of 48 kHz samples per symbol at 4800 bps.
const unsigned int  DSTAR_RADIO_BIT_LENGTH = DSTAR_RADIO_SAMPLE_RATE / DSTAR_GMSK_SYMBOL_RATE;

// D-Star frame period: one DV frame every 20 ms = 50 frames per second.
const unsigned int  DSTAR_FRAME_TIME_MS  = 20U;
const unsigned int  DSTAR_FRAMES_PER_SEC = 50U;

// Processing ticks per second derived from the DSP block size (48000/960 = 50).
const unsigned int  DSTAR_TICKS_PER_SEC = DSTAR_RADIO_SAMPLE_RATE / DSTAR_RADIO_BLOCK_SIZE;

// --- Network linking ports ---

// UDP port used by the DExtra reflector linking protocol.
const unsigned int  DEXTRA_PORT = 30001U;
// UDP port used by the DCS reflector linking protocol.
const unsigned int  DCS_PORT    = 30051U;

// --- Modem / protocol tuning ---

// Byte count exchanged per USB transaction with the GMSK modem firmware.
const unsigned int  GMSK_MODEM_DATA_LENGTH = 8U;

// Minimum consecutive valid frames required before a radio/local/network
// transmission is considered stable and handed to the repeater logic.
const unsigned int  RADIO_RUN_FRAME_COUNT   = 5U;
const unsigned int  LOCAL_RUN_FRAME_COUNT   = 1U;
const unsigned int  NETWORK_RUN_FRAME_COUNT = 25U;

// Parameters for the end-of-transmission "bleep" courtesy tone.
const unsigned int  DSTAR_BLEEP_FREQ   = 2000U;  // Hz
const unsigned int  DSTAR_BLEEP_LENGTH = 100U;   // ms
const float         DSTAR_BLEEP_AMPL   = 0.5F;   // 0.0–1.0 amplitude

// Seconds of silence from the network gateway before treating the link as lost.
const unsigned int  NETWORK_TIMEOUT = 2U;

// --- Split (half-duplex split site) frame counts ---

// How many frames to buffer on RX/TX sides in split mode.
const unsigned int SPLIT_RX_COUNT = 25U;
const unsigned int SPLIT_TX_COUNT = 5U;

// --- Enums ---

// Repeater main state machine.  Drives the TX/RX loop in the thread classes.
enum DSTAR_RPT_STATE {
	DSRS_SHUTDOWN,       // Repeater is administratively shut down
	DSRS_LISTENING,      // Idle, waiting for a valid D-Star header
	DSRS_VALID,          // Receiving a transmission from a known/allowed callsign
	DSRS_VALID_WAIT,     // Transmission ended; waiting for the tail to clear
	DSRS_INVALID,        // Receiving from an unknown or blocked callsign
	DSRS_INVALID_WAIT,   // Invalid transmission ended; clearing tail
	DSRS_TIMEOUT,        // Transmission exceeded the configured timeout
	DSRS_TIMEOUT_WAIT,   // Timeout tail clearing
	DSRS_NETWORK         // Relaying audio received from the network gateway
};

// Packet type tag returned by the network protocol handler to the repeater thread.
enum NETWORK_TYPE {
	NETWORK_NONE,        // No packet available
	NETWORK_HEADER,      // D-Star link header (start of a network QSO)
	NETWORK_DATA,        // DV frame payload
	NETWORK_TEXT,        // Slow-data text message from the gateway
	NETWORK_TEMPTEXT,    // Temporary/override text message
	NETWORK_STATUS1,     // ircDDB user status slots 1–5
	NETWORK_STATUS2,
	NETWORK_STATUS3,
	NETWORK_STATUS4,
	NETWORK_STATUS5,
	NETWORK_REGISTER     // Gateway registration / keep-alive packet
};

// Operating mode selected in the config file; determines which thread class is
// instantiated and which modem functions are enabled.
enum DSTAR_MODE {
	MODE_DUPLEX,    // Full duplex: separate RX and TX frequencies
	MODE_SIMPLEX,   // Simplex: share a single frequency for RX and TX
	MODE_GATEWAY,   // Gateway-only: no over-air receive, network traffic only
	MODE_TXONLY,    // Transmit only (e.g. beacon node)
	MODE_RXONLY,    // Receive only (e.g. logging node)
	MODE_TXANDRX    // Independent TX and RX paths (split site)
};

// Current state of a reflector or gateway link.
enum LINK_STATUS {
	LS_NONE,               // Not linked
	LS_PENDING_IRCDDB,     // Waiting for ircDDB callsign lookup to complete
	LS_LINKING_LOOPBACK,   // Handshake in progress for each protocol type:
	LS_LINKING_DEXTRA,
	LS_LINKING_DPLUS,
	LS_LINKING_DCS,
	LS_LINKING_CCS,
	LS_LINKED_LOOPBACK,    // Fully linked on each protocol:
	LS_LINKED_DEXTRA,
	LS_LINKED_DPLUS,
	LS_LINKED_DCS,
	LS_LINKED_CCS
};

// What the repeater sends back to the calling station after a transmission.
enum ACK_TYPE {
	AT_NONE,    // No acknowledgement
	AT_BER,     // Bit-error-rate report
	AT_STATUS   // Repeater status message
};

// Language used when generating spoken or text acknowledgement messages.
enum TEXT_LANG {
	TL_ENGLISH_UK,
	TL_DEUTSCH,
	TL_DANSK,
	TL_FRANCAIS,
	TL_ITALIANO,
	TL_POLSKI,
	TL_ESPANOL,
	TL_SVENSKA,
	TL_NEDERLANDS,
	TL_ENGLISH_US,
	TL_NORSK
};

// How the modem hardware is connected to the host.
enum CONNECTION_TYPE {
	CT_USB,
	CT_NETWORK
};

// Which USB backend to use for USB-connected modems.
enum USB_INTERFACE {
	UI_LIBUSB,
	UI_WINUSB
};

// DVMega radio module variant; determines which band(s) are active.
enum DVMEGA_VARIANT {
	DVMV_MODEM,          // Modem-only (external radio)
	DVMV_RADIO_2M,       // Built-in 2 m radio
	DVMV_RADIO_70CM,     // Built-in 70 cm radio
	DVMV_RADIO_2M_70CM   // Dual-band 2 m / 70 cm radio
};

#endif
