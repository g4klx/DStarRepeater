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

/*
 * INI-style configuration parser.
 *
 * File format:
 *   - [Section] headers delimit named groups of settings.
 *   - Lines of the form Key=Value set a parameter within the current section.
 *   - Lines beginning with '#' are comments and are ignored.
 *   - Empty lines are ignored.
 *   - Key lookup is case-sensitive; section names are case-sensitive.
 *
 * Default values are defined as DEFAULT_* constants at the top of this file.
 * Any key absent from the file retains its default value silently.
 */

#include <stdexcept>
#include <fstream>
#include <cstdio>
#include <string>
#include <vector>

#include "DStarRepeaterConfig.h"

// ---------------------------------------------------------------------------
// Section names
// ---------------------------------------------------------------------------

static const std::string SECTION_GENERAL      = "General";
static const std::string SECTION_LOG          = "Log";
static const std::string SECTION_PATHS        = "Paths";
static const std::string SECTION_NETWORK      = "Network";
static const std::string SECTION_MODEM        = "Modem";
static const std::string SECTION_TIMES        = "Times";
static const std::string SECTION_BEACON       = "Beacon";
static const std::string SECTION_ANNOUNCEMENT = "Announcement";
static const std::string SECTION_CONTROL      = "Control";
static const std::string SECTION_CONTROLLER   = "Controller";
static const std::string SECTION_OUTPUTS      = "Outputs";
static const std::string SECTION_FRAMELOGGING = "Frame Logging";
static const std::string SECTION_WHITELIST    = "Whitelist";
static const std::string SECTION_BLACKLIST    = "Blacklist";
static const std::string SECTION_GREYLIST     = "Greylist";
static const std::string SECTION_DVAP         = "DVAP";
static const std::string SECTION_GMSK         = "GMSK";
static const std::string SECTION_DVRPTR1      = "DV-RPTR V1";
static const std::string SECTION_DVRPTR2      = "DV-RPTR V2";
static const std::string SECTION_DVRPTR3      = "DV-RPTR V3";
static const std::string SECTION_DVMEGA       = "DVMEGA";
static const std::string SECTION_MMDVM        = "MMDVM";
static const std::string SECTION_SOUNDCARD    = "Sound Card";
static const std::string SECTION_SPLIT        = "Split";
static const std::string SECTION_ICOM         = "Icom";
#if defined(MQTT)
static const std::string SECTION_MQTT         = "MQTT";
#endif

// ---------------------------------------------------------------------------
// Default values
// ---------------------------------------------------------------------------

// [General]
static const std::string     DEFAULT_CALLSIGN           = "GB3IN  C";
static const std::string     DEFAULT_GATEWAY            = std::string();
static const DSTAR_MODE      DEFAULT_MODE               = MODE_DUPLEX;
static const ACK_TYPE        DEFAULT_ACK                = AT_BER;
static const bool            DEFAULT_RESTRICTION        = false;
static const bool            DEFAULT_RPT1_VALIDATION    = true;
static const bool            DEFAULT_DTMF_BLANKING      = true;
static const bool            DEFAULT_ERROR_REPLY        = true;

// [Log]
static const std::string     DEFAULT_LOG_FILE_PATH      = "/var/log";
static const unsigned int    DEFAULT_LOG_FILE_LEVEL     = 2U;  // LOG_MESSAGE
static const unsigned int    DEFAULT_LOG_DISPLAY_LEVEL  = 2U;  // LOG_MESSAGE
static const unsigned int    DEFAULT_LOG_MQTT_LEVEL     = 2U;  // LOG_MESSAGE

// [Paths]
static const std::string     DEFAULT_DATA_DIR           = "/usr/share/dstarrepeater";
static const std::string     DEFAULT_AUDIO_DIR          = "/var/log";

// [Network]
static const std::string     DEFAULT_GATEWAY_ADDRESS    = "127.0.0.1";
static const unsigned int    DEFAULT_GATEWAY_PORT       = 20010U;
static const std::string     DEFAULT_LOCAL_ADDRESS      = "127.0.0.1";
static const unsigned int    DEFAULT_LOCAL_PORT         = 20011U;
static const std::string     DEFAULT_NETWORK_NAME       = std::string();

// [Modem]
static const std::string     DEFAULT_MODEM_TYPE         = "DVAP";

// [Times]
static const unsigned int    DEFAULT_TIMEOUT            = 180U;
static const unsigned int    DEFAULT_ACK_TIME           = 500U;

// [Beacon]
static const unsigned int    DEFAULT_BEACON_TIME        = 600U;
static const std::string     DEFAULT_BEACON_TEXT        = "D-Star Repeater";
static const bool            DEFAULT_BEACON_VOICE       = false;
static const TEXT_LANG       DEFAULT_LANGUAGE           = TL_ENGLISH_UK;

// [Announcement]
static const bool            DEFAULT_ANNOUNCEMENT_ENABLED     = false;
static const unsigned int    DEFAULT_ANNOUNCEMENT_TIME        = 500U;
static const std::string     DEFAULT_ANNOUNCEMENT_RECORD_RPT1 = std::string();
static const std::string     DEFAULT_ANNOUNCEMENT_RECORD_RPT2 = std::string();
static const std::string     DEFAULT_ANNOUNCEMENT_DELETE_RPT1 = std::string();
static const std::string     DEFAULT_ANNOUNCEMENT_DELETE_RPT2 = std::string();

// [Control]
static const bool            DEFAULT_CONTROL_ENABLED      = false;
static const std::string     DEFAULT_CONTROL_RPT1         = std::string();
static const std::string     DEFAULT_CONTROL_RPT2         = std::string();
static const std::string     DEFAULT_CONTROL_SHUTDOWN     = std::string();
static const std::string     DEFAULT_CONTROL_STARTUP      = std::string();
static const std::string     DEFAULT_CONTROL_STATUS       = std::string();
static const std::string     DEFAULT_CONTROL_COMMAND      = std::string();
static const std::string     DEFAULT_CONTROL_COMMAND_LINE = std::string();
static const std::string     DEFAULT_CONTROL_OUTPUT       = std::string();

