# Releasing and deploying

**GitLab is the source of truth** (`https://gitlab.outiva.com/devteam/battlecity-ng`, remote `gitlab`). GitHub (`origin`) is a mirror and publishes the downloadable native packages.

## What deploys when

| You push | GitLab CI does | GitHub Actions does |
|----------|----------------|---------------------|
| `master` | build, test (smoke, sanitizers, fuzz, relay, load), package and deploy the **Linux server** to bc.outiva.com (skipped for documentation-only changes) | build and test Linux and Windows clients and servers |
| a tag `vX.Y.Z[-rcN]` | the same, plus build and deploy the **web client** | build, package and publish a **GitHub Release**: Linux and Windows clients and servers, with checksums |

Changes under `web/` also redeploy the web page. Pushing `master` restarts the production server (players are disconnected and their clients restart at the login screen), so batch changes.

## Releasing

1. Make sure the pipeline for `master` is green.
2. If the wire protocol changed, bump `VERSION` in both `server/CConstants.h` and `client/CConstants.h`. **Clients with a different version are refused and told to update; there is no overlap period.**
3. Tag and push to GitLab first, then GitHub:

       git tag v0.3.0
       git push gitlab master v0.3.0
       git push origin master v0.3.0

4. Watch the GitLab pipeline (server + web) and the GitHub `release` workflow (native packages). Deploy jobs check that port 9680 answers (rolling back automatically if not) and that the web page returns 200.
5. Hard-refresh the web client and connect once.

## Host setup

The production host, the web front end (Apache, websockify, HAProxy) and the Windows server service are described in `deploy/README.md` and `deploy/windows/README.md`.

## Release checklist

- [ ] All CI jobs green on `master`
- [ ] Load test (20 bots) passes
- [ ] A native client connects to a staging server first (`BC_SERVER=<staging>`), never the live one by accident
- [ ] The web page loads and a test account can join, build and chat
- [ ] Database backup exists (`/var/lib/battlecity/backups`, automatic hourly before restarts)
