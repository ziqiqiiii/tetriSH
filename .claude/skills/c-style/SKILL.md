---
name: c-style
description: norminette C style for this repository. Use when writing or editing any .c or .h file, adding a function or a prototype, scaffolding a new library or binary directory, or writing a C unit test.
---

# C style

**norminette** style with Doxygen function headers: tabs, Allman braces,
parenthesised returns, one public function per file, and a header that owns
every shared declaration.

Two branches live outside this file:

- Adding a new file, library, or binary directory → read [`LAYOUT.md`](LAYOUT.md)
  for the three component archetypes and the file-naming scheme.
- Writing or running a test → read [`TESTING.md`](TESTING.md) for the test file
  layout, output protocol, and `FILTER=`.

---

## Function documentation

Every function is preceded by a Doxygen block. Order: `@brief`, blank line,
extended description, blank line, `@param` per argument, `@return` last.

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

- `@brief` is one sentence, ending in a period.
- Document **every** parameter; mark unused ones —
  `@param argc Number of command-line arguments (unused).`
- `@return` covers every return path. `void` functions have no `@return` line.
- Keep it concise (IMPORTANT).

---

## One function group per file, helpers `static`

Each file is built around a single public entry function supported by
file-private helpers.

- The **first function defined in the file is always the public one.**
- Helpers are `static`, defined **after** it, **in the order it first calls them**.
- Every `static` helper is **forward-declared at the top of the file**, right
  after the include, under a `// Static Functions` banner. The declarations are
  **tab-aligned into a column** the way header prototypes are; the definitions
  below use a single tab.

```c
#include "tetrisroom.h"

// Static Functions
static t_slot	*slot_of(t_room *r, t_player_id pid);
static void		promote_into(t_room *r, t_slot *vacated, bool (*probe)(void *ctx, t_player_id pid), void *probe_ctx);
static void		report_owner(t_release_result *out, const t_membership *m);
```

```c
#include "system_program.h"

// Static Functions
static void	load_logo(void);
static void	userinfo(void);
static void	render(void);

int	main(int argc, char **argv)
{
	...
	load_logo();
	userinfo();
	render();
	return (0);
}

static void	load_logo(void)
{
	...
}
```

Module-private state goes at the top of the file under its own banner:

```c
// Static Variables
static char	*logo[MAX_LOGO_LINES];
static int	logo_count = 0;
```

---

## Variable declarations

All variables are declared **at the start of the function**, before any
statement, one per line:

```c
static void	load_logo(void)
{
	char	line[MAX_LOGO_WIDTH];
	FILE	*f;
	size_t	line_len;
	int	vis_w;
	ssize_t	exe_len;

	exe_len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	...
}
```

No declaration appears mid-function. Initialise on a separate statement line
(`i = 0;` after `int i;`).

---

## Braces

The opening `{` goes on its **own line**, aligned with the statement — function
definitions, `if` / `else`, `while` / `for`:

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

**Single-statement bodies omit the braces:**

```c
if (!isatty(fd))
	return (EXIT_SUCCESS);

while (*s)
	s++;
```

---

## `return` with parentheses

Return values are wrapped in parentheses — `return (EXIT_SUCCESS);`,
`return (0);`, `return (execute(args));`.

---

## Signatures, indentation, spacing

- Public functions start at **column 0**, return type and name on the same line,
  separated by a **tab**: `int\tft_tcgetattr(...)`.
- No function-pointer typedefs; signatures are declared directly in the header.
- **Tabs** for indentation, one per level.
- One blank line between functions, and between the declaration block and the
  function body.
- Align variable groups with tabs so the names line up:

```c
char	*dir_name = args[0];
char	local_dir_name[SHELL_BUFFERSIZE];
DIR	*d;
```

---

## Naming

| Kind | Convention | Example |
|---|---|---|
| Functions / variables | `snake_case` | `load_logo`, `logo_count` |
| Syscall-wrapper helpers | `ft_` prefix | `ft_pipe`, `ft_dup2`, `ft_open` |
| Types (typedef struct/enum) | `t_` prefix | `t_board`, `t_cell_type`, `t_brain_result` |
| Enums / macros | `UPPER_SNAKE_CASE` | `EXIT_SUCCESS`, `MAX_LINES` |
| Global variable | `g_` prefix | `g_exit_status` |

The project uses exactly **one** global variable (`g_exit_status`).

The rule behind the table is one sentence: **a reader who has never opened the
header should be able to say what a name means.** What follows from it:

1. **No abbreviation a newcomer has to learn.** `config` not `cfg`, `registry`
   not `reg`, `outbox` not `ob`, `nonblock` not `nb`. Established C idiom is
   exempt — `ms`, `fd`, `id`, `max`, `min`, `len`, `cap`, `ptr`, `argc`/`argv`,
   `errno`. The test is whether the abbreviation is standard *outside* this
   repo, not whether it is familiar inside it.
