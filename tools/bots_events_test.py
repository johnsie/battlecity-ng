#!/usr/bin/env python3
"""Checks the time-driven bot features (raids, missions, bounty, supply crates, ghost, news) on a server started with
BC_BOTS_TEMPO=0.02 so that the gaps between events are seconds instead of minutes.

  1. a mayor builds something, then plays for a minute: expects RAID, MISSION, BOUNTY and SUPPLY CRATE announcements,
     picks up a crate, and expects raiders to shoot;
  2. the player leaves (the run is recorded for the ghost); a new player joins: expects a Ghost bot and a "Recent events"
     section in the login news.

Usage: tools/bots_events_test.py <host> <port>
"""
import importlib.util, math, re, struct, sys, time

spec = importlib.util.spec_from_file_location("bots_test", __file__.rsplit("/", 1)[0] + "/bots_test.py")
bt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bt)
CM, SM = bt.CM, bt.SM


class Watcher(bt.Player):
    def __init__(self, *a):
        super().__init__(*a)
        self.items = {}       # id -> (x, y, city, type)
        self.picked = []
        self.news = ""
        self.buildings = []

    def handle(self, pid, data):
        super().handle(pid, data)
        name = bt.SM_BY_ID.get(pid)
        if name == "smAddItem" and len(data) >= 10:
            x, y, city, typ, active, iid = struct.unpack("<HHBBBxH", data[:10])
            self.items[iid] = (x, y, city, typ)
        elif name == "smPickedUp":
            self.picked.append(data)
        elif name == "smAppendNews":
            self.news += data.split(b"\0")[0].decode(errors="replace")
        elif name in ("smNewBuilding", "smPlaceBuilding"):
            self.buildings.append(pid)


