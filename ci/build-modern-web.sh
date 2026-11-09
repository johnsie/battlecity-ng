#!/usr/bin/env bash
# Builds the Godot Modern Preview for the browser using pinned, verified tools.
# Usage: ci/build-modern-web.sh <out-dir>
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(readlink -m "$1")
GODOT_VERSION=4.7.2
GODOT_TAG="${GODOT_VERSION}-stable"
CACHE=${BC_GODOT_CACHE:-$HOME/.cache/bc-godot-$GODOT_VERSION}
ENGINE_ZIP="$CACHE/Godot_v${GODOT_TAG}_linux.x86_64.zip"
ENGINE_DIR="$CACHE/engine"
ENGINE="$ENGINE_DIR/Godot_v${GODOT_TAG}_linux.x86_64"
TEMPLATES="$CACHE/Godot_v${GODOT_TAG}_export_templates.tpz"
DATA_HOME="$CACHE/data"
TEMPLATE_DIR="$DATA_HOME/godot/export_templates/${GODOT_VERSION}.stable"

# Digests published with the official 4.7.2 release assets.
ENGINE_SHA256=cadd3204e728a35d3f13adb7fd0d7902636b79f6b95c40c265eb73b6c35329e4
TEMPLATES_SHA256=f298490b8d44d934be425a5a65a51bf15f422428b229a06a6e11d9ffea248011
BASE_URL="https://github.com/godotengine/godot-builds/releases/download/$GODOT_TAG"

mkdir -p "$CACHE" "$ENGINE_DIR" "$CACHE/config" "$CACHE/runtime-cache" "$DATA_HOME"

if [ ! -x "$ENGINE" ]; then
  curl -L --fail --retry 3 --output "$ENGINE_ZIP" "$BASE_URL/$(basename "$ENGINE_ZIP")"
  echo "$ENGINE_SHA256  $ENGINE_ZIP" | sha256sum -c -
  unzip -oq "$ENGINE_ZIP" -d "$ENGINE_DIR"
  chmod +x "$ENGINE"
  rm -f "$ENGINE_ZIP"
fi

if [ ! -f "$TEMPLATE_DIR/web_nothreads_release.zip" ]; then
  if [ ! -f "$TEMPLATES" ]; then
    curl -L --fail --retry 3 --output "$TEMPLATES" "$BASE_URL/$(basename "$TEMPLATES")"
  fi
  echo "$TEMPLATES_SHA256  $TEMPLATES" | sha256sum -c -
  unpack=$(mktemp -d)
  trap 'rm -rf "$unpack"' EXIT
  # The official archive also contains every desktop/mobile template and expands
  # to several gigabytes. The Modern Preview only needs the small web subset.
  unzip -oq "$TEMPLATES" 'templates/web_nothreads_release.zip' 'templates/version.txt' -d "$unpack"
  mkdir -p "$TEMPLATE_DIR"
  cp -a "$unpack/templates/." "$TEMPLATE_DIR/"
  rm -f "$TEMPLATES"
fi

rm -rf "$OUT"
mkdir -p "$OUT"
XDG_CONFIG_HOME="$CACHE/config" XDG_CACHE_HOME="$CACHE/runtime-cache" XDG_DATA_HOME="$DATA_HOME" \
  "$ENGINE" --headless --path "$ROOT/premium-client" --export-release Web "$OUT/index.html"

test -s "$OUT/index.html"
test -s "$OUT/index.wasm"
test -s "$OUT/index.pck"
echo "Modern Preview web build:"
du -h "$OUT"/*
