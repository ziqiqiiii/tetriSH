#!/usr/bin/env bash
# tests/integration/test_builtin_env.sh
#
# Verifies env, setenv, unsetenv, and export builtins.

set -euo pipefail

SHELL_BIN=./macmini_shell
FAIL=0

# 1. setenv then env shows the new variable
OUTPUT=$(printf "setenv CSESHELL_TEST=hello\nenv\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "CSESHELL_TEST=hello" > /dev/null; then
  echo "FAIL: setenv did not register CSESHELL_TEST=hello in env output"
  echo "$OUTPUT"
  FAIL=1
fi

# 2. setenv then unsetenv then env should NOT show the variable
OUTPUT=$(printf "setenv CSESHELL_TEST=hello\nunsetenv CSESHELL_TEST\nenv\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if echo "$OUTPUT" | grep -E "^CSESHELL_TEST=hello$" > /dev/null; then
  echo "FAIL: unsetenv did not remove CSESHELL_TEST"
  echo "$OUTPUT"
  FAIL=1
fi

# 3. export KEY=VALUE then env shows it
OUTPUT=$(printf "export MY_VAR=world\nenv\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "MY_VAR=world" > /dev/null; then
  echo "FAIL: export MY_VAR=world not visible in env"
  echo "$OUTPUT"
  FAIL=1
fi

# 4. export with no args shows declare lines
OUTPUT=$(printf "export\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "declare -x" > /dev/null; then
  echo "FAIL: export with no args should show 'declare -x' lines"
  echo "$OUTPUT"
  FAIL=1
fi

# 5. overwrite existing variable — extract only the env output block
OUTPUT=$(printf "setenv OVER_TEST=first\nsetenv OVER_TEST=second\nenv\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -E "^OVER_TEST=second$" > /dev/null; then
  echo "FAIL: setenv should overwrite existing variable"
  echo "$OUTPUT"
  FAIL=1
fi

# 6. export invalid identifier prints error
OUTPUT=$(printf "export 1INVALID=bad\nexit\n" | timeout 3s $SHELL_BIN 2>&1 || true)
if ! echo "$OUTPUT" | grep -F "not a valid identifier" > /dev/null; then
  echo "FAIL: export with invalid identifier should print error"
  echo "$OUTPUT"
  FAIL=1
fi

# 7. unset on nonexistent key is a no-op (no crash)
OUTPUT=$(printf "unsetenv NONEXISTENT_XYZ\necho still_alive\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "still_alive" > /dev/null; then
  echo "FAIL: unsetenv on nonexistent key should not crash"
  echo "$OUTPUT"
  FAIL=1
fi

# 8. env output includes inherited PATH
OUTPUT=$(printf "env\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "PATH=" > /dev/null; then
  echo "FAIL: env should show inherited PATH"
  echo "$OUTPUT"
  FAIL=1
fi

# 9. setenv with value containing equals sign
OUTPUT=$(printf "setenv EQ_TEST=a=b=c\nenv\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "EQ_TEST=a=b=c" > /dev/null; then
  echo "FAIL: setenv should handle value with equals signs"
  echo "$OUTPUT"
  FAIL=1
fi

# 10. multiple unsetenv in one session
OUTPUT=$(printf "setenv A_TEST=1\nsetenv B_TEST=2\nunsetenv A_TEST\nunsetenv B_TEST\nenv\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if echo "$OUTPUT" | grep -E "^(A_TEST|B_TEST)=" > /dev/null; then
  echo "FAIL: multiple unsetenv should remove both vars"
  echo "$OUTPUT"
  FAIL=1
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
echo "PASS: env / setenv / unsetenv / export work with edge cases"
