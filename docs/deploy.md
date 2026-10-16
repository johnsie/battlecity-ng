# Releasing and deploying

**GitLab is the source of truth** (`https://gitlab.outiva.com/devteam/battlecity-ng`, remote `gitlab`). GitHub (`origin`) builds, tests, packages and publishes **every** deliverable (Linux and Windows servers and clients, and the web client) and deploys the Linux server and website to the LAN host.

## How it works

    developer --push--> GitLab (source of truth) --mirror/push--> GitHub
                                                                    |
                          GitHub Actions: build + test + package (all targets)
                                                                    |
          GitHub Release (signed-off assets, SHA256SUMS, provenance attestations)
                                                                    |
              self-hosted runner on the LAN host (outbound connection only)
                                                                    |
        downloads the release, verifies checksums, deploys server + website

Principles: **build once, deploy that exact artifact** (nothing is rebuilt on the host), **releases are immutable tags**, production is reached only through a protected GitHub environment, and the host runs a runner that can do nothing as root except restart the game service.

| Event | GitHub Actions does |
|-------|---------------------|
| push / pull request | `build`: Linux, Windows client (MSYS2), Windows server (MSVC), web client (Emscripten); unit, smoke, sanitizer, fuzz, relay and 20-bot load tests |
| tag `vX.Y.Z[-rcN]` | `release`: builds and packages **Linux and Windows servers and clients plus the web client**, writes `SHA256SUMS.txt`, attests build provenance, publishes the GitHub Release, then calls `deploy` |
| `deploy` (after a release, or run by hand with a tag) | on the LAN runner, in the `lan-production` environment (optionally gated by an approval): download the release, verify the checksums, install the Linux server (health-checked, automatic rollback) and the website |

GitLab CI (`.gitlab-ci.yml`) still runs build and tests on the LAN runner for quick feedback. Its deploy jobs are **manual fallbacks** only. Pushing to `master` no longer restarts production: only a tagged release does, so players are disconnected only when you decide.

Set-up of the runner and the GitHub environment: `deploy/github-runner/README.md`.

## Releasing

1. Make sure the `build` workflow is green on `master`.
2. If the wire protocol changed, bump `VERSION` in `server/CConstants.h` and `client/CConstants.h`. **Clients with a different version are refused and told to update; there is no overlap period.**
3. Push to GitLab (source of truth) and to GitHub, then tag:

       git push gitlab master && git push origin master
       git tag v0.3.0
       git push gitlab v0.3.0 && git push origin v0.3.0

4. Watch the `release` workflow on GitHub. Approve the `lan-production` deployment when prompted. The deploy checks that port 9680 answers (rolling back automatically if not) and that the web page returns 200.
5. Hard-refresh the web client and connect once.

Rollback or redeploy: Actions > deploy > Run workflow > enter an earlier tag.

## Host setup

The production host, the web front end (Apache, websockify, HAProxy) and the Windows server service are described in `deploy/README.md` and `deploy/windows/README.md`.

## Release checklist

- [ ] All CI jobs green on `master`
- [ ] Load test (20 bots) passes
- [ ] A native client connects to a staging server first (`BC_SERVER=<staging>`), never the live one by accident
- [ ] The web page loads and a test account can join, build and chat
- [ ] Database backup exists (`/var/lib/battlecity/backups`, automatic hourly before restarts)
