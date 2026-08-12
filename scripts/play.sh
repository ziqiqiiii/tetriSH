#!/usr/bin/env bash
# One command from a fresh clone to a playable client.
#
#   bash scripts/play.sh            set everything up, then play
#   bash scripts/play.sh --help     every flag
#
# Two ways to get a client, and the only difference is where it is built:
#
#   default      on this host - install the dependencies, compile them, run
#                the binary. `make play`.
#   --container  in a container - the image carries the toolchain, notcurses
#                and every library, so the host installs none of them.
#                `make play-image`.
#
# Both end the same way: a kitty window with tetrisu in it. The container does
# not draw - tetrisu's board is Kitty-graphics-protocol escape sequences, which
# are just bytes on the pty `docker run -t` allocates, so the host terminal
# renders them either way. That is what makes one Linux image serve a Linux,
# macOS and WSL client alike, and why the container path works on a Mac, which
# cannot build the server at all: tetrisd's reactor is epoll and timerfd and
# libcoreipc's message queues are POSIX mqueue, none of which Darwin has.
#
# The path it walks: dependencies, terminal, (engine), certificates, server,
# client, launch. The cheap checks come first on purpose - a missing toolchain,
# no terminal, or an engine needing a re-login all refuse the run, and none is
# worth discovering after a long build.
#
# It is deliberately re-runnable - every step checks before it acts, so a valid
# certificate is not reissued, a server already listening is left alone, and an
# image already built is not rebuilt. The second run is fast and the tenth is
# the normal way to restart the client.
#
# The three pieces have one owner each:
#   scripts/container.sh  the engine, the image, and how the client is run
#   scripts/terminal.sh   which terminal draws, and whether one can be opened
#   this script           the order they happen in

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

BOLD=$(printf '\033[1m'); RED=$(printf '\033[1;31m')
GRN=$(printf '\033[1;32m'); YEL=$(printf '\033[1;33m')
BLU=$(printf '\033[1;34m'); RST=$(printf '\033[0m')

UNAME_S="$(uname -s)"

# Declared here and exported so container.sh and terminal.sh inherit the same
# policy this script was invoked with, rather than each defaulting on its own.
AUTO_INSTALL_DEPS="${AUTO_INSTALL_DEPS:-1}"
export AUTO_INSTALL_DEPS

WANT_SERVER=1
WANT_CLIENT=1
DO_STOP=0
DO_REBUILD=0
# Built on this host unless asked otherwise. The container is the opt-in,
# because the machine that has just run `make` already has everything the
# native path needs and pulling an image would be the slower answer to a
# question already answered.
NATIVE=1
CLIENT_ONLY=0
WANT_LOCAL=0
PORT=""
# Where the server is. Empty means "not asked for", and that now resolves to the
# shared server rather than to this machine: playing together is the common
# case, and a bare `make play` that quietly served itself was a game nobody else
# could see. --host names a different one, --local brings it back here.
HOST=""
LOCAL_HOST="127.0.0.1"

# The shared tetriSH server, and the CA question rides on it: it is signed by
# the committed certs/demo-ca.crt, which is exactly what REMOTE=1 selects below.
#
# The address rather than tetrish.dev, because this is the one string in the
# path that cannot fall back - an unresolvable name fails inside the client's
# connect as a timeout, several screens away from anything naming DNS. Override
# either way with TETRISH_HOST=tetrish.dev or `make play HOST=...`.
DEFAULT_HOST="${TETRISH_HOST:-159.65.11.120}"

