#!/usr/bin/env bash
# Deploys a PUBLISHED GitHub release to THIS machine: downloads the server and web packages, verifies them against the
# release's SHA256SUMS.txt, then runs ci/deploy.sh (game server) and ci/deploy-web.sh (website).
# What runs in production is therefore byte-for-byte what was built, tested and published; nothing is rebuilt here.
# Usage: ci/deploy-release.sh <tag> [owner/repo]        e.g. ci/deploy-release.sh v0.3.0
set -euo pipefail
TAG=$1
REPO=${2:-${GITHUB_REPOSITORY:-johnsie/battlecity-ng}}
VERSION=${TAG#v}
BASE=${BC_RELEASE_BASE:-"https://github.com/$REPO/releases/download/$TAG"}   # override for tests
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

SERVER="battlecity-server-$VERSION-linux-x86_64.tar.gz"
WEB="battlecity-web-$VERSION.tar.gz"
echo "== Downloading $TAG from $REPO"
for f in SHA256SUMS.txt "$SERVER" "$WEB"; do
  curl -fsSL --retry 3 -o "$f" "$BASE/$f"
done
echo "== Verifying checksums"
grep -E " \*?($SERVER|$WEB)\$" SHA256SUMS.txt > wanted.txt
[ "$(wc -l < wanted.txt)" = 2 ] || { echo "SHA256SUMS.txt does not list both packages"; exit 1; }
sha256sum -c wanted.txt

echo "== Game server"
"$HERE/deploy.sh" "$WORK/$SERVER" "$TAG"

echo "== Website"
mkdir web && tar -xzf "$WEB" -C web --strip-components=1
"$HERE/deploy-web.sh" web "$TAG"
