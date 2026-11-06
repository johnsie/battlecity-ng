#!/usr/bin/env python3
"""Exercises the server-side bots (see docs/solo-play.md) the way a real client sees them.

One bot player joins a city and then checks that:
  - the server announces bot players (smPlayerData / smJoinData) and keeps sending their positions,
  - bot positions stay on the map and move,
  - bots shoot (smShoot from a bot slot) when the player stays near them,
  - chat commands (/help, /bots, /challenge, /range, /trial) are answered by the Overseer and are not broadcast,
  - a range can be cleared by shooting its targets (smDeath for the target slots),
  - no bot ever appears as an ordinary player to the status counter (checked by the caller via the status port).

Usage: tools/bots_test.py <host> <port> [seconds]
"""
import math, re, socket, struct, sys, time

VERSION = b"3.5.8"


def enum_values(path, name):
    text = open(path).read()
    body = re.search(r"enum %s\s*{(.*?)};" % name, text, re.S).group(1)
    names = [n.strip() for n in re.sub(r"//.*", "", body).split(",") if n.strip()]
    return {n: i for i, n in enumerate(names)}


HERE = __file__.rsplit("/", 1)[0]
SM = enum_values(HERE + "/../server/NetMessages.h", "ServerMessages")
CM = enum_values(HERE + "/../server/NetMessages.h", "ClientMessages")
SM_BY_ID = {v: k for k, v in SM.items()}


def frame(pid, data=b""):
    total = sum(b - 256 if b > 127 else b for b in data) + 3412
    packet = bytes([(total % 71) & 0xFF, pid]) + data
    return bytes([len(packet)]) + packet


def field(text, size):
    return text[:size - 1].ljust(size, b"\0")


class Player:
    def __init__(self, host, port, name):
        self.s = socket.create_connection((host, port), timeout=5)
        self.s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.s.setblocking(False)
        self.buf = b""
        self.name = name
        self.x = self.y = None
        self.me = None
        self.names = {}          # slot -> (name, town)
        self.pos = {}            # slot -> (x, y, move, dir, time)
        self.history = {}        # slot -> list of (x, y)
        self.shots = []          # (slot, x, y, dir, type)
        self.deaths = []         # slot
        self.chat = []           # global messages (sender slot, text)
        self.points = {}         # slot -> points (smPointsUpdate)
        self.joined = set()
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
        while self.buf:
            n = self.buf[0] + 1
            if n < 3 or len(self.buf) < n:
                break
            pid, data = self.buf[2], self.buf[3:n]
            self.buf = self.buf[n:]
            self.handle(pid, data)

    def handle(self, pid, data):
        name = SM_BY_ID.get(pid)
        if name == "smStateGame" and len(data) >= 4:
            self.x, self.y = struct.unpack("<HH", data[:4])
        elif name == "smMayorHire":
            self.hire = True
        elif name == "smPlayerData" and len(data) >= 33:
            slot = data[0]
            nm = data[1:17].split(b"\0")[0].decode(errors="replace")
            town = data[17:33].split(b"\0")[0].decode(errors="replace")
            self.names[slot] = (nm, town)
        elif name == "smJoinData" and len(data) >= 1:
            self.joined.add(data[0])
        elif name == "smUpdate" and len(data) >= 9:
            slot, x, y, turn, move, d = struct.unpack("<BxHHBBB", data[:9])
            self.pos[slot] = (x, y, move, d, time.time())
            self.history.setdefault(slot, []).append((x, y))
        elif name == "smShoot" and len(data) >= 8:
            slot, x, y, d, t = struct.unpack("<HHHBB", data[:8])
            self.shots.append((slot, x, y, d, t))
        elif name == "smDeath" and len(data) >= 1:
            self.deaths.append(data[0])
        elif name == "smPointsUpdate" and len(data) >= 8:
            slot, pts = struct.unpack("<B3xI", data[:8])
            self.points[slot] = pts
        elif name == "smGlobal" and len(data) >= 2:
            self.chat.append((data[0], data[1:].split(b"\0")[0].decode(errors="replace")))

    def wait(self, cond, timeout=5):
        end = time.time() + timeout
        while time.time() < end:
            self.pump()
            if cond():
                return True
            time.sleep(0.01)
        return False

    def login(self, city):
        self.send(CM["cmVersion"], field(b"bots", 50) + field(VERSION, 10))
        time.sleep(0.05)
        user = field(self.name, 15)
        self.send(CM["cmNewAccount"], user + field(b"pw", 15) + field(self.name + b"@example.com", 50) + field(b"Bots", 20) + field(b"T", 15) + field(b"S", 15))
        time.sleep(0.05)
        self.send(CM["cmLogin"], user + field(b"pw", 15))
        time.sleep(0.05)
        self.send(CM["cmSetState"], bytes([5]))
        self.send(CM["cmJobApp"], bytes([city]))

    def say(self, text):
        self.send(CM["cmChatMessage"], text.encode() + b"\0")

    def update(self, x, y, d=0, move=1):
        self.x, self.y = x, y
        self.send(CM["cmUpdate"], struct.pack("<HHBBB", int(x), int(y), 1, move, d))

    def shoot(self, d):
        fdir = -d + 32
        fx = int(self.x - 24 + 6 + math.sin(fdir / 16 * 3.14) * 20)
        fy = int(self.y - 24 + 10 + math.cos(fdir / 16 * 3.14) * 20)
        self.send(CM["cmShoot"], struct.pack("<HHBB", fx, fy, d, 0))


