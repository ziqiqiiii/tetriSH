# MacMini Shell

A POSIX-like shell implemented in C. Supports interactive prompts, command history, pipes, redirections, environment variable expansion, signal handling, and a set of built-in commands.

---

## Table of Contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Build](#build)
- [Run](#run)
- [Built-in Commands](#built-in-commands)
- [Shell Operators](#shell-operators)
- [System Programs](#system-programs)
- [Exit Codes](#exit-codes)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Testing](#testing)

---

## Features

- Interactive prompt with command history (readline)
- Environment variable expansion (`$VAR`, `$?`)
- Single and double quote handling
- Redirections: `<`, `>`, `<<` (heredoc), `>>`
- Pipes (`|`) with full pipeline support
- Signal handling: `Ctrl-C`, `Ctrl-D`, `Ctrl-\`
- Built-in commands: `echo`, `cd`, `pwd`, `export`, `unset`, `env`, `history`, `exit`, `help`, `usage`
- Bundled system programs in `./bin/` (`backup`, `dcheck`, `dkill`, `dplant`, `dspawn`, `find`, `ld`, `ldr`, `sys`), resolved before the system `$PATH`

---

## Prerequisites

### GCC and Make

- **Linux / WSL**: `sudo apt update && sudo apt install build-essential`
- **macOS**: `brew install gcc`

### Readline

`make` automatically checks for readline before building and installs it for you
if it is missing (`libreadline-dev` via `apt` on Linux/WSL, `readline` via Homebrew
on macOS). The Linux path uses `sudo apt-get install`, so you may be prompted for
your password during the first build.

To install it manually instead:

**Linux (Debian/Ubuntu)**
```bash
sudo apt install libreadline-dev
```

**macOS (Homebrew)**
```bash
brew install readline
```

**WSL (Windows Subsystem for Linux)**

WSL runs a Linux distribution, so use the same command as Linux:
```bash
sudo apt update
sudo apt install libreadline-dev
```

---

## Build

Clone the repository and build with `make`:

```bash
git clone https://github.com/ziqiqiiii/MacMini_Shell.git
cd MacMini_Shell
make
```

This compiles `libft`, all system programs under `./bin/`, and the `macmini_shell` binary.

Other Makefile targets:

| Command      | Description                                 |
|--------------|---------------------------------------------|
| `make`       | Build everything (`make all`)               |
| `make run`   | Build and launch `macmini_shell` immediately|
| `make clean` | Remove object files                         |
| `make fclean`| Remove object files and binary              |
| `make re`    | Full rebuild (`fclean` + `all`)             |

---

## Run

```bash
./macmini_shell
```

Or build and run in one step:

```bash
make run
```

---

## Built-in Commands

| Command              | Description                                      |
|----------------------|--------------------------------------------------|
| `echo [-n] [args...]`| Print arguments to stdout; `-n` omits newline    |
| `cd [path\|'-']`      | Change directory; defaults to `$HOME`; `-` goes to `$OLDPWD` |
| `pwd`                | Print current working directory                  |
| `export [name=value]`| Set or display environment variables             |
| `unset [name]`       | Remove environment variables                     |
| `env`                | Print all environment variables                  |
| `history`            | Print command history                            |
| `exit [n]`           | Exit shell with status `n`                       |
| `help`               | List all built-in commands                       |
| `usage [builtin]`    | Show detailed help for a built-in command        |

---

## Shell Operators

| Operator | Description                          |
|----------|--------------------------------------|
| `\|`      | Pipe stdout of left to stdin of right |
| `<`      | Redirect stdin from file             |
| `>`      | Redirect stdout to file (overwrite)  |
| `>>`     | Redirect stdout to file (append)     |
| `<<`     | Heredoc — read stdin until delimiter |

---

## System Programs

Standalone C programs compiled into `./bin/` and resolved ahead of the system `$PATH` (the shell prepends `$PWD/bin`). They can also be run directly.

| Program   | Description                                                       |
|-----------|------------------------------------------------------------------|
| `find`    | Recursively list files whose name contains a keyword             |
| `ld`      | List the current directory's contents with `ls -l`-style permissions |
| `ldr`     | Recursively list all non-hidden files with permissions           |
| `sys`     | Print system information alongside an ASCII logo (Linux only; excluded from the build on macOS) |
| `backup`  | Archive the path in `$BACKUP_DIR` into a timestamped tarball      |
| `dspawn`  | Daemonize a process (double-fork) and log spawn events           |
| `dplant`  | Spawn a named "plant" daemon guarded by an exclusive lock         |
| `dcheck`  | Display the status of all registered daemons                     |
| `dkill`   | Interactively kill one or all registered daemons                 |

---

## Exit Codes

| Code | Meaning                          |
|------|----------------------------------|
| `0`  | Success                          |
| `1`  | General error                    |
| `2`  | Misuse of shell built-ins        |
| `126`| Command found but cannot execute |
| `127`| Command not found                |
| `128`| Invalid argument to `exit`       |
| `130`| Terminated by `Ctrl-C`           |
| `137`| Terminated by signal             |
| `255`| Exit status out of range         |

---

## Architecture

Input flows through these pipeline stages:

```
readline input
     │
     ▼
  Expander          expand $VAR and $? before tokenising
     │
     ▼
   Lexer            tokenise into COMMAND / PIPE / RDIN / RDOUT / RDAPP / HEREDOC
     │
     ▼
   Parser           build a binary syntax tree (BST)
     │
     ▼
  Executor          recurse the BST and dispatch to:
     ├── Pipe handler
     ├── Redirection handler  (< > >> <<)
     └── Built-in / execve
```

One global variable `g_exit_status` tracks the most recent foreground pipeline exit code.

---

## Project Structure

```
MacMini_Shell/
├── src/
│   ├── shell/         Shell pipeline sources (numbered by stage) → macmini_shell
│   ├── system/        Standalone system programs → bin/
│   └── common/        Helpers shared by system programs → libcommon.a
├── includes/      Header files (minishell.h, system_program.h, common.h)
├── libft/         Custom C standard library
├── bin/           System program binaries, resolved ahead of $PATH
├── tests/
│   ├── unit/          Unity-based unit tests
│   ├── integration/   Shell script integration tests
│   └── unity/         Unity test framework
├── scripts/       Helper scripts (e.g. AI unit test generator)
├── obj/           Compiled object files (generated)
└── Makefile
```

---

## Testing

```bash
make unit          # run unit tests (Unity framework)
make integration   # run integration shell scripts
make test          # run both
```

### Filtering tests

Use `FILTER` to run only tests whose path contains a given substring:

```bash
make unit FILTER=lexer         # only tests matching "lexer"
make unit FILTER=09             # all builtin tests (09a–09l)
make integration FILTER=system  # only integration scripts matching "system"
```

### Generating unit tests with AI

```bash
make ai-unit-tests MODULE=<name>
# e.g. make ai-unit-tests MODULE=expand
```
