#!/usr/bin/env bash
# Hot-update seat keeping (tools/resume_test.py) against a throw-away server. Run it against the sanitizer build.
# Usage: ci/resume-test.sh <path-to-bc-server> <path-to-map.dat>
set -uo pipefail
BIN=$(readlink -f "$1"); MAP=$(readlink -f "$2")
TOOLS=$(cd "$(dirname "$0")/../tools" && pwd)
PORT=${BC_TEST_PORT:-19790}
ROOT=$(mktemp -d)
trap 'kill $SPID 2>/dev/null || true; rm -rf "$ROOT"' EXIT
cp "$MAP" "$ROOT/map.dat"; cd "$ROOT"
BC_BOTS=0 BC_LINGER_SECONDS=2 BC_PORT=$PORT BC_STATUS_PORT=$((PORT + 1)) "$BIN" > server.log 2>&1 &
SPID=$!
for _ in $(seq 1 100); do curl -fsS --max-time 1 "http://127.0.0.1:$((PORT + 1))/health" >/dev/null 2>&1 && break; sleep 0.1; done
rc=0
python3 "$TOOLS/resume_test.py" 127.0.0.1 $PORT || rc=$?
kill $SPID 2>/dev/null || true; wait $SPID 2>/dev/null || true
if grep -a -E "runtime error|ERROR: AddressSanitizer|SERVER CRASH" server.log > errors.log; then
  echo "FAIL: the server reported errors:"; cat errors.log; rc=1
fi
[ $rc = 0 ] || { echo "--- server log"; tail -30 server.log; }
exit $rc
