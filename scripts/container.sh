#!/usr/bin/env bash
# Own the container engine and the client image: check for an engine, install
# one, build the image, and run tetrisu inside it.
#
#   bash scripts/container.sh check              is an engine available
#   bash scripts/container.sh install            install one (colima on macOS)
#   bash scripts/container.sh build [--rebuild]  build the client image
#   bash scripts/container.sh run -- ARGS        run tetrisu in it
#
# The image runs the client and nothing else. It never draws: the board leaves
# as Kitty-graphics escape sequences on the pty `docker run -t` allocates, and
# the terminal on the host renders them (see scripts/terminal.sh). That is why
# one Linux image serves a macOS client too — nothing has to hand a container's
# framebuffer to a Mac, because there isn't one.
#
# Environment:
#   TETRISU_IMAGE      image tag (default tetrish/tetrisu)
#   AUTO_INSTALL_DEPS  1 to install a missing engine, 0 to only report

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

BOLD=$(printf '\033[1m'); RED=$(printf '\033[1;31m')
GRN=$(printf '\033[1;32m'); YEL=$(printf '\033[1;33m')
BLU=$(printf '\033[1;34m'); RST=$(printf '\033[0m')

say()  { printf '%b\n' "${BLU}==>${RST} ${BOLD}$*${RST}"; }
ok()   { printf '%b\n' "    ${GRN}$*${RST}"; }
warn() { printf '%b\n' "    ${YEL}$*${RST}" >&2; }
die()  { printf '%b\n' "${RED}container.sh:${RST} $*" >&2; exit 1; }

UNAME_S="$(uname -s)"
AUTO_INSTALL_DEPS="${AUTO_INSTALL_DEPS:-1}"
IMAGE="${TETRISU_IMAGE:-tetrish/tetrisu}"

################################################################################
#                                    ENGINE                                    #
################################################################################

# podman is accepted because its CLI covers everything used here and it needs
# no daemon or group membership; docker is preferred only because it is what is
# already installed on most of these machines.
engine_binary() {
    if command -v docker >/dev/null 2>&1; then
        printf 'docker'
    elif command -v podman >/dev/null 2>&1; then
        printf 'podman'
    fi
}

# Present on PATH is not the same as usable: the docker CLI talks to a daemon
# that may not be running (colima stopped, Docker Desktop quit) and, on Linux,
# to a socket the user may not be in the group for. `info` is the cheapest call
# that actually crosses to the daemon, so it catches both.
engine_ready() {
    local engine="$1"
    "$engine" info >/dev/null 2>&1
}

is_wsl() {
    grep -Eqi '(microsoft|wsl)' /proc/sys/kernel/osrelease /proc/version \
        2>/dev/null
}

# Whose daemon is on the far end of the socket, which is a different question
# from which CLI is on PATH. Docker Desktop's WSL integration puts a docker
# binary in the distro that talks to a daemon in a *separate* WSL VM, so the
# container is not in this distro's network namespace even though the command
# was typed here - and that is the one difference the run path has to know
# about (see network_args). `info` is asked because it is the only answer that
# comes from the daemon itself; the CLI's own path and version say nothing.
engine_is_desktop() {
    local engine="$1"

    case "$("$engine" info --format '{{.OperatingSystem}}' 2>/dev/null)" in
        *"Docker Desktop"*) return 0 ;;
    esac
    return 1
}

resolve_sudo() {
    if [ "$(id -u)" -eq 0 ]; then
        SUDO=""
    elif command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    else
        die "root access or sudo is required to install a container engine."
    fi
}

install_engine_linux() {
    resolve_sudo
    say "installing a container engine..."
    if command -v apt-get >/dev/null 2>&1; then
        $SUDO apt-get update && $SUDO apt-get install -y docker.io
    elif command -v dnf >/dev/null 2>&1; then
        $SUDO dnf install -y docker
    elif command -v pacman >/dev/null 2>&1; then
        $SUDO pacman -S --needed --noconfirm docker
    elif command -v zypper >/dev/null 2>&1; then
        $SUDO zypper --non-interactive install docker
    elif command -v apk >/dev/null 2>&1; then
        $SUDO apk add docker
    else
        die "unsupported package manager; install Docker or Podman by hand."
    fi
    if command -v systemctl >/dev/null 2>&1; then
        $SUDO systemctl enable --now docker || \
            warn "could not start the docker service; start it and retry."
    fi

    # Installing the engine is not the same as being able to reach it: the
    # socket is root:docker, so without this the very first `make play` after a
    # successful install fails with a bare permission error on the socket.
    # The membership does not apply to an already-running login session, which
    # is the part that has to be said out loud rather than assumed.
    if ! id -nG | grep -qw docker; then
        say "adding $USER to the docker group..."
        $SUDO usermod -aG docker "$USER" || \
            warn "could not add $USER to the docker group; use sudo or add it by hand."
        warn "group membership does not apply to this login session yet."
        warn "run ${BOLD}newgrp docker${RST}, or log out and back in, then retry."
    fi
}