// [Controller]
static const std::string     DEFAULT_CONTROLLER_TYPE    = std::string();
static const unsigned int    DEFAULT_SERIAL_CONFIG      = 1U;
static const bool            DEFAULT_PTT_INVERT         = false;
static const unsigned int    DEFAULT_ACTIVE_HANG_TIME   = 0U;

// [Outputs]
static const bool            DEFAULT_OUTPUT             = false;

// [Frame Logging]
static const bool            DEFAULT_LOGGING            = false;

// [DVAP]
static const std::string     DEFAULT_DVAP_PORT          = std::string();
static const unsigned int    DEFAULT_DVAP_FREQUENCY     = 145500000U;
static const int             DEFAULT_DVAP_POWER         = 10;
static const int             DEFAULT_DVAP_SQUELCH       = -100;

// [GMSK]
static const USB_INTERFACE   DEFAULT_GMSK_INTERFACE     = UI_LIBUSB;
static const unsigned int    DEFAULT_GMSK_ADDRESS       = 0x0300U;

// [DV-RPTR V1]
static const std::string     DEFAULT_DVRPTR1_PORT       = std::string();
static const bool            DEFAULT_DVRPTR1_RXINVERT   = false;
static const bool            DEFAULT_DVRPTR1_TXINVERT   = false;
static const bool            DEFAULT_DVRPTR1_CHANNEL    = false;
static const unsigned int    DEFAULT_DVRPTR1_MODLEVEL   = 20U;
static const unsigned int    DEFAULT_DVRPTR1_TXDELAY    = 150U;

// [DV-RPTR V2]
static const CONNECTION_TYPE DEFAULT_DVRPTR2_CONNECTION = CT_USB;
static const std::string     DEFAULT_DVRPTR2_USBPORT    = std::string();
static const std::string     DEFAULT_DVRPTR2_ADDRESS    = "127.0.0.1";
static const unsigned int    DEFAULT_DVRPTR2_PORT       = 0U;
static const bool            DEFAULT_DVRPTR2_TXINVERT   = false;
static const unsigned int    DEFAULT_DVRPTR2_MODLEVEL   = 20U;
static const unsigned int    DEFAULT_DVRPTR2_TXDELAY    = 150U;

// [DV-RPTR V3]
static const CONNECTION_TYPE DEFAULT_DVRPTR3_CONNECTION = CT_USB;
static const std::string     DEFAULT_DVRPTR3_USBPORT    = std::string();
static const std::string     DEFAULT_DVRPTR3_ADDRESS    = "127.0.0.1";
static const unsigned int    DEFAULT_DVRPTR3_PORT       = 0U;
static const bool            DEFAULT_DVRPTR3_TXINVERT   = false;
static const unsigned int    DEFAULT_DVRPTR3_MODLEVEL   = 20U;
static const unsigned int    DEFAULT_DVRPTR3_TXDELAY    = 150U;

// [DVMEGA]
static const std::string     DEFAULT_DVMEGA_PORT        = std::string();
static const DVMEGA_VARIANT  DEFAULT_DVMEGA_VARIANT     = DVMV_MODEM;
static const bool            DEFAULT_DVMEGA_RXINVERT    = false;
static const bool            DEFAULT_DVMEGA_TXINVERT    = false;
static const unsigned int    DEFAULT_DVMEGA_TXDELAY     = 150U;
static const unsigned int    DEFAULT_DVMEGA_RXFREQUENCY = 145500000U;
static const unsigned int    DEFAULT_DVMEGA_TXFREQUENCY = 145500000U;
static const unsigned int    DEFAULT_DVMEGA_POWER       = 100U;

// [MMDVM]
static const std::string     DEFAULT_MMDVM_PORT         = std::string();
static const bool            DEFAULT_MMDVM_RXINVERT     = false;
static const bool            DEFAULT_MMDVM_TXINVERT     = false;
static const bool            DEFAULT_MMDVM_PTTINVERT    = false;
static const unsigned int    DEFAULT_MMDVM_TXDELAY      = 50U;
static const unsigned int    DEFAULT_MMDVM_RXLEVEL      = 100U;
static const unsigned int    DEFAULT_MMDVM_TXLEVEL      = 100U;

// [Sound Card]
static const std::string     DEFAULT_SOUNDCARD_RXDEVICE = std::string();
static const std::string     DEFAULT_SOUNDCARD_TXDEVICE = std::string();
static const bool            DEFAULT_SOUNDCARD_RXINVERT = false;
static const bool            DEFAULT_SOUNDCARD_TXINVERT = false;
static const float           DEFAULT_SOUNDCARD_RXLEVEL  = 1.0F;
static const float           DEFAULT_SOUNDCARD_TXLEVEL  = 1.0F;
static const unsigned int    DEFAULT_SOUNDCARD_TXDELAY  = 150U;
static const unsigned int    DEFAULT_SOUNDCARD_TXTAIL   = 50U;

// [Split]
static const std::string     DEFAULT_SPLIT_LOCALADDRESS = std::string();
static const unsigned int    DEFAULT_SPLIT_LOCALPORT    = 0U;
static const unsigned int    DEFAULT_SPLIT_TIMEOUT      = 0U;

// [Icom]
static const std::string     DEFAULT_ICOM_PORT          = std::string();

#if defined(MQTT)
// [MQTT]
static const std::string     DEFAULT_MQTT_HOST          = "127.0.0.1";
static const unsigned int    DEFAULT_MQTT_PORT          = 1883U;
static const bool            DEFAULT_MQTT_AUTH          = false;
static const std::string     DEFAULT_MQTT_USERNAME      = std::string();
static const std::string     DEFAULT_MQTT_PASSWORD      = std::string();
static const unsigned int    DEFAULT_MQTT_KEEPALIVE     = 60U;
static const std::string     DEFAULT_MQTT_NAME          = "dstar-repeater";
#endif

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

