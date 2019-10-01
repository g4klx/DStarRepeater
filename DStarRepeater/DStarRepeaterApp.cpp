/*
 *   Copyright (C) 2011-2015,2018 by Jonathan Naylor G4KLX
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

#include <wx/cmdline.h>
#include <wx/filename.h>
#include <wx/event.h>

#include "DStarRepeaterLogRedirect.h"
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
#include "DStarDefines.h"
#include "Version.h"
#include "Logger.h"

wxIMPLEMENT_APP(CDStarRepeaterApp);

wxDEFINE_EVENT(wxEVT_THREAD_COMMAND, wxThreadEvent);

wxBEGIN_EVENT_TABLE(CDStarRepeaterApp, wxApp)
	EVT_THREAD(wxEVT_THREAD_COMMAND, CDStarRepeaterApp::OnRemoteCmd)
wxEND_EVENT_TABLE()

const wxString NAME_PARAM = 		"Repeater Name";
const wxString NOLOGGING_SWITCH =	"nolog";
const wxString DEBUG_SWITCH =		"debug";
const wxString GUI_SWITCH = 		"gui";
const wxString LOGDIR_OPTION =		"logdir";
const wxString CONFDIR_OPTION =		"confdir";
const wxString AUDIODIR_OPTION =	"audiodir";
#if (wxUSE_GUI == 1)
const wxString LOG_BASE_NAME   =	"dstarrepeater";
#else
const wxString LOG_BASE_NAME   =	"dstarrepeaterd";
#endif

CDStarRepeaterApp::CDStarRepeaterApp() :
wxApp(),
#if (wxUSE_GUI == 1)
m_frame(NULL),
#endif
m_name(),
m_nolog(false),
m_debug(false),
m_gui(false),
m_logDir(),
m_confDir(),
m_audioDir(),
m_thread(NULL),
m_config(NULL),
m_checker(NULL),
m_logChain(NULL)
{
}

CDStarRepeaterApp::~CDStarRepeaterApp()
{
}

bool CDStarRepeaterApp::OnInit()
{
	SetVendorName(VENDOR_NAME);

	if (!wxApp::OnInit())
		return false;

	if (!m_nolog) {
		wxString logBaseName = LOG_BASE_NAME;
		if (!m_name.IsEmpty()) {
			logBaseName.Append("_");
			logBaseName.Append(m_name);
		}

#if defined(__WINDOWS__)
		if (m_logDir.IsEmpty())
			m_logDir = ::wxGetHomeDir();
#else
		if (m_logDir.IsEmpty())
			m_logDir = LOG_DIR;
#endif

		try {
			CLogger* log = new CLogger(m_logDir, logBaseName);
			wxLog::SetActiveTarget(log);

			if (m_debug) {
				wxLog::SetVerbose(true);
				wxLog::SetLogLevel(wxLOG_Debug);
			} else {
				wxLog::SetVerbose(false);
				wxLog::SetLogLevel(wxLOG_Message);
			}
		} catch ( const std::runtime_error& e ) {
			wxLog::SetActiveTarget(new wxLogStderr());
			wxLogError("Could not open log file, logging to stderr");
		}
	} else {
		new wxLogNull;
	}

	m_logChain = new wxLogChain(new CDStarRepeaterLogRedirect);

	wxString appName;
	if (!m_name.IsEmpty())
		appName = APPLICATION_NAME + " " + m_name;
	else
		appName = APPLICATION_NAME;

#if !defined(__WINDOWS__)
	appName.Replace(" ", "_");
	m_checker = new wxSingleInstanceChecker(appName, "/tmp");
#else
	m_checker = new wxSingleInstanceChecker(appName);
#endif

	bool ret = m_checker->IsAnotherRunning();
	if (ret) {
		wxLogError("Another copy of the D-Star Repeater is running, exiting");
		return false;
	}

#if defined(__WINDOWS__)
	if (m_confDir.IsEmpty())
		m_confDir = ::wxGetHomeDir();

	m_config = new CDStarRepeaterConfig(new wxConfig(APPLICATION_NAME), m_confDir, CONFIG_FILE_NAME, m_name);
#else
	if (m_confDir.IsEmpty())
		m_confDir = CONF_DIR;

	try {
		m_config = new CDStarRepeaterConfig(m_confDir, CONFIG_FILE_NAME, m_name, true);
	} catch( std::runtime_error& e ) {
		wxLogError("Could not open configuration file");
		return false;
	}
#endif

	wxString type;
	m_config->getModem(type);

#if (wxUSE_GUI == 1)
	wxString frameName = APPLICATION_NAME + " (" + type + ") - ";
	if (!m_name.IsEmpty()) {
		frameName.Append(m_name);
		frameName.Append(" - ");
	}
	frameName.Append(VERSION);

	wxPoint position = wxDefaultPosition;

	int x, y;
	m_config->getPosition(x, y);
	if (x >= 0 && y >= 0)
		position = wxPoint(x, y);

	m_frame = new CDStarRepeaterFrame(frameName, type, position, m_gui);
	m_frame->Show();

	SetTopWindow(m_frame);
#endif

	wxLogInfo("Starting " + APPLICATION_NAME + " - " + VERSION);

	// Log the version of wxWidgets and the Operating System
	wxLogInfo("Using wxWidgets %d.%d.%d on %s", wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER, ::wxGetOsDescription().c_str());

	createThread();

	return true;
}

int CDStarRepeaterApp::OnExit()
{
	wxLogInfo(APPLICATION_NAME + " is exiting");

	m_logChain->SetLog(NULL);

	m_thread->kill();

	delete m_config;

	delete m_checker;

	return 0;
}

void CDStarRepeaterApp::OnInitCmdLine(wxCmdLineParser& parser)
{
	parser.AddSwitch(NOLOGGING_SWITCH, wxEmptyString, wxEmptyString, wxCMD_LINE_PARAM_OPTIONAL);
	parser.AddSwitch(DEBUG_SWITCH,     wxEmptyString, wxEmptyString, wxCMD_LINE_PARAM_OPTIONAL);
	parser.AddSwitch(GUI_SWITCH,       wxEmptyString, wxEmptyString, wxCMD_LINE_PARAM_OPTIONAL);
	parser.AddOption(LOGDIR_OPTION,    wxEmptyString, wxEmptyString, wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
	parser.AddOption(CONFDIR_OPTION,   wxEmptyString, wxEmptyString, wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
	parser.AddOption(AUDIODIR_OPTION,  wxEmptyString, wxEmptyString, wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
	parser.AddParam(NAME_PARAM, wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);

	wxApp::OnInitCmdLine(parser);
}

bool CDStarRepeaterApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
	if (!wxApp::OnCmdLineParsed(parser))
		return false;

	m_nolog = parser.Found(NOLOGGING_SWITCH);
	m_debug = parser.Found(DEBUG_SWITCH);
	m_gui   = parser.Found(GUI_SWITCH);

	wxString logDir;
	bool found = parser.Found(LOGDIR_OPTION, &logDir);
	if (found)
		m_logDir = logDir;

	wxString confDir;
	found = parser.Found(CONFDIR_OPTION, &confDir);
	if (found)
		m_confDir = confDir;

	wxString audioDir;
	found = parser.Found(AUDIODIR_OPTION, &audioDir);
	if (found)
		m_audioDir = audioDir;
	else
// XXX  Homedir here isn't appropriate.  I don't know if logDir is either.
// XXX  Need to look at more code
#if defined(__WINDOWS__)
		m_audioDir = ::wxGetHomeDir();
#else
		m_audioDir = logDir;
#endif

	if (parser.GetParamCount() > 0U)
		m_name = parser.GetParam(0U);

	return true;
}

#if defined(__WXDEBUG__)
void CDStarRepeaterApp::OnAssertFailure(const wxChar* file, int line, const wxChar* func, const wxChar* cond, const wxChar* msg)
{
	wxLogFatalError("Assertion failed on line %d in file %s and function %s: %s %s", line, file, func, cond, msg);
}
#endif

CDStarRepeaterStatusData* CDStarRepeaterApp::getStatus() const
{
	return m_thread->getStatus();
}

void CDStarRepeaterApp::showLog(const wxString& text)
{
#if (wxUSE_GUI == 1)
	if(m_frame)
		m_frame->showLog(text);
#endif
}

void CDStarRepeaterApp::setOutputs(bool out1, bool out2, bool out3, bool out4)
{
	wxLogInfo("Output 1 = %d, output 2 = %d, output 3 = %d, output 4 = %d", int(out1), int(out2), int(out3), int(out4));

	m_thread->setOutputs(out1, out2, out3, out4);
}

void CDStarRepeaterApp::setLogging(bool logging)
{
	wxLogInfo("Frame logging set to %d, in %s", int(logging), m_audioDir.c_str());

	m_thread->setLogging(logging, m_audioDir);
}

void CDStarRepeaterApp::setPosition(int x, int y)
{
	m_config->setPosition(x, y);
	m_config->write();
}

void CDStarRepeaterApp::OnRemoteCmd(wxThreadEvent& event)
{
	wxLogMessage("Request to execute command %s (%d)", m_commandLine[event.GetInt()], event.GetInt());
	// XXX sanity check the command line here.
	wxShell(m_commandLine[event.GetInt()]);
}

void CDStarRepeaterApp::startup()
{
	m_thread->startup();
}

void CDStarRepeaterApp::shutdown()
{
	m_thread->shutdown();
}

void CDStarRepeaterApp::createThread()
{
	wxASSERT(m_config != NULL);

	wxString callsign, gateway;
	DSTAR_MODE mode;
	ACK_TYPE ack;
	bool restriction, rpt1Validation, dtmfBlanking, errorReply;
	m_config->getCallsign(callsign, gateway, mode, ack, restriction, rpt1Validation, dtmfBlanking, errorReply);

	wxString modemType;
	m_config->getModem(modemType);

	// DVAP can only do simplex, force the mode accordingly
	if (modemType.IsSameAs("DVAP") || modemType.IsSameAs(wxT("Icom Access Point/Terminal Mode"))) {
		if (mode == MODE_DUPLEX) {
			wxLogInfo("Changing mode from DUPLEX to SIMPLEX");
			mode = MODE_SIMPLEX;
		} else if (mode == MODE_TXANDRX) {
			wxLogInfo("Changing mode from TX_AND_RX to RX_ONLY");
			mode = MODE_RXONLY;
		}
	}

	//  XXX This should be m_thread eventually.
	switch (mode) {
		case MODE_RXONLY:
			m_thread = new CDStarRepeaterRXThread(modemType);
			break;
		case MODE_TXONLY:
			m_thread = new CDStarRepeaterTXThread(modemType);
			break;
		case MODE_TXANDRX:
			m_thread = new CDStarRepeaterTXRXThread(modemType);
			break;
		default:
			m_thread = new CDStarRepeaterTRXThread(modemType);
			break;
	}

	m_thread->setCallsign(callsign, gateway, mode, ack, restriction, rpt1Validation, dtmfBlanking, errorReply);
	wxLogInfo("Callsign set to \"%s\", gateway set to \"%s\", mode: %d, ack: %d, restriction: %d, RPT1 validation: %d, DTMF blanking: %d, Error reply: %d", callsign.c_str(), gateway.c_str(), int(mode), int(ack), int(restriction), int(rpt1Validation), int(dtmfBlanking), int(errorReply));

	wxString gatewayAddress, localAddress, name;
	unsigned int gatewayPort, localPort;
	m_config->getNetwork(gatewayAddress, gatewayPort, localAddress, localPort, name);
	wxLogInfo("Gateway set to %s:%u, local set to %s:%u, name set to \"%s\"", gatewayAddress.c_str(), gatewayPort, localAddress.c_str(), localPort, name.c_str());

	if (!gatewayAddress.IsEmpty()) {
		bool local = gatewayAddress.IsSameAs("127.0.0.1");

		CRepeaterProtocolHandler* handler = new CRepeaterProtocolHandler(gatewayAddress, gatewayPort, localAddress, localPort, name);

		bool res = handler->open();
		if (!res)
			wxLogError("Cannot open the protocol handler");
		else
			m_thread->setProtocolHandler(handler, local);
	}

	unsigned int timeout, ackTime;
	m_config->getTimes(timeout, ackTime);
	m_thread->setTimes(timeout, ackTime);
	wxLogInfo("Timeout set to %u secs, ack time set to %u ms", timeout, ackTime);

	unsigned int beaconTime;
	wxString beaconText;
	bool beaconVoice;
	TEXT_LANG language;
	m_config->getBeacon(beaconTime, beaconText, beaconVoice, language);
	if (mode == MODE_GATEWAY)
		beaconTime = 0U;
	m_thread->setBeacon(beaconTime, beaconText, beaconVoice, language);
	wxLogInfo("Beacon set to %u mins, text set to \"%s\", voice set to %d, language set to %d", beaconTime / 60U, beaconText.c_str(), int(beaconVoice), int(language));

	bool announcementEnabled;
	unsigned int announcementTime;
	wxString announcementRecordRPT1, announcementRecordRPT2;
	wxString announcementDeleteRPT1, announcementDeleteRPT2;
	m_config->getAnnouncement(announcementEnabled, announcementTime, announcementRecordRPT1, announcementRecordRPT2, announcementDeleteRPT1, announcementDeleteRPT2);
	if (mode == MODE_GATEWAY)
		announcementEnabled = false;
	m_thread->setAnnouncement(announcementEnabled, announcementTime, announcementRecordRPT1, announcementRecordRPT2, announcementDeleteRPT1, announcementDeleteRPT2);
	wxLogInfo("Announcement enabled: %d, time: %u mins, record RPT1: \"%s\", record RPT2: \"%s\", delete RPT1: \"%s\", delete RPT2: \"%s\"", int(announcementEnabled), announcementTime / 60U, announcementRecordRPT1.c_str(), announcementRecordRPT2.c_str(), announcementDeleteRPT1.c_str(), announcementDeleteRPT2.c_str());

	wxLogInfo("Modem type set to \"%s\"", modemType.c_str());

	CModem* modem = NULL;
	if (modemType.IsSameAs("DVAP")) {
		wxString port;
		unsigned int frequency;
		int power, squelch;
		m_config->getDVAP(port, frequency, power, squelch);
		wxLogInfo("DVAP: port: %s, frequency: %u Hz, power: %d dBm, squelch: %d dBm", port.c_str(), frequency, power, squelch);
		modem = new CDVAPController(port, frequency, power, squelch);
	} else if (modemType.IsSameAs("DV-RPTR V1")) {
		wxString port;
		bool rxInvert, txInvert, channel;
		unsigned int modLevel, txDelay;
		m_config->getDVRPTR1(port, rxInvert, txInvert, channel, modLevel, txDelay);
		wxLogInfo("DV-RPTR V1, port: %s, RX invert: %d, TX invert: %d, channel: %s, mod level: %u%%, TX delay: %u ms", port.c_str(), int(rxInvert), int(txInvert), channel ? "B" : "A", modLevel, txDelay);
		modem = new CDVRPTRV1Controller(port, wxEmptyString, rxInvert, txInvert, channel, modLevel, txDelay);
	} else if (modemType.IsSameAs("DV-RPTR V2")) {
		CONNECTION_TYPE connType;
		wxString usbPort, address;
		bool txInvert;
		unsigned int port, modLevel, txDelay;
		m_config->getDVRPTR2(connType, usbPort, address, port, txInvert, modLevel, txDelay);
		wxLogInfo("DV-RPTR V2, type: %d, address: %s:%u, TX invert: %d, mod level: %u%%, TX delay: %u ms", int(connType), address.c_str(), port, int(txInvert), modLevel, txDelay);
		switch (connType) {
			case CT_USB:
				modem = new CDVRPTRV2Controller(usbPort, wxEmptyString, txInvert, modLevel, mode == MODE_DUPLEX || mode == MODE_TXANDRX, callsign, txDelay);
				break;
			case CT_NETWORK:
				modem = new CDVRPTRV2Controller(address, port, txInvert, modLevel, mode == MODE_DUPLEX || mode == MODE_TXANDRX, callsign, txDelay);
				break;
		}
	} else if (modemType.IsSameAs("DV-RPTR V3")) {
		CONNECTION_TYPE connType;
		wxString usbPort, address;
		bool txInvert;
		unsigned int port, modLevel, txDelay;
		m_config->getDVRPTR3(connType, usbPort, address, port, txInvert, modLevel, txDelay);
		wxLogInfo("DV-RPTR V3, type: %d, address: %s:%u, TX invert: %d, mod level: %u%%, TX delay: %u ms", int(connType), address.c_str(), port, int(txInvert), modLevel, txDelay);
		switch (connType) {
			case CT_USB:
				modem = new CDVRPTRV3Controller(usbPort, wxEmptyString, txInvert, modLevel, mode == MODE_DUPLEX || mode == MODE_TXANDRX, callsign, txDelay);
				break;
			case CT_NETWORK:
				modem = new CDVRPTRV3Controller(address, port, txInvert, modLevel, mode == MODE_DUPLEX || mode == MODE_TXANDRX, callsign, txDelay);
				break;
		}
	} else if (modemType.IsSameAs("DVMEGA")) {
		wxString port;
		DVMEGA_VARIANT variant;
		bool rxInvert, txInvert;
		unsigned int txDelay, rxFrequency, txFrequency, power;
		m_config->getDVMEGA(port, variant, rxInvert, txInvert, txDelay, rxFrequency, txFrequency, power);
		wxLogInfo("DVMEGA, port: %s, variant: %d, RX invert: %d, TX invert: %d, TX delay: %u ms, rx frequency: %u Hz, tx frequency: %u Hz, power: %u %%", port.c_str(), int(variant), int(rxInvert), int(txInvert), txDelay, rxFrequency, txFrequency, power);
		switch (variant) {
			case DVMV_MODEM:
				modem = new CDVMegaController(port, wxEmptyString, rxInvert, txInvert, txDelay);
				break;
			case DVMV_RADIO_2M:
			case DVMV_RADIO_70CM:
			case DVMV_RADIO_2M_70CM:
				modem = new CDVMegaController(port, wxEmptyString, txDelay, rxFrequency, txFrequency, power);
				break;
			default:
				wxLogError("Unknown DVMEGA variant - %d", int(variant));
				break;
		}
	} else if (modemType.IsSameAs("GMSK Modem")) {
		USB_INTERFACE iface;
		unsigned int address;
		m_config->getGMSK(iface, address);
		wxLogInfo("GMSK, interface: %d, address: %04X", int(iface), address);
		modem = new CGMSKController(iface, address, mode == MODE_DUPLEX || mode == MODE_TXANDRX);
	} else if (modemType.IsSameAs("Sound Card")) {
		wxString rxDevice, txDevice;
		bool rxInvert, txInvert;
		wxFloat32 rxLevel, txLevel;
		unsigned int txDelay, txTail;
		m_config->getSoundCard(rxDevice, txDevice, rxInvert, txInvert, rxLevel, txLevel, txDelay, txTail);
		wxLogInfo("Sound Card, devices: %s:%s, invert: %d:%d, levels: %.2f:%.2f, tx delay: %u ms, tx tail: %u ms", rxDevice.c_str(), txDevice.c_str(), int(rxInvert), int(txInvert), rxLevel, txLevel, txDelay, txTail);
		modem = new CSoundCardController(rxDevice, txDevice, rxInvert, txInvert, rxLevel, txLevel, txDelay, txTail);
	} else if (modemType.IsSameAs("MMDVM")) {
		wxString port;
		bool rxInvert, txInvert, pttInvert;
		unsigned int txDelay, rxLevel, txLevel;
		m_config->getMMDVM(port, rxInvert, txInvert, pttInvert, txDelay, rxLevel, txLevel);
		wxLogInfo("MMDVM, port: %s, RX invert: %d, TX invert: %d, PTT invert: %d, TX delay: %u ms, RX level: %u%%, TX level: %u%%", port.c_str(), int(rxInvert), int(txInvert), int(pttInvert), txDelay, rxLevel, txLevel);
		modem = new CMMDVMController(port, wxEmptyString, rxInvert, txInvert, pttInvert, txDelay, rxLevel, txLevel);
	} else if (modemType.IsSameAs("Split")) {
		wxString localAddress;
		unsigned int localPort;
		wxArrayString transmitterNames, receiverNames;
		unsigned int timeout;
		m_config->getSplit(localAddress, localPort, transmitterNames, receiverNames, timeout);
		wxLogInfo("Split, local: %s:%u, timeout: %u ms", localAddress.c_str(), localPort, timeout);
		for (unsigned int i = 0U; i < transmitterNames.GetCount(); i++) {
			wxString name = transmitterNames.Item(i);
			if (!name.IsEmpty()) {
				wxLogInfo("\tTX %u name: %s", i + 1U, name.c_str());
			}
		}
		for (unsigned int i = 0U; i < receiverNames.GetCount(); i++) {
			wxString name = receiverNames.Item(i);
			if (!name.IsEmpty()) {
				wxLogInfo("\tRX %u name: %s", i + 1U, name.c_str());
			}
		}
		modem = new CSplitController(localAddress, localPort, transmitterNames, receiverNames, timeout);
	} else if (modemType.IsSameAs("Icom Access Point/Terminal Mode")) {
		wxString port;
		m_config->getIcom(port);
		wxLogInfo("Icom, port: %s", port.c_str());
		modem = new CIcomController(port);
	} else {
		wxLogError("Unknown modem type: %s", modemType.c_str());
	}

	if (modem != NULL) {
		bool res = modem->start();
		if (!res)
			wxLogError("Cannot open the D-Star modem");
		else
			m_thread->setModem(modem);
	}

	wxString controllerType;
	unsigned int portConfig, activeHangTime;
	bool pttInvert;
	m_config->getController(controllerType, portConfig, pttInvert, activeHangTime);
	wxLogInfo("Controller set to %s, config: %u, PTT invert: %d, active hang time: %u ms", controllerType.c_str(), portConfig, int(pttInvert), activeHangTime);

	CExternalController* controller = NULL;

	wxString port;
	if (controllerType.StartsWith("Velleman K8055 - ", &port)) {
		unsigned long num;
		port.ToULong(&num);
		controller = new CExternalController(new CK8055Controller(num), pttInvert);
	} else if (controllerType.StartsWith("URI USB - ", &port)) {
                unsigned long num;
                port.ToULong(&num);
                controller = new CExternalController(new CURIUSBController(num, true), pttInvert);
	} else if (controllerType.StartsWith("Serial - ", &port)) {
		controller = new CExternalController(new CSerialLineController(port, portConfig), pttInvert);
	} else if (controllerType.StartsWith("Arduino - ", &port)) {
		controller = new CExternalController(new CArduinoController(port), pttInvert);
#if defined(GPIO)
	} else if (controllerType.IsSameAs("GPIO")) {
		controller = new CExternalController(new CGPIOController(portConfig), pttInvert);
	} else if (controllerType.IsSameAs(wxT("UDRC"))) {
		switch(portConfig) {
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
		wxLogError("Unrecognized controller %s, using dummy controller", controllerType);
		controller = new CExternalController(new CDummyController, pttInvert);
	}

	bool res = controller->open();
	if (!res)
		wxLogError("Cannot open the hardware interface - %s", controllerType.c_str());
	else
		m_thread->setController(controller, activeHangTime);

	bool out1, out2, out3, out4;
	m_config->getOutputs(out1, out2, out3, out4);
	m_thread->setOutputs(out1, out2, out3, out4);
#if (wxUSE_GUI == 1)
	m_frame->setOutputs(out1, out2, out3, out4);
#endif
	wxLogInfo("Output 1 = %d, output 2 = %d, output 3 = %d, output 4 = %d", int(out1), int(out2), int(out3), int(out4));

	bool enabled;
	wxString rpt1Callsign, rpt2Callsign;
	wxString shutdown, startup;

	//  XXX Initialization should be temporary until we get them coming
	//  from m_config->getControl
	wxArrayString status;
	status.Add("", 5);
	wxArrayString command;
	command.Add("", 6);
	wxArrayString output;
	output.Add("", 4);

	m_config->getControl(enabled, rpt1Callsign, rpt2Callsign, shutdown, startup, status[0], status[1], status[2], status[3], status[4], command[0], m_commandLine[0], command[1], m_commandLine[1], command[2], m_commandLine[2], command[3], m_commandLine[3], command[4], m_commandLine[4], command[5], m_commandLine[5], output[0], output[1], output[2], output[3]);

	m_thread->setControl(enabled, rpt1Callsign, rpt2Callsign, shutdown,
		startup, command, status, output);

	wxLogInfo(wxT("Control: enabled: %d, RPT1: %s, RPT2: %s, shutdown: %s, startup: %s, status1: %s, status2: %s, status3: %s, status4: %s, status5: %s, command1: %s = %s, command2: %s = %s, command3: %s = %s, command4: %s = %s, command5: %s = %s, command6: %s = %s, output1: %s, output2: %s, output3: %s, output4: %s"), enabled, rpt1Callsign.c_str(), rpt2Callsign.c_str(), shutdown.c_str(), startup.c_str(), status[0].c_str(), status[1].c_str(), status[2].c_str(), status[3].c_str(), status[4].c_str(), command[0].c_str(), m_commandLine[0].c_str(), command[1].c_str(), m_commandLine[1].c_str(), command[2].c_str(), m_commandLine[2].c_str(), command[3].c_str(), m_commandLine[3].c_str(), command[4].c_str(), m_commandLine[4].c_str(), command[5].c_str(), m_commandLine[5].c_str(), output[0].c_str(), output[1].c_str(), output[2].c_str(), output[3].c_str());

	bool logging;
	m_config->getLogging(logging);
	m_thread->setLogging(logging, m_audioDir);
#if (wxUSE_GUI == 1)
	m_frame->setLogging(logging);
#endif
	wxLogInfo("Frame logging set to %d, in %s", int(logging), m_audioDir.c_str());

#if defined(__WINDOWS__)
	wxFileName wlFilename(wxFileName::GetHomeDir(), PRIMARY_WHITELIST_FILE_NAME);
#else
	wxFileName wlFilename(CONF_DIR, PRIMARY_WHITELIST_FILE_NAME);
#endif
	bool exists = wlFilename.FileExists();

	if (!exists) {
#if defined(__WINDOWS__)
		wlFilename.Assign(wxFileName::GetHomeDir(), SECONDARY_WHITELIST_FILE_NAME);
#else
		wlFilename.Assign(CONF_DIR, SECONDARY_WHITELIST_FILE_NAME);
#endif
		exists = wlFilename.FileExists();
	}

	if (exists) {
		CCallsignList* list = new CCallsignList(wlFilename.GetFullPath());
		bool res = list->load();
		if (!res) {
			wxLogError("Unable to open white list file - %s", wlFilename.GetFullPath().c_str());
			delete list;
		} else {
			wxLogInfo("%u callsigns loaded into the white list", list->getCount());
			m_thread->setWhiteList(list);
		}
	}
#if defined(__WINDOWS__)
	wxFileName blFilename(wxFileName::GetHomeDir(), PRIMARY_BLACKLIST_FILE_NAME);
#else
	wxFileName blFilename(CONF_DIR, PRIMARY_BLACKLIST_FILE_NAME);
#endif
	exists = blFilename.FileExists();

	if (!exists) {
#if defined(__WINDOWS__)
		blFilename.Assign(wxFileName::GetHomeDir(), SECONDARY_BLACKLIST_FILE_NAME);
#else
		blFilename.Assign(CONF_DIR, SECONDARY_BLACKLIST_FILE_NAME);
#endif
		exists = blFilename.FileExists();
	}

	if (exists) {
		CCallsignList* list = new CCallsignList(blFilename.GetFullPath());
		bool res = list->load();
		if (!res) {
			wxLogError("Unable to open black list file - %s", blFilename.GetFullPath().c_str());
			delete list;
		} else {
			wxLogInfo("%u callsigns loaded into the black list", list->getCount());
			m_thread->setBlackList(list);
		}
	}
#if defined(__WINDOWS__)
	wxFileName glFilename(wxFileName::GetHomeDir(), GREYLIST_FILE_NAME);
#else
		wxFileName glFilename(CONF_DIR, GREYLIST_FILE_NAME);
#endif
	exists = glFilename.FileExists();
	if (exists) {
		CCallsignList* list = new CCallsignList(glFilename.GetFullPath());
		bool res = list->load();
		if (!res) {
			wxLogError("Unable to open grey list file - %s", glFilename.GetFullPath().c_str());
			delete list;
		} else {
			wxLogInfo("%u callsigns loaded into the grey list", list->getCount());
			m_thread->setGreyList(list);
		}
	}

	m_thread->Create();
	m_thread->SetPriority(wxPRIORITY_MAX);
	m_thread->Run();
}
