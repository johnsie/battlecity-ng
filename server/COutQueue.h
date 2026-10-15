/*
 * Battle City - per-connection outgoing queue.
 *
 * Game sockets are non-blocking. When a client is slow (or the path to it is), send() accepts only part of
 * the data, or none, and reports "would block". Dropping the rest, as the old SendAll() did, truncates a
 * packet in the middle and loses everything queued behind it: the client then sees garbled packets
 * ("Invalid Checksum") and never receives things like the "sector sent" message that lets it drive into
 * a new map sector. This queue keeps whatever the socket cannot take yet and sends it later, in order.
 */
#ifndef _COUTQUEUE
#define _COUTQUEUE

#include <string>
#include <stddef.h>
#ifdef _WIN32
	#include <winsock2.h>
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <errno.h>
#endif

class COutQueue {
	public:
		/// <summary> Sends what the socket accepts now and queues the rest. Returns false only on a real connection error. </summary>
		bool Send(int sock, const char *data, int length) {
			if (length <= 0) return true;
			if (!pending.empty()) {
				// Older bytes are still waiting: never let new data overtake them
				pending.append(data, (size_t)length);
				return Flush(sock);
			}
			int sent = 0;
			bool ok = Write(sock, data, length, sent);
			if (sent < length) pending.append(data + sent, (size_t)(length - sent));
			return ok;
		}

		/// <summary> Sends as much of the queue as the socket accepts. Returns false only on a real connection error. </summary>
		bool Flush(int sock) {
			if (pending.empty()) return true;
			int sent = 0;
			bool ok = Write(sock, pending.data(), (int)pending.size(), sent);
			pending.erase(0, (size_t)sent);
			return ok;
		}

		size_t Pending() const { return pending.size(); }
		void Clear() { pending.clear(); }

	private:
		std::string pending;

		static bool WouldBlock() {
#ifdef _WIN32
			return WSAGetLastError() == WSAEWOULDBLOCK;
#else
			return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
		}

		// Writes until done or the socket stops accepting data. false = connection error.
		static bool Write(int sock, const char *data, int length, int &sent) {
			sent = 0;
			while (sent < length) {
				int result = (int)send(sock, data + sent, length - sent, 0);
				if (result > 0) {
					sent += result;
				} else if (result < 0 && WouldBlock()) {
					return true;            // socket buffer full: try again later
				} else {
					return false;           // closed or failed
				}
			}
			return true;
		}
};

#endif