CDStarRepeaterConfig::CDStarRepeaterConfig(const std::string& filePath) :
m_callsign(DEFAULT_CALLSIGN),
m_gateway(DEFAULT_GATEWAY),
m_mode(DEFAULT_MODE),
m_ack(DEFAULT_ACK),
m_restriction(DEFAULT_RESTRICTION),
m_rpt1Validation(DEFAULT_RPT1_VALIDATION),
m_dtmfBlanking(DEFAULT_DTMF_BLANKING),
m_errorReply(DEFAULT_ERROR_REPLY),
m_logFilePath(DEFAULT_LOG_FILE_PATH),
m_logFileLevel(DEFAULT_LOG_FILE_LEVEL),
m_logDisplayLevel(DEFAULT_LOG_DISPLAY_LEVEL),
m_logMQTTLevel(DEFAULT_LOG_MQTT_LEVEL),
m_dataDir(DEFAULT_DATA_DIR),
m_audioDir(DEFAULT_AUDIO_DIR),
m_gatewayAddress(DEFAULT_GATEWAY_ADDRESS),
m_gatewayPort(DEFAULT_GATEWAY_PORT),
m_localAddress(DEFAULT_LOCAL_ADDRESS),
m_localPort(DEFAULT_LOCAL_PORT),
m_networkName(DEFAULT_NETWORK_NAME),
m_modemType(DEFAULT_MODEM_TYPE),
m_timeout(DEFAULT_TIMEOUT),
m_ackTime(DEFAULT_ACK_TIME),
m_beaconTime(DEFAULT_BEACON_TIME),
m_beaconText(DEFAULT_BEACON_TEXT),
m_beaconVoice(DEFAULT_BEACON_VOICE),
m_language(DEFAULT_LANGUAGE),
m_announcementEnabled(DEFAULT_ANNOUNCEMENT_ENABLED),
m_announcementTime(DEFAULT_ANNOUNCEMENT_TIME),
m_announcementRecordRPT1(DEFAULT_ANNOUNCEMENT_RECORD_RPT1),
m_announcementRecordRPT2(DEFAULT_ANNOUNCEMENT_RECORD_RPT2),
m_announcementDeleteRPT1(DEFAULT_ANNOUNCEMENT_DELETE_RPT1),
m_announcementDeleteRPT2(DEFAULT_ANNOUNCEMENT_DELETE_RPT2),
m_controlEnabled(DEFAULT_CONTROL_ENABLED),
m_controlRpt1Callsign(DEFAULT_CONTROL_RPT1),
m_controlRpt2Callsign(DEFAULT_CONTROL_RPT2),
m_controlShutdown(DEFAULT_CONTROL_SHUTDOWN),
m_controlStartup(DEFAULT_CONTROL_STARTUP),
m_controlStatus1(DEFAULT_CONTROL_STATUS),
m_controlStatus2(DEFAULT_CONTROL_STATUS),
m_controlStatus3(DEFAULT_CONTROL_STATUS),
m_controlStatus4(DEFAULT_CONTROL_STATUS),
m_controlStatus5(DEFAULT_CONTROL_STATUS),
m_controlCommand1(DEFAULT_CONTROL_COMMAND),
m_controlCommand1Line(DEFAULT_CONTROL_COMMAND_LINE),
m_controlCommand2(DEFAULT_CONTROL_COMMAND),
m_controlCommand2Line(DEFAULT_CONTROL_COMMAND_LINE),
m_controlCommand3(DEFAULT_CONTROL_COMMAND),
m_controlCommand3Line(DEFAULT_CONTROL_COMMAND_LINE),
m_controlCommand4(DEFAULT_CONTROL_COMMAND),
m_controlCommand4Line(DEFAULT_CONTROL_COMMAND_LINE),
m_controlCommand5(DEFAULT_CONTROL_COMMAND),
m_controlCommand5Line(DEFAULT_CONTROL_COMMAND_LINE),
m_controlCommand6(DEFAULT_CONTROL_COMMAND),
m_controlCommand6Line(DEFAULT_CONTROL_COMMAND_LINE),
m_controlOutput1(DEFAULT_CONTROL_OUTPUT),
m_controlOutput2(DEFAULT_CONTROL_OUTPUT),
m_controlOutput3(DEFAULT_CONTROL_OUTPUT),
m_controlOutput4(DEFAULT_CONTROL_OUTPUT),
m_controllerType(DEFAULT_CONTROLLER_TYPE),
m_serialConfig(DEFAULT_SERIAL_CONFIG),
m_pttInvert(DEFAULT_PTT_INVERT),
m_activeHangTime(DEFAULT_ACTIVE_HANG_TIME),
m_output1(DEFAULT_OUTPUT),
m_output2(DEFAULT_OUTPUT),
m_output3(DEFAULT_OUTPUT),
m_output4(DEFAULT_OUTPUT),
m_logging(DEFAULT_LOGGING),
m_whitelistFile(),
m_blacklistFile(),
m_greylistFile(),
m_dvapPort(DEFAULT_DVAP_PORT),
m_dvapFrequency(DEFAULT_DVAP_FREQUENCY),
m_dvapPower(DEFAULT_DVAP_POWER),
m_dvapSquelch(DEFAULT_DVAP_SQUELCH),
m_gmskInterface(DEFAULT_GMSK_INTERFACE),
m_gmskAddress(DEFAULT_GMSK_ADDRESS),
m_dvrptr1Port(DEFAULT_DVRPTR1_PORT),
m_dvrptr1RXInvert(DEFAULT_DVRPTR1_RXINVERT),
m_dvrptr1TXInvert(DEFAULT_DVRPTR1_TXINVERT),
m_dvrptr1Channel(DEFAULT_DVRPTR1_CHANNEL),
m_dvrptr1ModLevel(DEFAULT_DVRPTR1_MODLEVEL),
m_dvrptr1TXDelay(DEFAULT_DVRPTR1_TXDELAY),
m_dvrptr2Connection(DEFAULT_DVRPTR2_CONNECTION),
m_dvrptr2USBPort(DEFAULT_DVRPTR2_USBPORT),
m_dvrptr2Address(DEFAULT_DVRPTR2_ADDRESS),
m_dvrptr2Port(DEFAULT_DVRPTR2_PORT),
m_dvrptr2TXInvert(DEFAULT_DVRPTR2_TXINVERT),
m_dvrptr2ModLevel(DEFAULT_DVRPTR2_MODLEVEL),
m_dvrptr2TXDelay(DEFAULT_DVRPTR2_TXDELAY),
m_dvrptr3Connection(DEFAULT_DVRPTR3_CONNECTION),
m_dvrptr3USBPort(DEFAULT_DVRPTR3_USBPORT),
m_dvrptr3Address(DEFAULT_DVRPTR3_ADDRESS),
m_dvrptr3Port(DEFAULT_DVRPTR3_PORT),
m_dvrptr3TXInvert(DEFAULT_DVRPTR3_TXINVERT),
m_dvrptr3ModLevel(DEFAULT_DVRPTR3_MODLEVEL),
m_dvrptr3TXDelay(DEFAULT_DVRPTR3_TXDELAY),
m_dvmegaPort(DEFAULT_DVMEGA_PORT),
m_dvmegaVariant(DEFAULT_DVMEGA_VARIANT),
m_dvmegaRXInvert(DEFAULT_DVMEGA_RXINVERT),
m_dvmegaTXInvert(DEFAULT_DVMEGA_TXINVERT),
m_dvmegaTXDelay(DEFAULT_DVMEGA_TXDELAY),
m_dvmegaRXFrequency(DEFAULT_DVMEGA_RXFREQUENCY),
m_dvmegaTXFrequency(DEFAULT_DVMEGA_TXFREQUENCY),
m_dvmegaPower(DEFAULT_DVMEGA_POWER),
m_mmdvmPort(DEFAULT_MMDVM_PORT),
m_mmdvmRXInvert(DEFAULT_MMDVM_RXINVERT),
m_mmdvmTXInvert(DEFAULT_MMDVM_TXINVERT),
m_mmdvmPTTInvert(DEFAULT_MMDVM_PTTINVERT),
m_mmdvmTXDelay(DEFAULT_MMDVM_TXDELAY),
m_mmdvmRXLevel(DEFAULT_MMDVM_RXLEVEL),
m_mmdvmTXLevel(DEFAULT_MMDVM_TXLEVEL),
m_soundCardRXDevice(DEFAULT_SOUNDCARD_RXDEVICE),
m_soundCardTXDevice(DEFAULT_SOUNDCARD_TXDEVICE),
m_soundCardRXInvert(DEFAULT_SOUNDCARD_RXINVERT),
m_soundCardTXInvert(DEFAULT_SOUNDCARD_TXINVERT),
m_soundCardRXLevel(DEFAULT_SOUNDCARD_RXLEVEL),
m_soundCardTXLevel(DEFAULT_SOUNDCARD_TXLEVEL),
m_soundCardTXDelay(DEFAULT_SOUNDCARD_TXDELAY),
m_soundCardTXTail(DEFAULT_SOUNDCARD_TXTAIL),
m_splitLocalAddress(DEFAULT_SPLIT_LOCALADDRESS),
m_splitLocalPort(DEFAULT_SPLIT_LOCALPORT),
m_splitTXNames(),
m_splitRXNames(),
m_splitTimeout(DEFAULT_SPLIT_TIMEOUT),
m_icomPort(DEFAULT_ICOM_PORT)
#if defined(MQTT)
,m_mqttHost(DEFAULT_MQTT_HOST)
,m_mqttPort(DEFAULT_MQTT_PORT)
,m_mqttAuth(DEFAULT_MQTT_AUTH)
,m_mqttUsername(DEFAULT_MQTT_USERNAME)
,m_mqttPassword(DEFAULT_MQTT_PASSWORD)
,m_mqttKeepalive(DEFAULT_MQTT_KEEPALIVE)
,m_mqttName(DEFAULT_MQTT_NAME)
#endif
{
	std::ifstream file(filePath);
	if (!file.is_open())
		throw std::runtime_error("Cannot open configuration file: " + filePath);

	// Pre-size Split arrays at the maximum count; empty slots are ignored later.
	std::string splitTXName[SPLIT_TX_COUNT];
	std::string splitRXName[SPLIT_RX_COUNT];

	std::string section;
	std::string line;
	while (std::getline(file, line)) {
		// Strip trailing carriage-return so Windows-style CRLF files parse cleanly.
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (line.empty() || line[0] == '#')
			continue;

		// Section header: "[Section Name]"
		if (line[0] == '[') {
			std::string::size_type close = line.find(']');
			if (close != std::string::npos)
				section = line.substr(1, close - 1);
			continue;
		}

		// Key=Value pair
		std::string::size_type eq = line.find('=');
		if (eq == std::string::npos)
			continue;

		const std::string key = line.substr(0, eq);
		const std::string val = line.substr(eq + 1U);

		// ---------------------------------------------------------------------------
		// [General]
		// ---------------------------------------------------------------------------
		if (section == SECTION_GENERAL) {
			if (key == "Callsign")
				m_callsign = val;
			else if (key == "Gateway")
				m_gateway = val;
			else if (key == "Mode")
				m_mode = DSTAR_MODE(std::stol(val));
			else if (key == "Ack")
				m_ack = ACK_TYPE(std::stol(val));
			else if (key == "Restriction")
				m_restriction = std::stol(val) == 1L;
			else if (key == "RPT1Validation")
				m_rpt1Validation = std::stol(val) == 1L;
			else if (key == "DTMFBlanking")
				m_dtmfBlanking = std::stol(val) == 1L;
			else if (key == "ErrorReply")
				m_errorReply = std::stol(val) == 1L;

		// ---------------------------------------------------------------------------
		// [Log]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_LOG) {
			if (key == "FilePath")
				m_logFilePath = val;
			else if (key == "FileLevel")
				m_logFileLevel = (unsigned int)std::stoul(val);
			else if (key == "DisplayLevel")
				m_logDisplayLevel = (unsigned int)std::stoul(val);
			else if (key == "MQTTLevel")
				m_logMQTTLevel = (unsigned int)std::stoul(val);

		// ---------------------------------------------------------------------------
		// [Paths]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_PATHS) {
			if (key == "Data")
				m_dataDir = val;
			else if (key == "Audio")
				m_audioDir = val;

		// ---------------------------------------------------------------------------
		// [Network]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_NETWORK) {
			if (key == "GatewayAddress")
				m_gatewayAddress = val;
			else if (key == "GatewayPort")
				m_gatewayPort = (unsigned int)std::stoul(val);
			else if (key == "LocalAddress")
				m_localAddress = val;
			else if (key == "LocalPort")
				m_localPort = (unsigned int)std::stoul(val);
			else if (key == "Name")
				m_networkName = val;

		// ---------------------------------------------------------------------------
		// [Modem]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_MODEM) {
			if (key == "Type")
				m_modemType = val;

		// ---------------------------------------------------------------------------
		// [Times]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_TIMES) {
			if (key == "Timeout")
				m_timeout = (unsigned int)std::stoul(val);
			else if (key == "AckTime")
				m_ackTime = (unsigned int)std::stoul(val);

		// ---------------------------------------------------------------------------
		// [Beacon]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_BEACON) {
			if (key == "Time")
				m_beaconTime = (unsigned int)std::stoul(val);
			else if (key == "Text")
				m_beaconText = val;
			else if (key == "Voice")
				m_beaconVoice = std::stol(val) == 1L;
			else if (key == "Language")
				m_language = TEXT_LANG(std::stol(val));

		// ---------------------------------------------------------------------------
		// [Announcement]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_ANNOUNCEMENT) {
			if (key == "Enabled")
				m_announcementEnabled = std::stol(val) == 1L;
			else if (key == "Time")
				m_announcementTime = (unsigned int)std::stoul(val);
			else if (key == "RecordRPT1")
				m_announcementRecordRPT1 = val;
			else if (key == "RecordRPT2")
				m_announcementRecordRPT2 = val;
			else if (key == "DeleteRPT1")
				m_announcementDeleteRPT1 = val;
			else if (key == "DeleteRPT2")
				m_announcementDeleteRPT2 = val;

		// ---------------------------------------------------------------------------
		// [Control]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_CONTROL) {
			if (key == "Enabled")
				m_controlEnabled = std::stol(val) == 1L;
			else if (key == "RPT1")
				m_controlRpt1Callsign = val;
			else if (key == "RPT2")
				m_controlRpt2Callsign = val;
			else if (key == "Shutdown")
				m_controlShutdown = val;
			else if (key == "Startup")
				m_controlStartup = val;
			else if (key == "Status1")
				m_controlStatus1 = val;
			else if (key == "Status2")
				m_controlStatus2 = val;
			else if (key == "Status3")
				m_controlStatus3 = val;
			else if (key == "Status4")
				m_controlStatus4 = val;
			else if (key == "Status5")
				m_controlStatus5 = val;
			else if (key == "Command1")
				m_controlCommand1 = val;
			else if (key == "Command1Line")
				m_controlCommand1Line = val;
			else if (key == "Command2")
				m_controlCommand2 = val;
			else if (key == "Command2Line")
				m_controlCommand2Line = val;
			else if (key == "Command3")
				m_controlCommand3 = val;
			else if (key == "Command3Line")
				m_controlCommand3Line = val;
			else if (key == "Command4")
				m_controlCommand4 = val;
			else if (key == "Command4Line")
				m_controlCommand4Line = val;
			else if (key == "Command5")
				m_controlCommand5 = val;
			else if (key == "Command5Line")
				m_controlCommand5Line = val;
			else if (key == "Command6")
				m_controlCommand6 = val;
			else if (key == "Command6Line")
				m_controlCommand6Line = val;
			else if (key == "Output1")
				m_controlOutput1 = val;
			else if (key == "Output2")
				m_controlOutput2 = val;
			else if (key == "Output3")
				m_controlOutput3 = val;
			else if (key == "Output4")
				m_controlOutput4 = val;

		// ---------------------------------------------------------------------------
		// [Controller]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_CONTROLLER) {
			if (key == "Type")
				m_controllerType = val;
			else if (key == "SerialConfig")
				m_serialConfig = (unsigned int)std::stoul(val);
			else if (key == "PTTInvert")
				m_pttInvert = std::stol(val) == 1L;
			else if (key == "ActiveHangTime")
				m_activeHangTime = (unsigned int)std::stoul(val);

		// ---------------------------------------------------------------------------
		// [Outputs]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_OUTPUTS) {
			if (key == "Output1")
				m_output1 = std::stol(val) == 1L;
			else if (key == "Output2")
				m_output2 = std::stol(val) == 1L;
			else if (key == "Output3")
				m_output3 = std::stol(val) == 1L;
			else if (key == "Output4")
				m_output4 = std::stol(val) == 1L;

		// ---------------------------------------------------------------------------
		// [Frame Logging]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_FRAMELOGGING) {
			if (key == "Enabled")
				m_logging = std::stol(val) == 1L;

		// ---------------------------------------------------------------------------
		// [Whitelist] / [Blacklist] / [Greylist]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_WHITELIST) {
			if (key == "File")
				m_whitelistFile = val;
		} else if (section == SECTION_BLACKLIST) {
			if (key == "File")
				m_blacklistFile = val;
		} else if (section == SECTION_GREYLIST) {
			if (key == "File")
				m_greylistFile = val;

		// ---------------------------------------------------------------------------
		// [DVAP]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_DVAP) {
			if (key == "Port")
				m_dvapPort = val;
			else if (key == "Frequency")
				m_dvapFrequency = (unsigned int)std::stoul(val);
			else if (key == "Power")
				m_dvapPower = int(std::stol(val));
			else if (key == "Squelch")
				m_dvapSquelch = int(std::stol(val));

		// ---------------------------------------------------------------------------
		// [GMSK]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_GMSK) {
			if (key == "InterfaceType")
				m_gmskInterface = (USB_INTERFACE)std::stoul(val);
			else if (key == "Address")
				m_gmskAddress = (unsigned int)std::stoul(val);

		// ---------------------------------------------------------------------------
		// [DV-RPTR V1]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_DVRPTR1) {
			if (key == "Port")
				m_dvrptr1Port = val;
			else if (key == "RXInvert")
				m_dvrptr1RXInvert = std::stol(val) == 1L;
			else if (key == "TXInvert")
				m_dvrptr1TXInvert = std::stol(val) == 1L;
			else if (key == "Channel")
				m_dvrptr1Channel = std::stol(val) == 1L;
			else if (key == "ModLevel")
				m_dvrptr1ModLevel = (unsigned int)std::stoul(val);
			else if (key == "TXDelay")
				m_dvrptr1TXDelay = (unsigned int)std::stoul(val);

		// ---------------------------------------------------------------------------
		// [DV-RPTR V2]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_DVRPTR2) {
			if (key == "Connection")
				m_dvrptr2Connection = CONNECTION_TYPE(std::stol(val));
			else if (key == "USBPort")
				m_dvrptr2USBPort = val;
			else if (key == "Address")
				m_dvrptr2Address = val;
			else if (key == "Port")
				m_dvrptr2Port = (unsigned int)std::stoul(val);
			else if (key == "TXInvert")
				m_dvrptr2TXInvert = std::stol(val) == 1L;
			else if (key == "ModLevel")
				m_dvrptr2ModLevel = (unsigned int)std::stoul(val);
			else if (key == "TXDelay")
				m_dvrptr2TXDelay = (unsigned int)std::stoul(val);

		// ---------------------------------------------------------------------------
		// [DV-RPTR V3]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_DVRPTR3) {
			if (key == "Connection")
				m_dvrptr3Connection = CONNECTION_TYPE(std::stol(val));
			else if (key == "USBPort")
				m_dvrptr3USBPort = val;
			else if (key == "Address")
				m_dvrptr3Address = val;
			else if (key == "Port")
				m_dvrptr3Port = (unsigned int)std::stoul(val);
			else if (key == "TXInvert")
				m_dvrptr3TXInvert = std::stol(val) == 1L;
			else if (key == "ModLevel")
				m_dvrptr3ModLevel = (unsigned int)std::stoul(val);
			else if (key == "TXDelay")
				m_dvrptr3TXDelay = (unsigned int)std::stoul(val);

		// ---------------------------------------------------------------------------
		// [DVMEGA]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_DVMEGA) {
			if (key == "Port")
				m_dvmegaPort = val;
			else if (key == "Variant")
				m_dvmegaVariant = DVMEGA_VARIANT(std::stol(val));
			else if (key == "RXInvert")
				m_dvmegaRXInvert = std::stol(val) == 1L;
			else if (key == "TXInvert")
				m_dvmegaTXInvert = std::stol(val) == 1L;
			else if (key == "TXDelay")
				m_dvmegaTXDelay = (unsigned int)std::stoul(val);
			else if (key == "RXFrequency")
				m_dvmegaRXFrequency = (unsigned int)std::stoul(val);
			else if (key == "TXFrequency")
				m_dvmegaTXFrequency = (unsigned int)std::stoul(val);
			else if (key == "Power")
				m_dvmegaPower = (unsigned int)std::stoul(val);

		// ---------------------------------------------------------------------------
		// [MMDVM]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_MMDVM) {
			if (key == "Port")
				m_mmdvmPort = val;
			else if (key == "RXInvert")
				m_mmdvmRXInvert = std::stol(val) == 1L;
			else if (key == "TXInvert")
				m_mmdvmTXInvert = std::stol(val) == 1L;
			else if (key == "PTTInvert")
				m_mmdvmPTTInvert = std::stol(val) == 1L;
			else if (key == "TXDelay")
				m_mmdvmTXDelay = (unsigned int)std::stoul(val);
			else if (key == "RXLevel")
				m_mmdvmRXLevel = (unsigned int)std::stoul(val);
			else if (key == "TXLevel")
				m_mmdvmTXLevel = (unsigned int)std::stoul(val);

		// ---------------------------------------------------------------------------
		// [Sound Card]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_SOUNDCARD) {
			if (key == "RXDevice")
				m_soundCardRXDevice = val;
			else if (key == "TXDevice")
				m_soundCardTXDevice = val;
			else if (key == "RXInvert")
				m_soundCardRXInvert = std::stol(val) == 1L;
			else if (key == "TXInvert")
				m_soundCardTXInvert = std::stol(val) == 1L;
			else if (key == "RXLevel")
				m_soundCardRXLevel = float(std::stod(val));
			else if (key == "TXLevel")
				m_soundCardTXLevel = float(std::stod(val));
			else if (key == "TXDelay")
				m_soundCardTXDelay = (unsigned int)std::stoul(val);
			else if (key == "TXTail")
				m_soundCardTXTail = (unsigned int)std::stoul(val);

		// ---------------------------------------------------------------------------
		// [Split]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_SPLIT) {
			if (key == "LocalAddress")
				m_splitLocalAddress = val;
			else if (key == "LocalPort")
				m_splitLocalPort = (unsigned int)std::stoul(val);
			else if (key == "Timeout")
				m_splitTimeout = (unsigned int)std::stoul(val);
			else {
				// TXName0..TXNameN and RXName0..RXNameN
				for (unsigned int i = 0U; i < SPLIT_TX_COUNT; i++) {
					char namebuf[32];
					::snprintf(namebuf, sizeof(namebuf), "TXName%u", i);
					if (key == namebuf) {
						splitTXName[i] = val;
						break;
					}
				}
				for (unsigned int i = 0U; i < SPLIT_RX_COUNT; i++) {
					char namebuf[32];
					::snprintf(namebuf, sizeof(namebuf), "RXName%u", i);
					if (key == namebuf) {
						splitRXName[i] = val;
						break;
					}
				}
			}

		// ---------------------------------------------------------------------------
		// [Icom]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_ICOM) {
			if (key == "Port")
				m_icomPort = val;

#if defined(MQTT)
		// ---------------------------------------------------------------------------
		// [MQTT]
		// ---------------------------------------------------------------------------
		} else if (section == SECTION_MQTT) {
			if (key == "Host")
				m_mqttHost = val;
			else if (key == "Port")
				m_mqttPort = (unsigned int)std::stoul(val);
			else if (key == "Auth")
				m_mqttAuth = std::stol(val) == 1L;
			else if (key == "Username")
				m_mqttUsername = val;
			else if (key == "Password")
				m_mqttPassword = val;
			else if (key == "Keepalive")
				m_mqttKeepalive = (unsigned int)std::stoul(val);
			else if (key == "Name")
				m_mqttName = val;
#endif
		}
	}

	// Move the indexed arrays into vectors.
	for (unsigned int i = 0U; i < SPLIT_TX_COUNT; i++)
		m_splitTXNames.push_back(splitTXName[i]);
	for (unsigned int i = 0U; i < SPLIT_RX_COUNT; i++)
		m_splitRXNames.push_back(splitRXName[i]);
}

