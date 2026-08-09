# Bug Note — The handshake printed onto the board

## What the bug was

Connecting to tetrisd scrolled the game. Three lines appeared under the
playfield —

```text
Server cert not valid before: Aug  8 16:54:15 2026 GMT
Server cert not valid after:  Aug  9 ...
Server certificate verified successfully.
```

— and because they were written straight to the terminal rather than through
notcurses, the terminal scrolled to make room and every plane notcurses
believed it had placed was now one row higher than where it had drawn it.

They come from `verify_certificate` in `lib/libtetrissh/src/common.c`:

```c
printf("Server cert not valid before: ");
...
printf("Server certificate verified successfully.\n");
```

`common.c` is the frozen course-provided crypto helper. It is not modified —
that is a project constraint, not a preference — so the lines cannot be
removed where they are written.

## Why it was not caught

Nothing that runs in CI owns a screen. `net_smoke` and `net_provider_smoke` are
console programs; the cert report was three more lines of console output in a
test that prints console output, visible in every run and read as noise. It is
only damage when something else is drawing.

The renderer has no automated test at all, for the reason the tetrisu Makefile
states: notcurses probes the terminal for its capabilities and a scripted pty
never answers. So the one component that would have noticed cannot be driven
headless.

## The fix

`console_mute` / `console_unmute` in `net_client.c`, wrapped around the one
call that reaches `common.c`:

```c
console_mute(saved);
handshake = session_handshake_client(net->fd, &net->sess, cfg->ca_path);
console_unmute(saved);
```

`stdout` and `stderr` are pointed at `/dev/null` — or at `$TETRISU_NET_LOG`
when it is set, because a handshake that fails silently is worse than one that
scrolls the screen — and put back afterwards. Both descriptors are saved or
neither is redirected, so a failure cannot leave the caller with no way back,
and the `fflush` happens while the sink is still installed so nothing buffered
arrives on the terminal a statement after the unmute.

Only the handshake is muted. notcurses renders through `stdout`; muting it for
longer would blank the game.

## What to take from this

1. **A library that prints is a library that owns the terminal.** `libcoreipc`
   has "must not log" written into its constraints for a related reason. The
   constraint that was missing here is the reverse one — a *client* of a
   printing library, running under a full-screen renderer, has to assume the
   library will print.

2. **Frozen code is still your problem at the call site.** "We cannot change
   `common.c`" is true and settles nothing about what happens around the call.

3. **Output nobody reads is not output nobody sees.** These three lines were in
   every test run for as long as the network layer has existed.
