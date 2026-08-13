#!/usr/bin/env bash
# tests/integration/test_builtin_help.sh
#
# Verifies help, usage, echo, and pwd builtins.

set -euo pipefail

SHELL_BIN=./macmini_shell
FAIL=0

# --- help ---

# 1. help lists all required builtins
OUTPUT=$(printf "help\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
MISSING=""
for cmd in cd help exit usage env setenv unsetenv; do
  if ! echo "$OUTPUT" | grep -F "$cmd" > /dev/null; then
    MISSING="$MISSING $cmd"
  fi
done
if [ -n "$MISSING" ]; then
  echo "FAIL: help did not mention these builtins:$MISSING"
  echo "$OUTPUT"
  FAIL=1
fi

# --- usage ---

# 2. usage with no args prints general usage
OUTPUT=$(printf "usage\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -iF "usage" > /dev/null; then
  echo "FAIL: usage with no args should print usage info"
  echo "$OUTPUT"
  FAIL=1
fi

# 3. usage echo shows echo help
OUTPUT=$(printf "usage echo\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "echo" > /dev/null; then
  echo "FAIL: usage echo should print echo help"
  echo "$OUTPUT"
  FAIL=1
fi

# 4. usage cd shows cd help
OUTPUT=$(printf "usage cd\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "cd" > /dev/null; then
  echo "FAIL: usage cd should print cd help"
  echo "$OUTPUT"
  FAIL=1
fi

# 5. usage for unknown builtin prints error
OUTPUT=$(printf "usage notabuiltin\nexit\n" | timeout 3s $SHELL_BIN 2>&1 || true)
if ! echo "$OUTPUT" | grep -F "no such built-in" > /dev/null; then
  echo "FAIL: usage for unknown builtin should print 'no such built-in'"
  echo "$OUTPUT"
  FAIL=1
fi

# --- echo ---

# 6. echo prints arguments
OUTPUT=$(printf "echo hello world\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "hello world" > /dev/null; then
  echo "FAIL: echo should print its arguments"
  echo "$OUTPUT"
  FAIL=1
fi

# 7. echo with no args prints empty line (shell survives)
OUTPUT=$(printf "echo\necho after\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "after" > /dev/null; then
  echo "FAIL: echo with no args should not crash"
  echo "$OUTPUT"
  FAIL=1
fi

# 8. echo -n suppresses newline
OUTPUT=$(printf "echo -n hello\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "hello" > /dev/null; then
  echo "FAIL: echo -n should still print the text"
  echo "$OUTPUT"
  FAIL=1
fi

# 9. echo with special characters
OUTPUT=$(printf "echo foo    bar\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "foo" > /dev/null; then
  echo "FAIL: echo should handle arguments"
  echo "$OUTPUT"
  FAIL=1
fi

# --- pwd ---

# 10. pwd prints current directory
OUTPUT=$(printf "pwd\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "$(pwd)" > /dev/null; then
  echo "FAIL: pwd should print current working directory"
  echo "$OUTPUT"
  FAIL=1
fi

# 11. pwd after cd shows new directory
OUTPUT=$(printf "cd /tmp\npwd\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "/tmp" > /dev/null; then
  echo "FAIL: pwd after cd /tmp should show /tmp"
  echo "$OUTPUT"
  FAIL=1
fi

# 12. multiple pwd calls work
OUTPUT=$(printf "pwd\npwd\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
COUNT=$(echo "$OUTPUT" | grep -c "$(pwd)" || true)
if [ "$COUNT" -lt 2 ]; then
  echo "FAIL: two pwd calls should produce two matching lines"
  echo "$OUTPUT"
  FAIL=1
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
echo "PASS: help, usage, echo, and pwd work with edge cases"
