#!/usr/bin/env bash
# scripts/stress.sh
#
# Puts a fleet of players on one tetrisd and reports what it cost.
#
#   ./scripts/stress.sh                          20 players, Double, 20 s
#   ./scripts/stress.sh --players 50 --seconds 30
#   ./scripts/stress.sh --mode single --players 50 --ramp-ms 0
#   HOST=10.0.0.5 PORT=4242 ./scripts/stress.sh  an existing server, untouched
#
# A throwaway server by default: its own port, its own certificates and its own
# data directory, all removed on the way out. That is the same fixture the
# tetrisu integration suites use, and it is used here for the same reason -
# a load test that filled the developer's real store with fifty bots called
# st12345_0 would be a load test nobody runs twice.
#
# HOST= points the fleet at a server somebody else is running instead. Nothing
# is started, nothing is cleaned up, and TETRISU_CA_PATH has to name the CA
# that signed it.
#
# Deliberately not under src/tetrisu/tests/integration/: the runner's wildcard
# would pick it up, and `make test` would then take a minute per suite and fail
# on whatever else the machine happened to be doing.

set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CLIENT="$ROOT/src/tetrisu/tests/bin/stress_client"
PORT_BASE=${PORT:-24242}
HOST=${HOST:-}

# The fleet is bigger than one connection per player: the server's default cap
# is 64, which 50 players fit under only until a rerun's accounts are still
# seated. Raised rather than left to chance, because the run has to fail on the
# server's behaviour and not on a limit chosen for a laptop's default.
#
# Every one of these is a documented .tetrishrc key, and the environment wins
# over the file, so nothing here edits anything.
export TETRISD_MAX_CLIENTS=${TETRISD_MAX_CLIENTS:-256}
export TETRISD_HANDSHAKE_WORKERS=${TETRISD_HANDSHAKE_WORKERS:-8}
# Bots send at a fixed rate below the limiter's, so tripping it would mean the
# server fell behind rather than that the bots misbehaved - which is a finding
# and must not be hidden by loosening the bucket.
export TETRISD_INPUT_BURST=${TETRISD_INPUT_BURST:-120}
export TETRISD_INPUT_RATE=${TETRISD_INPUT_RATE:-60}

if [ "$(uname -s)" != "Linux" ] && [ -z "$HOST" ]; then
  echo "tetrisd needs epoll/timerfd; this is $(uname -s)."
  echo "Point the fleet at a Linux server instead: HOST=<address> $0 $*"
  exit 1
fi

echo "==> building the load generator"
if ! make -s -C "$ROOT/src/tetrisu" stress; then
  echo "FAIL: could not build $CLIENT"
  exit 1
fi

if [ -n "$HOST" ]; then
  echo "==> driving $HOST:${PORT:-4242} - no server is started here"
  TETRISU_HOST="$HOST" TETRISU_PORT="${PORT:-4242}" \
    TETRISU_CA_PATH="${TETRISU_CA_PATH:-$ROOT/certs/demo-ca.crt}" \
    "$CLIENT" "$@"
  exit $?
fi

source "$ROOT/src/tetrisu/tests/integration/lib/tetrisd_fixture.sh"

echo "==> starting a throwaway tetrisd (max_clients=$TETRISD_MAX_CLIENTS," \
  "handshake_workers=$TETRISD_HANDSHAKE_WORKERS)"
tetrisd_fixture_start "$PORT_BASE" || exit 1

tetrisd_fixture_run_client "$CLIENT" "$@"
STATUS=$?

# The server's own account of the run. With no tetrislogd beside it every
# record lands in the error file, so the whole feed is in there and printing it
# would bury the report under a thousand lines of `login` - what is worth
# reading is whatever was not INFO.
ERRORS=$(tetrisd_fixture_error_path)
if [ -s "$ERRORS" ]; then
  # Records are separated by a blank line, so the count and the filter both
  # have to drop empty lines - otherwise a clean run reports as many warnings
  # as it had records.
  TOTAL=$(grep -c '[^[:space:]]' "$ERRORS")
  LOUD=$(grep '[^[:space:]]' "$ERRORS" | grep -vc ' INFO ')
  echo "==> tetrisd logged $TOTAL records, $LOUD of them above INFO"
  if [ "$LOUD" -gt 0 ]; then
    grep '[^[:space:]]' "$ERRORS" | grep -v ' INFO ' | sed 's/^/  /' | head -40
  fi
fi

exit $STATUS
