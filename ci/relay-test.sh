#!/usr/bin/env bash
# Two bots join one city; one drives, the other must receive its position updates at a steady rhythm.
# Usage: ci/relay-test.sh <path-to-bc-server> <path-to-map.dat>
set -euo pipefail
BIN=$(readlink -f "$1"); MAP=$(readlink -f "$2")
TOOLS=$(cd "$(dirname "$0")/../tools" && pwd)
PORT=${BC_TEST_PORT:-19700}
WORK=$(mktemp -d)
trap 'kill "$PID" 2>/dev/null || true; rm -rf "$WORK"' EXIT
cd "$WORK"; cp "$MAP" map.dat
BC_PORT=$PORT "$BIN" > server.log 2>&1 &
PID=$!
sleep 3
python3 "$TOOLS/move_test.py" 127.0.0.1 "$PORT" 10
