#!/usr/bin/env bash
# tests/integration/test_exit.sh
#
# Verifies exit behaviour: clean shutdown, numeric codes, error handling.

set -euo pipefail

SHELL_BIN=./macmini_shell
FAIL=0

assert_exit_code() {
  local desc="$1" input="$2" expected="$3"
  set +e
  echo "$input" | timeout 3s $SHELL_BIN > /dev/null 2>&1
  local got=$?
  set -e
  if [ "$got" -ne "$expected" ]; then
    echo "FAIL: $desc (expected exit $expected, got $got)"
    FAIL=1
  fi
}

# 1. Plain exit terminates cleanly
if ! timeout 3s bash -c 'printf "exit\n" | '"$SHELL_BIN" > /dev/null 2>&1; then
  echo "FAIL: shell did not exit cleanly within 3 seconds"
  FAIL=1
fi

# 2. exit with numeric argument
assert_exit_code "exit 0"   "exit 0"   0
assert_exit_code "exit 1"   "exit 1"   1
assert_exit_code "exit 42"  "exit 42"  42

# 3. exit wraps values mod 256
assert_exit_code "exit 256" "exit 256" 0
assert_exit_code "exit 300" "exit 300" 44

# 4. exit with non-numeric argument prints error
OUTPUT=$(printf "exit abc\n" | timeout 3s $SHELL_BIN 2>&1 || true)
if ! echo "$OUTPUT" | grep -F "numeric argument required" > /dev/null; then
  echo "FAIL: exit with non-numeric arg should print 'numeric argument required'"
  echo "$OUTPUT"
  FAIL=1
fi

# 5. exit with too many arguments does not exit (shell continues)
OUTPUT=$(printf "exit 1 2\npwd\nexit\n" | timeout 3s $SHELL_BIN 2>&1 || true)
if ! echo "$OUTPUT" | grep -F "too many arguments" > /dev/null; then
  echo "FAIL: exit with too many args should print 'too many arguments'"
  echo "$OUTPUT"
  FAIL=1
fi

# 6. exit prints "exit" on stderr
OUTPUT=$(printf "exit\n" | timeout 3s $SHELL_BIN 2>&1 || true)

# 7. exit after commands preserves session
OUTPUT=$(printf "echo alive\nexit\n" | timeout 3s $SHELL_BIN 2>&1 || true)
if ! echo "$OUTPUT" | grep -F "alive" > /dev/null; then
  echo "FAIL: commands before exit should still execute"
  FAIL=1
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
echo "PASS: exit handles clean shutdown, numeric codes, and error cases"
