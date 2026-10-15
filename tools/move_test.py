#!/usr/bin/env python3
"""Measures how smoothly one player's movement reaches another.

Two bots join the same city. Bot A drives in a straight line and sends cmUpdate every 150 ms, like the real client.
Bot B records when each smUpdate about A arrives. A steady 150 ms rhythm is what the receiving client's
smoothing expects; bursts and gaps show up as a large spread.

Usage: tools/move_test.py <host> <port> [seconds]
"""
import socket, struct, sys, time, statistics

CM_VERSION, CM_LOGIN, CM_NEWACCOUNT, CM_SETSTATE, CM_JOBAPP, CM_HIREACCEPT, CM_NEXTSTEP, CM_UPDATE = 1, 2, 3, 5, 6, 8, 21, 28
SM_STATEGAME, SM_MAYORHIRE, SM_UPDATE = 8, 9, 46
VERSION = b"3.5.8"


def frame(pid, data=b""):
    total = sum(b - 256 if b > 127 else b for b in data) + 3412
    packet = bytes([(total % 71) & 0xFF, pid]) + data
    return bytes([len(packet)]) + packet


def field(text, size):
    return text[:size - 1].ljust(size, b"\0")


class Bot:
    def __init__(self, host, port, name):
        self.s = socket.create_connection((host, port), timeout=5)
        self.s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.s.setblocking(False)
        self.buf = b""
        self.name = name
        self.x = self.y = None
        self.updates = []        # (arrival time, x, y) of smUpdate packets
        self.hire = False

    def send(self, pid, data=b""):
        self.s.sendall(frame(pid, data))

    def pump(self):
        try:
            chunk = self.s.recv(65536)
            if not chunk:
                raise SystemExit("server closed the connection")
            self.buf += chunk
        except (BlockingIOError, InterruptedError):
            pass
        now = time.perf_counter()
        while self.buf:
            n = self.buf[0] + 1
            if n < 3 or len(self.buf) < n:
                break
            pid, data = self.buf[2], self.buf[3:n]
            self.buf = self.buf[n:]
            if pid == SM_STATEGAME and len(data) >= 4:
                self.x, self.y = struct.unpack("<HH", data[:4])
            elif pid == SM_MAYORHIRE:
                self.hire = True
            elif pid == SM_UPDATE and len(data) >= 7:
                _id, x, y = struct.unpack("<BHH", data[:5])
                self.updates.append((now, x, y))

    def wait(self, cond, timeout=5):
        end = time.time() + timeout
        while time.time() < end:
            self.pump()
            if cond():
                return True
            time.sleep(0.01)
        return False

    def login(self, city):
        self.send(CM_VERSION, field(b"move", 50) + field(VERSION, 10))
        time.sleep(0.05)
        user = field(self.name, 15)
        self.send(CM_NEWACCOUNT, user + field(b"pw", 15) + field(self.name + b"@example.com", 50) + field(b"Move", 20) + field(b"T", 15) + field(b"S", 15))
        time.sleep(0.05)
        self.send(CM_LOGIN, user + field(b"pw", 15))
        time.sleep(0.05)
        self.send(CM_SETSTATE, bytes([5]))
        self.send(CM_JOBAPP, bytes([city]))


def main():
    host, port = sys.argv[1], int(sys.argv[2])
    seconds = float(sys.argv[3]) if len(sys.argv) > 3 else 15
    tag = str(int(time.time()) % 100000).encode()
    a, b = Bot(host, port, b"movA" + tag), Bot(host, port, b"movB" + tag)
    a.login(0)
    a.send(CM_NEXTSTEP, bytes([65]))
    if not a.wait(lambda: a.x is not None):
        raise SystemExit("bot A never joined")
    b.login(0)
    if not a.wait(lambda: a.hire):
        raise SystemExit("bot A was never asked to hire")
    a.send(CM_HIREACCEPT, b" ")
    b.send(CM_NEXTSTEP, bytes([65]))
    if not b.wait(lambda: b.x is not None):
        raise SystemExit("bot B never joined")
    time.sleep(0.5)
    # A drives: direction 8 (a quarter turn), 0.38 units/ms like a normal tank
    x, y = float(a.x), float(a.y)
    start = time.perf_counter()
    nxt = start
    while time.perf_counter() - start < seconds:
        now = time.perf_counter()
        if now >= nxt:
            x += 0.38 * 150 * 0.7071
            y += 0.38 * 150 * 0.7071
            a.send(CM_UPDATE, struct.pack("<HHBBB", int(x), int(y), 1, 2, 8))
            nxt += 0.150
        a.pump()
        b.pump()
        time.sleep(0.002)
    mine = [u for u in b.updates]
    if len(mine) < 10:
        raise SystemExit(f"bot B saw only {len(mine)} updates")
    gaps = [(mine[i][0] - mine[i - 1][0]) * 1000 for i in range(1, len(mine))]
    print(f"updates seen by B: {len(mine)}  interval ms: mean {statistics.mean(gaps):.1f}  stdev {statistics.pstdev(gaps):.1f}  "
          f"min {min(gaps):.1f}  max {max(gaps):.1f}  p95 {sorted(gaps)[int(len(gaps) * 0.95)]:.1f}")
    if statistics.pstdev(gaps) > 25 or max(gaps) > 400:
        raise SystemExit("FAIL: updates reach the other player unevenly")
    print("OK: updates arrive steadily")


if __name__ == "__main__":
    main()