# macOS has no container runtime of its own, so the engine is a Linux VM.
# colima is used rather than Docker Desktop because it installs from Homebrew
# without a GUI installer or an admin password.
install_engine_darwin() {
    command -v brew >/dev/null 2>&1 \
        || die "Homebrew is required to install a container engine: https://brew.sh"
    say "installing colima and the docker CLI..."
    brew install colima docker
}

start_engine_darwin() {
    command -v colima >/dev/null 2>&1 || return 1
    colima status >/dev/null 2>&1 && return 0
    say "starting colima..."
    colima start
}

# Membership in the docker group is granted by the install but not applied to
# an already-running login session, so the very first `make play` after
# installing sees permission denied on the socket and nothing explains why.
report_socket_permission() {
    warn "the container engine is installed but not reachable."
    if [ "$UNAME_S" = "Linux" ] && ! id -nG | grep -qw docker; then
        warn "you are not in the ${BOLD}docker${RST} group. Add yourself and start a"
        warn "new login session (the group is not applied to this one):"
        warn "    ${BOLD}sudo usermod -aG docker $USER${RST}"
        warn "    then log out and back in, or run: ${BOLD}newgrp docker${RST}"
    else
        warn "check that the daemon is running:  ${BOLD}docker info${RST}"
    fi
}

ensure_engine() {
    local engine
    engine="$(engine_binary)"

    if [ -z "$engine" ]; then
        [ "$AUTO_INSTALL_DEPS" = "1" ] \
            || die "no container engine and automatic installation is disabled."
        if [ "$UNAME_S" = "Darwin" ]; then
            install_engine_darwin >&2
        else
            install_engine_linux >&2
        fi
        engine="$(engine_binary)"
        [ -n "$engine" ] || die "no container engine after installing."
    fi

    if ! engine_ready "$engine"; then
        [ "$UNAME_S" = "Darwin" ] && start_engine_darwin >&2 || true
    fi

    # An engine that is installed but unreachable is the common case rather than
    # an odd one: docker.io installed by hand leaves the socket root:docker and
    # the user outside the group. The install path above never runs here - there
    # was already an engine to find - so the group fix has to be reachable from
    # this branch too, or the machine stays stuck with a correct install.
    if ! engine_ready "$engine" && [ "$UNAME_S" = "Linux" ] \
            && [ "$AUTO_INSTALL_DEPS" = "1" ] \
            && ! id -nG | grep -qw docker; then
        say "docker is installed but not reachable; adding $USER to the docker group..." >&2
        resolve_sudo
        $SUDO usermod -aG docker "$USER" >&2 || true
    fi

    if ! engine_ready "$engine"; then
        report_socket_permission
        die "container engine is not usable yet."
    fi

    printf '%s' "$engine"
}

################################################################################
#                                    IMAGE                                     #
################################################################################

image_exists() {
    local engine="$1"
    "$engine" image inspect "$IMAGE" >/dev/null 2>&1
}

build_image() {
    local engine="$1" rebuild="${2:-0}" version

    if [ "$rebuild" != "1" ] && image_exists "$engine"; then
        ok "image $IMAGE is present"
        return 0
    fi

    # Passed explicitly rather than left to the Dockerfile's own sed so a
    # rebuild is triggered by the pin changing: the version becomes a build
    # argument, and the layer that builds notcurses is invalidated when it
    # moves. The Dockerfile keeps its fallback for a direct `docker build`.
    version="$(sed -n \
        's/^NOTCURSES_VERSION[[:space:]]*:=[[:space:]]*\([^[:space:]]*\).*/\1/p' \
        "$ROOT/src/tetrisu/Makefile" | head -n 1)"
    [ -n "$version" ] \
        || die "could not read NOTCURSES_VERSION from src/tetrisu/Makefile"

    say "building $IMAGE (notcurses $version) - the first build takes a while"
    "$engine" build \
        --build-arg "NOTCURSES_VERSION=$version" \
        -t "$IMAGE" \
        "$ROOT" || die "the image did not build"
    ok "image $IMAGE built"
}

################################################################################
#                                     RUN                                      #
################################################################################

