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

#include "TCPReaderWriter.h"
#include "UDPReaderWriter.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

// Same Winsock RAII initialisation pattern as UDPReaderWriter.
#if defined(_WIN32)
namespace {
struct WinsockInit {
	WinsockInit()  { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
	~WinsockInit() { WSACleanup(); }
};
static WinsockInit s_wsinit;
}
#endif

CTCPReaderWriter::CTCPReaderWriter(const std::string& address, unsigned int port, const std::string& localAddress) :
m_address(address),
m_port(port),
m_localAddress(localAddress),
#if defined(_WIN32)
m_fd(INVALID_SOCKET)
#else
m_fd(-1)
#endif
{
	assert(!address.empty());
	assert(port > 0U);
}

CTCPReaderWriter::CTCPReaderWriter() :
m_address(),
m_port(0U),
m_localAddress(),
#if defined(_WIN32)
m_fd(INVALID_SOCKET)
#else
m_fd(-1)
#endif
{
}

CTCPReaderWriter::~CTCPReaderWriter()
{
}

bool CTCPReaderWriter::open(const std::string& address, unsigned int port, const std::string& localAddress)
{
	m_address      = address;
	m_port         = port;
	m_localAddress = localAddress;

	return open();
}

bool CTCPReaderWriter::open()
{
#if defined(_WIN32)
	if (m_fd != INVALID_SOCKET)
#else
	if (m_fd != -1)
#endif
		return true;

	if (m_address.empty() || m_port == 0U)
		return false;

	m_fd = ::socket(PF_INET, SOCK_STREAM, 0);
#if defined(_WIN32)
	if (m_fd == INVALID_SOCKET) {
		::fprintf(stderr, "Cannot create the TCP  socket, err=%d\n", WSAGetLastError());
#else
	if (m_fd < 0) {
		::fprintf(stderr, "Cannot create the TCP  socket, err=%d\n", errno);
#endif
		return false;
	}

	if (!m_localAddress.empty()) {
		sockaddr_in addr;
		::memset(&addr, 0x00, sizeof(struct sockaddr_in));
		addr.sin_family = AF_INET;
		addr.sin_port   = 0U;
		addr.sin_addr.s_addr = ::inet_addr(m_localAddress.c_str());
		if (addr.sin_addr.s_addr == INADDR_NONE) {
			::fprintf(stderr, "The address is invalid - %s\n", m_localAddress.c_str());
			close();
			return false;
		}

		if (::bind(m_fd, (sockaddr*)&addr, sizeof(sockaddr_in)) == -1) {
#if defined(_WIN32)
			::fprintf(stderr, "Cannot bind the TCP  address, err=%d\n", WSAGetLastError());
#else
			::fprintf(stderr, "Cannot bind the TCP  address, err=%d\n", errno);
#endif
			close();
			return false;
		}
	}

	struct sockaddr_in addr;
	::memset(&addr, 0x00, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(m_port);
	addr.sin_addr   = CUDPReaderWriter::lookup(m_address);

	if (addr.sin_addr.s_addr == INADDR_NONE) {
		close();
		return false;
	}

	if (::connect(m_fd, (sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
#if defined(_WIN32)
		::fprintf(stderr, "Cannot connect the TCP  socket, err=%d\n", WSAGetLastError());
#else
		::fprintf(stderr, "Cannot connect the TCP  socket, err=%d\n", errno);
#endif
		close();
		return false;
	}

	int noDelay = 1;
	if (::setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&noDelay, sizeof(noDelay)) == -1) {
#if defined(_WIN32)
		::fprintf(stderr, "Cannot set the TCP  socket option, err=%d\n", WSAGetLastError());
#else
		::fprintf(stderr, "Cannot set the TCP  socket option, err=%d\n", errno);
#endif
		close();
		return false;
	}

	return true;
}

int CTCPReaderWriter::read(unsigned char* buffer, unsigned int length, unsigned int secs, unsigned int msecs)
{
	assert(buffer != nullptr);
	assert(length > 0U);
#if defined(_WIN32)
	assert(m_fd != INVALID_SOCKET);
#else
	assert(m_fd != -1);
#endif

	// Check that the recv() won't block
	fd_set readFds;
	FD_ZERO(&readFds);
	FD_SET(m_fd, &readFds);

	// Return after timeout
	timeval tv;
	tv.tv_sec  = secs;
	tv.tv_usec = msecs * 1000;

#if defined(_WIN32)
	int ret = ::select(0, &readFds, nullptr, nullptr, &tv);
#else
	int ret = ::select(m_fd + 1, &readFds, nullptr, nullptr, &tv);
#endif
	if (ret < 0) {
#if defined(_WIN32)
		::fprintf(stderr, "Error returned from TCP  select, err=%d\n", WSAGetLastError());
#else
		::fprintf(stderr, "Error returned from TCP  select, err=%d\n", errno);
#endif
		return -1;
	}

	if (!FD_ISSET(m_fd, &readFds))
		return 0;

#if defined(_WIN32)
	int len = ::recv(m_fd, (char*)buffer, length, 0);
#else
	ssize_t len = ::recv(m_fd, (char*)buffer, length, 0);
#endif
	if (len < 0) {
#if defined(_WIN32)
		::fprintf(stderr, "Error returned from recv, err=%d\n", WSAGetLastError());
#else
		::fprintf(stderr, "Error returned from recv, err=%d\n", errno);
#endif
		return -1;
	}

	return len;
}

bool CTCPReaderWriter::write(const unsigned char* buffer, unsigned int length)
{
	assert(buffer != nullptr);
	assert(length > 0U);
#if defined(_WIN32)
	assert(m_fd != INVALID_SOCKET);
#else
	assert(m_fd != -1);
#endif

#if defined(_WIN32)
	int ret = ::send(m_fd, (char *)buffer, length, 0);
	if (ret != int(length)) {
		::fprintf(stderr, "Error returned from send, err=%d\n", WSAGetLastError());
#else
	ssize_t ret = ::send(m_fd, (char *)buffer, length, 0);
	if (ret != ssize_t(length)) {
		::fprintf(stderr, "Error returned from send, err=%d\n", errno);
#endif
		return false;
	}

	return true;
}

void CTCPReaderWriter::close()
{
#if defined(_WIN32)
	if (m_fd != INVALID_SOCKET) {
		::closesocket(m_fd);
		m_fd = INVALID_SOCKET;
	}
#else
	if (m_fd != -1) {
		::close(m_fd);
		m_fd = -1;
	}
#endif
}
