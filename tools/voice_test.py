#!/usr/bin/env python3
"""Voice chat routing and limits (docs/voice-chat.md) with real protocol clients. Server started with BC_BOTS=0.

  * the server says voice is available (smVoiceState) when a player joins;
  * two players who both have voice on and the "everyone" channel hear each other, never themselves;
  * a player with voice off hears nothing and is not heard;
  * a player on "my city" is not heard by another city (and does not hear them);
  * malformed packets (too short, too long) are dropped, and a flood is cut off well below what was sent;
  * a player who has not turned voice on cannot transmit.
Usage: tools/voice_test.py <host> <port>
"""
import importlib.util, struct, sys, time

spec = importlib.util.spec_from_file_location("bots_test", __file__.rsplit("/", 1)[0] + "/bots_test.py")
bt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bt)
CM, SM = bt.CM, bt.SM


class V(bt.Player):
    def __init__(self, *a):
        super().__init__(*a)
        self.state = None
        self.voice = []          # (speaker, payload)
        self.slot = None

    def handle(self, pid, data):
        super().handle(pid, data)
        name = bt.SM_BY_ID.get(pid)
        if pid == 1 and len(data) >= 1:
            self.slot = data[0]
        elif name == "smVoiceState" and len(data) >= 3:
            self.state = tuple(data[:3])
        elif name == "smVoice" and len(data) >= 7:
            self.voice.append((data[0], bytes(data[1:])))


def join(host, port, name, city):
    p = V(host, port, name)
    p.login(city)
    p.send(CM["cmNextStep"], bytes([65]))
    if not p.wait(lambda: p.x is not None):
        raise SystemExit("FAIL: %s never joined" % name)
    return p


def packet(seq, n=160):
    return struct.pack("<HhB", seq, 0, 0) + bytes([seq & 255]) * n


def main():
    host, port = sys.argv[1], int(sys.argv[2])
    tag = str(int(time.time()) % 100000).encode()
    a = join(host, port, b"va" + tag, 0)
    b = join(host, port, b"vb" + tag, 1)
    c = join(host, port, b"vc" + tag, 2)
    for p in (a, b, c):
        p.wait(lambda: p.state is not None, 2)
    bt.check(a.state is not None and a.state[0] == 1, "joining players are told voice is available", "no smVoiceState (state=%s)" % (a.state,))

    # nobody has voice on yet: nothing is forwarded
    a.send(CM["cmVoice"], packet(1))
    b.wait(lambda: False, 0.4)
    bt.check(not b.voice, "a player with voice off cannot transmit", "voice was forwarded from a player with voice off")

    a.send(CM["cmVoiceSet"], bytes([1, 0])); b.send(CM["cmVoiceSet"], bytes([1, 0]))
    a.wait(lambda: a.state and a.state[1] == 1, 2)
    bt.check(a.state[1:] == (1, 0), "voice on, channel everyone, is acknowledged", "state %s" % (a.state,))
    a.send(CM["cmVoice"], packet(2))
    ok = b.wait(lambda: b.voice, 2)
    bt.check(ok, "a listener hears the speaker", "nothing reached the listener")
    if ok:
        bt.check(b.voice[0][0] == a.slot and b.voice[0][1] == packet(2), "...with the speaker's slot and the audio untouched", "wrong payload %s" % (b.voice[0],))
    a.wait(lambda: False, 0.3)
    bt.check(not a.voice, "the speaker does not hear themselves", "echo to the speaker")
    bt.check(not c.voice, "a player with voice off hears nothing", "voice reached a player with voice off")

    # c joins on 'my city' while a stays on 'everyone': different cities, so no connection either way
    c.send(CM["cmVoiceSet"], bytes([1, 1]))
    c.wait(lambda: c.state and c.state[1] == 1, 2)
    n0 = len(c.voice)
    a.send(CM["cmVoice"], packet(3)); c.send(CM["cmVoice"], packet(4))
    a.wait(lambda: False, 0.5)
    bt.check(len(c.voice) == n0 and all(s != c.slot for s, _ in a.voice), "'my city' keeps other cities out, both ways", "voice crossed between cities")

    # malformed packets are dropped
    b.wait(lambda: False, 0.3)   # (read what is still on its way first)
    n = len(b.voice)
    a.send(CM["cmVoice"], b"\x01\x02")
    a.send(CM["cmVoice"], packet(5, 230))
    b.wait(lambda: False, 0.4)
    bt.check(len(b.voice) == n, "too short and too long packets are dropped", "a malformed packet was forwarded")

    # a flood is cut off
    n = len(b.voice)
    for i in range(300):
        a.send(CM["cmVoice"], packet(i))
    b.wait(lambda: False, 1.0)
    got = len(b.voice) - n
    bt.check(got < 150, "a flood of 300 packets is cut off (%d forwarded)" % got, "no flood limit: %d of 300 forwarded" % got)
    bt.check(got > 10, "...but normal traffic still gets through", "nothing got through after the flood")

    # voice can be switched off again
    a.send(CM["cmVoiceSet"], bytes([0, 0]))
    a.wait(lambda: a.state and a.state[1] == 0, 2)
    n = len(b.voice)
    time.sleep(1.2)
    a.send(CM["cmVoice"], packet(9))
    b.wait(lambda: False, 0.4)
    bt.check(len(b.voice) == n, "turning voice off stops the audio", "audio still forwarded after voice off")
    if bt.check.failed:
        raise SystemExit(1)
    print("OK: voice routing works")


if __name__ == "__main__":
    main()
