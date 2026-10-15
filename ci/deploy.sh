#!/usr/bin/env bash
# Deploys a server tarball on THIS machine (run by the ubuntu-server GitLab runner).
# Layout:  /opt/battlecity/releases/<version>/   immutable release
#          /opt/battlecity/current -> releases/<version>
#          /var/lib/battlecity/                  persistent state, owned by the service user; the runner
#                                                never touches it (ci/prestart.sh seeds the map and backs up the DB)
# Rolls back to the previous release if the new one does not come up on the game port.
# Usage: ci/deploy.sh <tarball> <version>
set -euo pipefail

TARBALL=$(readlink -f "$1")
VERSION=$2
APP=/opt/battlecity
STATE=/var/lib/battlecity
PORT=${BC_PORT:-9680}
SERVICE=battlecity

REL="$APP/releases/$VERSION"
PREV=$(readlink -f "$APP/current" 2>/dev/null || true)

echo "== Installing release $VERSION"
mkdir -p "$REL"
tar -xzf "$TARBALL" -C "$REL" --strip-components=1

wait_for_port() {
  for _ in $(seq 1 30); do
    if (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null; then return 0; fi
    sleep 1
  done
  return 1
}

echo "== Switching to $VERSION and restarting"
ln -sfn "$REL" "$APP/current"
sudo -n /usr/bin/systemctl restart "$SERVICE"

if wait_for_port; then
  echo "== Healthy: $SERVICE is accepting connections on port $PORT"
  ls -1dt "$APP"/releases/* | tail -n +6 | xargs -r rm -rf   # keep the 5 newest releases
  exit 0
fi

echo "!! $SERVICE did not come up on port $PORT"
sudo -n /usr/bin/journalctl -u "$SERVICE" -n 40 --no-pager || true
if [ -n "$PREV" ] && [ "$PREV" != "$REL" ]; then
  echo "!! Rolling back to $PREV"
  ln -sfn "$PREV" "$APP/current"
  sudo -n /usr/bin/systemctl restart "$SERVICE"
fi
exit 1
