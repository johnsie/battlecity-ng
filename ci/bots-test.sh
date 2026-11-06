#!/usr/bin/env bash
# The server-side bots (docs/solo-play.md), end to end with headless players:
#  1. bots are announced and move, answer chat commands, can be shot (range targets, a snowman) and pay points;
#  2. with the event gaps shortened (BC_BOTS_TEMPO) raids, missions, a bounty boss and supply crates happen, a crate can be
#     picked up, a ghost is recorded and shown to the next player, and the login news lists the recent events.
# Run it against the sanitizer build: any memory error in the bot code fails the test.
# Usage: ci/bots-test.sh <path-to-bc-server> <path-to-map.dat>
set -euo pipefail
BIN=$(readlink -f "$1"); MAP=$(readlink -f "$2")
TOOLS=$(cd "$(dirname "$0")/../tools" && pwd)
PORT=${BC_TEST_PORT:-19760}
WORK=$(mktemp -d)
PID=""
trap 'kill "$PID" 2>/dev/null || true; rm -rf "$WORK"' EXIT
cd "$WORK"; cp "$MAP" map.dat

start() { BC_PORT=$PORT BC_STATUS_PORT=$((PORT + 1)) "$@" "$BIN" > server.log 2>&1 & PID=$!; sleep 3; }
stop() { kill "$PID" 2>/dev/null || true; wait "$PID" 2>/dev/null || true; }
check_log() {
  if grep -a -E "runtime error|ERROR: AddressSanitizer|SERVER CRASH" server.log; then
    echo "FAIL: the server reported errors while the bots were running"; exit 1
  fi
}

start env
python3 "$TOOLS/bots_test.py" 127.0.0.1 "$PORT" 20
status=$(curl -fsS --max-time 2 "http://127.0.0.1:$((PORT + 1))/health")
[ "$status" = '{"status":"available","players":0}' ] || { echo "FAIL: bots leaked into the public count: $status"; exit 1; }
echo "OK: bots are not counted in the public player count"
stop; check_log

rm -f bots_state.txt ghost.dat
start env BC_BOTS_TEMPO=0.02
python3 "$TOOLS/bots_events_test.py" 127.0.0.1 "$PORT"
stop; check_log

# BC_BOTS=0 must switch every bot feature off
start env BC_BOTS=0
python3 "$TOOLS/bots_off_test.py" 127.0.0.1 "$PORT"
stop; check_log
echo "OK: bot tests passed"
