#!/usr/bin/env bash
# Check, optionally install, then re-check Tetrisu-only dependencies.
#
# Audio gets its own step rather than riding the required-dependency check,
# because a warning-level dependency never reaches the install path: SDL2 is
# optional, so check_deps.sh warns about it and still exits 0, the probe below
# passes on a machine with notcurses and no SDL2, and the install is skipped —
# which is how a client built here came out with -DTETRISU_ENABLE_AUDIO=0 and
# ran silently. The same shape as the container engine's step in the root
# scripts/deps.sh, for the same reason.
#
# It installs through `install_deps.sh audio`, not the full script: notcurses
# is already satisfied here by definition, and the full path would re-enter its
# branch and rebuild it from source over a newer hand-built install.

set -euo pipefail

AUTO_INSTALL_DEPS="${AUTO_INSTALL_DEPS:-1}"
AUDIO_DEPS="${AUDIO_DEPS:-sdl2 SDL2_mixer}"
PKG_CONFIG="${PKG_CONFIG:-pkg-config}"
GREEN="${GREEN:-}"
CLR_RMV="${CLR_RMV:-}"
UNAME_S="${UNAME_S:-$(uname -s)}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

check_deps() {
    CC="${CC:-gcc}" PKG_CONFIG="$PKG_CONFIG" \
        NOTCURSES_MIN_VERSION="${NOTCURSES_MIN_VERSION:-3.0.5}" \
        bash "$SCRIPT_DIR/check_deps.sh"
}

# Optional, so its absence is never an error here — only a missed install.
check_audio() {
    "$PKG_CONFIG" --exists $AUDIO_DEPS 2>/dev/null
}

install_all() {
    INSTALL_NOTCURSES_FROM_SOURCE="${INSTALL_NOTCURSES_FROM_SOURCE:-1}" \
    NOTCURSES_VERSION="${NOTCURSES_VERSION:-v3.0.12}" \
    NOTCURSES_MIN_VERSION="${NOTCURSES_MIN_VERSION:-3.0.5}" \
    NOTCURSES_PREFIX="${NOTCURSES_PREFIX:-/usr/local}" \
        bash "$SCRIPT_DIR/install_deps.sh"
}

ready() {
    printf '%b\n' "${GREEN}tetrisu dependencies ready${CLR_RMV} (${UNAME_S})."
    exit 0
}

if check_deps >/dev/null 2>&1; then
    if check_audio; then
        ready
    fi

    # Renderer is fine and only audio is missing: install that alone, and let a
    # failure stay a warning. A machine with no SDL2 packages still builds and
    # plays, it just does it in silence.
    if [ "$AUTO_INSTALL_DEPS" = "1" ]; then
        bash "$SCRIPT_DIR/install_deps.sh" audio \
            || echo "tetrisu: could not install SDL2; audio stays disabled." >&2
    fi
    check_deps
    ready
fi

if [ "$AUTO_INSTALL_DEPS" != "1" ]; then
    check_deps
    echo "tetrisu: missing dependencies; automatic installation is disabled." >&2
    exit 1
fi

install_all
check_deps
ready