say()  { printf '%b\n' "${BLU}==>${RST} ${BOLD}$*${RST}"; }
ok()   { printf '%b\n' "    ${GRN}$*${RST}"; }
warn() { printf '%b\n' "    ${YEL}$*${RST}" >&2; }
die()  { printf '%b\n' "${RED}play.sh:${RST} $*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Usage: bash scripts/play.sh [options]

  --host ADDR     play on a different server (e.g. --host 10.27.229.33);
                  nothing is started locally and the demo CA is used
  --local         play on a server on this machine, starting one if needed,
                  verified against this machine's own certs/ca.crt
  --port N        port to serve and connect on (default: TETRISD_PORT in .tetrishrc)
  --server-only   bring a local server up and stop, without launching a client
  --client-only   launch a client against a local server that is already up
  --container     build and run the client in a container instead of on this
                  host; the image carries the toolchain, so nothing is
                  installed here (this is what `make play-image` runs)
  --native        build and run the client on this host (the default)
  --rebuild       rebuild the client image first; --container only
  --stop          stop the server and exit
  -h, --help      this text

With neither --host nor --local the client plays on the shared tetriSH server
and nothing is started here. --local is the old behaviour: a server is started
on this machine and left running when the client exits, so the next run starts a
client immediately; `bash scripts/play.sh --stop` takes it down.

Environment:
  TETRISH_HOST      the shared server used when no --host is given
  TETRISU_TERMINAL  force the terminal: kitty, wezterm, or none (run in place)
  TETRISU_IMAGE     client image tag (default tetrish/tetrisu)
  TETRISU_RENDERER  pin the renderer tier: cell, stationary, pixel
  AUTO_INSTALL_DEPS 0 to check for missing dependencies without installing
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --host)        HOST="${2:-}"; shift 2 || die "--host needs an address" ;;
        --host=*)      HOST="${1#*=}"; shift ;;
        --local)       WANT_LOCAL=1; shift ;;
        --port)        PORT="${2:-}"; shift 2 || die "--port needs a number" ;;
        --port=*)      PORT="${1#*=}"; shift ;;
        --server-only) WANT_CLIENT=0; shift ;;
        --client-only) WANT_SERVER=0; CLIENT_ONLY=1; shift ;;
        --rebuild)     DO_REBUILD=1; shift ;;
        --native)      NATIVE=1; shift ;;
        --container)   NATIVE=0; shift ;;
        --image)       NATIVE=0; shift ;;
        --stop)        DO_STOP=1; shift ;;
        -h|--help)     usage; exit 0 ;;
        *)             usage >&2; die "unknown option: $1" ;;
    esac
done

if [ "$WANT_SERVER" = "0" ] && [ "$WANT_CLIENT" = "0" ]; then
    die "--server-only and --client-only ask for opposite halves; pick one"
fi

# Three ways to name the server and only one of them has anything to start here.
#
# A named host is another machine, so there is nothing local to start or stop.
# Rather than quietly ignoring the flags that say otherwise, refuse them:
# --server-only with a remote host asks this script to start a server it has no
# reach into, and would otherwise appear to succeed.
#
# The three flags that are *about* a server on this machine - start one, stop
# one, connect to one already up - name it by saying so, so they select local
# rather than colliding with a default that points away from here. That keeps
# `--stop` meaning what it always meant after the default moved off this host.
if [ -n "$HOST" ]; then
    [ "$WANT_LOCAL" = "1" ] \
        && die "--host and --local name different servers; pick one"
    [ "$WANT_CLIENT" = "0" ] \
        && die "--host names a server elsewhere; --server-only cannot start one there"
    [ "$DO_STOP" = "1" ] \
        && die "--host names a server elsewhere; --stop only reaches the local one"
    WANT_SERVER=0
    REMOTE=1
elif [ "$WANT_LOCAL" = "1" ] || [ "$DO_STOP" = "1" ] \
     || [ "$WANT_CLIENT" = "0" ] || [ "$CLIENT_ONLY" = "1" ]; then
    HOST="$LOCAL_HOST"
    REMOTE=0
else
    HOST="$DEFAULT_HOST"
    WANT_SERVER=0
    REMOTE=1
fi

