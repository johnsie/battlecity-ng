#!/usr/bin/env bash
# Copies the files the browser client needs (and nothing else) into a staging directory that is
# preloaded into the Emscripten virtual file system at "/".
# Usage: tools/stage-web-data.sh <out-dir>
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$1
rm -rf "$OUT"
mkdir -p "$OUT"
cd "$ROOT/data"
cp img*.bmp map.dat options.ini Vera.ttf cloak.wav flare.wav TimGM6mb.sf2 "$OUT/"
mkdir -p "$OUT/wav" "$OUT/midi"
cp wav/*.wav "$OUT/wav/"
cp midi/*.mid "$OUT/midi/"
du -sh "$OUT" | sed 's/^/staged: /'
