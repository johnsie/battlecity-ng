#!/usr/bin/env bash
# Bundles a built client into a release archive: the executable, the game data it needs, and (Windows) the DLLs it links.
# Usage: ci/package-client.sh <path-to-bc-client[.exe]> <version> <linux-x86_64|windows-x64> <out-dir>
set -euo pipefail
BIN=$1; VERSION=$2; PLATFORM=$3; OUT=$(mkdir -p "$4" && readlink -f "$4")
STAGE=$(mktemp -d); trap 'rm -rf "$STAGE"' EXIT
DIR="$STAGE/battlecity-client-$VERSION"
mkdir -p "$DIR/data"
(cd data && cp img*.bmp map.dat options.ini Vera.ttf cloak.wav flare.wav TimGM6mb.sf2 TimGM6mb.LICENSE.txt "$DIR/data/" && cp -r wav midi "$DIR/data/")
install -m 0755 "$BIN" "$DIR/"
cp LICENSE.txt "$DIR/"
echo "$VERSION" > "$DIR/VERSION"
case "$PLATFORM" in
  windows-*)
    # copy the non-system DLLs (SDL2 and the compiler runtime) next to the exe
    ldd "$BIN" | awk '/=>/ {print $3}' | grep -iv '/windows/' | while read -r dll; do cp "$dll" "$DIR/"; done
    (cd "$STAGE" && zip -qr "$OUT/battlecity-client-$VERSION-$PLATFORM.zip" "battlecity-client-$VERSION")
    ;;
  *)
    tar -C "$STAGE" -czf "$OUT/battlecity-client-$VERSION-$PLATFORM.tar.gz" "battlecity-client-$VERSION"
    ;;
esac
ls -l "$OUT"
