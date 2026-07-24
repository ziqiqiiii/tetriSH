# tetriSH

## Table of Contents

- [Per-Use-Case Class Diagrams](#per-use-case-class-diagrams)
  - [CD UC-01 — Register Account](#cd-uc-01--register-account)
  - [CD UC-02 — Log In](#cd-uc-02--log-in)
  - [CD UC-03 — Browse Open Rooms](#cd-uc-03--browse-open-rooms)
  - [CD UC-04 — Create Room](#cd-uc-04--create-room)
  - [CD UC-05 — Join Room from List](#cd-uc-05--join-room-from-list)
  - [CD UC-06 — Join Room by Room ID](#cd-uc-06--join-room-by-room-id)
  - [CD UC-07 — Leave Room (with UC-07a)](#cd-uc-07--leave-room-with-uc-07a)
- [Combined Domain Class Diagram](#combined-domain-class-diagram)
  - [Concept extraction](#concept-extraction)
  - [Domain diagram](#domain-diagram)
- [Sequence Diagrams](#sequence-diagrams)
  - [Layering conventions](#layering-conventions)
  - [SD UC-01 — Register Account](#sd-uc-01--register-account)
  - [SD UC-02 — Log In](#sd-uc-02--log-in)
  - [SD UC-03 — Browse Open Rooms](#sd-uc-03--browse-open-rooms)
  - [SD UC-04 — Create Room](#sd-uc-04--create-room)
  - [SD UC-05 — Join Room from List](#sd-uc-05--join-room-from-list)
  - [SD UC-06 — Join Room by Room ID](#sd-uc-06--join-room-by-room-id)
  - [SD UC-07 — Leave Room (includes UC-07a)](#sd-uc-07--leave-room-includes-uc-07a)
- [Solution Class Diagram](#solution-class-diagram)
  - [Method inventory by use case](#method-inventory-by-use-case)
  - [Solution diagram](#solution-diagram)
- [Traceability Matrix](#traceability-matrix)

---

## Per-Use-Case Class Diagrams

### CD UC-01 — Register Account

```mermaid
classDiagram
    direction LR

    class SignUpUI {
        <<boundary>>
        -username : String
        -password : String
        -confirm : String
        +validateMatch(String, String) bool
        +show(msg : String) void
    }
    class HtttpClient {
        <<boundary>>
        +signup(username : String, password : String) HtttpResponse
    }
    class AuthController {
        <<control>>
        +signup(username : String, password : String) HtttpStatus
    }
    class Credential {
        <<value object>>
        -passwordHashed : String
        -salt : String
        +create(password : String)$ Credential
        +getSalt() String
    }
    class Player {
        -playerId : PlayerId
        -username : String
        -credential : Credential
        -walletPoints : long
        -equippedCharacter : ItemId
        -equippedTheme : ItemId
    }
    class Db {
        <<interface>>
        +db_signup(username, hash, salt, out) DbResult
    }
    class DbResult {
        <<enum>>
        DB_OK
        DB_EXISTS
        DB_IO_ERROR
    }

    SignUpUI ..> HtttpClient : 5. submit after validateMatch
    HtttpClient ..> AuthController : SIGNUP /account
    AuthController ..> Credential : creates (salt + hash)
    AuthController ..> Db : 6. db_signup
    Db ..> DbResult : returns
    Db ..> Player : 7. materialises (wallet 0, item 1)
    Player *--> Credential : stores hash + salt
```

### CD UC-02 — Log In

```mermaid
classDiagram
    direction LR

    class LoginUI {
        <<boundary>>
        -username : String
        -password : String
        -serverId : String
        +show(msg : String) void
    }
    class HtttpClient {
        <<boundary>>
        -session : Session
        +login(username, password, serverId) HtttpResponse
    }
    class Session {
        -sessionId : String
        -playerId : PlayerId
        -endpoint : ServerEndpoint
        +connect(endpoint : ServerEndpoint) bool
        +bind(playerId : PlayerId) void
        +isAuthenticated() bool
    }
    class ServerEndpoint {
        <<value object>>
        -serverId : String
        -host : String
        -port : int
        +resolve(serverId : String)$ ServerEndpoint
    }
    class AuthController {
        <<control>>
        +login(username : String, password : String) LoginResult
    }
    class Credential {
        <<value object>>
        -salt : String
        +getSalt() String
    }
    class Player {
        -playerId : PlayerId
        -username : String
        -credential : Credential
        -leaderboardScore : long
        +getUsername() String
    }
    class Db {
        <<interface>>
        +db_get_player(username, out) DbResult
        +db_login(username, hash, out) DbResult
    }
    class DbResult {
        <<enum>>
        DB_OK
        DB_BAD_CREDS
        DB_NOT_FOUND
    }

    LoginUI ..> HtttpClient : 4. login(u, p, serverId)
    HtttpClient *--> Session : owns
    Session o--> ServerEndpoint : 5. «include» UC-02a connect
    HtttpClient ..> AuthController : LOGIN /session
    AuthController ..> Db : 6. get salt, then db_login
    AuthController ..> Session : 7. bind(playerId)
    Db ..> DbResult : returns
    Db ..> Player : materialises
    Player *--> Credential : salt for re-hash
```

### CD UC-03 — Browse Open Rooms

```mermaid
classDiagram
    direction LR

    class LobbyUI {
        <<boundary>>
        -playerId : PlayerId
        -highlightedRoomId : String
        +show(msg : String) void
    }
    class HtttpClient {
        <<boundary>>
        +browseRooms(playerId : PlayerId) HtttpResponse
    }
    class LobbyController {
        <<control>>
        +listRooms(playerId : PlayerId) LobbyView
        +header(playerId : PlayerId) HeaderDto
    }
    class Lobby {
        -rooms : Map~String, Room~
        +listOpenRooms() RoomSummary[]
    }
    class Room {
        -roomId : String
        -mode : GameMode
        -status : GameRoomStatus
        -numberOfPlayers : int
        -slotCount : int
        +toSummary() RoomSummary
        +stateMessage() String
    }
    class RoomSummary {
        <<value object>>
        -roomId : String
        -mode : GameMode
        -occupancy : String
        -status : GameRoomStatus
        -ownerName : String
    }
    class LeaderboardEntry {
        <<value object>>
        -username : String
        -score : long
        -rank : int
    }
    class Player {
        -playerId : PlayerId
        -username : String
        -leaderboardScore : long
        +getLeaderboardScore() long
    }
    class Db {
        <<interface>>
        +db_get_player(playerId, out) DbResult
        +db_rank(playerId) int
    }
    class GameMode {
        <<enum>>
        SINGLE
        DOUBLE
        BATTLE_ROYALE
    }
    class GameRoomStatus {
        <<enum>>
        WAITING
        READY
        IN_GAME
        FINISHED
    }

    LobbyUI ..> HtttpClient : 1. browseRooms
    HtttpClient ..> LobbyController : LIST /rooms [Player-Id]
    LobbyController ..> Lobby : 2. listOpenRooms
    LobbyController ..> Db : 4. header (player + rank)
    Lobby *--> "0..*" Room : owns lifetime
    Room ..> RoomSummary : 3. toSummary per row
    Room o--> GameMode
    Room o--> GameRoomStatus
    Db ..> Player : materialises
    Player ..> LeaderboardEntry : ranked as
```

### CD UC-04 — Create Room

```mermaid
classDiagram
    direction TB

    class LobbyUI {
        <<boundary>>
        -playerId : PlayerId
        +show(msg : String) void
    }
    class HtttpClient {
        <<boundary>>
        +createRoom(playerId : PlayerId, mode : GameMode) HtttpResponse
    }
    class RoomController {
        <<control>>
        +create(playerId : PlayerId, mode : GameMode) RoomView
    }
    class Lobby {
        -rooms : Map~String, Room~
        +createRoom(owner : PlayerId, mode : GameMode) Room
        +destroyRoom(roomId : String) void
    }
    class Room {
        -roomId : String
        -mode : GameMode
        -status : GameRoomStatus
        -slotCount : int
        -minToStart : int
        -numberOfPlayers : int
        +seat(playerId : PlayerId) Slot
        +recomputeStatus() void
        +stateMessage() String
        +narrate(text : String) void
    }
    class Slot {
        -index : int
        -status : SlotStatus
        -occupant : Membership
        +setStatus(s : SlotStatus) void
        +occupy(m : Membership) void
    }
    class Membership {
        -role : PlayerStatus
        -playerId : PlayerId
        -roomId : String
        +isOwner() bool
    }
    class Narration {
        -text : String
        -at : timestamp
    }
    class GameMode {
        <<enum>>
        SINGLE
        DOUBLE
        BATTLE_ROYALE
        +slotCount() int
        +minToStart() int
    }
    class GameRoomStatus {
        <<enum>>
        WAITING
        READY
        IN_GAME
        FINISHED
    }
    class SlotStatus {
        <<enum>>
        WAITING
        JOINING
        LEAVING
        READY
    }
    class PlayerStatus {
        <<enum>>
        OWNER
        PLAYER
    }

    LobbyUI ..> HtttpClient : 4. [ENTER] create
    HtttpClient ..> RoomController : JOIN /room/newId + Mode
    RoomController ..> Lobby : 5. createRoom
    RoomController ..> Room : 5. seat(owner) / 7. narrate
    Lobby *--> "0..*" Room : owns lifetime
    Room *--> "1..99" Slot : slot_count from mode
    Room *--> "0..*" Narration : 7. broadcasts
    Room o--> GameMode : slotCount / minToStart
    Room o--> GameRoomStatus : stays WAITING at 1 player
    Slot o--> SlotStatus : WAITING → JOINING → READY
    Slot o--> "0..1" Membership : slot 1 occupied
    Membership o--> PlayerStatus : created as OWNER
```

### CD UC-05 — Join Room from List

```mermaid
classDiagram
    direction TB

    class LobbyUI {
        <<boundary>>
        -playerId : PlayerId
        -highlightedRoomId : String
        +show(msg : String) void
    }
    class HtttpClient {
        <<boundary>>
        +joinRoom(playerId : PlayerId, roomId : String) HtttpResponse
    }
    class RoomController {
        <<control>>
        +join(playerId : PlayerId, roomId : String) RoomView
    }
    class Lobby {
        -rooms : Map~String, Room~
        +findRoom(roomId : String) Room
    }
    class Room {
        -roomId : String
        -status : GameRoomStatus
        -slotCount : int
        -minToStart : int
        -numberOfPlayers : int
        +canAccept() JoinVerdict
        +seat(playerId : PlayerId) Slot
        +recomputeStatus() void
        +stateMessage() String
        +narrate(text : String) void
    }
    class Slot {
        -index : int
        -status : SlotStatus
        -occupant : Membership
        +setStatus(s : SlotStatus) void
        +occupy(m : Membership) void
        +isFree() bool
    }
    class Membership {
        -role : PlayerStatus
        -playerId : PlayerId
        -roomId : String
    }
    class Narration {
        -text : String
        -at : timestamp
    }
    class JoinVerdict {
        <<enum>>
        ACCEPTED
        FULL
        IN_GAME
        NOT_FOUND
    }
    class GameRoomStatus {
        <<enum>>
        WAITING
        READY
        IN_GAME
        FINISHED
    }
    class SlotStatus {
        <<enum>>
        WAITING
        JOINING
        LEAVING
        READY
    }
    class PlayerStatus {
        <<enum>>
        OWNER
        PLAYER
    }

    LobbyUI ..> HtttpClient : 2. [ENTER] on highlighted row
    HtttpClient ..> RoomController : JOIN /room/id [Player-Id]
    RoomController ..> Lobby : 3. findRoom (404 if null)
    RoomController ..> Room : 4. canAccept then seat
    Lobby *--> "0..*" Room
    Room ..> JoinVerdict : ext 4a FULL / 4b IN_GAME
    Room *--> "1..99" Slot
    Room *--> "0..*" Narration : 5. joined the room
    Room o--> GameRoomStatus : WAITING → READY at min_to_start
    Slot o--> SlotStatus
    Slot o--> "0..1" Membership
    Membership o--> PlayerStatus : created as PLAYER
```

### CD UC-06 — Join Room by Room ID

```mermaid
classDiagram
    direction TB

    class LobbyUI {
        <<boundary>>
        -playerId : PlayerId
        -typedRoomId : String
        +show(msg : String) void
        +focusEntryField() void
    }
    class HtttpClient {
        <<boundary>>
        +joinRoom(playerId : PlayerId, roomId : String) HtttpResponse
    }
    class RoomController {
        <<control>>
        +join(playerId : PlayerId, roomId : String) RoomView
    }
    class Lobby {
        -rooms : Map~String, Room~
        +findRoom(roomId : String) Room
    }
    class Room {
        -roomId : String
        -status : GameRoomStatus
        -numberOfPlayers : int
        -slotCount : int
        +canAccept() JoinVerdict
        +seat(playerId : PlayerId) Slot
        +recomputeStatus() void
        +stateMessage() String
    }
    class Slot {
        -index : int
        -status : SlotStatus
        -occupant : Membership
        +occupy(m : Membership) void
    }
    class Membership {
        -role : PlayerStatus
        -playerId : PlayerId
        -roomId : String
    }
    class JoinVerdict {
        <<enum>>
        ACCEPTED
        FULL
        IN_GAME
        NOT_FOUND
    }
    class GameRoomStatus {
        <<enum>>
        WAITING
        READY
        IN_GAME
        FINISHED
    }
    class SlotStatus {
        <<enum>>
        WAITING
        JOINING
        LEAVING
        READY
    }
    class PlayerStatus {
        <<enum>>
        OWNER
        PLAYER
    }

    LobbyUI ..> HtttpClient : 2. [ENTER] on typed id
    HtttpClient ..> RoomController : JOIN /room/id [Player-Id]
    RoomController ..> Lobby : 4. findRoom (ext 4a → 404)
    RoomController ..> Room : 4. canAccept then seat
    Lobby *--> "0..*" Room
    Room ..> JoinVerdict : ext 4b FULL / 4c IN_GAME
    Room *--> "1..99" Slot
    Room o--> GameRoomStatus
    Slot o--> SlotStatus
    Slot o--> "0..1" Membership
    Membership o--> PlayerStatus : PLAYER
```

### CD UC-07 — Leave Room (with UC-07a)

```mermaid
classDiagram
    direction TB

    class WaitingRoomUI {
        <<boundary>>
        -playerId : PlayerId
        -roomId : String
        +show(msg : String) void
    }
    class HtttpClient {
        <<boundary>>
        +leaveRoom(playerId : PlayerId, roomId : String) HtttpResponse
    }
    class RoomController {
        <<control>>
        +leave(playerId : PlayerId, roomId : String) HtttpStatus
    }
    class Lobby {
        -rooms : Map~String, Room~
        +findRoom(roomId : String) Room
        +destroyRoom(roomId : String) void
    }
    class Room {
        -roomId : String
        -status : GameRoomStatus
        -minToStart : int
        -numberOfPlayers : int
        +release(playerId : PlayerId) void
        +selectSuccessor() Membership
        +recomputeStatus() void
        +stateMessage() String
        +narrate(text : String) void
    }
    class Slot {
        -index : int
        -status : SlotStatus
        -occupant : Membership
        +setStatus(s : SlotStatus) void
        +clearData() void
        +isFree() bool
    }
    class Membership {
        -role : PlayerStatus
        -playerId : PlayerId
        -roomId : String
        +setRole(r : PlayerStatus) void
        +isOwner() bool
    }
    class Narration {
        -text : String
        -at : timestamp
    }
    class GameRoomStatus {
        <<enum>>
        WAITING
        READY
        IN_GAME
        FINISHED
    }
    class SlotStatus {
        <<enum>>
        WAITING
        JOINING
        LEAVING
        READY
    }
    class PlayerStatus {
        <<enum>>
        OWNER
        PLAYER
    }

    WaitingRoomUI ..> HtttpClient : 1. [L] leave
    HtttpClient ..> RoomController : LEAVE /room/id
    RoomController ..> Lobby : 2. findRoom
    RoomController ..> Room : 3. release(playerId)
    Room ..> Lobby : ext 3b last player → destroyRoom
    Room ..> Membership : «include» UC-07a selectSuccessor
    Lobby *--> "0..*" Room : owns lifetime
    Room *--> "1..99" Slot : owner slot LEAVING → WAITING
    Room *--> "0..*" Narration : 4. left the room
    Room o--> GameRoomStatus : READY → WAITING below min
    Slot o--> SlotStatus
    Slot o--> "0..1" Membership : cleared on departure
    Membership o--> PlayerStatus : UC-07a setRole(OWNER)
```

---

## Combined Domain Class Diagram

### Concept extraction

| Source (UC + step) | Concept surfaced | Class |
|---|---|---|
| UC-01.1–4 "username / password" | credentials, the account being created | `Account` |
| UC-01.5 "hashes with a per-user salt" | the hash + salt pair, a value object | `Credential` |
| UC-01.7 "wallet = 0, starter character/theme" | the durable player document | `Player` |
| UC-02.3 "enters the Server ID" | the server the client dials | `ServerEndpoint` |
| UC-02.5 "authenticated handshake" | the secure session, UC-02a | `Session` |
| UC-02.7 "opens a session" | authenticated identity for later requests | `Session` (holds `Player-Id`) |
| UC-03.2 "current room directory" | the collection of live rooms | `Lobby` |
| UC-03.3 "ID, Mode, Players, State, Owner" | one row of the directory | `RoomSummary` |
| UC-03.4 "username, score, ranking" | header data | `Player`, `LeaderboardEntry` |
| UC-04.2 "`[1] Double` / `[2] Battle Royale`" | mode with slot_count / min_to_start | `GameMode` `«enum»` |
| UC-04.5 "initialises the room with `slot_count` slots" | the room aggregate | `Room` |
| UC-04.5 "seats the Owner in slot 1" | a seat that can be empty or occupied | `Slot` |
| UC-04.5 "marks the Player as `OWNER`" | role *within a room*, not a global type | `Membership` + `PlayerStatus` `«enum»` |
| UC-04.5 "room status `WAITING`" | room lifecycle | `GameRoomStatus` `«enum»` |
| UC-04.5 slot `WAITING → JOINING → READY` | slot lifecycle | `SlotStatus` `«enum»` |
| UC-04.6 "STATE message `WAITING FOR OPPONENT`" | derived text keyed by room status | `Room.stateMessage()` (derived, not stored) |
| UC-04.7 "Server narrates to the room" | system-authored room message | `Narration` |
| UC-07.3 "`number_of_players -= 1`" | occupancy counter | `Room.numberOfPlayers` |
| UC-07a.1 "next player in slot order" | successor selection | `Room.selectSuccessor()` |
| UC-07 alt 3b "room is destroyed" | room lifetime is owned by the lobby | `Lobby *--> Room` |

### Domain diagram

```mermaid
classDiagram
    direction TB

    class Guest {
        <<actor>>
        +username : String
        +password : String
    }
    class Administrator {
        <<actor>>
    }

    class Player {
        +playerId : PlayerId
        +username : String
        +credential : Credential
        +leaderboardScore : long
        +walletPoints : long
        +equippedCharacter : ItemId
        +equippedTheme : ItemId
        +gamesPlayed : int
        +gamesWon : int
    }
    class Credential {
        <<value object>>
        +passwordHashed : String
        +salt : String
    }
    class Session {
        +sessionId : String
        +playerId : PlayerId
        +endpoint : ServerEndpoint
        +establishedAt : timestamp
    }
    class ServerEndpoint {
        <<value object>>
        +serverId : String
        +host : String
        +port : int
    }

    class Lobby {
        +listOpenRooms() RoomSummary[]
        +createRoom(owner, mode) Room
        +findRoom(roomId) Room
        +destroyRoom(roomId)
    }
    class RoomSummary {
        <<value object>>
        +roomId : String
        +mode : GameMode
        +occupancy : String
        +status : GameRoomStatus
        +ownerName : String
    }
    class Room {
        +roomId : String
        +mode : GameMode
        +status : GameRoomStatus
        +slotCount : int
        +minToStart : int
        +numberOfPlayers : int
        +stateMessage() String
        +seat(player) Slot
        +release(playerId)
        +selectSuccessor() Membership
        +recomputeStatus()
    }
    class Slot {
        +index : int
        +status : SlotStatus
        +occupantId : PlayerId
    }
    class Membership {
        +role : PlayerStatus
        +joinedAt : timestamp
    }
    class Narration {
        +text : String
        +at : timestamp
    }
    class LeaderboardEntry {
        +playerId : PlayerId
        +username : String
        +score : long
        +rank : int
    }

    class GameMode {
        <<enum>>
        SINGLE
        DOUBLE
        BATTLE_ROYALE
    }
    class GameRoomStatus {
        <<enum>>
        WAITING
        READY
        IN_GAME
        FINISHED
    }
    class SlotStatus {
        <<enum>>
        WAITING
        JOINING
        LEAVING
        READY
    }
    class PlayerStatus {
        <<enum>>
        OWNER
        PLAYER
    }

    Guest ..> Player : registers as / authenticates to
    Player *--> Credential : owns
    Player --> "0..1" Session : authenticated by
    Session o--> ServerEndpoint : connected to

    Lobby *--> "0..*" Room : owns lifetime
    Lobby ..> RoomSummary : produces
    Room *--> "1..99" Slot : slots
    Room *--> "0..*" Narration : broadcasts
    Room o--> GameMode
    Room o--> GameRoomStatus
    Slot o--> SlotStatus
    Slot o--> "0..1" Membership : occupied by

    Membership o--> PlayerStatus : role
    Membership o--> "1" Player : member
    Membership o--> "1" Room : of room

    Player ..> LeaderboardEntry : ranked as
```

---

## Sequence Diagrams

### Layering conventions

| Participant | Layer | Process |
|---|---|---|
| `:LoginUI`, `:SignUpUI`, `:LobbyUI`, `:WaitingRoomUI` | UI | tetrisu |
| `:AuthController`, `:LobbyController`, `:RoomController` | Controller | tetrisd |
| `:HtttpClient` | Boundary | tetrisu |
| `:Lobby`, `:Room`, `:Slot`, `:Membership`, `:Player` | Domain | tetrisd |
| `:Db` | Persistence | `libmacminidb` |

### SD UC-01 — Register Account

```mermaid
sequenceDiagram
    actor Guest
    participant UI as :SignUpUI
    participant C as :HtttpClient
    participant AC as :AuthController
    participant Cr as :Credential
    participant DB as :Db

    Guest->>UI: pressSignUp(username, password, confirm-password)
    activate UI

    alt password != confirm  (ext 5a)
        UI->>UI: validateMatch(password, confirm) : false
        activate UI
        deactivate UI
        UI-->>Guest: show("passwords do not match")
    else passwords match
        UI->>C: signup(username, password)
        activate C
        C->>AC: SIGNUP /account {username, password}
        activate AC

        AC->>Cr: create(password)
        activate Cr
        Cr->>Cr: generateSalt()
        Cr->>Cr: hash(password, salt)
        Cr-->>AC: credential(passwordHashed, salt)
        deactivate Cr

        AC->>DB: db_signup(username, passwordHashed, salt, &id)
        activate DB

        alt DB_OK
            DB->>DB: persist(player{wallet:0, character:1, theme:1})
            DB-->>AC: DB_OK, playerId
            AC-->>C: 201 Created
            C-->>UI: registered(playerId)
            UI-->>Guest: show("account created") / route to Login
        else DB_EXISTS  (ext 6a)
            DB-->>AC: DB_EXISTS
            AC-->>C: 409 Conflict
            C-->>UI: error(409)
            UI-->>Guest: show("username taken")
        else DB_IO_ERROR  (E1)
            DB-->>AC: DB_IO_ERROR
            AC-->>C: 500
            C-->>UI: error(500)
            UI-->>Guest: show("connection error")
        end

        deactivate DB
        deactivate AC
        deactivate C
    end
    deactivate UI
```

### SD UC-02 — Log In

```mermaid
sequenceDiagram
    actor Guest
    participant UI as :LoginUI
    participant C as :HtttpClient
    participant S as :Session
    participant AC as :AuthController
    participant DB as :Db

    Guest->>UI: enter(username, password, serverId)
    activate UI
    Guest->>UI: pressLogin()
    UI->>C: login(username, password, serverId)
    activate C

    rect rgb(240, 240, 240)
        note over C,S: «include» UC-02a Connect to Server
        C->>S: connect(endpoint(serverId))
        activate S
        S->>S: nonce → cert verify → RSA-OAEP AES key
        alt handshake ok
            S-->>C: session up
        else handshake fails  (E1)
            S-->>C: connectionDropped
            C-->>UI: error(unreachable)
            UI-->>Guest: show("cannot reach server")
        end
    end

    C->>AC: LOGIN /session {username, password}
    activate AC

    AC->>DB: db_get_player(username)  %% fetch salt
    activate DB
    DB-->>AC: player | DB_NOT_FOUND
    deactivate DB

    AC->>AC: hash(password, player.salt)

    AC->>DB: db_login(username, passwordHashed, &player)
    activate DB

    alt DB_OK
        DB-->>AC: DB_OK, player
        AC->>S: bind(playerId)
        S-->>AC: bound
        AC-->>C: 200 OK + Player-Id
        C-->>UI: session(playerId, player)
        UI-->>Guest: show(Home page)
    else DB_BAD_CREDS or DB_NOT_FOUND  (ext 6a)
        DB-->>AC: DB_BAD_CREDS | DB_NOT_FOUND
        AC-->>C: 401 Unauthorized
        C-->>UI: error(401)
        UI-->>Guest: show("invalid username or password")
    end

    deactivate DB
    deactivate AC
    deactivate S
    deactivate C
    deactivate UI
```

### SD UC-03 — Browse Open Rooms

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :LobbyUI
    participant C as :HtttpClient
    participant LC as :LobbyController
    participant L as :Lobby
    participant R as :Room
    participant DB as :Db

    P->>UI: selectMultiplayer()
    activate UI
    UI->>C: browseRooms(playerId)
    activate C
    C->>LC: LIST /rooms  [Player-Id]
    activate LC

    LC->>L: listOpenRooms()
    activate L
    loop for each Room in lobby
        L->>R: toSummary()
        activate R
        R->>R: stateMessage()
        R-->>L: RoomSummary{id, mode, occupancy, status, owner}
        deactivate R
    end
    L-->>LC: RoomSummary[]
    deactivate L

    LC->>DB: db_get_player(playerId)
    activate DB
    DB-->>LC: player(username, leaderboardScore)
    deactivate DB

    LC->>DB: db_rank(playerId)
    activate DB
    DB-->>LC: rank
    deactivate DB

    LC-->>C: 200 OK {rooms[], header{username, score, rank}}
    deactivate LC
    C-->>UI: roomList, header
    deactivate C

    alt rooms empty  (ext 2a)
        UI-->>P: show(empty list + "Create Room / Join by ID")
    else rooms present
        UI-->>P: show(room rows + header)
    end
    deactivate UI

    opt [R] Refresh  (UC-03a)
        P->>UI: pressRefresh()
        activate UI
        UI->>C: browseRooms(playerId)
        note right of C: re-runs the LIST /rooms exchange above
        deactivate UI
    end

    opt [B] Back  (ext 3b)
        P->>UI: pressBack()
        activate UI
        UI-->>P: show(Home page)
        deactivate UI
    end
```

### SD UC-04 — Create Room

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :LobbyUI
    participant C as :HtttpClient
    participant RC as :RoomController
    participant L as :Lobby
    participant R as :Room
    participant S1 as :Slot[1]
    participant M as :Membership

    P->>UI: pressCreate()
    activate UI
    UI-->>P: show(Create Room modal: [1] Double, [2] Battle Royale)
    P->>UI: selectMode(mode)

    alt [ESC] cancel  (ext 3a)
        P->>UI: pressEsc()
        UI-->>P: show(Lobby)
    else [ENTER] confirm
        P->>UI: pressEnter()
        UI->>C: createRoom(playerId, mode)
        activate C
        C->>RC: JOIN /room/<newId>  Mode: <mode>  [Player-Id]
        activate RC

        RC->>L: createRoom(playerId, mode)
        activate L
        L->>R: new Room(roomId, mode)
        activate R
        R->>R: slotCount, minToStart = fromMode(mode)
        loop slotCount times
            R->>S1: new Slot(index, WAITING)
            activate S1
            deactivate S1
        end
        R-->>L: room(status = WAITING)
        deactivate R
        deactivate L

        RC->>R: seat(playerId)
        activate R
        R->>S1: setStatus(JOINING)
        activate S1
        S1-->>R: JOINING
        deactivate S1
        R->>M: new Membership(playerId, roomId, role = OWNER)
        activate M
        M-->>R: membership
        deactivate M
        R->>S1: occupy(membership) → setStatus(READY)
        activate S1
        S1-->>R: READY
        deactivate S1
        R->>R: numberOfPlayers += 1  (= 1)
        R->>R: recomputeStatus()
        note right of R: 1 < minToStart → stays WAITING
        R-->>RC: seated(slot 1, OWNER)
        deactivate R

        alt seating ok
            RC-->>C: 201 Created
            C-->>UI: created(roomId, slot 1, OWNER)
            UI-->>P: show(Waiting Room, "WAITING FOR OPPONENT")
            RC->>R: narrate("PLAYER <name> joined the room <id>")
            activate R
            RC->>R: narrate("PLAYER <name> set as owner")
            R-->>RC: broadcast ok
            deactivate R
        else disconnect during join  (ext 5a)
            R->>S1: setStatus(WAITING)
            activate S1
            deactivate S1
            R->>L: destroyRoom(roomId)
            activate L
            L-->>RC: destroyed
            deactivate L
            RC-->>C: 500
            C-->>UI: error
            UI-->>P: show(Lobby)
        end

        deactivate RC
        deactivate C
    end
    deactivate UI
```

### SD UC-05 — Join Room from List

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :LobbyUI
    participant C as :HtttpClient
    participant RC as :RoomController
    participant L as :Lobby
    participant R as :Room
    participant Sn as :Slot[n]
    participant M as :Membership

    P->>UI: highlightRoom(roomId)
    activate UI
    P->>UI: pressEnter()
    UI->>C: joinRoom(playerId, roomId)
    activate C
    C->>RC: JOIN /room/<id>  [Player-Id]
    activate RC

    RC->>L: findRoom(roomId)
    activate L

    alt room not found  (ext 4c)
        L-->>RC: null
        RC-->>C: 404 Not Found
        C-->>UI: error(404)
        UI-->>P: show("no such room") / refresh list
    else room found
        L-->>RC: room
        RC->>R: canAccept()
        activate R

        alt status == IN_GAME  (ext 4b)
            R-->>RC: false(IN_GAME)
            RC-->>C: 409 Conflict
            C-->>UI: error(409)
            UI-->>P: show("game already in progress")
        else numberOfPlayers == slotCount  (ext 4a)
            R-->>RC: false(FULL)
            RC-->>C: 409 Conflict
            C-->>UI: error(409)
            UI-->>P: show("room full")
        else acceptable
            R-->>RC: true
            RC->>R: seat(playerId)
            R->>Sn: setStatus(JOINING)
            activate Sn
            Sn-->>R: JOINING
            deactivate Sn
            R->>M: new Membership(playerId, roomId, role = PLAYER)
            activate M
            M-->>R: membership
            deactivate M
            R->>Sn: occupy(membership) → setStatus(READY)
            activate Sn
            Sn-->>R: READY
            deactivate Sn
            R->>R: numberOfPlayers += 1
            R->>R: recomputeStatus()

            alt numberOfPlayers >= minToStart
                note right of R: WAITING → READY
                R->>R: status = READY
            else still below minToStart  (ext 4d)
                note right of R: BR 2/4 — slot READY, room stays WAITING
            end

            R-->>RC: seated(slot n, PLAYER, status)
            RC-->>C: 200 OK {room, slots, stateMessage}
            C-->>UI: joined(room)
            UI-->>P: show(Waiting Room + stateMessage)
            RC->>R: narrate("PLAYER <name> joined the room <id>")
            R-->>RC: broadcast ok
        end

        deactivate R
    end

    deactivate L
    deactivate RC
    deactivate C
    deactivate UI
```

### SD UC-06 — Join Room by Room ID

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :LobbyUI
    participant C as :HtttpClient
    participant RC as :RoomController
    participant L as :Lobby
    participant R as :Room

    P->>UI: typeRoomId(text)
    activate UI
    P->>UI: pressEnter()
    UI->>UI: tfd = roomId
    UI->>C: joinRoom(playerId, roomId)
    activate C
    C->>RC: JOIN /room/<id>  [Player-Id]
    activate RC

    RC->>L: findRoom(roomId)
    activate L
    L-->>RC: room | null
    deactivate L

    alt room == null  (ext 4a)
        RC-->>C: 404 Not Found
        C-->>UI: error(404)
        UI-->>P: show("no such room") / focus entry field
    else room full  (ext 4b)
        RC-->>C: 409 Conflict
        C-->>UI: error(409)
        UI-->>P: show("room full") / focus entry field
    else room IN_GAME  (ext 4c)
        RC-->>C: 409 Conflict
        C-->>UI: error(409)
        UI-->>P: show("game already in progress") / focus entry field
    else acceptable
        note over RC,R: seating + recomputeStatus() identical to UC-05
        RC->>R: seat(playerId)
        activate R
        R-->>RC: seated(slot n, PLAYER, status)
        deactivate R
        RC-->>C: 200 OK {room, slots, stateMessage}
        C-->>UI: joined(room)
        UI-->>P: show(Waiting Room)
    end

    deactivate RC
    deactivate C
    deactivate UI
```

### SD UC-07 — Leave Room (includes UC-07a)

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :WaitingRoomUI
    participant C as :HtttpClient
    participant RC as :RoomController
    participant L as :Lobby
    participant R as :Room
    participant So as :Slot[owner]
    participant Ms as :Membership[successor]

    P->>UI: pressLeave()
    activate UI
    UI->>C: leaveRoom(playerId, roomId)
    activate C
    C->>RC: LEAVE /room/<id>  [Player-Id]
    activate RC

    RC->>L: findRoom(roomId)
    activate L
    L-->>RC: room | null
    deactivate L

    alt not in room
        RC-->>C: 404 Not Found
        C-->>UI: error(404)
        UI-->>P: show("not in room")
    else in room
        RC->>R: release(playerId)
        activate R
        R->>So: setStatus(LEAVING)
        activate So
        So-->>R: LEAVING
        deactivate So

        alt leaver is OWNER and others remain  (ext 3a → «include» UC-07a)
            rect rgb(240, 240, 240)
                note over R,Ms: UC-07a Transfer Room Ownership
                R->>R: selectSuccessor()
                note right of R: next player in slot order
                R->>Ms: setRole(OWNER)
                activate Ms
                Ms-->>R: OWNER
                deactivate Ms
                R->>R: moveToVacatedSlot(successor)
                R->>R: broadcastRoomUpdate(newOwner)
                R->>R: narrate("PLAYER <name> set as the owner")
                note right of R: 1b — successor disconnected → repeat with next in slot order
            end
        end

        R->>So: clearData() → setStatus(WAITING)
        activate So
        So-->>R: WAITING
        deactivate So
        R->>R: numberOfPlayers -= 1
        R->>R: recomputeStatus()

        alt numberOfPlayers == 0  (ext 3b)
            R->>L: destroyRoom(roomId)
            activate L
            L->>L: clear slots, tear down chat
            L-->>R: ROOM_DESTROYED
            deactivate L
        else numberOfPlayers < minToStart
            note right of R: READY → WAITING, msg "WAITING FOR OPPONENT"
            R->>R: status = WAITING
        end

        R-->>RC: released
        deactivate R

        RC-->>C: 200 OK
        C-->>UI: left()
        UI-->>P: show(Lobby)
        RC->>R: narrate("PLAYER <name> left the room <id>")
        activate R
        R-->>RC: broadcast ok
        deactivate R
    end

    deactivate RC
    deactivate C
    deactivate UI
```

---

## Solution Class Diagram

### Method inventory by use case

| Behaviour | Owner | Signature | From |
|---|---|---|---|
| validate password match | `SignUpUI` | `validateMatch(String, String) : bool` | UC-01.5 |
| hash with fresh salt | `Credential` | `create(String) : Credential` | UC-01.5 |
| create the account | `AuthController` | `signup(String, String) : HtttpStatus` | UC-01.6 |
| authenticate | `AuthController` | `login(String, String) : LoginResult` | UC-02.6 |
| establish secure channel | `Session` | `connect(ServerEndpoint) : bool` | UC-02a |
| bind identity to session | `Session` | `bind(PlayerId)` | UC-02.7 |
| list rooms | `Lobby` | `listOpenRooms() : RoomSummary[]` | UC-03.2 |
| project a room to a row | `Room` | `toSummary() : RoomSummary` | UC-03.3 |
| derive STATE text | `Room` | `stateMessage() : String` | UC-04.6 |
| header data | `LobbyController` | `header(PlayerId) : HeaderDto` | UC-03.4 |
| create + register a room | `Lobby` | `createRoom(PlayerId, GameMode) : Room` | UC-04.5 |
| resolve mode parameters | `GameMode` | `slotCount() : int`, `minToStart() : int` | UC-04.5 |
| seat a player | `Room` | `seat(PlayerId) : Slot` | UC-04.5 / UC-05.4 |
| slot lifecycle | `Slot` | `setStatus(SlotStatus)`, `occupy(Membership)`, `clearData()` | UC-04.5 |
| admissibility check | `Room` | `canAccept() : JoinVerdict` | UC-05 ext 4a/4b |
| recompute room status | `Room` | `recomputeStatus()` | UC-05.4 |
| free a slot | `Room` | `release(PlayerId)` | UC-07.3 |
| pick the successor | `Room` | `selectSuccessor() : Membership` | UC-07a.1 |
| change role | `Membership` | `setRole(PlayerStatus)` | UC-07a.1 |
| system message | `Room` | `narrate(String)` | UC-04.7 / UC-07.4 |
| destroy the room | `Lobby` | `destroyRoom(String)` | UC-07 ext 3b |

### Solution diagram

```mermaid
j
```

---

## Traceability Matrix

| UC | Sequence diagram | Domain classes exercised | New solution methods |
|---|---|---|---|
| UC-01 | [SD UC-01](#sd-uc-01--register-account) | `Credential`, `Player` | `SignUpUI.validateMatch`, `Credential.create`, `AuthController.signup` |
| UC-02 | [SD UC-02](#sd-uc-02--log-in) | `Session`, `ServerEndpoint`, `Credential`, `Player` | `Session.connect`, `Session.bind`, `AuthController.login` |
| UC-03 | [SD UC-03](#sd-uc-03--browse-open-rooms) | `Lobby`, `Room`, `RoomSummary`, `Player` | `Lobby.listOpenRooms`, `Room.toSummary`, `LobbyController.header` |
| UC-04 | [SD UC-04](#sd-uc-04--create-room) | `Lobby`, `Room`, `Slot`, `Membership`, `GameMode` | `Lobby.createRoom`, `Room.seat`, `GameMode.slotCount/minToStart` |
| UC-05 | [SD UC-05](#sd-uc-05--join-room-from-list) | `Lobby`, `Room`, `Slot`, `Membership` | `Room.canAccept`, `Room.recomputeStatus`, `RoomController.join` |
| UC-06 | [SD UC-06](#sd-uc-06--join-room-by-room-id) | same as UC-05 | *(none — reuses `RoomController.join`)* |
| UC-07 | [SD UC-07](#sd-uc-07--leave-room-includes-uc-07a) | `Room`, `Slot`, `Membership`, `Lobby` | `Room.release`, `Room.selectSuccessor`, `Membership.setRole`, `Lobby.destroyRoom` |
| UC-07a | folded into SD UC-07 | `Room`, `Membership` | `Room.selectSuccessor`, `Membership.setRole` |