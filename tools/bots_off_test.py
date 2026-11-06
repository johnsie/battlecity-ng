#!/usr/bin/env python3
"""With BC_BOTS=0 no bot feature may appear: no bot players, no Overseer, no greeting. Usage: tools/bots_off_test.py <host> <port>"""
import importlib.util, sys, time

spec = importlib.util.spec_from_file_location("bots_test", __file__.rsplit("/", 1)[0] + "/bots_test.py")
bt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bt)

host, port = sys.argv[1], int(sys.argv[2])
a = bt.Player(host, port, b"off" + str(int(time.time()) % 100000).encode())
a.login(0)
a.send(bt.CM["cmNextStep"], bytes([65]))
if not a.wait(lambda: a.x is not None):
    raise SystemExit("FAIL: the test player never joined")
end = time.time() + 6
while time.time() < end:
    a.update(float(a.x), float(a.y))
    a.pump()
    time.sleep(0.15)
others = [v for k, v in a.names.items() if v[1] in ("Raiders", "Wildlife", "Bounty", "Command", "Range", "Echoes")]
bt.check(not others, "BC_BOTS=0: no bot players appeared", "bots appeared with BC_BOTS=0: %s" % others)
bt.check(not a.chat, "BC_BOTS=0: the server stayed quiet", "unexpected chat: %s" % a.chat)
bt.check(not a.shots, "BC_BOTS=0: nothing was shot", "unexpected shots: %s" % a.shots)
if bt.check.failed:
    raise SystemExit(1)
