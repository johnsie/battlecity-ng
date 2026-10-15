#!/usr/bin/env bash
# Publishes a directory of static web files to the Apache web root on THIS machine.
# Layout: /var/www/bc.outiva.com/releases/<version>/  +  current -> releases/<version>
# Usage: ci/deploy-web.sh <source-dir> <version>
set -euo pipefail
SRC=$(readlink -f "$1")
VERSION=$2
ROOT=/var/www/bc.outiva.com
REL="$ROOT/releases/$VERSION"

[ -f "$SRC/index.html" ] || { echo "no index.html in $SRC"; exit 1; }
echo "== Publishing $VERSION"
rm -rf "$REL"
mkdir -p "$REL"
cp -a "$SRC/." "$REL/"
chmod -R a+rX,go-w "$REL"
ln -sfn "$REL" "$ROOT/current"

echo "== Checking"
code=$(curl -ks -o /dev/null -w '%{http_code}' --resolve bc.outiva.com:443:127.0.0.1 https://bc.outiva.com/ || true)
echo "   https://bc.outiva.com/ -> HTTP $code"
[ "$code" = "200" ] || { echo "site did not answer 200"; exit 1; }
ls -1dt "$ROOT"/releases/* | tail -n +6 | xargs -r rm -rf   # keep the 5 newest
