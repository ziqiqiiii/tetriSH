#!/usr/bin/env bash
# tests/integration/test_system_daemons.sh
#
# Verifies dspawn's three launch forms: the built-in work loop, an exec'd
# target, and a target that does not exist. The first is the one worth
# guarding — a daemon that dies on the way to its work loop still registers
# and still writes its .err banner, so every artifact dspawn leaves behind
# looks healthy and only the missing process says otherwise.

set -euo pipefail

DSPAWN=./bin/dspawn
REG=./tmp/daemons.reg
SPAWN_LOG=./tmp/dspawn.log
FAIL=0
NAMES=()

# Every daemon this suite starts outlives it: the work loop runs forever and
# the exec'd target sleeps. Killing them is not optional, and neither is
# dropping their registry lines — dcheck reports whatever is left behind.
cleanup() {
  local name pid
  for name in "${NAMES[@]:-}"; do
    [ -z "$name" ] && continue
    while read -r pid; do
      [ -n "$pid" ] && kill "$pid" 2>/dev/null || true
    done < <(awk -v n="$name" '$1 == n || index($1, n ".") == 1 {print $2}' "$REG" 2>/dev/null || true)
    if [ -f "$REG" ]; then
      grep -v "^${name}[ .]" "$REG" > "${REG}.tmp" 2>/dev/null || true
      mv "${REG}.tmp" "$REG" 2>/dev/null || true
    fi
    rm -f "./tmp/${name}.err" "./tmp/${name}."*.err 2>/dev/null || true
  done
}
trap cleanup EXIT

# Read the pid dspawn registered for a name, or nothing when it never got there.
registered_pid() {
  awk -v n="$1" '$1 == n {print $2}' "$REG" 2>/dev/null | tail -1 || true
}

# 1. A bare name runs the built-in work loop: no "--", so no target program.
#    The daemon must reach that loop, not just the registry.
NAME=tdd_daemon_loop
NAMES+=("$NAME")
timeout 5s $DSPAWN "$NAME" >/dev/null 2>&1 || true
sleep 1
PID=$(registered_pid "$NAME")
if [ -z "$PID" ]; then
  echo "FAIL: dspawn $NAME did not register the daemon"
  FAIL=1
elif ! kill -0 "$PID" 2>/dev/null; then
  echo "FAIL: dspawn $NAME registered pid $PID but the daemon is not running"
  FAIL=1
fi

# 2. Reaching the work loop is what the spawn log records, so an entry there
#    is the difference between a daemon that started and one that only
#    registered before dying.
if ! grep -F "Started dspawn daemon $NAME" "$SPAWN_LOG" > /dev/null 2>&1; then
  echo "FAIL: dspawn $NAME left no start line in $SPAWN_LOG"
  FAIL=1
fi

# 3. Everything after "--" is a program to exec, and the daemon becomes it.
NAME=tdd_daemon_exec
NAMES+=("$NAME")
timeout 5s $DSPAWN "$NAME" -- sleep 30 >/dev/null 2>&1 || true
sleep 1
PID=$(registered_pid "$NAME")
if [ -z "$PID" ]; then
  echo "FAIL: dspawn $NAME -- sleep 30 did not register the daemon"
  FAIL=1
elif ! ps -p "$PID" -o args= 2>/dev/null | grep -F "sleep 30" > /dev/null; then
  echo "FAIL: dspawn $NAME -- sleep 30 did not exec the target"
  echo "  registered pid $PID is: $(ps -p "$PID" -o args= 2>/dev/null || echo gone)"
  FAIL=1
fi

# 4. A target that cannot be executed is refused before anything daemonises,
#    so the registry stays clean rather than gaining an entry that dcheck
#    would report as "down".
NAME=tdd_daemon_missing
NAMES+=("$NAME")
RC=0
timeout 5s $DSPAWN "$NAME" -- zzz_no_such_program_xyz >/dev/null 2>&1 || RC=$?
if [ "$RC" -ne 127 ]; then
  echo "FAIL: dspawn with a missing target should exit 127, got $RC"
  FAIL=1
fi
if [ -n "$(registered_pid "$NAME")" ]; then
  echo "FAIL: dspawn with a missing target should not register a daemon"
  FAIL=1
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
echo "PASS: dspawn runs its work loop, execs a target, and refuses a missing one"
