#!/usr/bin/env bash
# One command from a fresh clone to a playable client.
#
#   bash scripts/play.sh            set everything up, then play
#   bash scripts/play.sh --help     every flag
#
# The whole reason this script exists is that tetriSH cannot be run from one
# place on macOS. tetrisd's reactor is epoll and timerfd and libcoreipc's
# message queues are POSIX mqueue, none of which Darwin has, so the server does
# not compile there at all - while tetrisu wants the host's own terminal,
# because the board is drawn with Kitty-protocol bitmaps that a container has no
# way to hand to a Mac. So the two halves live in different places, and this
# walks the path between them: engine, certificates, image, server, client.
#
# On Linux both halves are native and there is no container in the picture; the
# script runs the same five steps against `make stack` instead.
#
# It is deliberately re-runnable. Every step checks before it acts - an image
# that exists is not rebuilt, a valid certificate is not reissued, a server
# already listening is left alone - so the second run is fast and the tenth is
# the normal way to restart the client.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

BOLD=$(printf '\033[1m'); RED=$(printf '\033[1;31m')
GRN=$(printf '\033[1;32m'); YEL=$(printf '\033[1;33m')
BLU=$(printf '\033[1;34m'); RST=$(printf '\033[0m')

UNAME_S="$(uname -s)"

# The image tag and container name are the Makefile's to choose. It passes them
# in from the `play` target, so overriding DOCKER_TAG there does not leave this
# script inspecting an image nobody built; the defaults are for a direct run.
IMAGE="${DOCKER_REF:-tetrish:dev}"
SERVER="${DOCKER_SERVER:-tetrish-server}"

ASSUME_YES=0
DO_REBUILD=0
WANT_SERVER=1
WANT_CLIENT=1
DO_STOP=0
PORT=""
# Where the server is. Empty means here, which is the only case with a server to
# start; --host names somebody else's, and then this script has a client to
# launch and nothing to bring up.
HOST=""
LOCAL_HOST="127.0.0.1"

say()  { printf '%b\n' "${BLU}==>${RST} ${BOLD}$*${RST}"; }
ok()   { printf '%b\n' "    ${GRN}$*${RST}"; }
warn() { printf '%b\n' "    ${YEL}$*${RST}" >&2; }
die()  { printf '%b\n' "${RED}play.sh:${RST} $*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Usage: bash scripts/play.sh [options]

  --host ADDR     play on somebody else's server (e.g. --host 10.27.229.33);
                  nothing is started locally and the demo CA is used
  --port N        port to serve and connect on (default: TETRISD_PORT in .tetrishrc)
  --server-only   bring the server up and stop, without launching a client
  --client-only   launch a client against a server that is already up
  --rebuild       rebuild the container image even if one exists
  --stop          stop the server and exit
  -y, --yes       install anything missing without asking
  -h, --help      this text

With no --host a server is started here and the client connects to it; the
server is left running when the client exits, so the next run starts a client
immediately. `bash scripts/play.sh --stop` takes it down.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --host)        HOST="${2:-}"; shift 2 || die "--host needs an address" ;;
        --host=*)      HOST="${1#*=}"; shift ;;
        --port)        PORT="${2:-}"; shift 2 || die "--port needs a number" ;;
        --port=*)      PORT="${1#*=}"; shift ;;
        --server-only) WANT_CLIENT=0; shift ;;
        --client-only) WANT_SERVER=0; shift ;;
        --rebuild)     DO_REBUILD=1; shift ;;
        --stop)        DO_STOP=1; shift ;;
        -y|--yes)      ASSUME_YES=1; shift ;;
        -h|--help)     usage; exit 0 ;;
        *)             usage >&2; die "unknown option: $1" ;;
    esac
done

if [ "$WANT_SERVER" = "0" ] && [ "$WANT_CLIENT" = "0" ]; then
    die "--server-only and --client-only ask for opposite halves; pick one"
fi

