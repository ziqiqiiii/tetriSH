#!/usr/bin/env bash
# Generic formatted test runner with optional path-substring filtering.

set -euo pipefail

mode="${1:?Usage: $0 unit <test ...>}"
shift
filter="${FILTER:-}"

green=$(printf '\033[1;32m')
red=$(printf '\033[1;31m')
yellow=$(printf '\033[1;33m')
cyan=$(printf '\033[1;36m')
reset=$(printf '\033[0m')

passed=0
failed=0
skipped=0

run_unit()
{
	local test_bin
	local output
	local status

	for test_bin in "$@"; do
		if [ -n "$filter" ] && [[ "$test_bin" != *"$filter"* ]]; then
			skipped=$((skipped + 1))
			continue
		fi
		printf '%s--- %s ---%s\n' "$yellow" "$test_bin" "$reset"
		if output=$("$test_bin" 2>&1); then
			status=0
		else
			status=$?
		fi
		while IFS= read -r line; do
			case "$line" in
				"PASS "*) printf '  %-50s %sPASS%s\n' \
					"${line#PASS }" "$green" "$reset" ;;
				"FAIL "*) if [ "$status" -eq 0 ]; then
						status=1
					fi
					printf '  %-50s %sFAIL%s\n' \
						"${line#FAIL }" "$red" "$reset" ;;
				*) printf '  %s\n' "$line" ;;
			esac
		done <<< "$output"
		printf '\n'
		if [ "$status" -eq 0 ]; then
			passed=$((passed + 1))
		else
			failed=$((failed + 1))
			printf '  %sFAILED: %s (exit code %d)%s\n' \
				"$red" "$test_bin" "$status" "$reset"
		fi
	done
}

case "$mode" in
	unit) run_unit "$@" ;;
	*) printf 'Unknown mode: %s\n' "$mode" >&2; exit 1 ;;
esac

if [ -n "$filter" ] && [ "$skipped" -gt 0 ]; then
	printf '%s(%d test(s) skipped by FILTER="%s")%s\n' \
		"$cyan" "$skipped" "$filter" "$reset"
fi
if [ "$passed" -eq 0 ] && [ "$failed" -eq 0 ]; then
	printf '%sUnit tests: no tests matched FILTER="%s"%s\n' \
		"$yellow" "$filter" "$reset"
	exit 1
fi
if [ "$failed" -eq 0 ]; then
	printf '%sUnit tests: %d passed, %d failed%s\n' \
		"$green" "$passed" "$failed" "$reset"
else
	printf '%sUnit tests: %d passed, %d failed%s\n' \
		"$red" "$passed" "$failed" "$reset"
fi
test "$failed" -eq 0
