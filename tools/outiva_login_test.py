#!/usr/bin/env python3
"""End-to-end test of guest and Outiva-ticket login against a throwaway server.

Usage: tools/outiva_login_test.py <host> <port> <ticket-secret> <path-to-BCData.db>
The server must have been started with BC_TICKET_SECRET_FILE holding the secret, BC_LEGACY_LOGIN=0 and BC_BOTS=0.
"""
import base64, hashlib, hmac, os, socket, sqlite3, sys, time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "deploy", "web"))
import outiva_broker

CM_VERSION, CM_LOGIN, CM_NEWACCOUNT, CM_SETSTATE, CM_JOBAPP, CM_GUESTLOGIN, CM_TICKETLOGIN = 1, 2, 3, 5, 6, 55, 56
SM_LOGINCORRECT, SM_ERROR = 1, 17
VERSION = b"3.5.8"


def frame(pid, data=b""):
    total = sum(b - 256 if b > 127 else b for b in data) + 3412
    packet = bytes([(total % 71) & 0xFF, pid]) + data
    return bytes([len(packet)]) + packet


def field(text, size):
    return text[:size - 1].ljust(size, b"\0")


def b64(data):
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode()


def ticket(secret, sub, callsign, lifetime=60, tid=None, now=None):
    expires = int((now or time.time()) + lifetime)
    tid = tid or b64(hashlib.sha256(f"{sub}{time.time_ns()}".encode()).digest()[:9])
    signed = "v1." + b64(f"{sub}|{callsign}|{expires}|{tid}".encode())
    mac = hmac.new(secret.encode(), signed.encode(), hashlib.sha256).digest()
    return f"{signed}.{b64(mac)}"


class Client:
    def __init__(self, host, port):
        self.s = socket.create_connection((host, port), timeout=5)
        self.s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.s.setblocking(False)
        self.buf = b""
        self.seen = []          # (packet id, payload)
        self.send(CM_VERSION, field(b"outiva-test", 50) + field(VERSION, 10))
        time.sleep(0.1)

    def send(self, pid, data=b""):
        self.s.sendall(frame(pid, data))

    def pump(self):
        try:
            chunk = self.s.recv(65536)
            if not chunk:
                return
            self.buf += chunk
        except (BlockingIOError, InterruptedError):
            pass
        while self.buf:
            n = self.buf[0] + 1
            if n < 3 or len(self.buf) < n:
                break
            self.seen.append((self.buf[2], self.buf[3:n]))
            self.buf = self.buf[n:]

    def wait(self, cond, timeout=3):
        end = time.time() + timeout
        while time.time() < end:
            self.pump()
            if cond():
                return True
            time.sleep(0.02)
        return False

    def logged_in(self):
        return any(p == SM_LOGINCORRECT for p, _ in self.seen)

    def error(self):
        for p, d in self.seen:
            if p == SM_ERROR:
                return d[:1].decode(errors="replace")
        return None

    def close(self):
        self.s.close()


def rows(db, where=""):
    with sqlite3.connect(db) as c:
        return c.execute(f"SELECT Account, OutivaSub, Guest FROM tAccounts {where}").fetchall()


def check(ok, message):
    if not ok:
        raise SystemExit("FAIL: " + message)
    print("OK:", message)


