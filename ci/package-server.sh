#!/usr/bin/env bash
# Bundles the built server into a release tarball: bc-server + map.dat.
# Usage: ci/package-server.sh <build-dir> <version> <out-dir>
set -euo pipefail
BUILD=$1; VERSION=$2; OUT=$3
STAGE=$(mktemp -d); trap 'rm -rf "$STAGE"' EXIT
DIR="$STAGE/battlecity-server-$VERSION"
mkdir -p "$DIR" "$OUT"
install -m 0755 "$BUILD/server/bc-server" "$DIR/bc-server"
install -m 0755 ci/prestart.sh "$DIR/prestart.sh"
install -m 0644 data/map.dat "$DIR/map.dat"
echo "$VERSION" > "$DIR/VERSION"
tar -C "$STAGE" -czf "$OUT/battlecity-server-$VERSION-linux-x86_64.tar.gz" "battlecity-server-$VERSION"
ls -l "$OUT"
