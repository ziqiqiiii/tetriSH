#!/usr/bin/env bash
# tests/integration/lib/tetrisd_fixture.sh
#
# One throwaway tetrisd, shared by every tetrisu integration suite.
#
# Sourced, never executed. It is not named test_*.sh and does not live beside
# the suites, so the runner's wildcard does not pick it up as one — a fixture
# that reported PASS/FAIL of its own would be counting a server as a test.
#
# Three suites were carrying byte-identical copies of this bring-up. The
# copies are what a fourth suite would have inherited, so it lives here:
#
#   source "$(dirname "${BASH_SOURCE[0]}")/lib/tetrisd_fixture.sh"
#   tetrisd_fixture_require_linux || exit 0
#   tetrisd_fixture_start 42423 || exit 1
#   tetrisd_fixture_run_client "$SMOKE"
#
# tetrisd_fixture_start installs its own EXIT trap, so a suite that sources
# this must not install one of its own.

# Resolved while sourcing: inside a function BASH_SOURCE[0] is still this
# file, but the depth is easier to read from one place than from five ".."
# in the middle of a start-up sequence. lib -> integration -> tests ->
# tetrisu -> src -> repository root.
TETRISD_FIXTURE_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." \
  && pwd)
TETRISD_FIXTURE_WORK=""
TETRISD_FIXTURE_PID=""
TETRISD_FIXTURE_PORT=""

# Reports whether tetrisd can run here at all. tetrisd needs epoll and
# timerfd; everywhere else a suite reports that it was skipped rather than
# failing, because a client that cannot be exercised is not a broken one.
tetrisd_fixture_require_linux() {
  if [ "$(uname -s)" != "Linux" ]; then
    echo "PASS: skipped — tetrisd needs epoll/timerfd (this is $(uname -s))"
    return 1
  fi
  return 0
}

tetrisd_fixture_cleanup() {
  [ -n "$TETRISD_FIXTURE_PID" ] && kill -CONT "$TETRISD_FIXTURE_PID" 2>/dev/null
  [ -n "$TETRISD_FIXTURE_PID" ] && kill "$TETRISD_FIXTURE_PID" 2>/dev/null
  [ -n "$TETRISD_FIXTURE_PID" ] && wait "$TETRISD_FIXTURE_PID" 2>/dev/null
  [ -n "$TETRISD_FIXTURE_WORK" ] && rm -rf "$TETRISD_FIXTURE_WORK"
  return 0
}

# Mints development certificates into the work directory. They live and die
# with it: nothing here reaches the repository's own certs/.
tetrisd_fixture_mint_certs() {
  local dir="$TETRISD_FIXTURE_WORK/certs"

  openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
    -keyout "$dir/ca.key" -out "$dir/ca.crt" \
    -subj "/CN=tetris-test-ca" >/dev/null 2>&1
  openssl req -newkey rsa:2048 -nodes \
    -keyout "$dir/server.key" -out "$dir/server.csr" \
    -subj "/CN=localhost" >/dev/null 2>&1
  openssl x509 -req -in "$dir/server.csr" -days 1 \
    -CA "$dir/ca.crt" -CAkey "$dir/ca.key" -CAcreateserial \
    -out "$dir/server.crt" >/dev/null 2>&1
  [ -s "$dir/server.crt" ]
}

# One attempt to bring the daemon up on $1. Separated from the retry so the
# environment block is written once.
tetrisd_fixture_launch() {
  local port="$1"

  TETRISD_PORT=$port \
  TETRISD_DATA_DIR="$TETRISD_FIXTURE_WORK/data" \
  TETRISD_CONFIG_DIR="$TETRISD_FIXTURE_ROOT/lib/libmacminidb/config" \
  TETRISD_CERT_PATH="$TETRISD_FIXTURE_WORK/certs/server.crt" \
  TETRISD_KEY_PATH="$TETRISD_FIXTURE_WORK/certs/server.key" \
  TETRISD_CA_PATH="$TETRISD_FIXTURE_WORK/certs/ca.crt" \
  TETRISD_PID_PATH="$TETRISD_FIXTURE_WORK/tetrisd.pid" \
  TETRISD_ERR_PATH="$TETRISD_FIXTURE_WORK/tetrisd.err" \
  TETRISD_LOG_IPC="$TETRISD_FIXTURE_WORK/logd.sock" \
    "$TETRISD_FIXTURE_ROOT/src/tetrisd/tetrisd" >/dev/null 2>&1
}

# Boots a tetrisd at or just above $1, building it first if it is not there.
# The daemon is an integration suite's dependency, not tetrisu's: it is built
# here rather than from the tetrisu Makefile, which must not know how to build
# a daemon.
#
# The port is walked upward rather than fixed. A suite cannot ask tetrisd for
# a kernel-assigned port, because it has to tell the client where to dial —
# and a fixed one collides with whatever the machine happens to be doing:
# 42422 sat inside this kernel's ephemeral range and a browser held it as the
# source port of an unrelated connection, which failed the suite with
# "cannot listen on port 42422" and nothing to do with tetrisu.
tetrisd_fixture_start() {
  local base="$1"
  local attempt

  trap tetrisd_fixture_cleanup EXIT
  # Unconditionally, not only when the binary is absent. `make` is a no-op
  # when nothing changed, and the alternative cost an afternoon: a server-side
  # fix was invisible to this suite because a tetrisd built before it was
  # still sitting there, and the suite reported the old behaviour as fact.
  if ! make -s -C "$TETRISD_FIXTURE_ROOT/src/tetrisd" >/dev/null 2>&1; then
    echo "FAIL: could not build tetrisd"
    return 1
  fi
  TETRISD_FIXTURE_WORK=$(mktemp -d "${TMPDIR:-/tmp}/tetrisu-net.XXXXXX")
  mkdir -p "$TETRISD_FIXTURE_WORK/certs" "$TETRISD_FIXTURE_WORK/data"
  if ! tetrisd_fixture_mint_certs; then
    echo "FAIL: could not mint test certificates"
    return 1
  fi
  cd "$TETRISD_FIXTURE_ROOT" || return 1
  attempt=0
  while [ "$attempt" -lt 16 ]; do
    if tetrisd_fixture_launch $((base + attempt)); then
      # tetrisd daemonises and exits 0 only once it is listening, so the pid
      # to stop is the one it wrote, not the one the shell forked.
      TETRISD_FIXTURE_PID=$(cat "$TETRISD_FIXTURE_WORK/tetrisd.pid" \
        2>/dev/null | tr -dc '0-9')
      TETRISD_FIXTURE_PORT=$((base + attempt))
      return 0
    fi
    attempt=$((attempt + 1))
  done
  echo "FAIL: tetrisd did not start on any port from $base"
  [ -s "$TETRISD_FIXTURE_WORK/tetrisd.err" ] \
    && sed 's/^/  /' "$TETRISD_FIXTURE_WORK/tetrisd.err"
  return 1
}

# Runs one client binary pointed at the fixture's server. Extra arguments are
# passed through to it.
tetrisd_fixture_run_client() {
  local client="$1"
  shift
  TETRISU_HOST=127.0.0.1 \
  TETRISU_PORT=$TETRISD_FIXTURE_PORT \
  TETRISU_CA_PATH="$TETRISD_FIXTURE_WORK/certs/ca.crt" \
  TETRISD_TEST_PID=$TETRISD_FIXTURE_PID \
    "$client" "$@"
}

# The daemon's error file, for a suite that wants to quote it on failure.
tetrisd_fixture_error_path() {
  echo "$TETRISD_FIXTURE_WORK/tetrisd.err"
}
