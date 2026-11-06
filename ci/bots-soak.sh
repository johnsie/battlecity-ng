#!/usr/bin/env bash
# Overnight soak test of the server-side bots on a LOCAL throwaway server (never the live one). Headless players join, wander, shoot,
# leave and come back for hours while the server's memory, files, threads and log are watched. See tools/bots_soak.py.
#
#   ci/bots-soak.sh <bc-server> <map.dat> [hours=8] [tempo=0.1]
#
# <bc-server>  prefer the sanitizer build (build-asan/server/bc-server): memory errors and leaks then fail the run, at some speed cost
# tempo        BC_BOTS_TEMPO: 1 = the real gaps between raids/missions/bounties; 0.1 = ten times as many events (default, exercises more)
# Environment: BC_BOTS (default 6), SOAK_PLAYERS (3), SOAK_PORT (19800), SOAK_OUT (soak-results/<time>), SOAK_QUIET_EVERY minutes (90)
# Results: <out>/report.md (verdict), samples.csv (graph it), server.log. Exit status 0 = pass.
set -uo pipefail
BIN=$(readlink -f "$1"); MAP=$(readlink -f "$2")
HOURS=${3:-8}; TEMPO=${4:-0.1}
TOOLS=$(cd "$(dirname "$0")/../tools" && pwd)
PORT=${SOAK_PORT:-19800}
OUT=$(mkdir -p "${SOAK_OUT:-soak-results/$(date +%Y%m%d-%H%M)}" && readlink -f "${SOAK_OUT:-soak-results/$(date +%Y%m%d-%H%M)}")
WORK="$OUT/server"; mkdir -p "$WORK"; cp "$MAP" "$WORK/map.dat"
BOTS=${BC_BOTS:-6}

cd "$WORK"
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1:abort_on_error=0}
BC_BOTS=$BOTS BC_BOTS_TEMPO=$TEMPO BC_PORT=$PORT BC_STATUS_PORT=$((PORT + 1)) "$BIN" > "$OUT/server.log" 2>&1 &
SPID=$!
trap 'kill $SPID 2>/dev/null || true; wait $SPID 2>/dev/null || true' EXIT
for _ in $(seq 1 100); do curl -fsS --max-time 1 "http://127.0.0.1:$((PORT + 1))/health" >/dev/null 2>&1 && break; sleep 0.1; done
echo "server pid $SPID, bots=$BOTS tempo=$TEMPO, $HOURS hours, results in $OUT"

python3 "$TOOLS/bots_soak.py" 127.0.0.1 "$PORT" --pid "$SPID" --log "$OUT/server.log" --out "$OUT" --hours "$HOURS" \
  --players "${SOAK_PLAYERS:-3}" --bots "$BOTS" --quiet-every "${SOAK_QUIET_EVERY:-90}" ${SOAK_ARGS:-}
RC=$?

# a clean shutdown lets the sanitizer report leaks
kill -TERM $SPID 2>/dev/null; wait $SPID 2>/dev/null
if grep -a -E "runtime error|ERROR: AddressSanitizer|ERROR: LeakSanitizer|SERVER CRASH" "$OUT/server.log" > "$OUT/errors.log"; then
  echo "FAIL: the server log has errors (see $OUT/errors.log)"; head -20 "$OUT/errors.log"; RC=1
fi
exit $RC
