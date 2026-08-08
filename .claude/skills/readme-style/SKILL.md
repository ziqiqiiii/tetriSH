---
name: readme-style
description: README conventions for this repository. Use when writing or editing any README.md here, or deciding what belongs in a README versus docs/ or a header.
---

# README style

Three rules govern everything below:

1. **Keep it clean and concise.**
2. **Overview first, then progressive drill-down** — a reader never meets a
   detail before its context.
3. **Carry material in the densest form that still reads** — a table over a
   paragraph, an inline literal over a sentence, nothing over a restatement.

The canonical examples are the top-level `README.md` (a large multi-component
system) and `src/tetrish/README.md` (a single self-contained tool).

---

## Length budget

Concision is a constraint, not an aspiration. A README that exceeds its budget is
a README carrying detail that belongs somewhere else.

| README for | Target | Hard ceiling |
|---|---|---|
| A library (`lib/libXXX/`) | ~150 lines | 250 |
| A binary (`src/XXX/`) | ~200 lines | 250 |
| The repository root | ~400 lines | 600 |

When a section outgrows the budget, move it:

- Full API semantics → the header file's Doxygen comments
- Design rationale, trade-offs, post-mortems → `docs/`
- Per-use-case detail → `docs/diagrams/`
- Style rules → `.claude/skills/`

The README links to those; it does not duplicate them. **One fact lives in
exactly one place.**

---

## What does not belong

Cut on sight:

- **Restatement** — a sentence under a table that says what the table says.
- **Narration of a code block** — the block is the instruction; at most one
  sentence for its *outcome*.
- **Exhaustive API listings** — a function table with every parameter and return
  code. Name the surface; the header documents it.
- **Changelogs, roadmaps, TODOs** — these belong in git history, `ROADMAP.md`, or
  an issue tracker.
- **Marketing** — "powerful", "blazing-fast", "welcome to", "simply", "just".
- **Motivational preamble** — background on why the problem is interesting.

The test: delete the passage. If nothing a reader needs is lost, it stays
deleted.

---

## Section order

Reuse this sequence; drop sections that don't apply, but keep the order. Usage
precedes internals — nobody reads how the parser works before learning how to
compile.

| # | Section | Answers |
|---|---------|---------|
| 1 | Title + one-line summary | *What is this?* |
| 2 | Table of Contents | *What's in this document?* |
| 3 | Features | *What can it do?* (one glance) |
| 4 | Prerequisites | *What do I need first?* |
| 5 | Build | *How do I compile it?* |
| 6 | Run | *How do I start it?* |
| 7 | Reference tables (commands, exit codes, …) | *What can I use?* |
| 8 | Architecture | *How does it work?* |
| 9 | Project Structure | *Where does everything live?* |
| 10 | Testing | *How do I verify / contribute?* |

Prerequisites → Build → Run appear **in the order the user performs them.** Every
section could be cut from the bottom and the document above it still stands.

---

## The opener

Sentence 1 = *what it is*; sentence 2 = *what it does*, as a comma list. Follow
it with a `---` rule.

```markdown
# MacMini_tetriSH

> A terminal-based Battle Royale Tetris system written in C — combining a custom
> Unix shell, concurrent daemon processes, authenticated encrypted networking,
> and a bespoke application-layer protocol (HTTTP).

Part of the **CoreStack Challenge** (50.003 × 50.005), Singapore University of
Technology and Design.
```

The blockquote is the elevator pitch; the line under it is affiliation, not a
second pitch. Bold only the one proper noun that matters.

---

## Table of contents block

Immediately after the opener, list **every `##` section** as anchor links, in
document order, so the document's shape is visible before any section is read.

```markdown
## Table of Contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Build](#build)
```

Anchors are the lowercased heading with spaces → `-`. Skip the block entirely on
a README under ~80 lines — a table of contents for five sections is noise.

---

## Density: table → code → list → prose

Reach for the densest form that still reads, in that order.

- Never write a paragraph where a table row works.
- Never write a sentence where an inline-code literal works.
- After a code block, add **at most one** sentence saying what it produced:

  ```markdown
  This compiles `libft`, all system programs under `./bin/`, and the
  `macmini_shell` binary.
  ```

---

## Reference tables

All "what can I use" material — built-ins, operators, exit codes — goes in a
table. **One row = one thing; one cell = one verb-first clause.**

```markdown
| Command              | Description                                   |
|----------------------|-----------------------------------------------|
| `echo [-n] [args...]`| Print arguments to stdout; `-n` omits newline |
| `pwd`                | Print current working directory               |
| `exit [n]`           | Exit shell with status `n`                    |
```

- Left column: the literal in `inline code`, with its argument grammar —
  `echo [-n] [args...]`, `cd [path\|'-']`.
- Description: a single clause, verb first. No trailing period, no
  multi-sentence cells.
