#!/bin/sh
set -eu

root_makefile="Makefile"
tetrisu_makefile="src/tetrisu/Makefile"

fail() {
	printf '%s\n' "dependency policy: $*" >&2
	exit 1
}

if grep -Eqi 'sqlite|sqlite3' "$root_makefile"; then
	fail "root Makefile must not mention SQLite"
fi

if grep -Eqi 'notcurses|sdl2|SDL2_mixer' "$root_makefile"; then
	fail "root Makefile must not own tetrisu-only dependencies"
fi

grep -Eq '^install-deps:' "$tetrisu_makefile" \
	|| fail "tetrisu Makefile needs install-deps target"
grep -Eq 'notcurses' "$tetrisu_makefile" \
	|| fail "tetrisu Makefile must manage notcurses"
grep -Eq 'sdl2|SDL2_mixer' "$tetrisu_makefile" \
	|| fail "tetrisu Makefile must manage SDL audio deps"

printf '%s\n' "dependency policy: ok"
