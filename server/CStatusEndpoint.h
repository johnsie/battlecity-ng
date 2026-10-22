#ifndef _CSTATUSENDPOINT
#define _CSTATUSENDPOINT

#include <stddef.h>

class CServer;

class CStatusEndpoint {
public:
	explicit CStatusEndpoint(CServer *server);
	~CStatusEndpoint();
	void Init();
	void Cycle();

private:
	CServer *server;
	int listener;
	int clients[8];
	char requests[8][1024];
	size_t requestLengths[8];
	void CloseClient(int slot);
	void Reply(int slot);
};

#endif
