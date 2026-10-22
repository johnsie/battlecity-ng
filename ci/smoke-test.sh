#!/usr/bin/env bash
# Starts the server in a throwaway directory, checks that it accepts TCP connections
# and that it shuts down cleanly on SIGTERM.
# Usage: ci/smoke-test.sh <path-to-bc-server> <path-to-map.dat>
set -euo pipefail

BIN=$(readlink -f "$1")
MAP=$(readlink -f "$2")
PORT=${BC_TEST_PORT:-19680}
STATUS_PORT=${BC_TEST_STATUS_PORT:-19682}

WORK=$(mktemp -d)
trap 'kill "$PID" 2>/dev/null || true; rm -rf "$WORK"' EXIT
cd "$WORK"
cp "$MAP" map.dat

BC_PORT=$PORT BC_STATUS_PORT=$STATUS_PORT "$BIN" > server.log 2>&1 &
PID=$!

for _ in $(seq 1 50); do
  if (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null; then
    echo "OK: server accepted a connection on port $PORT"
    STATUS=$(curl -fsS --max-time 2 "http://127.0.0.1:$STATUS_PORT/health") || {
      echo "FAIL: loopback status source did not answer"; cat server.log; exit 1;
    }
    [ "$STATUS" = '{"status":"available","players":0}' ] || {
      echo "FAIL: unexpected status response: $STATUS"; exit 1;
    }
    echo "OK: loopback status source reports an aggregate count"
    # Stay idle for a moment, then ask the server to stop. An idle server whose main loop is stuck
    # (e.g. blocking in accept()) would never notice, so this also proves the game loop keeps running.
    sleep 1.5
    kill -TERM "$PID"
    for _ in $(seq 1 50); do
      kill -0 "$PID" 2>/dev/null || { echo "OK: idle server shut down cleanly on SIGTERM"; exit 0; }
      sleep 0.1
    done
    echo "FAIL: idle server did not exit within 5s of SIGTERM (main loop stuck?)"; cat server.log; exit 1
  fi
  kill -0 "$PID" 2>/dev/null || { echo "FAIL: server exited early"; cat server.log; exit 1; }
  sleep 0.1
done
echo "FAIL: server never started listening on port $PORT"; cat server.log; exit 1
