#!/usr/bin/env bash
# tests/integration/test_net_provider.sh
#
# Drives tetrisu's network app data provider against a real tetrisd: a
# throwaway server on a kernel-assigned port, throwaway certificates and a
# throwaway data directory, talked to over a real socket by
# tests/bin/net_provider_smoke.
#
# This is the acceptance test for the net-mode wiring (Blocks 1-4): it boots
# tetrisd, then exercises the provider vtable the UI calls at CHECK SERVER,
# SIGN UP, LOGIN, Settings profile, Multiplayer -> Lobby and Create Room -
# proving the new seam maps a live session onto the same view models the
# fixture provider filled, with no fixture standing in for the wire.
#
# tetrisd needs epoll and timerfd, so this is a Linux suite. Everywhere else it
# reports that it was skipped rather than failing.

set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)
SMOKE="$ROOT/src/tetrisu/tests/bin/net_provider_smoke"
DAEMON="$ROOT/src/tetrisd/tetrisd"
PORT=${TETRISU_TEST_PORT:-42422}
WORK=""
PID=""

cleanup() {
  [ -n "$PID" ] && kill "$PID" 2>/dev/null
  [ -n "$PID" ] && wait "$PID" 2>/dev/null
  [ -n "$WORK" ] && rm -rf "$WORK"
  return 0
}
trap cleanup EXIT

if [ "$(uname -s)" != "Linux" ]; then
  echo "PASS: skipped — tetrisd needs epoll/timerfd (this is $(uname -s))"
  exit 0
fi

if [ ! -x "$SMOKE" ]; then
  echo "FAIL: $SMOKE was not built"
  exit 1
fi

# The daemon is this suite's dependency, not tetrisu's: build it here rather
# than from the tetrisu Makefile, which must not know how to build a daemon.
if [ ! -x "$DAEMON" ]; then
  if ! make -s -C "$ROOT/src/tetrisd" >/dev/null 2>&1; then
    echo "FAIL: could not build tetrisd"
    exit 1
  fi
fi

WORK=$(mktemp -d "${TMPDIR:-/tmp}/tetrisu-prov.XXXXXX")
mkdir -p "$WORK/certs" "$WORK/data"

# Development certificates, minted here and thrown away with the directory.
openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
  -keyout "$WORK/certs/ca.key" -out "$WORK/certs/ca.crt" \
  -subj "/CN=tetris-test-ca" >/dev/null 2>&1
openssl req -newkey rsa:2048 -nodes \
  -keyout "$WORK/certs/server.key" -out "$WORK/certs/server.csr" \
  -subj "/CN=localhost" >/dev/null 2>&1
openssl x509 -req -in "$WORK/certs/server.csr" -days 1 \
  -CA "$WORK/certs/ca.crt" -CAkey "$WORK/certs/ca.key" -CAcreateserial \
  -out "$WORK/certs/server.crt" >/dev/null 2>&1
if [ ! -s "$WORK/certs/server.crt" ]; then
  echo "FAIL: could not mint test certificates"
  exit 1
fi

cd "$ROOT" || exit 1
TETRISD_PORT=$PORT \
TETRISD_DATA_DIR="$WORK/data" \
TETRISD_CONFIG_DIR="$ROOT/lib/libmacminidb/config" \
TETRISD_CERT_PATH="$WORK/certs/server.crt" \
TETRISD_KEY_PATH="$WORK/certs/server.key" \
TETRISD_CA_PATH="$WORK/certs/ca.crt" \
TETRISD_PID_PATH="$WORK/tetrisd.pid" \
TETRISD_ERR_PATH="$WORK/tetrisd.err" \
TETRISD_LOG_IPC="$WORK/logd.sock" \
  "$DAEMON" >/dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "FAIL: tetrisd did not start"
  [ -s "$WORK/tetrisd.err" ] && sed 's/^/  /' "$WORK/tetrisd.err"
  exit 1
fi
# tetrisd daemonises and exits 0 only once it is listening, so the pid to stop
# is the one it wrote, not the one the shell forked.
PID=$(cat "$WORK/tetrisd.pid" 2>/dev/null | tr -dc '0-9')

TETRISU_HOST=127.0.0.1 \
TETRISU_PORT=$PORT \
TETRISU_CA_PATH="$WORK/certs/ca.crt" \
  "$SMOKE"
RC=$?

exit $RC