# A named host is somebody else's machine, so there is nothing here to start,
# stop or build an image for. Rather than quietly ignoring the flags that say
# otherwise, refuse them: --server-only with a remote host asks this script to
# start a server it has no reach into, and would otherwise appear to succeed.
if [ -n "$HOST" ]; then
    [ "$WANT_CLIENT" = "0" ] \
        && die "--host names a server elsewhere; --server-only cannot start one there"
    [ "$DO_STOP" = "1" ] \
        && die "--host names a server elsewhere; --stop only reaches the local one"
    [ "$DO_REBUILD" = "1" ] \
        && die "--host needs no image; --rebuild only applies to a server started here"
    WANT_SERVER=0
    REMOTE=1
else
    HOST="$LOCAL_HOST"
    REMOTE=0
fi

# Asks, unless --yes was given or nothing is attached to answer. A script that
# assumed consent when it could not ask would install a virtual machine inside
# somebody's CI run.
confirm() {
    [ "$ASSUME_YES" = "1" ] && return 0
    if [ ! -t 0 ]; then
        warn "not a terminal, so nothing will be installed; re-run with --yes"
        return 1
    fi
    printf '%b' "    ${BOLD}$1${RST} [y/N] "
    read -r reply
    case "$reply" in [yY]|[yY][eE][sS]) return 0 ;; *) return 1 ;; esac
}

# The port is one number shared by four places - the server's listener, the
# published container port, the client's dial and .tetrishrc - so it is read
# from .tetrishrc once and passed everywhere from here.
resolve_port() {
    [ -n "$PORT" ] && return 0
    PORT=$(sed -n \
        's/^[[:space:]]*export[[:space:]]*TETRISD_PORT=\([0-9]\{1,\}\).*/\1/p' \
        .tetrishrc 2>/dev/null | head -1)
    [ -z "$PORT" ] && PORT=4242
    return 0
}

# Is anything accepting connections there? nc where it exists, and bash's own
# /dev/tcp otherwise; this is the same check for a container-published port, a
# native daemon and a server across the room, which is the point - it tests the
# path the client will take, not whether a process exists.
#
# nc needs an explicit connect timeout, and which flag supplies one is not the
# same everywhere. macOS spells it -G (its -w bounds idle reads and was measured
# not to bound a connect at all: a probe of an unreachable address on this
# network sat past 60 seconds with -w 3), while -G is an invalid option on Linux,
# where -w does cover establishment. So the flag is chosen once from nc's own
# help rather than from uname, and an nc that advertises neither still gets -w.
#
# Without it the probe inherits the kernel's SYN timeout - 75 seconds on macOS -
# and a remote host that is off or firewalled answers nothing at all. That wait
# is the same one that made the client's own CHECK SERVER look like a hang.
# `|| true` because nc -h exits non-zero after printing its usage, and this script
# runs under `set -o pipefail`: without it the pipeline reports nc's failure even
# though grep matched, the -G branch is never taken, and the probe silently falls
# back to a flag macOS ignores for connects - which is a 75-second wait wearing a
# "within 5s" message.
NC_TIMEOUT_OPTS=$(
    if { nc -h 2>&1 || true; } | grep -q -- '-G'; then
        printf -- '-G 5 -w 5'
    else
        printf -- '-w 5'
    fi
)

port_open() {
    if command -v nc >/dev/null 2>&1; then
        # shellcheck disable=SC2086 # deliberately word-split into flags
        nc -z $NC_TIMEOUT_OPTS "$HOST" "$PORT" >/dev/null 2>&1
        return $?
    fi
    (exec 3<>"/dev/tcp/$HOST/$PORT") >/dev/null 2>&1
}

wait_for_port() {
    local waited=0
    while [ "$waited" -lt 60 ]; do
        port_open && return 0
        sleep 1
        waited=$((waited + 1))
    done
    return 1
}

################################################################################
#                            step 1 - the engine                               #
################################################################################

docker_ready() { docker info >/dev/null 2>&1; }

wait_for_docker() {
    local waited=0
    while [ "$waited" -lt 90 ]; do
        docker_ready && return 0
        sleep 2
        waited=$((waited + 2))
    done
    return 1
}

install_docker() {
    command -v brew >/dev/null 2>&1 \
        || die "Homebrew is needed to install Docker: https://brew.sh"
    # colima rather than Docker Desktop: it installs without a GUI installer or
    # an admin password, and `colima start` is scriptable in a way that clicking
    # through Docker.app's first-run screens is not.
    warn "no container engine found"
    confirm "install colima + the docker CLI with Homebrew?" \
        || die "nothing to run the server in; install Docker Desktop or colima"
    say "installing colima and the docker CLI..."
    brew install colima docker || die "the Homebrew install failed"
}

