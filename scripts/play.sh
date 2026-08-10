#!/usr/bin/env bash
# One command from a fresh clone to a playable client.
#
#   bash scripts/play.sh            set everything up, then play
#   bash scripts/play.sh --host H   play on a server someone else is running
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
# `--host` is the deployed case (docs/deployment.md): the server half already
# exists on a VPS, so every step but the client is skipped - no container engine
# is needed on the laptop at all, on either OS. What changes with it is the CA:
# the chain is verified against the *server's* CA, and certs/ca.crt is this
# machine's own, so a remote host defaults to the tracked deploy/vps-ca.crt
# instead. Verification is chain-only, so the certificate does not have to name
# the host being dialled.
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
#
# It is the *server* image. The client half of this script is native on both
# platforms, so nothing it starts in a container has a board to draw, and the
# server target leaves notcurses and SDL2 out of the build entirely.
IMAGE="${DOCKER_REF:-tetrish:server}"
SERVER="${DOCKER_SERVER:-tetrish-server}"

# Where each half looks for the CA. certs/ is git-ignored and holds whatever
# this machine last minted, so it can only ever be right for a server this
# machine is also running; a deployed server's CA is tracked instead, because
# every teammate needs it and nothing in it is secret.
LOCAL_HOST="127.0.0.1"
LOCAL_CA="certs/ca.crt"
REMOTE_CA="deploy/vps-ca.crt"
PROBE_TIMEOUT_S=3

ASSUME_YES=0
DO_REBUILD=0
WANT_SERVER=1
WANT_CLIENT=1
DO_STOP=0
PORT=""

say()  { printf '%b\n' "${BLU}==>${RST} ${BOLD}$*${RST}"; }
ok()   { printf '%b\n' "    ${GRN}$*${RST}"; }
warn() { printf '%b\n' "    ${YEL}$*${RST}" >&2; }
die()  { printf '%b\n' "${RED}play.sh:${RST} $*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Usage: bash scripts/play.sh [options]

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

# --host names a server this machine is not running, so there is no half left
# to bring up here: it implies --client-only rather than combining with it, and
# contradicts the two flags that act on a locally started server.
if [ -n "$HOST" ]; then
    if [ "$DO_STOP" = "1" ]; then
        die "--stop takes down a server started here; --host names someone else's"
    fi
    if [ "$WANT_CLIENT" = "0" ]; then
        die "--server-only starts a server here; --host says one is already up elsewhere"
    fi
    WANT_SERVER=0
fi

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

# Whether the server being dialled is on some other machine. Only the CA and a
# few messages depend on it: everything else is already addressed through $HOST.
is_remote() {
    case "$HOST" in
        127.0.0.1|localhost|::1) return 1 ;;
        *) return 0 ;;
    esac
}

# The host and the CA travel together, because the CA is only correct for the
# server it signed: this machine's certs/ can only vouch for a server this
# machine started, so a remote host falls back to the tracked one instead. The
# path is made absolute because the client is launched with it as an env var and
# resolves it against its own working directory, not this script's.
resolve_target() {
    [ -n "$HOST" ] || HOST="$LOCAL_HOST"
    if [ -z "$CA" ]; then
        if is_remote; then CA="$REMOTE_CA"; else CA="$LOCAL_CA"; fi
    fi
    case "$CA" in
        /*) ;;
        *) CA="$ROOT/$CA" ;;
    esac
}

# Is anything accepting connections there? nc where it exists, and bash's own
# /dev/tcp otherwise; this is the same check for a container-published port and
# a native daemon, which is the point - it tests the path the client will take,
# not whether a process exists.
port_open() {
    if command -v nc >/dev/null 2>&1; then
        nc -z 127.0.0.1 "$PORT" >/dev/null 2>&1
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
    say "building the server image - the first one takes a few minutes"
    make docker-build-server || die "the image build failed"
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

# notcurses queries the terminal for graphics support at start-up, and Solo is
# drawn as pixel art: on a terminal with no bitmap protocol it does not fall
# back, it refuses to start. Saying so here beats letting the client exit with
# a notcurses error the moment a game is chosen.
check_terminal() {
    case "${TERM:-}" in
        ""|dumb) die "no usable TERM; run this from a real terminal" ;;
    esac
    if [ "${TERM_PROGRAM:-}" = "Apple_Terminal" ]; then
        warn "Terminal.app has no bitmap graphics protocol, so Solo cannot draw."
        warn "Install one that has and run this again from it:"
        warn "    brew install --cask kitty      (or: ghostty)"
        confirm "start the client here anyway?" || exit 1
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
    # refuses the session without it. --client-only skips the step that mints it,
    # so this is the one path where it can be absent.
    [ -s certs/ca.crt ] \
        || die "certs/ca.crt is missing - run 'make certs' (the server needs the same CA)"
    say "launching tetrisu against 127.0.0.1:$PORT"
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
    TETRISU_CA_PATH="$ROOT/certs/ca.crt" \
        "$client"
}

################################################################################
#                                    main                                      #
################################################################################

resolve_port
resolve_target

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
    die "nothing is serving 127.0.0.1:$PORT - drop --client-only to start one"
fi

check_terminal
ensure_client
launch_client

echo
ok "client closed; the server is still running on 127.0.0.1:$PORT"
printf '%b\n' "    play again: ${BOLD}bash scripts/play.sh${RST}" \
    "    stop it:    ${BOLD}bash scripts/play.sh --stop${RST}"
