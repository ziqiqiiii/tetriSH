#!/usr/bin/env bash
# Print a human-readable summary of the tetriSH root dependency situation:
# host OS, whether it is WSL, whether auto-install is on, the required packages,
# and how Valgrind is treated. Read-only — changes nothing on the system.
#
# Environment:
#   UNAME_S            host OS (uname -s); defaults to `uname -s` if unset
#   AUTO_INSTALL_DEPS  1 if `make` auto-installs missing deps (reported verbatim)
#   REQUIRE_VALGRIND   1 if a missing Valgrind is a hard failure on Linux
#   REQUIRE_DOCKER     1 if a missing container engine is a hard failure
#   REQUIRE_KITTY      1 if a terminal that cannot draw the board is a hard failure

set -euo pipefail

UNAME_S="${UNAME_S:-$(uname -s)}"
AUTO_INSTALL_DEPS="${AUTO_INSTALL_DEPS:-1}"
REQUIRE_VALGRIND="${REQUIRE_VALGRIND:-0}"
REQUIRE_DOCKER="${REQUIRE_DOCKER:-0}"
REQUIRE_KITTY="${REQUIRE_KITTY:-0}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "OS: $UNAME_S"

if [ "$UNAME_S" = "Linux" ] \
        && grep -Eqi '(microsoft|wsl)' /proc/sys/kernel/osrelease \
            /proc/version 2>/dev/null; then
    echo "Environment: WSL"
else
    echo "Environment: native"
fi

echo "Auto-install: $AUTO_INSTALL_DEPS"
echo "Required: GCC, make, binutils, pkg-config, OpenSSL, Readline, ncurses"

# Reported in container.sh's own words rather than re-probed here, so the two
# never disagree about what counts as a usable engine.
if [ -f "$SCRIPT_DIR/container.sh" ]; then
    engine="$(bash "$SCRIPT_DIR/container.sh" check 2>/dev/null || true)"
    echo "Container engine: ${engine#container engine: }"
    if [ "$REQUIRE_DOCKER" = "1" ]; then
        echo "                  required by REQUIRE_DOCKER=1"
    else
        echo "                  optional for build, runs the client for 'make play'"
    fi
fi

# Likewise terminal.sh's own words, which carry the version verdict and not just
# a path - the whole point being that an installed kitty is not necessarily one
# that can draw.
if [ -f "$SCRIPT_DIR/terminal.sh" ]; then
    terminal="$(bash "$SCRIPT_DIR/terminal.sh" check 2>/dev/null || true)"
    echo "Terminal: ${terminal#terminal: }"
    if [ "$REQUIRE_KITTY" = "1" ]; then
        echo "          required by REQUIRE_KITTY=1"
    else
        echo "          optional for build, draws the board for 'make play'"
    fi
fi

if [ "$UNAME_S" = "Darwin" ]; then
    echo "Server: tetrisd cannot be built or run on macOS (needs epoll, timerfd, POSIX mqueue)"
    echo "Valgrind: use Linux/WSL for the mandatory memory-safety run"
elif [ "$REQUIRE_VALGRIND" = "1" ]; then
    echo "Valgrind: required by REQUIRE_VALGRIND=1"
else
    echo "Valgrind: optional for build, required for PR/checkoff memory-safety runs"
fi
