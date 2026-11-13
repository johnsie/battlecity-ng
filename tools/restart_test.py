#!/usr/bin/env python3
"""A server restart (every update) must not throw the players out of the game (CWorldState.cpp).

Starts a server, puts a mayor and a second player in two cities, moves the mayor away from the Command Center, stops the server
(SIGTERM, as systemd does), starts it again and lets both accounts log in. Each must be sent straight into the game (no meeting room),
at the place it had reached, the mayor as mayor again; a player who was not in the game must get the ordinary login.

Usage: tools/restart_test.py <bc-server> <map.dat> <port>
"""
import importlib.util, os, shutil, signal, subprocess, sys, tempfile, time

spec = importlib.util.spec_from_file_location("bots_test", __file__.rsplit("/", 1)[0] + "/bots_test.py")
bt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bt)
CM = bt.CM


class P(bt.Player):
    def __init__(self, *a):
        super().__init__(*a)
        self.login_slot = None
        self.steps = []

    def handle(self, pid, data):
        super().handle(pid, data)
        name = bt.SM_BY_ID.get(pid)
        if pid == 1 and len(data) >= 2:
            self.login_slot = data[0]
        elif name == "smNextStep":
            self.steps.append(data[:1])


def start(binary, work, port):
    env = dict(os.environ, BC_BOTS="0", BC_PORT=str(port), BC_STATUS_PORT=str(port + 1))
    log = open(os.path.join(work, "server.log"), "ab")
    proc = subprocess.Popen([binary], cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT)
    for _ in range(100):
        try:
            bt.socket.create_connection(("127.0.0.1", port), timeout=1).close()
            return proc
        except OSError:
            time.sleep(0.1)
    raise SystemExit("FAIL: the server did not start")


def main():
    binary, mapfile, port = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]), int(sys.argv[3])
    work = tempfile.mkdtemp()
    shutil.copy(mapfile, os.path.join(work, "map.dat"))
    tag = str(int(time.time()) % 100000).encode()
    proc = start(binary, work, port)
    try:
        mayor = P("127.0.0.1", port, b"mym" + tag)
        mayor.login(0)
        mayor.send(CM["cmNextStep"], bytes([65]))
        member = P("127.0.0.1", port, b"oth" + tag)
        member.login(5)
        member.send(CM["cmNextStep"], bytes([65]))
        idle = P("127.0.0.1", port, b"idl" + tag)       # logged in but never joined a city
        idle.send(CM["cmVersion"], bt.field(b"bots", 50) + bt.field(bt.VERSION, 10))
        time.sleep(0.05)
        idle.send(CM["cmNewAccount"], bt.field(idle.name, 15) + bt.field(b"pw", 15) + bt.field(idle.name + b"@example.com", 50) + bt.field(b"Bots", 20) + bt.field(b"T", 15) + bt.field(b"S", 15))
        time.sleep(0.05)
        idle.send(CM["cmLogin"], bt.field(idle.name, 15) + bt.field(b"pw", 15))
        for p in (mayor, member):
            if not p.wait(lambda: p.x is not None):
                raise SystemExit("FAIL: a test player never joined")
        # the mayor builds a house, which must also survive the restart (cmBuild: tile x, tile y, type 2 = House)
        mayor.send(CM["cmBuild"], bt.struct.pack("<HHBB", int(mayor.x) // 48 + 5, int(mayor.y) // 48, 2, 0))
        mayor.wait(lambda: False, 0.5)
        spots = {}
        for p in (mayor, member):
            x0, y0 = p.x, p.y
            for i in range(1, 4):
                p.update(x0 + 60 * i, y0 + 40 * i, 3, 1)
                time.sleep(0.15)
            p.wait(lambda: False, 0.3)
            spots[p] = (x0 + 180, y0 + 120)

        # stop the server the way systemd does and start it again
        proc.send_signal(signal.SIGTERM)
        proc.wait(timeout=20)
        saved = os.path.exists(os.path.join(work, "dat", "world.state"))
        bt.check(saved, "the world was written when the server stopped", "no dat/world.state after the server stopped")
        proc = start(binary, work, port)

        back = {}
        for p, label in ((mayor, "the mayor"), (member, "the other player")):
            q = P("127.0.0.1", port, p.name)
            q.send(CM["cmVersion"], bt.field(b"bots", 50) + bt.field(bt.VERSION, 10))
            time.sleep(0.05)
            q.send(CM["cmLogin"], bt.field(q.name, 15) + bt.field(b"pw", 15))
            q.send(CM["cmSetState"], bytes([67]))      # the real client opens its meeting room window at once and says so; it must not undo the restore
            ok = q.wait(lambda: q.steps and q.login_slot is not None, 3)
            bt.check(ok, "%s was sent straight into the game after the restart (no meeting room)" % label, "%s got no smNextStep after the restart" % label)
            if not ok:
                continue
            q.send(CM["cmNextStep"], bytes([65]))
            bt.check(q.wait(lambda: q.x is not None, 3), "%s started playing again" % label, "%s got no smStateGame" % label)
            tx, ty = spots[p]
            bt.check(q.x is not None and abs(q.x - tx) <= 2 and abs(q.y - ty) <= 2, "...at the place they had reached (%s,%s)" % (q.x, q.y), "placed at %s,%s instead of %s,%s" % (q.x, q.y, tx, ty))
            back[p] = q

        # the player who never joined a city gets the ordinary login
        r = P("127.0.0.1", port, idle.name)
        r.send(CM["cmVersion"], bt.field(b"bots", 50) + bt.field(bt.VERSION, 10))
        time.sleep(0.05)
        r.send(CM["cmLogin"], bt.field(r.name, 15) + bt.field(b"pw", 15))
        r.wait(lambda: r.login_slot is not None, 3)
        r.wait(lambda: False, 0.5)
        bt.check(r.login_slot is not None and not r.steps, "a player who was not in the game gets the ordinary login", "unexpected game start for the idle player: %s" % r.steps)

        time.sleep(0.5)
        log = open(os.path.join(work, "server.log"), "rb").read().decode(errors="replace")
        bt.check("Restored::" + mayor.name.decode() in log and "as mayor" in log, "the mayor is mayor again", "no 'as mayor' restore in the server log")
        bt.check("World::Saved 2 cities, 1 buildings" in log, "the city and its house were saved", "the save line is missing or has the wrong counts")
        bt.check("World::Loaded 2 cities, 1 buildings" in log, "the server read the world back", "the load line is missing or has the wrong counts")
    finally:
        proc.kill()
        proc.wait()
        if bt.check.failed:
            print(open(os.path.join(work, "server.log"), "rb").read().decode(errors="replace")[-2500:])
        shutil.rmtree(work, ignore_errors=True)
    if bt.check.failed:
        raise SystemExit(1)
    print("OK: a restart keeps the players in the game")


if __name__ == "__main__":
    main()
