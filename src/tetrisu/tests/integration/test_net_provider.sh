#!/usr/bin/env bash
# tests/integration/test_net_provider.sh
#
# Drives tetrisu's network app data provider against a real tetrisd, via
# tests/bin/net_provider_smoke.
#
# This is the acceptance test for the net-mode wiring: it exercises the
# provider vtable the UI calls at CHECK SERVER, SIGN UP, LOGIN, Settings
# profile, Multiplayer -> Lobby and Create Room, proving the seam maps a live
# session onto the same view models the fixture provider filled, with no
# fixture standing in for the wire.
#
# The server is lib/tetrisd_fixture.sh's.

set -uo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/lib/tetrisd_fixture.sh"

SMOKE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/tests/bin/net_provider_smoke"
PORT=${TETRISU_TEST_PORT:-14261}

tetrisd_fixture_require_linux || exit 0

if [ ! -x "$SMOKE" ]; then
  echo "FAIL: $SMOKE was not built"
  exit 1
fi

tetrisd_fixture_start "$PORT" || exit 1
tetrisd_fixture_run_client "$SMOKE"
exit $?
