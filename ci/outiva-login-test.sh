#!/usr/bin/env bash
# Guest and Outiva-ticket login against a throwaway server (see tools/outiva_login_test.py).
# Usage: ci/outiva-login-test.sh <path-to-bc-server> <path-to-map.dat>
set -euo pipefail
BIN=$(readlink -f "$1"); MAP=$(readlink -f "$2")
TOOLS=$(cd "$(dirname "$0")/../tools" && pwd)
PORT=${BC_TEST_PORT:-19711}
STATUS_PORT=${BC_TEST_STATUS_PORT:-19712}
WORK=$(mktemp -d)
trap 'kill "$PID" 2>/dev/null || true; rm -rf "$WORK"' EXIT
cd "$WORK"; cp "$MAP" map.dat
SECRET="outiva-test-secret-0123456789abcdef"
printf '%s\n' "$SECRET" > secret.txt
BC_BOTS=0 BC_PORT=$PORT BC_STATUS_PORT=$STATUS_PORT BC_LEGACY_LOGIN=0 BC_TICKET_SECRET_FILE="$WORK/secret.txt" "$BIN" > server.log 2>&1 &
PID=$!
for _ in $(seq 1 50); do (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null && break; sleep 0.2; done
sleep 1
python3 "$TOOLS/outiva_login_test.py" 127.0.0.1 "$PORT" "$SECRET" "$WORK/dat/BCData.db" || { cat server.log; exit 1; }
