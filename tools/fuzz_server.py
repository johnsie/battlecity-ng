#!/usr/bin/env python3
"""Protocol fuzzer for the Battle City server.

Connects several clients, gets some of them logged in (new account -> login -> take a city -> join), then sends
thousands of well-framed packets with random ids and payloads. The server must stay up and keep accepting
connections. Run it against an AddressSanitizer build so out-of-range indexing aborts the server.

Usage: tools/fuzz_server.py <host> <port> [seed] [packets-per-client]
"""
import random, socket, sys, time

CM_VERSION, CM_LOGIN, CM_NEWACCOUNT, CM_SETSTATE, CM_JOBAPP, CM_NEXTSTEP = 1, 2, 3, 5, 6, 21
CM_MAX = 49          # highest client message id (cmChangeStartingCity)
VERSION = b"3.5.8"


def frame(pid, data=b""):
    """[length][checksum][id][data]: the checksum sums the data as signed chars (see CSocket::ProcessBuffer)."""
    total = sum(b - 256 if b > 127 else b for b in data) + 3412
    if total < 0:
        data, total = b"", 3412
    packet = bytes([(total % 71) & 0xFF, pid]) + data
    return bytes([len(packet) + 0]) + packet if len(packet) < 255 else None


def field(text, size):
    return text[:size - 1].ljust(size, b"\0")


class Client:
    def __init__(self, host, port):
        self.s = socket.create_connection((host, port), timeout=5)
        self.s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.s.setblocking(False)

    def send(self, pid, data=b""):
        p = frame(pid, data)
        if p is None:
            return True
        try:
            self.s.sendall(p)
            return True
        except (BlockingIOError, InterruptedError):
            return True
        except OSError:
            return False

    def drain(self):
        try:
            while self.s.recv(65536):
                pass
        except (BlockingIOError, InterruptedError):
            pass
        except OSError:
            pass

    def join(self, name, city):
        self.send(CM_VERSION, field(b"fuzz", 50) + field(VERSION, 10))
        time.sleep(0.05)
        user = field(name, 15)
        self.send(CM_NEWACCOUNT, user + field(b"pw", 15) + field(name + b"@example.com", 50) + field(b"Fuzz", 20) + field(b"T", 15) + field(b"S", 15))
        time.sleep(0.05)
        self.send(CM_LOGIN, user + field(b"pw", 15))
        time.sleep(0.05)
        self.send(CM_SETSTATE, bytes([5]))
        self.send(CM_JOBAPP, bytes([city]))
        self.send(CM_NEXTSTEP, bytes([65]))
        self.drain()


def alive(host, port):
    try:
        socket.create_connection((host, port), timeout=3).close()
        return True
    except OSError:
        return False


def main():
    host, port = sys.argv[1], int(sys.argv[2])
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else int(time.time())
    count = int(sys.argv[4]) if len(sys.argv) > 4 else 3000
    rng = random.Random(seed)
    print(f"fuzz: seed {seed}, {count} packets per client")
    clients = []
    for i in range(6):
        c = Client(host, port)
        if i < 4:
            c.join(b"fuzz%d_%d" % (i, seed % 1000), i)
        clients.append(c)
    time.sleep(0.5)
    for n in range(count):
        c = rng.choice(clients)
        pid = rng.randint(0, CM_MAX + 5)
        size = rng.choice([0, 1, 2, 3, 8, 15, 30, 60, 120, 200])
        data = bytes(rng.randrange(256) for _ in range(size))
        if rng.random() < 0.3:   # indices at the edges are the interesting ones
            data = bytes(rng.choice([0, 1, 63, 64, 65, 127, 128, 254, 255]) for _ in range(size))
        c.send(pid, data)
        if n % 200 == 0:
            for cl in clients:
                cl.drain()
            time.sleep(0.02)
            if not alive(host, port):
                print(f"FAIL: server stopped answering after {n} packets (seed {seed})")
                return 1
    time.sleep(1)
    if not alive(host, port):
        print(f"FAIL: server stopped answering at the end (seed {seed})")
        return 1
    print("OK: server survived the fuzz run")
    return 0


if __name__ == "__main__":
    sys.exit(main())
