#!/usr/bin/env bash
# tests/integration/test_builtin_cd.sh
#
# Verifies cd behaviour: relative paths, absolute paths, tilde, errors.

set -euo pipefail

SHELL_BIN=./macmini_shell
FAIL=0

# 1. cd to relative directory, pwd shows new location
OUTPUT=$(printf "cd files\npwd\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "files" > /dev/null; then
  echo "FAIL: cd files did not change to files directory"
  echo "$OUTPUT"
  FAIL=1
fi

# 2. cd to relative directory, ld shows contents
OUTPUT=$(printf "cd files\nld\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "file1.txt" > /dev/null; then
  echo "FAIL: after 'cd files', ld should list file1.txt"
  echo "$OUTPUT"
  FAIL=1
fi

# 3. cd to absolute path
OUTPUT=$(printf "cd /tmp\npwd\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "/tmp" > /dev/null; then
  echo "FAIL: cd /tmp did not change to /tmp"
  echo "$OUTPUT"
  FAIL=1
fi

# 4. cd with no args goes to $HOME
OUTPUT=$(printf "cd\npwd\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "$HOME" > /dev/null; then
  echo "FAIL: cd with no args should go to HOME ($HOME)"
  echo "$OUTPUT"
  FAIL=1
fi

# 5. cd to tilde expands to $HOME
OUTPUT=$(printf "cd ~\npwd\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "$HOME" > /dev/null; then
  echo "FAIL: cd ~ should expand to HOME ($HOME)"
  echo "$OUTPUT"
  FAIL=1
fi

# 6. cd to nonexistent directory prints error
OUTPUT=$(printf "cd /this_does_not_exist_xyz\nexit\n" | timeout 3s $SHELL_BIN 2>&1 || true)
if ! echo "$OUTPUT" | grep -iF "no such file" > /dev/null; then
  echo "FAIL: cd to nonexistent dir should print error"
  echo "$OUTPUT"
  FAIL=1
fi

# 7. cd to invalid path does not crash, shell continues
OUTPUT=$(printf "cd /this_does_not_exist_xyz\npwd\nexit\n" | timeout 3s $SHELL_BIN 2>&1 || true)
if ! echo "$OUTPUT" | grep -F "/" > /dev/null; then
  echo "FAIL: shell should survive cd to invalid path"
  echo "$OUTPUT"
  FAIL=1
fi

# 8. cd then cd .. returns to original directory
ORIG_PWD=$(pwd)
OUTPUT=$(printf "cd files\ncd ..\npwd\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "$ORIG_PWD" > /dev/null; then
  echo "FAIL: cd files then cd .. should return to original dir"
  echo "$OUTPUT"
  FAIL=1
fi

# 9. multiple cd calls in sequence
OUTPUT=$(printf "cd /tmp\ncd /var\npwd\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "/var" > /dev/null; then
  echo "FAIL: sequential cd calls should work"
  echo "$OUTPUT"
  FAIL=1
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
echo "PASS: cd handles relative, absolute, tilde, dotdot, and error cases"
