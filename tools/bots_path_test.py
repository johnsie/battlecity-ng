#!/usr/bin/env python3
"""A bot that has a lake or a rock wall between itself and a player must find its way round (it used to stay stuck in front of it).

Needs a server started with BC_TEST_TURRET=1 BC_BOTS_SAFE_RADIUS=0 (the /testbot hook). The test reads the map, picks a spot near the
player where the straight line to a scout is blocked but a route exists, walks the player there, puts a scout on the other side and
checks that the scout comes within shooting range.

Usage: tools/bots_path_test.py <host> <port> <map.dat>
"""
import collections, math, random, re, sys, time
sys.path.insert(0, __import__("os").path.dirname(__file__))
import bots_test as bt

def main():
    host, port, mapfile = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    raw = open(mapfile, "rb").read()
    def tile(x, y): return raw[x * 512 + y]
    def free(x, y):
        return all(2 <= x + i <= 509 and 2 <= y + j <= 509 and tile(x + i, y + j) == 0 for i in (-1, 0, 1) for j in (-1, 0, 1))
    def blocked_line(a, b):
        n = int(max(abs(a[0] - b[0]), abs(a[1] - b[1])) * 3)
        return any(tile(round(a[0] + (b[0] - a[0]) * i / n), round(a[1] + (b[1] - a[1]) * i / n)) != 0 for i in range(n + 1))
    def route_len(a, b):
        seen = {a: 0}; q = collections.deque([a])
        while q:
            c = q.popleft()
            if max(abs(c[0] - b[0]), abs(c[1] - b[1])) <= 1: return seen[c]
            if seen[c] > 80: continue
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    n = (c[0] + dx, c[1] + dy)
                    if n in seen or not (2 <= n[0] <= 509 and 2 <= n[1] <= 509) or tile(*n) != 0: continue
                    if dx and dy and (tile(c[0] + dx, c[1]) != 0 or tile(c[0], c[1] + dy) != 0): continue
                    seen[n] = seen[c] + 1; q.append(n)
        return None

    tag = str(int(time.time()) % 100000).encode()
    a = bt.Player(host, port, b"path" + tag)
    a.login(0)
    a.send(bt.CM["cmNextStep"], bytes([65]))
    if not a.wait(lambda: a.x is not None):
        raise SystemExit("FAIL: the test player never joined")
    sx, sy = int(a.x) // 48, int(a.y) // 48

    rng = random.Random(7)
    scene = None
    for _ in range(200000):
        p = (sx + rng.randint(-40, 40), sy + rng.randint(-40, 40))
        d = rng.randint(8, 13); ang = rng.random() * 6.2832
        s = (p[0] + round(math.cos(ang) * d), p[1] + round(math.sin(ang) * d))
        if not (free(*p) and free(*s)) or not blocked_line(p, s): continue
        r = route_len(s, p)
        if r is not None and 14 <= r <= 45:
            scene = (p, s, r); break
    bt.check(scene is not None, "found a blocked spot near the start: player tile %s, scout tile %s, route %s tiles" % scene if scene else "", "no suitable spot on the map")
    (px, py), (bx, by), rl = scene

    # walk the player there in steps the server accepts
    target = (px * 48.0, py * 48.0)
    x, y = float(a.x), float(a.y)
    t_end = time.time() + 60
    while math.hypot(target[0] - x, target[1] - y) > 1 and time.time() < t_end:
        dx, dy = target[0] - x, target[1] - y
        d = math.hypot(dx, dy); st = min(d, 60.0)
        x, y = x + dx / d * st, y + dy / d * st
        a.update(x, y)
        time.sleep(0.1)
        a.pump()
    for _ in range(10):
        a.update(target[0], target[1]); time.sleep(0.1); a.pump()
    a.chat.clear()
    a.say("/testbot %d %d" % (bx - px, by - py))
    a.wait(lambda: any(t.startswith("Test bot") for _, t in a.chat), 3)
    m = [re.search(r"Test bot (\d+)", t) for _, t in a.chat if t.startswith("Test bot")]
    bt.check(m and m[0], "a scout was placed %d tiles away behind an obstacle" % int(math.hypot(bx - px, by - py)), "the test hook did not answer (is BC_TEST_TURRET=1 set?)")
    slot = int(m[0].group(1))

    best = 1e9
    t_end = time.time() + 75
    while time.time() < t_end and best > 330:
        a.update(target[0], target[1])
        a.pump()
        if slot in a.pos:
            best = min(best, math.hypot(a.pos[slot][0] - target[0], a.pos[slot][1] - target[1]))
        time.sleep(0.1)
    if best > 330 and slot in a.pos: print("INFO: scout ended at", a.pos[slot][:2], "player", target, "scout tile", a.pos[slot][0] // 48, a.pos[slot][1] // 48)
    bt.check(best <= 330, "the scout found its way round the obstacle (closest %.0f px, route %d tiles)" % (best, rl), "the scout never got round the obstacle (closest %.0f px)" % best)
    sys.exit(1 if bt.check.failed else 0)

if __name__ == "__main__":
    main()
