#!/usr/bin/env bash
# tests/integration/test_loop.sh
#
# Verifies the shell loop: multiple commands, empty input, unknown commands,
# whitespace handling, and resilience.

set -euo pipefail

SHELL_BIN=./macmini_shell
FAIL=0

INPUT=$(printf "ld\n\n\nnotacommand_xyz\nld\nexit\n")
OUTPUT=$(echo "$INPUT" | timeout 3s $SHELL_BIN 2>&1)

# 1. Multiple commands in one session
COUNT=$(echo "$OUTPUT" | grep -c "files" || true)
if [ "$COUNT" -lt 2 ]; then
  echo "FAIL: ld should run at least twice, got $COUNT"
  echo "$OUTPUT"
  FAIL=1
fi

# 2. Unknown command produces error
if ! echo "$OUTPUT" | grep -F "not found" > /dev/null; then
  echo "FAIL: unknown command should print 'not found'"
  echo "$OUTPUT"
  FAIL=1
fi

# 3. Empty lines don't crash the shell
OUTPUT=$(printf "\n\n\necho survived\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "survived" > /dev/null; then
  echo "FAIL: empty lines should be tolerated"
  echo "$OUTPUT"
  FAIL=1
fi

# 4. Whitespace-only input doesn't crash
OUTPUT=$(printf "   \n\t\necho ok\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "ok" > /dev/null; then
  echo "FAIL: whitespace-only lines should be tolerated"
  echo "$OUTPUT"
  FAIL=1
fi

# 5. Many commands in sequence
INPUT=""
for i in $(seq 1 20); do
  INPUT="${INPUT}echo line${i}\n"
done
INPUT="${INPUT}exit\n"
OUTPUT=$(printf "$INPUT" | timeout 5s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "line20" > /dev/null; then
  echo "FAIL: shell should handle 20 commands in sequence"
  echo "$OUTPUT"
  FAIL=1
fi

# 6. Multiple unknown commands don't crash
OUTPUT=$(printf "fake1\nfake2\nfake3\necho alive\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "alive" > /dev/null; then
  echo "FAIL: shell should survive multiple unknown commands"
  echo "$OUTPUT"
  FAIL=1
fi

# 7. Alternating valid and invalid commands
OUTPUT=$(printf "echo a\nfakeXYZ\necho b\nfakeABC\necho c\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
for letter in a b c; do
  if ! echo "$OUTPUT" | grep -F "$letter" > /dev/null; then
    echo "FAIL: echo $letter should appear in interleaved valid/invalid sequence"
    echo "$OUTPUT"
    FAIL=1
    break
  fi
done

# 8. Builtin after external command
OUTPUT=$(printf "ld\npwd\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "/" > /dev/null; then
  echo "FAIL: pwd after ld should still work"
  echo "$OUTPUT"
  FAIL=1
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
echo "PASS: shell loops correctly with empty input, errors, and many commands"
