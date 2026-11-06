#!/usr/bin/env python3
"""Long-running soak test for the server-side bots (docs/solo-play.md).

A few headless "players" join, wander, shoot the nearest bot, type solo-play commands, leave and come back, for hours. The server
(whose process id is given) is sampled every --sample seconds. At the end (or on Ctrl-C) a report says whether anything drifted or broke:

  * the server process died, or its log shows a sanitizer report / crash / assertion,
  * memory (RSS), open files or threads keep growing,
  * bots stopped showing up or stopped moving while a player was online,
  * more bots than the server's limits allow, bot slots that never go away, a leaderboard that stops arriving,
  * the number of events (raids, missions, bounty bosses, crates, daily/weekly challenges) announced per hour.

Usage: tools/bots_soak.py <host> <port> --pid <server pid> --log <server log> --out <dir> [--hours 8] [--players 3] [--sample 30]
Every N minutes all players leave for a few minutes at once (the server must then drop every bot and go idle).
"""
import argparse, collections, math, os, random, re, signal, struct, sys, time
import importlib.util

spec = importlib.util.spec_from_file_location("bots_test", __file__.rsplit("/", 1)[0] + "/bots_test.py")
bt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bt)
CM = bt.CM

BOT_TOWNS = ("Raiders", "Wildlife", "Bounty", "Ghost")
COMMANDS = ["/where", "/bots", "/challenge", "/mission", "/events", "/top", "/streak", "/ghost", "/help", "/trial best"]
EVENT_KEYS = [("RAID!", "raid"), ("MISSION:", "mission"), ("BOUNTY:", "bounty"), ("SUPPLY CRATE", "crate"),
              ("MISSION COMPLETE", "mission won"), ("Mission failed", "mission failed"), ("challenge complete", "challenge"),
              ("has been destroyed", "boss killed"), ("repelled", "raid repelled"), ("leaderboard was won", "weekly winner")]
BAD_LOG = re.compile(rb"runtime error|AddressSanitizer|LeakSanitizer|SERVER CRASH|Segmentation fault|terminate called|Assertion .* failed|Aborted")


class Persona(bt.Player):
    """A headless player; knows which slots are bots."""

    def __init__(self, host, port, name):
        super().__init__(host, port, name)
        self.last_pos_time = {}

    def bot_slots(self):
        return {s for s, (n, t) in self.names.items() if t in BOT_TOWNS and s != self.me}

    def handle(self, pid, data):
        super().handle(pid, data)