def main():
    host, port = sys.argv[1], int(sys.argv[2])
    tag = str(int(time.time()) % 100000).encode()
    a = Watcher(host, port, b"evt" + tag)
    a.login(0)
    a.send(CM["cmNextStep"], bytes([65]))
    if not a.wait(lambda: a.x is not None):
        raise SystemExit("FAIL: the test player never joined")
    x0, y0 = float(a.x), float(a.y)
    # as mayor, build whatever the city may build, a little way from the command centre
    for typ in range(1, 12):
        a.send(CM["cmBuild"], struct.pack("<HHBB", int(x0 / 48) - 6 - typ * 4 % 12, int(y0 / 48) + 6, typ, 0))
        time.sleep(0.05)
    a.wait(lambda: False, 0.5)
    print("INFO: building acknowledgements:", len(a.buildings))

    t0 = time.time()
    px, py = x0, y0
    nxt = 0
    t = 0.0
    seen = set()
    picked_crate = False
    crate_tile = None
    requested = False
    def done():
        return {"RAID!", "MISSION:", "BOUNTY:", "SUPPLY CRATE"} <= {k for k, _ in seen} and picked_crate and any(
            a.names.get(s[0], ("", ""))[1] in ("Raiders", "Bounty") for s in a.shots) and t > 4.5   # long enough for the ghost recording

    while time.time() - t0 < 60 and not done():
        now = time.time()
        if now >= nxt:
            nxt = now + 0.15
            t += 0.15
            if crate_tile and not picked_crate:
                # walk to the announced crate like a player would; ask for its sector when close, as the real client does
                cx, cy = crate_tile[0] * 48, crate_tile[1] * 48
                dx, dy = cx - px, cy - py
                dist = math.hypot(dx, dy)
                if dist < 700 and not requested:
                    a.send(CM["cmRequestSector"], bytes([crate_tile[0] // 16, crate_tile[1] // 16]))
                    requested = True
                if dist < 60 and a.items:
                    iid, (ix, iy, city, typ) = next(iter(a.items.items()))
                    a.send(CM["cmItemUp"], struct.pack("<HbB", iid, 0, typ))
                    a.wait(lambda: a.picked, 1)
                    picked_crate = bool(a.picked)
                elif dist >= 60:
                    step = min(dist, 0.38 * 150 * 0.9)
                    px += dx / dist * step
                    py += dy / dist * step
                    a.update(px, py, bt.dir_to(dx, dy), 2)
                else:
                    a.update(px, py, 0, 1)
            else:
                # otherwise walk a slow loop near the start so the recording has movement
                px = x0 + 200 * math.sin(t / 3.0) if not picked_crate else px + 8 * math.sin(t)
                py = y0 + 200 * math.cos(t / 3.0) if not picked_crate else py + 8 * math.cos(t)
                a.update(px, py, int(t * 2) % 32, 2)
        a.pump()
        for _, text in a.chat:
            m = re.search(r"SUPPLY CRATE .* at tile (\d+),(\d+)", text)
            if m and crate_tile is None:
                crate_tile = (int(m.group(1)), int(m.group(2)))
            for key in ("RAID!", "MISSION:", "BOUNTY:", "SUPPLY CRATE", "Zone:", "Warlord", "raid", "MISSION COMPLETE", "Mission failed", "Daily"):
                if key in text and (key, text[:30]) not in seen:
                    seen.add((key, text[:30]))
                    print("CHAT:", text)
        a.chat.clear()
        time.sleep(0.01)

    kinds = {k for k, _ in seen}
    bt.check("RAID!" in kinds, "a raid was announced", "no raid announcement within the window")
    bt.check("MISSION:" in kinds, "a mission was announced", "no mission announcement")
    bt.check("BOUNTY:" in kinds, "a bounty boss was announced", "no bounty announcement")
    bt.check("SUPPLY CRATE" in kinds, "a supply crate was announced", "no supply crate announcement")
    bt.check(picked_crate, "the crate was picked up by the city", "the crate could not be picked up (items seen: %s)" % a.items)
    bt.check(len(a.items) >= 1, "the crate reached the client as an item (%d seen)" % len(a.items), "no smAddItem for a crate")
    fired = [s for s in a.shots if a.names.get(s[0], ("", ""))[1] in ("Raiders", "Bounty")]
    bt.check(len(fired) >= 1, "bots shot (%d shots)" % len(fired), "bots never shot during the minute")

    # leave, then a second player joins and must see the ghost and the news
    a.s.close()
    time.sleep(2)
    b = Watcher(host, port, b"ghs" + tag)
    # like the real client: enter the meeting room first (that is when the login-screen news arrives), then apply to a city
    b.send(CM["cmVersion"], bt.field(b"bots", 50) + bt.field(bt.VERSION, 10))
    time.sleep(0.05)
    user = bt.field(b.name, 15)
    b.send(CM["cmNewAccount"], user + bt.field(b"pw", 15) + bt.field(b.name + b"@example.com", 50) + bt.field(b"Bots", 20) + bt.field(b"T", 15) + bt.field(b"S", 15))
    time.sleep(0.05)
    b.send(CM["cmLogin"], user + bt.field(b"pw", 15))
    time.sleep(0.05)
    b.send(CM["cmSetState"], bytes([65]))
    b.wait(lambda: "Recent events" in b.news, 3)
    bt.check("Recent events" in b.news and "Raiders attacked" in b.news, "the login news lists recent events", "no events in the login news: %r" % b.news[-200:])
    b.send(CM["cmSetState"], bytes([5]))
    b.send(CM["cmJobApp"], bytes([1]))
    b.send(CM["cmNextStep"], bytes([65]))
    if not b.wait(lambda: b.x is not None):
        raise SystemExit("FAIL: the second player never joined")
    bx, by = float(b.x), float(b.y)
    end = time.time() + 8
    while time.time() < end:
        b.update(bx, by)
        b.pump()
        time.sleep(0.15)
    ghosts = [n for n, t in b.names.values() if n.startswith("Ghost:")]
    bt.check(len(ghosts) >= 1, "a ghost appeared: %s" % ghosts, "no ghost bot: names %s" % list(b.names.values()))
    print("INFO: news seen on login (%d chars)" % len(b.news))
    if bt.check.failed:
        raise SystemExit(1)
    print("OK: events work")


if __name__ == "__main__":
    main()
