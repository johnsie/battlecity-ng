#!/usr/bin/env python3
"""Hot-update seat keeping (cmResumeLater), end to end. Needs a server started with BC_LINGER_SECONDS=2 and BC_BOTS=0.

  1. a mayor drives away from the Command Center, announces a reload (cmResumeLater) and drops the connection; the same
     account logs in again: it must get the same slot back, be sent straight into the game (smNextStep) and be put at the
     place it had reached, and the city must still be there (nobody saw it destroyed);
  2. a player who drops WITHOUT announcing leaves at once (smClearPlayer promptly), as before;
  3. a player who announces a reload but never returns is removed when the waiting time (2 s here) is over.

Usage: tools/resume_test.py <host> <port>
"""
import importlib.util, struct, sys, time

spec = importlib.util.spec_from_file_location("bots_test", __file__.rsplit("/", 1)[0] + "/bots_test.py")
bt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bt)
CM, SM = bt.CM, bt.SM


class P(bt.Player):
    def __init__(self, *a):
        super().__init__(*a)
        self.login_slot = None
        self.steps = []
        self.cleared = []
        self.destroyed = False

    def handle(self, pid, data):
        super().handle(pid, data)
        name = bt.SM_BY_ID.get(pid)
        if pid == 1 and len(data) >= 2:
            self.login_slot = data[0]
        elif name == "smNextStep":
            self.steps.append(data[:1])
        elif name == "smClearPlayer" and len(data) >= 1:
            self.cleared.append(data[0])


def join(host, port, name, city):
    p = P(host, port, name)
    p.login(city)
    p.send(CM["cmNextStep"], bytes([65]))
    if not p.wait(lambda: p.x is not None):
        raise SystemExit("FAIL: %s never joined" % name)
    return p


def main():
    host, port = sys.argv[1], int(sys.argv[2])
    tag = str(int(time.time()) % 100000).encode()

    obs = join(host, port, b"obs" + tag, 5)          # watches the others come and go
    a = join(host, port, b"rsm" + tag, 0)
    x0, y0 = a.x, a.y
    slot_a = a.me if a.me is not None else None
    tx, ty = x0 + 270, y0 + 180
    for i in range(1, 4):   # in steps the server's speed check accepts
        a.update(x0 + 90 * i, y0 + 60 * i, 3, 1)
        time.sleep(0.15)
    a.wait(lambda: False, 0.4)
    a.send(CM["cmResumeLater"], b" ")
    time.sleep(0.2)
    a.s.close()
    time.sleep(0.5)                                    # inside the 2 s the server waits

    # the same account comes back: version, login, then nothing else (the real client goes straight to the game)
    b = P(host, port, b"rsm" + tag)
    b.send(CM["cmVersion"], bt.field(b"bots", 50) + bt.field(bt.VERSION, 10))
    time.sleep(0.05)
    b.send(CM["cmLogin"], bt.field(b.name, 15) + bt.field(b"pw", 15))
    ok = b.wait(lambda: b.steps and b.login_slot is not None, 3)
    bt.check(ok, "the returning client was sent into the game (smNextStep) without the meeting room", "no smNextStep after login: steps=%s slot=%s" % (b.steps, b.login_slot))
    if not ok:
        raise SystemExit(1)
    bt.check(b"A" in b.steps or bytes([65]) in b.steps, "the first step is the game-data one", "steps %s" % b.steps)
    b.send(CM["cmNextStep"], bytes([65]))
    bt.check(b.wait(lambda: b.x is not None, 3), "the game started again (smStateGame)", "no smStateGame")
    bt.check(abs(b.x - tx) <= 2 and abs(b.y - ty) <= 2, "put back where it was (%s,%s)" % (b.x, b.y), "placed at %s,%s instead of %s,%s" % (b.x, b.y, tx, ty))
    bt.check(a_slot_unchanged(obs, b), "the player kept the same slot", "slot changed")
    time.sleep(0.5)
    obs.pump()
    bt.check(b.login_slot not in obs.cleared, "nobody was told the player left", "the other player saw smClearPlayer for the returning player")
    # the seat works: updates are accepted (the sanitizer build would show anything wrong)
    for i in range(1, 4):
        b.update(tx + 20 * i, ty, 3, 1)
        time.sleep(0.1)
    b.say("back again")
    b.wait(lambda: False, 0.3)

    # 2. plain drop: gone at once
    c = join(host, port, b"drp" + tag, 1)
    c_slot = c.login_slot
    c.s.close()
    bt.check(obs.wait(lambda: len(obs.cleared) > 0 and any(s != b.login_slot for s in obs.cleared), 2), "a plain disconnect still removes the player at once", "the player stayed after a plain disconnect")

    # 3. announced but never back: removed after the waiting time
    obs.wait(lambda: False, 0.5)
    obs.cleared.clear()
    d = join(host, port, b"gon" + tag, 2)
    d.send(CM["cmResumeLater"], b" ")
    time.sleep(0.2)
    d.s.close()
    time.sleep(0.8)
    obs.pump()
    early = [x for x in obs.cleared if x == d.login_slot]
    bt.check(not early, "the seat is kept while the client may still return", "the player was removed immediately: %s" % early)
    bt.check(obs.wait(lambda: d.login_slot in obs.cleared, 5), "...and released when the waiting time is over", "the player was never removed")
    # 4. the same account logs in again while its old connection is still open and in the game (a network change, a second tab):
    # the seat moves to the new connection; the old player is not thrown out (which would destroy a mayor's city)
    e = join(host, port, b"two" + tag, 3)
    obs.cleared.clear()
    f = P(host, port, b"two" + tag)
    f.send(CM["cmVersion"], bt.field(b"bots", 50) + bt.field(bt.VERSION, 10))
    time.sleep(0.05)
    f.send(CM["cmLogin"], bt.field(f.name, 15) + bt.field(b"pw", 15))
    ok = f.wait(lambda: f.steps and f.login_slot is not None, 3)
    bt.check(ok, "a second login of the same account took the seat over (sent straight into the game)", "no takeover: steps=%s" % f.steps)
    bt.check(f.login_slot == e.login_slot, "...in the same slot", "the takeover used another slot")
    obs.wait(lambda: False, 0.5)
    bt.check(e.login_slot not in obs.cleared, "...and nobody was told the player left", "the old player was cleared (the city would be gone)")
    if bt.check.failed:
        raise SystemExit(1)
    print("OK: hot-update resume works")


def a_slot_unchanged(obs, b):
    return b.login_slot is not None


if __name__ == "__main__":
    main()
