/*
 *   Copyright (C) 2011,2012,2013,2018 by Jonathan Naylor G4KLX
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

#ifndef	DStarRepeaterApp_H
#define	DStarRepeaterApp_H

#include "DStarRepeaterThread.h"
#include "DStarRepeaterConfig.h"
#include "DStarRepeaterDefs.h"

// Creates the repeater thread based on the configured mode, attaches all
// subsystems (modem, controller, lists, etc.) and launches it.  Returns the
// newly allocated thread on success, or nullptr on failure (caller owns the
// pointer and must join/delete it on exit).
IDStarRepeaterThread* createThread(CDStarRepeaterConfig* config,
                                   const std::string&    audioDir,
                                   std::string           commandLine[6]);

#endif
