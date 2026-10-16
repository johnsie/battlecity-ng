#!/usr/bin/env bash
# Bundles the built browser client (the output of ci/build-web.sh) into a release tarball.
# Usage: ci/package-web.sh <web-build-dir> <version> <out-dir>
set -euo pipefail
SRC=$(readlink -f "$1"); VERSION=$2; OUT=$(mkdir -p "$3" && readlink -f "$3")
[ -f "$SRC/index.html" ] || { echo "no index.html in $SRC"; exit 1; }
STAGE=$(mktemp -d); trap 'rm -rf "$STAGE"' EXIT
DIR="$STAGE/battlecity-web-$VERSION"
mkdir -p "$DIR"
cp -a "$SRC/." "$DIR/"
echo "$VERSION" > "$DIR/VERSION"
tar -C "$STAGE" -czf "$OUT/battlecity-web-$VERSION.tar.gz" "battlecity-web-$VERSION"
ls -l "$OUT"