2. **Macros carry the full component name** — `TETRISD_*`, `TETRISLOGD_*`,
   `TETRISCTL_*` — because those are exactly the `.tetrishrc` key prefixes, so
   `TETRISD_DEFAULT_PORT` visibly names the default for `TETRISD_PORT`.
3. **Functions read `subject_verb`** — `registry_add`, `outbox_push`,
   `client_spawn`. The subject matches the type the function takes.
4. **Request handlers read `method_handler`** — `signup_handler`,
   `move_handler`. They are the one place the noun comes first. A function in a
   `handlers_*.c` that is *not* a route handler gets an ordinary `subject_verb`
   name instead.
5. **A prefix names one module.** If a prefix covers unrelated
   responsibilities, the prefix is wrong and the module should be split.
6. **A prefix must not collide with a linked library.** `tetrisd` links eight
   static archives into one namespace.
7. **Types keep `t_`** — only the body of the name changes.
8. **One word, one meaning, repo-wide.** A filesystem path is `PATH`, an HTTTP
   path is `ROUTE`. Domain words follow `docs/CONTEXT.md` — a function that
   operates on a Target says `target`, never `victim` or `opponent`.

`docs/naming.md` §2 is the live prefix map — **check it before inventing a
prefix.** The rest of that document is the historical mapping from the one-time
rename (all three stages applied) plus §7, the procedure to follow if you ever
rename again.

---

## Error handling

Syscalls are wrapped by `ft_`-prefixed helpers that report the error and return
`EXIT_SUCCESS` / `EXIT_FAILURE`:

```c
if (tcgetattr(fd, termios_p) == -1)
{
	perror("tcgetattr failed");
	return (EXIT_FAILURE);
}
```

Report failures with `perror(...)`, or `fprintf(stderr, ...)` + `strerror(errno)`.

---

## Includes — the header owns everything shared

Each `.c` opens with a **single project header include, and only that**. A `.c`
file contains no other `#include`, no `#define`, and no
`typedef`/`struct`/`enum`. Library includes, macros, types, and prototypes all
live in the header, and the header pulls in the system headers it needs.

- `src/shell/` → `#include "minishell.h"`
- `src/system/` → `#include "system_program.h"`
- `src/common/` → `#include "common.h"`
- `lib/libXXX/src/` → `#include "XXX.h"` (e.g. `#include "tetrisbrain.h"`)

| Item | Location |
|---|---|
| Library/system `#include` (`<string.h>`, …) | header only |
| `#define` macros / constants | header only |
| `struct` / `enum` / `typedef` | header only |
| Function prototypes | header only |
| Function definitions | `.c` only |
| `static` helpers, variables, and `const` data tables | `.c` only |

A macro or type used by just one `.c` file still belongs in the header.

---

## Header formatting

**Preprocessor directives carry a space after `#`** — `# include`, `# define`,
and the include guard. (In `.c` files the single project include stays flush:
`#include "XXX.h"`.) `# define` values are **tab-aligned into a column**:

```c
# ifndef TETRISBRAIN_H
# define TETRISBRAIN_H

# include <stdbool.h>
# include <stdint.h>
# include <string.h>

# define BOARD_WIDTH			10
# define BOARD_HEIGHT			20
# define BRAIN_BAG_SIZE			7
# define BRAIN_MAX_CLEAR_LINES	4
# define LINES_PER_CHARGE		2
```

Comments in a header are **C-style `/* … */`** — `//` is the `.c` file's form
(and the `// Static Functions` banner).

**`struct` / `enum` / `union` open their brace on a new line**, the closing `}`
is separated from the type name by a **tab**, and member names (and enum `=`)
are tab-aligned into a column:

```c
/* what's in a cell */
typedef enum
{
	CELL_EMPTY		= 0,
	CELL_FILLED		= 1,
	CELL_GARBAGE	= 2
}	t_cell_type;

typedef struct
{
	t_cell_type	type;
	uint8_t		color;
}	t_cell;
```

**Prototypes are grouped by their owning `.c` file** under a `/* FILE.C */`
banner. Return type and name are separated by tabs, and every prototype in the
header aligns to the same column — one tab past the **longest return type in the
header** (here `t_brain_result`):

```c
/* BOARD.C */
void			board_init(t_board *b);
t_cell			board_get(const t_board *b, int col, int row);

/* PIECES.C */
t_piece			piece_spawn(t_piece_type type);
t_brain_result	piece_move(const t_board *b, t_piece *p, int dcol, int drow);

/* SCORING.C */
int				score_on_clear(int lines_cleared, int level);
```

---

## Compilation

Built with `-Wall -Wextra -Werror`; on Linux the build adds
`-fsanitize=address -g3`. Code must compile clean under these flags.
