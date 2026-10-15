# Battle City NG: Roadmap

Goal: make the game stable and playable on every target, with automated build and release.

| # | Deliverable | Build | Release / deploy |
|---|-------------|-------|------------------|
| 1 | Linux server | CMake, GitLab CI and GitHub Actions | GitLab CI deploys to bc.outiva.com; GitHub Release asset too |
| 2 | Windows server | CMake (MSVC), GitHub Actions | GitHub Release asset |
| 3 | Linux client (SDL2) | CMake, GitHub Actions | GitHub Release asset |
| 4 | Windows client (SDL2) | CMake (MSVC), GitHub Actions | GitHub Release asset |
| 5 | Web client (Emscripten) | GitLab CI | GitLab CI, to the web root of bc.outiva.com |

One code base: `server/` and `client/` are built with the top-level `CMakeLists.txt` (`BC_BUILD_SERVER`, `BC_BUILD_CLIENT`). The Win32/DirectX/FMOD client has been replaced by SDL2 + Dear ImGui (see `client/platform`).

## 1. Current state (as of 2026-10-15)

Done:
- Linux server runs in production (systemd unit, `deploy/`), deployed by GitLab on pushes to `master` and on tags.
- `.gitlab-ci.yml` already builds and tests the server and the Linux client, packages and deploys the server, and builds and deploys the web client.
- SDL2 client builds on Linux; web build exists (`ci/build-web.sh`, `web/`).
- Several network fixes: send queue (`COutQueue`), receive drain and resync, sector re-request, fixed-width wire types.

Done since the first draft:
- **Windows server crash fixed.** Root cause: map start-up creates command centres with city `-1`, and `CBuildingList::newBuilding` called `City[-1]->addBuilding()`. On the Windows memory layout that was a `CPlayer`. All `City[]` accesses driven by a player, building or item city are now bounds-checked, as are player indices taken from packets. The reset trace is removed; the two null guard slots in `CServer.h` stay until the soak test (Workstream 1.7).
- Topic branches are already merged into `windows-port`, which is now the integration branch (Workstream 0).
- Audio exists on the SDL2 client (SDL mixer plus TinySoundFont).
- GitHub Actions workflows added: `.github/workflows/build.yml` and `release.yml`. **Not yet run**: the Windows jobs (MSVC server, MSYS2 client) are untested.
- GitLab CI has a sanitizer job that gates the server deploy.
- Remote tank movement: send on input change plus blend of small corrections (needs two-player testing).

Known open problems:
- Default branch: GitLab's is `main`, CI matches `master`.
- Windows client has never been built from CMake; it is expected to need fixes on its first CI run.
- Client packages ship `options.ini` whose default address is the live server (Workstream 3.4).
- No automated gameplay or protocol test yet (Workstream 2.3).

## 2. Workstreams

### Workstream 0: Branch hygiene (mostly done)
1. Merge the topic branches (`fix-send-queue`, `recv-drain`, `sector-recovery`, `audio`, `readability`, `sdl2-client`, `windows-port`) into one integration branch. Review each for overlap first; `master` and `recv-drain` already point at the same commit.
2. Decide the default branch. GitLab's default is `main`, but CI matches `master` by name. Set GitLab's default to `master`, or change the rules.
3. Remove the temporary debug output from `windows-port` once the crash is fixed (see 3.1).

Exit: one branch builds on Linux, passes `ctest`, and both smoke tests.

