# A player has one connection; a second LOGIN displaces the first

A player may be bound to at most one connection at a time. When a LOGIN
succeeds for a player who is already bound elsewhere, the older connection is
closed and the new one takes the identity — the newcomer wins, and the login
does not return until the displaced connection has finished tearing itself
down.

The alternative was to refuse the second login with `409`. It reads safer, and
it is one lookup instead of forty lines, but it fails in the case that actually
happens: a client that dies without closing its socket — `kill -9`, a shut
laptop lid, a dropped WiFi association — leaves a half-open TCP connection whose
reader thread stays blocked in `session_recv` until the kernel's keepalive gives
up on it, which by default is around two hours. Refusing would lock a player out
of their own account for that whole window, and the failure would be invisible:
the server looks healthy, the account simply stops working. Displacement makes
reconnecting always work, which is the behaviour a room full of players on
campus WiFi needs.

The security objection — that this lets somebody kick you off — does not
survive contact with the threat model. Displacement requires a successful
LOGIN, so an attacker who can do it already holds the password and already owns
the account. Nothing is lost that was not already lost.

## Why the login waits

Displacement is synchronous, and that is the load-bearing part rather than a
detail of tidiness. A connection forfeits its room *before* it unlinks itself
from the registry (`teardown` in `src/tetrisd/src/client.c`), and `room_release`
is addressed by player id, not by connection. If the new connection were bound
while the old one's teardown was still in flight, that teardown's forfeit would
match the player who is now seated in a *new* room, and evict them from it.

So `login_handler` closes the old socket, then waits until no other connection holds
that player id before binding — at which point the old forfeit has provably
already run. The wait is bounded by `TD_DISPLACE_WAIT_MS`; a login that somehow
outlasts it is answered `503` rather than binding into an unresolved state.

## Relationship to the earlier decisions

This does not reintroduce the resumable sessions
[ADR-0001](./0001-tetrisd-owns-auth-connection-owned-identity.md) ruled out.
Nothing is resumed: the returning player completes a fresh handshake, a fresh
LOGIN, and receives a fresh binding. There are still no tokens and identity
still cannot outlive a connection. What ADR-0001 rules out is resuming a
*session*; what this allows is starting a new one.

Nor does it soften [ADR-0002](./0002-disconnect-is-a-forfeit.md). The displaced
connection forfeits exactly as any dropped connection does — the game is over
and recorded before the new connection binds. A player can come back to the
lobby; they cannot come back to the game they were playing.

## Consequences

- The registry rwlock now guards connection *identity* as well as connection
  lifetime: `player_id` and `state` are written under it, because they are what
  every other thread matches on.
- `LOGIN` gains a reachable `503`, for a displacement that did not complete.
- A client that reconnects during its own game will find that game forfeited.
  This is intended, and follows from ADR-0002 rather than from anything here.
