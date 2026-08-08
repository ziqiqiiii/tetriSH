#!/usr/bin/env bash
# tests/integration/test_net_solo.sh
#
# Drives tetrisu's network layer against a real tetrisd: a throwaway server on
# a fixed test port, throwaway certificates and a throwaway data directory,
# talked to over a real socket by tests/bin/net_smoke.
#
# The server is lib/tetrisd_fixture.sh's; what belongs here is which client to
# run against it.

set -uo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/lib/tetrisd_fixture.sh"

SMOKE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/tests/bin/net_smoke"
PORT=${TETRISU_TEST_PORT:-14241}

tetrisd_fixture_require_linux || exit 0

if [ ! -x "$SMOKE" ]; then
  echo "FAIL: $SMOKE was not built"
  exit 1
fi

tetrisd_fixture_start "$PORT" || exit 1
tetrisd_fixture_run_client "$SMOKE"
exit $?
