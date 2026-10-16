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
	RetryAttempts = 0;
	RetryPending = false;
	RetryAt = 0;
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
        this->CloseWinsock();
        this->ScheduleRetry((std::string("Unable to find the game server ") + ServerIP).c_str());
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
		this->ScheduleRetry("Unable to open a network socket");
		return;
	}

	//  No Nagle delay: small position updates must go out immediately, not in bursts
	{
		int one = 1;
		setsockopt(Socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
	}

	//  Non-blocking: the window must stay responsive while connecting (and a browser cannot block at all).
	//  The connection is completed in Cycle() / CheckConnect().
	bc_set_nonblocking(Socket);

    //  1 - Error connecting
    //  2 - Connected
	if (connect(Socket, (sockaddr*)(&sockAddr), sizeof(sockaddr_in)) == 0) {
		this->Connected = true;
		RetryAttempts = 0;
		Status = "";
		p->Process->ProcessEvent(2);
	} else if (bc_would_block()) {
		this->Connecting = true;
		this->ConnectStart = BCGetTicks();
	} else {
		this->CloseWinsock();
		this->ScheduleRetry("Unable to connect");
	}
}

/// <summary> A connect failed: retry with a growing delay (2, 4, 8, 16, 16 ... seconds) for a while, then ask the player </summary>
void CWinsock::ScheduleRetry(const char *reason) {
	const int maxAttempts = 6;
	RetryAttempts++;
	if (RetryAttempts > maxAttempts) {
		// Give up: tell the player and open the connection manager so they can pick another server
		RetryAttempts = 0;
		RetryPending = false;
		Status = "";
		p->Process->ProcessEvent(1);
		return;
	}
	unsigned long delay = 2000UL << (RetryAttempts - 1 < 3 ? RetryAttempts - 1 : 3);
	RetryAt = BCGetTicks() + delay;
	RetryPending = true;
	char text[200];
	snprintf(text, sizeof(text), "%s (attempt %d of %d). Trying again in %lu seconds...", reason, RetryAttempts, maxAttempts, delay / 1000);
	Status = text;
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
			RetryAttempts = 0;
			Status = "";
			p->Process->ProcessEvent(2);
			return;
		}
	} else if (BCGetTicks() - ConnectStart < 10000) {
		return;   // still trying
	}

	// Failed or timed out
	CloseWinsock();
	ScheduleRetry(ready > 0 ? "The server refused the connection" : "The server did not answer");
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
	memcpy(&SendString[3], TheData, length - 2);

	SendString[0] = (unsigned char)length;
	SendAll(SendString, length + 1);
}

void CWinsock::Cycle() {
	if (RetryPending && BCGetTicks() >= RetryAt) {
		RetryPending = false;
		StartTCP();
		return;
	}
	if (Connecting) {
		CheckConnect();
		return;
	}

	// Read everything that is waiting, not one message per call: in the browser every packet the server sends can
	// arrive as its own WebSocket message, and the burst sent at join is hundreds of them. Reading one per cycle took
	// seconds to catch up, long enough for sector requests to look lost. A time budget keeps a frame from stalling.
	unsigned long start = BCGetTicks();
	for (int reads = 0; reads < 1024 && this->BufferLength < 2048 && hasData(Socket); reads++) {
		if (!RecvTCP()) return;
		ProcessBuffer();
		if (BCGetTicks() - start > 6) break;
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

bool CWinsock::RecvTCP() {
	int nbytes = 0;
	nbytes = recv(Socket, this->Buffer + this->BufferLength, 2048 - this->BufferLength, 0);
	if (nbytes < 0 && bc_would_block()) {
		return true;   // nothing to read after all
	}
	if (nbytes <= 0) {
		p->Process->ProcessEvent(3); //Disconnected
		return false;
	}
	else {
		this->BufferLength += nbytes;
		// Test hook: BC_TEST_JUNK=1 puts one stray byte in front of the stream once, as damage would
		static bool junkDone = getenv("BC_TEST_JUNK") == NULL;
		if (!junkDone && this->BufferLength > 4 && this->BufferLength < 2047) {
			junkDone = true;
			memmove(this->Buffer + 1, this->Buffer, this->BufferLength);
			this->Buffer[0] = 0;
			this->BufferLength++;
		}
#ifdef BC_FRAME_TIMING
		static unsigned long lastReport = 0, calls = 0, bytes = 0, biggest = 0;
		calls++; bytes += nbytes; if ((unsigned long)nbytes > biggest) biggest = nbytes;
		if (BCGetTicks() - lastReport > 1000) { printf("net: %lu recv calls, %lu bytes in the last second (avg %lu, max %lu per call)\n", calls, bytes, calls ? bytes / calls : 0, biggest); calls = bytes = biggest = 0; lastReport = BCGetTicks(); }
#endif
	}
	return true;
}

void CWinsock::ProcessBuffer() {
	while (this->BufferLength > 0) {
		int packetlength = (unsigned char)this->Buffer[0] + 1;

		if (packetlength < 3) {
			// Not a valid packet start (damaged or out of step): skip a byte and look again instead of stalling forever
			memmove(this->Buffer, this->Buffer + 1, this->BufferLength - 1);
			this->BufferLength--;
			p->Engine->lognote("Receive buffer out of step, resynchronising");
			continue;
		}
		if (packetlength > this->BufferLength) break;     // the rest of this packet has not arrived yet

		char ValidPacket[256];
		memset(ValidPacket, 0, 256);

		memcpy(ValidPacket, this->Buffer, packetlength);
		if (this->BufferLength == packetlength)  {
			memset(this->Buffer, 0, 2048);
			this->BufferLength = 0;
		}
		else {
			memmove(this->Buffer, &this->Buffer[packetlength], this->BufferLength - packetlength);   // regions overlap
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
			// Report which packet failed (this text reaches the server log through cmCrash; keep it < 255 chars)
			int unsignedSum = 3412;
			for (int j = 0; j < (packetlength - 2); j++) unsignedSum += (unsigned char)ValidPacket[3 + j];
			char detail[240];
			int used = snprintf(detail, sizeof(detail), "Packet Dropped TCP - Invalid Checksum id=%d len=%d rx=%d calc=%d calcU=%d buf=%d hex:",
			                    (unsigned char)ValidPacket[2], packetlength, checksum, checksum2, unsignedSum % 71, this->BufferLength);
			for (int k = 0; k < packetlength && k < 24 && used < (int)sizeof(detail) - 4; k++)
				used += snprintf(detail + used, sizeof(detail) - used, " %02x", (unsigned char)ValidPacket[k]);
			p->Engine->lognote(detail);
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