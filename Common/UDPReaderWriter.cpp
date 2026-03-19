/*
 *   Copyright (C) 2006-2014 by Jonathan Naylor G4KLX
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

#include "UDPReaderWriter.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

// On Windows, Winsock must be initialised before any socket call and cleaned
// up at program exit.  The file-static WinsockInit object handles this via
// its constructor/destructor, which run at program startup and shutdown.
#if defined(_WIN32)
namespace {
struct WinsockInit {
	WinsockInit()  { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
	~WinsockInit() { WSACleanup(); }
};
static WinsockInit s_wsinit;
}
#endif

CUDPReaderWriter::CUDPReaderWriter(const std::string& address, unsigned int port) :
m_address(address),
m_port(port),
m_addr(),
#if defined(_WIN32)
m_fd(INVALID_SOCKET)
#else
m_fd(-1)
#endif
{
}

CUDPReaderWriter::~CUDPReaderWriter()
{
}

in_addr CUDPReaderWriter::lookup(const std::string& hostname)
{
	in_addr addr;

	in_addr_t address = ::inet_addr(hostname.c_str());
	if (address != in_addr_t(-1)) {
		addr.s_addr = address;
		return addr;
	}

	struct addrinfo hints{}, *res = nullptr;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	if (::getaddrinfo(hostname.c_str(), nullptr, &hints, &res) == 0 && res != nullptr) {
		addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr)->sin_addr;
		::freeaddrinfo(res);
		return addr;
	}

	::fprintf(stderr, "Cannot find address for host %s\n", hostname.c_str());
	addr.s_addr = INADDR_NONE;
	return addr;
}

bool CUDPReaderWriter::open()
{
	m_fd = ::socket(PF_INET, SOCK_DGRAM, 0);
#if defined(_WIN32)
	if (m_fd == INVALID_SOCKET) {
		::fprintf(stderr, "Cannot create the UDP socket, err: %d\n", WSAGetLastError());
#else
	if (m_fd < 0) {
		::fprintf(stderr, "Cannot create the UDP socket, err: %d\n", errno);
#endif
		return false;
	}

	if (m_port > 0U) {
		sockaddr_in addr;
		::memset(&addr, 0x00, sizeof(sockaddr_in));
		addr.sin_family      = AF_INET;
		addr.sin_port        = htons(m_port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (!m_address.empty()) {
			addr.sin_addr.s_addr = ::inet_addr(m_address.c_str());
			if (addr.sin_addr.s_addr == INADDR_NONE) {
				::fprintf(stderr, "The address is invalid - %s\n", m_address.c_str());
				return false;
			}
		}

		int reuse = 1;
		if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) == -1) {
#if defined(_WIN32)
			::fprintf(stderr, "Cannot set the UDP socket option (port: %u), err: %d\n", m_port, WSAGetLastError());
#else
			::fprintf(stderr, "Cannot set the UDP socket option (port: %u), err: %d\n", m_port, errno);
#endif
			return false;
		}

		if (::bind(m_fd, (sockaddr*)&addr, sizeof(sockaddr_in)) == -1) {
#if defined(_WIN32)
			::fprintf(stderr, "Cannot bind the UDP address (port: %u), err: %d\n", m_port, WSAGetLastError());
#else
			::fprintf(stderr, "Cannot bind the UDP address (port: %u), err: %d\n", m_port, errno);
#endif
			return false;
		}
	}

	return true;
}

int CUDPReaderWriter::read(unsigned char* buffer, unsigned int length, in_addr& address, unsigned int& port)
{
	// Check that the readfrom() won't block
	fd_set readFds;
	FD_ZERO(&readFds);
	FD_SET(m_fd, &readFds);

	// Return immediately
	timeval tv;
	tv.tv_sec  = 0L;
	tv.tv_usec = 0L;

#if defined(_WIN32)
	int ret = ::select(0, &readFds, nullptr, nullptr, &tv);
#else
	int ret = ::select(m_fd + 1, &readFds, nullptr, nullptr, &tv);
#endif
	if (ret < 0) {
#if defined(_WIN32)
		::fprintf(stderr, "Error returned from UDP select (port: %u), err: %d\n", m_port, WSAGetLastError());
#else
		::fprintf(stderr, "Error returned from UDP select (port: %u), err: %d\n", m_port, errno);
#endif
		return -1;
	}

	if (ret == 0)
		return 0;

	sockaddr_in addr;
	socklen_t size = sizeof(sockaddr_in);

#if defined(_WIN32)
	int len = ::recvfrom(m_fd, (char*)buffer, length, 0, (sockaddr *)&addr, &size);
#else
	ssize_t len = ::recvfrom(m_fd, (char*)buffer, length, 0, (sockaddr *)&addr, &size);
#endif
	if (len <= 0) {
#if defined(_WIN32)
		::fprintf(stderr, "Error returned from recvfrom (port: %u), err: %d\n", m_port, WSAGetLastError());
#else
		::fprintf(stderr, "Error returned from recvfrom (port: %u), err: %d\n", m_port, errno);
#endif
		return -1;
	}

	address = addr.sin_addr;
	port    = ntohs(addr.sin_port);

	return len;
}

bool CUDPReaderWriter::write(const unsigned char* buffer, unsigned int length, const in_addr& address, unsigned int port)
{
	sockaddr_in addr;
	::memset(&addr, 0x00, sizeof(sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_addr   = address;
	addr.sin_port   = htons(port);

#if defined(_WIN32)
	int ret = ::sendto(m_fd, (char *)buffer, length, 0, (sockaddr *)&addr, sizeof(sockaddr_in));
#else
	ssize_t ret = ::sendto(m_fd, (char *)buffer, length, 0, (sockaddr *)&addr, sizeof(sockaddr_in));
#endif
	if (ret < 0) {
#if defined(_WIN32)
		::fprintf(stderr, "Error returned from sendto (port: %u), err: %d\n", m_port, WSAGetLastError());
#else
		::fprintf(stderr, "Error returned from sendto (port: %u), err: %d\n", m_port, errno);
#endif
		return false;
	}

#if defined(_WIN32)
	if (ret != int(length))
#else
	if (ret != ssize_t(length))
#endif
		return false;

	return true;
}

void CUDPReaderWriter::close()
{
#if defined(_WIN32)
	::closesocket(m_fd);
#else
	::close(m_fd);
#endif
}

unsigned int CUDPReaderWriter::getPort() const
{
	return m_port;
}
