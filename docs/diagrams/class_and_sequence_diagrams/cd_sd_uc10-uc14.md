# tetriSH — Gameplay (UC-10 – UC-14)

## Table of Contents

- [Per-Use-Case Class Diagrams](#per-use-case-class-diagrams)
  - [CD UC-10 — Play Single-Player Game](#cd-uc-10--play-single-player-game)
  - [CD UC-11 — Play Double (2-Player) Game](#cd-uc-11--play-double-2-player-game)
  - [CD UC-12 — Play Battle Royale Game](#cd-uc-12--play-battle-royale-game)
  - [CD UC-13 — Control Falling Piece](#cd-uc-13--control-falling-piece)
  - [CD UC-14 — Activate Gaiden Ability](#cd-uc-14--activate-gaiden-ability)
- [Combined Domain Class Diagram](#combined-domain-class-diagram)
  - [Concept extraction](#concept-extraction)
  - [Domain diagram](#domain-diagram)
- [Sequence Diagrams](#sequence-diagrams)
  - [Layering conventions](#layering-conventions)
  - [SD UC-10 — Play Single-Player Game](#sd-uc-10--play-single-player-game)
  - [SD UC-11 — Play Double (2-Player) Game](#sd-uc-11--play-double-2-player-game)
  - [SD UC-12 — Play Battle Royale Game](#sd-uc-12--play-battle-royale-game)
  - [SD UC-13 — Control Falling Piece](#sd-uc-13--control-falling-piece)
  - [SD UC-14 — Activate Gaiden Ability](#sd-uc-14--activate-gaiden-ability)
- [Solution Class Diagram](#solution-class-diagram)
  - [Method inventory by use case](#method-inventory-by-use-case)
  - [Solution diagram](#solution-diagram)
- [Traceability Matrix](#traceability-matrix)

---

## Per-Use-Case Class Diagrams

### CD UC-10 — Play Single-Player Game

```mermaid
classDiagram
    direction LR

    class GameplayUI {
        <<boundary>>
        -playerId : PlayerId
        +render(view : BoardView) void
        +show(msg : String) void
        +showFinalScore(s : ScoreCard) void
    }
    class HtttpClient {
        <<boundary>>
        +onState(frame : StateFrame) void
    }
    class GameController {
        <<control>>
        +tick(roomId : String) void
        +finish(roomId : String) ScoreCard
    }
    class GameSession {
        -roomId : String
        -mode : GameMode
        -tick : long
        +boardOf(playerId : PlayerId) Board
        +advance() void
        +isOver() bool
        +snapshot() StateFrame
    }
    class Board {
        -cells : Cell[20][10]
        -active : Piece
        -queue : Piece[]
        -hold : Piece
        -topOut : bool
        +gravityTick() BrainResult
        +clearLines() int
    }
    class ScoreKeeper {
        -score : long
        -lines : int
        -level : int
        +onClear(lines : int) void
        +pointsEarned() long
    }
    class ScoreCard {
        <<value object>>
        -scoreDelta : long
        -pointsDelta : long
        -won : bool
    }
    class Db {
        <<interface>>
        +db_record_game(id, scoreDelta, pointsDelta, won) DbResult
    }
    class BrainResult {
        <<enum>>
        BRAIN_OK
        BRAIN_LOCKED
        BRAIN_CLEARED
        BRAIN_GAME_OVER
    }

    GameplayUI ..> HtttpClient : 1. renders pushed STATE
    HtttpClient ..> GameController : MOVE / ROTATE / DROP
    GameController ..> GameSession : 2. advance() on gravity timer
    GameSession *--> "1" Board : single board
    GameSession ..> StateFrame : 4. snapshot pushed
    Board ..> BrainResult : gravity / lock outcome
    Board ..> ScoreKeeper : 4. onClear(lines)
    GameController ..> ScoreCard : 6. builds on game over
    GameController ..> Db : 6. db_record_game (the only write)
    note for GameController "ext 5a quit mid-game → no db_record_game"
```

### CD UC-11 — Play Double (2-Player) Game

```mermaid
classDiagram
    direction TB

    class GameplayUI {
        <<boundary>>
        -playerId : PlayerId
        +renderSplit(own : BoardView, opponent : BoardView) void
        +show(msg : String) void
    }
    class HtttpClient {
        <<boundary>>
        +sendInput(i : InputCommand) HtttpResponse
        +onState(frame : StateFrame) void
    }
    class GameController {
        <<control>>
        +applyInput(playerId : PlayerId, i : InputCommand) HtttpStatus
        +tick(roomId : String) void
        +finish(roomId : String) MatchResult
    }
    class Room {
        -roomId : String
        -status : GameRoomStatus
        +members() Membership[]
        +selectSuccessor() Membership
        +narrate(text : String) void
    }
    class GameSession {
        -roomId : String
        -boards : Map~PlayerId, Board~
        -tick : long
        +advance() void
        +winner() PlayerId
        +isOver() bool
        +dropPlayer(playerId : PlayerId) void
        +snapshot() StateFrame
    }
    class Board {
        -owner : PlayerId
        -topOut : bool
        +injectGarbage(rows : int) void
        +clearLines() int
    }
    class GarbageRouter {
        +route(from : PlayerId, rows : int) PlayerId
    }
    class ScoreKeeper {
        -score : long
        +onClear(lines : int) void
        +pointsEarned() long
    }
    class MatchResult {
        <<value object>>
        -cards : Map~PlayerId, ScoreCard~
        +winnerId() PlayerId
    }
    class Db {
        <<interface>>
        +db_record_game(id, scoreDelta, pointsDelta, won) DbResult
    }
    class GameMode {
        <<enum>>
        DOUBLE
    }

    GameplayUI ..> HtttpClient : 2. concurrent inputs
    HtttpClient ..> GameController : MOVE / ROTATE / DROP
    GameController ..> GameSession : 2. applyInput / 3. advance
    GameSession *--> "2" Board : own + opponent
    GameSession o--> GameMode : DOUBLE
    GameSession ..> GarbageRouter : 4. line clears → garbage
    GarbageRouter ..> Board : injectGarbage
    Board ..> ScoreKeeper : 4. onClear
    GameSession ..> StateFrame : 3. STATE pushed both ways
    GameSession ..> Room : ext 5a quitter → selectSuccessor (UC-07a)
    GameController ..> MatchResult : 5. winner / loser
    GameController ..> Db : 5. db_record_game per player
    note for GameSession "ext 5a quitter dropped, not recorded;<br/>remaining player wins by default"
```

### CD UC-12 — Play Battle Royale Game

```mermaid
classDiagram
    direction TB

    class GameplayUI {
        <<boundary>>
        -playerId : PlayerId
        +renderArena(own : BoardView, others : BoardView[]) void
        +showElimination(rank : int) void
    }
    class HtttpClient {
        <<boundary>>
        +sendInput(i : InputCommand) HtttpResponse
        +onState(frame : StateFrame) void
    }
    class GameController {
        <<control>>
        +applyInput(playerId : PlayerId, i : InputCommand) HtttpStatus
        +tick(roomId : String) void
        +finish(roomId : String) MatchResult
    }
    class GameSession {
        -roomId : String
        -boards : Map~PlayerId, Board~
        -alive : PlayerId[]
        +advance() void
        +eliminate(playerId : PlayerId) int
        +survivorCount() int
        +isOver() bool
        +snapshot() StateFrame
    }
    class Board {
        -owner : PlayerId
        -eliminated : bool
        +injectGarbage(rows : int) void
        +clearLines() int
    }
    class GarbageRouter {
        +pickTarget(from : PlayerId) PlayerId
        +route(from : PlayerId, rows : int) PlayerId
    }
    class Ranking {
        -places : Map~PlayerId, int~
        +record(playerId : PlayerId, rank : int) void
        +lastStanding() PlayerId
    }
    class MatchResult {
        <<value object>>
        -cards : Map~PlayerId, ScoreCard~
        -ranking : Ranking
    }
    class Room {
        -roomId : String
        +selectSuccessor() Membership
        +narrate(text : String) void
    }
    class Db {
        <<interface>>
        +db_record_game(id, scoreDelta, pointsDelta, won) DbResult
    }
    class GameMode {
        <<enum>>
        BATTLE_ROYALE
    }

    GameplayUI ..> HtttpClient : 2. concurrent inputs
    HtttpClient ..> GameController : MOVE / ROTATE / DROP
    GameController ..> GameSession : 2. applyInput / 4. advance
    GameSession *--> "4..99" Board : one per participant
    GameSession o--> GameMode : BATTLE_ROYALE
    GameSession ..> GarbageRouter : 3. N cleared → N-1 rows
    GarbageRouter ..> Board : injectGarbage (random other room, via IPC)
    GameSession ..> Ranking : 5. eliminate → finishing rank
    GameSession ..> StateFrame : 4. STATE for visible boards
    GameSession ..> Room : ext 5b owner quit → UC-07a
    GameController ..> MatchResult : 6. ranking + cards
    GameController ..> Db : 6. db_record_game per survivor at game-over
    note for GameSession "ext 5b quitter removed, not recorded;<br/>5b-i one left → wins by default"
```

### CD UC-13 — Control Falling Piece

```mermaid
classDiagram
    direction LR

    class GameplayUI {
        <<boundary>>
        -playerId : PlayerId
        +onKey(k : Key) InputCommand
        +render(view : BoardView) void
    }
    class HtttpClient {
        <<boundary>>
        +sendInput(i : InputCommand) HtttpResponse
        +onState(frame : StateFrame) void
    }
    class InputCommand {
        <<value object>>
        -kind : InputKind
        -arg : String
        +isDrop() bool
    }
    class GameController {
        <<control>>
        +applyInput(playerId : PlayerId, i : InputCommand) HtttpStatus
    }
    class GameSession {
        -boards : Map~PlayerId, Board~
        +boardOf(playerId : PlayerId) Board
        +snapshot() StateFrame
    }
    class Board {
        -active : Piece
        +move(dir : String) BrainResult
        +rotate(dir : String) BrainResult
        +softDrop() BrainResult
        +hardDrop() BrainResult
        +clearLines() int
    }
    class Piece {
        -kind : PieceKind
        -row : int
        -col : int
        -rotation : int
        +isValid(b : Board) bool
    }
    class InputKind {
        <<enum>>
        MOVE
        ROTATE
        DROP
    }
    class BrainResult {
        <<enum>>
        BRAIN_OK
        BRAIN_BLOCKED
        BRAIN_LOCKED
        BRAIN_CLEARED
        BRAIN_GAME_OVER
    }

    GameplayUI ..> InputCommand : 1. onKey builds
    GameplayUI ..> HtttpClient : 1. sendInput
    HtttpClient ..> GameController : MOVE / ROTATE / DROP /room/id/player/pid
    InputCommand o--> InputKind
    GameController ..> GameSession : 2. boardOf(playerId)
    GameSession ..> Board : authoritative board
    Board *--> "1" Piece : active piece
    Board ..> BrainResult : 2. BLOCKED → ext 2a 409
    GameSession ..> StateFrame : 3. push updated STATE
    HtttpClient ..> GameplayUI : 3. render / correct to authoritative pos
```

### CD UC-14 — Activate Gaiden Ability

```mermaid
classDiagram
    direction TB

    class GameplayUI {
        <<boundary>>
        -playerId : PlayerId
        -charge : int
        +onAbilityKey(level : int) void
        +show(msg : String) void
    }
    class HtttpClient {
        <<boundary>>
        +useAbility(playerId : PlayerId, roomId : String, level : int) HtttpResponse
    }
    class AbilityController {
        <<control>>
        +activate(playerId : PlayerId, roomId : String, level : int) HtttpStatus
    }
    class ChargeMeter {
        -charge : int
        +grant(linesCleared : int) void
        +canAfford(cost : int) bool
        +deduct(cost : int) void
    }
    class Ability {
        <<value object>>
        -level : int
        -cost : int
        -name : String
        +costFor(level : int)$ int
    }
    class Character {
        -characterId : ItemId
        -name : String
        -abilities : bitfield
        +grants(level : int) Ability
    }
    class AbilityEffect {
        <<interface>>
        +apply(caster : Board, target : Board) void
    }
    class Board {
        -owner : PlayerId
        +cutTop(rows : int) void
        +cutBottom(rows : int) void
        +invert() void
        +fillRows(rows : int) void
        +clearCells(n : int) void
        +deleteColumns(cols : int) void
    }
    class GameSession {
        -boards : Map~PlayerId, Board~
        +boardOf(playerId : PlayerId) Board
        +opponentOf(playerId : PlayerId) Board
        +snapshot() StateFrame
    }
    class Room {
        +narrate(text : String) void
    }
    class Db {
        <<interface>>
        +db_player_owns_character(id, cid) DbBool
        +db_get_character(cid, out) DbResult
    }
    class AbilityVerdict {
        <<enum>>
        APPLIED
        NOT_OWNED
        INSUFFICIENT_CHARGE
    }

    GameplayUI ..> HtttpClient : 1. onAbilityKey(level)
    HtttpClient ..> AbilityController : 2. ABILITY /room/id/player/pid
    AbilityController ..> Db : 3. owns_character + get_character (read lock)
    AbilityController ..> ChargeMeter : 4. canAfford then deduct
    AbilityController ..> AbilityVerdict : ext 3a NOT_OWNED / 4a INSUFFICIENT
    Character ..> Ability : 3. abilities bitfield → level
    Ability o--> AbilityEffect : server-enforced effect
    AbilityController ..> AbilityEffect : 4. apply(caster, target)
    AbilityEffect ..> Board : libtetrisbrain primitives
    AbilityController ..> GameSession : 4. caster + opponent boards
    AbilityController ..> Room : 5. ABILITY_USED narrated
    GameSession ..> StateFrame : 5. STATE pushed
    ChargeMeter ..> Board : charge granted per 2 lines cleared
```

---

## Combined Domain Class Diagram

### Concept extraction

| Source (UC + step) | Concept surfaced | Class |
|---|---|---|
| UC-10.1 "renders the board" | the authoritative 20×10 playfield | `Board` |
| UC-10.1 "piece queue (next pieces), the hold column" | the falling tetromino | `Piece` + `PieceKind` `«enum»` |
| UC-10.2 "spawns falling pieces on a gravity timer" | the live match clock | `GameSession.tick` |
| UC-10.4 "clears completed lines and updates the score" | score / lines / level accumulator | `ScoreKeeper` |
| UC-10.5 "until the board tops out" | terminal board flag | `Board.topOut` |
| UC-10.6 `db_record_game(score_delta, points_delta, won)` | one player's post-game deltas | `ScoreCard` `«value object»` |
| UC-11.1 "split screen: own board / opponent board" | boards keyed by player | `GameSession.boards` |
| UC-11.3 "Server pushes each Player's `STATE`" | the pushed board snapshot | `StateFrame` `«value object»` |
| UC-11.4 "garbage may be applied per rules" | who receives cleared-line garbage | `GarbageRouter` |
| UC-11.5 "winner/loser determined" | per-match outcome for every player | `MatchResult` |
| UC-11 ext 5a "ownership is transferred" | reuses UC-07a | `Room.selectSuccessor()` |
| UC-12.3 "inserted at the bottom of a random other player's board" | target selection across rooms | `GarbageRouter.pickTarget()` |
| UC-12.5 "eliminated as they top out" | who is still playing | `GameSession.alive` |
| UC-12.6 "records the final ranking" | finishing place per participant | `Ranking` |
| UC-13.1 "`MOVE`/`ROTATE`/`DROP`" | one validated player action | `InputCommand` + `InputKind` `«enum»` |
| UC-13.2 "validates against the authoritative board" | brain return code | `BrainResult` `«enum»` |
| UC-14.1 "triggers the equipped ability" | one of four levels a character grants | `Ability` `«value object»` |
| UC-14.3 "reads the `abilities` bitfield" | the catalogue item that grants abilities | `Character` |
| UC-14.4 "enough line-clear charge, deducts the cost" | the charge meter (2 lines = 1 charge) | `ChargeMeter` |
| UC-14.4 "enforces the corresponding effect" | pluggable effect over board primitives | `AbilityEffect` `«interface»` |
| UC-14.5 "Server narrates it to the room" | reuses UC-04/UC-09 narration | `Room.narrate()` |

### Domain diagram

```mermaid
classDiagram
    direction TB

    class Player {
        <<actor>>
        +playerId : PlayerId
        +username : String
        +equippedCharacter : ItemId
    }

    class Room {
        +roomId : String
        +status : GameRoomStatus
        +members() Membership[]
        +selectSuccessor() Membership
        +narrate(text : String)
    }
    class GameSession {
        +roomId : String
        +mode : GameMode
        +tick : long
        +alive : PlayerId[]
        +advance()
        +applyInput(playerId, i : InputCommand) BrainResult
        +eliminate(playerId) int
        +isOver() bool
        +winner() PlayerId
        +snapshot() StateFrame
    }
    class Board {
        +owner : PlayerId
        +cells : Cell[20][10]
        +topOut : bool
        +eliminated : bool
        +move(dir) BrainResult
        +rotate(dir) BrainResult
        +hardDrop() BrainResult
        +clearLines() int
        +injectGarbage(rows)
    }
    class Piece {
        +kind : PieceKind
        +row : int
        +col : int
        +rotation : int
    }
    class InputCommand {
        <<value object>>
        +kind : InputKind
        +arg : String
    }
    class StateFrame {
        <<value object>>
        +roomId : String
        +tick : long
        +boards : BoardView[]
    }
    class ScoreKeeper {
        +score : long
        +lines : int
        +level : int
        +onClear(lines)
        +pointsEarned() long
    }
    class GarbageRouter {
        +pickTarget(from) PlayerId
        +route(from, rows) PlayerId
    }
    class ChargeMeter {
        +charge : int
        +grant(linesCleared)
        +canAfford(cost) bool
        +deduct(cost)
    }
    class Character {
        +characterId : ItemId
        +name : String
        +abilities : bitfield
    }
    class Ability {
        <<value object>>
        +level : int
        +cost : int
        +name : String
    }
    class AbilityEffect {
        <<interface>>
        +apply(caster : Board, target : Board)
    }
    class Ranking {
        +places : Map~PlayerId, int~
        +lastStanding() PlayerId
    }
    class ScoreCard {
        <<value object>>
        +scoreDelta : long
        +pointsDelta : long
        +won : bool
    }
    class MatchResult {
        <<value object>>
        +cards : Map~PlayerId, ScoreCard~
        +ranking : Ranking
    }

    class GameMode {
        <<enum>>
        SINGLE
        DOUBLE
        BATTLE_ROYALE
    }
    class InputKind {
        <<enum>>
        MOVE
        ROTATE
        DROP
    }
    class PieceKind {
        <<enum>>
        I
        O
        T
        S
        Z
        J
        L
    }
    class BrainResult {
        <<enum>>
        BRAIN_OK
        BRAIN_BLOCKED
        BRAIN_LOCKED
        BRAIN_CLEARED
        BRAIN_GAME_OVER
    }
    class AbilityVerdict {
        <<enum>>
        APPLIED
        NOT_OWNED
        INSUFFICIENT_CHARGE
    }

    Room *--> "0..1" GameSession : live match
    GameSession o--> GameMode
    GameSession *--> "1..99" Board : one per participant
    GameSession ..> StateFrame : pushes
    GameSession ..> BrainResult : per input
    GameSession *--> "0..1" Ranking : BR only
    GameSession ..> MatchResult : at game-over
    GameSession o--> GarbageRouter : routes cleared lines

    Board *--> "1" Piece : active
    Board *--> "0..*" Piece : queue + hold
    Piece o--> PieceKind
    Board --> "1" ScoreKeeper : scored by
    Board --> "1" ChargeMeter : charges by line clears
    GarbageRouter ..> Board : injectGarbage

    Player ..> InputCommand : issues
    InputCommand o--> InputKind
    GameSession ..> InputCommand : validates

    Player o--> "1" Character : equipped
    Character ..> "4" Ability : grants
    Ability o--> AbilityEffect : effect
    AbilityEffect ..> Board : mutates caster / target
    ChargeMeter ..> Ability : pays cost
    Ability ..> AbilityVerdict : activation outcome

    MatchResult *--> "1..*" ScoreCard : one per recorded player
    Ranking o--> "1..*" Player : finishing place
```

---

## Sequence Diagrams

### Layering conventions

| Participant | Layer | Process |
|---|---|---|
| `:GameplayUI` | UI | tetrisu |
| `:HtttpClient` | Boundary | tetrisu |
| `:GameController`, `:AbilityController` | Controller | tetrisd |
| `:GameSession`, `:Board`, `:ScoreKeeper`, `:ChargeMeter`, `:GarbageRouter`, `:Room` | Domain | tetrisd (logic via `libtetrisbrain`) |
| `:Db` | Persistence | `libmacminidb` |

### SD UC-10 — Play Single-Player Game

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :GameplayUI
    participant C as :HtttpClient
    participant GC as :GameController
    participant GS as :GameSession
    participant B as :Board
    participant SK as :ScoreKeeper
    participant DB as :Db

    P->>UI: selectSinglePlayer()
    activate UI
    UI->>C: startSingle(playerId)
    activate C
    C->>GC: START /room/<id>  Mode: SINGLE
    activate GC
    GC->>GS: new GameSession(roomId, SINGLE)
    activate GS
    GS->>B: new Board(queue, hold)
    activate B
    B-->>GS: board
    deactivate B
    GS-->>GC: session(initialState)
    GC-->>C: 200 OK + STATE {tick 0}
    C-->>UI: render(boardView, queue, hold, portrait, score)

    loop until topOut  (steps 2–5)
        GS->>B: gravityTick()
        activate B
        B-->>GS: BRAIN_OK | BRAIN_LOCKED | BRAIN_GAME_OVER
        deactivate B

        note over UI,B: «include» UC-13 Control Falling Piece for each input

        opt lines completed  (step 4)
            GS->>B: clearLines()
            activate B
            B-->>GS: n (0..4)
            deactivate B
            GS->>SK: onClear(n)
            activate SK
            SK->>SK: score += score_on_clear(n, level)
            SK-->>GS: score, level
            deactivate SK
        end

        GC->>C: STATE /room/<id>  {board, score, tick}
        C-->>UI: render(boardView)
    end

    alt board tops out  (step 5 → 6)
        GS-->>GC: BRAIN_GAME_OVER
        GC->>SK: pointsEarned()
        activate SK
        SK-->>GC: pointsDelta
        deactivate SK
        GC->>DB: db_record_game(playerId, scoreDelta, pointsDelta, won = true)
        activate DB
        DB-->>GC: DB_OK
        deactivate DB
        note right of GC: the only persisted call of UC-10 —<br/>leaderboard_score, wallet_points, games_played
        GC-->>C: 200 OK {finalScore}
        C-->>UI: showFinalScore(scoreCard)
        UI-->>P: show(final score + Home)
    else Player quits mid-game  (ext 5a)
        P->>UI: pressQuit()
        UI->>C: leaveGame(playerId)
        C->>GC: LEAVE /room/<id>
        GC->>GS: abort()
        note right of GC: db_record_game NOT called —<br/>an abandoned game is not scored
        GC-->>C: 200 OK
        UI-->>P: show(Home)
    end

    deactivate GS
    deactivate GC
    deactivate C
    deactivate UI

    opt server disconnect  (E1)
        C-->>UI: connectionLost()
        UI-->>P: show("game ended — server restarting")
    end
```

### SD UC-11 — Play Double (2-Player) Game

```mermaid
sequenceDiagram
    actor P1 as Player A
    actor P2 as Player B
    participant UI as :GameplayUI[A]
    participant C as :HtttpClient
    participant GC as :GameController
    participant GS as :GameSession
    participant BA as :Board[A]
    participant BB as :Board[B]
    participant GR as :GarbageRouter
    participant DB as :Db

    note over P1,GC: triggered by UC-08 Start Game completing for a Double room

    P1->>UI: startDoublePlayerGame()
    activate UI
    UI->>C: startDouble(playerId, roomId)
    activate C
    C->>GC: START /room/<id>  Mode: DOUBLE
    activate GC
    GC->>GS: new GameSession(roomId, DOUBLE, members[A,B])
    activate GS
    GS->>BA: new Board(queue, hold)
    activate BA
    BA-->>GS: board A
    deactivate BA
    GS->>BB: new Board(queue, hold)
    activate BB
    BB-->>GS: board B
    deactivate BB
    GS-->>GC: session(status = IN_GAME)

    GC->>GS: snapshot()
    GS-->>GC: StateFrame{boards[A,B], tick 0}
    GC->>C: STATE /room/<id>
    C-->>UI: renderSplit(ownBoard, opponentBoard)
    UI-->>P1: show(split screen + both usernames and scores)

    loop until one board tops out  (steps 2–4)
        par Player A input
            P1->>UI: MOVE / ROTATE / DROP
            note over UI,BA: «include» UC-13
            UI->>C: sendInput(cmd)
            C->>GC: MOVE /room/<id>/player/<A>
            GC->>GS: applyInput(A, cmd)
            GS->>BA: move / rotate / drop
            activate BA
            BA-->>GS: BRAIN_OK | BRAIN_CLEARED
            deactivate BA
        and Player B input
            P2->>GC: MOVE /room/<id>/player/<B>
            GC->>GS: applyInput(B, cmd)
            GS->>BB: move / rotate / drop
            activate BB
            BB-->>GS: BRAIN_OK | BRAIN_CLEARED
            deactivate BB
        end

        opt lines cleared  (step 4)
            GS->>GR: route(from = A, rows = n - 1)
            activate GR
            GR-->>GS: target = B
            deactivate GR
            GS->>BB: injectGarbage(n - 1)
            activate BB
            BB-->>GS: applied
            deactivate BB
        end

        opt ability pressed  (ext 2a)
            note over GS: «extend» UC-14 Activate Gaiden Ability
        end

        GC->>C: STATE /room/<id>  {both boards, scores, tick}
        C-->>UI: renderSplit(...)
    end

    alt one Player tops out  (step 5)
        GS->>GS: winner() = the surviving player
        GS-->>GC: MatchResult{cards}
        loop for each player in the match
            GC->>DB: db_record_game(id, scoreDelta, pointsDelta, won)
            activate DB
            DB-->>GC: DB_OK
            deactivate DB
        end
        note right of GC: winner won = true, loser won = false
        GC->>C: STATE /room/<id>  {gameOver, result}
        C-->>UI: showResult(win | lose)
        UI-->>P1: show(result + points credited)
    else a Player quits mid-game  (ext 5a)
        P2->>GC: LEAVE /room/<id>
        GC->>GS: dropPlayer(B)
        note right of GS: quitter NOT recorded

        opt quitter was the Owner
            rect rgb(240, 240, 240)
                note over GS: «include» UC-07a Transfer Room Ownership
                GS->>GS: room.selectSuccessor() → setRole(OWNER)
                GS->>GS: narrate("PLAYER <name> set as the owner")
            end
        end

        GC->>DB: db_record_game(A, scoreDelta, pointsDelta, won = true)
        activate DB
        DB-->>GC: DB_OK
        deactivate DB
        note right of GC: remaining player wins by default
        C-->>UI: showResult(win by default)
    end

    deactivate UI
    deactivate C
    deactivate GS

    opt server disconnect  (E1)
        note over GC,DB: no game-over reached → db_record_game called for no one
    end
```

### SD UC-12 — Play Battle Royale Game

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :GameplayUI
    participant C as :HtttpClient
    participant GC as :GameController
    participant GS as :GameSession
    participant B as :Board[self]
    participant GR as :GarbageRouter
    participant BT as :Board[target]
    participant RK as :Ranking
    participant DB as :Db

    note over P,GC: triggered by UC-08 Start Game completing for a Battle Royale room

    P->>UI: startBattleRoyaleGame()
    activate UI
    UI->>C: startBattleRoyale(playerId, roomId)
    activate C
    C->>GC: START /room/<id>  Mode: BATTLE_ROYALE
    activate GC
    GC->>GS: new GameSession(roomId, BATTLE_ROYALE, members[4..99])
    activate GS
    loop for each participant
        GS->>B: new Board(queue, hold)
        activate B
        B-->>GS: board
        deactivate B
    end
    GS->>GS: alive = every participant
    GS-->>GC: session(status = IN_GAME)

    GC->>GS: snapshot()
    GS-->>GC: StateFrame{visible boards, tick 0}
    GC->>C: STATE /room/<id>  {own board + visible boards, tick 0}
    C-->>UI: renderArena(ownBoard, otherBoards)
    UI-->>P: show(center board + surrounding grids + scores)

    loop until one survivor remains  (steps 2–5)
        P->>UI: MOVE / ROTATE / DROP
        note over UI,B: «include» UC-13
        UI->>C: sendInput(cmd)
        C->>GC: MOVE /room/<id>/player/<pid>
        activate GC
        GC->>GS: applyInput(playerId, cmd)
        activate GS
        GS->>B: apply move
        activate B
        B-->>GS: BRAIN_OK | BRAIN_CLEARED(n)
        deactivate B

        opt n >= 2 lines cleared  (step 3)
            GS->>GR: pickTarget(from = playerId)
            activate GR
            GR->>GR: random alive player in a different room
            GR-->>GS: targetId
            deactivate GR
            GS->>BT: injectGarbage(n - 1)
            activate BT
            note right of BT: cross-room garbage via libcoreipc
            BT-->>GS: applied
            deactivate BT
        end

        opt ability pressed  (ext 2a)
            note over GS: «extend» UC-14 Activate Gaiden Ability
        end

        GC->>C: STATE /room/<id>  {visible boards, tick}
        C-->>UI: renderArena(...)

        opt a Player tops out  (ext 5a)
            GS->>GS: board.eliminated = true
            GS->>RK: record(playerId, rank = survivorCount() + 1)
            activate RK
            RK-->>GS: recorded
            deactivate RK
            C-->>UI: showElimination(rank)
            note right of GS: eliminated players wait out the match,<br/>recorded at game-over with won = false
        end

        opt a Player quits  (ext 5b)
            GS->>GS: dropPlayer(playerId) — NOT recorded
            opt quitter was the Owner
                note over GS: «include» UC-07a Transfer Room Ownership
            end
            opt only one player remains  (5b-i)
                note right of GS: wins by default → jump to game-over
            end
        end

        deactivate GS
        deactivate GC
    end

    GS->>RK: lastStanding()
    activate RK
    RK-->>GS: winnerId
    deactivate RK
    GS-->>GC: MatchResult{ranking, cards}

    loop for each participant still in the game at game-over
        GC->>DB: db_record_game(id, scoreDelta, pointsDelta, won)
        activate DB
        DB-->>GC: DB_OK
        deactivate DB
    end
    note right of GC: last-standing won = true, others won = false<br/>points from line clears / KOs / win

    GC->>C: STATE /room/<id>  {gameOver, ranking}
    C-->>UI: showResult(rank, pointsCredited)
    UI-->>P: show(final ranking + leaderboard updated)

    deactivate UI
    deactivate C

    opt server disconnect  (E1)
        note over GC,DB: match aborted → db_record_game called for no one
    end
```

### SD UC-13 — Control Falling Piece

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :GameplayUI
    participant C as :HtttpClient
    participant GC as :GameController
    participant GS as :GameSession
    participant B as :Board
    participant SK as :ScoreKeeper

    P->>UI: pressKey(MOVE LEFT | ROTATE CW | DROP HARD)
    activate UI
    UI->>UI: cmd = InputCommand(kind, arg)
    UI->>C: sendInput(cmd)
    activate C
    C->>GC: MOVE|ROTATE|DROP /room/<id>/player/<pid>  body: LEFT|CW|HARD
    activate GC

    alt malformed body
        GC-->>C: 400 Bad Request
        C-->>UI: error(400)
    else well-formed
        GC->>GS: applyInput(playerId, cmd)
        activate GS
        GS->>B: move(dir) | rotate(dir) | softDrop() | hardDrop()
        activate B
        B->>B: piece_is_valid(candidate)

        alt collision  (ext 2a)
            B-->>GS: BRAIN_BLOCKED
            GS-->>GC: BLOCKED + authoritative position
            GC-->>C: 409 INVALID_MOVE {row, col, rotation}
            C-->>UI: correctTo(authoritativePosition)
            UI-->>P: render(unchanged board)
        else move applied
            B-->>GS: BRAIN_OK
            GS-->>GC: accepted
            GC-->>C: 200 OK
        else piece locks
            B-->>GS: BRAIN_LOCKED
            GS->>B: clearLines()
            B-->>GS: n (0..4)
            opt n > 0
                GS->>SK: onClear(n)
                activate SK
                SK-->>GS: score, level
                deactivate SK
                note right of SK: also grants ability charge (UC-14)
            end
            GS->>B: piece_spawn(next)
            B-->>GS: BRAIN_OK | BRAIN_GAME_OVER
            GS-->>GC: locked(n cleared)
            GC-->>C: 200 OK
        end
        deactivate B

        GC->>C: STATE /room/<id>  {board, tick}
        C-->>UI: render(boardView)
        UI-->>P: show(updated board)
        deactivate GS
    end

    deactivate GC
    deactivate C
    deactivate UI
```

### SD UC-14 — Activate Gaiden Ability

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :GameplayUI
    participant C as :HtttpClient
    participant AC as :AbilityController
    participant DB as :Db
    participant CM as :ChargeMeter
    participant EF as :AbilityEffect
    participant GS as :GameSession
    participant R as :Room

    note over UI: charge meter filled by line clears — 2 lines = 1 charge

    P->>UI: pressAbilityKey(level)
    activate UI
    UI->>C: useAbility(playerId, roomId, level)
    activate C
    C->>AC: ABILITY /room/<id>/player/<pid>  body {ability: level}
    activate AC

    AC->>DB: db_player_owns_character(playerId, characterId)
    activate DB
    DB-->>AC: DB_TRUE | DB_FALSE
    deactivate DB

    alt character not owned  (ext 3a)
        AC-->>C: 403 Forbidden
        C-->>UI: error(403)
        UI-->>P: show("character not owned")
    else owned
        AC->>DB: db_get_character(characterId)
        activate DB
        DB-->>AC: character{abilities bitfield}
        deactivate DB
        AC->>AC: ability = character.grants(level)
        note right of AC: cost = 2 / 4 / 6 / 8 by level

        AC->>CM: canAfford(ability.cost)
        activate CM

        alt insufficient charge  (ext 4a)
            CM-->>AC: false
            AC-->>C: 409 Conflict
            C-->>UI: error(409)
            UI-->>P: show("not enough charge")
            note right of CM: no charge consumed, no effect applied
        else affordable
            CM-->>AC: true
            AC->>CM: deduct(ability.cost)
            CM-->>AC: charge updated
            deactivate CM

            AC->>GS: boardOf(playerId), opponentOf(playerId)
            activate GS
            GS-->>AC: casterBoard, targetBoard

            AC->>EF: apply(casterBoard, targetBoard)
            activate EF
            EF->>EF: libtetrisbrain primitive<br/>(cutTop / cutBottom / invert / fillRows / clearCells / deleteColumns)
            EF-->>AC: effect applied to room state
            deactivate EF
            note right of EF: runtime only — the effect is never persisted

            AC->>R: narrate("PLAYER <name> used <ability>")
            activate R
            R-->>AC: broadcast ok
            deactivate R

            AC-->>C: 200 OK
            GS->>C: STATE /room/<id>  {affected boards}
            deactivate GS
            C-->>UI: render(updated boards)
            UI-->>P: show(ABILITY_USED + effect)
        end
    end

    deactivate AC
    deactivate C
    deactivate UI
```

---

## Solution Class Diagram

### Method inventory by use case

| Behaviour | Owner | Signature | From |
|---|---|---|---|
| advance the gravity timer | `GameSession` | `advance()` | UC-10.2 |
| one gravity step | `Board` | `gravityTick() : BrainResult` | UC-10.2 |
| clear completed lines | `Board` | `clearLines() : int` | UC-10.4 |
| accumulate score | `ScoreKeeper` | `onClear(int)`, `pointsEarned() : long` | UC-10.4 |
| detect game over | `GameSession` | `isOver() : bool` | UC-10.5 |
| persist the result | `Db` | `db_record_game(id, scoreDelta, pointsDelta, won)` | UC-10.6 |
| build post-game deltas | `GameController` | `finish(String) : MatchResult` | UC-10.6 / UC-11.5 |
| push a board snapshot | `GameSession` | `snapshot() : StateFrame` | UC-11.3 |
| decide the garbage target | `GarbageRouter` | `pickTarget(PlayerId) : PlayerId` | UC-12.3 |
| deliver garbage | `Board` | `injectGarbage(int)` | UC-11.4 / UC-12.3 |
| name the winner | `GameSession` | `winner() : PlayerId` | UC-11.5 |
| remove a quitter | `GameSession` | `dropPlayer(PlayerId)` | UC-11 ext 5a / UC-12 ext 5b |
| eliminate a topped-out board | `GameSession` | `eliminate(PlayerId) : int` | UC-12 ext 5a |
| record finishing places | `Ranking` | `record(PlayerId, int)`, `lastStanding() : PlayerId` | UC-12.6 |
| translate a keypress | `GameplayUI` | `onKey(Key) : InputCommand` | UC-13.1 |
| validate + apply an input | `GameController` | `applyInput(PlayerId, InputCommand) : HtttpStatus` | UC-13.2 |
| move / rotate / drop | `Board` | `move(String)`, `rotate(String)`, `softDrop()`, `hardDrop()` | UC-13.1–3 |
| collision check | `Piece` | `isValid(Board) : bool` | UC-13.2 |
| activate an ability | `AbilityController` | `activate(PlayerId, String, int) : HtttpStatus` | UC-14.2 |
| ownership + catalogue read | `Db` | `db_player_owns_character(id, cid)`, `db_get_character(cid, out)` | UC-14.3 |
| resolve level → ability | `Character` | `grants(int) : Ability` | UC-14.3 |
| charge accounting | `ChargeMeter` | `grant(int)`, `canAfford(int) : bool`, `deduct(int)` | UC-14.4 |
| enforce the effect | `AbilityEffect` | `apply(Board, Board)` | UC-14.4 |
| board primitives for effects | `Board` | `cutTop`, `cutBottom`, `invert`, `fillRows`, `clearCells`, `deleteColumns` | UC-14.4 |
| narrate the event | `Room` | `narrate(String)` | UC-14.5 |

### Solution diagram

```mermaid
classDiagram
    direction TB

    %% ---------------- UI layer (tetrisu) ----------------
    class GameplayUI {
        <<boundary>>
        -playerId : PlayerId
        -charge : int
        +render(view : BoardView) void
        +renderSplit(own : BoardView, opponent : BoardView) void
        +renderArena(own : BoardView, others : BoardView[]) void
        +onKey(k : Key) InputCommand
        +onAbilityKey(level : int) void
        +showElimination(rank : int) void
        +showFinalScore(s : ScoreCard) void
        +show(msg : String) void
    }

    %% ---------------- Boundary ----------------
    class HtttpClient {
        <<boundary>>
        +sendInput(i : InputCommand) HtttpResponse
        +useAbility(playerId : PlayerId, roomId : String, level : int) HtttpResponse
        +leaveGame(playerId : PlayerId) HtttpResponse
        +onState(frame : StateFrame) void
    }

    %% ---------------- Controllers (tetrisd, stateless) ----------------
    class GameController {
        <<control>>
        +applyInput(playerId : PlayerId, i : InputCommand) HtttpStatus
        +tick(roomId : String) void
        +finish(roomId : String) MatchResult
    }
    class AbilityController {
        <<control>>
        +activate(playerId : PlayerId, roomId : String, level : int) HtttpStatus
    }

    %% ---------------- Domain (tetrisd + libtetrisbrain) ----------------
    class GameSession {
        -roomId : String
        -mode : GameMode
        -tick : long
        -boards : Map~PlayerId, Board~
        -alive : PlayerId[]
        +advance() void
        +applyInput(playerId : PlayerId, i : InputCommand) BrainResult
        +boardOf(playerId : PlayerId) Board
        +opponentOf(playerId : PlayerId) Board
        +eliminate(playerId : PlayerId) int
        +dropPlayer(playerId : PlayerId) void
        +survivorCount() int
        +isOver() bool
        +winner() PlayerId
        +snapshot() StateFrame
        +abort() void
    }
    class Board {
        -owner : PlayerId
        -cells : Cell[20][10]
        -active : Piece
        -queue : Piece[]
        -hold : Piece
        -topOut : bool
        -eliminated : bool
        +gravityTick() BrainResult
        +move(dir : String) BrainResult
        +rotate(dir : String) BrainResult
        +softDrop() BrainResult
        +hardDrop() BrainResult
        +clearLines() int
        +injectGarbage(rows : int) void
        +cutTop(rows : int) void
        +cutBottom(rows : int) void
        +invert() void
        +fillRows(rows : int) void
        +clearCells(n : int) void
        +deleteColumns(cols : int) void
    }
    class Piece {
        -kind : PieceKind
        -row : int
        -col : int
        -rotation : int
        +isValid(b : Board) bool
    }
    class ScoreKeeper {
        -score : long
        -lines : int
        -level : int
        +onClear(lines : int) void
        +pointsEarned() long
        +level() int
    }
    class ChargeMeter {
        -charge : int
        +grant(linesCleared : int) void
        +canAfford(cost : int) bool
        +deduct(cost : int) void
    }
    class GarbageRouter {
        +pickTarget(from : PlayerId) PlayerId
        +route(from : PlayerId, rows : int) PlayerId
    }
    class Character {
        -characterId : ItemId
        -name : String
        -abilities : bitfield
        +grants(level : int) Ability
    }
    class Ability {
        <<value object>>
        -level : int
        -cost : int
        -name : String
        +costFor(level : int)$ int
    }
    class AbilityEffect {
        <<interface>>
        +apply(caster : Board, target : Board) void
    }
    class Ranking {
        -places : Map~PlayerId, int~
        +record(playerId : PlayerId, rank : int) void
        +lastStanding() PlayerId
    }
    class Room {
        -roomId : String
        -status : GameRoomStatus
        +members() Membership[]
        +selectSuccessor() Membership
        +narrate(text : String) void
    }

    %% ---------------- Value objects ----------------
    class InputCommand {
        <<value object>>
        -kind : InputKind
        -arg : String
        +isDrop() bool
    }
    class StateFrame {
        <<value object>>
        -roomId : String
        -tick : long
        -boards : BoardView[]
    }
    class ScoreCard {
        <<value object>>
        -scoreDelta : long
        -pointsDelta : long
        -won : bool
    }
    class MatchResult {
        <<value object>>
        -cards : Map~PlayerId, ScoreCard~
        -ranking : Ranking
        +winnerId() PlayerId
    }

    %% ---------------- Persistence ----------------
    class Db {
        <<interface>>
        libmacminidb
        +db_record_game(id, scoreDelta, pointsDelta, won) DbResult
        +db_player_owns_character(id, cid) DbBool
        +db_get_character(cid, out) DbResult
    }

    %% ---------------- Enums ----------------
    class GameMode {
        <<enum>>
        SINGLE
        DOUBLE
        BATTLE_ROYALE
    }
    class InputKind {
        <<enum>>
        MOVE
        ROTATE
        DROP
    }
    class PieceKind {
        <<enum>>
        I
        O
        T
        S
        Z
        J
        L
    }
    class BrainResult {
        <<enum>>
        BRAIN_OK
        BRAIN_BLOCKED
        BRAIN_LOCKED
        BRAIN_CLEARED
        BRAIN_GAME_OVER
    }
    class AbilityVerdict {
        <<enum>>
        APPLIED
        NOT_OWNED
        INSUFFICIENT_CHARGE
    }
    class DbResult {
        <<enum>>
        DB_OK
        DB_NOT_OWNED
        DB_NOT_FOUND
        DB_IO_ERROR
    }

    %% ---------------- Relationships ----------------
    GameplayUI ..> HtttpClient : uses
    GameplayUI ..> InputCommand : onKey builds
    HtttpClient ..> GameController : HTTTP MOVE / ROTATE / DROP
    HtttpClient ..> AbilityController : HTTTP ABILITY
    HtttpClient ..> StateFrame : receives push

    GameController ..> GameSession : applyInput / tick
    GameController ..> MatchResult : finish
    GameController ..> Db : db_record_game on game-over
    AbilityController ..> Db : owns_character / get_character
    AbilityController ..> ChargeMeter : canAfford / deduct
    AbilityController ..> AbilityEffect : apply
    AbilityController ..> GameSession : caster + target boards
    AbilityController ..> Room : narrate ABILITY_USED
    AbilityController ..> AbilityVerdict : outcome

    Room *--> "0..1" GameSession : live match
    GameSession *--> "1..99" Board
    GameSession *--> "0..1" Ranking : BR only
    GameSession o--> GameMode
    GameSession o--> GarbageRouter
    GameSession ..> StateFrame : snapshot
    GameSession ..> BrainResult
    GameSession ..> InputCommand : validates

    Board *--> "1" Piece : active
    Board *--> "0..*" Piece : queue + hold
    Board --> "1" ScoreKeeper
    Board --> "1" ChargeMeter
    Board ..> BrainResult
    Piece o--> PieceKind
    InputCommand o--> InputKind
    GarbageRouter ..> Board : injectGarbage
    AbilityEffect ..> Board : brain primitives

    Character ..> "4" Ability : grants
    Ability o--> AbilityEffect
    ChargeMeter ..> Ability : pays cost

    MatchResult *--> "1..*" ScoreCard
    MatchResult o--> "0..1" Ranking
    Db ..> DbResult
```

---

## Traceability Matrix

| UC | Sequence diagram | Domain classes exercised | New solution methods |
|---|---|---|---|
| UC-10 | [SD UC-10](#sd-uc-10--play-single-player-game) | `GameSession`, `Board`, `Piece`, `ScoreKeeper`, `ScoreCard` | `GameSession.advance/isOver`, `Board.gravityTick/clearLines`, `ScoreKeeper.onClear/pointsEarned`, `GameController.finish` |
| UC-11 | [SD UC-11](#sd-uc-11--play-double-2-player-game) | `GameSession`, `Board`, `GarbageRouter`, `MatchResult`, `Room` | `GameSession.winner/dropPlayer/snapshot`, `GarbageRouter.route`, `Board.injectGarbage` |
| UC-12 | [SD UC-12](#sd-uc-12--play-battle-royale-game) | `GameSession`, `Board`, `GarbageRouter`, `Ranking`, `MatchResult` | `GameSession.eliminate/survivorCount`, `GarbageRouter.pickTarget`, `Ranking.record/lastStanding` |
| UC-13 | [SD UC-13](#sd-uc-13--control-falling-piece) | `Board`, `Piece`, `InputCommand`, `ScoreKeeper` | `GameplayUI.onKey`, `GameController.applyInput`, `Board.move/rotate/softDrop/hardDrop`, `Piece.isValid` |
| UC-14 | [SD UC-14](#sd-uc-14--activate-gaiden-ability) | `Character`, `Ability`, `AbilityEffect`, `ChargeMeter`, `Board`, `Room` | `AbilityController.activate`, `Character.grants`, `ChargeMeter.grant/canAfford/deduct`, `AbilityEffect.apply`, `Board.cutTop/invert/fillRows/…` |
| UC-07a | folded into SD UC-11 / SD UC-12 | `Room`, `Membership` | *(none — reuses `Room.selectSuccessor`)* |
