# Deploying the Battle City server

Target: Linux host running the GitLab runner tagged `ubuntu-server`; public name `bc.outiva.com`, game port **9680/tcp**.

## One-time host setup
1. `sudo apt install build-essential cmake libsqlite3-dev`
2. `sudo ./deploy/setup.sh` — creates the `battlecity` user, `/opt/battlecity`, `/var/lib/battlecity`,
   the systemd unit, a sudoers rule limited to restarting the service, and the firewall rule.
3. Make sure the router forwards 9680/tcp to this host if players connect from outside the LAN.

## Deploying
Pipeline: build → smoke test → package → automatic `deploy:production` job (every push to master, and tags).
The job installs the release under `/opt/battlecity/releases/<version>`, restarts the service and checks
port 9680, rolling back automatically if it does not come up. The runner never touches the state directory:
the service's `ExecStartPre` (`ci/prestart.sh`) seeds `map.dat` and backs up the database as the service user.

State lives in `/var/lib/battlecity` (`dat/BCData.db`, `logs/`, `map.dat`, `news.txt`) and survives deploys.
Backups of the database: `/var/lib/battlecity/backups` (at most one per hour, newest 14 kept).

After changing `battlecity.service`, re-run `sudo ./deploy/setup.sh` on the host (it is safe to repeat).

## Useful commands
    systemctl status battlecity
    journalctl -u battlecity -f
    BC_PORT=9680 /opt/battlecity/current/bc-server   # run by hand (from /var/lib/battlecity)

## Web front end (https://bc.outiva.com)
Apache on the same host serves the static site and proxies browser game traffic:

    browser --wss://bc.outiva.com/ws--> HAProxy --TLS--> Apache :443 --ws--> websockify 127.0.0.1:9681 --TCP--> bc-server :9680

- Public TLS ends at HAProxy. Apache presents a self-signed *origin* certificate (`/etc/ssl/certs/bc-origin.crt`,
  10 years, SAN `bc.outiva.com` + `192.168.10.181`). HAProxy must send SNI/Host `bc.outiva.com` to `192.168.10.181:443`.
- One-time setup (root): `sudo ./deploy/web/setup-web.sh`. It installs `websockify`, enables the Apache modules, creates the
  certificate, installs the `battlecity-ws` service and the `bc-outiva-com` vhost, and **removes the old bc.outiva.com
  vhosts from outiva-com.conf** (backup in `/var/backups/bc-legacy-apache/`). Apache is reloaded only if `configtest`
  passes; otherwise everything is rolled back. Note the old vhosts also served `battlecity.orb` / `www.battlecity.orb`;
  those names are no longer served.
- Deploys: the `deploy:web` CI job publishes `web/` to `/var/www/bc.outiva.com/releases/<version>` and flips `current`
  whenever something under `web/` changes on master.
- Known limitation: the game server sees every browser player as coming from 127.0.0.1 (the gateway), so IP-based bans
  would affect all web players. A gateway that forwards the real client address is a later improvement.
- The old Let's Encrypt certificate/renewal for bc.outiva.com is no longer used by Apache; remove it when convenient with
  `sudo certbot delete --cert-name bc.outiva.com`.
