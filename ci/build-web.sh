#!/usr/bin/env bash
# Builds the browser client with Emscripten and collects the files to publish.
# The Emscripten SDK is installed once into a cache directory and reused by later runs.
# Usage: ci/build-web.sh <build-dir> <out-dir>
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD=$(readlink -m "$1")
OUT=$(readlink -m "$2")
EMSDK_VERSION=${EMSDK_VERSION:-3.1.74}
EMSDK_DIR=${EMSDK_DIR:-$HOME/.cache/bc-emsdk}

installed_version() { "$EMSDK_DIR/upstream/emscripten/emcc" --version 2>/dev/null | head -n1 | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' | head -n1 || true; }

mkdir -p "$(dirname "$EMSDK_DIR")"
# One installer at a time (several jobs may start together)
(
  flock 9
  if [ "$(installed_version)" != "$EMSDK_VERSION" ]; then
    echo "== Installing Emscripten $EMSDK_VERSION into $EMSDK_DIR (first run only)"
    [ -d "$EMSDK_DIR/.git" ] || git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
    (cd "$EMSDK_DIR" && ./emsdk install "$EMSDK_VERSION" && ./emsdk activate "$EMSDK_VERSION")
  fi
) 9>"$EMSDK_DIR.lock"

# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null
echo "== $(emcc --version | head -n1)"

emcmake cmake -S "$ROOT" -B "$BUILD" -DBC_BUILD_SERVER=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"$(nproc)"

rm -rf "$OUT"
mkdir -p "$OUT"
for f in index.html index.js index.wasm index.data TimGM6mb.sf2; do
  cp "$BUILD/client/$f" "$OUT/$f"
done
ls -l "$OUT"
