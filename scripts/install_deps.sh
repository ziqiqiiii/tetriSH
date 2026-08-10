#!/usr/bin/env bash
# Install the tetriSH root dependencies shared by multiple components: the
# compiler toolchain, pkg-config, OpenSSL, Readline, and ncurses (plus Valgrind
# where available, for the memory-safety runs). Component-only render/audio
# packages belong in that component's own Makefile/scripts, not here.
#
# Privilege is resolved once, at the top; sudo performs its own password prompt.

set -euo pipefail

UNAME_S="$(uname -s)"

resolve_sudo() {
    if [ "$(id -u)" -eq 0 ]; then
        SUDO=""
    elif command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    else
        echo "Root access or sudo is required to install packages." >&2
        exit 1
    fi
}

install_linux() {
    if grep -Eqi '(microsoft|wsl)' /proc/sys/kernel/osrelease \
            /proc/version 2>/dev/null; then
        echo "Installing tetriSH dependencies for WSL..."
    else
        echo "Installing tetriSH dependencies for Linux..."
    fi

    resolve_sudo

    if command -v apt-get >/dev/null 2>&1; then
        $SUDO apt-get update
        # binutils is named explicitly even though build-essential depends on
        # it: naming it is what upgrades an assembler that is older than the
        # installed GCC, and an already-satisfied build-essential upgrades
        # nothing. GCC 15 emits `.base64` for non-ASCII string constants and
        # only binutils 2.44 and newer can assemble it (see check_deps.sh).
        $SUDO apt-get install -y build-essential binutils pkg-config \
            libssl-dev libreadline-dev libncurses-dev
        if apt-cache show valgrind >/dev/null 2>&1; then
            $SUDO apt-get install -y valgrind libc6-dbg || \
                echo "Warning: Valgrind install failed; build dependencies are installed."
        else
            echo "Warning: Valgrind package not found; build can continue without it."
        fi
    elif command -v dnf >/dev/null 2>&1; then
        $SUDO dnf install -y gcc make binutils pkgconf-pkg-config \
            openssl-devel readline-devel ncurses-devel
        $SUDO dnf install -y valgrind || \
            echo "Warning: Valgrind install failed; build dependencies are installed."
    elif command -v yum >/dev/null 2>&1; then
        $SUDO yum install -y gcc make binutils pkgconfig \
            openssl-devel readline-devel ncurses-devel
        $SUDO yum install -y valgrind || \
            echo "Warning: Valgrind install failed; build dependencies are installed."
    elif command -v pacman >/dev/null 2>&1; then
        $SUDO pacman -S --needed --noconfirm base-devel pkgconf \
            openssl readline ncurses
        $SUDO pacman -S --needed --noconfirm valgrind || \
            echo "Warning: Valgrind install failed; build dependencies are installed."
    elif command -v zypper >/dev/null 2>&1; then
        $SUDO zypper --non-interactive install gcc make binutils \
            pkg-config libopenssl-devel readline-devel ncurses-devel
        $SUDO zypper --non-interactive install valgrind || \
            echo "Warning: Valgrind install failed; build dependencies are installed."
    elif command -v apk >/dev/null 2>&1; then
        $SUDO apk add build-base pkgconf openssl-dev readline-dev ncurses-dev
        $SUDO apk add valgrind || \
            echo "Warning: Valgrind install failed; build dependencies are installed."
    else
        echo "Unsupported Linux package manager." >&2
        echo "Install GCC, make, binutils, pkg-config, OpenSSL," >&2
        echo "Readline, and ncurses development headers." >&2
        exit 1
    fi
}

install_darwin() {
    echo "Installing tetriSH dependencies for macOS..."
    if ! xcrun --find cc >/dev/null 2>&1; then
        echo "Starting the Xcode Command Line Tools installer..."
        xcode-select --install
        echo "Finish that installer, then run make again." >&2
        exit 1
    fi
    if ! command -v brew >/dev/null 2>&1; then
        echo "Homebrew is required: https://brew.sh" >&2
        echo "Install it, then run make again." >&2
        exit 1
    fi
    brew install pkgconf openssl@3 readline ncurses

    # The server half of the project does not exist on macOS - tetrisd needs
    # epoll and timerfd, libcoreipc's mqueue module needs POSIX message queues,
    # and Darwin has neither - so a container engine is not a convenience here,
    # it is the only way to run a game server at all. Installed the way Valgrind
    # is on Linux: attempted, never fatal, because everything that *can* build
    # on macOS builds without it.
    #
    # colima and the docker CLI rather than the Docker Desktop cask: no GUI
    # installer, no admin password, and `colima start` is scriptable. A machine
    # that already has Docker Desktop is left alone.
    if command -v docker >/dev/null 2>&1; then
        echo "Container engine: docker is already installed."
    elif [ -d /Applications/Docker.app ]; then
        echo "Container engine: Docker Desktop is installed; start it once to add the CLI."
    else
        brew install colima docker || \
            echo "Warning: colima/docker install failed; the client still builds, but a server needs one."
    fi
}

case "$UNAME_S" in
    Linux)  install_linux ;;
    Darwin) install_darwin ;;
    *)
        echo "Unsupported operating system: $UNAME_S" >&2
        exit 1
        ;;
esac
