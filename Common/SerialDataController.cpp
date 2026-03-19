/*
 *   Copyright (C) 2002-2004,2007-2011,2013,2014,2015,2018 by Jonathan Naylor G4KLX
 *   Copyright (C) 1999-2001 by Thomas Sailor HB9JNX
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

#include "SerialDataController.h"

#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#endif


#if !defined(_WIN32)

CSerialDataController::CSerialDataController(const std::string& device, SERIAL_SPEED speed, bool assertRTS) :
m_device(device),
m_speed(speed),
m_assertRTS(assertRTS),
m_fd(-1)
{
	assert(!device.empty());
}

CSerialDataController::~CSerialDataController()
{
}

bool CSerialDataController::open()
{
	assert(m_fd == -1);

	m_fd = ::open(m_device.c_str(), O_RDWR | O_NOCTTY | O_NDELAY, 0);
	if (m_fd < 0) {
		::fprintf(stderr, "Cannot open device - %s\n", m_device.c_str());
		return false;
	}

	if (::isatty(m_fd) == 0) {
		::fprintf(stderr, "%s is not a TTY device\n", m_device.c_str());
		::close(m_fd);
		return false;
	}

	termios termios;
	if (::tcgetattr(m_fd, &termios) < 0) {
		::fprintf(stderr, "Cannot get the attributes for %s\n", m_device.c_str());
		::close(m_fd);
		return false;
	}

	termios.c_lflag    &= ~(ECHO | ECHOE | ICANON | IEXTEN | ISIG);
	termios.c_iflag    &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON | IXOFF | IXANY);
	termios.c_cflag    &= ~(CSIZE | CSTOPB | PARENB | CRTSCTS);
	termios.c_cflag    |= CS8;
	termios.c_oflag    &= ~(OPOST);
	termios.c_cc[VMIN]  = 0;
	termios.c_cc[VTIME] = 10;

	switch (m_speed) {
		case SERIAL_1200:
			::cfsetospeed(&termios, B1200);
			::cfsetispeed(&termios, B1200);
			break;
		case SERIAL_2400:
			::cfsetospeed(&termios, B2400);
			::cfsetispeed(&termios, B2400);
			break;
		case SERIAL_4800:
			::cfsetospeed(&termios, B4800);
			::cfsetispeed(&termios, B4800);
			break;
		case SERIAL_9600:
			::cfsetospeed(&termios, B9600);
			::cfsetispeed(&termios, B9600);
			break;
		case SERIAL_19200:
			::cfsetospeed(&termios, B19200);
			::cfsetispeed(&termios, B19200);
			break;
		case SERIAL_38400:
			::cfsetospeed(&termios, B38400);
			::cfsetispeed(&termios, B38400);
			break;
		case SERIAL_115200:
			::cfsetospeed(&termios, B115200);
			::cfsetispeed(&termios, B115200);
			break;
		case SERIAL_230400:
			::cfsetospeed(&termios, B230400);
			::cfsetispeed(&termios, B230400);
			break;
		default:
			::fprintf(stderr, "Unsupported serial port speed - %d\n", int(m_speed));
			::close(m_fd);
			return false;
	}

	if (::tcsetattr(m_fd, TCSANOW, &termios) < 0) {
		::fprintf(stderr, "Cannot set the attributes for %s\n", m_device.c_str());
		::close(m_fd);
		return false;
	}

	if (m_assertRTS) {
		unsigned int y;
		if (::ioctl(m_fd, TIOCMGET, &y) < 0) {
			::fprintf(stderr, "Cannot get the control attributes for %s\n", m_device.c_str());
			::close(m_fd);
			return false;
		}

		y |= TIOCM_RTS;

		if (::ioctl(m_fd, TIOCMSET, &y) < 0) {
			::fprintf(stderr, "Cannot set the control attributes for %s\n", m_device.c_str());
			::close(m_fd);
			return false;
		}
	}

	return true;
}

int CSerialDataController::read(unsigned char* buffer, unsigned int length, unsigned int timeout)
{
	assert(buffer != nullptr);
	assert(m_fd != -1);

	if (length == 0U)
		return 0;

	unsigned int offset = 0U;

	while (offset < length) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(m_fd, &fds);

		int n;
		if (offset == 0U) {
			struct timeval tv;
			tv.tv_sec  =  timeout / 1000U;
			tv.tv_usec = (timeout % 1000U) * 1000U;

			n = ::select(m_fd + 1, &fds, nullptr, nullptr, &tv);
			if (n == 0)
				return 0;
		} else {
			n = ::select(m_fd + 1, &fds, nullptr, nullptr, nullptr);
		}

		if (n < 0) {
			::fprintf(stderr, "Error from select(), errno=%d\n", errno);
			return -1;
		}

		if (n > 0) {
			ssize_t len = ::read(m_fd, buffer + offset, length - offset);
			if (len < 0) {
				if (errno != EAGAIN) {
					::fprintf(stderr, "Error from read(), errno=%d\n", errno);
					return -1;
				}
			}

			if (len > 0)
				offset += len;
		}
	}

	return length;
}

int CSerialDataController::write(const unsigned char* buffer, unsigned int length)
{
	assert(buffer != nullptr);
	assert(m_fd != -1);

	if (length == 0U)
		return 0;

	unsigned int ptr = 0U;

	while (ptr < length) {
		ssize_t n = ::write(m_fd, buffer + ptr, length - ptr);
		if (n < 0) {
			if (errno != EAGAIN) {
				::fprintf(stderr, "Error returned from write(), errno=%d\n", errno);
				return -1;
			}
		}

		if (n > 0)
			ptr += n;
	}

	return length;
}

void CSerialDataController::close()
{
	assert(m_fd != -1);

	::close(m_fd);
	m_fd = -1;
}

#else // _WIN32

CSerialDataController::CSerialDataController(const std::string& device, SERIAL_SPEED speed, bool assertRTS) :
m_device(device),
m_speed(speed),
m_assertRTS(assertRTS),
m_handle(INVALID_HANDLE_VALUE)
{
	assert(!device.empty());
}

CSerialDataController::~CSerialDataController()
{
}

bool CSerialDataController::open()
{
	assert(m_handle == INVALID_HANDLE_VALUE);

	// On Windows, ports above COM9 require the \\.\COMn prefix.
	std::string path = "\\\\.\\" + m_device;

	m_handle = ::CreateFileA(path.c_str(),
	                         GENERIC_READ | GENERIC_WRITE,
	                         0,
	                         nullptr,
	                         OPEN_EXISTING,
	                         FILE_ATTRIBUTE_NORMAL,
	                         nullptr);
	if (m_handle == INVALID_HANDLE_VALUE) {
		::fprintf(stderr, "Cannot open device - %s\n", m_device.c_str());
		return false;
	}

	DCB dcb;
	::memset(&dcb, 0, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	if (!::GetCommState(m_handle, &dcb)) {
		::fprintf(stderr, "Cannot get the attributes for %s\n", m_device.c_str());
		::CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
		return false;
	}

	dcb.BaudRate = static_cast<DWORD>(m_speed);
	dcb.ByteSize = 8;
	dcb.Parity   = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fBinary  = TRUE;
	dcb.fParity  = FALSE;
	dcb.fOutxCtsFlow  = FALSE;
	dcb.fOutxDsrFlow  = FALSE;
	dcb.fDtrControl   = DTR_CONTROL_DISABLE;
	dcb.fRtsControl   = RTS_CONTROL_DISABLE;
	dcb.fOutX  = FALSE;
	dcb.fInX   = FALSE;

	if (!::SetCommState(m_handle, &dcb)) {
		::fprintf(stderr, "Cannot set the attributes for %s\n", m_device.c_str());
		::CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
		return false;
	}

	// Use infinite inter-character timeout; per-read timeout set in read().
	COMMTIMEOUTS timeouts;
	::memset(&timeouts, 0, sizeof(COMMTIMEOUTS));
	timeouts.ReadIntervalTimeout         = 0;
	timeouts.ReadTotalTimeoutMultiplier  = 0;
	timeouts.ReadTotalTimeoutConstant    = 0;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant   = 0;
	if (!::SetCommTimeouts(m_handle, &timeouts)) {
		::fprintf(stderr, "Cannot set the timeouts for %s\n", m_device.c_str());
		::CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
		return false;
	}

	if (m_assertRTS) {
		if (!::EscapeCommFunction(m_handle, SETRTS)) {
			::fprintf(stderr, "Cannot set RTS for %s\n", m_device.c_str());
			::CloseHandle(m_handle);
			m_handle = INVALID_HANDLE_VALUE;
			return false;
		}
	}

	return true;
}

int CSerialDataController::read(unsigned char* buffer, unsigned int length, unsigned int timeout)
{
	assert(buffer != nullptr);
	assert(m_handle != INVALID_HANDLE_VALUE);

	if (length == 0U)
		return 0;

	unsigned int offset = 0U;

	// Apply the caller's timeout to the first byte only.
	COMMTIMEOUTS timeouts;
	::memset(&timeouts, 0, sizeof(COMMTIMEOUTS));
	timeouts.ReadIntervalTimeout         = 0;
	timeouts.ReadTotalTimeoutMultiplier  = 0;
	timeouts.ReadTotalTimeoutConstant    = (offset == 0U) ? static_cast<DWORD>(timeout) : MAXDWORD;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant   = 0;
	::SetCommTimeouts(m_handle, &timeouts);

	while (offset < length) {
		if (offset == 1U) {
			// After the first byte arrives, block indefinitely for the rest.
			timeouts.ReadTotalTimeoutConstant = MAXDWORD;
			::SetCommTimeouts(m_handle, &timeouts);
		}

		DWORD bytesRead = 0U;
		BOOL ok = ::ReadFile(m_handle,
		                     buffer + offset,
		                     length - offset,
		                     &bytesRead,
		                     nullptr);
		if (!ok) {
			::fprintf(stderr, "Error from ReadFile(), error=%lu\n", ::GetLastError());
			return -1;
		}

		if (bytesRead == 0U && offset == 0U)
			return 0; // timeout on first byte

		offset += bytesRead;
	}

	return static_cast<int>(length);
}

int CSerialDataController::write(const unsigned char* buffer, unsigned int length)
{
	assert(buffer != nullptr);
	assert(m_handle != INVALID_HANDLE_VALUE);

	if (length == 0U)
		return 0;

	unsigned int ptr = 0U;

	while (ptr < length) {
		DWORD bytesWritten = 0U;
		BOOL ok = ::WriteFile(m_handle,
		                      buffer + ptr,
		                      length - ptr,
		                      &bytesWritten,
		                      nullptr);
		if (!ok) {
			::fprintf(stderr, "Error returned from WriteFile(), error=%lu\n", ::GetLastError());
			return -1;
		}

		ptr += bytesWritten;
	}

	return static_cast<int>(length);
}

void CSerialDataController::close()
{
	assert(m_handle != INVALID_HANDLE_VALUE);

	::CloseHandle(m_handle);
	m_handle = INVALID_HANDLE_VALUE;
}

#endif // _WIN32
