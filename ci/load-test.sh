#!/usr/bin/env bash
# Many bots join one city and drive; positions must agree and the server must survive.
# Usage: ci/load-test.sh <path-to-bc-server> <path-to-map.dat> [bots] [seconds]
set -euo pipefail
BIN=$(readlink -f "$1"); MAP=$(readlink -f "$2")
BOTS=${3:-20}; SECONDS_TO_RUN=${4:-120}
TOOLS=$(cd "$(dirname "$0")/../tools" && pwd)
PORT=${BC_TEST_PORT:-19701}
WORK=$(mktemp -d)
trap 'kill "$PID" 2>/dev/null || true; rm -rf "$WORK"' EXIT
cd "$WORK"; cp "$MAP" map.dat
BC_BOTS=0 BC_PORT=$PORT BC_STATUS_PORT=$((PORT + 1)) "$BIN" > server.log 2>&1 &   # the server-side bots have their own test (ci/bots-test.sh); keep this one about the network
PID=$!
# ready as soon as the status endpoint answers (instead of a fixed sleep)
for _ in $(seq 1 100); do curl -fsS --max-time 1 "http://127.0.0.1:$((PORT + 1))/health" >/dev/null 2>&1 && break; sleep 0.1; done
python3 "$TOOLS/load_test.py" 127.0.0.1 "$PORT" --bots "$BOTS" --seconds "$SECONDS_TO_RUN" --pid "$PID" || { tail -30 server.log; exit 1; }
kill -0 "$PID" 2>/dev/null || { echo "FAIL: server is not running"; tail -30 server.log; exit 1; }
