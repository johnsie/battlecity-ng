#!/usr/bin/env python3
"""Waypoints (docs/waypoints.md): every player is told where their Command Center is, and everybody far from it is told where the
latest fight (a death) happened. Server started with BC_BOTS=0. Usage: tools/waypoints_test.py <host> <port>"""
import importlib.util, struct, sys, time

spec = importlib.util.spec_from_file_location("bots_test", __file__.rsplit("/", 1)[0] + "/bots_test.py")
bt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bt)
CM = bt.CM


class W(bt.Player):
    def __init__(self, *a):
        super().__init__(*a)
        self.lists = []

    def handle(self, pid, data):
        super().handle(pid, data)
        if bt.SM_BY_ID.get(pid) == "smWaypoints" and len(data) >= 1:
            n = data[0]
            if len(data) >= 1 + 5 * n:
                self.lists.append([(data[1 + 5 * i], *struct.unpack("<HH", data[2 + 5 * i:6 + 5 * i])) for i in range(n)])


def join(host, port, name, city):
    p = W(host, port, name)
    p.login(city)
    p.send(CM["cmNextStep"], bytes([65]))
    if not p.wait(lambda: p.x is not None):
        raise SystemExit("FAIL: %s never joined" % name)
    return p


def main():
    host, port = sys.argv[1], int(sys.argv[2])
    tag = str(int(time.time()) % 100000).encode()
    a = join(host, port, b"wpa" + tag, 0)
    b = join(host, port, b"wpb" + tag, 1)
    ok = a.wait(lambda: a.lists, 3)
    bt.check(ok, "a player is sent a waypoint list", "no smWaypoints within 3 s")
    if ok:
        home = [w for w in a.lists[-1] if w[0] == 0]
        bt.check(len(home) == 1 and abs(home[0][1] - a.x) <= 60 and abs(home[0][2] - a.y) <= 60, "the list holds their Command Center (%s, joined at %s,%s)" % (home, a.x, a.y), "wrong or missing home point: %s (at %s,%s)" % (a.lists[-1], a.x, a.y))
    bt.check(all(w[0] != 7 for w in a.lists[-1]), "no fight is shown before anything happened", "a fight appeared out of nowhere")
    # b dies: everybody far away is pointed to the spot
    bx, by = b.x, b.y
    b.send(CM["cmDeath"], bytes([0]))
    a.lists.clear()
    ok = a.wait(lambda: any(w[0] == 7 for l in a.lists for w in l), 4)
    bt.check(ok, "a death shows up as the latest fight for a player far away", "no fight waypoint after a death")
    if ok:
        f = [w for l in a.lists for w in l if w[0] == 7][-1]
        bt.check(abs(f[1] - bx) <= 100 and abs(f[2] - by) <= 100, "...at the right place (%s vs %s,%s)" % (f, bx, by), "fight point %s is not where the player died (%s,%s)" % (f, bx, by))
    if bt.check.failed:
        raise SystemExit(1)
    print("OK: waypoints work")


if __name__ == "__main__":
    main()