CDStarRepeaterConfig::~CDStarRepeaterConfig()
{
}

// ---------------------------------------------------------------------------
// Accessor implementations
// ---------------------------------------------------------------------------

void CDStarRepeaterConfig::getCallsign(std::string& callsign, std::string& gateway, DSTAR_MODE& mode, ACK_TYPE& ack, bool& restriction, bool& rpt1Validation, bool& dtmfBlanking, bool& errorReply) const
{
	callsign       = m_callsign;
	gateway        = m_gateway;
	mode           = m_mode;
	ack            = m_ack;
	restriction    = m_restriction;
	rpt1Validation = m_rpt1Validation;
	dtmfBlanking   = m_dtmfBlanking;
	errorReply     = m_errorReply;
}

void CDStarRepeaterConfig::getLog(std::string& filePath, unsigned int& fileLevel, unsigned int& displayLevel, unsigned int& mqttLevel) const
{
	filePath     = m_logFilePath;
	fileLevel    = m_logFileLevel;
	displayLevel = m_logDisplayLevel;
	mqttLevel    = m_logMQTTLevel;
}

void CDStarRepeaterConfig::getPaths(std::string& dataDir, std::string& audioDir) const
{
	dataDir  = m_dataDir;
	audioDir = m_audioDir;
}

void CDStarRepeaterConfig::getNetwork(std::string& gatewayAddress, unsigned int& gatewayPort, std::string& localAddress, unsigned int& localPort, std::string& name) const
{
	gatewayAddress = m_gatewayAddress;
	gatewayPort    = m_gatewayPort;
	localAddress   = m_localAddress;
	localPort      = m_localPort;
	name           = m_networkName;
}

