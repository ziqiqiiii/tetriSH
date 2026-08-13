#!/usr/bin/env bash
# tests/integration/test_net_chat.sh
#
# Drives tetrisu's room feed against a real tetrisd, via tests/bin/chat_smoke.
#
# This is the acceptance test for chat and narration being one feed served by
# the game server. What it proves that a unit test cannot is that lines arrive
# at all: narration nobody requested, a player's own line echoed back with the
# server's sequence number on it, and - the one that matters - a line that
# crossed a reply still reaching the ring. A fake socket cannot fail that last
# check, because it is net_request reading the socket that causes it.
#
# The server is lib/tetrisd_fixture.sh's.

set -uo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/lib/tetrisd_fixture.sh"

SMOKE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/tests/bin/chat_smoke"
PORT=${TETRISU_TEST_PORT:-14265}

tetrisd_fixture_require_linux || exit 0

if [ ! -x "$SMOKE" ]; then
  echo "FAIL: $SMOKE was not built"
  exit 1
fi

tetrisd_fixture_start "$PORT" || exit 1
tetrisd_fixture_run_client "$SMOKE"
exit $?
