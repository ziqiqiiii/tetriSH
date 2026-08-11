#!/usr/bin/env bash
# Print a human-readable summary of the tetriSH root dependency situation:
# host OS, whether it is WSL, whether auto-install is on, the required packages,
# and how Valgrind is treated. Read-only — changes nothing on the system.
#
# Environment:
#   UNAME_S            host OS (uname -s); defaults to `uname -s` if unset
#   AUTO_INSTALL_DEPS  1 if `make` auto-installs missing deps (reported verbatim)
#   REQUIRE_VALGRIND   1 if a missing Valgrind is a hard failure on Linux

set -euo pipefail

UNAME_S="${UNAME_S:-$(uname -s)}"
AUTO_INSTALL_DEPS="${AUTO_INSTALL_DEPS:-1}"
REQUIRE_VALGRIND="${REQUIRE_VALGRIND:-0}"

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

if [ "$UNAME_S" = "Darwin" ]; then
    echo "Server: tetrisd cannot be built or run on macOS (needs epoll, timerfd, POSIX mqueue)"
    echo "Valgrind: use Linux/WSL for the mandatory memory-safety run"
elif [ "$REQUIRE_VALGRIND" = "1" ]; then
    echo "Valgrind: required by REQUIRE_VALGRIND=1"
else
    echo "Valgrind: optional for build, required for PR/checkoff memory-safety runs"
fi
