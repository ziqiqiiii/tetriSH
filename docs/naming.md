# Naming Conventions

The rule is one sentence: **a reader who has never opened the header should be
able to say what a name means.**

Everything below follows from that. `TD_DEF_PORT` fails it — three
abbreviations deep, and "DEF" could be *default*, *definition* or *defer*.
`h_signup` fails it — `h` is a letter, not a word. `rb_drops` fails it twice:
`rb` means nothing, and `drops` could be a verb.

This document is both the standing convention and the plan for the one-time
rename that reaches it. Section 7 is the execution procedure; sections 3 to 5
are the complete mapping.

> **Stage 1 is applied.** Every rename in §3 has landed in `src/tetrisd`: 65
> symbols renamed one-for-one, `net_mkdir_p` deleted in favour of
> `daemon_mkdir_p`, `net.c` split into `listener.c` and `clock.c`, and every
> remaining abbreviated filename (`cfg.c`, `log.c`, `h_*.c`, `test_cfg.c`,
> `test_log.c`) renamed to match its symbols. 7 of 7 suites pass and every test
> binary is valgrind-clean. The old names in §3 are kept as the historical
> mapping — they no longer exist in the code.
>
> **Stage 2 is applied.** All four libraries in §4 are renamed —
> `libcoreipc`, `libcoredaemon`, `libstatusbody`, `libtetrissh` — along with
> their macros and their consumers in `tetrisd`, `tetrislogd` and `tetrisctl`.
> Every suite in the repo passes and every renamed library's test binaries are
> valgrind-clean. As with §3, the old names below are the historical mapping.
>
> **Stage 3 is applied.** `tetrislogd` and `tetrisctl` (§5) now match the
> convention `tetrisd` reached in stage 1, and `tetrislogd`'s duplicate
> `cfg_mkdir_*` is deleted in favour of `libcoredaemon`'s. Every suite passes
> and both components' test binaries are valgrind-clean. It also brought
> `tetrislogd`'s four `.tetrishrc` keys onto `tetrisd`'s `<THING>_PATH` form
> (§5.6) — the one behaviour change in this document, verified by booting the
> stack rather than by tests alone.
>
> With that, every prefix in §2 is current and no stage is outstanding.

---

## Table of Contents