# macOS cannot run the server at all, so there is never a local one to start
# there. The client half is unaffected - it runs in the container like anywhere
# else - so this drops the server steps rather than refusing the command, which
# is what makes a bare `make play` work on a Mac.
if [ "$UNAME_S" = "Darwin" ] && [ "$WANT_SERVER" = "1" ]; then
    if [ "$WANT_CLIENT" = "0" ]; then
        die "tetrisd cannot be built or run on macOS (needs epoll, timerfd, POSIX mqueue)"
    fi
    warn "macOS cannot run tetrisd, so no server is started here."
    warn "Type the server's address into ${BOLD}SERVER ID${RST} on the sign-in screen,"
    warn "or pass ${BOLD}--host ADDR${RST} to check it first."
    WANT_SERVER=0
fi

# The port is one number shared by three places - the server's listener, the
# client's dial and .tetrishrc - so it is read from .tetrishrc once and passed
# everywhere from here.
resolve_port() {
    [ -n "$PORT" ] && return 0
    PORT=$(sed -n \
        's/^[[:space:]]*export[[:space:]]*TETRISD_PORT=\([0-9]\{1,\}\).*/\1/p' \
        .tetrishrc 2>/dev/null | head -1)
    [ -z "$PORT" ] && PORT=4242
    return 0
}

# Is anything accepting connections there? nc where it exists, and bash's own
# /dev/tcp otherwise; this is the same check for a native daemon and a server
# across the room, which is the point - it tests the path the client will
# take, not whether a process exists.
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
#                       steps 1-2 - certificates, server                       #
################################################################################

ensure_certs() {
    say "checking the development certificates..."
    make certs || die "could not mint certificates (is openssl installed?)"
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
    [ -x ./bin/tetrisctl ] \
        || die "./bin/tetrisctl is not built; nothing was started from here"
    PATH="$ROOT/bin:$PATH" TETRISHRC="$ROOT/.tetrishrc" \
        ./bin/tetrisctl stop && ok "daemons stopped"
}

################################################################################
#                        step 3 - the client environment                       #
################################################################################

# The host toolchain, and only when this run will actually use it.
#
# The container path compiles nothing here - the image carries the compiler,
# notcurses and every library - so a client-only machine, which is every Mac, is
# not made to install GCC and OpenSSL just to play. The paths that do compile on
# this host ask for them: a local server, and --native. Both would pull `make
# deps` in on their own through `make stack` and src/tetrisu's
# check-dependencies, but doing it here means a missing toolchain is reported at
# the start rather than surfacing halfway through a build.
# AUTO_INSTALL_DEPS is passed as a make *override* rather than exported: the
# Makefile assigns it with `:=`, and a makefile assignment beats the
# environment, so exporting it would be silently ignored and a check-only run
# would install things anyway.
ensure_deps() {
    if [ "$WANT_SERVER" != "1" ] && [ "$NATIVE" != "1" ]; then
        return 0
    fi
    say "checking the build dependencies..."
    # WANT_ENGINE is answered here rather than left to default, because the
    # engine is a root dependency now: without this a native run would install
    # a container runtime, and ask for the docker group, on its way to a client
    # that never opens a container.
    #
    # WANT_TERMINAL is answered for the opposite reason: the terminal step would
    # do the right thing, but ensure_terminal runs a few lines below with the
    # messaging this script's own failure paths need, so letting `make deps`
    # check first only means checking twice and explaining it twice.
    WANT_ENGINE=$([ "$NATIVE" = "1" ] && echo 0 || echo 1) \
    WANT_TERMINAL=0 \
    make deps AUTO_INSTALL_DEPS="$AUTO_INSTALL_DEPS" \
        || die "dependencies are missing; see the output above"
}

# Done before the image, not with it: installing a terminal is quick and can
# fail in a way the user has to act on (WezTerm has to be installed on the
# Windows side by hand), and finding that out after a ten-minute first image
# build is finding it out too late.
ensure_terminal() {
    say "checking the terminal..."
    bash scripts/terminal.sh install || \
        warn "continuing without a terminal to launch; the client will run here."
}

# Split from the image build for the same reason: this is a cheap check that
# can demand an action from the user (a re-login for the docker group), and it
# has to fail before the expensive step rather than after it.
ensure_engine() {
    say "checking the container engine..."
    bash scripts/container.sh install || die "no usable container engine"
}

