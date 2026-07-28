# libcoreipc implementation via opencode CLI — design

## Goal

`lib/libcoreipc/` is fully spec'd (README.md is the agreed API contract, `include/coreipc.h`
declares every function, `tests/*.c` already exercise every function) but every function body
in `src/*.c` is a stub: `TODO` comment describing the required logic, then `errno = ENOSYS;
return (-1);`. No invention needed — just fill the 5 stub files to match the existing
contract and pass the existing tests.

Delivery constraint from the user: implementation is written by the `opencode` CLI
(`opencode/deepseek-v4-flash-free` model, free tier), one source file at a time. Claude
reviews/builds/tests each increment, reports results, and waits for explicit user
go-ahead before committing + pushing and moving to the next file.

## Scope

5 increments, one per `src/*.c` file, in dependency order:

1. `fd_signal.c` — `us_set_nonblock`, `us_close_unlink`, `sp_pipe`, `sp_notify`, `sp_drain`.
   No dependency on the others; `us_set_nonblock` is used by both socket files below.
2. `ring_buffer.c` — `rb_init`, `rb_push`, `rb_pop`, `rb_drain`, `rb_drops`, `rb_destroy`.
   Self-contained (mutex + atomic counter), no dependency on other stub files.
3. `unix_dgram.c` — `us_dgram_bind`, `us_dgram_open`, `us_dgram_send_nb`, `us_dgram_recv`.
   Uses `us_set_nonblock` from (1).
4. `unix_stream.c` — `us_stream_listen`, `us_stream_accept`, `us_stream_connect`,
   `us_send_all`, `us_recv_all`. Uses `us_set_nonblock` from (1).
5. `mq_helpers.c` — `mqh_open`, `mqh_send_nb`, `mqh_recv_nb`, `mqh_recv_timed`, `mqh_close`,
   `mqh_unlink`. Self-contained.

Out of scope: editing `include/coreipc.h`, any `tests/*.c`, or the `Makefile` — those are
already correct and define the contract each increment is implemented against. If a stub
turns out to need a signature change to satisfy its test, that's a stop-and-ask, not a
silent edit.

## Per-increment workflow

For file N of 5:

1. **Prompt.** Claude writes a prompt naming the exact file, quoting its function
   signatures + `TODO` comments from the stub, the relevant README.md table rows (exact
   contract per function: return codes, errno values, blocking behavior), and the fixed
   constraints below. The prompt instructs opencode to edit only that one `src/*.c` file.
2. **Generate.** Run headless:
   `opencode run -m opencode/deepseek-v4-flash-free --dir lib/libcoreipc --auto "<prompt>"`
   `--auto` auto-approves opencode's own file-edit permission prompts so the run doesn't
   block waiting on a TTY. Scope is local and reversible (git); nothing leaves the
   sandbox and nothing is pushed by opencode itself.
3. **Verify.** Claude runs, in order, stopping at first failure:
   - `make -C lib/libcoreipc re` — must build clean under `-Wall -Wextra -Werror` (already
     enforced by the Makefile).
   - `make -C lib/libcoreipc test FILTER=<module>` — that file's test suite must pass.
   - `valgrind --leak-check=full --error-exitcode=1` on the test binary — must exit 0.
   - Full run: `make -C lib/libcoreipc test` (regression check — earlier increments must
     stay green).
   - Claude reads the diff by hand for the fixed constraints below (grep is not sufficient
     for "did it call printf").
4. **Fix loop.** If verification fails, Claude re-prompts opencode with the specific
   failure (compiler error, failing assertion, valgrind report) and repeats step 3, up to
   2 retries. If still broken after 2 retries, Claude fixes it directly, and says so
   plainly in the report (don't silently pass off a human fix as opencode's work).
5. **Report.** Claude posts: which functions were implemented, build/test/valgrind
   results, diff summary, and anything it fixed by hand. Then stops.
6. **Gate.** Wait for the user's explicit go-ahead. No commit, no push, no next increment
   until then.
7. **Commit + push.** On approval: `git add` the one changed file (+ any local fix),
   commit with a normal (non-caveman) message describing what the file now does, push to
   `origin/feat/libcoreipc`. Then move to increment N+1.

## Fixed constraints (apply to every increment, from CLAUDE.md)

- No `printf`, no logging calls, no `exit()` — this library sits on the log path itself
  and must never recurse into it.
- Errno-style returns only (`-1` + `errno` set, per the README's per-function table).
- C11, `-Wall -Wextra -Werror` clean (Makefile already sets these flags — a warning is a
  build failure, not a lint suggestion).
- No hard-coded paths.
- `valgrind --leak-check=full --error-exitcode=1` clean.
- No mutex held across a blocking syscall (relevant to `ring_buffer.c`'s mutex and
  `mq_helpers.c`'s `mqh_recv_timed`, which the README explicitly says must not be called
  with a lock held).

## Error handling / edge cases

- **opencode produces code that doesn't compile or fails tests**: fix-loop above (2
  retries, then Claude takes over and discloses it).
- **opencode edits a file outside scope** (e.g. touches `coreipc.h` or a test): Claude
  reverts the out-of-scope edit before verification, keeps only the in-scope stub file
  change, and mentions the revert in the report.
- **`mq_helpers` suite requires `/dev/mqueue`**: README already notes the suite
  self-skips when absent — if that happens, Claude reports it as a skip, not a pass, and
  says so.
- **User requests changes at the gate**: Claude asks opencode to revise (or fixes by
  hand for small notes) and re-runs verification before reporting again — the gate
  doesn't advance until the user says go.

## Testing

Existing `tests/test_<module>.c` suites (already written, not modified) are the
acceptance criteria for each increment, plus the blanket valgrind/warnings-as-errors
requirements above. No new tests are written as part of this work.
