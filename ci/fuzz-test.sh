#!/usr/bin/env bash
# Sends malformed and random packets to a (sanitizer-built) server and fails if it dies or reports a memory error.
# Usage: ci/fuzz-test.sh <path-to-bc-server> <path-to-map.dat> [packets-per-run]
set -euo pipefail
BIN=$(readlink -f "$1"); MAP=$(readlink -f "$2"); COUNT=${3:-3000}
TOOLS=$(cd "$(dirname "$0")/../tools" && pwd)
PORT=${BC_TEST_PORT:-19690}
WORK=$(mktemp -d)
trap 'kill "$PID" 2>/dev/null || true; rm -rf "$WORK"' EXIT
cd "$WORK"; cp "$MAP" map.dat
BC_PORT=$PORT "$BIN" > server.log 2>&1 &
PID=$!
sleep 3
for seed in 1 2 3; do
  python3 "$TOOLS/fuzz_server.py" 127.0.0.1 "$PORT" "$seed" "$COUNT" || { echo "--- server log (tail)"; tail -40 server.log; exit 1; }
done
if grep -E "runtime error|ERROR: AddressSanitizer|buffer overflow" server.log; then
  echo "FAIL: sanitizer reported errors during fuzzing"; exit 1
fi
echo "OK: server survived fuzzing with no sanitizer reports"
