#!/usr/bin/env bash
# A server restart keeps players in the game (tools/restart_test.py). Run it against the sanitizer build.
# Usage: ci/restart-test.sh <path-to-bc-server> <path-to-map.dat>
set -uo pipefail
TOOLS=$(cd "$(dirname "$0")/../tools" && pwd)
python3 "$TOOLS/restart_test.py" "$1" "$2" "${BC_TEST_PORT:-19820}"