def main():
    host, port, secret, db = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]

    # the old name-and-password login and registration are off
    c = Client(host, port)
    c.send(CM_NEWACCOUNT, field(b"old", 15) + field(b"pw", 15) + field(b"o@example.com", 50) + field(b"O", 20) + field(b"T", 15) + field(b"S", 15))
    check(c.wait(lambda: c.error() == "O") and not rows(db, "WHERE Account = 'old'"), "registration with a password is refused")
    c.send(CM_LOGIN, field(b"old", 15) + field(b"pw", 15))
    check(c.wait(lambda: len([1 for p, d in c.seen if p == SM_ERROR]) >= 2), "password login is refused")
    c.close()

    # a guest plays as "<name> (nonreg)", appears in the table flagged, and leaves nothing behind
    g = Client(host, port)
    g.send(CM_GUESTLOGIN, field(b"Bob", 15))
    check(g.wait(g.logged_in), "a guest can log in")
    check(("Bob (nonreg)", "", 1) in rows(db), "the guest is a flagged temporary row named 'Bob (nonreg)'")
    g.send(CM_GUESTLOGIN, field(b"Bob", 15))   # already logged in: ignored
    g2 = Client(host, port)
    g2.send(CM_GUESTLOGIN, field(b"Bob", 15))
    check(g2.wait(g2.logged_in) and ("Bob2 (nonreg)", "", 1) in rows(db), "a second guest with the same name gets a number before the suffix")
    g3 = Client(host, port)
    g3.send(CM_GUESTLOGIN, field(b"Bobby Tables", 15))
    check(g3.wait(g3.logged_in) and ("Bobby (nonreg)", "", 1) in rows(db), "a long guest name is cut so the whole name fits 15 characters")
    g3.close()
    # a guest cannot take an empty city as its mayor
    g.send(CM_SETSTATE, bytes([5]))
    g.send(CM_JOBAPP, bytes([0]))
    check(g.wait(lambda: g.error() == "Y"), "a guest cannot found a city")
    g.close(); g2.close()
    deadline = time.time() + 5
    while time.time() < deadline and any(r[2] == 1 for r in rows(db)):
        time.sleep(0.2)
    check(not [r for r in rows(db) if r[2] == 1], "guest rows are deleted when the guests leave")

    # tickets
    t = Client(host, port)
    first = ticket(secret, "sub-maverick", "Maverick")
    t.send(CM_TICKETLOGIN, field(first.encode(), 200))
    check(t.wait(t.logged_in), "a valid ticket logs in")
    check(("Maverick", "sub-maverick", 0) in rows(db), "the character is named after the callsign and tied to the Outiva id")
    t.close()

    r = Client(host, port)
    r.send(CM_TICKETLOGIN, field(first.encode(), 200))
    check(r.wait(lambda: r.error() == "T") and not r.logged_in(), "a ticket cannot be used twice")
    r.close()

    again = Client(host, port)
    again.send(CM_TICKETLOGIN, field(ticket(secret, "sub-maverick", "Renamed").encode(), 200))
    check(again.wait(again.logged_in) and len(rows(db, "WHERE OutivaSub = 'sub-maverick'")) == 1, "the same Outiva id gets the same character back")
    again.close()

    clash = Client(host, port)
    clash.send(CM_TICKETLOGIN, field(ticket(secret, "sub-other", "Maverick").encode(), 200))
    check(clash.wait(clash.logged_in) and ("Maverick2", "sub-other", 0) in rows(db), "a callsign someone already has gets a number")
    clash.close()

    # a callsign cannot be made to look like a guest: parentheses are removed
    look = Client(host, port)
    look.send(CM_TICKETLOGIN, field(ticket(secret, "sub-look", "Eve (nonreg)").encode(), 200))
    check(look.wait(look.logged_in) and ("Eve nonreg", "sub-look", 0) in rows(db), "an Outiva callsign can never end in (nonreg)")
    look.close()

    for label, bad in (
        ("a ticket signed with another secret", ticket("some other secret value", "sub-x", "X")),
        ("an expired ticket", ticket(secret, "sub-x", "X", lifetime=-5)),
        ("a ticket that outlives the limit", ticket(secret, "sub-x", "X", lifetime=3600)),
        ("a garbled ticket", "v1.garbage.garbage"),
    ):
        b = Client(host, port)
        b.send(CM_TICKETLOGIN, field(bad.encode(), 200))
        check(b.wait(lambda: b.error() == "T") and not b.logged_in(), label + " is refused")
        b.close()
    check(not rows(db, "WHERE OutivaSub = 'sub-x'"), "refused tickets create nothing")

    # a ticket made by the real broker code is accepted by the server (the two sides agree on the format)
    broker = outiva_broker.Broker(secret, "https://accounts.outiva.com/realms/outiva", ["battlecity-web"], None)
    made, _ = broker.ticket_for({"sub": "sub-broker", "callsign": "Broker Bob"})
    k = Client(host, port)
    k.send(CM_TICKETLOGIN, field(made.encode(), 200))
    check(k.wait(k.logged_in) and ("Broker Bob", "sub-broker", 0) in rows(db), "a ticket from outiva_broker.py logs in")
    k.close()

    # an Outiva character can take an empty city
    m = Client(host, port)
    m.send(CM_TICKETLOGIN, field(ticket(secret, "sub-mayor", "Mayor Mo").encode(), 200))
    m.wait(m.logged_in)
    m.send(CM_SETSTATE, bytes([5]))
    m.send(CM_JOBAPP, bytes([0]))
    time.sleep(1)
    m.pump()
    check(m.error() != "Y", "an Outiva character is not stopped from founding a city")
    m.close()
    print("all Outiva login checks passed")


main()