start_engine() {
    if docker_ready; then
        ok "container engine is running"
        return 0
    fi
    if [ -d /Applications/Docker.app ]; then
        say "starting Docker Desktop..."
        open -a Docker || die "could not start Docker Desktop"
    elif command -v colima >/dev/null 2>&1; then
        say "starting colima..."
        colima start || die "colima could not start"
    else
        install_docker
        say "starting colima..."
        colima start || die "colima could not start"
    fi
    say "waiting for the engine..."
    wait_for_docker || die "the container engine did not come up"
    ok "container engine is running"
}

################################################################################
#                       steps 2-4 - certificates, image, server                 #
################################################################################

ensure_certs() {
    say "checking the development certificates..."
    make certs || die "could not mint certificates (is openssl installed?)"
}

ensure_image() {
    if [ "$DO_REBUILD" = "0" ] \
            && docker image inspect "$IMAGE" >/dev/null 2>&1; then
        ok "image $IMAGE is present (--rebuild to build it again)"
        return 0
    fi
    say "building the image - first time takes a while, notcurses is built from source"
    make docker-build DOCKER_PORT="$PORT" || die "the image build failed"
}

start_server_docker() {
    if port_open; then
        ok "something is already serving port $PORT; leaving it alone"
        return 0
    fi
    make docker-server DOCKER_PORT="$PORT" || die "the server container did not start"
    say "waiting for tetrisd to listen on $PORT..."
    if ! wait_for_port; then
        warn "tetrisd never answered on $PORT. Its own log says why:"
        docker logs --tail 40 "$SERVER" 2>&1 | sed 's/^/    /' >&2
        die "the server did not come up"
    fi
    ok "tetrisd is listening on $HOST:$PORT"
}

start_server_native() {
    if port_open; then
        ok "something is already serving port $PORT; leaving it alone"
        return 0
    fi
    say "starting the daemons natively..."
    make stack || die "the daemons did not come up"
    wait_for_port || die "tetrisd never answered on $PORT"
    ok "tetrisd is listening on $HOST:$PORT"
}

stop_server() {
    if [ "$UNAME_S" = "Darwin" ]; then
        docker_ready || die "the container engine is not running; nothing to stop"
        make docker-stop && ok "server stopped"
    else
        [ -x ./bin/tetrisctl ] \
            || die "./bin/tetrisctl is not built; nothing was started from here"
        PATH="$ROOT/bin:$PATH" TETRISHRC="$ROOT/.tetrishrc" \
            ./bin/tetrisctl stop && ok "daemons stopped"
    fi
}

################################################################################
#                             step 5 - the client                              #
################################################################################

ensure_client() {
    if [ -x bin/tetrisu ] || [ -x src/tetrisu/bin/tetrisu ]; then
        ok "tetrisu is built"
        return 0
    fi
    say "building tetrisu (this installs notcurses if it is missing)..."
    make -C src/tetrisu || die "tetrisu did not build"
    make bin-link >/dev/null 2>&1 || true
}

# notcurses is asked what the terminal can do at start-up and the board is drawn
# to match. A terminal with no bitmap protocol is not a dead end: only
# NCPIXEL_NONE loses bitmaps outright, and Solo composites a true-colour cell
# board instead (render_compatibility_mode in render_solo.c), so Terminal.app
# plays - it just plays in cells rather than pixel art. This used to claim the
# game could not draw at all, which was wrong, and wrong in the direction that
# talks somebody out of a client that would have worked.
check_terminal() {
    case "${TERM:-}" in
        ""|dumb) die "no usable TERM; run this from a real terminal" ;;
    esac
    if [ "${TERM_PROGRAM:-}" = "Apple_Terminal" ]; then
        warn "Terminal.app has no bitmap protocol, so the board draws in"
        warn "compatibility mode - playable, but cells instead of pixel art."
        warn "For the pixel board: brew install --cask kitty   (or: ghostty)"
    fi
}