ensure_client_image() {
    if [ "$DO_REBUILD" = "1" ]; then
        bash scripts/container.sh build --rebuild || die "the image did not build"
    else
        bash scripts/container.sh build || die "the image did not build"
    fi
}

# Compiled every run rather than skipped when a binary is already there: make
# settles in a moment when nothing changed, and skipping on the binary's mere
# existence is how an edited source file gets played around instead of played.
# This builds the four CoreStack archives tetrisu links as well as the client -
# they are its dependencies, and src/tetrisu/Makefile recurses into them.
ensure_native_client() {
    say "compiling tetrisu and the libraries it links..."
    # DEPS_READY=1 because ensure_deps already ran the root dependency step.
    # Without it src/tetrisu's check-dependencies recurses back into `make
    # deps` with this script's flags stripped - which is how a check-only run
    # ended up trying to install a container engine mid-compile. Its own
    # render/audio deps still run; only the root recursion is suppressed.
    # AUTO_INSTALL_DEPS as a make override, not just the exported value: both
    # Makefiles set it with `:=`, which beats the environment, so a check-only
    # run would otherwise install the client's optional audio packages anyway.
    make -C src/tetrisu DEPS_READY=1 \
        AUTO_INSTALL_DEPS="$AUTO_INSTALL_DEPS" || die "tetrisu did not build"
    make bin-link >/dev/null 2>&1 || true
    ok "tetrisu is built"
}

################################################################################
#                             step 4 - the client                              #
################################################################################

# Which CA proves the server is the server. A remote host is the demo server, so
# it is the committed demo-ca.crt; a local one was just signed by this machine's
# own scratch CA. They are deliberately different files - see .gitignore - and
# picking the wrong one fails the handshake rather than degrading, because
# libtetrissh verifies the chain and refuses the session on any doubt.
#
# "Local" is decided by the file being there rather than by which flags were
# passed, because there is a third case: a Mac, which starts no server and so
# mints no CA, but is also not talking to a named --host. It has only the
# committed demo CA, and asking it for certs/ca.crt would fail a fresh clone
# before the sign-in screen it was about to type an address into.
client_ca() {
    if [ "$REMOTE" != "1" ] && [ -s "$ROOT/certs/ca.crt" ]; then
        printf '%s' "$ROOT/certs/ca.crt"
    else
        printf '%s' "$ROOT/certs/demo-ca.crt"
    fi
}