### Workstream 1: Server correctness (Linux and Windows, same source)
3.1 **City index audit.** Find every `City[...]` and `Player[...]` index taken from packet data or from `Player->City`. Add a bounds check at each (`0 <= idx < MAX_*`). Known sites: `CProcess.cpp:127` (done), `CPlayer.cpp:71,303,357`, `CServer.cpp:313`, `CItem.cpp:143`, plus the `CBuilding.cpp` uses via `bld->City`. Reject or ignore the packet rather than index.
3.2 **Validate all client packets.** Check length and field ranges at the top of `CProcess::ProcessData`. A hostile or buggy client must never crash the server.
3.3 **Build with AddressSanitizer and UBSan** in a Linux CI job and run the smoke test under it. This finds the Windows-only crash class on Linux. Needs `libsqlite3-dev` (already required on the runner).
3.4 **Initialisation.** Audit uninitialised members (the `CCity::hiring` bug was one). Compile with `-Wall -Wextra -Wuninitialized`; fix new warnings.
3.5 **Movement anti-cheat.** `ProcessUpdate` accepts moves within 300 units and has an odd 3000 ms `lastMove` condition. Replace with a distance-versus-elapsed-time check using the real player speed (0.38 units/ms, 1.0 for admins).
3.6 **Persistence.** Back up the DB before schema changes; keep `ci/prestart.sh` behaviour.
3.7 **Remove temporary diagnostics** (guard slots, reset trace) after a clean 24 h soak on Windows and Linux.

Exit: ASan/UBSan smoke run is clean; 24 h Windows soak with simulated clients does not crash.

### Workstream 2: Protocol and movement quality
2.1 Test the client movement changes with two real clients: send on input change, blend of small corrections, snap for large ones. Tune the 100 ms blend and the 150-unit snap threshold.
2.2 Optional second step: send only state changes plus a coarse periodic position, and add a sequence number so stale updates are dropped. Needs a protocol version bump (`VERSION`), so server and clients must ship together.
2.3 Write a headless bot client (reuse the client network code) that connects, moves and shoots. Use it in CI as a load test (for example 20 bots for 2 minutes) and to check that positions seen by two bots agree within a tolerance.
2.4 Document the wire protocol (`NetMessages.h`) in `docs/protocol.md`: packet ids, sizes, byte order, who sends what.

### Workstream 3: Client usability (Linux, Windows, web)
3.1 Complete the dialog layer: all 14 former resource dialogs usable with keyboard and mouse; text fields keep focus (a mouse-coordinate bug was already fixed for web).
3.2 **Audio.** SDL mixer for the wavs, TinySoundFont for the `.mid` music. Mute option persisted in `options.ini`. On web, start audio after the first user gesture.
3.3 Connection handling: clear errors for unreachable server, version mismatch, ban, kick. Automatic reconnect with backoff.
3.4 **Safe defaults.** (`BC_SERVER` environment override: done.) `options.ini` points at the live server by default. Add a command-line / query-string override and a visible "connected to" label so test builds never hit production by accident.
3.5 Input: gamepad optional; key rebinding; window resize and fullscreen; HiDPI.
3.6 Web specifics: the browser cannot open raw TCP. Run a WebSocket-to-TCP bridge (for example websockify) behind the web server at `wss://bc.outiva.com/ws`, forwarding to port 9680. Add it to `deploy/` and the systemd setup.
3.7 First-run experience: short in-game help, a default account flow that works without e-mail, readable fonts at 1x and 2x scale.

Exit: a new player can open the web page or the downloaded client, create an account, join a city, build, fight and chat on all three clients.

### Workstream 4: CI/CD

#### 4.1 GitLab CI (Linux server and web client)
Already present in `.gitlab-ci.yml`; changes needed:
- Add an ASan/UBSan server job (Workstream 1.3) in the `test` stage; it must pass before `deploy:production`.
- Keep the server and client jobs independent so a client failure never blocks a server deploy.
- Deploy the WebSocket bridge config with `deploy:web` or `deploy:production`.
- After each deploy, run a post-deploy check: connect a bot to the server and fetch the web page.
- Keep the rule that `master` and tags deploy automatically. Only the server job touches `/var/lib/battlecity` through the service, never the runner.
- Remove the Windows and client artifacts from GitLab's duties: the native clients are released from GitHub.

#### 4.2 GitHub Actions (Linux and Windows clients, Linux and Windows servers)
Add `.github/workflows/build.yml` (push and pull request) and `.github/workflows/release.yml` (tags `v*`).

