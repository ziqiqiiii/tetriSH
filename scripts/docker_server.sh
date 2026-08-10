#!/usr/bin/env bash
# Headless tetriSH server for a container: bring the daemons up, then stay in
# the foreground for as long as they run.
#
# Runs inside the image, never on the host.
#
# Both daemons double-fork and return, which is exactly wrong for a container.
# Whatever the image runs as pid 1 finishes, pid 1 exits, and Docker tears the
# pid namespace down - taking the detached daemons with it. `CMD tetrisd` looks
# like it works and dies within the second. So this script starts them the way
# an operator would, through tetrisctl, and then blocks.
#
# Being pid 1 is also why TERM is trapped: `docker stop` signals pid 1 only, so
# without the trap the namespace is killed with neither daemon having run its
# shutdown - and stopping them in the wrong order is what pushes tetrisd's whole
# teardown into its error file instead of the log. tetrisctl owns that order.
#
# Nothing here names a port, a path or a certificate: those come from
# .tetrishrc, and tetrisd lets the environment win over it, so
# `docker run -e TETRISD_PORT=5252` needs no change on this side.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

export TETRISHRC="${TETRISHRC:-$ROOT/.tetrishrc}"
export PATH="$ROOT/bin:$PATH"

LOGD_LOG="tmp/tetrislogd/tetrislogd.log"
TETRISD_ERR="tmp/tetrisd/tetrisd.err"
TAIL_PID=""

# The port to announce. The environment wins over .tetrishrc for tetrisd, so it
# has to win here too, or the banner would advertise a port nothing is on.
announced_port() {
    if [ -n "${TETRISD_PORT:-}" ]; then
        echo "$TETRISD_PORT"
        return 0
    fi
    sed -n 's/^[[:space:]]*export[[:space:]]*TETRISD_PORT=\([0-9]\{1,\}\).*/\1/p' \
        "$TETRISHRC" 2>/dev/null | head -1
}

shutdown() {
    trap - TERM INT
    echo "==> stopping the daemons..."
    ./bin/tetrisctl stop || true
    if [ -n "$TAIL_PID" ]; then
        kill "$TAIL_PID" 2>/dev/null
    fi
    exit 0
}

# A tree built by the image needs nothing here. A tree bind-mounted over it does,
# and the binaries are the one thing this script cannot go on without.
if [ ! -x bin/tetrisd ] || [ ! -x bin/tetrislogd ] || [ ! -x bin/tetrisctl ]; then
    echo "==> building the stack..."
    if ! make all bin-link; then
        echo "docker_server.sh: the build failed; nothing to start" >&2
        exit 1
    fi
fi

# A no-op while the existing certificate is still valid, so this neither
# reissues the CA a client already trusts nor writes into a read-only mount.
# When certs/ is mounted from the host it is the host's `make certs` that fills
# it, and an empty read-only mount can only be reported.
if ! bash ./scripts/generate_certs.sh certs; then
    echo "docker_server.sh: no usable certificates in $ROOT/certs" >&2
    echo "  tetrisd treats that as a fatal boot error. If certs/ is mounted" >&2
    echo "  read-only from the host, run 'make certs' there and start again." >&2
    exit 1
fi

trap shutdown TERM INT

if ! ./bin/tetrisctl start; then
    echo "docker_server.sh: the daemons did not come up" >&2
    [ -s "$TETRISD_ERR" ] && sed 's/^/  /' "$TETRISD_ERR" >&2
    exit 1
fi
./bin/tetrisctl status

echo "==> tetrisd is listening on port $(announced_port) inside the container"

# `tail -F` rather than `-f`: both files are created by the daemons themselves
# and either may still be absent for a moment, and tetrislogd's sink can be
# rotated out from under this.
tail -F "$LOGD_LOG" "$TETRISD_ERR" 2>/dev/null &
TAIL_PID=$!
wait "$TAIL_PID"

# Only the trap is supposed to end that wait. Reaching here means the log follow
# died on its own, leaving a pid 1 with nothing left to watch and no way to
# report what the daemons do next - so stop them deliberately instead of
# lingering as a container that looks healthy and reports nothing.
shutdown