# Which CA proves the server is the server. A remote host is the demo server, so
# it is the committed demo-ca.crt; a local one was just signed by this machine's
# own scratch CA. They are deliberately different files - see .gitignore - and
# picking the wrong one fails the handshake rather than degrading, because
# libtetrissh verifies the chain and refuses the session on any doubt.
client_ca() {
    if [ "$REMOTE" = "1" ]; then
        printf '%s' "$ROOT/certs/demo-ca.crt"
    else
        printf '%s' "$ROOT/certs/ca.crt"
    fi
}

launch_client() {
    local client="bin/tetrisu"
    local ca
    [ -x "$client" ] || client="src/tetrisu/bin/tetrisu"
    ca=$(client_ca)
    # The client verifies the server's certificate chain against this CA and
    # refuses the session without it. --client-only skips the step that mints the
    # local one, so this is the one path where it can be absent.
    if [ ! -s "$ca" ]; then
        [ "$REMOTE" = "1" ] \
            && die "certs/demo-ca.crt is missing - it is committed, so restore it with 'git checkout certs/demo-ca.crt'"
        die "certs/ca.crt is missing - run 'make certs' (the server needs the same CA)"
    fi
    say "launching tetrisu against $HOST:$PORT"
    printf '%b\n' "    ${BOLD}CHECK SERVER${RST} on the sign-in screen must report" \
        "    ${BOLD}SERVER ONLINE${RST} - without it Solo silently plays the local" \
        "    rules instead, and looks identical."
    if [ "$REMOTE" = "1" ]; then
        printf '%b\n' "    verifying that server against ${BOLD}certs/demo-ca.crt${RST}"
    fi
    echo
    TETRISU_NET=1 \
    TETRISU_HOST="$HOST" \
    TETRISU_PORT="$PORT" \
    TETRISU_CA_PATH="$ca" \
        "$client"
}

################################################################################
#                                    main                                      #
################################################################################

resolve_port

if [ "$DO_STOP" = "1" ]; then
    stop_server
    exit 0
fi

case "$UNAME_S" in
    Darwin)
        if [ "$WANT_SERVER" = "1" ]; then
            start_engine
            ensure_certs
            ensure_image
            start_server_docker
        fi
        ;;
    Linux)
        if [ "$WANT_SERVER" = "1" ]; then
            ensure_certs
            start_server_native
        fi
        ;;
    *)
        die "unsupported operating system: $UNAME_S"
        ;;
esac

if [ "$WANT_CLIENT" = "0" ]; then
    ok "server is up on $HOST:$PORT"
    printf '%b\n' "    connect with: ${BOLD}bash scripts/play.sh --client-only${RST}"
    printf '%b\n' "    stop it with: ${BOLD}bash scripts/play.sh --stop${RST}"
    exit 0
fi

# Checked here rather than left to the client, because a client that cannot
# reach the server does not say so plainly: Solo falls back to the local rules
# and plays identically. The advice differs by whose server it is - a local one
# this script can start, a remote one it can only report on.
if [ "$WANT_SERVER" = "0" ] && ! port_open; then
    if [ "$REMOTE" = "1" ]; then
        warn "nothing answered $HOST:$PORT within 5s. On that machine, check that:"
        warn "    tetrisd is up            ./bin/tetrisctl status"
        warn "    it listens on all interfaces, not just loopback"
        warn "    its firewall allows $PORT  (Arch: sudo ss -lntp | grep $PORT)"
        warn "    the address is current   ip -4 addr show   (the number before /24)"
        die "no server at $HOST:$PORT"
    fi
    die "nothing is serving $HOST:$PORT - drop --client-only to start one"
fi

check_terminal
ensure_client
launch_client

echo
if [ "$REMOTE" = "1" ]; then
    ok "client closed; $HOST:$PORT is not ours to stop"
    printf '%b\n' "    play again: ${BOLD}bash scripts/play.sh --host $HOST${RST}"
else
    ok "client closed; the server is still running on $HOST:$PORT"
    printf '%b\n' "    play again: ${BOLD}bash scripts/play.sh${RST}" \
        "    stop it:    ${BOLD}bash scripts/play.sh --stop${RST}"
fi
