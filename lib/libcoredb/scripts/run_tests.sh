#!/usr/bin/env bash
# Formatted runner for libcoredb unit tests.

set -euo pipefail

MODE="${1:?Usage: $0 unit <test ...>}"
shift

FILTER="${FILTER:-}"

GRN=$(printf '\033[1;32m')
RED=$(printf '\033[1;31m')
YEL=$(printf '\033[1;33m')
CYN=$(printf '\033[1;36m')
RST=$(printf '\033[0m')

pass=0
fail=0

run_unit() {
  local skipped=0
  for t in "$@"; do
    if [ -n "$FILTER" ] && [[ "$t" != *"$FILTER"* ]]; then
      skipped=$((skipped + 1))
      continue
    fi
    echo "${YEL}--- ${t} ---${RST}"
    output=$("$t" 2>&1) && rc=0 || rc=$?
    echo "$output" | awk -v grn="$GRN" -v red="$RED" -v rst="$RST" '
      /^PASS / { printf "  %-50s %sPASS%s\n", substr($0, 6), grn, rst; next }
      /^FAIL / { printf "  %-50s %sFAIL%s\n", substr($0, 6), red, rst; next }
      { print "  " $0 }
    '
    if [ "$rc" -eq 0 ]; then
      pass=$((pass + 1))
    else
      fail=$((fail + 1))
      echo "  ${RED}^^^ FAILED: ${t} (exit code ${rc})${RST}"
      echo ""
    fi
  done
  if [ -n "$FILTER" ] && [ "$skipped" -gt 0 ]; then
    echo "${CYN}($skipped test(s) skipped by FILTER=\"${FILTER}\")${RST}"
  fi
}

case "$MODE" in
  unit) run_unit "$@" ;;
  *) echo "Unknown mode: $MODE" >&2; exit 1 ;;
esac

if [ "$fail" -eq 0 ]; then
  echo "${GRN}Unit tests: ${pass} passed, ${fail} failed${RST}"
else
  echo "${RED}Unit tests: ${pass} passed, ${fail} failed${RST}"
fi
test "$fail" -eq 0