void CDStarRepeaterConfig::getModem(std::string& type) const
{
	type = m_modemType;
}

void CDStarRepeaterConfig::getTimes(unsigned int& timeout, unsigned int& ackTime) const
{
	timeout = m_timeout;
	ackTime = m_ackTime;
}

void CDStarRepeaterConfig::getBeacon(unsigned int& time, std::string& text, bool& voice, TEXT_LANG& language) const
{
	time     = m_beaconTime;
	text     = m_beaconText;
	voice    = m_beaconVoice;
	language = m_language;
}

void CDStarRepeaterConfig::getAnnouncement(bool& enabled, unsigned int& time, std::string& recordRPT1, std::string& recordRPT2, std::string& deleteRPT1, std::string& deleteRPT2) const
{
	enabled    = m_announcementEnabled;
	time       = m_announcementTime;
	recordRPT1 = m_announcementRecordRPT1;
	recordRPT2 = m_announcementRecordRPT2;
	deleteRPT1 = m_announcementDeleteRPT1;
	deleteRPT2 = m_announcementDeleteRPT2;
}

void CDStarRepeaterConfig::getControl(bool& enabled, std::string& rpt1Callsign, std::string& rpt2Callsign, std::string& shutdown, std::string& startup, std::string& status1, std::string& status2, std::string& status3, std::string& status4, std::string& status5, std::string& command1, std::string& command1Line, std::string& command2, std::string& command2Line, std::string& command3, std::string& command3Line, std::string& command4, std::string& command4Line, std::string& command5, std::string& command5Line, std::string& command6, std::string& command6Line, std::string& output1, std::string& output2, std::string& output3, std::string& output4) const
{
	enabled      = m_controlEnabled;
	rpt1Callsign = m_controlRpt1Callsign;
	rpt2Callsign = m_controlRpt2Callsign;
	shutdown     = m_controlShutdown;
	startup      = m_controlStartup;

	status1      = m_controlStatus1;
	status2      = m_controlStatus2;
	status3      = m_controlStatus3;
	status4      = m_controlStatus4;
	status5      = m_controlStatus5;

	command1     = m_controlCommand1;
	command1Line = m_controlCommand1Line;
	command2     = m_controlCommand2;
	command2Line = m_controlCommand2Line;
	command3     = m_controlCommand3;
	command3Line = m_controlCommand3Line;
	command4     = m_controlCommand4;
	command4Line = m_controlCommand4Line;
	command5     = m_controlCommand5;
	command5Line = m_controlCommand5Line;
	command6     = m_controlCommand6;
	command6Line = m_controlCommand6Line;

	output1      = m_controlOutput1;
	output2      = m_controlOutput2;
	output3      = m_controlOutput3;
	output4      = m_controlOutput4;
}