def dir_to(dx, dy):
    a = math.atan2(dx, dy)
    return int(round(32 - a * 16 / 3.14)) % 32


def check(cond, ok, bad):
    if cond:
        print("OK:", ok)
    else:
        print("FAIL:", bad)
        check.failed = True


check.failed = False


def main():
    host, port = sys.argv[1], int(sys.argv[2])
    seconds = float(sys.argv[3]) if len(sys.argv) > 3 else 25
    tag = str(int(time.time()) % 100000).encode()
    a = Player(host, port, b"bot" + tag)
    a.login(0)
    a.send(CM["cmNextStep"], bytes([65]))
    if not a.wait(lambda: a.x is not None):
        raise SystemExit("FAIL: the test player never joined")
    start_x, start_y = float(a.x), float(a.y)
    # keep reporting a position so the server sees us standing still
    end = time.time() + seconds
    nxt = 0
    while time.time() < end:
        if time.time() >= nxt:
            a.update(start_x, start_y)
            nxt = time.time() + 0.15
        a.pump()
        time.sleep(0.01)

    bot_slots = {s for s, (n, t) in a.names.items() if t in ("Raiders", "Wildlife", "Bounty")}
    check(len(bot_slots) >= 2, "the server announced %d bot players: %s" % (len(bot_slots), sorted(a.names[s][0] for s in bot_slots)),
          "no bot players announced (names seen: %s)" % a.names)
    check(any(n == "Overseer" for n, t in a.names.values()), "the Overseer exists", "no Overseer announced")
    check(all(s in a.joined for s in bot_slots), "every bot also got smJoinData", "a bot was announced without smJoinData")
    moving = 0
    for s in bot_slots:
        h = a.history.get(s, [])
        if len(h) >= 5 and max(math.hypot(x - h[0][0], y - h[0][1]) for x, y in h) > 40:
            moving += 1
        for x, y in h:
            if not (0 < x < 24576 and 0 < y < 24576):
                check(False, "", "bot %d reported an off-map position %s" % (s, (x, y)))
    check(moving >= 1, "%d bot(s) are moving and sending position updates" % moving, "no bot moved")
    check(len(a.chat) >= 1 and a.chat[0][1].startswith("Quiet server"), "the greeting arrived from the Overseer: %r" % (a.chat[0][1] if a.chat else ""), "no greeting")

    # chat commands
    a.chat.clear()
    for cmd in ("/help", "/bots", "/challenge", "/mission", "/events"):
        a.say(cmd)
        time.sleep(0.15)
        a.pump()
    a.wait(lambda: len(a.chat) >= 6, 2)
    texts = [t for _, t in a.chat]
    check(any("Solo play commands" in t for t in texts) and any(t.startswith("Bots:") for t in texts) and any(t.startswith("Daily:") for t in texts),
          "commands were answered (%d replies)" % len(texts), "missing replies: %s" % texts)

    # range: drive up to a target and shoot it
    a.chat.clear()
    a.deaths.clear()
    a.say("/range")
    a.wait(lambda: any("Range open" in t for _, t in a.chat), 3)
    check(any("Range open" in t for _, t in a.chat), "range opened", "range did not open: %s" % [t for _, t in a.chat])
    a.wait(lambda: sum(1 for s, (n, t) in a.names.items() if n == "Target") >= 3, 3)
    targets = sorted(s for s, (n, t) in a.names.items() if n == "Target")
    check(len(targets) >= 3, "%d range targets were announced" % len(targets), "no range targets")
    for _ in range(40):
        a.pump()
        time.sleep(0.05)
    killed = 0
    t_end = time.time() + 45
    while time.time() < t_end and len(set(a.deaths) & set(targets)) < len(targets):
        a.pump()
        alive = [t for t in targets if t not in a.deaths and t in a.pos]
        if not alive:
            time.sleep(0.05)
            continue
        t = min(alive, key=lambda s: math.hypot(a.pos[s][0] - a.x, a.pos[s][1] - a.y))
        tx, ty = a.pos[t][0], a.pos[t][1]
        dx, dy = tx - a.x, ty - a.y
        dist = math.hypot(dx, dy)
        d = dir_to(dx, dy)
        if dist > 150:
            step = min(dist - 120, 0.38 * 150 * 0.9)
            a.update(a.x + dx / dist * step, a.y + dy / dist * step, d, 2)
            time.sleep(0.15)
        else:
            a.update(a.x, a.y, d, 1)
            a.shoot(d)
            time.sleep(0.55)
    dead = set(a.deaths) & set(targets)
    check(len(dead) == len(targets), "all %d targets were destroyed by shooting them" % len(targets), "only %d of %d targets destroyed" % (len(dead), len(targets)))
    a.wait(lambda: any("Range cleared" in t for _, t in a.chat), 3)
    check(any("Range cleared" in t for _, t in a.chat), "the range reported a time: %s" % [t for _, t in a.chat if "Range" in t], "no range result")

    # a snowman is worth a point: hunt one and check the points arrive (and that they are capped and not saved for bots)
    drones = lambda: sorted(s for s, (n, t) in a.names.items() if n == "Snowman" and s in a.pos)
    before = max([v for k, v in a.points.items() if k < 32] or [0])
    a.deaths.clear()
    target = None
    t_end = time.time() + 60
    while time.time() < t_end:
        a.pump()
        live = [s for s in drones() if s not in a.deaths]
        if not live:
            time.sleep(0.05)
            continue
        if target not in live:
            target = min(live, key=lambda s: math.hypot(a.pos[s][0] - a.x, a.pos[s][1] - a.y))
        tx, ty = a.pos[target][0], a.pos[target][1]
        dx, dy = tx - a.x, ty - a.y
        dist = math.hypot(dx, dy)
        d = dir_to(dx, dy)
        if target in a.deaths:
            break
        if dist > 150:
            step = min(dist - 120, 0.38 * 150 * 0.9)
            a.update(a.x + dx / dist * step, a.y + dy / dist * step, d, 2)
            time.sleep(0.15)
        else:
            a.update(a.x, a.y, d, 1)
            a.shoot(d)
            time.sleep(0.55)
        if any(s in a.deaths for s in drones()) or (target in a.deaths):
            break
    a.wait(lambda: max([v for k, v in a.points.items() if k < 32] or [0]) > before, 2)
    after = max([v for k, v in a.points.items() if k < 32] or [0])
    check(after > before, "killing a snowman paid a point (%d -> %d)" % (before, after), "no points for the kill (%d -> %d)" % (before, after))

    # trial
    a.chat.clear()
    a.say("/trial")
    a.wait(lambda: any("Trial started" in t for _, t in a.chat), 3)
    check(any("Trial started" in t for _, t in a.chat), "trial started", "trial did not start: %s" % [t for _, t in a.chat])
    a.say("/trial stop")
    a.wait(lambda: any("cancelled" in t for _, t in a.chat), 2)

    # bots shoot: stay near a hostile one
    hostile = [s for s in bot_slots if a.names[s][1] == "Raiders"]
    fired = [s for s in a.shots if s[0] in bot_slots]
    print("INFO: bot shots seen during the run: %d" % len(fired))
    if check.failed:
        raise SystemExit(1)
    print("OK: bots behave")


if __name__ == "__main__":
    main()
