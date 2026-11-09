#!/usr/bin/env bash
# Imports, runs and captures the deterministic Modern Preview scene.
# Usage: ci/smoke-test-premium-client.sh /path/to/godot
set -euo pipefail

GODOT_BIN=$(readlink -f "$1")
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PROJECT="$ROOT/premium-client"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/config" "$WORK/cache" "$WORK/data"

XDG_CONFIG_HOME="$WORK/config" XDG_CACHE_HOME="$WORK/cache" XDG_DATA_HOME="$WORK/data" \
  "$GODOT_BIN" --headless --path "$PROJECT" --editor --quit

RUNNER=()
if command -v xvfb-run >/dev/null 2>&1; then
  RUNNER=(xvfb-run -a)
fi

XDG_CONFIG_HOME="$WORK/config" XDG_CACHE_HOME="$WORK/cache" XDG_DATA_HOME="$WORK/data" \
  BC_PREMIUM_CAPTURE="$WORK/premium.png" \
  "${RUNNER[@]}" "$GODOT_BIN" --path "$PROJECT" --resolution 1600x900

test -s "$WORK/premium.png"
dimensions=$(file "$WORK/premium.png")
case "$dimensions" in
  *"1600 x 900"*) ;;
  *) echo "FAIL: unexpected Modern Preview capture: $dimensions" >&2; exit 1 ;;
esac

size=$(stat -c %s "$WORK/premium.png")
test "$size" -gt 30000 || { echo "FAIL: Modern Preview capture is unexpectedly small ($size bytes)" >&2; exit 1; }
echo "OK: Modern Preview imported, ran and rendered a 1600x900 frame ($size bytes)"
