#!/usr/bin/env bash
# tests/integration/test_bots.sh
#
# One person and three bots in a Battle Royale, against a real tetrisd.
#
# The mode's minimum is four and a player with two laptops has two, so this is
# the suite that says the feature works at all: bots fill the room from the
# server's own account pool without the player naming an account, the match
# deals, the bots play boards that visibly change, and letting them go empties
# the room again.
#
# The server is lib/tetrisd_fixture.sh's; what belongs here is which client to
# run against it, and where the bots' own output goes - which is never the
# terminal, because in a real client that terminal belongs to notcurses.

set -uo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/lib/tetrisd_fixture.sh"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SMOKE="$ROOT/tests/bin/bots_smoke"
BOT="$ROOT/bin/tetrisu-bot"
PORT=${TETRISU_TEST_PORT:-14251}

tetrisd_fixture_require_linux || exit 0

for binary in "$SMOKE" "$BOT"; do
  if [ ! -x "$binary" ]; then
    echo "FAIL: $binary was not built"
    exit 1
  fi
done

BOT_LOG=$(mktemp "${TMPDIR:-/tmp}/tetrisu-bot.XXXXXX.log")
trap 'rm -f "$BOT_LOG"' EXIT

tetrisd_fixture_start "$PORT" || exit 1

# TETRISU_BOT_BIN is what a build tree needs and an installed layout does not:
# bot_proc.c resolves the binary from the directory of the running client, and
# here the running client is tests/bin/bots_smoke rather than bin/tetrisu.
TETRISU_BOT_BIN="$BOT" TETRISU_BOT_LOG="$BOT_LOG" \
  tetrisd_fixture_run_client "$SMOKE"
status=$?

if [ "$status" -ne 0 ] && [ -s "$BOT_LOG" ]; then
  echo "--- the bots' own output ---"
  sed 's/^/  /' "$BOT_LOG"
fi

exit $status
