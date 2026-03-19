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

#ifndef	DStarRepeaterConfig_H
#define	DStarRepeaterConfig_H

#include "DStarDefines.h"

#include <stdexcept>
#include <string>
#include <vector>

/*
 * Parses the INI-style repeater configuration file.
 *
 * File format (MMDVMHost compatible):
 *   [Section] headers
 *   Key=Value pairs
 *   # comment lines
 *
 * The constructor takes the full path to the config file and throws
 * std::runtime_error if the file does not exist or cannot be opened.
 * There is no write-back facility; this class is read-only.
 *
 * Settings are grouped into logical sections exposed via typed getters.
 * Every getter reads all fields for that group atomically (all by
 * out-reference).
 */
class CDStarRepeaterConfig {
public:
	/*
	 * Opens and parses filePath.  Throws std::runtime_error if the file
	 * does not exist or cannot be opened.  All members are initialised to
	 * built-in defaults before parsing, so any key absent from the file
	 * silently retains its default value.
	 */
	explicit CDStarRepeaterConfig(const std::string& filePath);
	~CDStarRepeaterConfig();

	// [General]
	void getCallsign(std::string& callsign, std::string& gateway, DSTAR_MODE& mode, ACK_TYPE& ack, bool& restriction, bool& rpt1Validation, bool& dtmfBlanking, bool& errorReply) const;

	// [Log]
	void getLog(std::string& filePath, unsigned int& fileLevel, unsigned int& displayLevel, unsigned int& mqttLevel) const;

	// [Paths]
	void getPaths(std::string& dataDir, std::string& audioDir) const;

	// [Network]
	void getNetwork(std::string& gatewayAddress, unsigned int& gatewayPort, std::string& localAddress, unsigned int& localPort, std::string& name) const;

	// [Modem]
	void getModem(std::string& type) const;

	// [Times]
	void getTimes(unsigned int& timeout, unsigned int& ackTime) const;

	// [Beacon]
	void getBeacon(unsigned int& time, std::string& text, bool& voice, TEXT_LANG& language) const;

	// [Announcement]
	void getAnnouncement(bool& enabled, unsigned int& time, std::string& recordRPT1, std::string& recordRPT2, std::string& deleteRPT1, std::string& deleteRPT2) const;

	// [Control]
	void getControl(bool& enabled, std::string& rpt1Callsign, std::string& rpt2Callsign, std::string& shutdown, std::string& startup, std::string& status1, std::string& status2, std::string& status3, std::string& status4, std::string& status5, std::string& command1, std::string& command1Line, std::string& command2, std::string& command2Line, std::string& command3, std::string& command3Line, std::string& command4, std::string& command4Line, std::string& command5, std::string& command5Line, std::string& command6, std::string& command6Line, std::string& output1, std::string& output2, std::string& output3, std::string& output4) const;

	// [Controller]
	void getController(std::string& type, unsigned int& serialConfig, bool& pttInvert, unsigned int& activeHangTime) const;

	// [Outputs]
	void getOutputs(bool& out1, bool& out2, bool& out3, bool& out4) const;

	// [Frame Logging]
	void getLogging(bool& logging) const;

	// [Whitelist] / [Blacklist] / [Greylist]  (empty string = not configured)
	void getWhitelist(std::string& file) const;
	void getBlacklist(std::string& file) const;
	void getGreylist(std::string& file) const;

	// [DVAP]
	void getDVAP(std::string& port, unsigned int& frequency, int& power, int& squelch) const;

	// [GMSK]
	void getGMSK(USB_INTERFACE& type, unsigned int& address) const;

	// [DV-RPTR V1]
	void getDVRPTR1(std::string& port, bool& rxInvert, bool& txInvert, bool& channel, unsigned int& modLevel, unsigned int& txDelay) const;

