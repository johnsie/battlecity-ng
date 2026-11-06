# Releasing and deploying

**GitLab is the source of truth** (`https://gitlab.outiva.com/devteam/battlecity-ng`, remote `gitlab`). GitLab CI and GitHub Actions are two separate pipelines for two separate jobs, and neither depends on, waits for or calls the other:

- **GitLab CI** (runner `ubuntu-server`, on the LAN host) builds, tests, packages and **deploys production** (the Linux server and the website).
- **GitHub Actions** builds and publishes the **public releases** (Linux and Windows servers and clients, the web client, checksums, provenance). It never deploys anything and never reaches the LAN.

## How it works

    developer --push + tag--> GitLab --> GitLab CI: build, test, package, deploy to this host
                          \--> GitHub --> GitHub Actions: build, test, package ALL targets, publish the Release

Both start from the same tag, and each finishes in its own time. Production is built from the tag by GitLab itself, so a slow or failing GitHub build never holds up a deploy (and a failed deploy never blocks a public release). Releases are immutable tags.

| Event | What runs |
|-------|-----------|
| push / pull request | GitHub `build`: Linux, Windows client (MSYS2), Windows server (MSVC), web client (Emscripten); unit, smoke, sanitizer, fuzz, relay and 20-bot load tests. GitLab CI runs build and tests on the LAN too. |
| tag `vX.Y.Z[-rcN]` pushed to GitHub | GitHub `release`: builds and packages **Linux and Windows servers and clients plus the web client**, writes `SHA256SUMS.txt`, attests provenance, publishes the GitHub Release |
| tag pushed to GitLab | GitLab pipeline (`package:server-linux`, `build:web`, then `deploy:production`) builds the server and the web client from the tag, then installs them on the LAN host (the server is health-checked with automatic rollback) |

Pushing to `master` never restarts production: only a pushed `v*` tag does (automatically), so players are disconnected only when you tag a release. Rollback or redeploy: re-run `deploy:production` in an older tag's pipeline (its artifacts are kept for 4 weeks), or switch `/opt/battlecity/current` back to an older release directory.

## Releasing

1. Make sure the `build` workflow is green on `master`.
2. If the wire protocol changed, bump `VERSION` in `server/CConstants.h` and `client/CConstants.h`. **Clients with a different version are refused and told to update; there is no overlap period.**
3. Push to both remotes, then tag:

       git push gitlab master && git push origin master
       git tag v0.3.0
       git push gitlab v0.3.0 && git push origin v0.3.0

4. Watch the `release` workflow on GitHub until the Release appears.
5. Before the first release containing the console integration, run `sudo ./deploy/setup.sh` and
   `sudo ./deploy/web/setup-web.sh` on the host. The tag's GitLab pipeline then runs
   `deploy:production` by itself. It checks the game and status ports (rolling back automatically if either fails), then checks the
   public status JSON and CORS/cache headers.
6. Check the console status contract, then hard-refresh the web client and connect once:

       curl -i https://bc.outiva.com/outiva-status.json
       curl -i -H 'Origin: https://outiva.com' https://bc.outiva.com/outiva-status.json

   Expect a JSON-only response with `available` and an integer `players`, `Cache-Control: public, max-age=15`, and
   `Access-Control-Allow-Origin: https://outiva.com`. Stop `battlecity.service` briefly in a maintenance window to
   verify `degraded`; a timed-out or malformed source returns `unknown` with HTTP 503.

## Host setup

The production host, the web front end (Apache, websockify, HAProxy) and the Windows server service are described in `deploy/README.md` and `deploy/windows/README.md`.

The public `/outiva-status.json` endpoint is served by `battlecity-status.service`. It caches the loopback-only game
source (`127.0.0.1:9682`) for 15 seconds and constructs a fresh allow-listed response containing only status and a
count. It never proxies player records. Re-run `sudo ./deploy/web/setup-web.sh` after changing its service, adapter or
Apache configuration.

Rollback the console integration by removing the two `ProxyPass` lines and the status `<Location>` block from the
Apache vhost, running `apache2ctl configtest`, and reloading Apache. Then disable the adapter with
`systemctl disable --now battlecity-status`. The loopback source may remain enabled safely; set `BC_STATUS_PORT=0` in
`battlecity.service` and restart the game server if it must also be disabled.

## Release checklist

- [ ] All CI jobs green on `master`
- [ ] Load test (20 bots) passes
- [ ] A native client connects to a staging server first (`BC_SERVER=<staging>`), never the live one by accident
- [ ] The web page loads and a test account can join, build and chat
- [ ] Database backup exists (`/var/lib/battlecity/backups`, automatic hourly before restarts)
