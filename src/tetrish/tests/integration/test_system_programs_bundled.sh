#!/usr/bin/env bash
# tests/integration/test_system_programs_bundled.sh
#
# Verifies bundled system programs: ld, ldr, find, and their edge cases.

set -euo pipefail

SHELL_BIN=./macmini_shell
FAIL=0

# 1. ld lists the files directory
OUTPUT=$(printf "ld\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "files" > /dev/null; then
  echo "FAIL: ld did not show the 'files' directory"
  echo "$OUTPUT"
  FAIL=1
fi

# 2. find locates file1.txt
OUTPUT=$(printf "find file1\nexit\n" | timeout 5s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "file1.txt" > /dev/null; then
  echo "FAIL: find did not match file1.txt"
  echo "$OUTPUT"
  FAIL=1
fi

# 3. ldr recursively lists files (output may use ./files/ prefix)
OUTPUT=$(printf "ldr\nexit\n" | timeout 5s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "file1.txt" > /dev/null; then
  echo "FAIL: ldr did not recursively show file1.txt"
  echo "$OUTPUT"
  FAIL=1
fi

# 4. ld after cd shows contents of new directory
OUTPUT=$(printf "cd files\nld\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "file1.txt" > /dev/null; then
  echo "FAIL: ld after cd files should show file1.txt"
  echo "$OUTPUT"
  FAIL=1
fi

# 5. find with a term that matches nothing doesn't crash
OUTPUT=$(printf "find zzz_nonexistent_term\necho survived\nexit\n" | timeout 5s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "survived" > /dev/null; then
  echo "FAIL: find with no match should not crash the shell"
  echo "$OUTPUT"
  FAIL=1
fi

# 6. find locates file2.txt
OUTPUT=$(printf "find file2\nexit\n" | timeout 5s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "file2.txt" > /dev/null; then
  echo "FAIL: find did not match file2.txt"
  echo "$OUTPUT"
  FAIL=1
fi

# 7. ldr shows nested content including multiple files
OUTPUT=$(printf "ldr\nexit\n" | timeout 5s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "file2.txt" > /dev/null; then
  echo "FAIL: ldr should show file2.txt in recursive listing"
  echo "$OUTPUT"
  FAIL=1
fi

# 8. ld runs twice in same session
OUTPUT=$(printf "ld\nld\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
COUNT=$(echo "$OUTPUT" | grep -c "files" || true)
if [ "$COUNT" -lt 2 ]; then
  echo "FAIL: two ld calls should both produce output"
  echo "$OUTPUT"
  FAIL=1
fi

# 9. system programs followed by builtins work
OUTPUT=$(printf "ld\necho done\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "files" > /dev/null || ! echo "$OUTPUT" | grep -F "done" > /dev/null; then
  echo "FAIL: ld followed by echo should both work"
  echo "$OUTPUT"
  FAIL=1
fi

# 10. find with partial name match
OUTPUT=$(printf "find oneline\nexit\n" | timeout 5s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "oneline.txt" > /dev/null; then
  echo "FAIL: find should match oneline.txt"
  echo "$OUTPUT"
  FAIL=1
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
echo "PASS: ld, ldr, and find work with edge cases"