void CDStarRepeaterConfig::getController(std::string& type, unsigned int& serialConfig, bool& pttInvert, unsigned int& activeHangTime) const
{
	type           = m_controllerType;
	serialConfig   = m_serialConfig;
	pttInvert      = m_pttInvert;
	activeHangTime = m_activeHangTime;
}

void CDStarRepeaterConfig::getOutputs(bool& out1, bool& out2, bool& out3, bool& out4) const
{
	out1 = m_output1;
	out2 = m_output2;
	out3 = m_output3;
	out4 = m_output4;
}

void CDStarRepeaterConfig::getLogging(bool& logging) const
{
	logging = m_logging;
}

void CDStarRepeaterConfig::getWhitelist(std::string& file) const
{
	file = m_whitelistFile;
}

void CDStarRepeaterConfig::getBlacklist(std::string& file) const
{
	file = m_blacklistFile;
}

void CDStarRepeaterConfig::getGreylist(std::string& file) const
{
	file = m_greylistFile;
}

void CDStarRepeaterConfig::getDVAP(std::string& port, unsigned int& frequency, int& power, int& squelch) const
{
	port      = m_dvapPort;
	frequency = m_dvapFrequency;
	power     = m_dvapPower;
	squelch   = m_dvapSquelch;
}

void CDStarRepeaterConfig::getGMSK(USB_INTERFACE& type, unsigned int& address) const
{
	type    = m_gmskInterface;
	address = m_gmskAddress;
}

