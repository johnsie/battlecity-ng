// Regression test for COutQueue: pushing far more data than a socket buffer holds, to a reader that
// is slow to start, must deliver every byte, in order, without truncating anything.
// (The old SendAll() gave up at the first "would block" and lost the rest; run with -DOLD_BEHAVIOUR to see that.)
#include "../COutQueue.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

#ifdef OLD_BEHAVIOUR
// the original SendAll: stops at the first failed send()
static bool OldSendAll(int sock, const char *data, int length) {
	int total = 0;
	while (total < length) { int r = (int)send(sock, data + total, length - total, 0); if (r == -1) return false; total += r; }
	return true;
}
#endif

int main() {
	int fds[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) { perror("socketpair"); return 2; }
	int writer = fds[0], reader = fds[1];
	for (int fd : fds) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
	int small = 2048;
	setsockopt(writer, SOL_SOCKET, SO_SNDBUF, &small, sizeof(small));
	setsockopt(reader, SOL_SOCKET, SO_RCVBUF, &small, sizeof(small));

	// 1 MB of recognisable data, sent in packet-sized pieces like the game does (<= 255 bytes)
	std::string expected;
	for (int i = 0; i < 1000000; i++) expected += (char)(i * 31 + i / 251);
	COutQueue queue;
	for (size_t offset = 0; offset < expected.size(); offset += 200) {
		int n = (int)std::min<size_t>(200, expected.size() - offset);
#ifdef OLD_BEHAVIOUR
		OldSendAll(writer, expected.data() + offset, n);
#else
		if (!queue.Send(writer, expected.data() + offset, n)) { printf("FAIL: send reported a connection error\n"); return 1; }
#endif
	}
	printf("queued behind the full socket buffer: %zu bytes\n", queue.Pending());

	// the reader only starts now; keep flushing as it drains
	std::string received;
	char buffer[4096];
	for (int idle = 0; idle < 200 && received.size() < expected.size(); ) {
#ifndef OLD_BEHAVIOUR
		queue.Flush(writer);
#endif
		ssize_t r = recv(reader, buffer, sizeof(buffer), 0);
		if (r > 0) { received.append(buffer, (size_t)r); idle = 0; }
		else { idle++; usleep(1000); }
	}
	printf("sent %zu bytes, received %zu bytes\n", expected.size(), received.size());
	if (received != expected) { printf("FAIL: data lost or reordered\n"); return 1; }
	printf("OK: every byte arrived, in order\n");
	return 0;
}
