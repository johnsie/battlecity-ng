#!/usr/bin/env bash
# Headless smoke test of the native client: it must start, load every asset, draw a frame and exit cleanly.
# Uses SDL's dummy video/audio drivers and a private copy of the data directory (the client writes options.ini),
# pointed at a closed local port so it never contacts a real server.
# Usage: ci/smoke-test-client.sh <path-to-bc-client> <data-dir>
set -euo pipefail
BIN=$(readlink -f "$1")
DATA=$(readlink -f "$2")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/data"
# only what the game needs (skins and city files are large and unused)
(cd "$DATA" && cp img*.bmp map.dat options.ini Vera.ttf cloak.wav flare.wav TimGM6mb.sf2 "$WORK/data/" && cp -r wav midi "$WORK/data/")
printf '\n[GameServer]\nAddress=127.0.0.1\n' >> "$WORK/data/options.ini"

SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy BC_DATA_DIR="$WORK/data" \
  BC_SCREENSHOT="$WORK/shot.bmp" BC_SCREENSHOT_FRAMES=30 timeout 60 "$BIN"

size=$(stat -c %s "$WORK/shot.bmp")
[ "$size" -gt 100000 ] || { echo "FAIL: screenshot missing or too small ($size bytes)"; exit 1; }
echo "OK: client started, loaded its assets and rendered a frame ($size bytes)"
