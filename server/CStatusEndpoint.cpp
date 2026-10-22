#include "CServer.h"
#include "CStatusEndpoint.h"
#include "PublicStatus.h"

#include <sstream>

CStatusEndpoint::CStatusEndpoint(CServer *server)
	: server(server), listener(-1) {
	for (int i = 0; i < 8; ++i) {
		clients[i] = -1;
		requestLengths[i] = 0;
	}
}

CStatusEndpoint::~CStatusEndpoint() {
	for (int i = 0; i < 8; ++i) CloseClient(i);
	if (listener >= 0) {
#ifdef WIN32
		closesocket(listener);
#else
		close(listener);
#endif
	}
}

void CStatusEndpoint::Init() {
	int port = 9682;
	if (const char *configured = getenv("BC_STATUS_PORT")) {
		port = atoi(configured);
		if (port < 0 || port > 65535) port = 9682;
	}
	if (port == 0) {
		cout << "CStatusEndpoint [ disabled ]" << endl;
		return;
	}

	listener = (int)socket(AF_INET, SOCK_STREAM, 0);
	if (listener < 0) {
		perror("Unable to create status listener");
		return;
	}
	int reuse = 1;
	setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = htons((unsigned short)port);
	if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 || listen(listener, 8) != 0) {
		perror("Unable to bind loopback status listener");
#ifdef WIN32
		closesocket(listener);
#else
		close(listener);
#endif
		listener = -1;
		return;
	}
#ifdef WIN32
	u_long nonBlocking = 1;
	ioctlsocket(listener, FIONBIO, &nonBlocking);
#else
	fcntl(listener, F_SETFL, fcntl(listener, F_GETFL, 0) | O_NONBLOCK);
#endif
	cout << "CStatusEndpoint [ Listening on 127.0.0.1:" << port << " ]" << endl;
}

void CStatusEndpoint::CloseClient(int slot) {
	if (clients[slot] < 0) return;
#ifdef WIN32
	closesocket(clients[slot]);
#else
	close(clients[slot]);
#endif
	clients[slot] = -1;
	requestLengths[slot] = 0;
}

void CStatusEndpoint::Reply(int slot) {
	const bool valid = strncmp(requests[slot], "GET /health HTTP/1.", 19) == 0;
	std::string body;
	std::string status;
	if (valid) {
		PublicPlayerState publicPlayers[MAX_PLAYERS];
		for (int i = 0; i < MAX_PLAYERS; ++i) {
			CPlayer *player = server->Player[i];
			publicPlayers[i].state = player->State;
			publicPlayers[i].city = player->City;
			publicPlayers[i].automated = player->IsBot;
		}
		std::ostringstream json;
		json << "{\"status\":\"available\",\"players\":"
			<< CountPublicActivePlayers(publicPlayers, MAX_PLAYERS) << "}";
		body = json.str();
		status = "200 OK";
	} else {
		body = "{\"status\":\"unknown\"}";
		status = "404 Not Found";
	}
	std::ostringstream response;
	response << "HTTP/1.1 " << status << "\r\n"
		<< "Content-Type: application/json\r\n"
		<< "Cache-Control: no-store\r\n"
		<< "Content-Length: " << body.size() << "\r\n"
		<< "Connection: close\r\n\r\n" << body;
	const std::string wire = response.str();
	send(clients[slot], wire.data(), (int)wire.size(), 0);
	CloseClient(slot);
}

void CStatusEndpoint::Cycle() {
	if (listener < 0) return;
	for (;;) {
		struct sockaddr_in peer;
		socklen_t peerLength = sizeof(peer);
		int incoming = (int)accept(listener, (struct sockaddr *)&peer, &peerLength);
		if (incoming < 0) break;
		int slot = -1;
		for (int i = 0; i < 8; ++i) if (clients[i] < 0) { slot = i; break; }
		if (slot < 0) {
#ifdef WIN32
			closesocket(incoming);
#else
			close(incoming);
#endif
			continue;
		}
#ifdef WIN32
		u_long nonBlocking = 1;
		ioctlsocket(incoming, FIONBIO, &nonBlocking);
#else
		fcntl(incoming, F_SETFL, fcntl(incoming, F_GETFL, 0) | O_NONBLOCK);
#endif
		clients[slot] = incoming;
		requestLengths[slot] = 0;
	}

	for (int i = 0; i < 8; ++i) {
		if (clients[i] < 0) continue;
		const size_t remaining = sizeof(requests[i]) - 1 - requestLengths[i];
		int received = recv(clients[i], requests[i] + requestLengths[i], (int)remaining, 0);
		if (received == 0) {
			CloseClient(i);
			continue;
		}
		if (received < 0) {
#ifdef WIN32
			if (WSAGetLastError() != WSAEWOULDBLOCK) CloseClient(i);
#else
			if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) CloseClient(i);
#endif
			continue;
		}
		requestLengths[i] += (size_t)received;
		requests[i][requestLengths[i]] = '\0';
		if (strstr(requests[i], "\r\n\r\n") != NULL) Reply(i);
		else if (requestLengths[i] == sizeof(requests[i]) - 1) CloseClient(i);
	}
}
