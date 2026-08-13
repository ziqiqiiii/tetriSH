# tetriSH — Complete Solution Class Diagram (UC-01 – UC-27)

## Table of Contents

- [Method Inventory by Use Case](#method-inventory-by-use-case)
  - [UC-01 – UC-09 — account, lobby, rooms, chat](#uc-01--uc-09--account-lobby-rooms-chat)
  - [UC-10 – UC-14 — gameplay, garbage, input, abilities](#uc-10--uc-14--gameplay-garbage-input-abilities)
  - [UC-15 – UC-19 — marketplace, purchase, equip](#uc-15--uc-19--marketplace-purchase-equip)
  - [UC-20 – UC-21 — profile and leaderboard](#uc-20--uc-21--profile-and-leaderboard)
  - [UC-22 – UC-27 — control plane](#uc-22--uc-27--control-plane)
- [Complete Solution Diagram](#complete-solution-diagram)
  - [Diagram](#diagram)

---
### UC-01 – UC-09 — account, lobby, rooms, chat

| Behaviour | Owner | Signature | From |
|---|---|---|---|
| validate password match | `SignUpUI` | `validateMatch(String, String) : bool` | UC-01.5 |
| hash with fresh salt | `Credential` | `create(String) : Credential` | UC-01.5 |
| create the account | `AuthController` | `signup(String, String) : HtttpStatus` | UC-01.6 |
| authenticate | `AuthController` | `login(String, String) : LoginResult` | UC-02.6 |
| fetch the account salt | `Db` | `db_get_salt(String, char*, size_t) : DbResult` | UC-02.6 |
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
| owner-only start check | `Room` | `canStart(PlayerId) : StartVerdict` | UC-08.4 / UC-08a.3 |
| begin the match | `Room` | `start() : GameSession` | UC-08.5 |
| initial board snapshot | `GameSession` | `initialState() : StateFrame` | UC-08.5 |
| abort a failed start | `GameSession` | `abort()` | UC-08 E1 |
| start a room | `RoomController` | `start(PlayerId, String) : HtttpStatus` | UC-08.3 |
| log a rejected start | `AuditLog` | `warn(String, PlayerId)` | UC-08a.5 |
| throttle a sender | `RateLimiter` | `tryConsume(PlayerId) : bool` | UC-09.4 |
| moderation check | `Membership` | `isMuted() : bool` | UC-09 ext 4b |
| fan out a message | `Room` | `broadcast(ChatMessage)` | UC-09.4 |
| post a chat message | `ChatController` | `post(PlayerId, String, String) : HtttpStatus` | UC-09.3 |

### UC-10 – UC-14 — gameplay, garbage, input, abilities

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

### UC-15 – UC-19 — marketplace, purchase, equip

| Behaviour | Owner | Signature | From |
|---|---|---|---|
| switch store tab | `MarketplaceUI` | `selectTab(StoreTab)` | UC-15.1 / UC-16.1 |
| select an item | `MarketplaceUI` | `selectCharacter(ItemId)`, `selectTheme(ItemId)` | UC-15.1 / UC-16.1 |
| show preview + abilities | `MarketplaceUI` | `showPreview(Character)`, `showPreview(Theme)` | UC-15.2 / UC-16.2 |
| resolve level → ability | `Character` | `grants(int) : Ability` | UC-15.2 |
| expose a theme's palette | `Theme` | `palette() : ColorScheme` | UC-16.2 |
| derive the button pair | `ButtonState` | `ownedState()`, `unownedState()`, `bothDisabled()` | UC-15a.3–4 / UC-16a.3–4 |
| apply the button pair | `MarketplaceUI` | `applyButtonState(ButtonState)` | UC-15a.3 / UC-16a.3 |
| ownership test | `Db` | `db_player_owns_character(id, cid)`, `db_player_owns_theme(id, tid)` | UC-15a.2 / UC-16a.2 |
| buy a character | `StoreController` | `buyCharacter(PlayerId, ItemId) : HtttpStatus` | UC-15.3–4 |
| buy a theme | `StoreController` | `buyTheme(PlayerId, ItemId) : HtttpStatus` | UC-16.3–4 |
| persist a purchase | `Db` | `db_buy_character(id, cid)`, `db_buy_theme(id, tid)` | UC-15.4 / UC-16.4 |
| read the catalogue | `Catalogue` | `findCharacter(ItemId)`, `findTheme(ItemId)` | UC-15.2 / UC-16.2 |
| run the atomic purchase | `PurchaseTxn` | `run() : DbResult`, `precheck() : DbResult`, `settle()` | UC-17.1–4 |
| hold the critical section | `WriteLock` | `acquire()`, `release()` | UC-17.1 / UC-17.4 |
| affordability + debit | `Wallet` | `canAfford(long) : bool`, `debit(long)` | UC-17.2–3 |
| ownership + cap + grant | `Inventory` | `owns(ItemId) : bool`, `isFull() : bool`, `add(ItemId)` | UC-17.2–3 |
| one whole-record write | `PlayerRecord` | `debit(long)`, `grant(ItemId)` | UC-17.3 |
| durable append (LWW) | `WalLog` | `append(PlayerRecord) : DbResult`, `flush()` | UC-17.4 |
| equip a character | `EquipController` | `equipCharacter(PlayerId, ItemId) : HtttpStatus` | UC-18.2–3 |
| equip a theme | `EquipController` | `equipTheme(PlayerId, ItemId) : HtttpStatus` | UC-19.2–3 |
| persist the equip | `Db` | `db_equip_character(id, cid)`, `db_equip_theme(id, tid)` | UC-18.3 / UC-19.3 |
| hold what is equipped | `Loadout` | `setCharacter(ItemId)`, `setTheme(ItemId)`, `character()`, `theme()` | UC-18.3 / UC-19.3 |
| reflect in Settings | `SettingsUI` | `showDefaultCharacter(Character)`, `showProfilePicture(Character)`, `showCurrentTheme(Theme)` | UC-18.4 / UC-19.4 |
| adopt the palette in-game | `GameplayUI` | `applyTheme(ColorScheme)` | UC-19.4 |

### UC-20 – UC-21 — profile and leaderboard

| Behaviour | Owner | Signature | From |
|---|---|---|---|
| open the settings screen | `SettingsUI` | `openSettings()` | UC-20.1 |
| render the whole profile | `SettingsUI` | `render(ProfileView)` | UC-20.3 |
| leave for an equip change | `SettingsUI` | `pressChangeDefaultCharacter(ItemId)`, `pressChangeDefaultTheme(ItemId)` | UC-20 ext 3a / 3b |
| serve the profile | `ProfileController` | `profile(PlayerId) : HtttpStatus` | UC-20.2–3 |
| build the read payload | `ProfileController` | `assemble(PlayerRecord, int) : ProfileView` | UC-20.3 |
| read the player document | `Db` | `db_get_player(id, out) : DbResult` | UC-20.2 |
| read the Player's own rank | `Db` | `db_rank(id, out_rank) : DbResult` | UC-20.2 |
| resolve owned ids to items | `Catalogue` | `findCharacter(ItemId)`, `findTheme(ItemId)` | UC-20.3 |
| expose the equipped ids | `Loadout` | `character() : ItemId`, `theme() : ItemId` | UC-20.3 |
| list an owned set | `Inventory` | `ids() : ItemId[]` | UC-20.3 |
| open the leaderboard screen | `LeaderboardUI` | `openLeaderboard()` | UC-21.1 |
| render the two bands | `LeaderboardUI` | `renderPodium(RankEntry[])`, `renderList(RankEntry[])` | UC-21.3 |
| serve the top-N | `LeaderboardController` | `leaderboard(int) : HtttpStatus` | UC-21.2 |
| read the top-N | `Db` | `db_leaderboard(out, cap, out_count) : DbResult` | UC-21.2 |
| walk the ordered index | `SkipList` | `topN(int) : RankEntry[]`, `position(PlayerId) : int` | UC-21.2 / UC-20.2 |
| split podium from list | `Leaderboard` | `top(int)`, `podium()`, `remainder()` | UC-21.3 |
| hold the shared read | `ReadLock` | `acquire()`, `release()` | UC-20.2 / UC-21.2 |

### UC-22 – UC-27 — control plane

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

---

## Complete Solution Diagram

### Diagram

```mermaid
classDiagram
    direction TB

    %% ================= UI layer (tetrisu) =================
    class SignUpUI {
        <<boundary>>
        -username : String
        -password : String
        -confirm : String
        +validateMatch(String, String) bool
        +show(msg : String) void
    }
    class LoginUI {
        <<boundary>>
        -username : String
        -password : String
        -serverId : String
        +show(msg : String) void
    }
    class LobbyUI {
        <<boundary>>
        -playerId : PlayerId
        -highlightedRoomId : String
        -typedRoomId : String
        +show(msg : String) void
    }
    class WaitingRoomUI {
        <<boundary>>
        -playerId : PlayerId
        -roomId : String
        +show(msg : String) void
        +switchToGameplay(view : GameView) void
    }
    class ChatUI {
        <<boundary>>
        -roomId : String
        -draft : String
        +openComposer() void
        +append(m : ChatMessage) void
        +show(msg : String) void
    }
    class GameplayUI {
        <<boundary>>
        -playerId : PlayerId
        -charge : int
        +render(view : BoardView) void
        +renderSplit(own : BoardView, opponent : BoardView) void
        +renderArena(own : BoardView, others : BoardView[]) void
        +onKey(k : Key) InputCommand
        +onAbilityKey(level : int) void
        +applyTheme(p : ColorScheme) void
        +showElimination(rank : int) void
        +showFinalScore(s : ScoreCard) void
        +show(msg : String) void
    }
    class MarketplaceUI {
        <<boundary>>
        -playerId : PlayerId
        -tab : StoreTab
        -selected : ItemId
        +selectTab(t : StoreTab) void
        +selectCharacter(cid : ItemId) void
        +selectTheme(tid : ItemId) void
        +showPreview(c : Character) void
        +showPreview(t : Theme) void
        +applyButtonState(s : ButtonState) void
        +pressBuy() void
        +pressSetAsDefaultCharacter(cid : ItemId) void
        +pressSetAsDefaultTheme(tid : ItemId) void
        +show(msg : String) void
    }
    class SettingsUI {
        <<boundary>>
        -playerId : PlayerId
        +openSettings() void
        +render(v : ProfileView) void
        +pressChangeDefaultCharacter(cid : ItemId) void
        +pressChangeDefaultTheme(tid : ItemId) void
        +showDefaultCharacter(c : Character) void
        +showProfilePicture(c : Character) void
        +showCurrentTheme(t : Theme) void
        +show(msg : String) void
    }
    class LeaderboardUI {
        <<boundary>>
        +openLeaderboard() void
        +renderPodium(top3 : RankEntry[]) void
        +renderList(rest : RankEntry[]) void
        +show(msg : String) void
    }

    %% ================= Client boundary =================
    class HtttpClient {
        <<boundary>>
        -session : Session
        +signup(String, String) HtttpResponse
        +login(String, String, String) HtttpResponse
        +browseRooms(PlayerId) HtttpResponse
        +createRoom(PlayerId, GameMode) HtttpResponse
        +joinRoom(PlayerId, String) HtttpResponse
        +leaveRoom(PlayerId, String) HtttpResponse
        +startGame(PlayerId, String) HtttpResponse
        +sendChat(PlayerId, String, String) HtttpResponse
        +sendInput(i : InputCommand) HtttpResponse
        +useAbility(PlayerId, String, int) HtttpResponse
        +leaveGame(PlayerId) HtttpResponse
        +buyCharacter(PlayerId, ItemId) HtttpResponse
        +buyTheme(PlayerId, ItemId) HtttpResponse
        +ownsCharacter(PlayerId, ItemId) HtttpResponse
        +ownsTheme(PlayerId, ItemId) HtttpResponse
        +equipCharacter(PlayerId, ItemId) HtttpResponse
        +equipTheme(PlayerId, ItemId) HtttpResponse
        +profile(PlayerId) HtttpResponse
        +leaderboard(cap : int) HtttpResponse
        +onState(frame : StateFrame) void
        +onChat(m : ChatMessage) void
    }

    %% ================= Admin CLI (tetrisctl) =================
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

    %% ================= Controllers (tetrisd, stateless) =================
    class AuthController {
        <<control>>
        +signup(username : String, password : String) HtttpStatus
        +login(username : String, password : String) LoginResult
    }
    class LobbyController {
        <<control>>
        +listRooms(playerId : PlayerId) LobbyView
        +header(playerId : PlayerId) HeaderDto
    }
    class RoomController {
        <<control>>
        +create(playerId : PlayerId, mode : GameMode) RoomView
        +join(playerId : PlayerId, roomId : String) RoomView
        +leave(playerId : PlayerId, roomId : String) HtttpStatus
        +start(playerId : PlayerId, roomId : String) HtttpStatus
    }
    class ChatController {
        <<control>>
        +post(playerId : PlayerId, roomId : String, text : String) HtttpStatus
    }
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
    class StoreController {
        <<control>>
        +buyCharacter(playerId : PlayerId, cid : ItemId) HtttpStatus
        +buyTheme(playerId : PlayerId, tid : ItemId) HtttpStatus
        +ownsCharacter(playerId : PlayerId, cid : ItemId) DbBool
        +ownsTheme(playerId : PlayerId, tid : ItemId) DbBool
    }
    class EquipController {
        <<control>>
        +equipCharacter(playerId : PlayerId, cid : ItemId) HtttpStatus
        +equipTheme(playerId : PlayerId, tid : ItemId) HtttpStatus
    }
    class ProfileController {
        <<control>>
        +profile(playerId : PlayerId) HtttpStatus
        -assemble(p : PlayerRecord, rank : int) ProfileView
    }
    class LeaderboardController {
        <<control>>
        +leaderboard(cap : int) HtttpStatus
    }
    class CtlListenerThread {
        <<control>>
        +handleStatus() StatusSnapshot
        +handleShutdown() void
        +handleKick(playerId : PlayerId) HtttpStatus
        +handleListRooms() RoomSnapshot[]
        +handleListPlayers() PlayerSnapshot[]
        +handleDroppedLogs() DroppedLogCount
    }

    %% ================= Daemon process =================
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

    %% ================= Domain — identity & session =================
    class Player {
        -playerId : PlayerId
        -username : String
        -credential : Credential
        -leaderboardScore : long
        -walletPoints : long
        -gamesPlayed : int
        -gamesWon : int
        +getUsername() String
        +getLeaderboardScore() long
    }
    class Credential {
        -passwordHashed : String
        -salt : String
        +create(password : String)$ Credential
        +verify(password : String) bool
        +getSalt() String
    }
    class Session {
        -sessionId : String
        -playerId : PlayerId
        -username : String
        -roomId : String
        -connectedAt : timestamp
        -endpoint : ServerEndpoint
        +connect(endpoint : ServerEndpoint) bool
        +bind(playerId : PlayerId) void
        +isAuthenticated() bool
        +close() void
    }
    class ServerEndpoint {
        -serverId : String
        -host : String
        -port : int
        +resolve(serverId : String)$ ServerEndpoint
    }
    class SessionTable {
        -sessions : Map~PlayerId, Session~
        +listConnected() PlayerSnapshot[]
    }
    class RateLimiter {
        -tokens : double
        -capacity : double
        -refillPerSec : double
        +tryConsume(playerId : PlayerId) bool
    }

    %% ================= Domain — lobby & rooms =================
    class Lobby {
        -rooms : Map~String, Room~
        +listOpenRooms() RoomSummary[]
        +listAllRooms() RoomSnapshot[]
        +createRoom(owner : PlayerId, mode : GameMode) Room
        +findRoom(roomId : String) Room
        +destroyRoom(roomId : String) void
    }
    class Room {
        -roomId : String
        -mode : GameMode
        -status : GameRoomStatus
        -slotCount : int
        -minToStart : int
        -numberOfPlayers : int
        -tick : long
        +stateMessage() String
        +canAccept() JoinVerdict
        +seat(playerId : PlayerId) Slot
        +release(playerId : PlayerId) void
        +selectSuccessor() Membership
        +recomputeStatus() void
        +narrate(text : String) void
        +broadcast(m : ChatMessage) void
        +broadcast(update : RoomUpdate) void
        +toSummary() RoomSummary
        +toSnapshot() RoomSnapshot
        +isOwner(playerId : PlayerId) bool
        +canStart(requester : PlayerId) StartVerdict
        +start() GameSession
        +members() Membership[]
    }
    class Slot {
        -index : int
        -status : SlotStatus
        -occupant : Membership
        +setStatus(s : SlotStatus) void
        +occupy(m : Membership) void
        +clearData() void
        +isFree() bool
        +isReady() bool
    }
    class Membership {
        -role : PlayerStatus
        -playerId : PlayerId
        -roomId : String
        -muted : bool
        +setRole(r : PlayerStatus) void
        +isOwner() bool
        +isMuted() bool
    }

    %% ================= Domain — match & brain =================
    class GameSession {
        -roomId : String
        -mode : GameMode
        -startedAt : timestamp
        -tick : long
        -boards : Map~PlayerId, Board~
        -alive : PlayerId[]
        +initialState() StateFrame
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
    class Ranking {
        -places : Map~PlayerId, int~
        +record(playerId : PlayerId, rank : int) void
        +lastStanding() PlayerId
    }
    class AbilityEffect {
        <<interface>>
        +apply(caster : Board, target : Board) void
    }

    %% ================= Domain — store internals (libmacminidb) =================
    class PlayerRecord {
        -playerId : PlayerId
        -username : String
        -walletPoints : long
        -leaderboardScore : long
        -gamesPlayed : int
        -gamesWon : int
        +debit(cost : long) void
        +grant(itemId : ItemId) void
    }
    class Wallet {
        -points : long
        +canAfford(cost : long) bool
        +debit(cost : long) void
        +balance() long
    }
    class Inventory {
        -items : ItemId[]
        -count : int
        -cap : int
        +owns(itemId : ItemId) bool
        +isFull() bool
        +add(itemId : ItemId) void
        +ids() ItemId[]
    }
    class Loadout {
        -equippedCharacter : ItemId
        -equippedTheme : ItemId
        +setCharacter(cid : ItemId) void
        +setTheme(tid : ItemId) void
        +character() ItemId
        +theme() ItemId
    }
    class PurchaseTxn {
        <<control>>
        -playerId : PlayerId
        -itemId : ItemId
        -cost : long
        +run() DbResult
        -precheck() DbResult
        -settle() void
    }
    class WriteLock {
        +acquire() void
        +release() void
    }
    class ReadLock {
        +acquire() void
        +release() void
    }
    class WalLog {
        -path : String
        +append(rec : PlayerRecord) DbResult
        +flush() void
        +replay() void
    }
    class Flusher {
        -appendLog : File
        +stop() void
        +finalFsync() void
    }
    class SkipList {
        ordered by score then id
        +topN(cap : int) RankEntry[]
        +position(id : PlayerId) int
    }
    class Leaderboard {
        -entries : RankEntry[]
        -count : int
        +top(cap : int) RankEntry[]
        +podium() RankEntry[]
        +remainder() RankEntry[]
    }
    class Catalogue {
        -characters : Character[]
        -themes : Theme[]
        +findCharacter(cid : ItemId) Character
        +findTheme(tid : ItemId) Theme
    }
    class StoreItem {
        <<abstract>>
        -itemId : ItemId
        -name : String
        -costPoints : long
        +price() long
    }
    class Character {
        -characterId : ItemId
        -name : String
        -abilities : bitfield
        +grants(level : int) Ability
    }
    class Theme {
        -themeId : ItemId
        -description : String
        +palette() ColorScheme
    }

    %% ================= Value objects =================
    class RoomSummary {
        <<value object>>
        -roomId : String
        -mode : GameMode
        -occupancy : String
        -status : GameRoomStatus
        -ownerName : String
    }
    class ChatMessage {
        <<value object>>
        -senderId : PlayerId
        -senderName : String
        -text : String
        -at : timestamp
        +isSystem() bool
    }
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
    class Ability {
        <<value object>>
        -level : int
        -cost : int
        -name : String
        +costFor(level : int)$ int
    }
    class ButtonState {
        <<value object>>
        -buyEnabled : bool
        -setDefaultEnabled : bool
        +ownedState()$ ButtonState
        +unownedState()$ ButtonState
        +bothDisabled()$ ButtonState
    }
    class ColorScheme {
        <<value object>>
        -cellColors : Color[7]
        -accent : Color
    }
    class ProfileView {
        <<value object>>
        -username : String
        -defaultCharacter : Character
        -ownedCharacters : Character[]
        -currentTheme : Theme
        -ownedThemes : Theme[]
        -walletPoints : long
        -leaderboardScore : long
        -rank : int
    }
    class RankEntry {
        <<value object>>
        -playerId : PlayerId
        -username : String
        -leaderboardScore : long
        -rank : int
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

    %% ================= Persistence & logging façades =================
    class Db {
        <<interface>>
        libmacminidb
        +db_signup(username, hash, salt, out) DbResult
        +db_get_salt(username, out_salt, cap) DbResult
        +db_login(username, hash, out) DbResult
        +db_get_player(id, out) DbResult
        +db_record_game(id, scoreDelta, pointsDelta, won) DbResult
        +db_buy_character(id, cid) DbResult
        +db_buy_theme(id, tid) DbResult
        +db_equip_character(id, cid) DbResult
        +db_equip_theme(id, tid) DbResult
        +db_player_owns_character(id, cid) DbBool
        +db_player_owns_theme(id, tid) DbBool
        +db_get_character(cid, out) DbResult
        +db_get_theme(tid, out) DbResult
        +db_leaderboard(out, cap, out_count) DbResult
        +db_rank(id, out_rank) DbResult
        +db_close() void
    }
    class AuditLog {
        <<interface>>
        tetrislogd via libcoreipc
        +warn(event : String, playerId : PlayerId) void
        +log(event : String, actor : String) void
    }
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

    %% ================= Enums =================
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
    class StartVerdict {
        <<enum>>
        ACCEPTED
        NOT_OWNER
        TOO_FEW_PLAYERS
        ALREADY_STARTED
    }
    class ChatVerdict {
        <<enum>>
        DELIVERED
        RATE_LIMITED
        MUTED
        NOT_FOUND
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
    class StoreTab {
        <<enum>>
        CHARACTERS
        THEMES
    }
    class DbResult {
        <<enum>>
        DB_OK
        DB_EXISTS
        DB_BAD_CREDS
        DB_INSUFFICIENT
        DB_NOT_OWNED
        DB_FULL
        DB_NOT_FOUND
        DB_INVALID
        DB_IO_ERROR
    }
    class DbBool {
        <<enum>>
        DB_TRUE
        DB_FALSE
        DB_UNKNOWN
    }
    class HtttpStatus {
        <<enum>>
        OK_200
        ACCEPTED_202
        BAD_ARGUMENT_400
        FORBIDDEN_403
        NOT_FOUND_404
        CONFLICT_409
        PAYLOAD_TOO_LARGE_413
        INTERNAL_500
    }

    %% ================= Relationships — UI to boundary =================
    SignUpUI ..> HtttpClient : uses
    LoginUI ..> HtttpClient : uses
    LobbyUI ..> HtttpClient : uses
    WaitingRoomUI ..> HtttpClient : uses
    ChatUI ..> HtttpClient : uses
    GameplayUI ..> HtttpClient : uses
    MarketplaceUI ..> HtttpClient : uses
    SettingsUI ..> HtttpClient : uses
    LeaderboardUI ..> HtttpClient : uses

    GameplayUI ..> InputCommand : onKey builds
    GameplayUI ..> ColorScheme : applied at game start
    MarketplaceUI o--> StoreTab
    MarketplaceUI ..> ButtonState : UC-15a / UC-16a
    MarketplaceUI ..> Character : preview
    MarketplaceUI ..> Theme : preview
    SettingsUI ..> ProfileView : renders
    SettingsUI ..> Character : default + pfp
    SettingsUI ..> Theme : current theme
    LeaderboardUI ..> Leaderboard : podium + list

    HtttpClient *--> Session : owns
    Session o--> ServerEndpoint
    HtttpClient ..> StateFrame : receives push
    HtttpClient ..> ChatMessage : receives push

    %% ================= Relationships — boundary to controllers =================
    HtttpClient ..> AuthController : HTTTP SIGNUP / LOGIN
    HtttpClient ..> LobbyController : HTTTP LIST
    HtttpClient ..> RoomController : HTTTP CREATE / JOIN / LEAVE / START
    HtttpClient ..> ChatController : HTTTP CHAT
    HtttpClient ..> GameController : HTTTP MOVE / ROTATE / DROP
    HtttpClient ..> AbilityController : HTTTP ABILITY
    HtttpClient ..> StoreController : HTTTP BUY /store/...
    HtttpClient ..> EquipController : HTTTP EQUIP /player/...
    HtttpClient ..> ProfileController : HTTTP PROFILE /player/<pid>
    HtttpClient ..> LeaderboardController : HTTTP LEADERBOARD /leaderboard

    tetrisctl ..> CtlSocket : uses
    CtlSocket ..> CtlListenerThread : control-plane HTTTP

    %% ================= Relationships — controllers to domain =================
    AuthController ..> Credential : creates
    AuthController ..> Db : signup / login
    AuthController ..> Session : bind

    LobbyController ..> Lobby : listOpenRooms
    LobbyController ..> Db : player + rank
    LobbyController ..> RoomSummary : returns

    RoomController ..> Lobby : create / find / destroy
    RoomController ..> Room : seat / release / narrate / start
    RoomController ..> AuditLog : warn on rejected start

    ChatController ..> Lobby : findRoom
    ChatController ..> RateLimiter : tryConsume
    ChatController ..> Room : broadcast
    ChatController ..> ChatVerdict : outcome

    GameController ..> GameSession : applyInput / tick
    GameController ..> MatchResult : finish
    GameController ..> Db : db_record_game on game-over

    AbilityController ..> Db : owns_character / get_character
    AbilityController ..> ChargeMeter : canAfford / deduct
    AbilityController ..> AbilityEffect : apply
    AbilityController ..> GameSession : caster + target boards
    AbilityController ..> Room : narrate ABILITY_USED
    AbilityController ..> AbilityVerdict : outcome

    StoreController ..> Db : db_buy_* / db_player_owns_*
    StoreController ..> HtttpStatus : maps DbResult
    EquipController ..> Db : db_equip_*
    EquipController ..> HtttpStatus : maps DbResult
    ProfileController ..> Db : db_get_player + db_rank + db_get_*
    ProfileController ..> ProfileView : assembles
    ProfileController ..> HtttpStatus : maps DbResult
    LeaderboardController ..> Db : db_leaderboard
    LeaderboardController ..> Leaderboard : positional ranks
    LeaderboardController ..> HtttpStatus : maps DbResult

    CtlListenerThread ..> tetrisd : status / locks / drop count
    CtlListenerThread ..> Lobby : UC-25 listAllRooms
    CtlListenerThread ..> SessionTable : UC-26 listConnected
    CtlListenerThread ..> LogIpcChannel : UC-27 queryDroppedCount
    CtlListenerThread ..> AuditLog : logs every query/command
    CtlListenerThread ..> HtttpStatus : maps outcome

    %% ================= Relationships — lobby & rooms =================
    Lobby *--> "0..*" Room : owns
    Room *--> "1..99" Slot
    Room *--> "0..1" GameSession : live match
    Room *--> "0..*" ChatMessage : room feed
    Room o--> GameMode
    Room o--> GameRoomStatus
    Room ..> RoomSummary : produces
    Room ..> RoomSnapshot : toSnapshot
    Room ..> StartVerdict : canStart
    Room ..> Membership : UC-07a selectSuccessor
    Slot o--> SlotStatus
    Slot o--> "0..1" Membership
    Membership o--> PlayerStatus
    Membership ..> Player : refers to
    ChatMessage o--> "1" Player : authored by

    %% ================= Relationships — sessions =================
    tetrisd *--> "0..*" Session : owns
    tetrisd ..> Session : UC-24 findSession / closeSession
    tetrisd ..> StatusSnapshot : UC-22 assembles
    tetrisd ..> Db : UC-23 db_close
    SessionTable *--> "0..*" Session : owns
    Session --> "0..1" Room : occupies a slot in
    Session --> "0..1" RateLimiter : throttled by
    Session ..> PlayerSnapshot : assembles

    %% ================= Relationships — match & brain =================
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

    %% ================= Relationships — persistence =================
    Db ..> PurchaseTxn : delegates UC-17
    Db ..> PlayerRecord : point lookup by id
    Db ..> Loadout : equip writes
    Db ..> Catalogue : item lookup
    Db ..> SkipList : top-N + own position
    Db ..> ReadLock : shared read only
    Db ..> Flusher : stop / finalFsync
    Db ..> Player : materialises
    Db ..> DbResult
    Db ..> DbBool : probe answers (owns / unknown)

    PurchaseTxn ..> WriteLock : one atom
    PurchaseTxn ..> Wallet : canAfford / debit
    PurchaseTxn ..> Inventory : owns / isFull / add
    PurchaseTxn ..> PlayerRecord : settle
    PurchaseTxn ..> WalLog : append whole record
    PurchaseTxn ..> DbResult : outcome

    Player *--> Credential
    PlayerRecord *--> "1" Wallet
    PlayerRecord *--> "2" Inventory : characters + themes
    PlayerRecord *--> "1" Loadout
    WalLog ..> PlayerRecord : LWW replay on boot
    ProfileView ..> PlayerRecord : projection
    Leaderboard o--> "1..10" RankEntry
    SkipList ..> RankEntry : ordered rows

    StoreItem <|-- Character
    StoreItem <|-- Theme
    Catalogue o--> "0..*" Character : config/*.cfg
    Catalogue o--> "0..*" Theme
    Inventory o--> "0..64" StoreItem : capped at DB_MAX_OWNED
    Loadout --> "1" Character : equipped
    Loadout --> "1" Theme : equipped
    Theme ..> ColorScheme : palette
    ButtonState ..> Inventory : derived from owns()

    %% ================= Relationships — logging =================
    LogIpcChannel ..> tetrislogd : IPC request/response
    LogIpcChannel ..> DroppedLogCount : assembles total
```
