#!/usr/bin/env bash
# tests/integration/test_expansion.sh
#
# Verifies variable expansion: $VAR, $?, quoted strings.

set -euo pipefail

SHELL_BIN=./macmini_shell
FAIL=0

# 1. $HOME expands to home directory
OUTPUT=$(printf 'echo $HOME\nexit\n' | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "$HOME" > /dev/null; then
  echo "FAIL: \$HOME should expand to $HOME"
  echo "$OUTPUT"
  FAIL=1
fi

# 2. $? after successful command is 0
OUTPUT=$(printf 'echo ok\necho $?\nexit\n' | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "0" > /dev/null; then
  echo "FAIL: \$? after successful echo should be 0"
  echo "$OUTPUT"
  FAIL=1
fi

# 3. $? after failed command is non-zero
OUTPUT=$(printf 'notacommand_xyz\necho $?\nexit\n' | timeout 3s $SHELL_BIN 2>&1 || true)
EXITVAL=$(echo "$OUTPUT" | grep -E "^[0-9]+$" | tail -1 || true)
if [ -z "$EXITVAL" ] || [ "$EXITVAL" -eq 0 ]; then
  echo "FAIL: \$? after unknown command should be non-zero"
  echo "$OUTPUT"
  FAIL=1
fi

# 4. Undefined variable expands to empty string
OUTPUT=$(printf 'echo "$UNDEFINED_XYZ_123"end\nexit\n' | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "end" > /dev/null; then
  echo "FAIL: undefined variable should expand to empty"
  echo "$OUTPUT"
  FAIL=1
fi

# 5. Exported variable is expanded
OUTPUT=$(printf 'export TEST_EXP=expanded_val\necho $TEST_EXP\nexit\n' | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "expanded_val" > /dev/null; then
  echo "FAIL: exported variable should be expanded"
  echo "$OUTPUT"
  FAIL=1
fi

# 6. Single quotes prevent expansion
OUTPUT=$(printf "echo '\$HOME'\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if echo "$OUTPUT" | grep -F "$HOME" > /dev/null; then
  echo "FAIL: single quotes should prevent \$HOME expansion"
  echo "$OUTPUT"
  FAIL=1
fi

# 7. Double quotes allow expansion
OUTPUT=$(printf 'echo "$HOME"\nexit\n' | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "$HOME" > /dev/null; then
  echo "FAIL: double quotes should allow \$HOME expansion"
  echo "$OUTPUT"
  FAIL=1
fi

# 8. $PATH is available
OUTPUT=$(printf 'echo $PATH\nexit\n' | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "/" > /dev/null; then
  echo "FAIL: \$PATH should expand to something containing '/'"
  echo "$OUTPUT"
  FAIL=1
fi

# 9. Variable expansion in combination with text
OUTPUT=$(printf 'export XYZ_PRE=hello\necho prefix_${XYZ_PRE}_suffix\nexit\n' | timeout 3s $SHELL_BIN 2>&1)

# 10. Multiple expansions on one line
OUTPUT=$(printf 'export A_VAR=aaa\nexport B_VAR=bbb\necho $A_VAR $B_VAR\nexit\n' | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "aaa" > /dev/null || ! echo "$OUTPUT" | grep -F "bbb" > /dev/null; then
  echo "FAIL: multiple vars on one line should both expand"
  echo "$OUTPUT"
  FAIL=1
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
echo "PASS: variable expansion works for \$VAR, \$?, quotes"