	// [DV-RPTR V2]
	void getDVRPTR2(CONNECTION_TYPE& connectionType, std::string& usbPort, std::string& address, unsigned int& port, bool& txInvert, unsigned int& modLevel, unsigned int& txDelay) const;

	// [DV-RPTR V3]
	void getDVRPTR3(CONNECTION_TYPE& connectionType, std::string& usbPort, std::string& address, unsigned int& port, bool& txInvert, unsigned int& modLevel, unsigned int& txDelay) const;

	// [DVMEGA]
	void getDVMEGA(std::string& port, DVMEGA_VARIANT& variant, bool& rxInvert, bool& txInvert, unsigned int& txDelay, unsigned int& rxFrequency, unsigned int& txFrequency, unsigned int& power) const;

	// [MMDVM]
	void getMMDVM(std::string& port, bool& rxInvert, bool& txInvert, bool& pttInvert, unsigned int& txDelay, unsigned int& rxLevel, unsigned int& txLevel) const;

	// [Sound Card]
	void getSoundCard(std::string& rxDevice, std::string& txDevice, bool& rxInvert, bool& txInvert, float& rxLevel, float& txLevel, unsigned int& txDelay, unsigned int& txTail) const;

	// [Split]
	void getSplit(std::string& localAddress, unsigned int& localPort, std::vector<std::string>& transmitterNames, std::vector<std::string>& receiverNames, unsigned int& timeout) const;

	// [Icom]
	void getIcom(std::string& port) const;

#if defined(MQTT)
	// [MQTT]
	void getMQTT(std::string& host, unsigned int& port, bool& auth, std::string& username, std::string& password, unsigned int& keepalive, std::string& name) const;
#endif

private:
	// [General]
	std::string   m_callsign;
	std::string   m_gateway;
	DSTAR_MODE    m_mode;
	ACK_TYPE      m_ack;
	bool          m_restriction;
	bool          m_rpt1Validation;
	bool          m_dtmfBlanking;
	bool          m_errorReply;

	// [Log]
	std::string   m_logFilePath;
	unsigned int  m_logFileLevel;
	unsigned int  m_logDisplayLevel;
	unsigned int  m_logMQTTLevel;

	// [Paths]
	std::string   m_dataDir;
	std::string   m_audioDir;

	// [Network]
	std::string   m_gatewayAddress;
	unsigned int  m_gatewayPort;
	std::string   m_localAddress;
	unsigned int  m_localPort;
	std::string   m_networkName;

	// [Modem]
	std::string   m_modemType;

	// [Times]
	unsigned int  m_timeout;
	unsigned int  m_ackTime;

	// [Beacon]
	unsigned int  m_beaconTime;
	std::string   m_beaconText;
	bool          m_beaconVoice;
	TEXT_LANG     m_language;

	// [Announcement]
	bool          m_announcementEnabled;
	unsigned int  m_announcementTime;
	std::string   m_announcementRecordRPT1;
	std::string   m_announcementRecordRPT2;
	std::string   m_announcementDeleteRPT1;
	std::string   m_announcementDeleteRPT2;

	// [Control]
	bool          m_controlEnabled;
	std::string   m_controlRpt1Callsign;
	std::string   m_controlRpt2Callsign;
	std::string   m_controlShutdown;
	std::string   m_controlStartup;
	std::string   m_controlStatus1;
	std::string   m_controlStatus2;
	std::string   m_controlStatus3;
	std::string   m_controlStatus4;
	std::string   m_controlStatus5;
	std::string   m_controlCommand1;
	std::string   m_controlCommand1Line;
	std::string   m_controlCommand2;
	std::string   m_controlCommand2Line;
	std::string   m_controlCommand3;
	std::string   m_controlCommand3Line;
	std::string   m_controlCommand4;
	std::string   m_controlCommand4Line;
	std::string   m_controlCommand5;
	std::string   m_controlCommand5Line;
	std::string   m_controlCommand6;
	std::string   m_controlCommand6Line;
	std::string   m_controlOutput1;
	std::string   m_controlOutput2;
	std::string   m_controlOutput3;
	std::string   m_controlOutput4;

