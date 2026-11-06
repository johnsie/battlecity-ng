#!/usr/bin/env bash
# Sets up Outiva login for Battle City on the host (run once as root; safe to run again):
#   * makes the secret the broker and the game server share (kept if it exists)
#   * installs and starts the login broker
#   * tells the game server where the secret is (a systemd drop-in; the game server must be restarted afterwards)
#   * routes /outiva/ticket on the website to the broker (Apache; see bc-outiva-com.conf)
# Guests and Outiva login are then available. The old name-and-password login stays on until you set BC_LEGACY_LOGIN=0 in
# /etc/systemd/system/battlecity.service.d/outiva-login.conf and restart the game server.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
[ "$(id -u)" -eq 0 ] || { echo "Run as root (sudo)." >&2; exit 1; }

install -d -m 0750 -o root -g battlecity /etc/battlecity
if [ ! -s /etc/battlecity/ticket.secret ]; then
  umask 077
  head -c 48 /dev/urandom | base64 -w0 > /etc/battlecity/ticket.secret
  echo >> /etc/battlecity/ticket.secret
  echo "created /etc/battlecity/ticket.secret"
fi
chown root:battlecity /etc/battlecity/ticket.secret
chmod 0640 /etc/battlecity/ticket.secret

install -d /usr/local/lib/battlecity
install -m 0755 "$HERE/outiva_broker.py" /usr/local/lib/battlecity/outiva_broker.py
install -m 0644 "$HERE/battlecity-broker.service" /etc/systemd/system/battlecity-broker.service

install -d /etc/systemd/system/battlecity.service.d
if [ ! -f /etc/systemd/system/battlecity.service.d/outiva-login.conf ]; then
  cat > /etc/systemd/system/battlecity.service.d/outiva-login.conf <<'CONF'
[Service]
Environment=BC_TICKET_SECRET_FILE=/etc/battlecity/ticket.secret
# Set to 0 once everyone uses Outiva or plays as a guest: turns the old name-and-password login and registration off
Environment=BC_LEGACY_LOGIN=1
Environment=BC_GUEST_LOGIN=1
CONF
fi
systemctl daemon-reload
systemctl enable --now battlecity-broker
systemctl restart battlecity-broker

if ! grep -q "/outiva/ticket" /etc/apache2/sites-available/bc-outiva-com.conf; then
  echo "Apache is not routing /outiva/ticket yet: install the current deploy/web/bc-outiva-com.conf (deploy/web/push-apache-conf.sh), then reload Apache."
fi
echo "Restart the game server to load the secret:  sudo systemctl restart battlecity"
