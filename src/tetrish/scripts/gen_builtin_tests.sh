#!/usr/bin/env bash
# Generates an AI prompt for unit-testing a shell builtin/command source file.
#
# Usage:
#   bash scripts/gen_builtin_tests.sh <module>
#   make ai-builtin-tests MODULE=<module>
#
# <module> is the source file stem, e.g. "09a_echo" for src/09a_echo.c.
#
# The generated prompt tells the AI to:
#   - Include "minishell.h"
#   - All shell source files (except 00_main.c) and libft.a are linked
#   - Every function in minishell.h is callable — no stubs needed
#
# Set MACMINI_AGENT_CMD to pipe the prompt to your AI tool, otherwise the
# prompt is printed to stdout for manual use:
#
#   export MACMINI_AGENT_CMD="claude --print"
#   export MACMINI_AGENT_CMD="tee /tmp/prompt.txt"

set -euo pipefail

MODULE="${1:-}"
if [[ -z "$MODULE" ]]; then
  echo "Usage: $0 <module>" >&2
  echo "  Example: $0 09a_echo" >&2
  exit 1
fi

SOURCE="$(find ./src -name "${MODULE}.c" | head -1)"
if [[ -z "$SOURCE" || ! -f "$SOURCE" ]]; then
  echo "Error: ./src/${MODULE}.c not found under ./src/." >&2
  exit 1
fi

# ---------------------------------------------------------------------------
# Extract non-static public function signatures from the source file.
# In 42 / norminette style, each public function starts at column 0 with its
# return type and name on the same line, followed by '{' on the next line.
# Static functions are prefixed with "static" on the same line.
# ---------------------------------------------------------------------------
extract_public_signatures() {
  awk '
    /^static[[:space:]]/ { next }
    /^[a-z][a-zA-Z0-9_*[:space:]	]*[a-zA-Z_][a-zA-Z0-9_]*[[:space:]]*\(/ {
      # strip trailing brace or whitespace, append semicolon
      sub(/[[:space:]]*\{[[:space:]]*$/, "")
      sub(/[[:space:]]+$/, "")
      if (/\)/) print $0 ";"
    }
  ' "$1"
}

SIGNATURES="$(extract_public_signatures "$SOURCE")"

if [[ -z "$SIGNATURES" ]]; then
  echo "Warning: no public function signatures found in $SOURCE." >&2
  echo "The prompt will include the full source for the AI to analyse." >&2
fi

# ---------------------------------------------------------------------------
# Identify any project-defined types used in the signatures so the AI knows
# they are available via minishell.h.
# ---------------------------------------------------------------------------
USED_TYPES="$(echo "$SIGNATURES" \
  | grep -oE 't_[a-z_]+|t_list|t_root|t_tree|t_history' \
  | sort -u | tr '\n' ' ' | sed 's/[[:space:]]*$//' \
  || true)"

build_prompt() {
  cat <<PROMPT
You are generating a C unit test file for the MacMini Shell project.

## Goal

Write tests/unit/test_${MODULE}.c that exercises the public functions in
${SOURCE}.

## Hard constraints

1. Include "minishell.h".
2. ALL shell source files (except 00_main.c) and libft.a are linked into each
   unit test binary. Every function declared in minishell.h and libft.h is
   available — call them directly. Do NOT write stubs for any of these functions.
3. Define \`int g_exit_status = 0;\` at file scope — minishell.h declares it
   as extern, so each test binary must supply the definition.
4. Only include standard C headers, "unity.h", and "minishell.h".
5. Do NOT create any new header file.

## Project conventions

- Test framework : Unity (tests/unity/unity.h)
- Test file      : tests/unit/test_${MODULE}.c
- setUp() and tearDown() must always be defined (even if empty)
- All test functions: static void, named test_<what>_<expected>()
- main(): UNITY_BEGIN() → RUN_TEST(…) for each → return UNITY_END()
- No heap allocation in tests; use stack buffers where possible
- Cover the happy path, edge cases, and boundary conditions
- Capture stdout via pipe() + dup2() when testing functions that write to fd 1
- Capture stderr via pipe() + dup2() when testing functions that write to fd 2

## Public functions to test

\`\`\`c
${SIGNATURES:-/* (no signatures extracted — see full source below) */}
\`\`\`

${USED_TYPES:+Project types referenced: ${USED_TYPES}
These types are already defined in minishell.h — no forward declarations needed.
}
## All available functions (from minishell.h)

All of the following functions are linked and callable in the test binary:

\`\`\`c
$(grep -E '^[a-z].*\(.*\);' ./includes/minishell.h)
\`\`\`

## Full source: ${SOURCE}

\`\`\`c
$(cat "$SOURCE")
\`\`\`

## Output

Respond with the complete, compilable C source for tests/unit/test_${MODULE}.c.
Output only the raw C code — no markdown fences, no tool calls, no explanation.
PROMPT
}

AGENT_CMD="${MACMINI_AGENT_CMD:-}"

if [[ -z "$AGENT_CMD" ]]; then
  {
    echo "================================================================"
    echo "MACMINI_AGENT_CMD not set — printing prompt to stdout."
    echo "Paste everything below into your AI tool, then save the output"
    echo "to tests/unit/test_${MODULE}.c"
    echo "================================================================"
    echo ""
  } >&2
  build_prompt
  exit 0
fi

build_prompt | eval "$AGENT_CMD"
