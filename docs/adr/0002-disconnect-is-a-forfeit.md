# Disconnect is an immediate forfeit

When a player's connection drops mid-game, the server forfeits them on the
spot: the game result is recorded, their slot is released, and the room plays
on without them (the ticker exits when the room empties). Top-out, an explicit
LEAVE, and a pulled cable all flow through this same forfeit path — one code
path, three triggers.

The rejected alternative was a grace period with reconnection, which would
require session tokens (ruled out by
[ADR-0001](./0001-tetrisd-owns-auth-connection-owned-identity.md)) plus ghost
players and pause semantics in every mode.