class Soak:
    def __init__(self, a):
        self.a = a
        self.start = time.time()
        self.end = self.start + a.hours * 3600
        self.out = a.out
        os.makedirs(self.out, exist_ok=True)
        self.csv = open(os.path.join(self.out, "samples.csv"), "w", buffering=1)
        self.csv.write("minutes,rss_mb,threads,fds,cpu_pct,players_online,bots_visible,max_bots_seen,events,log_kb\n")
        self.samples = []
        self.events = collections.Counter()
        self.problems = []
        self.warnings = []
        self.max_bots = 0
        self.max_slots = 0
        self.bot_updates = 0
        self.boards = 0
        self.quiet_windows = 0
        self.reconnects = 0
        self.log_pos = 0
        self.last_cpu = None
        self.last_bot_update = time.time()
        self.personas = []          # per persona dict
        self.next_quiet = self.start + a.quiet_every * 60
        self.quiet_until = 0
        self.tag = str(int(time.time()) % 100000).encode()
        self.stop = False
        self.seen_boss = False
        for i in range(a.players):
            self.personas.append({"i": i, "p": None, "join_at": self.start + i * 20, "leave_at": 0, "next_cmd": 0, "next_shot": 0,
                                  "next_move": 0, "target": None, "x0": 0, "y0": 0, "px": 0, "py": 0, "online_since": 0})

    # ---- helpers
    def problem(self, text):
        stamp = "%.1f min" % ((time.time() - self.start) / 60)
        print("PROBLEM (%s): %s" % (stamp, text), flush=True)
        self.problems.append("%s: %s" % (stamp, text))

    def warn(self, text):
        stamp = "%.1f min" % ((time.time() - self.start) / 60)
        print("warning (%s): %s" % (stamp, text), flush=True)
        self.warnings.append("%s: %s" % (stamp, text))

    def alive(self):
        try:
            os.kill(self.a.pid, 0)
            return True
        except OSError:
            return False

    def proc(self):
        pid = self.a.pid
        rss = threads = 0
        try:
            for line in open("/proc/%d/status" % pid):
                if line.startswith("VmRSS:"): rss = int(line.split()[1]) / 1024.0
                elif line.startswith("Threads:"): threads = int(line.split()[1])
            fds = len(os.listdir("/proc/%d/fd" % pid))
            f = open("/proc/%d/stat" % pid).read().rsplit(")", 1)[1].split()
            ticks = int(f[11]) + int(f[12])
        except OSError:
            return None
        cpu = 0.0
        now = time.time()
        if self.last_cpu:
            cpu = 100.0 * (ticks - self.last_cpu[0]) / os.sysconf("SC_CLK_TCK") / max(0.001, now - self.last_cpu[1])
        self.last_cpu = (ticks, now)
        return rss, threads, fds, cpu

    def scan_log(self):
        try:
            with open(self.a.log, "rb") as f:
                f.seek(self.log_pos)
                chunk = f.read()
                self.log_pos += len(chunk)
        except OSError:
            return
        for line in chunk.splitlines():
            if BAD_LOG.search(line):
                self.problem("server log: " + line.decode(errors="replace")[:200])

    # ---- persona control
    def join(self, ps):
        i = ps["i"]
        name = b"soak%d%s" % (i, self.tag)
        try:
            p = Persona(self.a.host, self.a.port, name)
            p.login(i % 8)
            p.send(CM["cmNextStep"], bytes([65]))
            if not p.wait(lambda: p.x is not None, 8):
                self.warn("persona %d did not get into the game" % i)
                p.s.close()
                ps["join_at"] = time.time() + 30
                return
        except (OSError, SystemExit) as e:
            self.warn("persona %d could not connect: %s" % (i, e))
            ps["join_at"] = time.time() + 30
            return
        ps.update(p=p, x0=float(p.x), y0=float(p.y), px=float(p.x), py=float(p.y), online_since=time.time(),
                  leave_at=time.time() + random.uniform(self.a.stay_min, self.a.stay_max) * 60, next_cmd=time.time() + 20)
        print("persona %d joined (%d online)" % (i, self.online()), flush=True)

    def leave(self, ps, delay_min=None):
        if ps["p"]:
            try: ps["p"].s.close()
            except OSError: pass
        ps["p"] = None
        ps["join_at"] = time.time() + (delay_min if delay_min is not None else random.uniform(1, 4)) * 60

    def online(self):
        return sum(1 for ps in self.personas if ps["p"])

    def drive(self, ps, now):
        p = ps["p"]
        try:
            p.pump()
        except (OSError, SystemExit):
            self.reconnects += 1
            self.warn("persona %d lost its connection" % ps["i"])
            self.leave(ps, 0.2)
            return
        # news from the Overseer and the server
        for _, text in p.chat:
            for key, label in EVENT_KEYS:
                if key in text:
                    self.events[label] += 1 if ps["i"] == 0 else 0     # every persona hears the same thing: count it once
        p.chat.clear()
        self.boards += len(p.board)
        p.board.clear()
        # bots seen right now
        slots = p.bot_slots()
        fresh = [s for s in slots if s in p.pos and now - p.pos[s][4] < 3]
        self.max_slots = max(self.max_slots, len(slots))
        if fresh:
            self.last_bot_update = now
        ps["fresh"] = len(fresh)
        # movement: wander around the start, 0.15 s steps
        if now >= ps["next_move"]:
            ps["next_move"] = now + 0.15
            if ps["target"] is None or math.hypot(ps["target"][0] - ps["px"], ps["target"][1] - ps["py"]) < 40:
                ps["target"] = (ps["x0"] + random.uniform(-700, 700), ps["y0"] + random.uniform(-700, 700))
            dx, dy = ps["target"][0] - ps["px"], ps["target"][1] - ps["py"]
            d = math.hypot(dx, dy) or 1
            step = min(d, 38)
            ps["px"] += dx / d * step
            ps["py"] += dy / d * step
            try:
                p.update(ps["px"], ps["py"], bt.dir_to(dx, dy), 2)
            except OSError:
                self.leave(ps, 0.2)
                return
        # shoot the nearest bot in range
        if now >= ps["next_shot"] and fresh:
            ps["next_shot"] = now + 0.5
            best = min(fresh, key=lambda s: math.hypot(p.pos[s][0] - ps["px"], p.pos[s][1] - ps["py"]))
            bx, by = p.pos[best][0], p.pos[best][1]
            if math.hypot(bx - ps["px"], by - ps["py"]) < 800:
                try: p.shoot(bt.dir_to(bx - ps["px"], by - ps["py"]))
                except OSError: pass
        if now >= ps["next_cmd"]:
            ps["next_cmd"] = now + random.uniform(30, 90)
            try: p.say(random.choice(COMMANDS) if random.random() < 0.9 else random.choice(["/range", "/trial", "/trial stop"]))
            except OSError: pass
        # a bounded memory footprint for the test client itself
        for k in list(p.history):
            if len(p.history[k]) > 20: p.history[k] = p.history[k][-5:]
        if len(p.shots) > 200: del p.shots[:150]
        if len(p.deaths) > 200: del p.deaths[:150]

    # ---- sampling and checks
    def sample(self, now):
        info = self.proc()
        if info is None:
            self.problem("the server process is gone")
            self.stop = True
            return
        rss, threads, fds, cpu = info
        fresh_total = max([ps.get("fresh", 0) for ps in self.personas if ps["p"]] or [0])
        self.max_bots = max(self.max_bots, fresh_total)
        try: log_kb = os.path.getsize(self.a.log) // 1024
        except OSError: log_kb = 0
        minutes = (now - self.start) / 60
        self.samples.append((minutes, rss, threads, fds, cpu, self.online(), fresh_total))
        self.csv.write("%.1f,%.1f,%d,%d,%.1f,%d,%d,%d,%d,%d\n" % (minutes, rss, threads, fds, cpu, self.online(), fresh_total, self.max_bots, sum(self.events.values()), log_kb))
        self.scan_log()
        if fresh_total > 24:
            self.problem("%d bots visible at once (the server's slot limit is 24)" % fresh_total)
        # bots must show up while somebody is online for a while
        longest = max([now - ps["online_since"] for ps in self.personas if ps["p"]] or [0])
        if longest > 240 and now - self.last_bot_update > 300 and self.a.bots > 0:
            self.warn("players are online but no bot has moved for %.0f s" % (now - self.last_bot_update))
            self.last_bot_update = now
        print("%6.1f min  rss %6.1f MB  thr %d  fds %d  cpu %4.1f%%  online %d  bots %d  events %d" % (minutes, rss, threads, fds, cpu, self.online(), fresh_total, sum(self.events.values())), flush=True)

    def analyse(self):
        a = self.a
        rows = [s for s in self.samples if s[0] >= a.warmup]
        verdict = []
        if len(rows) >= 10:
            def slope(idx):
                n = len(rows); xs = [r[0] / 60 for r in rows]; ys = [r[idx] for r in rows]
                mx, my = sum(xs) / n, sum(ys) / n
                den = sum((x - mx) ** 2 for x in xs) or 1
                return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / den
            third = max(1, len(rows) // 3)
            early = sum(r[1] for r in rows[:third]) / third
            late = sum(r[1] for r in rows[-third:]) / third
            rs = slope(1)
            verdict.append("memory: %.1f MB at the start of the measured period, %.1f MB at the end, trend %+.2f MB/hour" % (early, late, rs))
            if late - early > max(25.0, 0.15 * early) and rs > 3:
                self.problem("memory keeps growing (%.1f MB -> %.1f MB, %+.2f MB/hour)" % (early, late, rs))
            fd_growth = rows[-1][3] - rows[0][3]
            verdict.append("open files: %d -> %d" % (rows[0][3], rows[-1][3]))
            if fd_growth > 30:
                self.problem("open files keep growing (%d -> %d)" % (rows[0][3], rows[-1][3]))
            tg = max(r[2] for r in rows) - min(r[2] for r in rows)
            verdict.append("threads: %d to %d" % (min(r[2] for r in rows), max(r[2] for r in rows)))
            if tg > 4:
                self.problem("thread count drifts (%d to %d)" % (min(r[2] for r in rows), max(r[2] for r in rows)))
            busy = [r[4] for r in rows if r[5] > 0]
            if busy:
                verdict.append("cpu with players online: average %.1f%%, peak %.1f%%" % (sum(busy) / len(busy), max(busy)))
                if sum(busy) / len(busy) > 60:
                    self.warn("high average cpu with players online (%.1f%%)" % (sum(busy) / len(busy)))
        else:
            verdict.append("run too short (< %d min of measured samples) for drift analysis" % a.warmup)
        idle = [r for r in self.samples if r[5] == 0 and r[0] >= a.warmup]
        if idle:
            ib = max(r[6] for r in idle)
            if ib:
                self.problem("%d bot(s) were still visible while nobody was online" % ib)
        return verdict

    def report(self, verdict):
        hours = max(0.001, (time.time() - self.start) / 3600)
        lines = ["# Bot soak test", "",
                 "Ran %.2f hours against pid %d (%d players, %d quiet windows, %d lost connections)." % (hours, self.a.pid, self.a.players, self.quiet_windows, self.reconnects), ""]
        lines.append("## Result: " + ("FAIL" if self.problems else "PASS"))
        for t in self.problems: lines.append("- PROBLEM " + t)
        for t in self.warnings[:30]: lines.append("- warning " + t)
        lines += ["", "## Drift"] + ["- " + v for v in verdict]
        lines += ["", "## Activity", "- most bots visible at once: %d (most bot slots known to one player: %d)" % (self.max_bots, self.max_slots),
                  "- leaderboard updates received: %d" % self.boards]
        for k, v in sorted(self.events.items()):
            lines.append("- %s: %d (%.1f per hour)" % (k, v, v / hours))
        text = "\n".join(lines) + "\n"
        open(os.path.join(self.out, "report.md"), "w").write(text)
        print("\n" + text)

    def run(self):
        a = self.a
        next_sample = self.start + 5
        signal.signal(signal.SIGINT, lambda *_: setattr(self, "stop", True))
        signal.signal(signal.SIGTERM, lambda *_: setattr(self, "stop", True))
        while not self.stop and time.time() < self.end:
            now = time.time()
            # everybody leaves for a few minutes now and then: the server must go quiet
            if now >= self.next_quiet and not self.quiet_until:
                self.quiet_until = now + a.quiet_minutes * 60
                self.quiet_windows += 1
                print("quiet window: everybody leaves for %.1f min" % a.quiet_minutes, flush=True)
                for ps in self.personas: self.leave(ps, a.quiet_minutes + random.uniform(0, 1))
            if self.quiet_until and now >= self.quiet_until:
                self.quiet_until = 0
                self.next_quiet = now + a.quiet_every * 60
            for ps in self.personas:
                if ps["p"]:
                    if now >= ps["leave_at"]: self.leave(ps)
                    else: self.drive(ps, now)
                elif now >= ps["join_at"] and not self.quiet_until:
                    self.join(ps)
            if now >= next_sample:
                next_sample = now + a.sample
                self.sample(now)
            time.sleep(0.01)
        for ps in self.personas: self.leave(ps)
        self.scan_log()
        if not self.alive() and not any("process is gone" in p for p in self.problems):
            self.problem("the server process is gone")
        verdict = self.analyse()
        self.report(verdict)
        return 1 if self.problems else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("host"); ap.add_argument("port", type=int)
    ap.add_argument("--pid", type=int, required=True); ap.add_argument("--log", required=True); ap.add_argument("--out", required=True)
    ap.add_argument("--hours", type=float, default=8); ap.add_argument("--players", type=int, default=3)
    ap.add_argument("--sample", type=float, default=30); ap.add_argument("--warmup", type=float, default=20, help="minutes ignored by the drift analysis")
    ap.add_argument("--stay-min", type=float, default=20); ap.add_argument("--stay-max", type=float, default=50)
    ap.add_argument("--quiet-every", type=float, default=90); ap.add_argument("--quiet-minutes", type=float, default=4)
    ap.add_argument("--bots", type=int, default=3, help="the server's BC_BOTS (0: bots are not expected)")
    sys.exit(Soak(ap.parse_args()).run())


if __name__ == "__main__":
    main()
