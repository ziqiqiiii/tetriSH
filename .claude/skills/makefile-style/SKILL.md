---
name: makefile-style
description: Makefile conventions for this repository. Use when writing or editing any Makefile here, adding or changing a build target, or moving dependency install/check logic out of a recipe.
---

# Makefile style

A Makefile a reader skims top-to-bottom like a document: **banner** sections in a
fixed order, `:=` assignment, **quiet** colour-coded recipes that narrate instead
of echoing, and pattern rules over enumerated targets.

The canonical example is `src/tetrish/Makefile`; every snippet below is drawn
from it.

When a recipe grows dependency install, check, or probe logic → read
[`DEPS_SCRIPTS.md`](DEPS_SCRIPTS.md), which covers moving it into
`scripts/*.sh` and the rules those scripts follow.

---

## Layout — banner sections in fixed order

Each section is introduced by a full-width banner comment, giving every kind of
declaration a fixed home.

```make
################################################################################
#                                    CONFIG                                    #
################################################################################
```

- The banner is exactly **80 `#` wide**, title **centred** between two full lines.
- Order, top to bottom: **CONFIG → PLATFORM SPECIFICS → DIRECTORIES → SOURCES →
  BUILD → SYSTEM PROGRAMS → TESTS → AI TESTS → CLEANUP → PHONY && PRECIOUS.**
  Declarations come before rules; `.PHONY`/`.PRECIOUS` close the file.
- One blank line after a banner before its content.

The file opens with a small ASCII-art logo of the project name, as a comment,
before the first banner:

```make
#    \  |         |
#  |\/ |   _  |  |  /   _
#  |   |  (   |    <    __/
# _|  _| \__,_| _|\_\ \___|
#
```

---

## `:=` by default

Use **`:=` (simple, immediate expansion)** for essentially every variable:

```make
NAME		:= macmini_shell
CC			:= gcc
FLAGS		:= -Wall -Wextra -Werror
SHELL_SRC	:= $(wildcard $(SRC_DIR)/shell/*.c)
```

Use **`=` only when a value must resolve later than parse time** — and say why
in a comment:

```make
# RL_PREFIX is lazily expanded so it resolves correctly even when readline is
# installed by the check-readline target during a build.
RL_PREFIX	 = $(shell brew --prefix readline)
READLINE	 = -lreadline -L$(RL_PREFIX)/lib
```

Reserve `?=` for values a caller overrides from the environment.

---

## Naming & alignment

- Variable names are **`UPPER_SNAKE_CASE`**: `SRC_DIR`, `COMMON_LIB`,
  `TEST_CFLAGS`.
- The assignment operator is **tab-aligned into a column**:

  ```make
  SRC_DIR		:= src
  OBJ_DIR		:= obj
  BIN_DIR		:= bin
  INC_DIR		:= includes
  ```

- Alignment is **per block, not file-wide.** Each blank-line-separated group
  picks one operator column and every line in that group tabs to it. When a
  group's longest name overflows the usual column, the **whole group** shifts
  right to a wider column of its own:

  ```make
  AUTO_INSTALL_DEPS				?= 1
  INSTALL_NOTCURSES_FROM_SOURCE	?= 1
  NOTCURSES_VERSION				?= v3.0.17
  NOTCURSES_PREFIX				?= /usr/local
  ```

  Pad with **tabs** (the file is read at a 4-wide tab stop), so a name landing
  exactly on a tab stop still gets at least one separating tab.
- When a group mixes lazy `=` with `:=`, pad the `=` lines with a **single
  leading space** (`	 = `) so the `=` sits in the same column as the `:=` above it.
- Derived variables are built from their base with a substitution reference,
  never re-typed:

  ```make
  SHELL_OBJ	:= $(SHELL_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
  SYS_BINS	:= $(SYS_SRC:$(SRC_DIR)/system/%.c=$(BIN_DIR)/%)
  ```

- Every path reference goes through a variable — no literal `src/` or `obj/` in
  a rule.

---

## Quiet, decorated recipes

Colour escapes are defined once in `CONFIG` and reused; recipes are **silenced
with `@`** and print their own progress instead of echoing the raw command:

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

- Every recipe line starts with **`@ `**; the recipe narrates rather than echoes.
- **Colours carry meaning**: `GREEN` success or heading, `BLUE` the artefact
  name, `YELLOW` the file being compiled, `CYAN` a library name or test-run
  header, `RED` deletion.
- **Always reset with `$(CLR_RMV)`** after a coloured span.
- Compile rules `printf` the source being built (`"$(YELLOW)$<$(CLR_RMV)... "`);
  completion lines `echo` a `[Success] … ✔️` message.
