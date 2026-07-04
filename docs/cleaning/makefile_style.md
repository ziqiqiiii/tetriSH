# MAKEFILE_STYLE.md

The conventions for writing a `Makefile` in this repository, written so they can
be replicated in other repos. The style favours **banner-sectioned layout,
`:=` assignment, colour-coded quiet recipes, and pattern rules over enumerated
targets** — a Makefile a reader can skim top-to-bottom like a document.

The canonical example is [`src/tetrish/Makefile`](../../src/tetrish/Makefile); every
snippet below is drawn from it.

---

## Table of contents

1. [Overall layout — banner sections in fixed order](#1-overall-layout--banner-sections-in-fixed-order)
2. [ASCII logo header](#2-ascii-logo-header)
3. [Variable assignment — `:=` by default](#3-variable-assignment----by-default)
4. [Variable naming & alignment](#4-variable-naming--alignment)
5. [Colour variables & quiet, decorated recipes](#5-colour-variables--quiet-decorated-recipes)
6. [Platform specifics](#6-platform-specifics)
7. [Directory & source variables](#7-directory--source-variables)
8. [Pattern rules over enumerated targets](#8-pattern-rules-over-enumerated-targets)
9. [Standard target set](#9-standard-target-set)
10. [Order-only prerequisites & dependency gating](#10-order-only-prerequisites--dependency-gating)
11. [Outsource installation to a bash script](#11-outsource-installation-to-a-bash-script)
12. [Comments](#12-comments)
13. [`.PHONY` & `.PRECIOUS`](#13-phony--precious)

---

## 1. Overall layout — banner sections in fixed order

The file is divided into labelled sections, each introduced by a full-width
banner comment. This makes the Makefile skimmable and gives every kind of
declaration a fixed home.

```make
################################################################################
#                                    CONFIG                                    #
################################################################################
```

- The banner is exactly **80 `#` wide**, with the title **centred** between two
  full lines.
- Section order, top to bottom: **CONFIG → PLATFORM SPECIFICS → DIRECTORIES →
  SOURCES → BUILD → SYSTEM PROGRAMS → TESTS → AI TESTS → CLEANUP → PHONY &&
  PRECIOUS.** Declarations (variables) come before rules; `.PHONY`/`.PRECIOUS`
  close the file.
- One blank line after a banner before its content.

---

## 2. ASCII logo header

The file opens with a small ASCII-art logo of the project name, as a comment,
before the first banner:

```make
#    \  |         |
#  |\/ |   _  |  |  /   _
#  |   |  (   |    <    __/
# _|  _| \__,_| _|\_\ \___|
#
```

Decorative but consistent — every Makefile in the repo leads with its name in
this form, then the `CONFIG` banner.

---

## 3. Variable assignment — `:=` by default

Use **`:=` (simple / immediate expansion)** for essentially every variable:

```make
NAME		:= macmini_shell
CC			:= gcc
FLAGS		:= -Wall -Wextra -Werror
SHELL_SRC	:= $(wildcard $(SRC_DIR)/shell/*.c)
```

Use **`=` (recursive / lazy expansion) only when a value must be resolved
later** than parse time — and say why in a comment:

```make
# RL_PREFIX is lazily expanded so it resolves correctly even when readline is
# installed by the check-readline target during a build.
RL_PREFIX	 = $(shell brew --prefix readline)
READLINE	 = -lreadline -L$(RL_PREFIX)/lib
```

Never use `?=` or bare `=` for a value that could be `:=`.

---

## 4. Variable naming & alignment

- Variable names are **`UPPER_SNAKE_CASE`**: `SRC_DIR`, `COMMON_LIB`,
  `TEST_CFLAGS`.
- The assignment operator is **tab-aligned into a column** so the `:=` line up:

  ```make
  SRC_DIR		:= src
  OBJ_DIR		:= obj
  BIN_DIR		:= bin
  INC_DIR		:= includes
  ```

- Alignment is **per block, not file-wide.** Each blank-line-separated group of
  assignments picks one operator column and every line in that group tabs to it.
  When a group's longest name overflows the usual column, the **whole group**
  shifts right to a wider column of its own — never leave one operator out of
  line with its neighbours:

  ```make
  AUTO_INSTALL_DEPS				?= 1
  INSTALL_NOTCURSES_FROM_SOURCE	?= 1
  NOTCURSES_VERSION				?= v3.0.17
  NOTCURSES_PREFIX				?= /usr/local
  ```

  Use **tabs** for the padding (the file is read at a 4-wide tab stop), so a
  name landing exactly on a tab stop still gets at least one separating tab.

- When a group mixes lazy `=` (§3) with `:=`, pad the `=` lines with a **single
  leading space** (`	 = `) so the `=` sits in the same column as the `:=` above
  it.

- Derived variables are built from their base with a substitution reference, not
  re-typed:

  ```make
  SHELL_OBJ	:= $(SHELL_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
  SYS_BINS	:= $(SYS_SRC:$(SRC_DIR)/system/%.c=$(BIN_DIR)/%)
  ```

- Every path reference goes through a variable — no literal `src/` or `obj/`
  scattered in rules.

---

## 5. Colour variables & quiet, decorated recipes

Colour escapes are defined once in `CONFIG` and reused; recipes are **silenced
with `@`** and print their own human-readable progress instead of echoing the
raw command:

```make
CLR_RMV		:= \033[0m
RED			:= \033[1;31m
GREEN		:= \033[1;32m
YELLOW		:= \033[1;33m
BLUE		:= \033[1;34m
CYAN		:= \033[1;36m
```

```make
$(NAME): $(LIBFT) $(COMMON_LIB) $(SHELL_OBJ)
	@ echo "\n$(GREEN)Compilation $(CLR_RMV)of $(BLUE)$(NAME)$(CLR_RMV)..."
	@ $(CC) $(FLAGS) $(FSAN) $(SHELL_OBJ) $(COMMON_LIB) $(LIBFT) $(LIB) $(READLINE) -o $(NAME)
	@ echo "$(GREEN)[Success] $(BLUE)$(NAME)$(CLR_RMV) created ✔️"
```

Conventions:
- Every recipe line starts with **`@ `** (silenced) — the command itself is not
  echoed; the recipe narrates instead.
- **Colours carry meaning**: `GREEN` success / heading, `BLUE` the artefact name,
  `YELLOW` the file being compiled, `CYAN` a library name or a test-run header,
  `RED` deletion.
- **Always reset with `$(CLR_RMV)`** after a coloured span.
- Compile rules `printf` the source file being built (`"$(YELLOW)$<$(CLR_RMV)... "`);
  completion lines `echo` a `[Success] … ✔️` message.

---

## 6. Platform specifics

All `uname`-based branching lives in the **PLATFORM SPECIFICS** section, keyed off
one variable, never scattered through rules:

```make
UNAME		:= $(shell uname)

# Readline + AddressSanitizer for Linux
ifeq ($(UNAME), Linux)
	READLINE	:= -lreadline
	INC_RL		:= -I/usr/include/readline
	FSAN		:= -fsanitize=address -g3
endif

ifeq ($(UNAME), Darwin)
	RL_PREFIX	 = $(shell brew --prefix readline)
	...
endif
```

- Compute `UNAME := $(shell uname)` once; branch on it with `ifeq`.
- Each platform block sets the **same variable names** to platform-appropriate
  values, so downstream rules stay platform-agnostic.
- Platform-specific source exclusions use `filter-out`, with a comment saying
  why:

  ```make
  # sys.c relies on Linux-only APIs (e.g. sysinfo); skip it on macOS.
  ifeq ($(UNAME), Darwin)
  	SYS_SRC	:= $(filter-out $(SRC_DIR)/system/sys.c, $(SYS_SRC))
  endif
  ```

---

## 7. Directory & source variables

- Every directory is a variable (`SRC_DIR`, `OBJ_DIR`, `BIN_DIR`, `INC_DIR`),
  defined in **DIRECTORIES**.
- Sources are gathered with **`$(wildcard …)`**, never hand-listed, so adding a
  `.c` needs no Makefile edit:

  ```make
  SHELL_SRC	:= $(wildcard $(SRC_DIR)/shell/*.c)
  ```

- Objects mirror the source tree under `OBJ_DIR` via substitution (§4), and the
  object rule recreates the tree with `mkdir -p $(dir $@)`.
- A short trailing comment on each source group states **what it builds into**:

  ```make
  # Shell  : every .c under src/shell        -> macmini_shell
  # Common : helpers shared by system programs -> libcommon.a
  # System : every .c under src/system        -> one binary each in bin/
  ```

---

## 8. Pattern rules over enumerated targets

Prefer a single **`%` pattern rule** to a hand-written rule per file. One rule
compiles every object; one rule builds every standalone binary:

```make
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | check-dependencies
	@ mkdir -p $(dir $@)
	@ $(CC) $(FLAGS) $(FSAN) $(INC) -c $< -o $@
	@ printf "$(YELLOW)$<$(CLR_RMV)... "

$(BIN_DIR)/%: $(SRC_DIR)/system/%.c $(COMMON_LIB) $(LIBFT) | check-dependencies
	@ mkdir -p $(BIN_DIR)
	@ $(CC) $(FLAGS) $(INC) $< $(COMMON_LIB) $(LIBFT) -o $@
```

- Use automatic variables — `$<` (first prereq), `$@` (target), `$*` (stem) —
  never re-name the file literally.
- When one tree needs a different recipe (e.g. common objects built **without**
  AddressSanitizer), give it its own more-specific pattern
  (`$(OBJ_DIR)/common/%.o`) rather than an `if` inside a shared recipe.
- A one-line `# --- name ------` comment precedes each rule group (§12).

---

## 9. Standard target set

Every Makefile exposes the same core targets, so muscle memory transfers between
libraries and binaries:

| Target | Does |
|---|---|
| `all` | Default build (`$(NAME)` + everything it needs); usually the first rule |
| `run` | Build then launch the artefact |
| `test` | Run all tests (aggregates `unit` + `integration`) |
| `unit` / `integration` | The individual test suites |
| `clean` | Remove objects and test binaries |
| `fclean` | `clean` + remove the final artefact(s) and recurse `fclean` into sub-libs |
| `re` | `fclean all` — full rebuild |

- `all` is the **first target** in the BUILD section so a bare `make` runs it.
- `fclean` depends on `clean` and additionally `$(MAKE) fclean -C` each
  sub-library — cleanup recurses.
- `re: fclean all` — always defined as exactly those two prerequisites.
- A target that takes an argument validates it and prints usage:

  ```make
  ai-unit-tests ai-builtin-tests:
  	@ if [ -z "$(MODULE)" ]; then \
  		echo "Usage: make $@ MODULE=name"; exit 1; fi
  ```

---

## 10. Order-only prerequisites & dependency gating

Use an **order-only prerequisite** (after `|`) for things that must *exist*
before a rule runs but whose timestamp should not trigger a rebuild — directory
creation, a dependency check, a prebuilt sub-library:

```make
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | check-dependencies
$(LIBFT): | check-dependencies
```

- `check-dependencies` is a gate run once before compiling; it self-skips when
  the umbrella build has already run it (`DEPS_READY=1`), so a direct
  `make -C src/tetrish` still installs deps but the umbrella build doesn't repeat
  the work.
- Recipes that create output directories always `mkdir -p` defensively at the
  top (`mkdir -p $(dir $@)`), rather than relying on the directory existing.

---

## 11. Outsource installation to a bash script

The Makefile **checks** dependencies but does not **install** them inline.
Multi-branch package-manager logic, privilege escalation, and source builds do
not belong in a recipe — a Makefile recipe is one shell line per step with
awkward `\`-continuations and no real control flow. Move all of it into a bash
script under the component's `scripts/` directory (§ layout matches
`run_tests.sh`) and have the recipe just call it:

```make
install-deps:
	@ bash ./scripts/install_deps.sh

install-notcurses-from-source:
	@ bash ./scripts/install_notcurses.sh
```

- The recipe is a **single `bash ./scripts/*.sh` line** — no `case`, no `if`,
  no package lists in the Makefile. Pass anything variable through the
  environment, not by templating into the script:
  `@ NOTCURSES_VERSION=$(NOTCURSES_VERSION) bash ./scripts/install_notcurses.sh`.
- Scripts live in **`scripts/` next to the Makefile that calls them**
  (`src/tetrisu/scripts/install_deps.sh`), keeping each component self-contained.
- The **check** side (`check-deps` — a compile/link probe, §10) may stay in the
  Makefile; only the **install** side is outsourced. Install is where the
  branching and privilege live.

**Privilege / authorization** is handled *inside the script*, once, at the top —
never assume root and never hard-code `sudo`. Detect it: use nothing if already
root, `sudo` if available, and fail with a clear message otherwise:

```bash
#!/usr/bin/env bash
set -euo pipefail

if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
elif command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
else
    echo "Root access or sudo is required to install packages." >&2
    exit 1
fi

# ... then prefix every privileged command:
$SUDO apt-get update
$SUDO apt-get install -y build-essential pkg-config libssl-dev
```

Rules for the script:
- `#!/usr/bin/env bash` and `set -euo pipefail` on every install script.
- Resolve `$SUDO` **once** at the top; prefix every privileged command with
  `$SUDO` (unquoted, so an empty value expands to nothing when already root).
- `sudo` itself performs the interactive password prompt — the script does not
  read or store credentials.
- Detect the package manager (`command -v apt-get`, `dnf`, `pacman`, …) and
  branch inside the script; print an actionable message and `exit 1` on an
  unsupported platform, never a silent no-op.
- Clean up temp dirs with `trap '...' EXIT HUP INT TERM` (source builds).

---

## 12. Comments

Two comment forms, used consistently:

- **Section banners** — the 80-wide centred `#` block (§1) for top-level
  sections.
- **Rule-group dividers** — a single `# --- label ----` line above a related
  group of rules:

  ```make
  # --- libft ------------------------------------------------------------------
  # --- shell binary -----------------------------------------------------------
  ```

- **Explanatory comments** state *why*, not *what* — reserved for the
  non-obvious (lazy expansion, a platform exclusion, an order-only gate). A rule
  whose intent is clear gets no comment.

---

## 13. `.PHONY` & `.PRECIOUS`

Both live in the final banner section:

```make
.PHONY:		all run system-programs unit integration test \
			ai-unit-tests ai-builtin-tests clean fclean re \
			reset check-dependencies

.PRECIOUS:	$(OBJ_DIR)/%.o
```

- **`.PHONY`** lists **every** target that isn't a real file — all the standard
  targets plus any command-style ones. Continuation lines use `\` and align
  under the first target.
- **`.PRECIOUS`** protects intermediate pattern outputs (objects) that `make`
  would otherwise auto-delete, so incremental rebuilds stay fast.
- These directives are the last thing in the file.
