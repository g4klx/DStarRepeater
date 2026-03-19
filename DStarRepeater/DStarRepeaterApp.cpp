/*
 *   Copyright (C) 2011-2015,2018,2025 by Jonathan Naylor G4KLX
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

#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <csignal>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/utsname.h>
#endif

#include "DStarRepeaterTXRXThread.h"
#include "RepeaterProtocolHandler.h"
#include "DStarRepeaterTRXThread.h"
#include "DStarRepeaterTXThread.h"
#include "DStarRepeaterRXThread.h"
#include "SerialLineController.h"
#include "DStarRepeaterThread.h"
#include "SoundCardController.h"
#include "DVRPTRV1Controller.h"
#include "DVRPTRV2Controller.h"
#include "DVRPTRV3Controller.h"
#include "ArduinoController.h"
#include "DVMegaController.h"
#include "DStarRepeaterApp.h"
#include "MMDVMController.h"
#include "URIUSBController.h"
#include "K8055Controller.h"
#include "DummyController.h"
#include "SplitController.h"
#include "IcomController.h"
#if defined(GPIO)
#include "GPIOController.h"
#include "UDRCController.h"
#endif
#include "DVAPController.h"
#include "GMSKController.h"
#include "CallsignList.h"
#include "DStarDefines.h"
#include "Version.h"
#include "Logger.h"
#if defined(MQTT)
#include "MQTTConnection.h"
#endif

// ---------------------------------------------------------------------------
// Signal handling
// ---------------------------------------------------------------------------

static volatile sig_atomic_t g_running = 1;

#if defined(_WIN32)
static BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
        g_running = 0;
        return TRUE;
    }
    return FALSE;
}
#else
static void handleSignal(int /*sig*/)
{
	g_running = 0;
}
#endif

// ---------------------------------------------------------------------------
// createThread -- builds and launches the repeater thread
// ---------------------------------------------------------------------------

