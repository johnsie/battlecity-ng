#!/usr/bin/env bash
# One-time setup of the game server host. Run as root:  sudo ./deploy/setup.sh
set -euo pipefail
cd "$(dirname "$0")"

id battlecity >/dev/null 2>&1 || useradd --system --home /var/lib/battlecity --shell /usr/sbin/nologin battlecity
install -d -o gitlab-runner -g gitlab-runner -m 0755 /opt/battlecity /opt/battlecity/releases
install -d -o battlecity -g battlecity -m 0750 /var/lib/battlecity

install -m 0644 battlecity.service /etc/systemd/system/battlecity.service
install -m 0440 sudoers-battlecity /etc/sudoers.d/battlecity
visudo -cf /etc/sudoers.d/battlecity

systemctl daemon-reload
systemctl enable battlecity

# Firewall (only if ufw is in use)
if command -v ufw >/dev/null && ufw status | grep -q "Status: active"; then
  ufw allow 9680/tcp comment 'Battle City'
fi
echo "Done. The service starts after the first successful deploy."
