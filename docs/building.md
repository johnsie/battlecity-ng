# Building Battle City NG

One top-level `CMakeLists.txt` builds the server and the client. `-DBC_BUILD_SERVER=OFF` or `-DBC_BUILD_CLIENT=OFF` build only one.

## Linux

    sudo apt install build-essential cmake libsqlite3-dev libsdl2-dev
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j"$(nproc)"
    ctest --test-dir build --output-on-failure

The server needs SQLite (`libsqlite3-dev`), the client needs SDL2. Dear ImGui, TinySoundFont and the other libraries are in `external/`. Binaries: `build/server/bc-server`, `build/client/bc-client`.

Run the server from a directory holding `map.dat`: `cd somewhere && cp /path/to/data/map.dat . && BC_PORT=9680 /path/to/bc-server`.
Run the client with `BC_DATA_DIR=data` (the repository's `data/`). **Set `BC_SERVER=127.0.0.1` for test runs**: the shipped `options.ini` points at the live server.

## Windows

The Windows builds use MSYS2 (UCRT64) for the client and MSVC or MSYS2 for the server; the GitHub workflows in `.github/workflows/` are the reference.

    pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,SDL2}
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build

`ci/package-client.sh` and `ci/package-server.sh` bundle a build with its data and DLLs.

## Web (Emscripten)

    ci/build-web.sh build-web dist-web

Installs the Emscripten SDK into `~/.cache/bc-emsdk` on first use (several GB, a few minutes) and writes the site (`index.html`, `.js`, `.wasm`, `.data`) to `dist-web/`. The page template is `web/shell.html`. Note the site's Content-Security-Policy forbids `eval()`: never use `emscripten_run_script`; use `EM_ASM` instead.

## Tests

| Script | What it checks |
|--------|----------------|
| `ci/smoke-test.sh <bc-server> <map.dat>` | the server accepts connections and shuts down cleanly |
| `ci/smoke-test-client.sh <bc-client> data` | the client starts, loads every asset and renders a frame headless |
| `ci/fuzz-test.sh <bc-server> <map.dat>` | malformed packets never crash the server |
| `ci/relay-test.sh <bc-server> <map.dat>` | one bot's movement reaches another steadily; impossible jumps are refused |
| `ci/load-test.sh <bc-server> <map.dat> [bots] [seconds]` | many bots join, drive, and agree on each other's positions |

Run the server tests against a build made with `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize=alignment -fno-omit-frame-pointer"` to catch memory errors; CI does.

## Test hooks (client)

Environment variables: `BC_DATA_DIR`, `BC_SERVER`, `BC_SCREENSHOT`, `BC_SCREENSHOT_FRAMES`, `BC_TEST_SCRIPT`, `BC_WINDOW_SIZE`, `BC_TEST_DIALOG`. See `docs/TECHNICAL.md`.