IDStarRepeaterThread* createThread(CDStarRepeaterConfig* config,
                                   const std::string&    audioDir,
                                   std::string           commandLine[6])
{
	assert(config != nullptr);

	std::string callsign, gateway;
	DSTAR_MODE  mode;
	ACK_TYPE    ack;
	bool        restriction, rpt1Validation, dtmfBlanking, errorReply;
	config->getCallsign(callsign, gateway, mode, ack, restriction, rpt1Validation, dtmfBlanking, errorReply);

	std::string modemType;
	config->getModem(modemType);

	// DVAP and Icom terminal mode can only do simplex -- adjust accordingly
	if (modemType == "DVAP" || modemType == "Icom Access Point/Terminal Mode") {
		if (mode == MODE_DUPLEX) {
			wxLogInfo("Changing mode from DUPLEX to SIMPLEX");
			mode = MODE_SIMPLEX;
		} else if (mode == MODE_TXANDRX) {
			wxLogInfo("Changing mode from TX_AND_RX to RX_ONLY");
			mode = MODE_RXONLY;
		}
	}

	IDStarRepeaterThread* thread = nullptr;
	switch (mode) {
		case MODE_RXONLY:
			thread = new CDStarRepeaterRXThread(modemType);
			break;
		case MODE_TXONLY:
			thread = new CDStarRepeaterTXThread(modemType);
			break;
		case MODE_TXANDRX:
			thread = new CDStarRepeaterTXRXThread(modemType);
			break;
		default:
			thread = new CDStarRepeaterTRXThread(modemType);
			break;
	}

	thread->setCallsign(callsign, gateway, mode, ack, restriction, rpt1Validation, dtmfBlanking, errorReply);
	wxLogInfo("Callsign set to \"%s\", gateway set to \"%s\", mode: %d, ack: %d, restriction: %d, RPT1 validation: %d, DTMF blanking: %d, Error reply: %d",
		callsign.c_str(), gateway.c_str(), int(mode), int(ack), int(restriction), int(rpt1Validation), int(dtmfBlanking), int(errorReply));

	std::string gatewayAddress, localAddress, netName;
	unsigned int gatewayPort, localPort;
	config->getNetwork(gatewayAddress, gatewayPort, localAddress, localPort, netName);
	wxLogInfo("Gateway set to %s:%u, local set to %s:%u, name set to \"%s\"",
		gatewayAddress.c_str(), gatewayPort, localAddress.c_str(), localPort, netName.c_str());

	if (!gatewayAddress.empty()) {
		bool local = (gatewayAddress == "127.0.0.1");

		CRepeaterProtocolHandler* handler = new CRepeaterProtocolHandler(
			gatewayAddress, gatewayPort, localAddress, localPort, netName);

		bool res = handler->open();
		if (!res) {
			wxLogError("Cannot open the protocol handler");
			delete handler;
		} else {
			thread->setProtocolHandler(handler, local);
		}
	}

	unsigned int timeout, ackTime;
	config->getTimes(timeout, ackTime);
	thread->setTimes(timeout, ackTime);
	wxLogInfo("Timeout set to %u secs, ack time set to %u ms", timeout, ackTime);

	unsigned int beaconTime;
	std::string  beaconText;
	bool         beaconVoice;
	TEXT_LANG    language;
	config->getBeacon(beaconTime, beaconText, beaconVoice, language);
	if (mode == MODE_GATEWAY)
		beaconTime = 0U;
	thread->setBeacon(beaconTime, beaconText, beaconVoice, language);
	wxLogInfo("Beacon set to %u mins, text set to \"%s\", voice set to %d, language set to %d",
		beaconTime / 60U, beaconText.c_str(), int(beaconVoice), int(language));

	bool         announcementEnabled;
	unsigned int announcementTime;
	std::string  announcementRecordRPT1, announcementRecordRPT2;
	std::string  announcementDeleteRPT1, announcementDeleteRPT2;
	config->getAnnouncement(announcementEnabled, announcementTime,
		announcementRecordRPT1, announcementRecordRPT2,
		announcementDeleteRPT1, announcementDeleteRPT2);
	if (mode == MODE_GATEWAY)
		announcementEnabled = false;
	thread->setAnnouncement(announcementEnabled, announcementTime,
		announcementRecordRPT1, announcementRecordRPT2,
		announcementDeleteRPT1, announcementDeleteRPT2);
	wxLogInfo("Announcement enabled: %d, time: %u mins, record RPT1: \"%s\", record RPT2: \"%s\", delete RPT1: \"%s\", delete RPT2: \"%s\"",
		int(announcementEnabled), announcementTime / 60U,
		announcementRecordRPT1.c_str(), announcementRecordRPT2.c_str(),
		announcementDeleteRPT1.c_str(), announcementDeleteRPT2.c_str());

	wxLogInfo("Modem type set to \"%s\"", modemType.c_str());

	CModem* modem = nullptr;

	if (modemType == "DVAP") {
		std::string  port;
		unsigned int frequency;
		int          power, squelch;
		config->getDVAP(port, frequency, power, squelch);
		wxLogInfo("DVAP: port: %s, frequency: %u Hz, power: %d dBm, squelch: %d dBm",
			port.c_str(), frequency, power, squelch);
		modem = new CDVAPController(port, frequency, power, squelch);

	} else if (modemType == "DV-RPTR V1") {
		std::string  port;
		bool         rxInvert, txInvert, channel;
		unsigned int modLevel, txDelay;
		config->getDVRPTR1(port, rxInvert, txInvert, channel, modLevel, txDelay);
		wxLogInfo("DV-RPTR V1, port: %s, RX invert: %d, TX invert: %d, channel: %s, mod level: %u%%, TX delay: %u ms",
			port.c_str(), int(rxInvert), int(txInvert), channel ? "B" : "A", modLevel, txDelay);
		modem = new CDVRPTRV1Controller(port, std::string(), rxInvert, txInvert, channel, modLevel, txDelay);

	} else if (modemType == "DV-RPTR V2") {
		CONNECTION_TYPE connType;
		std::string     usbPort, address;
		bool            txInvert;
		unsigned int    port, modLevel, txDelay;
		config->getDVRPTR2(connType, usbPort, address, port, txInvert, modLevel, txDelay);
		wxLogInfo("DV-RPTR V2, type: %d, address: %s:%u, TX invert: %d, mod level: %u%%, TX delay: %u ms",
			int(connType), address.c_str(), port, int(txInvert), modLevel, txDelay);
		bool duplex = (mode == MODE_DUPLEX || mode == MODE_TXANDRX);
		switch (connType) {
			case CT_USB:
				modem = new CDVRPTRV2Controller(usbPort, std::string(), txInvert, modLevel, duplex, callsign, txDelay);
				break;
			case CT_NETWORK:
				modem = new CDVRPTRV2Controller(address, port, txInvert, modLevel, duplex, callsign, txDelay);
				break;
		}

	} else if (modemType == "DV-RPTR V3") {
		CONNECTION_TYPE connType;
		std::string     usbPort, address;
		bool            txInvert;
		unsigned int    port, modLevel, txDelay;
		config->getDVRPTR3(connType, usbPort, address, port, txInvert, modLevel, txDelay);
		wxLogInfo("DV-RPTR V3, type: %d, address: %s:%u, TX invert: %d, mod level: %u%%, TX delay: %u ms",
			int(connType), address.c_str(), port, int(txInvert), modLevel, txDelay);
		bool duplex = (mode == MODE_DUPLEX || mode == MODE_TXANDRX);
		switch (connType) {
			case CT_USB:
				modem = new CDVRPTRV3Controller(usbPort, std::string(), txInvert, modLevel, duplex, callsign, txDelay);
				break;
			case CT_NETWORK:
				modem = new CDVRPTRV3Controller(address, port, txInvert, modLevel, duplex, callsign, txDelay);
				break;
		}

	} else if (modemType == "DVMEGA") {
		std::string    port;
		DVMEGA_VARIANT variant;
		bool           rxInvert, txInvert;
		unsigned int   txDelay, rxFrequency, txFrequency, power;
		config->getDVMEGA(port, variant, rxInvert, txInvert, txDelay, rxFrequency, txFrequency, power);
		wxLogInfo("DVMEGA, port: %s, variant: %d, RX invert: %d, TX invert: %d, TX delay: %u ms, rx frequency: %u Hz, tx frequency: %u Hz, power: %u %%",
			port.c_str(), int(variant), int(rxInvert), int(txInvert), txDelay, rxFrequency, txFrequency, power);
		switch (variant) {
			case DVMV_MODEM:
				modem = new CDVMegaController(port, std::string(), rxInvert, txInvert, txDelay);
				break;
			case DVMV_RADIO_2M:
			case DVMV_RADIO_70CM:
			case DVMV_RADIO_2M_70CM:
				modem = new CDVMegaController(port, std::string(), txDelay, rxFrequency, txFrequency, power);
				break;
			default:
				wxLogError("Unknown DVMEGA variant - %d", int(variant));
				break;
		}

	} else if (modemType == "GMSK Modem") {
		USB_INTERFACE iface;
		unsigned int  address;
		config->getGMSK(iface, address);
		wxLogInfo("GMSK, interface: %d, address: %04X", int(iface), address);
		modem = new CGMSKController(iface, address, mode == MODE_DUPLEX || mode == MODE_TXANDRX);

	} else if (modemType == "Sound Card") {
		std::string rxDevice, txDevice;
		bool        rxInvert, txInvert;
		float       rxLevel, txLevel;
		unsigned int txDelay, txTail;
		config->getSoundCard(rxDevice, txDevice, rxInvert, txInvert, rxLevel, txLevel, txDelay, txTail);
		wxLogInfo("Sound Card, devices: %s:%s, invert: %d:%d, levels: %.2f:%.2f, tx delay: %u ms, tx tail: %u ms",
			rxDevice.c_str(), txDevice.c_str(), int(rxInvert), int(txInvert), rxLevel, txLevel, txDelay, txTail);
		modem = new CSoundCardController(rxDevice, txDevice, rxInvert, txInvert, rxLevel, txLevel, txDelay, txTail);

	} else if (modemType == "MMDVM") {
		std::string  port;
		bool         rxInvert, txInvert, pttInvert;
		unsigned int txDelay, rxLevel, txLevel;
		config->getMMDVM(port, rxInvert, txInvert, pttInvert, txDelay, rxLevel, txLevel);
		wxLogInfo("MMDVM, port: %s, RX invert: %d, TX invert: %d, PTT invert: %d, TX delay: %u ms, RX level: %u%%, TX level: %u%%",
			port.c_str(), int(rxInvert), int(txInvert), int(pttInvert), txDelay, rxLevel, txLevel);
		modem = new CMMDVMController(port, std::string(), rxInvert, txInvert, pttInvert, txDelay, rxLevel, txLevel);

	} else if (modemType == "Split") {
		std::string              localAddr;
		unsigned int             localP;
		std::vector<std::string> transmitterNames, receiverNames;
		unsigned int             splitTimeout;
		config->getSplit(localAddr, localP, transmitterNames, receiverNames, splitTimeout);
		wxLogInfo("Split, local: %s:%u, timeout: %u ms", localAddr.c_str(), localP, splitTimeout);
		for (unsigned int i = 0U; i < transmitterNames.size(); i++) {
			if (!transmitterNames[i].empty())
				wxLogInfo("\tTX %u name: %s", i + 1U, transmitterNames[i].c_str());
		}
		for (unsigned int i = 0U; i < receiverNames.size(); i++) {
			if (!receiverNames[i].empty())
				wxLogInfo("\tRX %u name: %s", i + 1U, receiverNames[i].c_str());
		}
		modem = new CSplitController(localAddr, localP, transmitterNames, receiverNames, splitTimeout);

	} else if (modemType == "Icom Access Point/Terminal Mode") {
		std::string port;
		config->getIcom(port);
		wxLogInfo("Icom, port: %s", port.c_str());
		modem = new CIcomController(port);

	} else {
		wxLogError("Unknown modem type: %s", modemType.c_str());
	}

	if (modem != nullptr) {
		bool res = modem->start();
		if (!res) {
			wxLogError("Cannot open the D-Star modem");
			delete modem;
		} else {
			thread->setModem(modem);
		}
	}

	std::string  controllerType;
	unsigned int portConfig, activeHangTime;
	bool         pttInvert;
	config->getController(controllerType, portConfig, pttInvert, activeHangTime);
	wxLogInfo("Controller set to %s, config: %u, PTT invert: %d, active hang time: %u ms",
		controllerType.c_str(), portConfig, int(pttInvert), activeHangTime);

	CExternalController* controller = nullptr;

	const std::string PREFIX_K8055   = "Velleman K8055 - ";
	const std::string PREFIX_URIUSB  = "URI USB - ";
	const std::string PREFIX_SERIAL  = "Serial - ";
	const std::string PREFIX_ARDUINO = "Arduino - ";

	auto startsWith = [](const std::string& s, const std::string& prefix, std::string& rest) -> bool {
		if (s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0) {
			rest = s.substr(prefix.size());
			return true;
		}
		return false;
	};

	std::string portStr;
	if (startsWith(controllerType, PREFIX_K8055, portStr)) {
		unsigned long num = std::stoul(portStr);
		controller = new CExternalController(new CK8055Controller(num), pttInvert);
	} else if (startsWith(controllerType, PREFIX_URIUSB, portStr)) {
		unsigned long num = std::stoul(portStr);
		controller = new CExternalController(new CURIUSBController(num, true), pttInvert);
	} else if (startsWith(controllerType, PREFIX_SERIAL, portStr)) {
		controller = new CExternalController(new CSerialLineController(portStr, portConfig), pttInvert);
	} else if (startsWith(controllerType, PREFIX_ARDUINO, portStr)) {
		controller = new CExternalController(new CArduinoController(portStr), pttInvert);
#if defined(GPIO)
	} else if (controllerType == "GPIO") {
		controller = new CExternalController(new CGPIOController(portConfig), pttInvert);
	} else if (controllerType == "UDRC") {
		switch (portConfig) {
			case 1:
				controller = new CUDRCController(AUTO_FM);
				break;
			case 3:
				controller = new CUDRCController(DIGITAL_DIGITAL);
				break;
			case 4:
				controller = new CUDRCController(FM_FM);
				break;
			case 5:
				controller = new CUDRCController(HOTSPOT);
				break;
			default:
			case 2:
				controller = new CUDRCController(AUTO_AUTO);
				break;
		}
#endif
	} else {
		wxLogError("Unrecognized controller %s, using dummy controller", controllerType.c_str());
		controller = new CExternalController(new CDummyController, pttInvert);
	}

	bool res = controller->open();
	if (!res) {
		wxLogError("Cannot open the hardware interface - %s", controllerType.c_str());
		delete controller;
	} else {
		thread->setController(controller, activeHangTime);
	}

	bool out1, out2, out3, out4;
	config->getOutputs(out1, out2, out3, out4);
	thread->setOutputs(out1, out2, out3, out4);
	wxLogInfo("Output 1 = %d, output 2 = %d, output 3 = %d, output 4 = %d",
		int(out1), int(out2), int(out3), int(out4));

	bool        controlEnabled;
	std::string rpt1Callsign, rpt2Callsign, ctrlShutdown, ctrlStartup;
	std::vector<std::string> status(5);
	std::vector<std::string> command(6);
	std::vector<std::string> output(4);

	config->getControl(controlEnabled, rpt1Callsign, rpt2Callsign, ctrlShutdown, ctrlStartup,
		status[0], status[1], status[2], status[3], status[4],
		command[0], commandLine[0],
		command[1], commandLine[1],
		command[2], commandLine[2],
		command[3], commandLine[3],
		command[4], commandLine[4],
		command[5], commandLine[5],
		output[0], output[1], output[2], output[3]);

	std::vector<std::string> commandLineVec(commandLine, commandLine + 6);
	thread->setControl(controlEnabled, rpt1Callsign, rpt2Callsign, ctrlShutdown,
		ctrlStartup, command, commandLineVec, status, output);

	wxLogInfo("Control: enabled: %d, RPT1: %s, RPT2: %s, shutdown: %s, startup: %s, "
		"status1: %s, status2: %s, status3: %s, status4: %s, status5: %s, "
		"command1: %s = %s, command2: %s = %s, command3: %s = %s, "
		"command4: %s = %s, command5: %s = %s, command6: %s = %s, "
		"output1: %s, output2: %s, output3: %s, output4: %s",
		int(controlEnabled),
		rpt1Callsign.c_str(), rpt2Callsign.c_str(),
		ctrlShutdown.c_str(), ctrlStartup.c_str(),
		status[0].c_str(), status[1].c_str(), status[2].c_str(), status[3].c_str(), status[4].c_str(),
		command[0].c_str(), commandLine[0].c_str(),
		command[1].c_str(), commandLine[1].c_str(),
		command[2].c_str(), commandLine[2].c_str(),
		command[3].c_str(), commandLine[3].c_str(),
		command[4].c_str(), commandLine[4].c_str(),
		command[5].c_str(), commandLine[5].c_str(),
		output[0].c_str(), output[1].c_str(), output[2].c_str(), output[3].c_str());

	bool logging;
	config->getLogging(logging);
	thread->setLogging(logging, audioDir);
	wxLogInfo("Frame logging set to %d, in %s", int(logging), audioDir.c_str());

	// White list
	{
		std::string wlFile;
		config->getWhitelist(wlFile);
		if (!wlFile.empty() && access(wlFile.c_str(), F_OK) == 0) {
			CCallsignList* list = new CCallsignList(wlFile);
			bool ok = list->load();
			if (!ok) {
				wxLogError("Unable to open white list file - %s", wlFile.c_str());
				delete list;
			} else {
				wxLogInfo("%u callsigns loaded into the white list", list->getCount());
				thread->setWhiteList(list);
			}
		}
	}

	// Black list
	{
		std::string blFile;
		config->getBlacklist(blFile);
		if (!blFile.empty() && access(blFile.c_str(), F_OK) == 0) {
			CCallsignList* list = new CCallsignList(blFile);
			bool ok = list->load();
			if (!ok) {
				wxLogError("Unable to open black list file - %s", blFile.c_str());
				delete list;
			} else {
				wxLogInfo("%u callsigns loaded into the black list", list->getCount());
				thread->setBlackList(list);
			}
		}
	}

	// Grey list
	{
		std::string glFile;
		config->getGreylist(glFile);
		if (!glFile.empty() && access(glFile.c_str(), F_OK) == 0) {
			CCallsignList* list = new CCallsignList(glFile);
			bool ok = list->load();
			if (!ok) {
				wxLogError("Unable to open grey list file - %s", glFile.c_str());
				delete list;
			} else {
				wxLogInfo("%u callsigns loaded into the grey list", list->getCount());
				thread->setGreyList(list);
			}
		}
	}

	// Launch the repeater thread (virtual dispatch through lambda)
	thread->m_thread = std::thread([thread]() { thread->entry(); });

	return thread;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
	if (argc < 2) {
		std::fprintf(stderr, "Usage: %s <config-file>\n", argv[0]);
		return 1;
	}
	const std::string configFile = argv[1];

	// -----------------------------------------------------------------------
	// Configuration -- must be loaded before logger so we can read [Log]
	// -----------------------------------------------------------------------

	CDStarRepeaterConfig* config = nullptr;
	try {
		config = new CDStarRepeaterConfig(configFile);
	} catch (const std::exception& e) {
		std::fprintf(stderr, "Could not open configuration file %s: %s\n",
			configFile.c_str(), e.what());
		return 1;
	}

	// -----------------------------------------------------------------------
	// Logging -- driven entirely by [Log] in the config file
	// -----------------------------------------------------------------------

	{
		std::string  logFilePath;
		unsigned int logFileLevel, logDisplayLevel, logMQTTLevel;
		config->getLog(logFilePath, logFileLevel, logDisplayLevel, logMQTTLevel);

#if defined(MQTT)
		g_mqttLevel = logMQTTLevel;
#endif

		const bool wantFile    = (logFileLevel    != 0U);
		const bool wantDisplay = (logDisplayLevel != 0U);

		if (wantFile || wantDisplay) {
			const std::string logBaseName = "dstarrepeaterd";
			// Pass empty directory when file logging is disabled so the logger
			// skips opening a file while still writing to stdout.
			const std::string logDir = wantFile ? logFilePath : std::string();
			try {
				CLogger::setInstance(new CLogger(logDir, logBaseName, logFileLevel, logDisplayLevel));
			} catch (const std::exception& e) {
				std::fprintf(stderr, "Could not open log file in %s: %s -- logging to stderr only\n",
					logFilePath.c_str(), e.what());
			}
		}
		// When both levels are 0, no CLogger is installed; the wxLogXxx macros
		// silently drop output because getInstance() returns nullptr.
	}

	// -----------------------------------------------------------------------
	// Single-instance PID file lock
	// -----------------------------------------------------------------------

	std::string pidName = APPLICATION_NAME;
	for (char& ch : pidName)
		if (ch == ' ') ch = '_';

#if defined(_WIN32)
	HANDLE hMutex = CreateMutexA(nullptr, TRUE, pidName.c_str());
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		wxLogError("Another copy of the D-Star Repeater is running, exiting");
		delete config;
		return 1;
	}
