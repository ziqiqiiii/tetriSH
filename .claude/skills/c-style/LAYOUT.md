# Component layout & file naming

Every buildable component is a **self-contained directory** — it owns its
`Makefile`, `src/`, `tests/`, `scripts/run_tests.sh`, and builds its artefacts in
place. Consumers only reach in through the component's include path and (for a
library) its `.a`. Three archetypes exist.

---

## Library — `lib/libtetrisbrain`

The canonical self-contained library. Public declarations live in a dedicated
`include/` directory whose single header shares the library's name; every `.c`
under `src/` includes **only** that header.

```
lib/libtetrisbrain/
├── Makefile              # make -C lib/libtetrisbrain [test|clean|fclean|re]
├── include/
│   └── tetrisbrain.h     # public header — consumers add -I lib/libtetrisbrain/include
├── src/                  # one .c per module, each #include "tetrisbrain.h"
│   ├── board.c
│   ├── pieces.c
│   └── gravity.c
├── tests/                # one test_<module>.c per source module
│   ├── test_board.c
│   └── test_pieces.c
├── scripts/
│   └── run_tests.sh
├── obj/                  # generated objects (git-ignored)
└── libtetrisbrain.a      # generated archive
```

- **Header placement:** its own `include/` directory, header named after the lib.
- **`src/` is flat** — no sub-grouping; one file per module.
- Consumers link `libtetrisbrain.a` and add `-I lib/libtetrisbrain/include`.

---

## Small binary — `src/tetrisu`

A lean terminal binary with a **single header at the component root**
(`tetrisu.h`, not inside an `include/` directory) that every `.c` in the flat
`src/` includes.

```
src/tetrisu/
├── Makefile
├── tetrisu.h             # single root header, included from every src/*.c
├── src/                  # flat, one file per responsibility
│   ├── main.c
│   ├── app_state.c
│   ├── audio.c
│   ├── render_background.c
│   └── render_menu.c
├── assets/               # runtime media (images, audio, video)
├── tests/                # test_<module>.c, flat
│   └── test_app_state.c
└── scripts/
    ├── install_deps.sh   # dependency bootstrap (e.g. notcurses)
    └── run_tests.sh
```

- **Header placement:** single header at the component root, named after the
  binary — no `include/` directory while it stays small.
- **`src/` is flat**; files are named by responsibility (`render_menu.c`, …).
- `assets/` holds non-code runtime files; `scripts/` holds dependency install and
  test runners.

---

## Large binary bundle — `src/tetrish`

A shell plus its bundled system programs and a vendored `libft`. Headers go in a
**plural `includes/` directory**, one per domain, and `src/` is **grouped into
sub-directories by domain** rather than flat.

```
src/tetrish/
├── Makefile
├── includes/             # plural; one header per domain
│   ├── minishell.h       #   src/shell/*.c  → #include "minishell.h"
│   ├── system_program.h  #   src/system/*.c → #include "system_program.h"
│   └── common.h          #   src/common/*.c → #include "common.h"
├── src/                  # grouped by domain (NOT flat)
│   ├── shell/            # 00_main.c, 01a_init.c, … numbered by pipeline stage
│   ├── system/           # one .c per bundled binary (find.c, backup.c, …)
│   └── common/           # one helper per file (ft_open.c, perms.c, …)
├── libft/                # vendored library, self-contained (own Makefile/src/includes)
├── tests/
│   ├── unit/             # test_<module>.c
│   ├── integration/      # test_*.sh
│   └── unity/            # vendored Unity test framework
├── files/                # fixtures / sample data
├── scripts/              # run_tests.sh + gen_*_tests.sh generators
├── obj/  bin/            # generated (git-ignored)
├── banner.txt  .macminishellrc  README.md  CLAUDE.md
```

- **Header placement:** plural `includes/`, one header per domain; each `src/`
  sub-directory maps to exactly one header.
- **Tests are split by kind:** `unit/` (C), `integration/` (shell), plus the
  vendored `unity/` framework; generators live in `scripts/gen_*_tests.sh`.
- A nested self-contained library (`libft/`) follows the library layout above in
  miniature.

---

## Choosing between the archetypes

| Component | Header(s) | `src/` layout | Tests |
|---|---|---|---|
| **Library** (`lib/libXXX`) | `include/XXX.h` | flat, one file per module | flat `tests/test_*.c` |
| **Small binary** (`tetrisu`) | single root `XXX.h` | flat, one file per responsibility | flat `tests/test_*.c` |
| **Large bundle** (`tetrish`) | plural `includes/`, one per domain | grouped sub-dirs by domain | `tests/{unit,integration,…}` |

A library always uses `include/`. A binary uses a single root header while it
stays small, and graduates to a plural `includes/` directory with domain-grouped
`src/` sub-directories once it spans several domains.

---

## File naming by responsibility

Files are named to make their place in the pipeline obvious:

- **Shell** (`src/shell/`): numbered by pipeline stage — `00_` main, `01_` init,
  `02_` prompt, `03_` expand, `04_` lexer, `05_` parser, `06_` execute, `07_`
  pipe, `08_` redirection, `09_` builtins, `10_` quote, `11_` signal, `12_` free,
  `13_` utilities. Letter suffixes (`04a`, `04b`, …) are overflow files for the
  same stage.
- **System programs** (`src/system/`): one file per standalone binary
  (`find.c`, `backup.c`, `sys.c`, …), each with its own `main`.
- **Common helpers** (`src/common/`): one helper per file (`ft_open.c`,
  `ft_pipe.c`, `perms.c`, …).