# How the containerised client reaches a server on the host.
#
# On Linux --network host puts it in the host's namespace, so 127.0.0.1 is the
# same loopback the native tetrisd is listening on. On macOS the engine is a
# VM and there is no host namespace to join, so a loopback address has to be
# rewritten to the name the engine publishes for the host. A server somewhere
# else on the network is reached the same way from either.
#
# WSL is the case that looks like the first and behaves like the second, and it
# is decided by the daemon rather than by uname: `uname -s` says Linux, so
# --network host was passed, but under Docker Desktop that host is the
# docker-desktop VM and not this distro. Measured on a WSL2 distro with a
# listener on 0.0.0.0: `--network host` plus 127.0.0.1 does not connect and
# nothing reaches the listener at all, which is `play.sh --local` failing with
# no clue that the address was the problem. With a distro-local daemon
# (docker.io installed in WSL itself) the namespace really is shared and the
# Linux answer is correct, so this asks which daemon it is.
DESKTOP_IN_WSL=0

detect_desktop_in_wsl() {
    local engine="$1"

    DESKTOP_IN_WSL=0
    if [ "$UNAME_S" = "Linux" ] && is_wsl && engine_is_desktop "$engine"; then
        DESKTOP_IN_WSL=1
    fi
}

network_args() {
    if [ "$UNAME_S" = "Linux" ] && [ "$DESKTOP_IN_WSL" != "1" ]; then
        printf '%s' "--network host"
    fi
}

# The address this distro answers on as seen from outside it, which is what a
# container in the neighbouring VM has to dial. The source address of a route
# out is asked rather than `hostname -I`, because that lists every address the
# machine has - docker's own bridges included - and the first is not reliably
# the routable one. It is eth0's under WSL's default NAT and the mirrored
# Windows interface's under networkingMode=mirrored, and both are right.
wsl_distro_address() {
    local addr

    addr="$(ip -4 route get 1.1.1.1 2>/dev/null \
        | sed -n 's/.*[[:space:]]src[[:space:]]\([0-9.]\{7,\}\).*/\1/p' \
        | head -n 1)"
    [ -n "$addr" ] || return 1
    printf '%s' "$addr"
}

# Only a loopback address is ever rewritten: it is the one that means "this
# machine" to the caller and something else inside a container. A named server
# is left exactly as typed, here and on macOS both.
#
# Under Desktop-in-WSL the distro's own address is preferred over
# host.docker.internal, because they are not the same machine: that name is the
# *Windows* host, and it reaches a server in this distro only if WSL's
# localhost forwarding relays it - a Windows-side setting this script cannot
# see. The distro's address needs no relay and was measured to carry data;
# host.docker.internal stays as the fallback for a distro with no route out.
resolve_host() {
    local host="$1" addr

    case "$host" in
        127.0.0.1|localhost|::1) ;;
        *) printf '%s' "$host"; return 0 ;;
    esac

    if [ "$DESKTOP_IN_WSL" = "1" ]; then
        if addr="$(wsl_distro_address)"; then
            warn "Docker Desktop's containers are not in this distro's network"
            warn "namespace, so the local server is dialled at $addr."
            printf '%s' "$addr"
        else
            warn "could not find this distro's address; trying the Windows host."
            printf 'host.docker.internal'
        fi
        return 0
    fi
    if [ "$UNAME_S" != "Linux" ]; then
        printf 'host.docker.internal'
        return 0
    fi
    printf '%s' "$host"
}

# Sound, unlike the board, does not leave over the pty. The board is escape
# sequences the host terminal renders, so the container needs no display; audio
# is SDL2 opening a device, and a container has none - no /dev/snd, no
# PulseAudio socket - so Mix_OpenAudio fails and audio_init() takes its silent
# fallback. The image links SDL2_mixer and SDL2's pulse driver, so the whole gap
# is a socket: bind mount the host's and name it in PULSE_SERVER.
#
# ALSA is deliberately not offered as a fallback. Passing --device /dev/snd
# hands the container exclusive access to the card on a machine without a sound
# server, and every machine this runs on has one.
pulse_socket() {
    local candidate

    # WSLg publishes its own, outside XDG_RUNTIME_DIR, and it is the one that
    # works there: the runtime-dir entry is a symlink into /mnt/wslg anyway.
    for candidate in \
        /mnt/wslg/PulseServer \
        "${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/pulse/native" \
        "/run/user/$(id -u)/pulse/native"
    do
        if [ -S "$candidate" ]; then
            printf '%s' "$candidate"
            return 0
        fi
    done
    return 1
}

# A desktop PulseAudio/PipeWire authenticates a native-socket client with a
# cookie; WSLg's server does not ask for one. Mount it when it exists rather
# than deciding which server is on the far end.
pulse_cookie() {
    local candidate="${PULSE_COOKIE:-${XDG_CONFIG_HOME:-$HOME/.config}/pulse/cookie}"

    if [ -f "$candidate" ]; then
        printf '%s' "$candidate"
        return 0
    fi
    return 1
}

