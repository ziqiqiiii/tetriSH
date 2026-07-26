# tetriSH — Administration / `tetrisctl` (UC-22 – UC-27)

## Table of Contents

- [Per-Use-Case Class Diagrams](#per-use-case-class-diagrams)
  - [CD UC-22 — Query Server Status](#cd-uc-22--query-server-status)
  - [CD UC-23 — Graceful Shutdown](#cd-uc-23--graceful-shutdown)
  - [CD UC-24 — Kick Player (with UC-07a)](#cd-uc-24--kick-player-with-uc-07a)
  - [CD UC-25 — List Rooms](#cd-uc-25--list-rooms)
  - [CD UC-26 — List Players](#cd-uc-26--list-players)
  - [CD UC-27 — Query Dropped Logs](#cd-uc-27--query-dropped-logs)
- [Combined Domain Class Diagram](#combined-domain-class-diagram)
  - [Concept extraction](#concept-extraction)
  - [Domain diagram](#domain-diagram)
- [Sequence Diagrams](#sequence-diagrams)
  - [Layering conventions](#layering-conventions)
  - [SD UC-22 — Query Server Status](#sd-uc-22--query-server-status)
  - [SD UC-23 — Graceful Shutdown](#sd-uc-23--graceful-shutdown)
  - [SD UC-24 — Kick Player (includes UC-07a)](#sd-uc-24--kick-player-includes-uc-07a)
  - [SD UC-25 — List Rooms](#sd-uc-25--list-rooms)
  - [SD UC-26 — List Players](#sd-uc-26--list-players)
  - [SD UC-27 — Query Dropped Logs](#sd-uc-27--query-dropped-logs)
- [Solution Class Diagram](#solution-class-diagram)
  - [Method inventory by use case](#method-inventory-by-use-case)
  - [Solution diagram](#solution-diagram)

---

## Per-Use-Case Class Diagrams

### CD UC-22 — Query Server Status

```mermaid
classDiagram
    direction LR

    class tetrisctl {
        <<boundary>>
        +status() void
    }
    class CtlSocket {
        <<boundary>>
        -socketPath : String
        +connect() bool
        +send(req : HtttpRequest) HtttpResponse
    }
    class CtlListenerThread {
        <<control>>
        +handleStatus() StatusSnapshot
    }
    class tetrisd {
        -uptime_s : long
        -pid : int
        +getRoomCount() int
        +getPlayerCount() int
        +getTcpListenerStatus() String
        +getLogdStatus() String
    }
    class StatusSnapshot {
        <<value object>>
        -uptime_s : long
        -rooms : int
        -players : int
        -tcpListener : String
        -logd : String
        -pid : int
    }
    class AuditLog {
        <<interface>>
        +log(event : String, actor : String) void
    }
    class HtttpStatus {
        <<enum>>
        200_OK
        500_INTERNAL
    }

    tetrisctl ..> CtlSocket : 1. connects to control socket
    CtlSocket ..> CtlListenerThread : 2. STATUS /admin HTTTP/1.0
    CtlListenerThread ..> tetrisd : 3. gathers snapshot
    tetrisd ..> StatusSnapshot : 3. assembles
    CtlListenerThread ..> AuditLog : 4. logs the query
    CtlListenerThread ..> HtttpStatus : returns 200 OK
    CtlListenerThread ..> CtlSocket : 5. 200 OK + body
    CtlSocket ..> tetrisctl : 6. prints snapshot, exits
    note for CtlListenerThread "ext 2a socket missing/unreachable →<br/>tetrisctl prints error, exits non-zero (no response)"
```

### CD UC-23 — Graceful Shutdown

```mermaid
classDiagram
    direction LR

    class tetrisctl {
        <<boundary>>
        +shutdown() void
    }
    class CtlSocket {
        <<boundary>>
        -socketPath : String
        +connect() bool
        +send(req : HtttpRequest) HtttpResponse
    }
    class CtlListenerThread {
        <<control>>
        +handleShutdown() void
    }
    class tetrisd {
        -tcpAcceptor : Acceptor
        -roomTickers : Ticker[]
        +stopAccepting() void
        +stopTickers() void
        +endInFlightRooms() void
        +flushLogsToLogd() void
        +freeResources() void
        +exit() void
    }
    class Db {
        <<interface>>
        libmacminidb
        +db_close() void
    }
    class Flusher {
        -appendLog : File
        +stop() void
        +finalFsync() void
    }
    class tetrislogd {
        <<supporting actor>>
        +receivePendingRecords() void
    }
    class AuditLog {
        <<interface>>
        +log(event : String, actor : String) void
    }
    class HtttpStatus {
        <<enum>>
        202_ACCEPTED
        500_INTERNAL
    }

    tetrisctl ..> CtlSocket : 1. connects to control socket
    CtlSocket ..> CtlListenerThread : 2. SHUTDOWN /admin HTTTP/1.0
    CtlListenerThread ..> tetrisd : 4. initiates shutdown sequence
    CtlListenerThread ..> CtlSocket : 3. 202 Accepted
    CtlSocket ..> tetrisctl : 3. prints confirmation, exits
    tetrisd ..> tetrisd : 4. stopAccepting → stopTickers
    tetrisd ..> tetrisd : 5. endInFlightRooms (no db_record_game)
    tetrisd ..> tetrislogd : 5. flushLogsToLogd (pending records)
    tetrisd ..> Db : 6. db_close
    Db ..> Flusher : 6. stop → finalFsync
    tetrisd ..> AuditLog : logs shutdown event
    tetrisd ..> tetrisd : 7. freeResources → close socket → exit
    CtlListenerThread ..> HtttpStatus : returns 202 Accepted
    note for tetrisd "ext 4a mid-play game → terminate,<br/>countdown shown; NO db_record_game"
```

### CD UC-24 — Kick Player (with UC-07a)

```mermaid
classDiagram
    direction LR

    class tetrisctl {
        <<boundary>>
        +kick(playerId : String) void
    }
    class CtlSocket {
        <<boundary>>
        -socketPath : String
        +connect() bool
        +send(req : HtttpRequest) HtttpResponse
    }
    class CtlListenerThread {
        <<control>>
        +handleKick(playerId : PlayerId) HtttpStatus
    }
    class tetrisd {
        -sessions : Map~PlayerId, Session~
        +findSession(playerId : PlayerId) Session
        +closeSession(session : Session) void
    }
    class Session {
        -playerId : PlayerId
        -roomId : String
        +close() void
    }
    class Room {
        -roomId : String
        -numberOfPlayers : int
        +release(playerId : PlayerId) void
        +selectSuccessor() Membership
        +recomputeStatus() void
        +broadcast(update : RoomUpdate) void
        +narrate(text : String) void
    }
    class Slot {
        -index : int
        -status : SlotStatus
        -occupant : Membership
        +setStatus(s : SlotStatus) void
        +clearData() void
    }
    class Membership {
        -role : PlayerStatus
        -playerId : PlayerId
        +isOwner() bool
        +setRole(r : PlayerStatus) void
    }
    class AuditLog {
        <<interface>>
        +log(event : String, actor : String) void
    }
    class HtttpStatus {
        <<enum>>
        200_OK
        404_NOT_FOUND
        400_BAD_ARGUMENT
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

    tetrisctl ..> CtlSocket : 1. connects to control socket
    CtlSocket ..> CtlListenerThread : KICK /admin/player/pid HTTTP/1.0
    CtlListenerThread ..> tetrisd : 2. findSession(pid)
    tetrisd ..> Session : locates
    CtlListenerThread ..> Session : 2. close()
    CtlListenerThread ..> Room : 2. release(playerId)
    Room ..> Slot : slot READY → LEAVING → WAITING
    Room ..> Membership : «include» UC-07a selectSuccessor
    Membership ..> PlayerStatus : successor → OWNER
    Room ..> Room : recomputeStatus + broadcast
    Room ..> Room : narrate("PLAYER kicked")
    CtlListenerThread ..> AuditLog : 3. log(kick event)
    CtlListenerThread ..> HtttpStatus : returns 200 OK
    CtlListenerThread ..> CtlSocket : 3. 200 OK
    CtlSocket ..> tetrisctl : 3. prints result, exits
    Slot o--> SlotStatus
    Slot o--> "0..1" Membership
    note for CtlListenerThread "ext 2a player not found/already gone →<br/>404, no change"
```

### CD UC-25 — List Rooms

```mermaid
classDiagram
    direction LR

    class tetrisctl {
        <<boundary>>
        +rooms() void
    }
    class CtlSocket {
        <<boundary>>
        -socketPath : String
        +connect() bool
        +send(req : HtttpRequest) HtttpResponse
    }
    class CtlListenerThread {
        <<control>>
        +handleListRooms() RoomSnapshot[]
    }
    class tetrisd {
        -roomDirLock : Mutex
        +acquireRoomDirLock() void
        +releaseRoomDirLock() void
    }
    class Lobby {
        -rooms : Map~String, Room~
        +listAllRooms() RoomSnapshot[]
    }
    class Room {
        -roomId : String
        -mode : GameMode
        -status : GameRoomStatus
        -numberOfPlayers : int
        -tick : long
        +toSnapshot() RoomSnapshot
    }
    class RoomSnapshot {
        <<value object>>
        -id : String
        -players : int
        -state : String
        -tick : long
        -mode : GameMode
        -owner : String
    }
    class AuditLog {
        <<interface>>
        +log(event : String, actor : String) void
    }
    class HtttpStatus {
        <<enum>>
        200_OK
        500_INTERNAL
    }
    class GameRoomStatus {
        <<enum>>
        WAITING
        READY
        IN_GAME
        FINISHED
    }
    class GameMode {
        <<enum>>
        SINGLE
        DOUBLE
        BATTLE_ROYALE
    }

    tetrisctl ..> CtlSocket : 1. connects to control socket
    CtlSocket ..> CtlListenerThread : ROOMS /admin HTTTP/1.0
    CtlListenerThread ..> tetrisd : 2. acquireRoomDirLock
    CtlListenerThread ..> Lobby : 2. listAllRooms
    Lobby *--> "0..*" Room : owns lifetime
    Room ..> RoomSnapshot : toSnapshot per room
    Room o--> GameRoomStatus
    Room o--> GameMode
    CtlListenerThread ..> tetrisd : 2. releaseRoomDirLock
    CtlListenerThread ..> AuditLog : 3. logs the query
    CtlListenerThread ..> HtttpStatus : returns 200 OK
    CtlListenerThread ..> CtlSocket : 3. 200 OK + room list
    CtlSocket ..> tetrisctl : 3. prints list, exits
    note for Lobby "same runtime data as UC-03,<br/>retrieved via the control plane"
```

### CD UC-26 — List Players

```mermaid
classDiagram
    direction LR

    class tetrisctl {
        <<boundary>>
        +players() void
    }
    class CtlSocket {
        <<boundary>>
        -socketPath : String
        +connect() bool
        +send(req : HtttpRequest) HtttpResponse
    }
    class CtlListenerThread {
        <<control>>
        +handleListPlayers() PlayerSnapshot[]
    }
    class tetrisd {
        -sessionTableLock : Mutex
        +acquireSessionLock() void
        +releaseSessionLock() void
    }
    class SessionTable {
        -sessions : Map~PlayerId, Session~
        +listConnected() PlayerSnapshot[]
    }
    class Session {
        -playerId : PlayerId
        -username : String
        -roomId : String
        -connectedAt : timestamp
    }
    class PlayerSnapshot {
        <<value object>>
        -id : PlayerId
        -user : String
        -room : String
        -score : long
    }
    class AuditLog {
        <<interface>>
        +log(event : String, actor : String) void
    }
    class HtttpStatus {
        <<enum>>
        200_OK
        500_INTERNAL
    }

    tetrisctl ..> CtlSocket : 1. connects to control socket
    CtlSocket ..> CtlListenerThread : PLAYERS /admin HTTTP/1.0
    CtlListenerThread ..> tetrisd : 2. acquireSessionLock
    CtlListenerThread ..> SessionTable : 2. listConnected
    SessionTable *--> "0..*" Session : owns
    Session ..> PlayerSnapshot : assembles per session
    CtlListenerThread ..> tetrisd : 2. releaseSessionLock
    CtlListenerThread ..> AuditLog : 3. logs the query
    CtlListenerThread ..> HtttpStatus : returns 200 OK
    CtlListenerThread ..> CtlSocket : 3. 200 OK + player list
    CtlSocket ..> tetrisctl : 3. prints list, exits
    note for SessionTable "provides the <player> targets<br/>consumed by UC-24 Kick Player"
```

### CD UC-27 — Query Dropped Logs

```mermaid
classDiagram
    direction LR

    class tetrisctl {
        <<boundary>>
        +droppedLogs() void
    }
    class CtlSocket {
        <<boundary>>
        -socketPath : String
        +connect() bool
        +send(req : HtttpRequest) HtttpResponse
    }
    class CtlListenerThread {
        <<control>>
        +handleDroppedLogs() DroppedLogCount
    }
    class tetrisd {
        -localDropCount : int
        +getLocalDropCount() int
    }
    class LogIpcChannel {
        <<boundary>>
        +queryDroppedCount() int
    }
    class tetrislogd {
        <<supporting actor>>
        -droppedRecords : int
        +getDroppedCount() int
    }
    class DroppedLogCount {
        <<value object>>
        -logdDropped : int
        -tetrisdLocalDropped : int
        +total() int
    }
    class AuditLog {
        <<interface>>
        +log(event : String, actor : String) void
    }
    class HtttpStatus {
        <<enum>>
        200_OK
        500_INTERNAL
    }

    tetrisctl ..> CtlSocket : 1. connects to control socket
    CtlSocket ..> CtlListenerThread : DROPPED-LOGS /admin HTTTP/1.0
    CtlListenerThread ..> tetrisd : 2. getLocalDropCount
    CtlListenerThread ..> LogIpcChannel : 2. queryDroppedCount
    LogIpcChannel ..> tetrislogd : IPC request → droppedRecords
    LogIpcChannel ..> CtlListenerThread : logd count
    CtlListenerThread ..> DroppedLogCount : 3. assembles total
    CtlListenerThread ..> AuditLog : 4. logs the query
    CtlListenerThread ..> HtttpStatus : returns 200 OK
    CtlListenerThread ..> CtlSocket : 3. 200 OK + counts
    CtlSocket ..> tetrisctl : 4. prints counts, exits
    note for LogIpcChannel "E1 tetrislogd unreachable → 500;<br/>tetrislogd survives tetrisd restarts, not vice versa"
```

---

## Combined Domain Class Diagram

### Concept extraction

| Source (UC + step) | Concept surfaced | Class |
|---|---|---|
| UC-22.2 "connects to the control socket" | the local-only admin transport | `CtlSocket` |
| UC-22.3 "gathers a snapshot (uptime, rooms, players/connections, tick rate)" | the assembled health snapshot | `StatusSnapshot` `«value object»` |
| UC-22 ext 2a "control socket missing/unreachable" | client-side failure with no daemon response | `CtlSocket.connect()` |
| UC-23.3 "stops accepting new TCP connections and stops room tickers" | the ordered stop sequence | `tetrisd.stopAccepting/stopTickers` |
| UC-23.4 "in-flight rooms are ended… pending log records shipped to `tetrislogd`" | the drain step before persistence closes | `tetrisd.endInFlightRooms/flushLogsToLogd` |
| UC-23.5 "`db_close` stops the flusher and performs a final fsync" | the durable-shutdown guarantee | `Db.db_close()` / `Flusher.finalFsync()` |
| UC-23 ext 4a "no `db_record_game` for unfinished games" | the same abandon-game rule as UC-10/11/12 | `tetrisd.endInFlightRooms()` |
| UC-24.2 "locates the player's session, closes it, frees their room slot" | the forced-disconnect path | `tetrisd.findSession` / `Session.close()` |
| UC-24.2 "if they were Room Owner) transfers ownership" | reuse of the successor-selection rule | `Room.selectSuccessor()` «include» UC-07a |
| UC-25.2 "reads its in-memory room directory (under the room-directory lock)" | the same lobby data UC-03 shows, over the control plane | `Lobby.listAllRooms()` |
| UC-26.2 "reads its connection/session table… assembles the connected-player list" | the live session table as an admin view | `SessionTable.listConnected()` |
| UC-26 "provides the `<player>` targets for UC-24" | list output feeding another use case's input | `PlayerSnapshot.id` |
| UC-27.2 "queries `tetrislogd`… for its dropped-records counter" | the cross-daemon drop counter, summed | `DroppedLogCount` `«value object»` |
| UC-27 E1 "`tetrislogd` unreachable" | the one admin query that can fail on a peer daemon, not on `tetrisd` itself | `LogIpcChannel.queryDroppedCount()` |
| UC-22–UC-27 "the action is logged" | the audit trail every control-plane call writes to | `AuditLog` |

### Domain diagram

```mermaid
classDiagram
    direction TB

    class Administrator {
        <<actor>>
        +runs tetrisctl commands
    }

    class tetrisctl {
        +status()
        +shutdown()
        +kick(playerId : String)
        +rooms()
        +players()
        +droppedLogs()
    }
    class CtlSocket {
        +socketPath : String
        +connect() bool
        +send(req : HtttpRequest) HtttpResponse
    }

    class tetrisd {
        +uptime_s : long
        +pid : int
        +sessions : Map~PlayerId, Session~
    }
    class StatusSnapshot {
        <<value object>>
        +uptime_s : long
        +rooms : int
        +players : int
    }
    class Db {
        <<interface>>
        libmacminidb
        +db_close()
    }
    class Flusher {
        +stop()
        +finalFsync()
    }

    class Session {
        +playerId : PlayerId
        +roomId : String
        +close()
    }
    class Lobby {
        +rooms : Map~String, Room~
        +listAllRooms() RoomSnapshot[]
    }
    class Room {
        +roomId : String
        +mode : GameMode
        +status : GameRoomStatus
        +release(playerId : PlayerId)
        +selectSuccessor() Membership
    }
    class RoomSnapshot {
        <<value object>>
        +id : String
        +players : int
        +state : String
    }
    class Membership {
        +role : PlayerStatus
        +isOwner() bool
    }
    class SessionTable {
        +sessions : Map~PlayerId, Session~
        +listConnected() PlayerSnapshot[]
    }
    class PlayerSnapshot {
        <<value object>>
        +id : PlayerId
        +user : String
        +room : String
    }

    class tetrislogd {
        <<supporting actor>>
        +droppedRecords : int
    }
    class LogIpcChannel {
        +queryDroppedCount() int
    }
    class DroppedLogCount {
        <<value object>>
        +logdDropped : int
        +tetrisdLocalDropped : int
        +total() int
    }

    class AuditLog {
        <<interface>>
        +log(event : String, actor : String)
    }
    class GameRoomStatus {
        <<enum>>
        WAITING
        READY
        IN_GAME
        FINISHED
    }
    class GameMode {
        <<enum>>
        SINGLE
        DOUBLE
        BATTLE_ROYALE
    }
    class PlayerStatus {
        <<enum>>
        OWNER
        PLAYER
    }

    Administrator --> tetrisctl : issues commands
    tetrisctl ..> CtlSocket : all admin ops
    CtlSocket ..> tetrisd : control-plane HTTTP

    tetrisd ..> StatusSnapshot : UC-22
    tetrisd ..> Db : UC-23 db_close
    Db ..> Flusher : final fsync

    tetrisd *--> "0..*" Session : owns
    Session --> "1" Room : occupies a slot in
    tetrisd ..> Lobby : UC-25
    Lobby *--> "0..*" Room : owns lifetime
    Room ..> RoomSnapshot : UC-25 toSnapshot
    Room ..> Membership : UC-07a successor
    Room o--> GameRoomStatus
    Room o--> GameMode
    Membership o--> PlayerStatus

    tetrisd ..> SessionTable : UC-26
    SessionTable *--> "0..*" Session
    Session ..> PlayerSnapshot : UC-26 assembles
    PlayerSnapshot ..> tetrisctl : UC-24 kick target

    tetrisd ..> LogIpcChannel : UC-27
    LogIpcChannel ..> tetrislogd : IPC query
    LogIpcChannel ..> DroppedLogCount : UC-27 assembles

    tetrisd ..> AuditLog : every query/command logged
```

---

## Sequence Diagrams

### Layering conventions

| Participant | Layer | Process |
|---|---|---|
| `:tetrisctl` | UI / CLI | tetrisctl |
| `:CtlSocket` | Boundary | tetrisctl |
| `:CtlListenerThread` | Controller | tetrisd (control-plane thread) |
| `:tetrisd`, `:Lobby`, `:Room`, `:SessionTable`, `:Session` | Domain | tetrisd |
| `:Db`, `:Flusher` | Persistence façade | `libmacminidb` |
| `:LogIpcChannel`, `:tetrislogd` | IPC / peer daemon | `tetrislogd` |

All six use cases go over the **local-only control socket**, never the public TCP
port. UC-22/25/26/27 are read-only queries; UC-23 and UC-24 mutate runtime state.

### SD UC-22 — Query Server Status

```mermaid
sequenceDiagram
    actor A as Administrator
    participant CLI as :tetrisctl
    participant CS as :CtlSocket
    participant LT as :CtlListenerThread
    participant D as :tetrisd

    A->>CLI: status()
    activate CLI
    CLI->>CS: connect()
    activate CS

    alt control socket reachable
        CS->>LT: STATUS /admin HTTTP/1.0
        activate LT
        LT->>D: gather snapshot
        activate D
        D-->>LT: StatusSnapshot
        deactivate D
        LT->>LT: log(query, "admin")
        LT-->>CS: 200 OK + body
        deactivate LT
        CS-->>CLI: StatusSnapshot
        CLI-->>A: prints snapshot, exits
    else socket missing/unreachable  (ext 2a)
        CS-->>CLI: connect failed
        CLI-->>A: "daemon not running / cannot reach control plane", exit non-zero
    end

    deactivate CS
    deactivate CLI
```

### SD UC-23 — Graceful Shutdown

```mermaid
sequenceDiagram
    actor A as Administrator
    participant CLI as :tetrisctl
    participant CS as :CtlSocket
    participant LT as :CtlListenerThread
    participant D as :tetrisd
    participant LOGD as :tetrislogd
    participant DB as :Db
    participant F as :Flusher

    A->>CLI: shutdown()
    activate CLI
    CLI->>CS: connect() + send(SHUTDOWN /admin)
    activate CS
    CS->>LT: SHUTDOWN /admin HTTTP/1.0
    activate LT
    note right of CLI: tetrisctl blocks until teardown completes,<br/>may take several seconds if a game is mid-play (ext 4a)

    LT->>D: initiate shutdown sequence
    activate D
    D->>D: stopAccepting() then stopTickers()

    alt game mid-play  (ext 4a)
        D->>D: terminate room, show 10s countdown
        note right of D: no db_record_game for unfinished games
    end

    D->>D: endInFlightRooms()
    D->>LOGD: flushLogsToLogd(pending records)
    activate LOGD
    LOGD-->>D: ack
    deactivate LOGD

    D->>DB: db_close()
    activate DB
    DB->>F: stop()
    F-->>DB:
    DB-->>D: closed
    deactivate DB

    D->>D: log(shutdown event)
    D->>D: freeResources() → close control socket
    D-->>LT: shutdown complete
    deactivate D
    LT-->>CS: 200 OK
    deactivate LT
    CS-->>CLI: 200 OK
    deactivate CS
    CLI-->>A: prints "server shut down", exits
    deactivate CLI
```

### SD UC-24 — Kick Player (includes UC-07a)

```mermaid
sequenceDiagram
    actor A as Administrator
    participant CLI as :tetrisctl
    participant CS as :CtlSocket
    participant LT as :CtlListenerThread
    participant D as :tetrisd
    participant S as :Session
    participant R as :Room

    A->>CLI: kick(playerId)
    activate CLI
    CLI->>CS: connect() + send(KICK /admin/player/<pid>)
    activate CS
    CS->>LT: KICK /admin/player/<pid> HTTTP/1.0
    activate LT
    LT->>D: findSession(pid)
    activate D

    alt session found  (step 2)
        D-->>LT: Session
        LT->>S: close()
        activate S
        S-->>LT: closed
        deactivate S
        LT->>R: release(playerId)
        activate R
        R->>R: slot READY → LEAVING → WAITING

        opt kicked player was Owner
            note over R: «include» UC-07a Transfer Room Ownership
            R->>R: selectSuccessor()
            R->>R: successor.setRole(OWNER)
        end

        R->>R: recomputeStatus()
        R->>R: broadcast(update)
        R->>R: narrate("PLAYER kicked")
        deactivate R
        LT->>LT: log(kick event, "admin")
        LT-->>CS: 200 OK
        CS-->>CLI: 200 OK
        CLI-->>A: prints result, exits
    else player not found / already gone  (ext 2a)
        D-->>LT: absent
        deactivate D
        LT-->>CS: 404 Not Found
        CS-->>CLI: 404
        CLI-->>A: "no such connected player" — no change
    end

    deactivate LT
    deactivate CS
    deactivate CLI
```

### SD UC-25 — List Rooms

```mermaid
sequenceDiagram
    actor A as Administrator
    participant CLI as :tetrisctl
    participant CS as :CtlSocket
    participant LT as :CtlListenerThread
    participant D as :tetrisd
    participant L as :Lobby

    A->>CLI: rooms()
    activate CLI
    CLI->>CS: connect() + send(ROOMS /admin)
    activate CS
    CS->>LT: ROOMS /admin HTTTP/1.0
    activate LT
    LT->>D: acquireRoomDirLock()
    activate D
    LT->>L: listAllRooms()
    activate L

    alt rooms exist
        L-->>LT: RoomSnapshot[]
    else no open rooms  (ext 2a)
        L-->>LT: []
    end

    deactivate L
    LT->>D: releaseRoomDirLock()
    deactivate D
    LT->>LT: log(query, "admin")
    LT-->>CS: 200 OK + room list
    deactivate LT
    CS-->>CLI: room list
    deactivate CS
    CLI-->>A: prints list, exits
    deactivate CLI
```

### SD UC-26 — List Players

```mermaid
sequenceDiagram
    actor A as Administrator
    participant CLI as :tetrisctl
    participant CS as :CtlSocket
    participant LT as :CtlListenerThread
    participant D as :tetrisd
    participant ST as :SessionTable

    A->>CLI: players()
    activate CLI
    CLI->>CS: connect() + send(PLAYERS /admin)
    activate CS
    CS->>LT: PLAYERS /admin HTTTP/1.0
    activate LT
    LT->>D: acquireSessionLock()
    activate D
    LT->>ST: listConnected()
    activate ST

    alt players connected
        ST-->>LT: PlayerSnapshot[]
    else no one connected  (ext 2a)
        ST-->>LT: []
    end

    deactivate ST
    LT->>D: releaseSessionLock()
    deactivate D
    LT->>LT: log(query, "admin")
    LT-->>CS: 200 OK + player list
    deactivate LT
    CS-->>CLI: player list
    deactivate CS
    CLI-->>A: prints list, exits
    note right of CLI: list feeds the <player> argument for UC-24
    deactivate CLI
```

### SD UC-27 — Query Dropped Logs

```mermaid
sequenceDiagram
    actor A as Administrator
    participant CLI as :tetrisctl
    participant CS as :CtlSocket
    participant LT as :CtlListenerThread
    participant D as :tetrisd
    participant IPC as :LogIpcChannel
    participant LOGD as :tetrislogd

    A->>CLI: droppedLogs()
    activate CLI
    CLI->>CS: connect() + send(DROPPED-LOGS /admin)
    activate CS
    CS->>LT: DROPPED-LOGS /admin HTTTP/1.0
    activate LT
    LT->>D: getLocalDropCount()
    activate D
    D-->>LT: tetrisdLocalDropped
    deactivate D
    LT->>IPC: queryDroppedCount()
    activate IPC
    IPC->>LOGD: IPC request

    alt tetrislogd reachable  (step 3)
        activate LOGD
        LOGD-->>IPC: droppedRecords
        deactivate LOGD
        IPC-->>LT: logdDropped
        LT->>LT: assemble DroppedLogCount.total()
        LT->>LT: log(query, "admin")
        LT-->>CS: 200 OK + counts
        CS-->>CLI: counts
        CLI-->>A: prints counts, exits
    else tetrislogd unreachable  (E1)
        IPC-->>LT: no response
        LT-->>CS: 500
        CS-->>CLI: 500
        CLI-->>A: "logger unreachable"
    end

    deactivate IPC
    deactivate LT
    deactivate CS
    deactivate CLI
```

---

## Solution Class Diagram

### Method inventory by use case

| Behaviour | Owner | Signature | From |
|---|---|---|---|
| connect to control socket | `CtlSocket` | `connect() : bool`, `send(HtttpRequest) : HtttpResponse` | UC-22–UC-27 (all) |
| request a status snapshot | `tetrisctl` | `status()` | UC-22.1 |
| gather the snapshot | `CtlListenerThread` / `tetrisd` | `handleStatus() : StatusSnapshot`, `getRoomCount/getPlayerCount/getTcpListenerStatus/getLogdStatus()` | UC-22.3 |
| request a shutdown | `tetrisctl` | `shutdown()` | UC-23.1 |
| run the shutdown sequence | `tetrisd` | `stopAccepting()`, `stopTickers()`, `endInFlightRooms()`, `flushLogsToLogd()`, `freeResources()`, `exit()` | UC-23.3–7 |
| close persistence durably | `Db` / `Flusher` | `db_close()`, `stop()`, `finalFsync()` | UC-23.5 |
| request a kick | `tetrisctl` | `kick(String)` | UC-24.1 |
| locate + close a session | `tetrisd` / `Session` | `findSession(PlayerId) : Session`, `close()` | UC-24.2 |
| free the room slot | `Room` | `release(PlayerId)`, `recomputeStatus()`, `broadcast(RoomUpdate)`, `narrate(String)` | UC-24.2 |
| transfer ownership on kick | `Room` / `Membership` | `selectSuccessor() : Membership`, `setRole(PlayerStatus)` | UC-24.2 «include» UC-07a |
| request the room list | `tetrisctl` | `rooms()` | UC-25.1 |
| list all rooms | `Lobby` / `Room` | `listAllRooms() : RoomSnapshot[]`, `toSnapshot() : RoomSnapshot` | UC-25.2 |
| request the player list | `tetrisctl` | `players()` | UC-26.1 |
| list connected sessions | `SessionTable` | `listConnected() : PlayerSnapshot[]` | UC-26.2 |
| request dropped-log counts | `tetrisctl` | `droppedLogs()` | UC-27.1 |
| read the local drop count | `tetrisd` | `getLocalDropCount() : int` | UC-27.2 |
| query the logger's drop count | `LogIpcChannel` / `tetrislogd` | `queryDroppedCount() : int`, `getDroppedCount() : int` | UC-27.2–3 |
| sum the total dropped count | `DroppedLogCount` | `total() : int` | UC-27.3 |
| record the audit trail | `AuditLog` | `log(event : String, actor : String)` | UC-22–UC-27 (all) |

### Solution diagram

```mermaid
classDiagram
    direction TB

    %% ---------------- CLI / Boundary (tetrisctl) ----------------
    class tetrisctl {
        <<boundary>>
        +status() void
        +shutdown() void
        +kick(playerId : String) void
        +rooms() void
        +players() void
        +droppedLogs() void
    }
    class CtlSocket {
        <<boundary>>
        -socketPath : String
        +connect() bool
        +send(req : HtttpRequest) HtttpResponse
    }

    %% ---------------- Controller (tetrisd control-plane thread) ----------------
    class CtlListenerThread {
        <<control>>
        +handleStatus() StatusSnapshot
        +handleShutdown() void
        +handleKick(playerId : PlayerId) HtttpStatus
        +handleListRooms() RoomSnapshot[]
        +handleListPlayers() PlayerSnapshot[]
        +handleDroppedLogs() DroppedLogCount
    }

    %% ---------------- Domain (tetrisd) ----------------
    class tetrisd {
        -uptime_s : long
        -pid : int
        -sessions : Map~PlayerId, Session~
        -roomDirLock : Mutex
        -sessionTableLock : Mutex
        -localDropCount : int
        -tcpAcceptor : Acceptor
        -roomTickers : Ticker[]
        +getRoomCount() int
        +getPlayerCount() int
        +getTcpListenerStatus() String
        +getLogdStatus() String
        +findSession(playerId : PlayerId) Session
        +closeSession(session : Session) void
        +acquireRoomDirLock() void
        +releaseRoomDirLock() void
        +acquireSessionLock() void
        +releaseSessionLock() void
        +getLocalDropCount() int
        +stopAccepting() void
        +stopTickers() void
        +endInFlightRooms() void
        +flushLogsToLogd() void
        +freeResources() void
        +exit() void
    }
    class Lobby {
        -rooms : Map~String, Room~
        +listAllRooms() RoomSnapshot[]
    }
    class Room {
        -roomId : String
        -mode : GameMode
        -status : GameRoomStatus
        -numberOfPlayers : int
        -tick : long
        +release(playerId : PlayerId) void
        +selectSuccessor() Membership
        +recomputeStatus() void
        +broadcast(update : RoomUpdate) void
        +narrate(text : String) void
        +toSnapshot() RoomSnapshot
    }
    class Slot {
        -index : int
        -status : SlotStatus
        -occupant : Membership
        +setStatus(s : SlotStatus) void
        +clearData() void
    }
    class Membership {
        -role : PlayerStatus
        -playerId : PlayerId
        +isOwner() bool
        +setRole(r : PlayerStatus) void
    }
    class SessionTable {
        -sessions : Map~PlayerId, Session~
        +listConnected() PlayerSnapshot[]
    }
    class Session {
        -playerId : PlayerId
        -username : String
        -roomId : String
        -connectedAt : timestamp
        +close() void
    }

    %% ---------------- Persistence façade ----------------
    class Db {
        <<interface>>
        libmacminidb
        +db_close() void
    }
    class Flusher {
        -appendLog : File
        +stop() void
        +finalFsync() void
    }

    %% ---------------- IPC / peer daemon ----------------
    class LogIpcChannel {
        <<boundary>>
        +queryDroppedCount() int
    }
    class tetrislogd {
        <<supporting actor>>
        -droppedRecords : int
        +receivePendingRecords() void
        +getDroppedCount() int
    }

    %% ---------------- Value objects ----------------
    class StatusSnapshot {
        <<value object>>
        -uptime_s : long
        -rooms : int
        -players : int
        -tcpListener : String
        -logd : String
        -pid : int
    }
    class RoomSnapshot {
        <<value object>>
        -id : String
        -players : int
        -state : String
        -tick : long
        -mode : GameMode
        -owner : String
    }
    class PlayerSnapshot {
        <<value object>>
        -id : PlayerId
        -user : String
        -room : String
        -score : long
    }
    class DroppedLogCount {
        <<value object>>
        -logdDropped : int
        -tetrisdLocalDropped : int
        +total() int
    }

    %% ---------------- Cross-cutting ----------------
    class AuditLog {
        <<interface>>
        +log(event : String, actor : String) void
    }

    %% ---------------- Enums ----------------
    class HtttpStatus {
        <<enum>>
        OK_200
        ACCEPTED_202
        BAD_ARGUMENT_400
        NOT_FOUND_404
        INTERNAL_500
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
    class GameRoomStatus {
        <<enum>>
        WAITING
        READY
        IN_GAME
        FINISHED
    }
    class GameMode {
        <<enum>>
        SINGLE
        DOUBLE
        BATTLE_ROYALE
    }

    %% ---------------- Relationships ----------------
    tetrisctl ..> CtlSocket : uses
    CtlSocket ..> CtlListenerThread : control-plane HTTTP

    CtlListenerThread ..> tetrisd : status / lock acquire-release / drop count
    CtlListenerThread ..> Lobby : UC-25 listAllRooms
    CtlListenerThread ..> SessionTable : UC-26 listConnected
    CtlListenerThread ..> LogIpcChannel : UC-27 queryDroppedCount
    CtlListenerThread ..> AuditLog : logs every query/command
    CtlListenerThread ..> HtttpStatus : maps outcome

    tetrisd ..> StatusSnapshot : UC-22 assembles
    tetrisd ..> Db : UC-23 db_close
    Db ..> Flusher : stop / finalFsync
    tetrisd *--> "0..*" Session : owns
    tetrisd ..> Session : UC-24 findSession / closeSession
    Session --> "0..1" Room : occupies a slot in

    Lobby *--> "0..*" Room : owns lifetime
    Room ..> RoomSnapshot : toSnapshot
    Room *--> "0..*" Slot
    Slot o--> SlotStatus
    Slot o--> "0..1" Membership
    Room ..> Membership : UC-07a selectSuccessor
    Membership o--> PlayerStatus
    Room o--> GameRoomStatus
    Room o--> GameMode

    SessionTable *--> "0..*" Session : owns
    Session ..> PlayerSnapshot : assembles

    LogIpcChannel ..> tetrislogd : IPC request/response
    LogIpcChannel ..> DroppedLogCount : assembles total
```
