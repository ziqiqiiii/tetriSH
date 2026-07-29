# README_STYLE.md

FIRST THING FIRST - KEEP IT CONCISE

The conventions for writing a `README.md` in this repository, written so they can
be replicated in other repos. The guiding principle is **big picture first, then
progressive drill-down** — a reader never meets a detail before its context, and
no section over-writes what a table, code block, or diagram can carry.

The canonical examples are the top-level [`README.md`](../README.md) (a large
multi-component system) and [`src/tetrish/README.md`](../src/tetrish/README.md) (a
single self-contained tool); the snippets below are drawn from both.

---

## Table of contents

1. [The one rule: overview → progressive detail](#1-the-one-rule-overview--progressive-detail)
2. [Section order](#2-section-order)
3. [The opener — identity in one sentence](#3-the-opener--identity-in-one-sentence)
4. [Table of contents block](#4-table-of-contents-block)
5. [Detail without over-writing](#5-detail-without-over-writing)
6. [Reference tables](#6-reference-tables)
7. [Code blocks](#7-code-blocks)
8. [Diagrams — architecture & tree](#8-diagrams--architecture--tree)
9. [Inline code for every literal](#9-inline-code-for-every-literal)
10. [Section rules & visual rhythm](#10-section-rules--visual-rhythm)
11. [Tone & grammar](#11-tone--grammar)
12. [Constraints stated in place](#12-constraints-stated-in-place)
13. [Blockquote tagline](#13-blockquote-tagline)
14. [Summary table, then per-item deep-dive](#14-summary-table-then-per-item-deep-dive)
15. [Layered box diagram](#15-layered-box-diagram)
16. [Grammar & enumerable-value blocks](#16-grammar--enumerable-value-blocks)
17. [Numbered runbooks](#17-numbered-runbooks)
18. [Template placeholders](#18-template-placeholders)
19. [Closing matter — authors & attribution](#19-closing-matter--authors--attribution)

---

## 1. The one rule: overview → progressive detail

The whole document is ordered so that **earlier = broader audience and bigger
picture; later = narrower and deeper.** A reader scanning top-to-bottom sees
*what it is*, then *how to run it*, then *what it offers*, then *how it works*,
then *how to test it* — each layer resting on the one above.

Two consequences:
- **Usage comes before internals.** Build/Run/Reference precede
  Architecture/Structure. Nobody reads how the parser works before learning how
  to compile.
- **Every section could be cut from the bottom** and the document above it still
  stands. Detail accretes downward.

---

## 2. Section order

The baseline README follows this sequence. Reuse it; drop sections that don't
apply, but keep the order.

| # | Section | Answers |
|---|---------|---------|
| 1 | Title + one-line summary | *What is this?* |
| 2 | Table of Contents | *What's in this document?* |
| 3 | Features | *What can it do?* (one glance) |
| 4 | Prerequisites | *What do I need first?* |
| 5 | Build | *How do I compile it?* |
| 6 | Run | *How do I start it?* |
| 7 | Reference tables (commands, operators, exit codes, …) | *What can I use?* |
| 8 | Architecture | *How does it work?* |
| 9 | Project Structure | *Where does everything live?* |
| 10 | Testing | *How do I verify / contribute?* |

Prerequisites → Build → Run appear **in the order the user performs them.**

---

## 3. The opener — identity in one sentence

The first content after the title is a **single sentence** that names the
category and lists the concrete capabilities in the same breath:

```markdown
# MacMini Shell

A POSIX-like shell implemented in C. Supports interactive prompts, command
history, pipes, redirections, environment variable expansion, signal handling,
and a set of built-in commands.
```

Rules:
- Sentence 1 = *what it is* (category + language/context). Sentence 2 = *what it
  does*, as a comma list.
- No marketing adjectives ("powerful", "blazing-fast", "welcome to"). Every word
  carries information.
- Follow it with a `---` rule before the Table of Contents.

---

## 4. Table of contents block

Immediately after the opener, list **every top-level section** as anchor links,
so the document's whole shape is visible before any section is read:

```markdown
## Table of Contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Build](#build)
- [Run](#run)
- ...
```

- One bullet per `##` heading, in document order.
- Anchors are the lowercased heading with spaces → `-`.

---

## 5. Detail without over-writing

The core discipline: **carry dense material in the densest form that still
reads.** Prefer, in order — a table, then a code block, then a one-line list
item, and only then a sentence.

- Never write a paragraph where a table row works.
- Never write a sentence where an inline-code literal works.
- After a code block, add **at most one** sentence saying what it produced:

  ```markdown
  This compiles `libft`, all system programs under `./bin/`, and the
  `macmini_shell` binary.
  ```

---

## 6. Reference tables

All "what can I use" material — built-ins, operators, exit codes, sub-programs —
goes in a table. **One row = one thing; one cell = one verb-first clause.**

```markdown
| Command              | Description                                   |
|----------------------|-----------------------------------------------|
| `echo [-n] [args...]`| Print arguments to stdout; `-n` omits newline |
| `pwd`                | Print current working directory               |
| `exit [n]`           | Exit shell with status `n`                    |
```

Rules:
- The left column is the literal (command, operator, code) in `inline code`,
  with its argument grammar: `echo [-n] [args...]`, `cd [path\|'-']`.
- The description is a **single clause, verb first**: "Print arguments to
  stdout", "Redirect stdin from file", "Daemonize a process (double-fork) and log
  spawn events."
- No trailing period on cell descriptions; no multi-sentence cells.

---

## 7. Code blocks

Every action a user performs is a copy-pasteable fenced block, language-tagged:

````markdown
Clone the repository and build with `make`:

```bash
git clone https://github.com/ziqiqiiii/MacMini_Shell.git
cd MacMini_Shell
make
```
````

- One imperative lead-in sentence, then the block.
- Group a command with its alternatives where useful ("Or build and run in one
  step:" → `make run`).
- Show, don't narrate: the block *is* the instruction; prose only states the
  outcome.

---

## 8. Diagrams — architecture & tree

Where a picture beats a paragraph, draw one inside a plain code fence.

**Architecture** — a top-to-bottom flow with `│ ▼` connectors and a
right-hand annotation per stage:

```
readline input
     │
     ▼
  Expander          expand $VAR and $? before tokenising
     │
     ▼
   Lexer            tokenise into COMMAND / PIPE / RDIN / RDOUT / ...
```

**Project Structure** — an annotated `tree`, each entry with a right-aligned
one-line note on its role:

```
MacMini_Shell/
├── src/
│   ├── shell/         Shell pipeline sources (numbered by stage) → macmini_shell
│   ├── system/        Standalone system programs → bin/
│   └── common/        Helpers shared by system programs → libcommon.a
├── includes/      Header files
└── Makefile
```

- Annotations explain *purpose*, not restate the name.
- Use `→ target` to show what a directory builds into.

---

## 9. Inline code for every literal

Any command, flag, filename, path, env var, operator, or code symbol is wrapped
in `inline code` in prose — never spelled out bare.

- Yes: "expand `$VAR` and `$?`", "resolved before the system `$PATH`",
  "programs under `./bin/`".
- No: "expand $VAR", "the PATH variable", "the bin folder".

---

## 10. Section rules & visual rhythm

- A `---` horizontal rule separates **every** top-level `##` section — a
  consistent visual beat down the page.
- Sections use `##`; sub-sections (e.g. "Filtering tests" under "Testing") use
  `###`.
- One blank line around fences, tables, and rules.

---

## 11. Tone & grammar

- **Instructions:** second person, imperative — "Clone the repository and build
  with `make`."
- **Facts:** present tense, declarative — "This compiles `libft`, all system
  programs…", "The shell prepends `$PWD/bin`."
- No first person, no filler ("simply", "just", "as you can see"), no future
  tense for present behaviour.

---

## 12. Constraints stated in place

Caveats, platform limits, and warnings are stated **at the point they matter**,
inline — not collected into a separate "Notes" or "Warnings" dump.

- "Package installation may request sudo access. Use the following when system
  changes are not allowed." — right under Prerequisites, beside the command it
  qualifies.
- "(Linux only; excluded from the build on macOS)" — inside the table cell for
  the program it constrains.

---

## 13. Blockquote tagline

For a larger project, the one-sentence identity (§3) is set as a **blockquote**
directly under the title — a richer tagline that names the moving parts — followed
by a plain line placing the project in context:

```markdown
# MacMini_tetriSH

> A terminal-based Battle Royale Tetris system written in C — combining a custom
> Unix shell, concurrent daemon processes, authenticated encrypted networking,
> and a bespoke application-layer protocol (HTTTP).

Part of the **CoreStack Challenge** (50.003 × 50.005), Singapore University of
Technology and Design.
```

- The blockquote is the "elevator pitch"; the line under it is affiliation /
  context, not a second pitch.
- Use `**bold**` for the one proper noun that matters (the program/challenge
  name), nothing else.

---

## 14. Summary table, then per-item deep-dive

When a set of things (binaries, libraries) each needs more than a one-line cell,
lead with the **summary table** (§6) for the glance, then give **one `###`
subsection per row** for the detail. Overview before depth, applied within a
single section:

```markdown
## Components

| Binary | Role |
|---|---|
| `tetrish` | Interactive shell — reads `.tetrishrc`, launches daemons |
| `tetrisd` | Concurrent game server — accepts clients, manages rooms |

### tetrish

`tetrish` is the entry shell. It implements the full PA1 REPL: ...

### tetrisd

`tetrisd` is the game daemon. When launched in background it:
- Detaches from the controlling terminal
- Binds to the TCP port from `.tetrishrc`
- ...
```

- The subsection heading is the bare item name (`### tetrish`), matching its
  table row.
- Each deep-dive opens by restating the item and its role in one sentence, then
  drops into a **bulleted breakdown** (verb-first lines) for specifics.
- Every row in the summary table gets a subsection; don't expand some and not
  others.

---

## 15. Layered box diagram

For layered or stacked architecture (as opposed to the top-to-bottom *flow* of
§8), draw ASCII boxes, outermost/topmost layer first, with a one-line gloss per
layer:

```
+---------------------------------------------+
|  Application: HTTTP messages                |
|  (HyperText Tetris Transfer Protocol)       |
+---------------------------------------------+
|  Secure session                             |
|  (cert auth, RSA-wrapped AES, framed)       |
+---------------------------------------------+
|  Transport: TCP via POSIX sockets           |
+---------------------------------------------+
```

Follow it with one sentence drawing the boundary — what the project implements
versus what it relies on: "TCP reliability … is provided by the kernel; tetriSH
implements the two layers above it."

---

## 16. Grammar & enumerable-value blocks

Formal specs get their own precise form:

- **Grammars** go in a plain code fence as BNF-style productions, one rule per
  line, aligned on `::=`:

  ```
  REQUEST       ::= REQUEST-LINE *(HEADER CRLF) CRLF [BODY]
  REQUEST-LINE  ::= METHOD SP PATH SP "HTTTP/1.0" CRLF
  ```

- **Short enumerable value sets** (status codes, flags) are a single line of
  comma-separated `inline code`, not a table:

  ```markdown
  `200`, `201`, `400`, `401`, `403`, `404`, `409`, `429`, `500`
  ```

- **Config directives** go in a plain fence with an aligned trailing `#` comment
  per line:

  ```
  listen_port  <port>           # TCP port for tetrisd
  cert_path    <path>           # Server certificate
  ```

---

## 17. Numbered runbooks

A sequential operating procedure (start the config, start the server, connect a
client…) is a **numbered list of bold steps**, each an imperative label followed
by its command block:

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

- Steps are ordered by execution; the bold label says the goal, the block does
  it.
- Distinct from §7 code blocks (isolated actions) — a runbook is one procedure
  performed in order.

---

## 18. Template placeholders

When the README ships ahead of a decision, leave an explicit **bracketed
placeholder** rather than an empty section or an invented answer:

```markdown
**Mechanism:** [document: Unix domain socket / POSIX message queue / named pipe]

| Role | Member | Owns |
|---|---|---|
| Systems | [Name] | tetrish, concurrency, tetrislogd, ... |
```

- Format: `[document …]` for an unmade design choice, `[Name]` / `[…]` for a
  value to fill in.
- The bracket makes an unfinished spot greppable and unmistakably not-yet-done —
  never leave it silently blank or guess.

---

## 19. Closing matter — authors & attribution

Close the document with, in order:

- an **Authors** table — `Role | Member | Owns`, one row per contributor;
- a final `---` rule;
- a single **italic attribution line** naming the project and context:

```markdown
---

*50.003 × 50.005 CoreStack Challenge — SUTD, Summer 2026*
```

- The attribution line is the last thing in the file, italicised, no heading
  above it.
