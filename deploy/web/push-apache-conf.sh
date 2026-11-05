#!/usr/bin/env bash
# Run from a dev machine: copies deploy/web/bc-outiva-com.conf to the production host, tests it and reloads Apache.
# Asks for the host's sudo password once. Usage: deploy/web/push-apache-conf.sh [user@host]
set -euo pipefail
HOST="${1:-john@192.168.10.181}"
HERE="$(cd "$(dirname "$0")" && pwd)"
scp "$HERE/bc-outiva-com.conf" "$HOST:/tmp/bc-outiva-com.conf"
ssh -t "$HOST" 'set -e
  sudo install -o root -g root -m 644 /etc/apache2/sites-available/bc-outiva-com.conf /tmp/bc-outiva-com.conf.bak 2>/dev/null || true
  sudo install -o root -g root -m 644 /tmp/bc-outiva-com.conf /etc/apache2/sites-available/bc-outiva-com.conf
  rm -f /tmp/bc-outiva-com.conf
  sudo apachectl configtest
  sudo systemctl reload apache2'
echo "== Header count (expect 1)"
ssh "$HOST" 'curl -sk -D - -o /dev/null -H "Origin: https://outiva.com" --resolve bc.outiva.com:443:127.0.0.1 https://bc.outiva.com/outiva-status.json | grep -ic "^access-control-allow-origin"'
