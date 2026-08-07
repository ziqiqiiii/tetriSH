# Bug Note — A room runtime outlived the room it described

## What the bug was

`tetrisd` keeps two objects per room, addressed by the same index:

- `t_room` — the pure domain object, owned by `libtetrisroom` and handed out and
  taken back by `lobby_create_room` / `lobby_destroy_room`;
- `t_server_room` — the runtime beside it, owned by `tetrisd`: the per-slot
  `t_game` boards, a `dirty[]` flag per slot, and `ticking`.

Only the first has a lifecycle. `lobby_destroy_room` frees an index for reuse;
nothing blanked the runtime attached to it, so a destroyed room left its games
and — after [ADR-0008](../adr/0008-tetrisd-is-event-driven.md) step 4 — its
`ticking` flag behind for whoever got that index next.

The sequence that bites:

```text
amber JOIN /rooms         -> lobby index 0, room S-01
amber START               -> rooms[0].ticking = true
amber LEAVE               -> forfeit, room empty, lobby_destroy_room(S-01)
                             rooms[0].ticking is STILL true
blake JOIN /rooms         -> lobby index 0 is free, so blake gets it
next gravity tick         -> walks rooms[0] because ticking is true
                             room_is_over(): no active games -> "the game ended"
                             record_and_reset() -> room_finish(), every slot cleared
                             destroy_if_empty() -> blake's room destroyed
```

Blake created a room and was silently evicted from it, by a tick belonging to a
game that had already finished. Nothing errored; the room simply stopped
existing, within one tick period of being created.

## Why it was not caught

The window is one tick — 12 ms by default — between the destroy and the next
timer expiry, so it needed a second `JOIN` to land inside it. Nothing in the
suite did, and nothing would have reliably: at the shipped tick period this is
a race a test loses far more often than it wins.

It is not purely an ADR-0008 defect. Under the ticker threads the same runtime
outlived the same room, and the same stale games were there to be found. What
the threaded version had was `server_room_begin`'s `pthread_join` of the
previous ticker, which narrowed the window without closing it. Step 4 deleted
the join, because a timer has no thread to join — and deleted the accidental
mitigation with it.

## The fix

The immediate fix was to blank the runtime wherever the room was destroyed —
one place, because there was one place a room was destroyed. That closed the
window but left the shape that opened it: two objects, one index, and a
create/destroy pair written down for only one of them, with the two calls that
bracket it sitting in different files.

So the pairing moved into the type instead. `t_server_room` is now the only
Room `tetrisd` knows, and `room.c` owns both halves of it:

- `server_room_open` is the only caller of `lobby_create_room`, and the static
  `room_close` the only caller of `lobby_destroy_room`. Closing a room and
  blanking its runtime is one call, and opening one blanks it again — so a
  reused index cannot see the last game either way.
- Nothing outside `room.c` reaches through `->room`. Seating, starting,
  applying an input and describing a room are all asked of the Room
  (`server_room_seat`, `server_room_start`, `server_room_input`,
  `server_room_describe`), so no other file can put the two halves out of step
  by touching one of them.

The regression test is
`test_a_destroyed_room_does_not_take_its_successor_with_it` in
`src/tetrisd/tests/test_lobby.c`. It runs the fixture at `tick_ms = 1000` so the
second `JOIN` certainly lands inside the window, which turns a race the suite
would lose sometimes into a case it fails every time without the fix.

## What to take from this

1. **Two objects sharing an index share a lifecycle, whether you wrote one or
   not.** The runtime had no create/destroy pair because it is a fixed array
   member and never allocated — so nothing prompted anyone to ask what happens
   when its partner is destroyed. Storage lifetime and *meaning* lifetime are
   different things, and only the first one is obvious from the declaration.

2. **A flag that says "this is live" is the dangerous kind of stale state.**
   Leftover `t_game` boards were harmless: the next `START` overwrote them. The
   `bool` that decides whether anything reads them at all was not, because a
   stale `true` gives dead data an audience.

3. **Deleting a synchronisation primitive can delete a guarantee nobody wrote
   down.** The `pthread_join` in `server_room_begin` was there to stop a thread
   leaking, and its effect on this window was incidental — which is exactly why
   removing it was not obviously load-bearing. When a migration removes a
   `join`, a lock or a wait, the question is not only "what did this protect?"
   but "what did it happen to serialise?".

4. **Reuse is when stale state stops being invisible.** Everything here was
   correct while indices were never handed out twice. `lobby_destroy_room`
   existed precisely so they would be, and the runtime beside it was never
   revisited.
