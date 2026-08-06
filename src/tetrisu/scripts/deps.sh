#!/usr/bin/env bash
# Check, optionally install, then re-check Tetrisu-only dependencies.

set -euo pipefail

AUTO_INSTALL_DEPS="${AUTO_INSTALL_DEPS:-1}"
GREEN="${GREEN:-}"
CLR_RMV="${CLR_RMV:-}"
UNAME_S="${UNAME_S:-$(uname -s)}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

check_deps() {
    CC="${CC:-gcc}" PKG_CONFIG="${PKG_CONFIG:-pkg-config}" \
        bash "$SCRIPT_DIR/check_deps.sh"
}

if check_deps >/dev/null 2>&1; then
    printf '%b\n' "${GREEN}tetrisu dependencies ready${CLR_RMV} (${UNAME_S})."
    exit 0
fi

if [ "$AUTO_INSTALL_DEPS" != "1" ]; then
    check_deps
    echo "tetrisu: missing dependencies; automatic installation is disabled." >&2
    exit 1
fi

INSTALL_NOTCURSES_FROM_SOURCE="${INSTALL_NOTCURSES_FROM_SOURCE:-1}" \
NOTCURSES_VERSION="${NOTCURSES_VERSION:-v3.0.17}" \
NOTCURSES_PREFIX="${NOTCURSES_PREFIX:-/usr/local}" \
    bash "$SCRIPT_DIR/install_deps.sh"
check_deps
printf '%b\n' "${GREEN}tetrisu dependencies ready${CLR_RMV} (${UNAME_S})."