void CDStarRepeaterConfig::getDVRPTR1(std::string& port, bool& rxInvert, bool& txInvert, bool& channel, unsigned int& modLevel, unsigned int& txDelay) const
{
	port     = m_dvrptr1Port;
	rxInvert = m_dvrptr1RXInvert;
	txInvert = m_dvrptr1TXInvert;
	channel  = m_dvrptr1Channel;
	modLevel = m_dvrptr1ModLevel;
	txDelay  = m_dvrptr1TXDelay;
}

void CDStarRepeaterConfig::getDVRPTR2(CONNECTION_TYPE& connection, std::string& usbPort, std::string& address, unsigned int& port, bool& txInvert, unsigned int& modLevel, unsigned int& txDelay) const
{
	connection = m_dvrptr2Connection;
	usbPort    = m_dvrptr2USBPort;
	address    = m_dvrptr2Address;
	port       = m_dvrptr2Port;
	txInvert   = m_dvrptr2TXInvert;
	modLevel   = m_dvrptr2ModLevel;
	txDelay    = m_dvrptr2TXDelay;
}

void CDStarRepeaterConfig::getDVRPTR3(CONNECTION_TYPE& connection, std::string& usbPort, std::string& address, unsigned int& port, bool& txInvert, unsigned int& modLevel, unsigned int& txDelay) const
{
	connection = m_dvrptr3Connection;
	usbPort    = m_dvrptr3USBPort;
	address    = m_dvrptr3Address;
	port       = m_dvrptr3Port;
	txInvert   = m_dvrptr3TXInvert;
	modLevel   = m_dvrptr3ModLevel;
	txDelay    = m_dvrptr3TXDelay;
}

