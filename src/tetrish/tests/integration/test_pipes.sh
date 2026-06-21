#!/usr/bin/env bash
# tests/integration/test_pipes.sh
#
# Verifies pipe behaviour: simple pipes, chained pipes, builtins in pipes.

set -euo pipefail

SHELL_BIN=./macmini_shell
FAIL=0

# 1. Simple pipe: echo into external command
OUTPUT=$(printf "echo hello world | cat\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "hello world" > /dev/null; then
  echo "FAIL: echo hello world | cat should produce 'hello world'"
  echo "$OUTPUT"
  FAIL=1
fi

# 2. Pipe between external commands
OUTPUT=$(printf "echo -n aaa | wc -c\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "3" > /dev/null; then
  echo "FAIL: echo -n aaa | wc -c should show 3"
  echo "$OUTPUT"
  FAIL=1
fi

# 3. Chained pipes
OUTPUT=$(printf "echo one two three | cat | cat\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "one two three" > /dev/null; then
  echo "FAIL: chained cat pipes should pass text through"
  echo "$OUTPUT"
  FAIL=1
fi

# 4. Pipe preserves shell session (commands after pipe still work)
OUTPUT=$(printf "echo piped | cat\necho after\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "after" > /dev/null; then
  echo "FAIL: commands after a pipe should still execute"
  echo "$OUTPUT"
  FAIL=1
fi

# 5. Pipe with grep
OUTPUT=$(printf "echo findme_xyz | grep findme\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "findme_xyz" > /dev/null; then
  echo "FAIL: echo | grep should work"
  echo "$OUTPUT"
  FAIL=1
fi

# 6. Pipe with grep filtering out non-matching
OUTPUT=$(printf "echo aaa | grep bbb\necho survived\nexit\n" | timeout 3s $SHELL_BIN 2>&1 || true)
if ! echo "$OUTPUT" | grep -F "survived" > /dev/null; then
  echo "FAIL: failed grep in pipe should not crash shell"
  echo "$OUTPUT"
  FAIL=1
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
echo "PASS: pipes work for simple, chained, and builtin cases"
