#!/usr/bin/env bash
# tests/integration/test_net_arena.sh
#
# Drives four of tetrisu's clients through one Battle Royale against a real
# tetrisd, and asserts what the arena has to be true of: a complete roster,
# cards filed by seat, the room's own head count, and a frame carrying no
# arena leaving the cards where they were.
#
# The server is lib/tetrisd_fixture.sh's; what belongs here is which client to
# run against it.

set -uo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/lib/tetrisd_fixture.sh"

SMOKE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/tests/bin/arena_smoke"
PORT=${TETRISU_TEST_PORT:-14249}

tetrisd_fixture_require_linux || exit 0

if [ ! -x "$SMOKE" ]; then
  echo "FAIL: $SMOKE was not built"
  exit 1
fi

tetrisd_fixture_start "$PORT" || exit 1
tetrisd_fixture_run_client "$SMOKE"
exit $?
