# Bug Note — The countdown only ticked offline

## What the bug was

Solo online was unplayable. The first piece fell, the 3-2-1 overlay sat on the
board forever, and no key ever did anything.

`solo_mode_run` starts the same way whoever owns the board:

```c
solo_authority_open(&authority, net, &game, new_game_seed());
solo_game_set_personal_best(&game, solo_best_load());
solo_game_start_countdown(&game);
```

and then gates every input behind the countdown, in three places:

```c
if (game->paused || game->countdown_active || game->phase != SOLO_ACTIVE ...)
```

`countdown_active` is cleared by `advance_event_animations`, which is reached
from `solo_game_update` — the *offline* rules. Online the loop calls
`solo_authority_update`, which called `online_update`, which pumped the socket
and applied snapshots and advanced nothing:

```c
static bool	online_update(t_solo_authority *authority, t_solo_game *game)
{
	fresh = net_pump(authority->net);
	...
	return (net_solo_apply(authority->net, game));
}
```

So `countdown_active` stayed `true` for the whole session and every keypress
was swallowed by a gate in front of a timer nobody was running. Gravity still
worked, because gravity was tetrisd's.

There was a second defect underneath the first, and fixing only the timer would
have left it: the client's countdown is 2.4 seconds
(`SOLO_COUNTDOWN_STEP_MS * SOLO_COUNTDOWN_STEPS`) and tetrisd starts gravity the
instant it accepts a `START`. At level one that is two rows the player watches
fall and is not allowed to touch.

## Why it was not caught

`src/tetrisu/tests/integration/test_net_solo.sh` covers the wire and passes: `net_smoke`
calls `net_solo_start`, `net_solo_action` and `net_solo_apply` directly and
never starts a countdown, because the countdown is not part of the protocol.
`test_solo_game.c` covers the countdown and passes: it drives
`solo_game_update`, which is the offline path.

Both sides of the seam were tested. The seam — `solo_authority.c`, the only
file that knows an online game still has client-side timers — was tested by
neither, and it is the file `solo_mode.c` actually calls.

## The fix

Three changes, in one place each:

- `solo_game_update_presentation` is the four `advance_*` timers — ability
  feedback, personal best, event animations (the countdown among them) and the
  danger fade — reachable without the rules that follow them.
  `solo_game_update` calls it first and is otherwise unchanged, so there is one
  definition of what "presentation" means rather than two.
- `online_update` runs it, and takes `elapsed_ms` to do so. Online, it is the
  only thing that ticks.
- `t_solo_authority` grew `countdown_hold`. Opening (or restarting) an online
  game sends `PAUSE`, and the countdown running out sends `RESUME`. The two
  clocks now agree about when the game starts. The pause overlay already yields
  to the countdown overlay when both are up, so the screen is unchanged.

The regression test is `src/tetrisu/tests/integration/test_solo_authority.sh`, driving
`solo_authority.c` against a real tetrisd. Reverting the presentation tick
fails four of its six checks; reverting the hold fails the first.

## What to take from this

1. **Two paths through one gate need one owner of the gate.**
   `countdown_active` was written by presentation and read by input, and the
   two lived on opposite sides of an `if (!authority->online)`. The offline
   branch happened to run both.

2. **Test the layer the caller calls.** `net_smoke` tests the layer below
   `solo_authority.c` and `test_solo_game.c` the layer beside it. Neither
   composes them the way `solo_mode.c` does, which is where the defect was.

3. **A client-side animation in front of a server-side clock is a
   synchronisation problem, not a decoration.** "Presentation stays on the
   client" is true of what is *drawn*; it stops being true the moment the
   drawing also decides when input is accepted.
