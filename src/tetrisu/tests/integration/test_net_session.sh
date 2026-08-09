#!/usr/bin/env bash
# tests/integration/test_net_session.sh
#
# Drives what a tetrisu session leaves behind, against a real tetrisd, via
# tests/bin/session_smoke.
#
# Every check here is a regression rather than a feature, and none of them can
# be seen without a server: a descriptor only leaks if there was a socket to
# leak, SIGNUP only fails to bind anybody if a real server answered it, and a
# room is only stranded if a real lobby is still holding the seat.
#
# The server is lib/tetrisd_fixture.sh's.

set -uo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/lib/tetrisd_fixture.sh"

SMOKE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/tests/bin/session_smoke"
PORT=${TETRISU_TEST_PORT:-14267}

tetrisd_fixture_require_linux || exit 0

if [ ! -x "$SMOKE" ]; then
  echo "FAIL: $SMOKE was not built"
  exit 1
fi

tetrisd_fixture_start "$PORT" || exit 1
tetrisd_fixture_run_client "$SMOKE"
exit $?