# The client is launched *through* the terminal script rather than beside it:
# terminal.sh opens a window and runs the command in it, or - when there is no
# display to open one on - runs it right here. Either way the process it ends up
# running is the container, and the escape sequences that come back out reach
# whatever terminal is on the far end.
launch_client() {
    local ca
    ca=$(client_ca)
    # The client verifies the server's certificate chain against this CA and
    # refuses the session without it. --client-only skips the step that mints the
    # local one, so this is the one path where it can be absent.
    #
    # The advice is keyed off which file was actually chosen rather than off
    # REMOTE, because they disagree on a Mac: no --host, but the demo CA all the
    # same, and "run make certs" would be the wrong instruction there - it mints
    # a CA for a server that machine cannot run.
    if [ ! -s "$ca" ]; then
        case "$ca" in
            *demo-ca.crt)
                die "certs/demo-ca.crt is missing - it is committed, so restore it with 'git checkout certs/demo-ca.crt'" ;;
            *)
                die "certs/ca.crt is missing - run 'make certs' (the server needs the same CA)" ;;
        esac
    fi
    say "launching tetrisu against $HOST:$PORT"
    printf '%b\n' "    ${BOLD}CHECK SERVER${RST} on the sign-in screen must report" \
        "    ${BOLD}SERVER ONLINE${RST} - without it Solo silently plays the local" \
        "    rules instead, and looks identical."
    # Which CA is in play is worth saying out loud, because the client can
    # trust exactly one and the sign-in screen invites you to change the
    # server it applies to. lib/libtetrissh's load_cert_file reads a single
    # certificate (PEM_read_X509, in the frozen common.c), so a bundle holding
    # both CAs is not an option - the second one would be ignored.
    printf '%b\n' "    verifying against ${BOLD}${ca#$ROOT/}${RST}"
    if [ "$REMOTE" = "1" ] && [ "$HOST" = "$DEFAULT_HOST" ]; then
        printf '%b\n' "    play again: ${BOLD}make play${RST}" \
            "    on this machine instead: ${BOLD}bash scripts/play.sh --local${RST}"
    elif [ "$REMOTE" = "1" ]; then
        printf '%b\n' "    play again: ${BOLD}make play HOST=$HOST${RST}"
    else
        printf '%b\n' \
            "    ${YEL}typing a different SERVER ID than $HOST will fail${RST}" \
            "    ${YEL}verification - that server is signed by another CA.${RST}" \
            "    for one elsewhere: ${BOLD}make play HOST=<address>${RST}"
        printf '%b\n' "    stop server: ${BOLD}bash scripts/play.sh --stop${RST}"
    fi
    echo

    export TETRISU_HOST="$HOST"
    export TETRISU_PORT="$PORT"
    export TETRISU_CA_PATH="$ca"

    if [ "$NATIVE" = "1" ]; then
        local client="bin/tetrisu"
        [ -x "$client" ] || client="src/tetrisu/bin/tetrisu"
        # Only the native path sets this: container.sh passes it into the image
        # itself. Without it the client builds its fixture provider instead of a
        # session, and CHECK SERVER reports offline without opening a socket -
        # the same screen a wrong address gives, for a reason no address fixes.
        export TETRISU_NET=1
        exec bash scripts/terminal.sh launch -- "$ROOT/$client"
    fi

    exec bash scripts/terminal.sh launch -- \
        bash "$ROOT/scripts/container.sh" run --
}

################################################################################
#                                    main                                      #
################################################################################

resolve_port

if [ "$DO_STOP" = "1" ]; then
    stop_server
    exit 0
fi

# Everything cheap that can refuse this run happens first: a missing toolchain,
# no terminal, an engine that needs a re-login. None of them is worth
# discovering after `make stack` has built the tree or an image build has run
# for ten minutes.
ensure_deps
ensure_terminal
[ "$NATIVE" = "1" ] || ensure_engine

case "$UNAME_S" in
    Darwin) ;;
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
#
# Only these two cases name a server that is supposed to be up already. A Mac
# with no --host names nothing yet: the address is about to be typed into
# SERVER ID, so there is no address here to probe and nothing to warn about.
if [ "$REMOTE" = "1" ] && [ "$HOST" = "$DEFAULT_HOST" ] && ! port_open; then
    warn "nothing answered the shared server $HOST:$PORT within 5s."
    warn "It is not this machine, so nothing here can bring it up. Either:"
    warn "    play on this machine     ${BOLD}bash scripts/play.sh --local${RST}"
    warn "    name another server      ${BOLD}make play HOST=<address>${RST}"
    warn "    or wait for it to return"
    die "no server at $HOST:$PORT"
elif [ "$REMOTE" = "1" ] && ! port_open; then
    warn "nothing answered $HOST:$PORT within 5s. On that machine, check that:"
    warn "    tetrisd is up            ./bin/tetrisctl status"
    warn "    it listens on all interfaces, not just loopback"
    warn "    its firewall allows $PORT  (Arch: sudo ss -lntp | grep $PORT)"
    warn "    the address is current   ip -4 addr show   (the number before /24)"
    die "no server at $HOST:$PORT"
elif [ "$CLIENT_ONLY" = "1" ] && ! port_open; then
    die "nothing is serving $HOST:$PORT - drop --client-only to start one"
fi

if [ "$NATIVE" = "1" ]; then
    ensure_native_client
else
    ensure_client_image
fi

# Replaces this process, so nothing runs after it - the "play again" hints are
# printed by announce() before the client starts rather than after it exits.
launch_client
