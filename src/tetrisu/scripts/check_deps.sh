#!/usr/bin/env bash
# Verify Tetrisu's renderer and optional audio development dependencies.

set -euo pipefail

CC="${CC:-gcc}"
PKG_CONFIG="${PKG_CONFIG:-pkg-config}"
AUDIO_DEPS="${AUDIO_DEPS:-sdl2 SDL2_mixer}"

for tool in "$CC" "$PKG_CONFIG"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "tetrisu: missing tool: $tool" >&2
        exit 1
    fi
done

if ! "$PKG_CONFIG" --exists notcurses; then
    echo "tetrisu: missing notcurses development files." >&2
    echo "Run 'make -C src/tetrisu deps' or install notcurses manually." >&2
    exit 1
fi

probe="$(mktemp /tmp/tetrisu-deps-check.XXXXXX)"
trap 'rm -f "$probe"' EXIT HUP INT TERM

printf '%s\n' \
    '#include <notcurses/notcurses.h>' \
    'int main(void) { return 0; }' \
    | "$CC" -x c - $("$PKG_CONFIG" --cflags --libs notcurses) -o "$probe"

if ! "$PKG_CONFIG" --exists $AUDIO_DEPS; then
    echo "Warning: Tetrisu audio is disabled; install SDL2 and SDL2_mixer development packages to enable it." >&2
fi
