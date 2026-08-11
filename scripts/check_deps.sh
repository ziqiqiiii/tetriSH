#!/usr/bin/env bash
# Verify the tetriSH root dependencies without changing the system: check the
# required tools are on PATH, then compile and link one small program against
# OpenSSL, Readline, and ncurses. Compiling beats trusting the package database
# — it catches missing headers, libraries, and unusable pkg-config search paths.
#
# Environment:
#   UNAME_S            host OS (uname -s); defaults to `uname -s` if unset
#   REQUIRE_VALGRIND   1 to treat a missing Valgrind as a hard failure on Linux
#                      (otherwise it is only a warning)
#
# Exit code: 0 if all required dependencies are present and the probe links.

set -euo pipefail

UNAME_S="${UNAME_S:-$(uname -s)}"
REQUIRE_VALGRIND="${REQUIRE_VALGRIND:-0}"

probe="$(mktemp /tmp/tetrish-deps-check.XXXXXX)"
probe_log="$probe.log"
trap 'rm -f "$probe" "$probe_log"' EXIT HUP INT TERM

missing=""
for tool in gcc as make ar pkg-config; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        missing="$missing $tool"
    fi
done

if [ "$UNAME_S" = "Linux" ] \
        && [ "$REQUIRE_VALGRIND" = "1" ] \
        && ! command -v valgrind >/dev/null 2>&1; then
    missing="$missing valgrind"
fi

if [ -n "$missing" ]; then
    echo "Missing tools:$missing" >&2
    exit 1
fi

# The assembler has to understand what the compiler emits, and on a
# partially-upgraded box it does not: GCC 15 encodes non-ASCII string constants
# with the `.base64` directive, which GNU as only learned in binutils 2.44. A
# GCC that has outrun its binutils compiles most of the tree and then dies on
# the one file that draws a box — notcurses' ncplane_perimeter_* inlines carry
# the box-drawing characters — with "unknown pseudo-op: `.base64'". Probing the
# pair here turns that into one sentence at the start of the build.
if ! printf '%s\n' \
        'const char *tetrish_box_probe(void);' \
        'const char *tetrish_box_probe(void) { return "╔╗╚╝═║╭╮╰╯─│"; }' \
        | gcc -x c - -c -o /dev/null 2>"$probe_log"; then
    if grep -q 'base64' "$probe_log"; then
        echo "The assembler cannot assemble what this GCC emits." >&2
        echo "  gcc: $(gcc -dumpfullversion 2>/dev/null || echo unknown)" >&2
        echo "  as:  $(command -v as) - $(as --version 2>/dev/null | head -1)" >&2
        echo "GCC 15 writes non-ASCII string constants as '.base64', which needs" >&2
        echo "binutils 2.44 or newer. Upgrade the assembler, e.g. on Debian/Kali:" >&2
        echo "  sudo apt-get update && sudo apt-get install --only-upgrade binutils" >&2
    else
        echo "The C toolchain cannot compile a trivial file:" >&2
        cat "$probe_log" >&2
    fi
    exit 1
fi

pkg-config --exists openssl readline ncursesw || {
    echo "Missing development packages: OpenSSL, Readline, or ncurses." >&2
    exit 1
}

printf '%s\n' \
    '#include <openssl/evp.h>' \
    '#include <readline/readline.h>' \
    '#include <ncurses.h>' \
    'int main(void) { return 0; }' \
    | gcc -x c - $(pkg-config --cflags --libs openssl readline ncursesw) -o "$probe"

if [ "$UNAME_S" = "Linux" ] \
        && [ "$REQUIRE_VALGRIND" != "1" ] \
        && ! command -v valgrind >/dev/null 2>&1; then
    echo "Warning: Valgrind is not installed. Build is OK, but PR/checkoff memory-safety runs need it."
fi

if [ "$UNAME_S" = "Darwin" ]; then
    echo "Note: tetrisd cannot be built or run on macOS - it needs epoll, timerfd,"
    echo "      and POSIX mqueue, none of which Darwin has. 'make play' builds"
    echo "      and runs tetrisu only; play against a server on another machine"
    echo "      with 'bash scripts/play.sh --host ADDR'."
fi