void CDStarRepeaterConfig::getDVMEGA(std::string& port, DVMEGA_VARIANT& variant, bool& rxInvert, bool& txInvert, unsigned int& txDelay, unsigned int& rxFrequency, unsigned int& txFrequency, unsigned int& power) const
{
	port        = m_dvmegaPort;
	variant     = m_dvmegaVariant;
	rxInvert    = m_dvmegaRXInvert;
	txInvert    = m_dvmegaTXInvert;
	txDelay     = m_dvmegaTXDelay;
	rxFrequency = m_dvmegaRXFrequency;
	txFrequency = m_dvmegaTXFrequency;
	power       = m_dvmegaPower;
}

void CDStarRepeaterConfig::getMMDVM(std::string& port, bool& rxInvert, bool& txInvert, bool& pttInvert, unsigned int& txDelay, unsigned int& rxLevel, unsigned int& txLevel) const
{
	port      = m_mmdvmPort;
	rxInvert  = m_mmdvmRXInvert;
	txInvert  = m_mmdvmTXInvert;
	pttInvert = m_mmdvmPTTInvert;
	txDelay   = m_mmdvmTXDelay;
	rxLevel   = m_mmdvmRXLevel;
	txLevel   = m_mmdvmTXLevel;
}

void CDStarRepeaterConfig::getSoundCard(std::string& rxDevice, std::string& txDevice, bool& rxInvert, bool& txInvert, float& rxLevel, float& txLevel, unsigned int& txDelay, unsigned int& txTail) const
{
	rxDevice = m_soundCardRXDevice;
	txDevice = m_soundCardTXDevice;
	rxInvert = m_soundCardRXInvert;
	txInvert = m_soundCardTXInvert;
	rxLevel  = m_soundCardRXLevel;
	txLevel  = m_soundCardTXLevel;
	txDelay  = m_soundCardTXDelay;
	txTail   = m_soundCardTXTail;
}

void CDStarRepeaterConfig::getSplit(std::string& localAddress, unsigned int& localPort, std::vector<std::string>& transmitterNames, std::vector<std::string>& receiverNames, unsigned int& timeout) const
{
	localAddress     = m_splitLocalAddress;
	localPort        = m_splitLocalPort;
	transmitterNames = m_splitTXNames;
	receiverNames    = m_splitRXNames;
	timeout          = m_splitTimeout;
}

void CDStarRepeaterConfig::getIcom(std::string& port) const
{
	port = m_icomPort;
}

#if defined(MQTT)
void CDStarRepeaterConfig::getMQTT(std::string& host, unsigned int& port, bool& auth, std::string& username, std::string& password, unsigned int& keepalive, std::string& name) const
{
	host      = m_mqttHost;
	port      = m_mqttPort;
	auth      = m_mqttAuth;
	username  = m_mqttUsername;
	password  = m_mqttPassword;
	keepalive = m_mqttKeepalive;
	name      = m_mqttName;
}
#endif