#else
	std::string pidPath = "/var/run/" + pidName + ".pid";
#ifdef O_NOFOLLOW
	int pidFd = open(pidPath.c_str(), O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
#else
	int pidFd = open(pidPath.c_str(), O_RDWR | O_CREAT, 0600);
#endif
	if (pidFd < 0) {
		wxLogError("Cannot create PID file %s", pidPath.c_str());
		delete config;
		return 1;
	}
	if (flock(pidFd, LOCK_EX | LOCK_NB) < 0) {
		wxLogError("Another copy of the D-Star Repeater is running, exiting");
		close(pidFd);
		delete config;
		return 1;
	}

	// Write our PID to the file for service managers
	char pidBuf[16];
	int pidLen = ::snprintf(pidBuf, sizeof(pidBuf), "%d\n", (int)::getpid());
	if (::ftruncate(pidFd, 0) == 0) {
		ssize_t ret = ::write(pidFd, pidBuf, pidLen);
		if (ret < 0)
			::fprintf(stderr, "Failed to write PID file\n");
	}
#endif

	// -----------------------------------------------------------------------
	// OS identification
	// -----------------------------------------------------------------------

#if defined(_WIN32)
	wxLogInfo("Running on Windows");
#else
	struct utsname unameInfo;
	if (uname(&unameInfo) == 0) {
		wxLogInfo("Using %s %s on %s", unameInfo.sysname, unameInfo.release, unameInfo.machine);
	}
#endif

	wxLogInfo("Starting %s - %s", APPLICATION_NAME.c_str(), VERSION.c_str());
	wxLogInfo("Config file: %s", configFile.c_str());

	// -----------------------------------------------------------------------
	// MQTT (optional)
	// -----------------------------------------------------------------------

#if defined(MQTT)
	{
		std::string  mqttHost, mqttUsername, mqttPassword, mqttName;
		unsigned int mqttPort, mqttKeepalive;
		bool         mqttAuth;
		config->getMQTT(mqttHost, mqttPort, mqttAuth, mqttUsername, mqttPassword, mqttKeepalive, mqttName);

		if (!mqttHost.empty()) {
			std::vector<std::pair<std::string, void (*)(const unsigned char*, unsigned int)>> subscriptions;

			g_mqtt = new CMQTTConnection(
				mqttHost,
				static_cast<unsigned short>(mqttPort),
				mqttName,
				mqttAuth,
				mqttUsername,
				mqttPassword,
				subscriptions,
				mqttKeepalive);

			bool ok = g_mqtt->open();
			if (!ok) {
				wxLogError("Unable to start MQTT connection to %s:%u", mqttHost.c_str(), mqttPort);
				delete g_mqtt;
				g_mqtt = nullptr;
			} else {
				wxLogInfo("MQTT connected to %s:%u as %s", mqttHost.c_str(), mqttPort, mqttName.c_str());
			}
		}
	}
#endif

	// -----------------------------------------------------------------------
	// Audio directory (from config [Paths])
	// -----------------------------------------------------------------------

	std::string dataDir, audioDir;
	config->getPaths(dataDir, audioDir);
	(void)dataDir;  // DATA_DIR is baked in at compile time for BeaconUnit

	// -----------------------------------------------------------------------
	// Signal handlers
	// -----------------------------------------------------------------------

#if defined(_WIN32)
	SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
	struct sigaction sa;
	std::memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handleSignal;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, nullptr);
	sigaction(SIGINT,  &sa, nullptr);
