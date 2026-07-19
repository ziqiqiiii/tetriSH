#!/usr/bin/env bash
# tests/integration/test_redirections.sh
#
# Verifies input/output redirections: >, >>, <, and combinations.

set -euo pipefail

SHELL_BIN=./macmini_shell
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT
FAIL=0

# 1. Output redirect creates file with content
OUTPUT=$(printf "echo hello > $TMPDIR/out1.txt\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if [ ! -f "$TMPDIR/out1.txt" ]; then
  echo "FAIL: > redirect did not create file"
  FAIL=1
elif ! grep -qF "hello" "$TMPDIR/out1.txt"; then
  echo "FAIL: > redirect file should contain 'hello'"
  cat "$TMPDIR/out1.txt"
  FAIL=1
fi

# 2. Output redirect overwrites existing file
printf "old content\n" > "$TMPDIR/out2.txt"
OUTPUT=$(printf "echo new > $TMPDIR/out2.txt\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if grep -qF "old" "$TMPDIR/out2.txt"; then
  echo "FAIL: > should overwrite, not append"
  cat "$TMPDIR/out2.txt"
  FAIL=1
fi

# 3. Append redirect adds to file
printf "first\n" > "$TMPDIR/out3.txt"
OUTPUT=$(printf "echo second >> $TMPDIR/out3.txt\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! grep -qF "first" "$TMPDIR/out3.txt" || ! grep -qF "second" "$TMPDIR/out3.txt"; then
  echo "FAIL: >> should append, keeping both 'first' and 'second'"
  cat "$TMPDIR/out3.txt"
  FAIL=1
fi

# 4. Multiple appends accumulate
OUTPUT=$(printf "echo line1 >> $TMPDIR/out4.txt\necho line2 >> $TMPDIR/out4.txt\necho line3 >> $TMPDIR/out4.txt\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
COUNT=$(wc -l < "$TMPDIR/out4.txt" || echo 0)
if [ "$COUNT" -lt 3 ]; then
  echo "FAIL: three appends should produce at least 3 lines, got $COUNT"
  cat "$TMPDIR/out4.txt"
  FAIL=1
fi

# 5. Input redirect reads from file
printf "from_file\n" > "$TMPDIR/in1.txt"
OUTPUT=$(printf "cat < $TMPDIR/in1.txt\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "from_file" > /dev/null; then
  echo "FAIL: < redirect should feed file content to cat"
  echo "$OUTPUT"
  FAIL=1
fi

# 6. Redirect does not break subsequent commands
OUTPUT=$(printf "echo redir > $TMPDIR/out5.txt\necho after\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "after" > /dev/null; then
  echo "FAIL: commands after redirect should still work"
  echo "$OUTPUT"
  FAIL=1
fi

# 7. Redirect to /dev/null suppresses output — verify next command still runs
OUTPUT=$(printf "echo hidden > /dev/null\necho visible\nexit\n" | timeout 3s $SHELL_BIN 2>&1)
if ! echo "$OUTPUT" | grep -F "visible" > /dev/null; then
  echo "FAIL: echo after /dev/null redirect should still show"
  echo "$OUTPUT"
  FAIL=1
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
echo "PASS: redirections (>, >>, <) work correctly"