- A dependency-check status line reads **`$(GREEN)… ready$(CLR_RMV) (<OS>).`** —
  the green span covers only the "… ready" phrase, with the OS in plain
  parentheses after the reset. A component qualifies it with its own name in the
  same shape:

  ```make
  echo "$(GREEN)Dependencies ready$(CLR_RMV) ($(UNAME_S))."          # umbrella
  echo "$(GREEN)tetrisu dependencies ready$(CLR_RMV) ($(UNAME))."    # component
  ```

---

## Platform specifics

All `uname`-based branching lives in the **PLATFORM SPECIFICS** section, keyed
off one variable, never scattered through rules:

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
- **Keep conditional directives at column 0.** `ifeq` / `ifneq` / `else` /
  `endif` and the assignments between them all start flush left. A leading tab
  makes `make` read the line as a *recipe*; inside a conditional this
  mis-tokenises and can intermittently corrupt a build (an `export` or
  assignment silently dropped, breaking a downstream rule only on some runs).
  The column alignment above is achieved with tabs *after* the variable name.
- Platform-specific source exclusions use `filter-out`, with a comment saying why:

  ```make
  # sys.c relies on Linux-only APIs (e.g. sysinfo); skip it on macOS.
  ifeq ($(UNAME), Darwin)
  SYS_SRC	:= $(filter-out $(SRC_DIR)/system/sys.c, $(SYS_SRC))
  endif
  ```

---

## Directory & source variables

- Every directory is a variable (`SRC_DIR`, `OBJ_DIR`, `BIN_DIR`, `INC_DIR`),
  defined in **DIRECTORIES**.
- Sources are gathered with **`$(wildcard …)`**, never hand-listed, so adding a
  `.c` needs no Makefile edit:

  ```make
  SHELL_SRC	:= $(wildcard $(SRC_DIR)/shell/*.c)
  ```

- Objects mirror the source tree under `OBJ_DIR` via substitution, and the object
  rule recreates the tree with `mkdir -p $(dir $@)`.
- A short trailing comment on each source group states **what it builds into**:

  ```make
  # Shell  : every .c under src/shell          -> macmini_shell
  # Common : helpers shared by system programs -> libcommon.a
  # System : every .c under src/system         -> one binary each in bin/
  ```

---

## Pattern rules over enumerated targets

One `%` rule compiles every object; one builds every standalone binary:

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

---

## Standard target set

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
- `re: fclean all` — always exactly those two prerequisites.
- **Every recursive `$(MAKE) -C` carries `--no-print-directory`**, defined once
  as a variable and reused on every recursion (build *and* clean), so an umbrella
  build isn't buried in `Entering directory` / `make[1]:` noise:

  ```make
  MAKE_FLAGS	:= --no-print-directory

  libs: | deps
  	@ for d in $(LIB_DIRS); do $(MAKE) $(MAKE_FLAGS) -C $$d || exit 1; done
  ```

- A target taking an argument validates it and prints usage:

  ```make
  ai-unit-tests ai-builtin-tests:
  	@ if [ -z "$(MODULE)" ]; then \
  		echo "Usage: make $@ MODULE=name"; exit 1; fi
  ```

---

## Order-only prerequisites

Use an **order-only prerequisite** (after `|`) for anything that must *exist*
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
- Recipes that create output always `mkdir -p` defensively at the top
  (`mkdir -p $(dir $@)`) rather than relying on the directory existing.

---

## Comments

- **Section banners** — the 80-wide centred `#` block, for top-level sections.
- **Rule-group dividers** — a single `# --- label ----` line above a related
  group of rules:

  ```make
  # --- libft ------------------------------------------------------------------
  # --- shell binary -----------------------------------------------------------
  ```

- **Explanatory comments** state *why*, not *what*, and are reserved for the
  non-obvious — lazy expansion, a platform exclusion, an order-only gate. A rule
  whose intent is clear gets no comment.

---

## `.PHONY` & `.PRECIOUS`

Both live in the final banner section, the last thing in the file:

```make
.PHONY:		all run system-programs unit integration test \
			ai-unit-tests ai-builtin-tests clean fclean re \
			reset check-dependencies

.PRECIOUS:	$(OBJ_DIR)/%.o
```

- **`.PHONY`** lists **every** target that isn't a real file. Continuation lines
  use `\` and align under the first target.
- **`.PRECIOUS`** protects intermediate pattern outputs (objects) that `make`
  would otherwise auto-delete, keeping incremental rebuilds fast.
