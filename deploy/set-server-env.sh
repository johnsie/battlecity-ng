#!/usr/bin/env bash
# Sets one per-host server setting that must not live in the repository (the Discord webhook URL is a secret).
# Run as root on the game server host:
#     sudo ./deploy/set-server-env.sh BC_DISCORD_WEBHOOK 'https://discord.com/api/webhooks/...'
# The value goes to /etc/battlecity/server.env (readable by root and the battlecity service user only), the unit is refreshed
# if needed and the service is restarted so it takes effect (players are disconnected for a moment).
set -euo pipefail
[ "$(id -u)" = 0 ] || { echo "run as root (sudo)"; exit 1; }
[ $# -eq 2 ] || { echo "usage: $0 NAME VALUE"; exit 2; }
NAME=$1; VALUE=$2
case "$NAME" in BC_[A-Z0-9_]*) ;; *) echo "the name must look like BC_SOMETHING"; exit 2;; esac
cd "$(dirname "$0")"
install -d -o root -g battlecity -m 0750 /etc/battlecity
touch /etc/battlecity/server.env
chown root:battlecity /etc/battlecity/server.env
chmod 0640 /etc/battlecity/server.env
grep -v "^${NAME}=" /etc/battlecity/server.env > /etc/battlecity/server.env.new || true
printf '%s=%s\n' "$NAME" "$VALUE" >> /etc/battlecity/server.env.new
chown root:battlecity /etc/battlecity/server.env.new
chmod 0640 /etc/battlecity/server.env.new
mv /etc/battlecity/server.env.new /etc/battlecity/server.env
install -m 0644 battlecity.service /etc/systemd/system/battlecity.service
systemctl daemon-reload
systemctl restart battlecity
sleep 2
systemctl is-active battlecity
echo "Set $NAME. The server log should say discord=on for BC_DISCORD_WEBHOOK:  journalctl -u battlecity -n 20 | grep CBotManager"
