#!/usr/bin/env bash
# Install tetrisu render/audio dependencies (notcurses, SDL2, SDL2_mixer) plus
# pkg-config, for the host's package manager. Root deps (compiler, OpenSSL, ...)
# are the umbrella Makefile's job; this script owns only the client-only packages.
#
# Environment:
#   AUTO_INSTALL_DEPS              (unused here; gating lives in deps.sh)
#   INSTALL_NOTCURSES_FROM_SOURCE  1 to build notcurses from source when no
#                                  distro development package is available (Linux)
#   NOTCURSES_VERSION              git tag to build when falling back to source
#
# Privilege is resolved once, at the top; sudo performs its own password prompt.

set -euo pipefail

INSTALL_NOTCURSES_FROM_SOURCE="${INSTALL_NOTCURSES_FROM_SOURCE:-1}"
NOTCURSES_VERSION="${NOTCURSES_VERSION:-v3.0.17}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

UNAME_S="$(uname -s)"

resolve_sudo() {
    if [ "$(id -u)" -eq 0 ]; then
        SUDO=""
    elif command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    else
        echo "Root access or sudo is required to install tetrisu packages." >&2
        exit 1
    fi
}

install_linux() {
    echo "Installing tetrisu render/audio dependencies for Linux..."
    resolve_sudo

    if command -v apt-get >/dev/null 2>&1; then
        $SUDO apt-get update
        $SUDO apt-get install -y pkg-config libsdl2-dev libsdl2-mixer-dev

        NOTCURSES_PKG=""
        for pkg in libnotcurses-dev notcurses-dev; do
            if apt-cache show "$pkg" >/dev/null 2>&1; then
                NOTCURSES_PKG="$pkg"
                break
            fi
        done

        # Ubuntu keeps notcurses in the universe component; enable it and retry.
        if [ -z "$NOTCURSES_PKG" ] && [ -r /etc/os-release ]; then
            . /etc/os-release
            if [ "${ID:-}" = "ubuntu" ]; then
                echo "Ubuntu keeps notcurses in universe; enabling universe if needed..."
                if ! command -v add-apt-repository >/dev/null 2>&1; then
                    $SUDO apt-get install -y software-properties-common
                fi
                $SUDO add-apt-repository -y universe || true
                $SUDO apt-get update
                for pkg in libnotcurses-dev notcurses-dev; do
                    if apt-cache show "$pkg" >/dev/null 2>&1; then
                        NOTCURSES_PKG="$pkg"
                        break
                    fi
                done
            fi
        fi

        if [ -n "$NOTCURSES_PKG" ]; then
            $SUDO apt-get install -y "$NOTCURSES_PKG"
        elif [ "$INSTALL_NOTCURSES_FROM_SOURCE" = "1" ]; then
            echo "No APT notcurses development package found; building notcurses $NOTCURSES_VERSION from source."
            $SUDO apt-get install -y git cmake libavdevice-dev \
                libdeflate-dev libgpm-dev libswscale-dev libunistring-dev
            NOTCURSES_VERSION="$NOTCURSES_VERSION" bash "$SCRIPT_DIR/install_notcurses.sh"
        else
            echo "No notcurses development package is available in configured APT repositories." >&2
            echo "Enable Ubuntu universe/Debian testing, install notcurses manually, or run with INSTALL_NOTCURSES_FROM_SOURCE=1." >&2
            exit 1
        fi
    elif command -v dnf >/dev/null 2>&1; then
        $SUDO dnf install -y pkgconf-pkg-config notcurses-devel \
            SDL2-devel SDL2_mixer-devel || {
            echo "Enable Fedora/EPEL/CRB repos or install tetrisu render/audio packages manually." >&2
            exit 1
        }
    elif command -v yum >/dev/null 2>&1; then
        $SUDO yum install -y pkgconfig notcurses-devel \
            SDL2-devel SDL2_mixer-devel || {
            echo "Enable EPEL/CRB repos or install tetrisu render/audio packages manually." >&2
            exit 1
        }
    elif command -v pacman >/dev/null 2>&1; then
        # sdl2-compat (common on Arch, pulled in by ffmpeg/wine/etc.) provides
        # sdl2 and conflicts with the real sdl2 package; don't force sdl2
        # explicitly, let whichever provider is already installed satisfy it.
        $SUDO pacman -S --needed --noconfirm pkgconf notcurses sdl2_mixer
    elif command -v zypper >/dev/null 2>&1; then
        $SUDO zypper --non-interactive install pkg-config \
            notcurses-devel libSDL2-devel libSDL2_mixer-devel || {
            echo "Enable needed openSUSE repos or install tetrisu render/audio packages manually." >&2
            exit 1
        }
    elif command -v apk >/dev/null 2>&1; then
        $SUDO apk add pkgconf notcurses-dev sdl2-dev sdl2_mixer-dev
    else
        echo "Unsupported Linux package manager." >&2
        echo "Install pkg-config, notcurses, SDL2, and SDL2_mixer development headers." >&2
        exit 1
    fi
}

install_darwin() {
    echo "Installing tetrisu render/audio dependencies for macOS..."
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
    brew install pkgconf notcurses sdl2 sdl2_mixer
}

case "$UNAME_S" in
    Linux)  install_linux ;;
    Darwin) install_darwin ;;
    *)
        echo "Unsupported operating system: $UNAME_S" >&2
        exit 1
        ;;
esac
