#!/usr/bin/env bash
# Orchestrate the tetriSH root dependency flow: probe for the dependencies and,
# if they are missing, install them (unless auto-install is disabled). `make`
# installs only when the compile/link probe fails; set AUTO_INSTALL_DEPS=0 in CI
# or managed environments to make this check-only.
#
# Delegates to the sibling scripts (check_deps.sh / install_deps.sh) rather than
# re-entering make. Environment:
#   UNAME_S            host OS (uname -s); forwarded to the check/install scripts
#   AUTO_INSTALL_DEPS  1 to install on a failed probe, 0 to only report
#   REQUIRE_VALGRIND   forwarded to the check
#   REQUIRE_DOCKER     forwarded to the check
#   REQUIRE_KITTY      forwarded to the check
#   GREEN / CLR_RMV    colour escapes (defined once in the Makefile); optional

set -euo pipefail

UNAME_S="${UNAME_S:-$(uname -s)}"
AUTO_INSTALL_DEPS="${AUTO_INSTALL_DEPS:-1}"
REQUIRE_VALGRIND="${REQUIRE_VALGRIND:-0}"
REQUIRE_DOCKER="${REQUIRE_DOCKER:-0}"
REQUIRE_KITTY="${REQUIRE_KITTY:-0}"
GREEN="${GREEN:-}"
CLR_RMV="${CLR_RMV:-}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

run_check() {
    UNAME_S="$UNAME_S" REQUIRE_VALGRIND="$REQUIRE_VALGRIND" \
        REQUIRE_DOCKER="$REQUIRE_DOCKER" REQUIRE_KITTY="$REQUIRE_KITTY" \
        bash "$SCRIPT_DIR/check_deps.sh"
}

run_install() {
    UNAME_S="$UNAME_S" bash "$SCRIPT_DIR/install_deps.sh"
}

# The container engine gets its own step rather than riding install_deps.sh,
# because it is a warning-level dependency and warnings never reach the install
# path: the check above passes without an engine, so nothing would ever install
# one. Asking for it separately is what makes `make deps` actually produce an
# engine on a machine whose toolchain is already complete.
#
# Never fatal on its own - REQUIRE_DOCKER=1 is how a caller demands one, and it
# does so through run_check. A server-only box and CI have no use for an engine
# and must not be stopped by the absence of one.
#
# WANT_ENGINE=0 turns the step off for callers that are certain they do not
# need it. `make play` is the one that matters: it builds and runs the client
# on this host, so pulling in an engine - and asking for the group membership
# that makes one usable - would be installing a container runtime as a side
# effect of a build that never opens a container.
run_engine() {
    [ "${WANT_ENGINE:-1}" = "1" ] || return 0
    bash "$SCRIPT_DIR/container.sh" check >/dev/null 2>&1 && return 0
    [ "$AUTO_INSTALL_DEPS" = "1" ] || return 0
    AUTO_INSTALL_DEPS=1 bash "$SCRIPT_DIR/container.sh" install || true
}

# The terminal is the engine's twin and gets its own step for the same reason:
# it draws the board and compiles nothing, so run_check passes without one and
# nothing on the install path would ever produce one.
#
# What a presence probe would miss is that this is a *version* question. kitty
# is routinely already installed and still unable to draw - too old to parse the
# graphics protocol (Ubuntu 20.04 packages a 2019 build) or too new for the
# machine's glibc to start - so the step asks terminal.sh, which knows the range
# and repairs a kitty outside it rather than only reporting one.
#
# WANT_TERMINAL=0 turns the step off. `make play` sets it: play.sh runs the same
# check itself moments later, with the messaging its own failure paths need.
run_terminal() {
    [ "${WANT_TERMINAL:-1}" = "1" ] || return 0
    bash "$SCRIPT_DIR/terminal.sh" check >/dev/null 2>&1 && return 0
    [ "$AUTO_INSTALL_DEPS" = "1" ] || return 0
    AUTO_INSTALL_DEPS=1 bash "$SCRIPT_DIR/terminal.sh" install || true
}

if run_check >/dev/null 2>&1; then
    run_engine
    run_terminal
    printf '%b\n' "${GREEN}Dependencies ready${CLR_RMV} ($UNAME_S)."
elif [ "$AUTO_INSTALL_DEPS" = "1" ]; then
    run_install
    run_engine
    run_terminal
    run_check
else
    # Re-run the check unsilenced so the specific missing dependency is shown.
    run_check
    echo "Missing dependencies; automatic installation is disabled." >&2
    exit 1
fi
