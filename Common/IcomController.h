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

#ifndef	IcomController_H
#define	IcomController_H

#include "SerialDataController.h"
#include "RingBuffer.h"
#include "Modem.h"
#include "Utils.h"

#include <wx/wx.h>

enum RESP_TYPE_ICOM {
	RTI_TIMEOUT,
	RTI_ERROR,
	RTI_UNKNOWN,
	RTI_HEADER,
	RTI_DATA,
	RTI_EOT,
	RTI_HEADER_ACK,
	RTI_DATA_ACK,
	RTI_PONG
};

class CIcomController : public CModem {
public:
	CIcomController(const wxString& port);
	virtual ~CIcomController();

	virtual void* Entry();

	virtual bool start();

	virtual unsigned int getSpace();
	virtual bool isTXReady();

	virtual bool writeHeader(const CHeaderData& header);
	virtual bool writeData(const unsigned char* data, unsigned int length, bool end);

	virtual wxString getPath() const;

private:
	wxString                   m_port;
	CSerialDataController      m_serial;
	CRingBuffer<unsigned char> m_txData;
	unsigned char              m_txCounter;
	unsigned char              m_pktCounter;

	RESP_TYPE_ICOM getResponse(unsigned char* buffer, unsigned int& length);
	bool writePoll();
	bool writePing();
};

#endif

