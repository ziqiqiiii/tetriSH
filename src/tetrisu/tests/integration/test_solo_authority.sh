#!/usr/bin/env bash
# tests/integration/test_solo_authority.sh
#
# Drives the layer solo_mode.c calls — solo_authority.c — against a real
# tetrisd, via tests/bin/solo_authority_smoke.
#
# net_solo covers the wire. This covers the seam above it, where the client's
# 3-2-1 meets the server's gravity: the hold that stops tetrisd's clock for
# the length of the countdown, the release that hands the board back, and the
# fall back to the local rules when the session goes away mid-game.
#
# The server is lib/tetrisd_fixture.sh's.

set -uo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/lib/tetrisd_fixture.sh"

SMOKE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/tests/bin/solo_authority_smoke"
PORT=${TETRISU_TEST_PORT:-14281}

tetrisd_fixture_require_linux || exit 0

if [ ! -x "$SMOKE" ]; then
  echo "FAIL: $SMOKE was not built"
  exit 1
fi

tetrisd_fixture_start "$PORT" || exit 1
tetrisd_fixture_run_client "$SMOKE"
exit $?
