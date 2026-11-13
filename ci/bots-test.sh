#!/usr/bin/env bash
# The server-side bots (docs/solo-play.md), end to end with headless players. Three independent cases run at the same time,
# each against its own server on its own port, and each test goes on as soon as what it waits for has happened:
#  basic   bots are announced and move, answer chat commands, can be shot (range targets, a snowman) and pay points,
#          and are not counted in the public player count
#  events  with the event gaps shortened (BC_BOTS_TEMPO) raids, missions, a bounty boss and supply crates happen, a crate can
#          be picked up, a ghost is recorded and shown to the next player, and the login news lists the recent events
#  off     BC_BOTS=0 switches every bot feature off
# Run it against the sanitizer build: any memory error in the bot code fails the test.
# Usage: ci/bots-test.sh <path-to-bc-server> <path-to-map.dat>
set -uo pipefail
BIN=$(readlink -f "$1"); MAP=$(readlink -f "$2")
TOOLS=$(cd "$(dirname "$0")/../tools" && pwd)
BASE=${BC_TEST_PORT:-19760}
ROOT=$(mktemp -d)
PIDS=""
trap 'kill $PIDS 2>/dev/null || true; rm -rf "$ROOT"' EXIT

# run_case <name> <port> <env assignments...> -- <test command...>
run_case() {
  local name=$1 port=$2; shift 2
  local envs=() 
  while [ "$1" != "--" ]; do envs+=("$1"); shift; done
  shift
  local dir="$ROOT/$name"; mkdir -p "$dir"; cp "$MAP" "$dir/map.dat"; cd "$dir"
  env "${envs[@]}" BC_PORT=$port BC_STATUS_PORT=$((port + 1)) "$BIN" > server.log 2>&1 &
  local spid=$!
  # ready as soon as the status endpoint answers (instead of a fixed sleep)
  for _ in $(seq 1 100); do curl -fsS --max-time 1 "http://127.0.0.1:$((port + 1))/health" >/dev/null 2>&1 && break; sleep 0.1; done
  local rc=0
  "$@" > test.log 2>&1 || rc=$?
  if [ "$name" = basic ] && [ $rc -eq 0 ]; then
    status=$(curl -fsS --max-time 2 "http://127.0.0.1:$((port + 1))/health" 2>/dev/null || true)
    case "$status" in
      *'"players":0'*|*'"players":1'*) echo "OK: bots are not counted in the public player count ($status)" >> test.log ;;
      *) echo "FAIL: unexpected public count: $status" >> test.log; rc=1 ;;
    esac
  fi
  kill "$spid" 2>/dev/null || true; wait "$spid" 2>/dev/null || true
  if grep -a -E "runtime error|ERROR: AddressSanitizer|SERVER CRASH" server.log > errors.log; then
    echo "FAIL: the server reported errors while the bots were running:" >> test.log; cat errors.log >> test.log; rc=1
  fi
  echo $rc > rc
}

( run_case basic  $BASE BC_TEST_TURRET=1 -- python3 "$TOOLS/bots_test.py" 127.0.0.1 $BASE 15 ) &  PIDS="$PIDS $!"
( run_case events $((BASE + 2)) BC_BOTS_TEMPO=0.005 BC_BOTS_SAFE_RADIUS=0 -- python3 "$TOOLS/bots_events_test.py" 127.0.0.1 $((BASE + 2)) ) &  PIDS="$PIDS $!"
( run_case off    $((BASE + 4)) BC_BOTS=0 -- python3 "$TOOLS/bots_off_test.py" 127.0.0.1 $((BASE + 4)) ) &  PIDS="$PIDS $!"
wait

failed=0
for c in basic events off; do
  echo "=== $c"; cat "$ROOT/$c/test.log"
  [ "$(cat "$ROOT/$c/rc" 2>/dev/null || echo 1)" = 0 ] || failed=1
done
[ $failed = 0 ] || { echo "FAIL: bot tests"; exit 1; }
echo "OK: bot tests passed"
