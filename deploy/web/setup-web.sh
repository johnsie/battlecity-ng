#!/usr/bin/env bash
# One-time setup of the Battle City web front end on the shared Apache host. Run as root:
#     sudo ./deploy/web/setup-web.sh
# Safe to repeat. It adds the bc.outiva.com vhost and removes the OLD bc.outiva.com vhosts from
# outiva-com.conf (backed up first); other sites are untouched. Apache is reloaded only if the full
# configuration passes "apache2ctl configtest", otherwise everything is rolled back.
set -euo pipefail
cd "$(dirname "$0")"

SITE=bc-outiva-com
WEBROOT=/var/www/bc.outiva.com
CRT=/etc/ssl/certs/bc-origin.crt
KEY=/etc/ssl/private/bc-origin.key

echo "== Packages"
apt-get install -y websockify openssl >/dev/null

echo "== Apache modules"
a2enmod ssl proxy proxy_http proxy_wstunnel headers rewrite >/dev/null

echo "== Self-signed origin certificate"
if [ ! -f "$CRT" ] || [ ! -f "$KEY" ]; then
  openssl req -x509 -newkey rsa:2048 -nodes -days 3650 -sha256 \
    -subj "/CN=bc.outiva.com" \
    -addext "subjectAltName=DNS:bc.outiva.com,IP:192.168.10.181" \
    -keyout "$KEY" -out "$CRT" 2>/dev/null
  chmod 600 "$KEY"
  chmod 644 "$CRT"
  echo "   created $CRT (valid 10 years)"
else
  echo "   already present, keeping it"
fi

echo "== Web root"
install -d -o gitlab-runner -g www-data -m 0755 "$WEBROOT" "$WEBROOT/releases"

echo "== Browser gateway service"
install -m 0644 battlecity-ws.service /etc/systemd/system/battlecity-ws.service
systemctl daemon-reload
systemctl enable --now battlecity-ws

echo "== Apache vhost"
install -m 0644 bc-outiva-com.conf "/etc/apache2/sites-available/$SITE.conf"
a2ensite "$SITE" >/dev/null

# Replace the OLD bc.outiva.com vhosts (they live inside outiva-com.conf next to the real outiva.com ones).
# Only vhosts that serve nothing but the legacy names (bc.outiva.com, battlecity.orb, www.battlecity.orb)
# are removed; the outiva.com vhosts are left alone. Originals are backed up first.
BACKUP=/var/backups/bc-legacy-apache/$(date +%Y%m%d-%H%M%S)
mkdir -p "$BACKUP"
LEGACY_FILES=()
for f in /etc/apache2/sites-available/outiva-com.conf /etc/apache2/sites-enabled/outiva-com.conf; do
  [ -f "$f" ] || continue
  # sites-enabled may be a symlink/hardlink of sites-available: only handle each real file once
  real=$(readlink -f "$f")
  case " ${LEGACY_FILES[*]:-} " in *" $real "*) continue;; esac
  LEGACY_FILES+=("$real")
done
restore_legacy() {
  for real in "${LEGACY_FILES[@]:-}"; do
    [ -n "$real" ] && [ -f "$BACKUP/$(echo "$real" | tr / _)" ] && cp -p "$BACKUP/$(echo "$real" | tr / _)" "$real"
  done
}
for real in "${LEGACY_FILES[@]:-}"; do
  [ -n "$real" ] || continue
  cp -p "$real" "$BACKUP/$(echo "$real" | tr / _)"
  python3 ./remove-legacy-vhost.py "$real" || { restore_legacy; a2dissite "$SITE" >/dev/null; exit 1; }
done

if apache2ctl configtest 2>&1 | tee /tmp/bc-configtest.log | grep -q "Syntax OK"; then
  systemctl reload apache2
  echo "   Apache reloaded (backup of the old config: $BACKUP)"
else
  echo "!! Apache configuration test FAILED - rolling back, nothing was reloaded:"
  cat /tmp/bc-configtest.log
  restore_legacy
  a2dissite "$SITE" >/dev/null
  exit 1
fi

# Until the first web deploy there is no 'current' release: serve a placeholder so the site answers
if [ ! -e "$WEBROOT/current" ]; then
  install -d -o gitlab-runner -g www-data "$WEBROOT/releases/initial"
  printf '<!doctype html><meta charset="utf-8"><title>Battle City</title><p>Battle City is being set up.</p>\n' > "$WEBROOT/releases/initial/index.html"
  chown gitlab-runner:www-data "$WEBROOT/releases/initial/index.html"
  ln -sfn "$WEBROOT/releases/initial" "$WEBROOT/current"
fi

echo
echo "Done. Checks:"
echo "  systemctl status battlecity-ws"
echo "  curl -kI --resolve bc.outiva.com:443:127.0.0.1 https://bc.outiva.com/"
echo "HAProxy must send SNI/Host bc.outiva.com to 192.168.10.181:443 (TLS, verification off)."
