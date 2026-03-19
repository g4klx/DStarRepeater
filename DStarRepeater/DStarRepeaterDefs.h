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

// Application-level constants for the D-Star Repeater daemon.
// The config file path is now supplied as a required command-line argument
// rather than being constructed from compile-time macros.

#ifndef	DStarRepeaterDefs_H
#define	DStarRepeaterDefs_H

#include "StdCompat.h"

inline const std::string APPLICATION_NAME = "D-Star Repeater";

// Per-frame receive state machine used inside the repeater thread to parse the
// incoming bit stream once a valid header has been detected.
enum DSTAR_RX_STATE {
	DSRXS_LISTENING,        // Waiting for a DATA_SYNC pattern
	DSRXS_PROCESS_DATA,     // Consuming DV frame payload bytes
	DSRXS_PROCESS_SLOW_DATA // Decoding the 3-byte slow-data section of a frame
};

#endif
