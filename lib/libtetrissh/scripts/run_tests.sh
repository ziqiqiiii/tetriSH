#!/bin/sh
set -eu

pass=0
fail=0

for test_bin in "$@"; do
	printf '%s\n' "==> $test_bin"
	if "$test_bin"; then
		pass=$((pass + 1))
	else
		fail=$((fail + 1))
	fi
done

printf '\nlibtetrissh tests: %d passed, %d failed\n' "$pass" "$fail"
test "$fail" -eq 0
