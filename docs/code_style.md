# CODE_STYLE.md

The formatting conventions used across this repository, written so they can be
replicated in other repos. The project follows **norminette** style
with Doxygen-style function headers.

---

## Table of contents

1. [Function documentation](#1-function-documentation)
2. [File organisation — one function group per file, helpers `static`](#2-file-organisation--one-function-group-per-file-helpers-static)
3. [Variable declarations](#3-variable-declarations)
4. [Braces](#4-braces)
5. [`return` with parentheses](#5-return-with-parentheses)
6. [Public function signatures](#6-public-function-signatures)
7. [Indentation & spacing](#7-indentation--spacing)
8. [Naming conventions](#8-naming-conventions)
9. [Error handling](#9-error-handling)
10. [Includes](#10-includes)
11. [Header file formatting](#11-header-file-formatting)
12. [File naming by responsibility](#12-file-naming-by-responsibility)
13. [Testing](#13-testing)
14. [Compilation](#14-compilation)

---

## 1. Function documentation

Every function is preceded by a Doxygen block comment. Order: `@brief`, a blank
line, an extended description, a blank line, then `@param` for each argument and
`@return` last.

```c
/**
 * @brief Wraps the tcgetattr function, providing error handling.
 *
 * This function retrieves the parameters associated with the
 * terminal referred to by the given file descriptor and stores
 * them in the termios structure.
 *
 * @param fd The file descriptor of the terminal.
 * @param termios_p Pointer to the termios structure.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int	ft_tcgetattr(int fd, struct termios *termios_p)
```

Rules:
- `@brief` is one sentence, ending in a period.
- Document **every** parameter with `@param <name> <description>`.
- Mark unused parameters as such: `@param argc Number of command-line arguments (unused).`
- `@return` describes every return path (e.g. `EXIT_SUCCESS on success, EXIT_FAILURE on failure.`).
- `void` functions have no `@return` line.

---

## 2. File organisation — one function group per file, helpers `static`

Each file is built around a single public/entry function, supported by file-private helpers.

- The **first function defined in the file is always the main/public function.**
- Supporting helpers are `static` (file-private).
- `static` helpers are defined **after** the main function, **in the order they
  are first called** from it.
- `static` helpers are **forward-declared at the top of the file**, right after
  the includes, before any function body.
- **No function exceeds 25 lines** (counting the body between the opening and
  closing braces, excluding the signature, braces, and blank lines). Split longer
  functions into `static` helpers.
- **Each file holds at most 5 functions** (public + `static` helpers combined).
  A file that needs more is doing too much — split it, using letter-suffixed
  overflow files for the same stage (see §12).

```c
#include "system_program.h"

// Static Functions
static void	load_logo(void);
static void	userinfo(void);
static void	resourceinfo(void);
static void	systeminfo(void);
static void	render(void);

int	main(int argc, char **argv)
{
	...
	load_logo();
	userinfo();
	resourceinfo();
	systeminfo();
	render();
	return (0);
}

static void	load_logo(void)
{
	...
}
```

Module-private state is declared as `static` globals at the top of the file,
under a comment banner:

```c
// Static Variables
static char	*logo[MAX_LOGO_LINES];
static int	logo_count = 0;
```

---

## 3. Variable declarations

All variables are declared **at the start of the function**, before any
statements, one per line:

```c
static void	load_logo(void)
{
	char	line[MAX_LOGO_WIDTH];
	FILE	*f;
	size_t	line_len;
	int	vis_w;
	char	*slash;
	ssize_t	exe_len;

	exe_len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	...
}
```

- No declarations mixed in mid-function.
- Initialise on a separate line where a value is needed (`i = 0;` after `int i;`).

---

## 4. Braces

Opening brace `{` goes on its **own line**, aligned with the statement, for:
- function definitions
- `if` / `else`
- `while` / `for`

```c
int	ft_tcgetattr(int fd, struct termios *termios_p)
{
	if (tcsetattr(fd, optional_actions, termios_p) == -1)
	{
		perror("tcsetattr failed");
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}
```

**Single-statement bodies omit the braces** — for `if`, `else`, `while`, `for`:

```c
if (!isatty(fd))
	return (EXIT_SUCCESS);

while (*s)
	s++;

if (vis_w > max_logo_width)
	max_logo_width = vis_w;
```

---

## 5. `return` with parentheses

Return values are wrapped in parentheses:

```c
return (EXIT_SUCCESS);
return (EXIT_FAILURE);
return (0);
return (execute(args));
```

---

## 6. Public function signatures

- Public functions start at **column 0**, with return type and name on the **same line**.
- A **tab** separates the return type from the name: `int\tft_tcgetattr(...)`.
- No function-pointer typedefs; signatures are declared directly in the headers
  (`minishell.h`, `system_program.h`, `common.h`).

---

## 7. Indentation & spacing

- **Tabs** for indentation (one tab per level), not spaces.
- One blank line between functions and between the variable-declaration block and
  the function body.
- Align struct/variable groups with tabs so names line up:

```c
char	*dir_name = args[0];
char	*to_match = args[1];
char	local_dir_name[SHELL_BUFFERSIZE];
DIR	*d;
```

---

## 8. Naming conventions

| Kind                     | Convention            | Example                          |
|--------------------------|-----------------------|----------------------------------|
| Functions / variables    | `snake_case`          | `load_logo`, `logo_count`        |
| Syscall-wrapper helpers  | `ft_` prefix          | `ft_pipe`, `ft_dup2`, `ft_open`  |
| Types (typedef structs)  | `t_` prefix           | `t_root`, `t_env`, `t_tree`      |
| Enums / macros           | `UPPER_SNAKE_CASE`    | `EXIT_SUCCESS`, `MAX_LINES`      |
| Global variable          | `g_` prefix           | `g_exit_status`                  |

The project uses exactly **one** global variable (`g_exit_status`).

---

## 9. Error handling

Syscalls are wrapped by `ft_`-prefixed helpers that handle the error and return
`EXIT_SUCCESS` / `EXIT_FAILURE` (see `13_minishell_utils.c`, `src/common/ft_*.c`):

```c
if (tcgetattr(fd, termios_p) == -1)
{
	perror("tcgetattr failed");
	return (EXIT_FAILURE);
}
```

- Report failures with `perror(...)` or `fprintf(stderr, ...)` + `strerror(errno)`.
- Functions signal success/failure via `EXIT_SUCCESS` / `EXIT_FAILURE`.

---

## 10. Includes

Each `.c` opens with a single project header include — **and only that**. A `.c`
file contains no other `#include`, no `#define`, and no `typedef`/`struct`/`enum`
declarations. Everything shared — library (system) includes, macros, structs,
enums, and typedefs — lives in the project header, and the header pulls in the
system headers it needs:

- `src/shell/` files: `#include "minishell.h"`
- `src/system/` files: `#include "system_program.h"`
- `src/common/` files: `#include "common.h"`
- `lib/libXXX/src/` files: `#include "XXX.h"` (e.g. `#include "tetrisbrain.h"`)

What goes **where**:

| Item | Location |
|---|---|
| Library/system `#include` (`<string.h>`, …) | header only |
| `#define` macros / constants | header only |
| `struct` / `enum` / `typedef` | header only |
| Function prototypes | header only |
| Function definitions | `.c` only |
| `static` (file-private) helpers, variables, and `const` data tables | `.c` only |

So a `.c` file holds only its `#include "XXX.h"` line, its function definitions,
and any `static` file-private helpers/data — never a macro, struct, or library
include. A macro or type used by just one `.c` file still belongs in the header.

```c
/* XXX.h  (directive spacing + struct layout per §11) */
# include <string.h>

# define GRAVITY_BASE_MS	1000

typedef struct s_cell_offset
{
	int8_t	dcol;
	int8_t	drow;
}	t_cell_offset;

/* XXX.c — the only include; memcpy, the macro, and the struct all resolve */
/*         through the header */
#include "tetrisbrain.h"
```

---

## 11. Header file formatting

The header is where all shared declarations live (see §10), and it follows the
conventions of `minishell.h`, distinct from the `.c` files.

**The include guard's opening `#ifndef` stays flush**, and every preprocessor
directive after it carries a **space after `#`** — `# define`, `# include`,
`# endif`. (In `.c` files the single project include is flush: `#include
"XXX.h"`.) `# define` **values are tab-aligned** into a single column; comments
on a directive use C-style `/* */`, not `//`:

```c
#ifndef MINISHELL_H
# define MINISHELL_H

# include <stdio.h>				/* printf */
# include <stdlib.h>			/* malloc, free, exit, getenv */

# include "libft.h"
# include "common.h"

# define GREEN	"\033[1;32m"
# define RESET	"\033[0m"

# define EXIT_MISUSE	2		/* Misused of shell built-ins */
# define EXIT_NO_CMD	127	/* Command not found */
```

**`struct` / `enum` carry a named tag** — `e_` for enums, `s_` for structs —
and are `typedef`'d to a `t_` name. The brace opens on a **new line**, the
closing `}` is separated from the `t_` name by a **tab**, and members are
**tab-indented** with their names tab-aligned into a column:

```c
typedef enum e_token
{
	END,
	RDAPP,
	HEREDOC,
	RDIN,
	RDOUT,
	PIPE,
	COMMAND,
	NO_OF_TOKEN_TYPES
}	t_token;

typedef struct s_tree
{
	t_token			token;
	char			*value;
	struct s_tree	*left;
	struct s_tree	*right;
}	t_tree;
```

Enum members are listed one per line and are **not** forced to carry `= value`
unless a specific value matters.

**Prototypes are grouped by their owning `.c` file**, under a banner comment of
the form `/* NN_NAME */` (the source file's stage number + name, C-style `/* */`),
with a blank line above and below each group. The return type and function name
are **separated by tabs**, and every prototype across the whole header aligns to
the same column — one tab past the **longest return type** in the header. Lines
that run long wrap with a `\` and the continuation indented one tab past that
column:

```c
/* 04_LEXER */

t_list				*lexer(char *cmd);
int					count_token(char *cmd);

/* 05_PARSER */

t_tree				*parser(t_list *lexer, int n_token, t_root *sh);
t_tree				*tree_node_new(t_token type, char *value, \
						t_tree *left, t_tree *right);

/* 13_MINISHELL_UTILS */

int					ft_tcsetattr(int fd, int optional_actions, \
						struct termios *termios_p);
```

So shorter return types (`void`, `int`, `t_tree`) get as many tabs as needed to
reach the same column as the widest one.

---

## 12. File naming by responsibility

Files are named to make their place in the pipeline / system obvious:

- **Shell** (`src/shell/`): numbered by pipeline stage —
  `00_` main, `01_` init, `02_` prompt, `03_` expand, `04_` lexer, `05_` parser,
  `06_` execute, `07_` pipe, `08_` redirection, `09_` builtins, `10_` quote,
  `11_` signal, `12_` free, `13_` utilities. Letter suffixes (`04a`, `04b`, …)
  are overflow files for the same stage.
- **System programs** (`src/system/`): one file per standalone binary
  (`find.c`, `backup.c`, `sys.c`, …), each with its own `main`.
- **Common helpers** (`src/common/`): one helper per file
  (`ft_open.c`, `ft_pipe.c`, `perms.c`, …).

---

## 13. Testing

Each self-contained library owns its tests under `tests/`, plus a shared
`scripts/run_tests.sh` that runs them and formats the output. There are two
kinds of test:

- **Unit tests** — compiled C, one file per source module, named
  `tests/test_<module>.c` (e.g. `test_board.c`, `test_abilities.c`). Each builds
  into `tests/bin/` and links the library archive directly.
- **Integration tests** — shell scripts driven through the same runner in
  `integration` mode.

**Test file layout** mirrors the source conventions: a leading comment banner
describing the suite, then one `void test_<description>(void)` per case, with
`static` helpers (e.g. `fill_row`, `fill_board`) shared within the file. Cases
use `assert()` for checks and report their result on stdout in the runner's
protocol:

```c
void	test_cut_top_zero_is_noop(void)
{
	...
	assert(memcmp(&before, &after, sizeof(board_t)) == 0);
	printf("PASS test_cut_top_zero_is_noop\n");
}
```

**Output protocol** parsed by `run_tests.sh`:

- Unit tests print `PASS <name>` / `FAIL <name>` per case (Unity-style
  `file:line:name:PASS|FAIL[:detail]` is also recognised).
- Integration scripts print `PASS: <desc>` / `FAIL: <desc>`.
- A test binary/script signals overall failure with a non-zero exit code; the
  runner exits 0 only if every test passed.

**Running:**

```bash
make -C lib/libXXX test                  # build every test_*.c and run them
make -C lib/libXXX test FILTER=abilities  # only tests whose path contains the pattern
```

`FILTER=<substring>` is matched against each test's path, so it selects a module
(`FILTER=board`) or a stage (`FILTER=04`). No match → the runner reports it and
exits non-zero.

---

## 14. Compilation

Built with `-Wall -Wextra -Werror`; on Linux the build also adds
`-fsanitize=address -g3`. Code must compile cleanly under these flags.
