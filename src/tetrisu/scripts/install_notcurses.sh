#!/usr/bin/env bash
# Build and install notcurses from source, as a Linux fallback when no distro
# development package is available. macOS uses Homebrew, so this refuses to run
# there. Called by install_deps.sh (and directly by the Makefile target).
#
# Environment:
#   NOTCURSES_VERSION  git tag to build (default v3.0.12, the newest release
#                       supported by Debian 11's CMake 3.18)
#   NOTCURSES_PREFIX   install prefix (default /usr/local)
#
# Privilege is resolved once, at the top; sudo performs its own password prompt.

set -euo pipefail

NOTCURSES_VERSION="${NOTCURSES_VERSION:-v3.0.12}"
NOTCURSES_PREFIX="${NOTCURSES_PREFIX:-/usr/local}"

if [ "$(uname -s)" != "Linux" ]; then
    echo "Source fallback is only needed on Linux; use Homebrew on macOS." >&2
    exit 1
fi

for tool in git cmake; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing $tool; install it before building notcurses from source." >&2
        exit 1
    fi
done

if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
elif command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
else
    echo "Root access or sudo is required to install source-built notcurses." >&2
    exit 1
fi

tmp="$(mktemp -d /tmp/notcurses-src.XXXXXX)"
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf 2)"

git clone --depth 1 --branch "$NOTCURSES_VERSION" \
    https://github.com/dankamongmen/notcurses.git "$tmp/notcurses"

cmake -S "$tmp/notcurses" -B "$tmp/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$NOTCURSES_PREFIX" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DUSE_MULTIMEDIA=ffmpeg \
    -DUSE_DOCTEST=off \
    -DUSE_PANDOC=off \
    -DUSE_QRCODEGEN=off \
    -DBUILD_TESTING=off

cmake --build "$tmp/build" --parallel "$jobs"
$SUDO cmake --install "$tmp/build"

if command -v ldconfig >/dev/null 2>&1; then
    $SUDO ldconfig
fi
