# tetriSH — Profile, Settings & Leaderboard (UC-20 – UC-21)

## Table of Contents

- [Per-Use-Case Class Diagrams](#per-use-case-class-diagrams)
  - [CD UC-20 — View Settings / Profile](#cd-uc-20--view-settings--profile)
  - [CD UC-21 — View Leaderboard](#cd-uc-21--view-leaderboard)
- [Combined Domain Class Diagram](#combined-domain-class-diagram)
  - [Concept extraction](#concept-extraction)
  - [Domain diagram](#domain-diagram)
- [Sequence Diagrams](#sequence-diagrams)
  - [Layering conventions](#layering-conventions)
  - [SD UC-20 — View Settings / Profile](#sd-uc-20--view-settings--profile)
  - [SD UC-21 — View Leaderboard](#sd-uc-21--view-leaderboard)
- [Solution Class Diagram](#solution-class-diagram)
  - [Method inventory by use case](#method-inventory-by-use-case)
  - [Solution diagram](#solution-diagram)

---

## Per-Use-Case Class Diagrams

### CD UC-20 — View Settings / Profile

```mermaid
classDiagram
    direction LR

    class SettingsUI {
        <<boundary>>
        -playerId : PlayerId
        +openSettings() void
        +render(v : ProfileView) void
        +pressChangeDefaultCharacter(cid : ItemId) void
        +pressChangeDefaultTheme(tid : ItemId) void
        +show(msg : String) void
    }
    class HtttpClient {
        <<boundary>>
        +profile(playerId : PlayerId) HtttpResponse
    }
    class ProfileController {
        <<control>>
        +profile(playerId : PlayerId) HtttpStatus
        -assemble(p : PlayerRecord, rank : int) ProfileView
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
    class PlayerRecord {
        -playerId : PlayerId
        -username : String
        -walletPoints : long
        -leaderboardScore : long
    }
    class Inventory {
        -ownedCharacters : ItemId[]
        -ownedThemes : ItemId[]
        +ids() ItemId[]
    }
    class Loadout {
        -equippedCharacter : ItemId
        -equippedTheme : ItemId
        +character() ItemId
        +theme() ItemId
    }
    class Catalogue {
        +findCharacter(cid : ItemId) Character
        +findTheme(tid : ItemId) Theme
    }
    class Db {
        <<interface>>
        libmacminidb
        +db_get_player(id, out) DbResult
        +db_rank(id, out_rank) DbResult
        +db_get_character(cid) Character
        +db_get_theme(tid) Theme
    }
    class DbResult {
        <<enum>>
        DB_OK
        DB_NOT_FOUND
        DB_IO_ERROR
    }

    SettingsUI ..> HtttpClient : 1. openSettings
    HtttpClient ..> ProfileController : PROFILE /player/<pid>
    ProfileController ..> Db : 2. db_get_player + db_rank (read lock)
    Db ..> PlayerRecord : profile + wallet + score
    Db ..> Inventory : owned sets
    Db ..> Loadout : equipped ids
    Db ..> Catalogue : id → name/portrait/palette
    Db ..> DbResult
    ProfileController ..> ProfileView : 3. assemble one payload
    SettingsUI ..> ProfileView : 3. render
    SettingsUI ..> Loadout : 3a. Change → UC-18<br/>3b. Change → UC-19
    note for Db "read-only — two read-lock reads,<br/>no persisted change"
    note for ProfileController "E1 DB_IO_ERROR → 500"
```

### CD UC-21 — View Leaderboard

```mermaid
classDiagram
    direction LR

    class LeaderboardUI {
        <<boundary>>
        +openLeaderboard() void
        +renderPodium(top3 : RankEntry[]) void
        +renderList(rest : RankEntry[]) void
        +show(msg : String) void
    }
    class HtttpClient {
        <<boundary>>
        +leaderboard(cap : int) HtttpResponse
    }
    class LeaderboardController {
        <<control>>
        +leaderboard(cap : int) HtttpStatus
    }
    class Leaderboard {
        -entries : RankEntry[]
        -count : int
        +top(cap : int) RankEntry[]
        +podium() RankEntry[]
        +remainder() RankEntry[]
    }
    class RankEntry {
        <<value object>>
        -playerId : PlayerId
        -username : String
        -leaderboardScore : long
        -rank : int
    }
    class SkipList {
        ordered by score then id
        +topN(cap : int) RankEntry[]
    }
    class Db {
        <<interface>>
        libmacminidb
        +db_leaderboard(out, cap, out_count) DbResult
    }
    class DbResult {
        <<enum>>
        DB_OK
        DB_IO_ERROR
    }

    LeaderboardUI ..> HtttpClient : 1. openLeaderboard
    HtttpClient ..> LeaderboardController : LEADERBOARD /leaderboard
    LeaderboardController ..> Db : 2. db_leaderboard(out, 10, &count)
    Db ..> SkipList : 2. topN under read lock
    SkipList ..> RankEntry : descending by (score, id)
    LeaderboardController ..> Leaderboard : rank = position + 1
    Leaderboard ..> RankEntry : 1..10
    LeaderboardUI ..> Leaderboard : 3. podium(1-3) + list(4-10)
    note for Leaderboard "rank is positional —<br/>never stored on the record"
    note for LeaderboardUI "ext 2a → ranking temporarily unavailable"
```

---

## Combined Domain Class Diagram

### Concept extraction

| Source (UC + step) | Concept surfaced | Class |
|---|---|---|
| UC-20.1 "Player opens Settings" | the settings/profile screen | `SettingsUI` |
| UC-20.3 "Username … Wallet Points … Leaderboard Scores" | the one read-only payload the screen renders | `ProfileView` `«value object»` |
| UC-20.2 "`db_get_player(id, ...)`" | the durable player document | `PlayerRecord` |
| UC-20.3 "profile picture of current default character" | what the Player currently has equipped | `Loadout` |
| UC-20.3 "Character List / Theme List (current marked)" | the owned sets, one per item kind | `Inventory` |
| UC-20.3 "Default Character / Current Theme" names + art | id → catalogue item resolution | `Catalogue` |
| UC-20.2 "`db_rank(id, &rank)` (1-based rank)" | the Player's own position | `ProfileView.rank` |
| UC-20 ext 3a / 3b "Change Default …" | the two outbound links to UC-18 / UC-19 | `SettingsUI` presses |
| UC-21.1 "Player opens the Leaderboard" | the ranking screen | `LeaderboardUI` |
| UC-21.2 "top-N by `(score, id)` straight off the skip list" | the ordered leaderboard index | `SkipList` |
| UC-21.2 "`db_leaderboard(out, 10, &count)`" | the fetched top-N slice | `Leaderboard` |
| UC-21.2 "per-row rank is positional" | one ranked row | `RankEntry` `«value object»` |
| UC-21.3 "podium for top 3, list for 4–10" | the two render bands | `Leaderboard.podium()` / `.remainder()` |
| UC-20/21 DB mapping "read-lock read" | the shared-read critical section | `ReadLock` |
| UC-20/21 "`DB_OK` / `DB_IO_ERROR`" | the read outcome vocabulary | `DbResult` `«enum»` |

### Domain diagram

```mermaid
classDiagram
    direction TB

    class Player {
        <<actor>>
        +playerId : PlayerId
        +username : String
    }

    class PlayerRecord {
        +playerId : PlayerId
        +username : String
        +walletPoints : long
        +leaderboardScore : long
        +gamesPlayed : int
        +gamesWon : int
    }
    class Wallet {
        +points : long
        +balance() long
    }
    class Inventory {
        +items : ItemId[]
        +count : int
        +ids() ItemId[]
        +owns(itemId : ItemId) bool
    }
    class Loadout {
        +equippedCharacter : ItemId
        +equippedTheme : ItemId
        +character() ItemId
        +theme() ItemId
    }

    class StoreItem {
        <<abstract>>
        +itemId : ItemId
        +name : String
        +costPoints : long
    }
    class Character {
        +characterId : ItemId
        +abilities : bitfield
    }
    class Theme {
        +themeId : ItemId
        +description : String
        +palette() ColorScheme
    }
    class Catalogue {
        +findCharacter(cid : ItemId) Character
        +findTheme(tid : ItemId) Theme
    }

    class ProfileView {
        <<value object>>
        +username : String
        +defaultCharacter : Character
        +ownedCharacters : Character[]
        +currentTheme : Theme
        +ownedThemes : Theme[]
        +walletPoints : long
        +leaderboardScore : long
        +rank : int
    }

    class Leaderboard {
        +entries : RankEntry[]
        +count : int
        +top(cap : int) RankEntry[]
        +podium() RankEntry[]
        +remainder() RankEntry[]
    }
    class RankEntry {
        <<value object>>
        +playerId : PlayerId
        +username : String
        +leaderboardScore : long
        +rank : int
    }
    class SkipList {
        +topN(cap : int) RankEntry[]
        +position(id : PlayerId) int
    }
    class ReadLock {
        +acquire()
        +release()
    }

    class DbResult {
        <<enum>>
        DB_OK
        DB_NOT_FOUND
        DB_IO_ERROR
    }

    Player --> "1" PlayerRecord : owns the record
    PlayerRecord *--> "1" Wallet
    PlayerRecord *--> "2" Inventory : characters + themes
    PlayerRecord *--> "1" Loadout

    StoreItem <|-- Character
    StoreItem <|-- Theme
    Catalogue o--> "0..*" Character : config/*.cfg at db_open
    Catalogue o--> "0..*" Theme
    Inventory o--> "0..64" StoreItem : owned
    Loadout --> "1" Character : equipped
    Loadout --> "1" Theme : equipped

    ProfileView ..> PlayerRecord : projected from  (UC-20)
    ProfileView ..> Loadout : default character + current theme
    ProfileView ..> Inventory : owned lists
    ProfileView ..> Catalogue : ids resolved to names/art
    Player ..> ProfileView : reads own profile

    SkipList o--> "0..*" PlayerRecord : ordered by (score, id)
    Leaderboard o--> "1..10" RankEntry : top-N slice
    Leaderboard ..> SkipList : topN(cap)
    RankEntry ..> PlayerRecord : username + score snapshot
    SkipList ..> ReadLock : shared read
    ProfileView ..> SkipList : own rank via position()
    Player ..> Leaderboard : reads global ranking
    Leaderboard ..> DbResult : read outcome
```

---

## Sequence Diagrams

### Layering conventions

| Participant | Layer | Process |
|---|---|---|
| `:SettingsUI`, `:LeaderboardUI` | UI | tetrisu |
| `:HtttpClient` | Boundary | tetrisu |
| `:ProfileController`, `:LeaderboardController` | Controller | tetrisd |
| `:Leaderboard`, `:SkipList`, `:ReadLock` | Domain (internal to the store) | `libmacminidb` |
| `:Db` | Persistence façade | `libmacminidb` |

Both use cases are **persisted reads** — they take the DB **read** lock only and
change nothing. Every write they lead to belongs to another use case (UC-18 / UC-19).

### SD UC-20 — View Settings / Profile

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :SettingsUI
    participant C as :HtttpClient
    participant PC as :ProfileController
    participant DB as :Db

    P->>UI: openSettings()  (step 1)
    activate UI
    UI->>C: profile(playerId)
    activate C
    C->>PC: PROFILE /player/<pid>
    activate PC

    PC->>DB: db_get_player(id, &player)
    activate DB
    DB->>DB: readLock.acquire() → read → release()
    DB-->>PC: DB_OK {player}
    PC->>DB: db_rank(id, &rank)
    DB->>DB: readLock.acquire() → skipList.position(id) → release()
    DB-->>PC: DB_OK {rank}

    loop each owned character / theme id  (step 3)
        PC->>DB: db_get_character(cid) / db_get_theme(tid)
        DB-->>PC: Character | Theme  (borrowed pointer)
    end
    deactivate DB

    alt DB_OK  (step 3)
        PC->>PC: assemble(player, rank) → ProfileView
        PC-->>C: 200 OK {ProfileView}
        C-->>UI: profileView
        UI-->>P: render(v) — username, pfp, default character,<br/>character list, current theme, theme list,<br/>wallet points, score, rank
    else DB_IO_ERROR  (E1)
        DB-->>PC: DB_IO_ERROR
        PC-->>C: 500 Internal Server Error
        C-->>UI: error(500)
        UI-->>P: show("could not load profile")
    end

    deactivate PC
    deactivate C

    opt ext 3a — Change Default Character
        P->>UI: pressChangeDefaultCharacter(cid)
        note over UI: → UC-18 Set Default Character
    end
    opt ext 3b — Change Default Theme
        P->>UI: pressChangeDefaultTheme(tid)
        note over UI: → UC-19 Set Default Theme
    end
    deactivate UI
```

### SD UC-21 — View Leaderboard

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :LeaderboardUI
    participant C as :HtttpClient
    participant LC as :LeaderboardController
    participant DB as :Db
    participant SL as :SkipList

    P->>UI: openLeaderboard()  (step 1)
    activate UI
    UI->>C: leaderboard(cap = 10)
    activate C
    C->>LC: LEADERBOARD /leaderboard
    activate LC

    LC->>DB: db_leaderboard(out, 10, &count)
    activate DB
    DB->>DB: readLock.acquire()
    DB->>SL: topN(10)
    activate SL
    SL-->>DB: entries descending by (score, id)
    deactivate SL
    DB->>DB: readLock.release()

    alt DB_OK  (step 2)
        DB-->>LC: DB_OK {entries, count}
        LC->>LC: rank = position + 1 per row
        LC-->>C: 200 OK {RankEntry[count]}
        C-->>UI: entries
        UI-->>P: renderPodium(1st, 2nd, 3rd)  (step 3)
        UI-->>P: renderList(ranks 4-10 with points)
    else DB_IO_ERROR  (ext 2a)
        DB-->>LC: DB_IO_ERROR
        LC-->>C: 500 Internal Server Error
        C-->>UI: error(500)
        UI-->>P: show("ranking temporarily unavailable")
    end

    deactivate DB
    deactivate LC
    deactivate C
    deactivate UI
```

---

## Solution Class Diagram

### Method inventory by use case

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

### Solution diagram

```mermaid
classDiagram
    direction TB

    %% ---------------- UI layer (tetrisu) ----------------
    class SettingsUI {
        <<boundary>>
        -playerId : PlayerId
        +openSettings() void
        +render(v : ProfileView) void
        +pressChangeDefaultCharacter(cid : ItemId) void
        +pressChangeDefaultTheme(tid : ItemId) void
        +show(msg : String) void
    }
    class LeaderboardUI {
        <<boundary>>
        +openLeaderboard() void
        +renderPodium(top3 : RankEntry[]) void
        +renderList(rest : RankEntry[]) void
        +show(msg : String) void
    }

    %% ---------------- Boundary ----------------
    class HtttpClient {
        <<boundary>>
        +profile(playerId : PlayerId) HtttpResponse
        +leaderboard(cap : int) HtttpResponse
    }

    %% ---------------- Controllers (tetrisd, stateless) ----------------
    class ProfileController {
        <<control>>
        +profile(playerId : PlayerId) HtttpStatus
        -assemble(p : PlayerRecord, rank : int) ProfileView
    }
    class LeaderboardController {
        <<control>>
        +leaderboard(cap : int) HtttpStatus
    }

    %% ---------------- Domain (libmacminidb internals) ----------------
    class PlayerRecord {
        -playerId : PlayerId
        -username : String
        -walletPoints : long
        -leaderboardScore : long
        -gamesPlayed : int
        -gamesWon : int
    }
    class Inventory {
        -items : ItemId[]
        -count : int
        +ids() ItemId[]
        +owns(itemId : ItemId) bool
    }
    class Loadout {
        -equippedCharacter : ItemId
        -equippedTheme : ItemId
        +character() ItemId
        +theme() ItemId
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
    }
    class Character {
        -characterId : ItemId
        -abilities : bitfield
    }
    class Theme {
        -themeId : ItemId
        -description : String
        +palette() ColorScheme
    }
    class Leaderboard {
        -entries : RankEntry[]
        -count : int
        +top(cap : int) RankEntry[]
        +podium() RankEntry[]
        +remainder() RankEntry[]
    }
    class SkipList {
        ordered by score then id
        +topN(cap : int) RankEntry[]
        +position(id : PlayerId) int
    }
    class ReadLock {
        +acquire() void
        +release() void
    }

    %% ---------------- Value objects ----------------
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
    class ColorScheme {
        <<value object>>
        -cellColors : Color[7]
        -accent : Color
    }

    %% ---------------- Persistence façade ----------------
    class Db {
        <<interface>>
        libmacminidb
        +db_get_player(id, out) DbResult
        +db_rank(id, out_rank) DbResult
        +db_leaderboard(out, cap, out_count) DbResult
        +db_get_character(cid) Character
        +db_get_theme(tid) Theme
    }

    %% ---------------- Enums ----------------
    class DbResult {
        <<enum>>
        DB_OK
        DB_NOT_FOUND
        DB_IO_ERROR
    }
    class HtttpStatus {
        <<enum>>
        OK_200
        NOT_FOUND_404
        SERVER_ERROR_500
    }

    %% ---------------- Relationships ----------------
    SettingsUI ..> HtttpClient : uses
    LeaderboardUI ..> HtttpClient : uses
    SettingsUI ..> ProfileView : renders
    LeaderboardUI ..> Leaderboard : podium + list
    SettingsUI ..> Character : default + pfp
    SettingsUI ..> Theme : current theme
    Theme ..> ColorScheme : palette

    HtttpClient ..> ProfileController : HTTTP PROFILE /player/<pid>
    HtttpClient ..> LeaderboardController : HTTTP LEADERBOARD /leaderboard

    ProfileController ..> Db : db_get_player + db_rank + db_get_*
    ProfileController ..> ProfileView : assembles
    ProfileController ..> HtttpStatus : maps DbResult
    LeaderboardController ..> Db : db_leaderboard
    LeaderboardController ..> Leaderboard : positional ranks
    LeaderboardController ..> HtttpStatus : maps DbResult

    Db ..> PlayerRecord : point lookup by id
    Db ..> SkipList : top-N + own position
    Db ..> Catalogue : item lookup
    Db ..> ReadLock : shared read only
    Db ..> DbResult

    PlayerRecord *--> "2" Inventory : characters + themes
    PlayerRecord *--> "1" Loadout
    ProfileView ..> PlayerRecord : projection
    Leaderboard o--> "1..10" RankEntry
    SkipList ..> RankEntry : ordered rows

    StoreItem <|-- Character
    StoreItem <|-- Theme
    Catalogue o--> "0..*" Character : config/*.cfg
    Catalogue o--> "0..*" Theme
    Inventory o--> "0..64" StoreItem : owned
    Loadout --> "1" Character : equipped
    Loadout --> "1" Theme : equipped
```
