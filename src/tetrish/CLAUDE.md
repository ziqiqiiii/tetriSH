# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
make          # build libft, system programs (./bin/), and macmini_shell binary
make run      # build and launch the shell immediately
make re       # full rebuild (fclean + all)
make clean    # remove object files
make fclean   # remove object files and binary
```

**Readline:** `make` runs a `check-readline` target before building that auto-installs readline if missing — `libreadline-dev` via `sudo apt-get` on Linux/WSL, `readline` via `brew` on macOS. To install manually: `sudo apt install libreadline-dev` (Linux/WSL) or `brew install readline` (macOS).

The binary is named `macmini_shell`. Compiled with `-Wall -Wextra -Werror`. On Linux the build also enables `-fsanitize=address -g3`.

## Testing

```bash
make unit          # compile and run Unity-based unit tests in tests/unit/
make integration   # run shell-script integration tests in tests/integration/
make test          # run both
```

Filter tests by substring with `FILTER`:
```bash
make unit FILTER=lexer         # only tests matching "lexer"
make unit FILTER=09             # all builtin tests (09a–09l)
make integration FILTER=system  # only integration scripts matching "system"
```

Run a single unit test binary directly after building:
```bash
make unit && ./tests/unit/bin/test_09a_echo
```

### Generating unit tests with AI

```bash
# For modules with a dedicated header in includes/libs/<module>.h:
make ai-unit-tests MODULE=<stem>         # e.g. MODULE=rc_parser
# For builtin source files (no separate header needed):
make ai-builtin-tests MODULE=<stem>      # e.g. MODULE=09a_echo
```

Both targets invoke `scripts/gen_unit_tests.sh` / `scripts/gen_builtin_tests.sh`, which build a structured prompt and pipe it to `$MACMINI_AGENT_CMD` (defaults to printing to stdout). The generated file lands at `tests/unit/test_<MODULE>.c`.

## Source Tree Layout

Sources are split into three groups under `src/`, each built differently by the Makefile:

- `src/shell/` — every `.c` here is compiled (with AddressSanitizer on Linux) and linked into the `macmini_shell` binary. These are the numbered pipeline files (`00_`–`13_`).
- `src/system/` — each `.c` here is a standalone program compiled into its own binary in `./bin/` (`backup`, `dcheck`, `dkill`, `dplant`, `dspawn`, `find`, `ld`, `ldr`, `sys`). They include `system_program.h`. `sys.c` uses Linux-only APIs and is filtered out of `SYS_SRC` on macOS (`UNAME == Darwin`).
- `src/common/` — helpers shared by the system programs (`perms.c`, `project.c`), archived into `obj/libcommon.a` and linked into every `bin/` program. They include `common.h`.

Headers live in `includes/`: `minishell.h` (shell), `system_program.h` (system programs), `common.h` (shared helpers).

## Architecture

Input flows through these pipeline stages, each in its own numbered source file under `src/shell/`:

```
readline input
     │
     ▼
  Expander (03_expand)      — $VAR / $? substitution before tokenising
     │
     ▼
  Lexer (04_lexer)          — tokenise into COMMAND / PIPE / RDIN / RDOUT / RDAPP / HEREDOC
     │
     ▼
  Parser (05_parser)        — build a binary syntax tree (BST) of t_tree nodes
     │
     ▼
  Executor (06_execute)     — recurse the BST and dispatch to:
     ├── Pipe handler        (07_pipe)
     ├── Redirection handler (08_redirection)  <  >  >>  <<
     └── Built-in or execve (09_builtin + 06b_exec_path)
```

**Global state:** `g_exit_status` (defined in `00_main.c`, declared `extern` in `minishell.h`) holds the most recent foreground exit code. It is the only global variable.

**Central struct:** `t_root` (defined in `minishell.h`) is the shell's top-level state, passed by pointer through every stage. Key fields: `env_list` (linked list of `t_env`), `history`, `stdin_tmp`/`stdout_tmp` (saved FDs for redirection restore), `pipe`, and terminal `termios` snapshots.

**Environment:** stored as a `t_list` of `t_env` structs (not as `char **envp`). Helper `env_link_list()` builds it from `envp` at startup.

**BST tokens:** `t_token` enum — `END`, `RDAPP`, `HEREDOC`, `RDIN`, `RDOUT`, `PIPE`, `COMMAND`. The parser builds a right-leaning BST; the executor recurses it.

**Local binaries:** `init_root()` prepends `$PWD/bin` to `PATH` so programs under `./bin/` are resolved before system directories. The system programs (`backup`, `dcheck`, `dkill`, `dplant`, `dspawn`, `find`, `ld`, `ldr`, `sys`) are standalone C programs in `src/system/`, each compiled into its own binary under `./bin/` and linked against `libcommon.a` + `libft.a`.

## Unit Test Conventions

Unit tests use the [Unity](https://github.com/ThrowTheSwitch/Unity) framework (`tests/unity/unity.h`).

- Test files: `tests/unit/test_<module>.c`
- Compiled binaries: `tests/unit/bin/test_<module>`
- The Makefile links **all `src/shell/` source files** (except `00_main.c`), `libcommon.a`, `libft.a`, and `-lreadline` into each test binary. Every function declared in `minishell.h` is available — **do not write stubs** for shell or libft functions.
- Include `minishell.h` in unit tests to get all type definitions and function declarations.
- Define `int g_exit_status = 0;` in each test file to satisfy the linker.
- `setUp()` and `tearDown()` must always be defined even if empty.
- Test functions: `static void`, named `test_<what>_<expected>()`.
- Capture stdout via `pipe()` + `dup2()` when testing functions that write to fd 1.

## Source File Naming

Shell files (`src/shell/`) are numbered by pipeline stage: `00_` main, `01_` init/rc/banner, `02_` prompt, `03_` expand, `04_` lexer, `05_` parser, `06_` execute, `07_` pipe, `08_` redirection, `09_` builtins (`09a`–`09l`), `10_` quote, `11_` signal, `12_` free, `13_` utilities. Letter suffixes (e.g. `04a`, `04b`, `13a`–`13c`) are overflow files for the same stage.

## Code Style

The project follows 42 School / norminette conventions:
- Public functions start at column 0 with return type and name on the same line.
- Static helper functions are file-private.
- No function pointer typedefs; function signatures are declared directly in `minishell.h`.
- Error-wrapping functions (`ft_pipe`, `ft_dup2`, `ft_open`, `ft_close`, `ft_fork`) in `13a_minishell_utils.c` handle syscall errors and return `EXIT_FAILURE`/`EXIT_SUCCESS`.
