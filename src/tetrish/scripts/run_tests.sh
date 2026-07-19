#!/usr/bin/env bash
# Generic test runner with coloured, formatted output.
#
# Usage:
#   scripts/run_tests.sh unit   <bin1> <bin2> ...    # run compiled test binaries
#   scripts/run_tests.sh integration <script1> ...   # run shell test scripts
#
# Filtering:
#   FILTER=<pattern>  — only run tests whose path contains <pattern>
#   e.g.  make unit FILTER=lexer
#         make unit FILTER=04
#         make integration FILTER=system
#
# Exit code: 0 if all pass, 1 if any fail.

set -euo pipefail

MODE="${1:?Usage: $0 unit|integration <test ...>}"
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
      skipped=$((skipped+1))
      continue
    fi
    echo "${YEL}--- ${t} ---${RST}"
    echo ""
    output=$("$t" 2>&1) && rc=0 || rc=$?
    echo "$output" | awk -v grn="$GRN" -v red="$RED" -v rst="$RST" -v cyn="$CYN" -F: '
      /^-{3,}/ || /^OK$/ || /^FAIL$/ { next }
      NF >= 4 && $4 == "PASS" { printf "  %-50s %sPASS%s\n", $3, grn, rst; next }
      NF >= 4 && $4 == "FAIL" {
        detail = ""
        for (i = 5; i <= NF; i++) detail = detail (i > 5 ? ":" : "") $i
        printf "  %-50s %sFAIL%s\n", $3, red, rst
        if (detail != "") printf "    %s→ %s:%s:%s%s\n", red, $1, $2, detail, rst
        next
      }
      /^[0-9]+.*Tests.*Failures.*Ignored/ { next }
      /^[0-9]/ { printf "\n%s  %s%s\n\n", cyn, $0, rst }
      { print "  " $0 }
    '
    if [ "$rc" -eq 0 ]; then
      pass=$((pass+1))
    else
      fail=$((fail+1))
      echo "  ${RED}^^^ FAILED: ${t} (exit code ${rc})${RST}"
      echo ""
    fi
  done
  if [ -n "$FILTER" ] && [ "$skipped" -gt 0 ]; then
    echo "${CYN}($skipped test(s) skipped by FILTER=\"${FILTER}\")${RST}"
  fi
}

run_integration() {
  
  local skipped=0
  for s in "$@"; do
    [ -f "$s" ] || continue
    if [ -n "$FILTER" ] && [[ "$s" != *"$FILTER"* ]]; then
      skipped=$((skipped+1))
      continue
    fi
    echo ""
    echo "${YEL}--- ${s} ---${RST}"
    output=$(bash "$s" 2>&1) && rc=0 || rc=$?
    echo "$output" | awk -v grn="$GRN" -v red="$RED" -v rst="$RST" -v cyn="$CYN" '
      /^PASS: / || /^FAIL: / {
        if ($0 ~ /^PASS:/) { tag = "PASS"; clr = grn } else { tag = "FAIL"; clr = red }
        d = substr($0, 7)
        if (length(d) <= 50) {
          printf "  %-50s %s%s%s\n", d, clr, tag, rst
        } else {
          n = split(d, words, " ")
          line = ""
          for (i = 1; i <= n; i++) {
            tl = (line == "" ? words[i] : line " " words[i])
            if (length(tl) > 50 && line != "") {
              printf "  %s\n", line
              line = words[i]
            } else {
              line = tl
            }
          }
          printf "  %-50s %s%s%s\n", line, clr, tag, rst
        }
        next
      }
      { print }
    '
    if [ "$rc" -eq 0 ]; then
      pass=$((pass+1))
    else
      fail=$((fail+1))
      echo "  ${RED}^^^ FAILED: ${s} (exit code ${rc})${RST}"
      echo ""
    fi
  done
  if [ -n "$FILTER" ] && [ "$skipped" -gt 0 ]; then
    echo "${CYN}($skipped test(s) skipped by FILTER=\"${FILTER}\")${RST}"
  fi
}

case "$MODE" in
  unit)        run_unit "$@" ;;
  integration) run_integration "$@" ;;
  *)           echo "Unknown mode: $MODE" >&2; exit 1 ;;
esac

echo ""
LABEL="$(echo "$MODE" | sed 's/.*/\u&/') tests"
if [ -n "$FILTER" ] && [ "$pass" -eq 0 ] && [ "$fail" -eq 0 ]; then
  echo "${YEL}${LABEL}: no tests matched FILTER=\"${FILTER}\"${RST}"
  exit 1
elif [ "$fail" -eq 0 ]; then
  echo "${GRN}${LABEL}: ${pass} passed, ${fail} failed${RST}"
else
  echo "${RED}${LABEL}: ${pass} passed, ${fail} failed${RST}"
fi
test "$fail" -eq 0
