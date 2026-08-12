# tetriSH

Shared language for the tetriSH system — a terminal Battle Royale Tetris stack
(shell, game server, logger, admin CLI, client, and their libraries). One
context: every component uses these terms with these meanings.

## Language

### People and connections

**Player**:
A registered account — unique username, coins, characters, themes, and game
history — stored in macminidb.
_Avoid_: user, account

**Client**:
A connected program (tetrisu, or the test harness) speaking HTTTP over a
secure session. A client is bound to at most one player, by LOGIN — and a
player to at most one client: two connections never act as the same player at
the same time.
_Avoid_: user, peer

### Lobby and rooms

**Lobby**:
The set of rooms a player can list and join.

**Room**:
A game space with a mode, slots, and a status. A room hosts one game at a time.

**Slot**:
A position in a room that one player occupies.
_Avoid_: seat (as a noun — "seat" is the verb for taking a slot)

**Owner**:
The player who controls a room's start. Ownership passes to a successor when
the owner leaves.
_Avoid_: host, admin

**Mode**:
How many players a room's game is played with: Single, Double, or Battle
Royale.

**Target**:
The player an offensive ability or garbage lands on. Single mode has no
Target, so offensive abilities are unavailable there; Double implies the one
other player; in Battle Royale the Targets are whoever the sender's Targeting
mode names — one player under Randoms and Badges, everyone matched under
Attackers and KOs. Every Target is charged the whole amount; the rows are
never divided between them.
_Avoid_: victim, enemy, opponent (as a role)

**Targeting mode**:
Which kind of rival a Battle Royale player's garbage goes to, and how many of
them it reaches. Two of the four name a situation and hit all of it —
Attackers (everyone who has landed rows on you recently) and KOs (every stack
level with the tallest in the room, which is one player unless they are tied).
Two name a person and hit one, drawn from the room's seeded random source —
Randoms (every player still in the game) and Badges (whoever holds Knockouts),
because both match a crowd and spraying either would put one clear on most of
the room.
A mode whose set is empty falls back to Randoms — one player, drawn — so
choosing one can never cost a player the garbage they earned, and declaring
Attackers before anybody has attacked never hits the whole room.
_Avoid_: aim, lock-on (nothing is held; a mode narrows a set per resolution)

### Gameplay

**Game**:
One round played in a room, from START until it finishes; its outcome is
recorded per player.
_Avoid_: match, round

**Lock Down**:
The moment a falling piece becomes part of the stack. It is not the moment
the piece lands: a landed piece keeps a Lock Delay of half a second, and each
move or rotation buys that back up to fifteen times per piece, refilled
whenever the piece falls past the lowest row it has reached. This is the Tetris
Guideline's Extended Placement, and it is why a piece can be slid into a gap
rather than only dropped onto one. A hard drop is the one input that locks
immediately; a soft drop into the floor does nothing at all.
_Avoid_: settle, land (landing is what starts the delay, not what ends it)

**Top-out**:
Losing a game because the stack leaves no room for the next piece to spawn.

**Knockout**:
A Top-out credited to the player whose garbage most recently *landed* on that
board — at the lock that took the rows, not when they were sent. Rows queued
against a player who clears them away first buried nobody; rows that arrive
after their sender left the room still did. A player who buried themselves is
nobody's Knockout, which keeps the count a measure of aggression rather than
of luck.
_Avoid_: kill, elimination (an elimination is the event; the Knockout is who
is credited for it)

**Placing**:
Where a player finished a Battle Royale, taken the moment they leave the
match rather than when it ends: the number of players still in it, including
them. Everybody eliminated on the same tick shares one Placing and the next
elimination skips the numbers they took, because nothing observable separates
two boards that stopped on the same tick. Leaving mid-match takes a Placing
too — quitting in 40th records 40th.
_Avoid_: rank (that is the leaderboard's word), position, score

**Arena**:
The Battle Royale projection of every occupied seat in a room: one card per
player carrying a one-bit-per-cell mask of their board, their Knockouts,
their Placing and whether they are still alive. It rides its own clock rather
than the board's, and every push is the complete roster — so a seat that does
not appear is a seat nobody is in, never a player who was knocked out.
_Avoid_: grid, minimap, spectator view

**Forfeit**:
The immediate, irrevocable end of a player's participation in an in-progress
game — whether by top-out, by leaving, or by disconnection. All three are the
same event; there is no grace period and no rejoining a game in progress.
_Avoid_: quit, abandon, drop out

### Processes

**Daemon**:
A tetriSH process that runs detached from any terminal, unattended:
`tetrisd` and `tetrislogd`. Being detached is the definition, not who did the
detaching — each of them now does it to itself, in `main.c`, and publishes a
locked pidfile that `tetrisctl` starts, inspects and stops it through. The shell's `dspawn` still
daemonises arbitrary programs, but not these two; and the shell's own
background processes are not daemons in this sense and are no part of the game
system.
_Avoid_: service, background process

### Logging

**Log record**:
One fixed-size event — level, timestamp, component, pid, message — emitted by
a daemon. The unit every counter below counts.

**Component**:
The subsystem a log record came from — `tetrisd`, `tetrislogd`, `chat`,
`market`. Not a process: `chat` and `market` are subsystems of `tetrisd` and
share its pid, and it is the pid that names the process.
_Avoid_: daemon, service, module

**Sink**:
The file `tetrislogd` appends records to. Exactly one logger owns it at a time.
_Avoid_: log file (as a term of art), output

**Dropped**:
A record the producer never sent, because its ring buffer was full. Lost, and
lost inside `tetrisd`.
_Avoid_: discarded, lost

**Rejected**:
A record that reached `tetrislogd` but failed validation. Lost, and lost at the
logger — the two are counted separately because they blame different halves of
the system.

**Degraded**:
A valid record that could not reach the sink and went to stderr instead.
Where it survives depends on where stderr points; for a detached daemon that
is an error file rather than a terminal. Still never report it as Dropped:
Dropped means the producer never sent it, and the two blame different halves
of the system.

### Protocol

**STATE**:
The server-pushed snapshot describing one player's board and progress. One of
the two server-originated messages, the other being a pushed CHAT; everything
else is request/response.
_Avoid_: update, broadcast

**Subject**:
The player whose board a STATE snapshot describes, named by the snapshot's
resource path.

**Feed**:
The ordered list of messages belonging to one Room, numbered by that Room's
own counter. It holds both authors — what players said and what the server
said — and it is not kept: a Room's feed exists only as the lines its members
have already been sent.
_Avoid_: chat log, history, transcript

**Narration**:
A message on the Feed that the server wrote, describing something that
happened to the Room — somebody joined, somebody left, ownership passed. It
has no author. Narration is not a separate mechanism from chat: it is a chat
message with the server as its writer, which is why the two are numbered
together and drawn as one list.
_Avoid_: system message, announcement, event log
