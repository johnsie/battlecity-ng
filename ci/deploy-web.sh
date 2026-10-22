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

CHECK_DIR=$(mktemp -d)
trap 'rm -rf "$CHECK_DIR"' EXIT
status_code=$(curl -ksS -o "$CHECK_DIR/body" -D "$CHECK_DIR/headers" -w '%{http_code}' \
  -H 'Origin: https://outiva.com' --resolve bc.outiva.com:443:127.0.0.1 \
  https://bc.outiva.com/outiva-status.json || true)
echo "   https://bc.outiva.com/outiva-status.json -> HTTP $status_code"
[ "$status_code" = "200" ] || { echo "status endpoint did not answer 200"; cat "$CHECK_DIR/body"; exit 1; }
grep -Eq '^\{"status":"available","players":[0-9]+\}$' "$CHECK_DIR/body" || {
  echo "status endpoint returned an invalid body"; cat "$CHECK_DIR/body"; exit 1;
}
grep -Eiq '^Access-Control-Allow-Origin: https://outiva\.com\r?$' "$CHECK_DIR/headers" || {
  echo "status endpoint is missing the Outiva CORS header"; exit 1;
}
grep -Eiq '^Cache-Control: public, max-age=15\r?$' "$CHECK_DIR/headers" || {
  echo "status endpoint is missing the 15-second cache header"; exit 1;
}
ls -1dt "$ROOT"/releases/* | tail -n +6 | xargs -r rm -rf   # keep the 5 newest