`build.yml`:
- Matrix: `ubuntu-latest` and `windows-latest`.
- Linux: install `libsdl2-dev`; `cmake -S . -B build -DBC_BUILD_SERVER=OFF`; build; run `ci/smoke-test-client.sh`.
- Windows: MSVC; fetch SDL2 with vcpkg or the release zip; build client and server (`-DCMAKE_BUILD_TYPE=Release`), keep the server PDB.
- Run `ctest` on both. Cache vcpkg and build directories.

`release.yml` (trigger: tag `v*`):
- Build the same matrix and package:
  - `battlecity-server-<ver>-linux-x86_64.tar.gz` (the same package GitLab deploys; GitLab stays the deployer)
  - `battlecity-client-<ver>-linux-x86_64.tar.gz` (binary, `data/`, README; AppImage as a later option)
  - `battlecity-client-<ver>-windows-x64.zip` (exe, `SDL2.dll`, `data/`, PDB)
  - `battlecity-server-<ver>-windows-x64.zip` (exe, PDB, `data/map.dat`)
- Generate SHA-256 checksums and attach all files to a GitHub Release (`softprops/action-gh-release` or `gh release create`) with notes from the tag.
- Mirror the tag to GitLab so the Linux server and web client deploy from the same version.

#### 4.3 Versioning
- One version string (`VERSION` in the server and client constants) bumped on protocol changes.
- Tag `vMAJOR.MINOR.PATCH` on `master`; GitLab deploys server and web, GitHub publishes the native clients and both servers.
- Remotes: `origin` is GitHub, `gitlab` is GitLab. Push `master` and tags to both.

### Workstream 5: Windows server operations
- Run as a Windows service (NSSM or `sc create`) with a restart-on-failure policy.
- Log to a file with rotation; keep the crash handler output with the PDB.
- Document firewall rule for port 9680/tcp in `deploy/README.md`.

### Workstream 6: Documentation and quality gate
- `README.md`: what the game is, download links, how to play, how to run a server.
- `docs/protocol.md`, `docs/building.md` (Linux, Windows, web), `docs/deploy.md`.
- Release checklist: all CI green, 24 h soak, bot load test, web page loads, native clients connect to a staging server first.

## 3. Milestones

| Milestone | Theme | Contents | Done when |
|-----------|-------|----------|-----------|
| **Now: Stabilise** | Stop the crashes | Workstream 0; Workstream 1 (3.1 to 3.3) | Sanitizer smoke run is clean; Windows server survives a soak |
| **Next: Ship** | Downloadable builds | Workstream 4.2 (GitHub Actions); Workstream 2.1 and 3.4 (test movement, safe defaults) | Tagging `v0.x` publishes the Windows and Linux clients and servers |
| **Then: Playable** | Usable on all clients | Workstream 3.1 to 3.3 and 3.6 (dialogs, audio, reconnect, WebSocket bridge) | A new player can create an account and play from the web or a native client |
| **Later: Harden** | Confidence | Workstream 2.3 and 4.1 additions (bots, sanitizer jobs, post-deploy checks); Workstreams 5 and 6 | Load test and post-deploy checks pass in CI; docs complete |
| **Release: v1.0.0** | Launch | Release checklist in Workstream 6 | Tagged release deployed and published on both platforms |

## 4. Risks and open decisions

| Item | Risk / decision |
|------|-----------------|
| Windows-only crash | May have more than one cause; do the full index audit, not only the first hit. |
| Protocol change (2.2) | Breaks older clients. Decide whether to support an overlap period or force an update. |
| Web client needs a WebSocket bridge | New moving part on the production host; needs root to set up (`john` has no passwordless sudo). |
| GitHub vs GitLab | Two CI systems must stay in sync on version and tags. Decide which remote is the source of truth. |
| Audio licensing | Check licences of the wav/midi data and of the soundfont before bundling in releases. |
| Pushing `master` | Deploys production automatically. Confirm before each push. |