- Caveats go **in the cell they qualify** — "(Linux only; excluded from the build
  on macOS)" — never in a separate "Notes" dump.

---

## Code blocks & runbooks

Every action a user performs is a copy-pasteable fenced block, language-tagged,
with one imperative lead-in sentence:

````markdown
Clone the repository and build with `make`:

```bash
git clone https://github.com/ziqiqiiii/MacMini_tetriSH.git
cd MacMini_tetriSH
make
```
````

A *sequential procedure* (start the config, start the server, connect a client)
is instead a **numbered list of bold steps**, each a goal label plus its block:

````markdown
**1. Copy and edit the config:**
```bash
cp sample.tetrishrc .tetrishrc
```

**2. Start the shell:**
```bash
./bin/tetrish
```
````

---

## Diagrams

Where a picture beats a paragraph, draw one in a plain code fence. Annotations
explain *purpose*; they never restate the name.

**Flow** — top-to-bottom with `│ ▼` connectors and a right-hand note per stage:

```
readline input
     │
     ▼
  Expander          expand $VAR and $? before tokenising
     │
     ▼
   Lexer            tokenise into COMMAND / PIPE / RDIN / RDOUT / ...
```

**Layers** — ASCII boxes, topmost first, one gloss per layer, followed by one
sentence drawing the boundary between what the project implements and what it
relies on:

```
+---------------------------------------------+
|  Application: HTTTP messages                |
+---------------------------------------------+
|  Secure session (cert auth, RSA-wrapped AES)|
+---------------------------------------------+
|  Transport: TCP via POSIX sockets           |
+---------------------------------------------+
```

**Tree** — an annotated `tree`, each entry with a one-line note, `→ target` to
show what a directory builds into. Prune it to the entries a newcomer needs —
not `ls -R`:

```
MacMini_tetriSH/
├── src/
│   ├── tetrish/       Interactive shell → macmini_shell
│   └── tetrisd/       Concurrent game server → tetrisd
├── lib/               Statically linked libraries (libXXX/libXXX.a)
└── Makefile
```

---

## Summary table, then per-item deep-dive

When a set of things (binaries, libraries) each needs more than a one-line cell,
lead with the summary table, then give **one `###` subsection per row**:

```markdown
## Components

| Binary | Role |
|---|---|
| `tetrish` | Interactive shell — reads `.tetrishrc`, launches daemons |
| `tetrisd` | Concurrent game server — accepts clients, manages rooms |

### tetrish

`tetrish` is the entry shell. It implements the full PA1 REPL: ...
```

- The subsection heading is the bare item name, matching its table row.
- Each deep-dive opens with one sentence restating the item's role, then drops
  into verb-first bullets.
- Every row gets a subsection, or none do.

If each item fits in a single cell, **stop at the table** — don't manufacture
subsections to fill.

---

## Grammar & enumerable-value blocks

Formal specs get their own precise form:

- **Grammars** — a plain fence, one BNF-style production per line, aligned on
  `::=`:

  ```
  REQUEST       ::= REQUEST-LINE *(HEADER CRLF) CRLF [BODY]
  REQUEST-LINE  ::= METHOD SP PATH SP "HTTTP/1.0" CRLF
  ```

- **Short enumerable value sets** — one line of comma-separated `inline code`,
  not a table:

  ```markdown
  `200`, `201`, `400`, `401`, `403`, `404`, `409`, `429`, `500`
  ```

- **Config keys** — a plain fence with an aligned trailing `#` comment. Use the
  key's real `.tetrishrc` spelling, `<COMPONENT>_<THING>_PATH`:

  ```
  TETRISD_PORT=<port>           # TCP port for tetrisd
  TETRISD_CERT_PATH=<path>      # Server certificate
  TETRISLOGD_SOCKET_PATH=<path> # Datagram socket tetrisd ships records to
  ```

---

## Inline code, tone, rhythm

Any command, flag, filename, path, env var, operator, or code symbol is wrapped
in `inline code` in prose — never spelled out bare. Yes: "expand `$VAR` and
`$?`", "programs under `./bin/`". No: "expand $VAR", "the PATH variable", "the
bin folder".

- **Instructions:** second person, imperative — "Clone the repository and build
  with `make`."
- **Facts:** present tense, declarative — "The shell prepends `$PWD/bin`."
- No first person, no filler ("simply", "just", "as you can see"), no future
  tense for present behaviour.
- A `---` rule separates **every** top-level `##` section.
- Sections use `##`; sub-sections use `###`. Stop at `###` — a fourth level means
  the content belongs in `docs/`.
- One blank line around fences, tables, and rules.

---

## Placeholders

When the README ships ahead of a decision, leave an explicit **bracketed
placeholder** — `[document: Unix domain socket / POSIX message queue]`, `[Name]`
— rather than an empty section or an invented answer. The bracket makes an
unfinished spot greppable and unmistakably not-yet-done.