	// [Controller]
	std::string   m_controllerType;
	unsigned int  m_serialConfig;
	bool          m_pttInvert;
	unsigned int  m_activeHangTime;

	// [Outputs]
	bool          m_output1;
	bool          m_output2;
	bool          m_output3;
	bool          m_output4;

	// [Frame Logging]
	bool          m_logging;

	// [Whitelist] / [Blacklist] / [Greylist]  (empty = not configured)
	std::string   m_whitelistFile;
	std::string   m_blacklistFile;
	std::string   m_greylistFile;

	// [DVAP]
	std::string   m_dvapPort;
	unsigned int  m_dvapFrequency;
	int           m_dvapPower;
	int           m_dvapSquelch;

	// [GMSK]
	USB_INTERFACE m_gmskInterface;
	unsigned int  m_gmskAddress;

	// [DV-RPTR V1]
	std::string   m_dvrptr1Port;
	bool          m_dvrptr1RXInvert;
	bool          m_dvrptr1TXInvert;
	bool          m_dvrptr1Channel;
	unsigned int  m_dvrptr1ModLevel;
	unsigned int  m_dvrptr1TXDelay;

	// [DV-RPTR V2]
	CONNECTION_TYPE m_dvrptr2Connection;
	std::string   m_dvrptr2USBPort;
	std::string   m_dvrptr2Address;
	unsigned int  m_dvrptr2Port;
	bool          m_dvrptr2TXInvert;
	unsigned int  m_dvrptr2ModLevel;
	unsigned int  m_dvrptr2TXDelay;

	// [DV-RPTR V3]
	CONNECTION_TYPE m_dvrptr3Connection;
	std::string   m_dvrptr3USBPort;
	std::string   m_dvrptr3Address;
	unsigned int  m_dvrptr3Port;
	bool          m_dvrptr3TXInvert;
	unsigned int  m_dvrptr3ModLevel;
	unsigned int  m_dvrptr3TXDelay;

	// [DVMEGA]
	std::string    m_dvmegaPort;
	DVMEGA_VARIANT m_dvmegaVariant;
	bool           m_dvmegaRXInvert;
	bool           m_dvmegaTXInvert;
	unsigned int   m_dvmegaTXDelay;
	unsigned int   m_dvmegaRXFrequency;
	unsigned int   m_dvmegaTXFrequency;
	unsigned int   m_dvmegaPower;

	// [MMDVM]
	std::string   m_mmdvmPort;
	bool          m_mmdvmRXInvert;
	bool          m_mmdvmTXInvert;
	bool          m_mmdvmPTTInvert;
	unsigned int  m_mmdvmTXDelay;
	unsigned int  m_mmdvmRXLevel;
	unsigned int  m_mmdvmTXLevel;

	// [Sound Card]
	std::string   m_soundCardRXDevice;
	std::string   m_soundCardTXDevice;
	bool          m_soundCardRXInvert;
	bool          m_soundCardTXInvert;
	float         m_soundCardRXLevel;
	float         m_soundCardTXLevel;
	unsigned int  m_soundCardTXDelay;
	unsigned int  m_soundCardTXTail;

	// [Split]
	std::string              m_splitLocalAddress;
	unsigned int             m_splitLocalPort;
	std::vector<std::string> m_splitTXNames;
	std::vector<std::string> m_splitRXNames;
	unsigned int             m_splitTimeout;

	// [Icom]
	std::string   m_icomPort;

#if defined(MQTT)
	// [MQTT]
	std::string   m_mqttHost;
	unsigned int  m_mqttPort;
	bool          m_mqttAuth;
	std::string   m_mqttUsername;
	std::string   m_mqttPassword;
	unsigned int  m_mqttKeepalive;
	std::string   m_mqttName;
#endif
};

#endif
