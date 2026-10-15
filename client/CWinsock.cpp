/*
===============================================================================

    Battle City - CWinsock
    Copyright (C) 2005-2013  battlecity.org

    This file is part of Battle City.

    Battle City is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Battle City is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Battle City.  If not, see <http://www.gnu.org/licenses/>.
===============================================================================
*/
#include "CWinsock.h"

CWinsock::CWinsock(CGame *game)  {
	p = game;
	Connected = false;
	Connecting = false;
	ConnectStart = 0;
	MyIndex = 0;
	Socket = BC_INVALID_SOCKET;

	BufferLength = 0;
	memset(Buffer, 0, 2048);
}

CWinsock::~CWinsock() {
	CloseWinsock();
}

void CWinsock::Init(char *IP) {
	ServerIP = IP;
}
/// <summary>   Establishes a TCP connection with the game server. </summary>
void CWinsock::StartTCP() {
    //  Close existing connections
	this->CloseWinsock();
    //  Start connection
	this->StartWinsock();
    //  Establish host entry structure
	struct hostent *host_entry;
    //  Populate host_entry with ip address
    host_entry = gethostbyname(ServerIP);
	if(host_entry==NULL)
    {
        MessageBox(p->hWnd, (std::string("Unable to find the game server: ") + ServerIP).c_str(), "BattleCity", 0);
        p->Process->ProcessEvent(1);
        return;
    }
    //  Establish socket address object
    //  http://msdn.microsoft.com/en-us/library/aa917469.aspx
	sockaddr_in sockAddr;
    //  Populate sin_family with AF_INET
	sockAddr.sin_family = AF_INET;
    //  Converts a u_short from host to TCP/IP network byte order and assigns to sin_port
    //  http://msdn.microsoft.com/en-us/library/windows/desktop/ms738557(v=vs.85).aspx
	sockAddr.sin_port = htons(TCPPORT);
    //  Sets IP address to sin_addr in network byte order
	memset(&sockAddr, 0, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(TCPPORT);
	memcpy(&sockAddr.sin_addr, host_entry->h_addr, sizeof(sockAddr.sin_addr));
    //  Initialize socket 
	Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    //  Upon verifying Socket connection failed display message box with an error message
	if (Socket == BC_INVALID_SOCKET) {
		MessageBox(p->hWnd, "Invalid Socket!", 0, 0);
		p->Process->ProcessEvent(1);
		return;
	}

	//  Non-blocking: the window must stay responsive while connecting (and a browser cannot block at all).
	//  The connection is completed in Cycle() / CheckConnect().
	bc_set_nonblocking(Socket);

    //  1 - Error connecting
    //  2 - Connected
	if (connect(Socket, (sockaddr*)(&sockAddr), sizeof(sockaddr_in)) == 0) {
		this->Connected = true;
		p->Process->ProcessEvent(2);
	} else if (bc_would_block()) {
		this->Connecting = true;
		this->ConnectStart = BCGetTicks();
	} else {
		this->CloseWinsock();
		p->Process->ProcessEvent(1);
	}
}

/// <summary> Finishes a non-blocking connect: success -> event 2, failure or 10s timeout -> event 1 </summary>
void CWinsock::CheckConnect() {
	if (!Connecting || Socket == BC_INVALID_SOCKET) return;

	fd_set writable, failed;
	FD_ZERO(&writable);
	FD_ZERO(&failed);
	FD_SET(Socket, &writable);
	FD_SET(Socket, &failed);
	timeval none;
	none.tv_sec = 0;
	none.tv_usec = 0;
	int ready = select((int)Socket + 1, NULL, &writable, &failed, &none);

	if (ready > 0) {
		int error = 0;
		socklen_t length = sizeof(error);
		getsockopt(Socket, SOL_SOCKET, SO_ERROR, (char *)&error, &length);
		if (error == 0 && !FD_ISSET(Socket, &failed)) {
			Connecting = false;
			Connected = true;
			p->Process->ProcessEvent(2);
			return;
		}
	} else if (BCGetTicks() - ConnectStart < 10000) {
		return;   // still trying
	}

	// Failed or timed out
	CloseWinsock();
	p->Process->ProcessEvent(1);
}

void CWinsock::StartWinsock() {
	bc_net_startup();
}
void CWinsock::CloseWinsock() {
	if (Socket != BC_INVALID_SOCKET) {
		bc_close_socket(Socket);
		Socket = BC_INVALID_SOCKET;
	}
	Connected = false;
	Connecting = false;
	MyIndex = 0;
	Sleep(5);
	bc_net_shutdown();
}

void CWinsock::SendData(unsigned char PacketID, char *TheData, int len) {
	char SendString[256];
	memset(SendString, 0, 256);
	int length = 0;

	if (len == -1)
		length = (int)(strlen(TheData) + 2);
	else 
		length = len + 2;

	int checksum = 0;
	for (int j = 0; j < (length - 2); j++) {
		checksum += TheData[j];
	}
	checksum += 3412;
	checksum = checksum % 71;

	SendString[1] = (unsigned char)checksum;
	SendString[2] = (unsigned char)PacketID;
	memcpy(&SendString[3], TheData, length);

	SendString[0] = (unsigned char)length;
	SendAll(SendString, length + 1);
}

void CWinsock::Cycle() {
	if (Connecting) {
		CheckConnect();
		return;
	}

	if (hasData(Socket)) {
		RecvTCP();	
	}

	ProcessBuffer();
}

bool CWinsock::hasData(bc_socket sock) {
	if (sock == BC_INVALID_SOCKET) return false;
	timeval timev;
	timev.tv_sec = 0;
	timev.tv_usec = 0;
	fd_set dta;
	FD_ZERO(&dta);
	FD_SET(sock,&dta);
	select((int)sock+1,&dta,0,0,&timev);
	if (FD_ISSET(sock, &dta)) return true;
	return false;
}

void CWinsock::RecvTCP() {
	int nbytes = 0;
	nbytes = recv(Socket, this->Buffer + this->BufferLength, 2048 - this->BufferLength, 0);
	if (nbytes < 0 && bc_would_block()) {
		return;   // nothing to read after all
	}
	if (nbytes <= 0) {
		p->Process->ProcessEvent(3); //Disconnected
	}
	else {
		this->BufferLength += nbytes;
	}
}

void CWinsock::ProcessBuffer() {
	while (this->BufferLength > 0) {
		int packetlength = (unsigned char)this->Buffer[0] + 1;

		if (packetlength > this->BufferLength || packetlength < 3) break;

		char ValidPacket[256];
		memset(ValidPacket, 0, 256);

		memcpy(ValidPacket, this->Buffer, packetlength);
		if (this->BufferLength == packetlength)  {
			memset(this->Buffer, 0, 2048);
			this->BufferLength = 0;
		}
		else {
			memcpy(this->Buffer, &this->Buffer[packetlength], this->BufferLength - packetlength);
			this->BufferLength -= packetlength;
		}
		
		int checksum = (unsigned char)ValidPacket[1];
		int checksum2 = 0;
		for (int j = 0; j < (packetlength - 2); j++) {
			checksum2 += ValidPacket[3 + j];
		}
		checksum2 += 3412;
		checksum2 = checksum2 % 71;
		if (checksum == checksum2) {
			p->Process->ProcessData(&ValidPacket[2]);
		}
		else {
			if (getenv("BC_DEBUG_NET")) {
				printf("DROPPED packet: length=%d id=%d checksum(received)=%d checksum(computed)=%d bytes:", packetlength, (unsigned char)ValidPacket[2], checksum, checksum2);
				for (int k = 0; k < packetlength && k < 32; k++) printf(" %02x", (unsigned char)ValidPacket[k]);
				printf("\n");
			}
			p->Engine->logerror("Packet Dropped TCP - Invalid Checksum");
		}
	}
}

int CWinsock::SendAll(char *SendString, int SendLength) {
    int TotalSent = 0;
    int bytesLeft = SendLength;
    int SendReturn = 0;
    int retries = 0;

    while(TotalSent < SendLength)  {
        SendReturn = send(Socket, SendString+TotalSent, bytesLeft, 0);
        if (SendReturn == -1) {
			if (bc_would_block() && ++retries < 2000) {
				SendReturn = 0;
				BCSleep(1);
				continue;
			}
			break;
		}
        TotalSent += SendReturn;
        bytesLeft -= SendReturn;
    }

    return SendReturn==-1?-1:0;
} 