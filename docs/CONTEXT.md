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

**Administrator**:
The person in charge of the server, who acts on it through `tetrisctl`. Not a
player and never in a room: an Administrator is recognised by filesystem
access to the Control channel, not by a login, and holds no standing in the
game. Shortened to *admin* in prose and in the `/admin` resource path.
_Avoid_: operator, owner

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
the owner leaves. Ownership is a standing inside one room, which is what
separates it from an Administrator: the Owner is a player, the Administrator
is never one.
_Avoid_: host, admin (as a room role — an Administrator is a different actor)

**Mode**:
How many players a room's game is played with: Single, Double, or Battle
Royale.

**Target**:
The player an offensive ability or garbage lands on. Single mode has no
Target, so offensive abilities are unavailable there; Double implies the one
other player; in Battle Royale a Target is drawn per resolution from the
room's seeded random source, among players still in the game.
_Avoid_: victim, enemy, opponent (as a role)

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

**Control channel**:
The local, Administrator-only way into a running Daemon, separate from the port
players connect to. Each Daemon has its own. Reaching it is a matter of local
filesystem access rather than of holding a player's identity, which is why
nothing arriving on it names a Player — the reachability *is* the credential.
It carries HTTTP, like everything else two tetriSH processes say to each other.
_Avoid_: control plane, admin socket, control socket

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

**Access line**:
One log record standing for one complete HTTTP exchange — the method that
arrived, who sent it, and the status that went back. It is written once the
answer is known, so a request and its response are a single line rather than
two, and an exchange that failed before reaching a handler still has one.

**Interval report**:
The counter line a Daemon emits on a fixed cadence rather than in answer to an
event, carrying what has changed since the previous one. A delta, not a total,
and emitted even when nothing changed: an interval report of zero is how the
cadence shows it is still running. Each Daemon reports only the counts it can
observe — Dropped is `tetrisd`'s, Rejected and Degraded are `tetrislogd`'s —
and they are shown together but never summed.
_Avoid_: heartbeat, drop summary, periodic dump

**Flush**:
Emptying a producer's pending log records toward the Sink on demand, ahead of
the cadence that would have carried them. It moves records that already exist;
it changes no Daemon state and stops nothing. Deliberately not called a
*drain*: in its ordinary operational sense a drain refuses new work while work
in flight finishes, which is a lifecycle state tetriSH does not currently have,
and the word is left unclaimed for it.
_Avoid_: drain

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
