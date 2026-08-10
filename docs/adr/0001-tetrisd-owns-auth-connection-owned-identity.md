# tetrisd owns authentication; identity is connection-owned

`docs/use_cases.md` sketches a separate "account svc", but no such daemon
exists or will: SIGNUP and LOGIN are ordinary `tetrisd` routes backed by
`libmacminidb`. A successful LOGIN binds the player to the connection itself
(`ctx->player_id`); every authenticated request's `Player-Id` header must
match that binding or the request is rejected with 401. There are no session
tokens, so an identity cannot outlive its connection and no session is ever
resumed — a deliberate trade of resumability for a radically simpler identity
model with no token storage, expiry, or replay concerns.

"No session is resumed" is the precise claim, and it is worth stating exactly:
a player may always connect again and log in again, which mints a *new*
binding on a *new* connection (see
[ADR-0004](./0004-one-connection-per-player.md)). What cannot happen is
picking up a previous session — its identity, its game, or its place in a
room — where it left off.

## Consequences

- A dropped connection ends the player's participation in any in-progress
  game (see [ADR-0002](./0002-disconnect-is-a-forfeit.md)).
- `docs/use_cases.md`'s "account svc" column is wrong and needs correcting.
