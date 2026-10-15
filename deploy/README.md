# Deploying the Battle City server

Target: Linux host running the GitLab runner tagged `ubuntu-server`; public name `bc.outiva.com`, game port **9680/tcp**.

## One-time host setup
1. `sudo apt install build-essential cmake libsqlite3-dev`
2. `sudo ./deploy/setup.sh` — creates the `battlecity` user, `/opt/battlecity`, `/var/lib/battlecity`,
   the systemd unit, a sudoers rule limited to restarting the service, and the firewall rule.
3. Make sure the router forwards 9680/tcp to this host if players connect from outside the LAN.

## Deploying
Pipeline: build → smoke test → package → automatic `deploy:production` job (every push to master, and tags).
The job installs the release under `/opt/battlecity/releases/<version>`, backs up the database,
restarts the service and checks port 9680, rolling back automatically if it does not come up.

State lives in `/var/lib/battlecity` (`dat/BCData.db`, `logs/`, `map.dat`, `news.txt`) and survives deploys.
Backups of the database: `/var/lib/battlecity/backups` (newest 14 kept).

## Useful commands
    systemctl status battlecity
    journalctl -u battlecity -f
    BC_PORT=9680 /opt/battlecity/current/bc-server   # run by hand (from /var/lib/battlecity)
