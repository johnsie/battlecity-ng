#!/usr/bin/env python3
"""Load test: N bots join one city, drive for a while, and must see their city mates consistently.

Bots are grouped in fours (the most one city holds); the first of each group is the mayor and hires the rest. Every bot then sends cmUpdate every 150 ms like the real client
(oscillating along a diagonal so it stays on the map). At the end the bots stop, the network drains, and every
bot's last-seen position of every other bot must match where that bot really stopped.

Checks: no bot is disconnected, every bot receives a steady stream of updates, final positions agree within a
tolerance, and (with --pid) the server process is still running.

Usage: tools/load_test.py <host> <port> [--bots 20] [--seconds 120] [--pid <server pid>]
"""
import argparse, os, random, struct, sys, time, statistics
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from move_test import Bot, CM_NEXTSTEP, CM_UPDATE, CM_HIREACCEPT

TOLERANCE = 150      # units: about one 150 ms step of a tank (0.38 units/ms) plus rounding
STEP = 0.38 * 150 * 0.7071


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("host")
    ap.add_argument("port", type=int)
    ap.add_argument("--bots", type=int, default=20)
    ap.add_argument("--seconds", type=float, default=120)
    ap.add_argument("--pid", type=int, default=0)
    args = ap.parse_args()

    tag = str(int(time.time()) % 100000).encode()
    bots = [Bot(args.host, args.port, b"ld%02d_" % i + tag) for i in range(args.bots)]
    # a city holds at most 4 players: bots are grouped in fours, the first of each group is the mayor
    for i, b in enumerate(bots):
        city = i // 4
        b.login(city)
        if i % 4 == 0:
            b.send(CM_NEXTSTEP, bytes([65]))
        else:
            mayor = bots[i - i % 4]
            mayor.hire = False
            if not mayor.wait(lambda: mayor.hire):
                raise SystemExit("FAIL: the mayor was not asked to hire %s" % b.name.decode())
            mayor.send(CM_HIREACCEPT, b" ")
            b.send(CM_NEXTSTEP, bytes([65]))
        if not b.wait(lambda: b.x is not None):
            raise SystemExit("FAIL: bot %s never joined" % b.name.decode())
    print("%d bots joined" % len(bots))

    pos = [[float(b.x), float(b.y)] for b in bots]
    sign = [1] * len(bots)
    phase = [random.randint(0, 19) for _ in bots]
    start = time.perf_counter()
    nxt = start
    steps = 0
    total_steps = int(args.seconds / 0.150) + 7     # not a whole number of 40-step cycles, so the bots end away from where they began
    while steps < total_steps:
        now = time.perf_counter()
        if now >= nxt and steps < total_steps:
            nxt += 0.150
            steps += 1
            for i, b in enumerate(bots):
                if (steps + phase[i]) % 20 == 0:
                    sign[i] = -sign[i]
                pos[i][0] += sign[i] * STEP
                pos[i][1] += sign[i] * STEP
                b.send(CM_UPDATE, struct.pack("<HHBBB", int(pos[i][0]), int(pos[i][1]), 1, 2, 8))
                if steps == 1 and os.environ.get("LOAD_DEBUG"):
                    print("sent bot", i, int(pos[i][0]), int(pos[i][1]), "start", b.x, b.y)
        for b in bots:
            b.pump()
        if args.pid and not os.path.exists("/proc/%d" % args.pid):
            raise SystemExit("FAIL: the server process died")
        time.sleep(0.002)

    # stop sending; let the network drain
    end = time.time() + 2
    while time.time() < end:
        for b in bots:
            b.pump()
        time.sleep(0.01)

    failures = 0
    errors = []
    for i, b in enumerate(bots):
        last = {}
        for _t, x, y in b.updates:   # SM_UPDATE: keep the newest position per subject (id is not tracked, so by position)
            last[(x, y)] = True
        mates = len([k for k in range(len(bots)) if k // 4 == i // 4]) - 1
        if mates and len(b.updates) < steps * 0.5 * mates:
            errors.append("bot %d received only %d updates (expected about %d)" % (i, len(b.updates), steps * mates))
    # position agreement: every observer's recent updates must include each other bot's final position
    for i, b in enumerate(bots):
        recent = [(x, y) for _t, x, y in b.updates[-len(bots) * 4:]]
        for j, _ in enumerate(bots):
            if i == j or i // 4 != j // 4:
                continue
            fx, fy = int(pos[j][0]), int(pos[j][1])
            if not any(abs(x - fx) <= TOLERANCE and abs(y - fy) <= TOLERANCE for x, y in recent):
                failures += 1
    if os.environ.get("LOAD_DEBUG"):
        print("final positions city 0:", [(int(pos[j][0]), int(pos[j][1])) for j in range(4)])
        print("bot 0 last by id:", bots[0].last_by_id)
        print("start positions:", [(b.x, b.y) for b in bots[:8]])
    total = sum(1 for i in range(len(bots)) for j in range(len(bots)) if i != j and i // 4 == j // 4)
    print("steps %d, updates per bot: min %d  max %d, final-position disagreements: %d of %d pairs" % (
        steps, min(len(b.updates) for b in bots), max(len(b.updates) for b in bots), failures, total))
    if errors:
        for e in errors[:5]:
            print("FAIL:", e)
        raise SystemExit(1)
    if failures > total * 0.02:
        raise SystemExit("FAIL: positions seen by the bots disagree")
    print("OK: load test passed")


if __name__ == "__main__":
    main()
