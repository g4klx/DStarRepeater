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

/*
 * IDStarRepeaterThread is the abstract interface implemented by all repeater
 * thread variants (TRX, TX-only, RX-only, TXRX split).
 *
 * Lifecycle
 * ---------
 * main() (createThread()) selects the concrete subclass,
 * calls the set* configuration methods, then launches the thread by calling
 * entry() inside a std::thread stored in the public m_thread member.  The
 * thread runs until kill() is called, after which main() joins m_thread.
 *
 * Thread ownership
 * ----------------
 * The set* methods are called from the main thread before the worker thread
 * starts, so no locking is needed there.  shutdown() and startup() may be
 * called from the main thread at runtime (e.g. in response to a control
 * command) and must be safe to call concurrently with entry().  getStatus()
 * is likewise called from the main thread to refresh the GUI.
 */

#ifndef	DStarRepeaterThread_H
#define	DStarRepeaterThread_H

#include "DStarRepeaterStatusData.h"
#include "RepeaterProtocolHandler.h"
#include "ExternalController.h"
#include "DStarRepeaterDefs.h"
#include "CallsignList.h"
#include "Modem.h"

#include "StdCompat.h"
#include <thread>

// Classification of a received DV frame, used to decide how to route it.
enum FRAME_TYPE {
	FRAME_NORMAL,  // Regular DV payload frame
	FRAME_SYNC,    // Frame containing a DATA_SYNC marker
	FRAME_END      // End-of-transmission frame
};

class IDStarRepeaterThread {
public:
	IDStarRepeaterThread();

	virtual ~IDStarRepeaterThread() = 0;

	// Called from main() before the thread starts to configure the repeater identity.
	virtual void setCallsign(const std::string& callsign, const std::string& gateway, DSTAR_MODE mode, ACK_TYPE ack, bool restriction, bool rpt1Validation, bool dtmfBlanking, bool errorReply) = 0;

	virtual void setProtocolHandler(CRepeaterProtocolHandler* handler, bool local) = 0;
	virtual void setModem(CModem* modem) = 0;
	virtual void setController(CExternalController* controller, unsigned int activeHangTime) = 0;

	virtual void setTimes(unsigned int timeout, unsigned int ackTime) = 0;

	virtual void setBeacon(unsigned int time, const std::string& text, bool voice, TEXT_LANG language) = 0;
	virtual void setAnnouncement(bool enabled, unsigned int time, const std::string& recordRPT1, const std::string& recordRPT2, const std::string& deleteRPT1, const std::string& deleteRPT2) = 0;
	// setControl has a default no-op so TX/RX-only threads need not implement it.
	virtual void setControl(bool enabled, const std::string& rpt1Callsign,
		const std::string& rpt2Callsign, const std::string& shutdown,
		const std::string& startup, const std::vector<std::string>& command,
		const std::vector<std::string>& commandLine,
		const std::vector<std::string>& status, const std::vector<std::string>& outputs
	) { };
	virtual void setOutputs(bool out1, bool out2, bool out3, bool out4) = 0;
	virtual void setLogging(bool logging, const std::string& dir) = 0;

	virtual void setWhiteList(CCallsignList* list) = 0;
	virtual void setBlackList(CCallsignList* list) = 0;
	virtual void setGreyList(CCallsignList* list) = 0;

	// Administratively shut down or restart the repeater without stopping the thread.
	virtual void shutdown() = 0;
	virtual void startup() = 0;

	// Called from the main thread to poll repeater state for GUI or MQTT reporting.
	virtual CDStarRepeaterStatusData* getStatus() = 0;

	// The thread body: runs the repeater state machine until kill() is called.
	virtual void entry() = 0;

	// Signal the thread to exit its main loop; join m_thread after calling this.
	virtual void kill() = 0;

	// The worker thread, launched by createThread() in the application.
	std::thread m_thread;

private:
};

#endif
