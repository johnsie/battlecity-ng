/*
 * Battle City - socket portability layer (BSD sockets / Winsock).
 * The game uses plain TCP with non-blocking sockets and select().
 */
#ifndef BC_PLATFORM_NET_H
#define BC_PLATFORM_NET_H

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	typedef SOCKET bc_socket;
	#define BC_INVALID_SOCKET INVALID_SOCKET
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/select.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <errno.h>
	typedef int bc_socket;
	#define SOCKET bc_socket
	#define INVALID_SOCKET (-1)
	#define SOCKET_ERROR (-1)
	#define BC_INVALID_SOCKET (-1)
#endif

bool bc_net_startup();
void bc_net_shutdown();
void bc_set_nonblocking(bc_socket s);
void bc_close_socket(bc_socket s);
// true when the last socket call failed only because it would block / is in progress
bool bc_would_block();

#endif
