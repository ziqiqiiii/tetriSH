#!/usr/bin/env bash
# tests/integration/test_net_store.sh
#
# Drives tetrisu's Marketplace and Settings against a real tetrisd, via
# tests/bin/store_smoke.
#
# This is the acceptance test for making those two screens server-authoritative:
# the catalogue and its prices come from the server's config, the wallet is the
# store's, and buying and equipping are LIST /store, BUY and EQUIP rather than
# edits to a view model that vanished with the screen. What it proves that a
# unit test cannot is persistence - an equip is still there when the account is
# read back.
#
# The server is lib/tetrisd_fixture.sh's.

set -uo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/lib/tetrisd_fixture.sh"

SMOKE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/tests/bin/store_smoke"
PORT=${TETRISU_TEST_PORT:-14263}

tetrisd_fixture_require_linux || exit 0

if [ ! -x "$SMOKE" ]; then
  echo "FAIL: $SMOKE was not built"
  exit 1
fi

tetrisd_fixture_start "$PORT" || exit 1
tetrisd_fixture_run_client "$SMOKE"
exit $?
