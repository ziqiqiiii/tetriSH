#!/usr/bin/env bash
# tests/integration/test_logrotate.sh
#
# Verifies the logrotate rule scripts/logrotate.sh generates for the sink.
#
# The property worth guarding is the handover: logrotate renames the file the
# daemon is holding, and the daemon only lets go of that inode when SIGHUP
# reaches it (sink_reopen, src/tetrislogd/src/sink.c). A rule that rotates
# without signalling leaves the logger appending to an unlinked inode - the
# disk fills with a file nobody can read and the sink never comes back. So the
# test drives a real logrotate against real files and asserts the signal was
# delivered, rather than grepping the generated text for a postrotate line.
#
# copytruncate is the other half of the same property, and is asserted absent:
# it would truncate the sink under an O_APPEND writer, which is the one shape
# that loses records already written.

set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)
GEN="$ROOT/scripts/logrotate.sh"
FAIL=0
WORK=""
SIGNALLEE=""

cleanup() {
  [ -n "$SIGNALLEE" ] && kill "$SIGNALLEE" 2>/dev/null
  [ -n "$WORK" ] && rm -rf "$WORK"
  return 0
}
trap cleanup EXIT

if ! command -v logrotate > /dev/null 2>&1; then
  echo "SKIP: logrotate is not installed"
  exit 0
fi

WORK=$(mktemp -d "${TMPDIR:-/tmp}/tetrislogd-rotate.XXXXXX")
mkdir -p "$WORK/tmp/tetrislogd" "$WORK/tmp/tetrisd"

SINK=$WORK/tmp/tetrislogd/tetrislogd.log
PIDFILE=$WORK/tmp/tetrislogd/tetrislogd.pid
CONF=$WORK/tetrish.logrotate
STATE=$WORK/logrotate.state
HUPPED=$WORK/hup-arrived

# A fixture rc rather than the repository's own: the generated rule names
# absolute paths, and a test that pointed logrotate at tmp/tetrislogd/ would
# rotate the log of whatever daemon the developer had running.
cat > "$WORK/.tetrishrc" <<'RC'
export TETRISLOGD_LOG_PATH=tmp/tetrislogd/tetrislogd.log
export TETRISLOGD_PID_PATH=tmp/tetrislogd/tetrislogd.pid
export TETRISLOGD_ERR_PATH=tmp/tetrislogd/tetrislogd.err
export TETRISD_ERR_PATH=tmp/tetrisd/tetrisd.err
RC

# --no-su because logrotate refuses the su directive when it is not root, and
# this suite is not. The installed rule keeps it: there the directory is owned
# by the player and logrotate is root, which is exactly what su exists for.
if ! "$GEN" --rc "$WORK/.tetrishrc" --root "$WORK" --no-su > "$CONF" 2> "$WORK/gen.err"; then
  echo "FAIL: scripts/logrotate.sh exited non-zero"
  sed 's/^/  /' "$WORK/gen.err"
  exit 1
fi

# 1. The rule has to name the sink by absolute path. logrotate has no notion of
#    the repository root, so a root-relative path out of .tetrishrc silently
#    matches nothing and the log is never rotated at all.
if ! grep -qF "$SINK" "$CONF"; then
  echo "FAIL: the generated rule does not name the sink $SINK"
  FAIL=1
fi

# 2. copytruncate against an O_APPEND sink loses whatever was written between
#    the copy and the truncate. Scoped to the sink's own stanza: the error
#    files are stderr with no reopen signal, and copytruncate is correct there.
sink_stanza() {
  awk -v sink="$SINK" '$1 == sink {inside = 1} inside {print} inside && /^}/ {exit}' "$CONF"
}

if [ -z "$(sink_stanza)" ]; then
  echo "FAIL: the generated rule has no stanza for the sink"
  FAIL=1
elif sink_stanza | grep -q '^[[:space:]]*copytruncate'; then
  echo "FAIL: the sink stanza uses copytruncate"
  FAIL=1
fi

# The handover lives in that stanza too - an error file's postrotate would
# satisfy a whole-file grep while the sink rotated unsignalled.
if ! sink_stanza | grep -qF -- "-HUP"; then
  echo "FAIL: the sink stanza does not signal the daemon"
  FAIL=1
fi

# 3. `create` hands the fresh sink to whoever it names, and tetrislogd opens it
#    O_APPEND as the player. Naming root there costs the log everything: the
#    daemon cannot write the file it was just given, degrades to stderr, and
#    stays degraded. The fixture directory is owned by whoever runs the suite,
#    so that is who the rule has to name.
WANT_OWNER=$(id -un)
if ! sink_stanza | grep -qE "^[[:space:]]*create 0644 $WANT_OWNER "; then
  echo "FAIL: the sink stanza does not create the log as $WANT_OWNER"
  sink_stanza | grep -E '^[[:space:]]*create' | sed 's/^/  got: /'
  FAIL=1
fi

# 4. Syntax, before the run that depends on it - a config logrotate rejects
#    fails every assertion below for the same uninformative reason.
if ! logrotate --debug --state "$STATE" "$CONF" > "$WORK/debug.out" 2>&1; then
  echo "FAIL: logrotate rejected the generated config"
  sed 's/^/  /' "$WORK/debug.out"
  exit 1
fi

# 5. The handover itself. The stand-in holds no file - what is being tested is
#    that logrotate's postrotate reaches the pid in the pidfile, and tetrislogd
#    reopening on that signal is test_sink's job, not this one.
head -c 4096 /dev/urandom | base64 > "$SINK"
bash -c 'trap "touch \"$1\"; exit 0" HUP; while :; do sleep 0.2; done' _ "$HUPPED" &
SIGNALLEE=$!
echo "$SIGNALLEE" > "$PIDFILE"

if ! logrotate --force --state "$STATE" "$CONF" > "$WORK/run.out" 2>&1; then
  echo "FAIL: logrotate --force exited non-zero"
  sed 's/^/  /' "$WORK/run.out"
  FAIL=1
fi

if [ ! -e "$SINK.1" ] && [ ! -e "$SINK.1.gz" ]; then
  echo "FAIL: the sink was not rotated - no $SINK.1"
  FAIL=1
fi

if [ ! -f "$SINK" ]; then
  echo "FAIL: no fresh sink was created for the daemon to reopen"
  FAIL=1
fi

# The signal is asynchronous: postrotate returns once kill(1) has delivered it,
# but the shell being signalled still has to run its trap.
for _ in 1 2 3 4 5 6 7 8 9 10; do
  [ -f "$HUPPED" ] && break
  sleep 0.2
done
if [ ! -f "$HUPPED" ]; then
  echo "FAIL: postrotate did not deliver SIGHUP to the pid in $PIDFILE"
  FAIL=1
fi

# 6. A stopped daemon is the ordinary case at 3am, not an error: the pidfile is
#    stale or gone, and rotation still has to happen and still has to succeed.
#    A postrotate that exits non-zero here makes logrotate report failure for
#    every run once the daemon is down.
kill "$SIGNALLEE" 2>/dev/null
wait "$SIGNALLEE" 2>/dev/null
SIGNALLEE=""
rm -f "$PIDFILE"
head -c 4096 /dev/urandom | base64 > "$SINK"

if ! logrotate --force --state "$STATE" "$CONF" > "$WORK/down.out" 2>&1; then
  echo "FAIL: rotation failed when the daemon was not running"
  sed 's/^/  /' "$WORK/down.out"
  FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
  echo "PASS: the sink rotates and the daemon is signalled to reopen it"
fi
exit $FAIL
