/*
 *   Copyright (C) 2010,2011,2012 by Jonathan Naylor G4KLX
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

#ifndef TCPReaderWriter_H
#define TCPReaderWriter_H

#include "StdCompat.h"

// Platform socket headers.
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#endif

/*
 * Thin wrapper around a blocking TCP client socket.
 *
 * open() resolves the remote hostname via CUDPReaderWriter::lookup(), creates
 * a SOCK_STREAM socket, and calls connect().  TCP_NODELAY is set to avoid
 * Nagle buffering delays on small control packets.
 *
 * read() uses select() with a caller-supplied timeout (seconds + milliseconds)
 * so the caller can implement protocol-level timeouts without blocking forever.
 * It returns the number of bytes received, 0 on timeout, or -1 on error.
 *
 * A default-constructed CTCPReaderWriter can be opened later by calling the
 * two-argument open() overload, which also sets the address and port.
 */
class CTCPReaderWriter {
public:
	CTCPReaderWriter(const std::string& address, unsigned int port, const std::string& localAddress = std::string());
	CTCPReaderWriter();
	~CTCPReaderWriter();

	bool open(const std::string& address, unsigned int port, const std::string& localAddress = std::string());
	bool open();

	// Blocks until data arrives or the timeout (secs + msecs) expires.
	// Returns bytes read, 0 on timeout, -1 on error.
	int  read(unsigned char* buffer, unsigned int length, unsigned int secs, unsigned int msecs = 0U);
	bool write(const unsigned char* buffer, unsigned int length);

	void close();

private:
	std::string    m_address;
	unsigned short m_port;
	std::string    m_localAddress;
#if defined(_WIN32)
	SOCKET         m_fd;
#else
	int            m_fd;
#endif
};

#endif
