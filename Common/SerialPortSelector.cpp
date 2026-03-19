/*
 *   Copyright (C) 2002-2004,2007-2011,2013,2015 by Jonathan Naylor G4KLX
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

#include "SerialPortSelector.h"

#if !defined(_WIN32)
#include <sys/types.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>
#endif

#if defined(__APPLE__) && defined(__MACH__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOBSD.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <termios.h>
#endif


std::vector<std::string> CSerialPortSelector::getDevices()
{
	std::vector<std::string> devices;

#if defined(__APPLE__) && defined(__MACH__)
	mach_port_t masterPort;
	kern_return_t ret = ::IOMasterPort(MACH_PORT_NULL, &masterPort);
	if (ret != KERN_SUCCESS)
		return devices;

	CFMutableDictionaryRef match = ::IOServiceMatching(kIOSerialBSDServiceValue);
	if (match == nullptr)
		::fprintf(stderr, "IOServiceMatching() returned nullptr\n");
	else
		::CFDictionarySetValue(match, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDRS232Type));

	io_iterator_t services;
	ret = ::IOServiceGetMatchingServices(masterPort, match, &services);
	if (ret != KERN_SUCCESS)
		return devices;

	io_object_t modem;
	while ((modem = ::IOIteratorNext(services))) {
		CFTypeRef filePath = ::IORegistryEntryCreateCFProperty(modem, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0);
		if (filePath != nullptr) {
			char port[MAXPATHLEN];
			Boolean result = ::CFStringGetCString((const __CFString*)filePath, port, MAXPATHLEN, kCFStringEncodingASCII);
			::CFRelease(filePath);

			if (result)
				devices.push_back(port);

			::IOObjectRelease(modem);
		}
	}

	::IOObjectRelease(services);
#elif defined(_WIN32)
	// Windows: probe COM1 through COM32 with CreateFile.
	for (int i = 1; i <= 32; ++i) {
		char portName[16];
		::sprintf_s(portName, sizeof(portName), "\\\\.\\COM%d", i);

		HANDLE h = ::CreateFileA(portName,
		                         GENERIC_READ | GENERIC_WRITE,
		                         0,
		                         nullptr,
		                         OPEN_EXISTING,
		                         FILE_ATTRIBUTE_NORMAL,
		                         nullptr);
		if (h != INVALID_HANDLE_VALUE) {
			::CloseHandle(h);
			char name[8];
			::sprintf_s(name, sizeof(name), "COM%d", i);
			devices.push_back(name);
		}
	}
#else
	// Linux: enumerate /dev for common serial port patterns
	static const char* const patterns[] = {
		"ttyACM", "ttyAMA", "ttyS", "ttyUSB", nullptr
	};

	DIR* devDir = ::opendir("/dev");
	if (devDir != nullptr) {
		struct dirent* entry;
		while ((entry = ::readdir(devDir)) != nullptr) {
			if (entry->d_type != DT_CHR && entry->d_type != DT_LNK && entry->d_type != DT_UNKNOWN)
				continue;
			for (int i = 0; patterns[i] != nullptr; ++i) {
				if (::strncmp(entry->d_name, patterns[i], ::strlen(patterns[i])) == 0) {
					devices.push_back(std::string("/dev/") + entry->d_name);
					break;
				}
			}
		}
		::closedir(devDir);
		std::sort(devices.begin(), devices.end());
	}
#endif

	return devices;
}
