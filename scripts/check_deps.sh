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
trap 'rm -f "$probe"' EXIT HUP INT TERM

missing=""
for tool in gcc make ar pkg-config; do
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