# Emitted as an array of `docker run` flags, empty when the host has no server.
audio_args() {
    local socket cookie

    if ! socket="$(pulse_socket)"; then
        # Not worth saying on macOS, where there is no bridge from colima's VM
        # to CoreAudio and so nothing the reader could act on.
        if [ "$UNAME_S" = "Linux" ]; then
            warn "no PulseAudio socket on this host; the client runs silent."
        fi
        return 0
    fi
    AUDIO_FLAGS=(
        -v "$socket:/tetrish/run/pulse"
        -e PULSE_SERVER=unix:/tetrish/run/pulse
    )
    if cookie="$(pulse_cookie)"; then
        AUDIO_FLAGS+=(
            -v "$cookie:/tetrish/run/pulse-cookie:ro"
            -e PULSE_COOKIE=/tetrish/run/pulse-cookie
        )
    fi
}

# The client verifies the server's chain against a CA on the host, and reads
# its artwork from the host's checkout. Both are bind mounted read-only at the
# paths the binary already names: certs/ because credentials are never baked
# into an image, assets/ because 183 MB of artwork in a layer would be a copy
# of what is already on disk.
run_client() {
    local engine host port ca net
    engine="$(ensure_engine)"

    # Before anything asks how to reach the host, because that answer depends
    # on which daemon accepted the connection rather than on this OS.
    detect_desktop_in_wsl "$engine"

    host="$(resolve_host "${TETRISU_HOST:-127.0.0.1}")"
    port="${TETRISU_PORT:-4242}"
    ca="${TETRISU_CA_PATH:-$ROOT/certs/demo-ca.crt}"
    net="$(network_args)"

    AUDIO_FLAGS=()
    audio_args

    [ -s "$ca" ] || die "CA file '$ca' is missing or empty"
    [ -d "$ROOT/src/tetrisu/assets" ] \
        || die "src/tetrisu/assets is missing from this checkout"

    # TERM and COLORTERM are the host terminal's own, because this runs inside
    # the terminal that will render the output: notcurses inside the container
    # must see the capabilities of the terminal on the far end of the pty, not
    # a guess about it.
    #
    # --init matters more than it looks: tetrisu is the image's ENTRYPOINT and
    # installs no signal handlers, so without it tetrisu is PID 1 of its own
    # namespace, where Linux discards any signal with no explicit handler
    # instead of taking the default action. Closing the kitty window hangs up
    # the pty (SIGHUP) to this docker client, which forwards it into the
    # container - and PID 1 drops it on the floor, so the container outlives
    # the window and --rm never runs. --init puts Docker's bundled tini at
    # PID 1 instead, so tetrisu runs at PID 2 or later under ordinary signal
    # semantics and the forwarded SIGHUP actually kills it.
    # shellcheck disable=SC2086 # $net is deliberately word-split into flags
    # ${a[@]+"${a[@]}"} rather than "${a[@]}": under `set -u` bash 3.2, which is
    # what macOS ships, an empty array expands as an unset variable and aborts -
    # and macOS is precisely where it stays empty, having no socket to pass.
    exec "$engine" run --rm -it --init \
        $net \
        ${AUDIO_FLAGS[@]+"${AUDIO_FLAGS[@]}"} \
        -e TERM="${TERM:-xterm-kitty}" \
        -e COLORTERM="${COLORTERM:-truecolor}" \
        -e TETRISU_NET=1 \
        -e TETRISU_HOST="$host" \
        -e TETRISU_PORT="$port" \
        -e TETRISU_CA_PATH=/tetrish/certs/ca-bundle.crt \
        -e TETRISU_RENDERER="${TETRISU_RENDERER:-}" \
        -v "$ca:/tetrish/certs/ca-bundle.crt:ro" \
        -v "$ROOT/src/tetrisu/assets:/tetrish/src/tetrisu/assets:ro" \
        "$IMAGE" "$@"
}

################################################################################
#                                     MAIN                                     #
################################################################################

action="${1:-check}"
[ $# -gt 0 ] && shift

case "$action" in
    check)
        engine="$(engine_binary)"
        [ -n "$engine" ] || { echo "container engine: none"; exit 1; }
        if engine_ready "$engine"; then
            echo "container engine: $engine (ready)"
        else
            echo "container engine: $engine (installed, not reachable)"
            exit 1
        fi
        ;;
    install)
        ensure_engine >/dev/null
        ok "container engine is ready"
        ;;
    build)
        rebuild=0
        [ "${1:-}" = "--rebuild" ] && rebuild=1
        engine="$(ensure_engine)"
        build_image "$engine" "$rebuild"
        ;;
    run)
        [ "${1:-}" = "--" ] && shift
        run_client "$@"
        ;;
    *)
        die "unknown action '$action' (check, install, build, run)"
        ;;
esac
