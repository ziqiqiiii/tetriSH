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
#   GREEN / CLR_RMV    colour escapes (defined once in the Makefile); optional

set -euo pipefail

UNAME_S="${UNAME_S:-$(uname -s)}"
AUTO_INSTALL_DEPS="${AUTO_INSTALL_DEPS:-1}"
REQUIRE_VALGRIND="${REQUIRE_VALGRIND:-0}"
REQUIRE_DOCKER="${REQUIRE_DOCKER:-0}"
GREEN="${GREEN:-}"
CLR_RMV="${CLR_RMV:-}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

run_check() {
    UNAME_S="$UNAME_S" REQUIRE_VALGRIND="$REQUIRE_VALGRIND" \
        REQUIRE_DOCKER="$REQUIRE_DOCKER" \
        bash "$SCRIPT_DIR/check_deps.sh"
}

run_install() {
    UNAME_S="$UNAME_S" bash "$SCRIPT_DIR/install_deps.sh"
}

if run_check >/dev/null 2>&1; then
    printf '%b\n' "${GREEN}Dependencies ready${CLR_RMV} ($UNAME_S)."
elif [ "$AUTO_INSTALL_DEPS" = "1" ]; then
    run_install
    run_check
else
    # Re-run the check unsilenced so the specific missing dependency is shown.
    run_check
    echo "Missing dependencies; automatic installation is disabled." >&2
    exit 1
fi
