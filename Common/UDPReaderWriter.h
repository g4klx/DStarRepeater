/*
 *   Copyright (C) 2009-2011,2013 by Jonathan Naylor G4KLX
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

#ifndef UDPReaderWriter_H
#define UDPReaderWriter_H

#include "StdCompat.h"

// Platform socket headers — Winsock on Windows, POSIX on everything else.
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
#include <arpa/inet.h>
#include <errno.h>
#endif

/*
 * Thin wrapper around a single UDP socket.
 *
 * open() creates and binds a SOCK_DGRAM socket to the configured address and
 * port.  read() is non-blocking (uses select() with a zero timeout); write()
 * sends to an explicit destination address supplied by the caller.
 *
 * The static lookup() method resolves a hostname or dotted-decimal string to
 * an in_addr, returning INADDR_NONE on failure.
 */
class CUDPReaderWriter {
public:
	CUDPReaderWriter(const std::string& address, unsigned int port);
	~CUDPReaderWriter();

	// Resolves hostname or dotted-decimal IP string to in_addr.
	// Tries inet_addr() first; falls back to getaddrinfo() for hostnames.
	static in_addr lookup(const std::string& hostName);

	bool open();

	// Returns the number of bytes received, 0 if no datagram was ready, or -1 on error.
	int  read(unsigned char* buffer, unsigned int length, in_addr& address, unsigned int& port);
	bool write(const unsigned char* buffer, unsigned int length, const in_addr& address, unsigned int port);

	void close();

	unsigned int getPort() const;

private:
	std::string    m_address;
	unsigned short m_port;
	in_addr        m_addr;
#if defined(_WIN32)
	SOCKET         m_fd;
#else
	int            m_fd;
#endif
};

#endif
