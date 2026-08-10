# Bug Note — The chat never left the client

## What the bug was

The waiting room had a complete chat interface. `C` opened a composer,
`t_waiting_room_state` held the line being typed, backspace and Ctrl-U worked,
every printable key was captured so a message containing "s" could not start
the match, and a panel on the right drew the transcript. It had its own state
machine, its own feedback codes (`CHAT_SENT`, `CHAT_EMPTY`, `CHAT_FULL`) and
its own unit tests, and all of them passed.

Nothing was ever sent anywhere.

`waiting_room_send_chat` appended the composed line to `room->chat[]` — an
array on the client's own view model — and returned true. The other players in
the room never learned a message existed. `tetrisd` had no `CHAT` route at all,
so there was nothing to send it to even if the screen had tried.

Two players sitting in the same room, both typing, each saw only themselves.

## Why it survived

Because every layer was individually correct and individually tested. The
composer really did compose. The panel really did draw what it was given. The
state machine really did refuse an empty message. `test_waiting_room_screen.c`
asserted all of it and was right to pass — the missing piece was not inside any
of the units, it was the absence of a call between them.

This is the same shape as the Marketplace before it became
server-authoritative: a view model that answered every question asked of it,
forgot everything when the screen closed, and was never contradicted because
nobody else could see it. A wallet nobody else can see looks exactly like a
working wallet. So does a chat log.

The lesson both times: **a feature whose whole output is a local field is
untestable in the only way that matters.** No unit test can fail on "and then
it should have reached another process", because there is no other process in
a unit test. The check has to be an integration test, or the gap stays
invisible.

## The fix

Chat became one feed served by `tetrisd`, in the same shape as the marketplace
before it:

- `CHAT /room/<name>` on the server, rate limited, refusing `404` when the
  caller is not seated there, `403 muted`, and `400 bad-text`;
- a `t_body_chat` codec in `libstatusbody`, and `application/tetris-chat` in
  `libhtttp` — which made `CHAT` the first method to travel in both directions
  and ended "only `STATE` is server-originated";
- `narrate.c` in `tetrisd`, so the server's own lines (joined, left, ownership
  passed) ride the same feed with no author rather than being a second
  mechanism;
- a **third outbox lane** for it, because chat cannot share the response FIFO:
  a Battle Royale room narrating its knockouts would fill it, and the next
  genuine response would close a connection whose only fault was being slow;
- `net_chat.c` in `tetrisu`, a drop-oldest ring the pushed lines land in, and
  a `send_chat` on the provider vtable.

`waiting_room_send_chat` survives unchanged, as the fallback for a provider
that serves no feed — which is all an offline room can do. What changed is that
it is no longer the only path.

## What the fix had to get right

**The sender's own line comes back down the socket.** `net_send_chat` appends
nothing locally. This is not an oversight — appending would draw the message
twice, and the copy drawn first would be the one without the server's ordering
or its sequence number. The feed everyone sees has to be the same feed, in the
same order, or `seq` means nothing.

**A line that crosses a reply is still filed.** `net_request` reads the socket
too, so the echo of one message arrives while the next is waiting for its own
reply. A client that only asked `net_pump` "did anything arrive?" would lose
most of the feed during any burst of typing — precisely the mistake
`applied_seq` already documents for snapshots
([`the_line_clear_never_reached_the_client.md`](the_line_clear_never_reached_the_client.md)
is the same family). `take_chat` is therefore called from both readers, and the
ring counts every line ever received rather than the ones it still holds.

`src/tetrisu/tests/integration/test_net_chat.sh` is the test that could have caught the
original bug, and the check named "a line crossing a reply is kept" is the one
that cannot pass without a real socket on the other end.
