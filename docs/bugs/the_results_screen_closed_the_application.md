# The results screen closed the application

## Symptom

Pressing Enter or Escape at the end of a Double match did not return to the
waiting room. The whole client exited, with no error and no message.

## What was actually happening

Nothing was crashing. The client was being told to quit, correctly, by a rule
that was wrong one level up.

`tetrisd` destroys a room when its match ends — `record_and_reset` runs and
`room_close` takes the room out of the lobby. The client's route out of a
match is `APP_NAV_BACK`, which `app_screen_parent` maps `DOUBLE →
WAITING_ROOM`. So the first thing `run_waiting_room_screen` does on the way
back is ask for a room that the server has already forgotten:

```c
if (!load_room_view(provider, session))
    return (-1);            /* <- and main.c reads -1 as APP_NAV_QUIT */
```

`app_room_view_load` answered `APP_PROVIDER_INVALID` (the server's `404`),
`load_room_view` returned false, and `main.c`'s

```c
if (run_waiting_room_screen(...) < 0)
    (void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
```

did exactly what it was written to do. Both keys did it because both keys
leave the match: at `MP_MATCH_FINISHED`, Escape returns `true` from the
general exit branch and Enter from the finished-phase branch, and the two
meet at the same `break`.

## The fix

A room that is not there is not a reason to close the game. It is the
*ordinary* way to arrive at the waiting room after a match, so the screen
treats it as navigation rather than as failure: it clears the stale room id,
dispatches `APP_NAV_BACK` again (`WAITING_ROOM → LOBBY`), and returns 0.

`net_provider_smoke.c` pins the contract the fix reads: asking for a room that
does not exist answers `APP_PROVIDER_INVALID`, not `APP_PROVIDER_UNAVAILABLE`.
The distinction is the whole of it — one means "you have no room", the other
means "the session is in trouble", and only the second is worth dying of.

## The lesson

**A return code that means "this screen could not run" is not the same as one
that means "the program cannot continue", and a screen that conflates them
hands the decision to whichever caller happens to be least equipped to make
it.** Every `< 0` in `main.c`'s screen dispatch is a quit; that is a
reasonable default for a screen that genuinely failed, and a disaster for one
that merely found the world had moved on while it was away.

The related design gap named here has since been closed. At the time a Double
room was destroyed when its match ended, so "play again" meant "go back and
make another room", and going back a screen was the honest answer rather than
the final one. `room_rematch` is the final one: a match can end without the
room ending, and both players come back to the seats they never left.
