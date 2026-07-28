#!/usr/bin/env bash

set -euo pipefail

mode="${1:?Usage: $0 unit <test binaries...>}"
shift

if [ "$mode" != "unit" ]; then
	echo "Unsupported test mode: $mode" >&2
	exit 1
fi

passed=0
for test_bin in "$@"; do
	echo "--- $test_bin ---"
	"$test_bin"
	passed=$((passed + 1))
done

echo "Unit tests: $passed passed, 0 failed"