#endif

	// -----------------------------------------------------------------------
	// Create and run the repeater thread
	// -----------------------------------------------------------------------

	std::string commandLine[6];
	IDStarRepeaterThread* thread = nullptr;
	try {
		thread = createThread(config, audioDir, commandLine);
	} catch (const std::exception& e) {
		wxLogError("Failed to create repeater thread: %s", e.what());
		delete config;
#if defined(MQTT)
		if (g_mqtt != nullptr) {
			g_mqtt->close();
			delete g_mqtt;
			g_mqtt = nullptr;
		}
#endif
#if defined(_WIN32)
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
#else
		flock(pidFd, LOCK_UN);
		close(pidFd);
		unlink(pidPath.c_str());
#endif
		delete CLogger::getInstance();
		CLogger::setInstance(nullptr);
		return 1;
	}

	// -----------------------------------------------------------------------
	// Main loop -- block until signalled
	// -----------------------------------------------------------------------

	while (g_running)
		std::this_thread::sleep_for(std::chrono::seconds(1));

	// -----------------------------------------------------------------------
	// Shutdown
	// -----------------------------------------------------------------------

	wxLogInfo("%s is exiting", APPLICATION_NAME.c_str());

	thread->kill();
	thread->m_thread.join();
	delete thread;

#if defined(MQTT)
	if (g_mqtt != nullptr) {
		g_mqtt->close();
		delete g_mqtt;
		g_mqtt = nullptr;
	}
#endif

	delete config;

#if defined(_WIN32)
	ReleaseMutex(hMutex);
	CloseHandle(hMutex);
#else
	flock(pidFd, LOCK_UN);
	close(pidFd);
	unlink(pidPath.c_str());
#endif

	return 0;
}