- [1. Principles](#1-principles)
- [2. The prefix map — what is already taken](#2-the-prefix-map--what-is-already-taken)
- [3. Stage 1 — `tetrisd`](#3-stage-1--tetrisd)
- [4. Stage 2 — the libraries](#4-stage-2--the-libraries)
  - [4.1 Macros](#41-macros)
  - [4.2 Names that are not a straight prefix swap](#42-names-that-are-not-a-straight-prefix-swap)
  - [4.3 `daemon_` collides with the shell, deliberately](#43-daemon_-collides-with-the-shell-deliberately)
- [5. Stage 3 — `tetrislogd` and `tetrisctl`](#5-stage-3--tetrislogd-and-tetrisctl)
  - [5.1 Macros](#51-macros)
  - [5.2 `tetrislogd`](#52-tetrislogd)
  - [5.3 `tetrisctl`](#53-tetrisctl)
  - [5.4 `daemon_` again, and why `tetrisctl` could not have it](#54-daemon_-again-and-why-tetrisctl-could-not-have-it)
  - [5.5 The duplicate that was deleted](#55-the-duplicate-that-was-deleted)
  - [5.6 The `.tetrishrc` keys](#56-the-tetrishrc-keys)
- [6. Decisions still open](#6-decisions-still-open)
- [7. How to execute a rename safely](#7-how-to-execute-a-rename-safely)
- [8. Sequencing against ADR-0008](#8-sequencing-against-adr-0008)

---

## 1. Principles

1. **No abbreviation a newcomer has to learn.** `cfg` → `config`, `reg` →
   `registry`, `ob` → `outbox`, `rt` → spelled out. Established C idiom is
   exempt: `ms`, `fd`, `id`, `max`, `min`, `len`, `cap`, `ptr`, `argc/argv`,
   `errno`. The test is whether the abbreviation is standard *outside* this
   repo, not whether it is familiar inside it.
2. **Macros carry the full component name.** `TETRISD_*`, `TETRISLOGD_*`,
   `TETRISCTL_*`. This is not verbosity for its own sake — those are exactly
   the `.tetrishrc` key prefixes, so `TETRISD_DEFAULT_PORT` visibly names the
   default for `TETRISD_PORT`. Today `TD_DEF_PORT` and `TETRISD_PORT` look
   unrelated.
3. **Functions read `subject_verb`.** `registry_add`, `outbox_push`,
   `client_spawn`. The subject is the thing being acted on, and it matches the
   type the function takes.
4. **Request handlers read `method_handler`.** `signup_handler`,
   `move_handler`. They are the one place the noun comes first, because the
   HTTTP method is the identity of the thing and "handler" is its role. A
   function in `h_*.c` that is *not* a route handler does not get the suffix —
   it gets an ordinary `subject_verb` name.
5. **A prefix names one module.** If a prefix covers unrelated
   responsibilities, the prefix is wrong and the module should be split
   (see `net_` in §3.3).
6. **A prefix must not collide with a linked library.** See §2.
7. **Types keep `t_`.** That is 42 style and is not up for negotiation; only
   the body of the name changes. `t_cfg` → `t_config`, `t_room_rt` →
   `t_server_room`.
8. **One word, one meaning, repo-wide.** "Path" currently means both a
   filesystem path and an HTTTP route. Pick one and rename the other.
   Domain words follow [`CONTEXT.md`](CONTEXT.md) — a function that operates on
   a Target says `target`, not `victim` or `opponent`.

### Not covered by this document

Local variable names and static helper names inside a `.c`. They follow the
same principles, but they are file-local, reviewed with the file, and
enumerating them here would be noise. Rename them opportunistically when you
touch a file.

---

## 2. The prefix map — what is already taken

`tetrisd` links eight static libraries into one binary, so every public symbol
shares a namespace. This table is the constraint any new prefix has to clear.
It is also why `t_room_rt` exists at all: `room_*` belongs to `libtetrisroom`,
so `tetrisd`'s room runtime could not use it.

| Prefix | Owner | Meaning |
|---|---|---|
| `board_` `piece_` `gravity_` `score_` `level_` `charge_` `effect_` `ability_` | `libtetrisbrain` | pure game logic |
| `room_` `slot_` `lobby_` `membership_` | `libtetrisroom` | pure lobby/room domain |
| `db_` | `libmacminidb` | player store (public) |
| `catalogue_` `flusher_` `hashmap_` `log_` `node_` `owned_` `player_` `random_` `recovery_` `skiplist_` | `libmacminidb` | store internals |
| `session_` | `libtetrissh` | secure session (public) |
| `sessionio_` | `libtetrissh` | session internals |
| `htttp_` | `libhtttp` | protocol |
| `body_` | `libstatusbody` | body codecs |
| `logrecord_` `ring_` `selfpipe_` `unixsock_` `msgqueue_` | `libcoreipc` | IPC primitives |
| `daemon_` | `libcoredaemon` | daemonising — but see §4.3 |

Note `log_` in that table. `libmacminidb` already owns it internally, and
`tetrisd` also defines `log_init`, `log_emit`, `log_shutdown`. They do not
collide at link time today, but the name gives no clue which layer you are
looking at. `tetrisd`'s becomes `logger_*`.

---

## 3. Stage 1 — `tetrisd`

Scope: `src/tetrisd/include/tetrisd.h`, `src/tetrisd/src/*.c`,
`src/tetrisd/tests/*.c`, plus the prose in `src/tetrisd/README.md` and
`CLAUDE.md`. 149 macro occurrences in `src/`, 48 in `tests/`; roughly 350
function-name occurrences across both.

### 3.1 Macros — `TD_` becomes `TETRISD_`

**Defaults.** Each one is the compiled default for the `.tetrishrc` key of the
same name, and after this rename that relationship is visible.

| Now | Becomes | Backs |
|---|---|---|
| `TD_DEF_PORT` | `TETRISD_DEFAULT_PORT` | `TETRISD_PORT` |
| `TD_DEF_DATA_DIR` | `TETRISD_DEFAULT_DATA_DIR` | `TETRISD_DATA_DIR` |
| `TD_DEF_CONFIG_DIR` | `TETRISD_DEFAULT_CONFIG_DIR` | `TETRISD_CONFIG_DIR` |
| `TD_DEF_CERT` | `TETRISD_DEFAULT_CERT_PATH` | `TETRISD_CERT_PATH` |
| `TD_DEF_KEY` | `TETRISD_DEFAULT_KEY_PATH` | `TETRISD_KEY_PATH` |
| `TD_DEF_CA` | `TETRISD_DEFAULT_CA_PATH` | `TETRISD_CA_PATH` |
| `TD_DEF_LOG_IPC` | `TETRISD_DEFAULT_LOG_IPC_PATH` | `TETRISD_LOG_IPC` |
| `TD_DEF_PID` | `TETRISD_DEFAULT_PID_PATH` | `TETRISD_PID_PATH` |
| `TD_DEF_ERR` | `TETRISD_DEFAULT_ERR_PATH` | `TETRISD_ERR_PATH` |
| `TD_DEF_MAX_CLIENTS` | `TETRISD_DEFAULT_MAX_CLIENTS` | `TETRISD_MAX_CLIENTS` |
| `TD_DEF_TICK_MS` | `TETRISD_DEFAULT_TICK_MS` | `TETRISD_TICK_MS` |
| `TD_DEF_BR_SLOTS` | `TETRISD_DEFAULT_BATTLE_ROYALE_SLOTS` | `TETRISD_BR_SLOTS` |
| `TD_DEF_INPUT_BURST` | `TETRISD_DEFAULT_INPUT_BURST` | `TETRISD_INPUT_BURST` |
| `TD_DEF_INPUT_RATE` | `TETRISD_DEFAULT_INPUT_RATE` | `TETRISD_INPUT_RATE` |

**Validation bounds.** Reordered so the subject comes first and the bound last,
which sorts the pairs together and reads as "port, minimum" rather than
"minimum port".

| Now | Becomes |
|---|---|
| `TD_MIN_PORT` / `TD_MAX_PORT` | `TETRISD_PORT_MIN` / `TETRISD_PORT_MAX` |
| `TD_MIN_TICK_MS` / `TD_MAX_TICK_MS` | `TETRISD_TICK_MS_MIN` / `TETRISD_TICK_MS_MAX` |
| `TD_MIN_INPUT_LIMIT` / `TD_MAX_INPUT_LIMIT` | `TETRISD_INPUT_LIMIT_MIN` / `TETRISD_INPUT_LIMIT_MAX` |
| `TD_MAX_CLIENT_CAP` | `TETRISD_MAX_CLIENTS_LIMIT` |

**Sizes and capacities.** `_MAX` on a buffer is ambiguous about its unit, so
byte counts say so.

| Now | Becomes |
|---|---|
| `TD_PATH_MAX` | `TETRISD_FILESYSTEM_PATH_MAX` |
| `TD_FRAME_MAX` | `TETRISD_FRAME_MAX_BYTES` |
| `TD_BODY_MAX` | `TETRISD_BODY_MAX_BYTES` |
| `TD_LINE_MAX` | `TETRISD_CONFIG_LINE_MAX` |
| `TD_PASSWORD_MAX` | `TETRISD_PASSWORD_MAX` |
| `TD_OUTBOX_CAP` | `TETRISD_OUTBOX_CAPACITY` |
| `TD_LOG_RING_CAP` | `TETRISD_LOG_RING_CAPACITY` |
| `TD_LOG_DRAIN_MAX` | `TETRISD_LOG_DRAIN_MAX` |
| `TD_SHIPPER_WAIT_MS` | `TETRISD_LOG_SHIPPER_WAIT_MS` |
| `TD_TOKEN_SCALE` | `TETRISD_TOKEN_SCALE` |

`FILESYSTEM` is spelled out, and both shorter forms were tried and rejected.
`FS_` shipped first and did not survive first contact — the first question
anyone asked of it was what the two letters meant, which is principle 1's test
failing in one move. `FILE_` was the next attempt and is worse in the place it
matters most: `tetrislogd` already uses `FILE` to mean *the log file*
(`TETRISLOGD_FILE_MODE`, `TETRISLOGD_DEFAULT_FILE`), so
`TETRISLOGD_FILE_PATH_MAX` would read as a cap on that one path rather than on
any path. Principle 8 again, and the long name is the only one that cannot be
misread.

**Routes.** These are HTTTP paths, not filesystem paths, and sharing the word
`PATH` with `TD_PATH_MAX` is the ambiguity principle 8 exists to kill.

| Now | Becomes |
|---|---|
| `TD_PATH_ACCOUNT` | `TETRISD_ROUTE_ACCOUNT` |
| `TD_PATH_SESSION` | `TETRISD_ROUTE_SESSION` |
| `TD_PATH_ROOMS` | `TETRISD_ROUTE_ROOMS` |
| `TD_PATH_ROOM` | `TETRISD_ROUTE_ROOM_PREFIX` |

**Other.**

| Now | Becomes |
|---|---|
| `TD_COMPONENT` | `TETRISD_COMPONENT_NAME` |
| `TD_RC_NAME` | `TETRISD_RC_FILENAME` |
| `TD_KEY_PREFIX` | `TETRISD_CONFIG_KEY_PREFIX` |

**Do not rename** — scheduled for deletion, see §8:
`TD_DISPLACE_WAIT_MS`, `TD_MAX_GAMES`.

### 3.2 Request handlers — `h_` becomes `_handler`

| Now | Becomes |
|---|---|
| `h_signup` | `signup_handler` |
| `h_login` | `login_handler` |
| `h_list` | `list_handler` |
| `h_join` | `join_handler` |
| `h_leave` | `leave_handler` |
| `h_start` | `start_handler` |
| `h_move` | `move_handler` |
| `h_rotate` | `rotate_handler` |
| `h_drop` | `drop_handler` |

Three functions in the `h_*.c` files are **not** route handlers and must not
take the suffix — giving them one would say they are dispatch targets when
they are not:

| Now | Becomes | Why |
|---|---|---|
| `h_authorised` | `request_is_authorised` | predicate on a request |
| `h_hash_password` | `password_hash` | crypto helper |
| `h_make_salt` | `salt_generate` | crypto helper |

The files are renamed too. `h_` was doing one genuinely useful thing — making
every route-handler file sort together in a listing — but a single letter fails
principle 1 in a filename exactly as it does in a symbol. So the grouping stays
and the abbreviation goes, in the plural form that matches the `*_handler`
symbols the files contain:

| Now | Becomes |
|---|---|
| `h_account.c` | `handlers_account.c` |
| `h_lobby.c` | `handlers_lobby.c` |
| `h_input.c` | `handlers_input.c` |

### 3.3 Modules — spelled-out prefixes

| Now | Becomes | Notes |
|---|---|---|
| `cfg_*` | `config_*` | `cfg_resolve_rc` → `config_resolve_rc_path` |
| `cli_*` | `client_*` | `cli_handle_frame` → `client_handle_frame` |
| `reg_*` | `registry_*` | straight swap across 12 functions |
| `ob_*` | `outbox_*` | straight swap across 6 functions |
| `log_*` | `logger_*` | frees `log_` for `libmacminidb`; `log_dropped` → `logger_dropped_count` |
| `req_*` | `request_*` | `req_bodyf` → `request_body_printf` |
| `room_rt_*` | `server_room_*` | see §6 — this one is a real choice |
| `game_*` | *unchanged* | already `subject_verb` and unambiguous |
| `server_*` | *unchanged* | already clear |
| `signals_*` | *unchanged* except `signals_take_dump` → `signals_take_state_dump` | |

Two loose functions with no prefix at all, which is worse than a bad prefix
because a bare `reply` is a plausible symbol in any C program:

| Now | Becomes |
|---|---|
| `reply` | `request_reply` |
| `state_dump` | `server_state_dump` |
| `input_take_token` | `rate_limit_take_token` |

**`net_` is split.** It is three unrelated modules wearing one prefix, and a
mechanical swap would have preserved the problem:

| Now | Becomes | New home |
|---|---|---|
| `net_listen` | `listener_open` | `net.c` |
| `net_accept` | `listener_accept` | `net.c` |
| `net_now_ms` | `clock_now_ms` | `clock.c` (new) |
| `net_elapsed_ms` | `clock_elapsed_ms` | `clock.c` (new) |
| `net_mkdir_p` | **delete** | see below |

`net_mkdir_p` duplicates `cd_mkdir_p`, which `tetrisd` already links from
`libcoredaemon`. Delete the local copy and call the library's. This is the one
entry in this document that changes behaviour rather than spelling, so it gets
its own commit and its own test run.

### 3.4 Types

| Now | Becomes |
|---|---|
| `t_cfg` | `t_config` |
| `t_outmsg` | `t_outbound_message` |
| `t_reqctx` | `t_request_context` |
| `t_room_rt` | `t_server_room` |
| `t_server` `t_client` `t_logger` `t_outbox` `t_game` `t_registry` `t_client_state` `t_input_action` | *unchanged* |

---

## 4. Stage 2 — the libraries

The libraries have the same disease in a more concentrated form. Nothing about
`rb_push`, `sp_notify`, `us_dgram_bind`, `lr_make`, `mqh_open`, `cd_detach` or
`sb_state_encode` tells a reader what subsystem they belong to.

Blast radius is larger than stage 1 — these are public APIs consumed by
`tetrisd`, `tetrislogd`, `tetrisctl`, `tetrisu` and every library's own test
suite — so this is a separate effort, one library per commit, after stage 1
has settled.

| Prefix | Becomes | Library | Example |
|---|---|---|---|
| `rb_` | `ring_` | `libcoreipc` | `rb_push` → `ring_push` |
| `sp_` | `selfpipe_` | `libcoreipc` | `sp_notify` → `selfpipe_notify` |
| `us_` | `unixsock_` | `libcoreipc` | `us_dgram_bind` → `unixsock_dgram_bind` |
| `lr_` | `logrecord_` | `libcoreipc` | `lr_make` → `logrecord_make` |
| `mqh_` | `msgqueue_` | `libcoreipc` | `mqh_recv_nb` → `msgqueue_recv_nonblock` |
| `cd_` | `daemon_` | `libcoredaemon` | `cd_pid_claim` → `daemon_pid_claim` |
| `sb_` | `body_` | `libstatusbody` | `sb_state_encode` → `body_state_encode` |
| `tsh_` | `sessionio_` | `libtetrissh` | internal header only; lowest priority |

`db_`, `htttp_`, `session_`, `board_`, `piece_`, `room_`, `lobby_` and the rest
of `libtetrisbrain` and `libtetrisroom` are already words. They stay.

`_nb` as a suffix (`us_dgram_send_nb`, `mqh_recv_nb`) becomes `_nonblock`
throughout — it appears on five functions and means nothing on sight.

### 4.1 Macros

Principle 2 applies to the libraries as it does to the daemons, and the
prefix a library's macros carry is the prefix its functions carry, not the
directory name. `libcoreipc` is the one library whose macros are shared
across modules rather than owned by one, so those take the library's own name:

| Now | Becomes | Library |
|---|---|---|
| `CIPC_LOG_*` | `COREIPC_LOG_*` | `libcoreipc` — shared by every module |
| `SP_READ` / `SP_WRITE` | `SELFPIPE_READ` / `SELFPIPE_WRITE` | `libcoreipc` — one module owns them |
| `CD_*` | `DAEMON_*` | `libcoredaemon` |
| `SB_*` | `BODY_*` | `libstatusbody` |
| `TSH_*` | `SESSIONIO_*` | `libtetrissh` |

One of these needed more than the prefix: `SESSIONIO_IO_*` was the mechanical
result for `TSH_IO_OK` and stuttered, so the enum is `SESSIONIO_OK` /
`SESSIONIO_EOF` / `SESSIONIO_ERR` and its type `t_sessionio_result`.

`CD_PATH_MAX` → `DAEMON_PATH_MAX`, and `FIXTURE_PATH_MAX` in the test
harnesses likewise keeps the short form. Neither was widened to
`FILESYSTEM_PATH_MAX`, because the rule is narrower than it looks: **`FS` is
expanded where it exists, not added where it never was.** §3.1's rename earns
the long name by resolving a real collision — `tetrisd` has both filesystem
paths and HTTTP routes. `libcoredaemon` and the fixtures have no routes, so
`PATH_MAX` there is already unambiguous and the extra word would be noise.

### 4.2 Names that are not a straight prefix swap

Three functions needed more than their prefix replaced, for reasons §1 already
gives:

| Now | Becomes | Why |
|---|---|---|
| `rb_drops` | `ring_dropped_count` | `drops` reads as a verb (§1's own example); matches `logger_dropped_count` from stage 1 |
| `sp_pipe` | `selfpipe_open` | `selfpipe_pipe` stutters; `open` is the verb in `subject_verb` |
| `mqh_recv_nb` | `msgqueue_recv_nonblock` | both the prefix and the `_nb` suffix |

One filename was an abbreviation the symbols no longer share:

| Now | Becomes |
|---|---|
| `src/mq_helpers.c` | `src/msgqueue.c` |
| `tests/test_mq_helpers.c` | `tests/test_msgqueue.c` |
| `src/sb_util.c` / `src/sb_util.h` | `src/body_util.c` / `src/body_util.h` |

`libcoreipc`'s other filenames (`log_record.c`, `ring_buffer.c`,
`unix_dgram.c`, `unix_stream.c`, `fd_signal.c`) already match their symbols or
use exempt idiom, and were left alone.

### 4.4 Types

`libstatusbody` was the only library carrying its prefix into type names, and
all twelve follow the functions: `t_sb_state` → `t_body_state`, `t_sb_cell` →
`t_body_cell`, and so on for `ability`, `cursor`, `piece`, `profile`,
`room_row`, `room_status`, `mode`, `phase`, `clear_label` — plus their `s_`
and `e_` tags. One was not a straight swap: `t_sb_lb_row` →
`t_body_leaderboard_row`, because `lb` is exactly principle 1's target.

`libcoreipc`'s `t_log_record`, `t_log_level` and `t_ring_buffer` were already
words and stayed. See §7 for why these were nearly missed.

### 4.3 `daemon_` collides with the shell, deliberately

`src/tetrish/includes/common.h` already declares `daemon_ready(int ready_fd)`
for `dspawn`/`dcheck`/`dkill`, so `cd_ready` → `daemon_ready` puts two
functions of that exact name and signature in the repo. This was noticed
before the rename and accepted rather than avoided: `tetrish` does not link
`libcoredaemon`, so nothing collides at link time, and the alternative
(`coredaemon_`) buys unambiguity at the cost of every call site.

What this means in practice: **`daemon_*` is `libcoredaemon` everywhere except
under `src/tetrish/`**, where it is the shell's own helper. If a third user of
the prefix ever appears, revisit — two is the most this arrangement carries.

---

## 5. Stage 3 — `tetrislogd` and `tetrisctl`

The two daemons stage 1 skipped. Same disease as `TD_`, same treatment, and
nothing here was a surprise except §5.4.

### 5.1 Macros

`TL_` → `TETRISLOGD_` and `TC_` → `TETRISCTL_`, with the §3.1 expansions
reused verbatim: `_KEY_PREFIX` → `_CONFIG_KEY_PREFIX`, `_LINE_MAX` →
`_CONFIG_LINE_MAX`, `_PATH_MAX` → `_FILESYSTEM_PATH_MAX`, `_RC_NAME` →
`_RC_FILENAME`, `_COMPONENT` → `_COMPONENT_NAME`. `TL_SIG_*` →
`TETRISLOGD_SIGNAL_*`.

`FX_*` — the fixture macros in three test harnesses, including
`libcoredaemon`'s — become `FIXTURE_*`. Tests are code.

### 5.2 `tetrislogd`

| Now | Becomes |
|---|---|
| `cfg_*` | `config_*` (`cfg_resolve_rc` → `config_resolve_rc_path`, as §3.3) |
| `sig_*` | `signals_*` |
| `t_cfg` | `t_config` |
| `logd_*` `sink_*` | *unchanged* — see below |
| `src/cfg.c`, `tests/test_cfg.c` | `src/config.c`, `tests/test_config.c` |

`logd_*` was considered and kept. It is the binary's own name, not a coined
abbreviation, which is the same ground on which §3.3 left `game_*` and
`server_*` alone. `sink_*` is already a word.

`sig_atomic_t` is not `sig_*`. This is exactly why §7 says to rename from a
mapping file of whole names rather than by prefix pattern.

### 5.3 `tetrisctl`

| Now | Becomes | Why |
|---|---|---|
| `cfg_*` | `config_*` | as `tetrislogd` |
| `cmd_start` `cmd_status` `cmd_stop` `cmd_restart` | `start_command` `status_command` `stop_command` `restart_command` | the §3.2 handler rule — the CLI verb is the identity, "command" is the role |
| `d_state` `d_start` `d_stop` | `managed_state` `managed_start` `managed_stop` | see §5.4 |
| `ctl_find` | `ctl_find_daemon` | find *what*; a bare `find` says nothing |
| `t_daemon` | `t_managed` | |
| `t_state` / `TC_STOPPED` … | `t_managed_state` / `MANAGED_STOPPED` … | |
| `t_known` | `t_known_daemon` | |
| `t_ctl` | *unchanged* | the program's own aggregate, as `t_server` is `tetrisd`'s |
| `src/cfg.c` `src/cmd.c` `src/daemon.c` | `src/config.c` `src/commands.c` `src/managed.c` | |

### 5.4 `daemon_` again, and why `tetrisctl` could not have it

`d_*` acts on `t_daemon`, so `daemon_*` was the obvious target — and stage 2
had just given that prefix to `libcoredaemon`, which `tetrisctl` links. Unlike
the `tetrish` case in §4.3, this one is a genuine violation of principle 6:
`daemon_pid_probe` and `daemon_stop` would sit in the same file meaning
different layers, and no reader could tell which without checking the header.

`managed_*` is the answer, taken from the wording already in `tetrisctl.h`
("One managed daemon"). The contrast is now visible at every call site —
`managed_stop` asks, `daemon_pid_wait` answers.

This is the second time `daemon_` has forced a decision in two stages. Both
were resolvable, but the pattern is the warning: a prefix that is an ordinary
English noun for a thing several components each have their own view of will
keep doing this.

### 5.5 The duplicate that was deleted

`cfg_mkdir_p` / `cfg_mkdir_parent` in `tetrislogd` were the twins of
`libcoredaemon`'s `daemon_mkdir_p` / `daemon_mkdir_parent` — the same
situation §3.3 found with `net_mkdir_p`, in the other daemon. Both used mode
`0755`, so the swap is behaviour-preserving in every case the local copy
handled, and the library's additionally rejects a path that runs through a
regular file (`ENOTDIR`) instead of failing later at `open`.

`tetrislogd`'s `test_mkdir_p_creates_parents` went with it:
`libcoredaemon/tests/test_paths.c` covers the same function with five cases
against that one, and a component suite testing its dependency's function
through no seam of its own is coverage in the wrong place.

### 5.6 The `.tetrishrc` keys

`tetrislogd`'s four keys were `SOCK`, `FILE`, `PID` and `ERR` while
`tetrisd`'s were already `PID_PATH`, `ERR_PATH`, `CERT_PATH`, `KEY_PATH`. Two
components naming the same concept two ways is principle 8, and it was
propagating: §3.1 requires a default macro to name the key it backs, so
`TETRISLOGD_DEFAULT_SOCK` inherited the abbreviation.

The rule is now one rule — **`<THING>_PATH`, matching `TETRISD_PID_PATH`**:

| Now | Becomes |
|---|---|
| `TETRISLOGD_SOCK` | `TETRISLOGD_SOCKET_PATH` |
| `TETRISLOGD_FILE` | `TETRISLOGD_LOG_PATH` |
| `TETRISLOGD_PID` | `TETRISLOGD_PID_PATH` |
| `TETRISLOGD_ERR` | `TETRISLOGD_ERR_PATH` |

`FILE` became `LOG_PATH` rather than `FILE_PATH` because "file" names the
container, not the contents — every one of these four is a file. It is the
log, and `TETRISLOGD_LOG_PATH` sits correctly beside `TETRISD_LOG_IPC` and
`TETRISD_LOG_LEVEL`.

**This one is a behaviour change, not a rename**, and it is the only entry in
this document that is. A `.tetrishrc` written before it silently falls back to
compiled defaults rather than failing, so it gets its own commit and a note in
the release history. Three things move together or not at all: the keys in
`.tetrishrc`, the `strcmp` arms in `tetrislogd/src/config.c`, and the
`g_known` table in `tetrisctl/src/config.c` that maps a daemon name to the key
its pidfile path is published under.

Tests are necessary but not sufficient here, because a suite can pass against
consistently-wrong keys. What settles it is booting the stack: `tetrisctl
start`, then confirm the pidfiles, sink and socket appear at the paths
`.tetrishrc` names and not at the compiled defaults, and that a `tetrisd`
record reaches the sink — that last one is the proof `TETRISD_LOG_IPC` and
`TETRISLOGD_SOCKET_PATH` still name the same socket, which no unit test in
either component can check.
---

## 6. Decisions still open

**~~`t_room_rt` → what?~~ Resolved: `t_server_room` / `server_room_*`.** This
was the only rename in stage 1 that was a genuine judgement call rather than an
expansion of an abbreviation, because the obvious name is taken. It is the
server's mutable state beside `libtetrisroom`'s pure `t_room` — the mutex, the
per-slot games, the ticker.

- **`t_server_room` / `server_room_*` — chosen.** Says whose view it is, and
  sits beside the existing `server_*` prefix. Accepted risk: `server_` now
  covers both the daemon lifecycle and room runtime.
- `t_room_runtime` / `room_runtime_*` — most literal, but starts with `room_`,
  so at a glance it reads as a `libtetrisroom` symbol, which is the confusion
  worth avoiding.
- `t_live_room` / `live_room_*` — reads well, but is wrong: the array covers
  every room including `ROOM_WAITING` ones, not just live games.

The local variable was `rt` throughout, which said even less than the type did.
It is now `server_room` — named after its type, 192 occurrences across four
files.

Plain `room` is **not** available for it, which is worth recording so nobody
tries again: `t_server_room` has a `t_room *room` member, so `room->room`
would be the common access, and `src/room.c` and `src/handlers_lobby.c` already
declare a `t_room *room` local in functions that hold the server room too — a
redeclaration, not merely a confusing one. An intermediate `sroom` was tried
and rejected for being the same kind of abbreviation this document exists to
remove.

**~~Stage 2 at all?~~ Resolved: done, all four libraries at once.** The
argument against was that stage 2 touches four libraries that are green,
valgrind-clean and finished, to improve names in code nobody is currently
editing — so it could have been done lazily, one library at a time, whenever
each was next opened for real work. It was done eagerly instead, for the same
reason stage 1 goes before ADR-0008 (§8): a mechanical rename that lands while
the code is quiet is a diff nobody has to review line by line, and a rename
deferred into a library's next real change is one that tangles with it.

The one open question stage 2 raised and did not fully close is `daemon_`
(§4.3) — accepted at two owners, revisit at three.


---

## 7. How to execute a rename safely

A rename is the one refactor where "it compiles and the tests pass" is close to
proof, because C has no reflection and no string-built symbol names. Exploit
that.

**One prefix per commit.** Never batch. A commit that renames `reg_*` and
`ob_*` together cannot be bisected when a suite goes red.

**Use whole-word replacement.** Partial matches are the failure mode —
`log_` is a prefix of nothing here, but `cfg_` sits next to a struct field
literally named `cfg`, and `game_` is a prefix of nothing while `t_game` must
not be touched by a `game_` rule.

**But `\b` will not find a prefix that is not at the start of a word.** This
is the one thing that actually went wrong, twice, and it is worth being
concrete about because it is silent: `\bsb_` does **not** match `t_sb_state`.
There is no word boundary between `_` and `s`, so a sweep for the prefix
reports clean while every type is still misnamed. Stage 2 shipped `body_*`
functions beside `t_sb_*` types for exactly this reason, and stage 1 left 65
`sroom` locals behind on the same mistake. Both were caught later by grepping
for the *component* rather than the prefix.

So after the prefix sweep passes, sweep again without the leading boundary:

```bash
# the sweep that finds what \bold_ misses
git grep -nE '[a-z_]old_' -- . | grep -v docs/naming.md
```

```bash
# one prefix, whole word, tracked files only
git grep -lE '\bold_name\b' -- 'src/tetrisd' \
  | xargs sed -i -E 's/\bold_name\b/new_name/g'
```

Do it name by name from a mapping file rather than by prefix pattern, so a
name that should *not* change (§3.1's deletion list) cannot be swept up.

**Verify the symbol count is unchanged.** This catches an accidental merge of
two names into one, which compiles cleanly and is otherwise invisible:

```bash
nm -g --defined-only src/tetrisd/tetrisd | awk '{print $3}' | sort > /tmp/before
# ...rename, rebuild...
nm -g --defined-only src/tetrisd/tetrisd | awk '{print $3}' | sort > /tmp/after
diff <(wc -l < /tmp/before) <(wc -l < /tmp/after)   # must be identical
```

**Then the full gate**, every commit:

```bash
make -C src/tetrisd re && make -C src/tetrisd test
valgrind --leak-check=full --error-exitcode=1 ...
```

**Sweep the prose last.** `CLAUDE.md`, `src/tetrisd/README.md` and
`docs/diagrams/` name these symbols — 6 macro and ~26 function references
outside the code. They break silently, so grep for the old names once at the
end and fix what is left.

---

## 8. Sequencing against ADR-0008

**Do stage 1 before step 1 of the migration**, not after.

The reason is that [ADR-0008](adr/0008-tetrisd-is-event-driven.md) rewrites
much of `tetrisd`. Renaming first means every line of new reactor code is
written in the final convention, and the rename diff — large, mechanical,
boring — never tangles with the migration diff, which is small, subtle and
needs real review. Renaming afterwards means writing new code in a convention
you have already decided to abandon.

The cost of going first is only the work spent renaming things that ADR-0008
later deletes, and that is avoidable by simply not renaming them:

| Leave alone | Deleted by |
|---|---|
| `TD_DISPLACE_WAIT_MS` | step 5 |
| `reg_wait_absent` | step 5 |
| `reg_wait_empty` | step 5 (registry teardown becomes loop-owned) |
| `TD_MAX_GAMES` | step 7 (games allocated per seated slot) |

Everything else in §3 survives the migration under its new name.

Stage 2 has no such dependency and can happen whenever.
