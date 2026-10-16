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
BC_PORT=$PORT "$BIN" > server.log 2>&1 &
PID=$!
sleep 3
python3 "$TOOLS/load_test.py" 127.0.0.1 "$PORT" --bots "$BOTS" --seconds "$SECONDS_TO_RUN" --pid "$PID" || { tail -30 server.log; exit 1; }
kill -0 "$PID" 2>/dev/null || { echo "FAIL: server is not running"; tail -30 server.log; exit 1; }
