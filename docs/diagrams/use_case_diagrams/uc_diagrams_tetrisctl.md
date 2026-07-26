# tetriSH — Use Case Diagrams: Administration (`tetrisctl`)

Individual use case diagrams for each `tetrisctl` control-plane operation. Each diagram shows the actors, system boundary, and the specific use case with its relationships.

---

## Table of Contents

- [UC-22 — Query Server Status](#uc-22--query-server-status)
- [UC-23 — Graceful Shutdown](#uc-23--graceful-shutdown)
- [UC-24 — Kick Player](#uc-24--kick-player)
- [UC-25 — List Rooms](#uc-25--list-rooms)
- [UC-26 — List Players](#uc-26--list-players)
- [UC-27 — Query Dropped Logs](#uc-27--query-dropped-logs)

---

## UC-22 — Query Server Status

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
    CtlSocket ..> CtlListenerThread : STATUS /admin HTTTP/1.0
    CtlListenerThread ..> tetrisd : 3. gathers snapshot
    tetrisd ..> StatusSnapshot : 3. assembles
    CtlListenerThread ..> AuditLog : 4. logs the query
    CtlListenerThread ..> HtttpStatus : returns 200 OK
    CtlListenerThread ..> CtlSocket : 3. 200 OK + body
    CtlSocket ..> tetrisctl : 4. prints snapshot, exits
```

**Actors & roles**

| Actor | Role |
|---|---|
| Administrator | Primary — runs `tetrisctl status` |
| tetrisd | Supporting — provides runtime snapshot |

**Wire exchange**

```
STATUS /admin HTTTP/1.0        →  control socket
HTTTP/1.0 200 OK               ←  {"uptime_s":…,"rooms":…,"players":…,"tcp_listener":"up","logd":"connected","pid":…}
```

---

## UC-23 — Graceful Shutdown

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
    CtlSocket ..> CtlListenerThread : SHUTDOWN /admin HTTTP/1.0
    CtlListenerThread ..> tetrisd : 3. initiates shutdown sequence
    CtlListenerThread ..> CtlSocket : 2. 202 Accepted
    CtlSocket ..> tetrisctl : 2. prints confirmation, exits
    tetrisd ..> tetrisd : 3. stopAccepting → stopTickers
    tetrisd ..> tetrisd : 4. endInFlightRooms (no db_record_game)
    tetrisd ..> tetrislogd : 4. flushLogsToLogd (pending records)
    tetrisd ..> Db : 5. db_close
    Db ..> Flusher : 5. stop → finalFsync
    tetrisd ..> AuditLog : logs shutdown event
    tetrisd ..> tetrisd : 6. freeResources → close socket → exit
    CtlListenerThread ..> HtttpStatus : returns 202 Accepted
```

**Actors & roles**

| Actor | Role |
|---|---|
| Administrator | Primary — runs `tetrisctl shutdown` |
| tetrisd | Supporting — executes the graceful shutdown sequence |
| tetrislogd | Supporting — receives flushed log records before exit |
| DB (libmacminidb) | Supporting — performs final fsync via `db_close` |

**Wire exchange**

```
SHUTDOWN /admin HTTTP/1.0      →  control socket
HTTTP/1.0 202 Accepted         ←  {"shutting_down":true}
                                   (daemon then exits)
```

---

## UC-24 — Kick Player

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
```

**Actors & roles**

| Actor | Role |
|---|---|
| Administrator | Primary — runs `tetrisctl kick <player>` |
| tetrisd | Supporting — closes session, updates room |

**Relationships**

| Relationship | Target |
|---|---|
| `«include»` | UC-07a Transfer Room Ownership (if kicked player is Owner) |

**Wire exchange**

```
KICK /admin/player/p17 HTTTP/1.0  →  control socket
{"reason":"admin"}
HTTTP/1.0 200 OK                   ←  (empty body)
```

---

## UC-25 — List Rooms

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
```

**Actors & roles**

| Actor | Role |
|---|---|
| Administrator | Primary — runs `tetrisctl rooms` |
| tetrisd | Supporting — provides the in-memory room directory (same data as UC-03 but via control plane) |

**Wire exchange**

```
GET /admin/rooms HTTTP/1.0     →  control socket
HTTTP/1.0 200 OK               ←  {"rooms":[{"id":"main","players":4,"state":"RUNNING","tick":48124},…]}
```

---

## UC-26 — List Players

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
```

**Actors & roles**

| Actor | Role |
|---|---|
| Administrator | Primary — runs `tetrisctl players` |
| tetrisd | Supporting — provides the connection/session table |

**Relationships**

| Relationship | Target |
|---|---|
| provides targets for | UC-24 Kick Player (the `<player>` argument) |

**Wire exchange**

```
GET /admin/players HTTTP/1.0   →  control socket
HTTTP/1.0 200 OK               ←  {"players":[{"id":"p17","user":"alice","room":"main","score":9100},…]}
```

---

## UC-27 — Query Dropped Logs

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
    LogIpcChannel ..> tetrislogd : IPC query
    tetrislogd ..> LogIpcChannel : returns droppedRecords
    LogIpcChannel ..> CtlListenerThread : logd count
    CtlListenerThread ..> DroppedLogCount : 3. assembles total
    CtlListenerThread ..> AuditLog : 4. logs the query
    CtlListenerThread ..> HtttpStatus : returns 200 OK
    CtlListenerThread ..> CtlSocket : 3. 200 OK + counts
    CtlSocket ..> tetrisctl : 4. prints counts, exits
```

**Actors & roles**

| Actor | Role |
|---|---|
| Administrator | Primary — runs `tetrisctl dropped-logs` |
| tetrisd | Supporting — provides its own local drop count |
| tetrislogd | Secondary — provides the logger-side dropped-records counter over IPC |

**Wire exchange**

```
GET /admin/logs/dropped HTTTP/1.0  →  control socket
HTTTP/1.0 200 OK                    ←  {"logd_dropped":152,"tetrisd_local_dropped":8}
```

**Exceptions**

| Condition | Response |
|---|---|
| `tetrislogd` unreachable | `500 Internal` — operator is told the logger is down |
