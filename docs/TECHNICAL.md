# Battle City NG: Technical Overview

For the plan and status of the work, see the [roadmap](../ROADMAP.md). For server hosting, see [deploy/README.md](../deploy/README.md).

## 1. What is in the repository

| Path | Contents |
|------|----------|
| `server/` | The game server (C++14). One process, one UDP-free TCP listener (default port 9680, `BC_PORT` overrides). |
| `client/` | The game client (C++14, SDL2, Dear ImGui). `client/platform/` holds the SDL2 layer that replaced the old Win32/DirectX/FMOD code. |
| `data/` | Game assets: bitmaps, sounds, MIDI, fonts, the map (`map.dat`), default `options.ini`. |
| `web/`, `deploy/web/` | Browser page shell, Apache virtual host and the WebSocket-to-TCP gateway service. |
| `deploy/` | Linux server install: systemd unit, setup script, sudoers rule. |
| `ci/` | Build, test, package and deploy scripts used by both CI systems. |
| `external/` | Vendored libraries: Dear ImGui, SQLite amalgamation, TinySoundFont, legacy DirectX/FMOD/pthread headers. |
| `tools/` | Asset and dialog generators (`rc2dialogs.py` converts the old resource dialogs to ImGui). |

One top-level `CMakeLists.txt` builds both programs: `-DBC_BUILD_SERVER=OFF` or `-DBC_BUILD_CLIENT=OFF` select one.

## 2. Architecture

The server is authoritative. Clients send input and requests; the server validates them, updates the world and tells the relevant clients.

```
 Native client (Linux/Windows) ──TCP 9680──────────────┐
                                                       ▼
 Web client ──wss──> Apache ──> WebSocket gateway ──> bc-server ── SQLite (accounts, stats)
 (Emscripten build of the same client)                    └── map.dat, logs, news.txt
```

### Server (`server/`)
- `CServer`: owns everything; fixed tables `Player[64]` and `City[64]`; main loop and tick.
- `CSocket`: connections and per-socket outgoing queues (`COutQueue`); the receive path splits the byte stream into packets.
- `CProcess`: one handler per client packet type (`cm*`).
- `CSend`: helpers that send to a player, a city, everyone, or everyone in the sectors around a point.
- `CPlayer`, `CCity`, `CBuilding`, `CItem`, `CBullet`, `CMap`, `CCollision`: the game world. The map is 512×512 tiles of 48 units; cities are created at the command-centre tiles in `map.dat`.
- `CAccount`, `CDatabase`, `CSQLite3`: accounts and statistics in SQLite.
- `WinCrash.cpp`: Windows crash handler that prints a symbolised stack trace (needs the `.pdb`).

### Client (`client/`)
- `CGame`, `CInGame`, `CDrawing`, `CEngine`: game loop, in-game state, rendering.
- `CWinsock`, `CProcess`: network connection and handlers for server packets (`sm*`).
- `CPlayer`: local simulation of every tank (dead reckoning between server updates).
- `CSound`: SDL mixer for effects, TinySoundFont for MIDI music.
- `platform/`: window, input, networking and a Win32-dialog emulation layer drawn with ImGui.
- Test hooks (environment variables): `BC_DATA_DIR`, `BC_SERVER` (override server address), `BC_SCREENSHOT`, `BC_SCREENSHOT_FRAMES`, `BC_TEST_SCRIPT`, `BC_WINDOW_SIZE`.

## 3. Network protocol

- Plain TCP. Each packet starts with a type byte, followed by a fixed-size struct. The enums are in `NetMessages.h` (`ServerMessages` for `sm*`, client messages for `cm*`); the file is duplicated in `server/` and `client/` and must stay identical.
- Wire structs use fixed-width types (`uint32_t`, `unsigned short`) so that every platform agrees. Both programs are built with `-fsigned-char` because the packet checksum sums `char` values.
- Positions: a client sends `cmUpdate` (x, y, turn, move, direction) every 150 ms and immediately when its movement or turning input changes. The server checks the move against the player's last position and relays `smUpdate` to nearby clients. Receivers keep moving the tank using its direction and speed, then blend towards each new authoritative position (snapping if the error is large).
- The world is divided into sectors; a client asks for the sectors around it and the server sends their buildings and items.

## 4. Building

Requirements: CMake 3.16+, a C++14 compiler. Linux server: `libsqlite3-dev`. Linux client: `libsdl2-dev`. On Windows the server uses MSVC with the vendored SQLite; the client is built with MSYS2/MinGW and SDL2.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Web client: `ci/build-web.sh build-web dist-web` (installs Emscripten on first use).

Tests: `ci/smoke-test.sh` (server accepts connections and shuts down cleanly), `ci/smoke-test-client.sh` (client starts, loads all assets, renders a frame headless), `test-outqueue` (unit test for the send queue). The server is also built and smoke-tested with AddressSanitizer and UBSan.

## 5. Running

- Server: run `bc-server` in a directory containing `map.dat`. State is written to `dat/BCData.db`, `logs/` and `news.txt` in the working directory.
- Client: run `bc-client`; it looks for a `data/` folder in the working directory or next to the executable (or `BC_DATA_DIR`). The server address comes from `options.ini` (`[GameServer] Address`, default `bc.outiva.com`, the live server) or `BC_SERVER`.

## 6. Build and release pipelines

| System | Does |
|--------|------|
| GitLab CI (`.gitlab-ci.yml`) | Builds and tests the Linux server (including a sanitizer build), packages it, and deploys it automatically to the production host on pushes to `master` and on tags. Builds and deploys the web client. |
| GitHub Actions (`.github/workflows/`) | `build.yml`: builds and tests Linux and Windows clients and servers on each push and pull request. `release.yml`: on a `v*` tag, builds, packages and publishes both clients and both servers, with checksums, as a GitHub Release. |

Packages are produced by `ci/package-server.sh` and `ci/package-client.sh`.

## 7. Known limitations

- The server keeps all game state in fixed arrays and trusts packet indices only after explicit checks; new handlers must validate every index and length (the Windows crash came from an unchecked `City[-1]`).
- Windows client builds from CMake are new and may need fixes.
- No automated gameplay or protocol test yet (planned: scripted bot clients).
