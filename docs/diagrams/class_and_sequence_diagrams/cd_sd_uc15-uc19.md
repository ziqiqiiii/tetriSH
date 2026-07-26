# tetriSH — Marketplace (UC-15 – UC-19)

## Table of Contents

- [Per-Use-Case Class Diagrams](#per-use-case-class-diagrams)
  - [CD UC-15 — Buy Character](#cd-uc-15--buy-character)
  - [CD UC-15a — Determine Character Button State](#cd-uc-15a--determine-character-button-state)
  - [CD UC-16 — Buy Theme](#cd-uc-16--buy-theme)
  - [CD UC-16a — Determine Theme Button State](#cd-uc-16a--determine-theme-button-state)
  - [CD UC-17 — Deduct Wallet Points](#cd-uc-17--deduct-wallet-points)
  - [CD UC-18 — Set Default Character](#cd-uc-18--set-default-character)
  - [CD UC-19 — Set Default Theme](#cd-uc-19--set-default-theme)
- [Combined Domain Class Diagram](#combined-domain-class-diagram)
  - [Concept extraction](#concept-extraction)
  - [Domain diagram](#domain-diagram)
- [Sequence Diagrams](#sequence-diagrams)
  - [Layering conventions](#layering-conventions)
  - [SD UC-15 — Buy Character (includes UC-15a, UC-17)](#sd-uc-15--buy-character-includes-uc-15a-uc-17)
  - [SD UC-15a — Determine Character Button State](#sd-uc-15a--determine-character-button-state)
  - [SD UC-16 — Buy Theme (includes UC-16a, UC-17)](#sd-uc-16--buy-theme-includes-uc-16a-uc-17)
  - [SD UC-16a — Determine Theme Button State](#sd-uc-16a--determine-theme-button-state)
  - [SD UC-17 — Deduct Wallet Points](#sd-uc-17--deduct-wallet-points)
  - [SD UC-18 — Set Default Character](#sd-uc-18--set-default-character)
  - [SD UC-19 — Set Default Theme](#sd-uc-19--set-default-theme)
- [Solution Class Diagram](#solution-class-diagram)
  - [Method inventory by use case](#method-inventory-by-use-case)
  - [Solution diagram](#solution-diagram)

---

## Per-Use-Case Class Diagrams

### CD UC-15 — Buy Character

```mermaid
classDiagram
    direction LR

    class MarketplaceUI {
        <<boundary>>
        -playerId : PlayerId
        -tab : StoreTab
        -selected : ItemId
        +selectTab(t : StoreTab) void
        +selectCharacter(cid : ItemId) void
        +showPreview(c : Character) void
        +pressBuy() void
        +show(msg : String) void
    }
    class ButtonState {
        <<value object>>
        -buyEnabled : bool
        -setDefaultEnabled : bool
        +ownedState()$ ButtonState
        +unownedState()$ ButtonState
    }
    class HtttpClient {
        <<boundary>>
        +buyCharacter(playerId : PlayerId, cid : ItemId) HtttpResponse
    }
    class StoreController {
        <<control>>
        +buyCharacter(playerId : PlayerId, cid : ItemId) HtttpStatus
    }
    class Character {
        -characterId : ItemId
        -name : String
        -abilities : bitfield
        -costPoints : long
        +grants(level : int) Ability
    }
    class Wallet {
        -points : long
        +canAfford(cost : long) bool
        +debit(cost : long) void
    }
    class Inventory {
        -ownedCharacters : ItemId[]
        -ownedCharactersCount : int
        +owns(cid : ItemId) bool
        +isFull() bool
        +add(cid : ItemId) void
    }
    class Ability {
        <<value object>>
        -level : int
        -cost : int
        -name : String
    }
    class Db {
        <<interface>>
        libmacminidb
        +db_buy_character(id, cid) DbResult
        +db_get_character(cid) Character
    }
    class DbResult {
        <<enum>>
        DB_OK
        DB_EXISTS
        DB_INSUFFICIENT
        DB_FULL
        DB_NOT_FOUND
        DB_IO_ERROR
    }

    MarketplaceUI ..> HtttpClient : 3. pressBuy
    MarketplaceUI ..> ButtonState : 2. + 5. «include» UC-15a
    MarketplaceUI ..> Character : 2. showPreview(abilities)
    HtttpClient ..> StoreController : BUY /store/character/<cid>
    StoreController ..> Db : 4. db_buy_character «include» UC-17
    Db ..> DbResult : outcome → HTTTP status
    Db ..> Wallet : debits under write lock
    Db ..> Inventory : grants under the same lock
    Character ..> Ability : 2. Ability 1..4 shown in preview
    note for Db "check-and-act is ONE atom:<br/>exists → owned → affordable → cap"
    note for StoreController "4b DB_INSUFFICIENT → 403<br/>4c DB_FULL → 409<br/>4d DB_NOT_FOUND → 404<br/>E1 DB_IO_ERROR → 500"
```

### CD UC-15a — Determine Character Button State

```mermaid
classDiagram
    direction LR

    class MarketplaceUI {
        <<boundary>>
        -selected : ItemId
        +selectCharacter(cid : ItemId) void
        +applyButtonState(s : ButtonState) void
        +show(msg : String) void
    }
    class ButtonState {
        <<value object>>
        -buyEnabled : bool
        -setDefaultEnabled : bool
        +ownedState()$ ButtonState
        +unownedState()$ ButtonState
        +bothDisabled()$ ButtonState
    }
    class HtttpClient {
        <<boundary>>
        +ownsCharacter(playerId : PlayerId, cid : ItemId) HtttpResponse
    }
    class StoreController {
        <<control>>
        +ownsCharacter(playerId : PlayerId, cid : ItemId) DbBool
    }
    class Inventory {
        -ownedCharacters : ItemId[]
        +owns(cid : ItemId) bool
    }
    class Db {
        <<interface>>
        libmacminidb
        +db_player_owns_character(id, cid) DbBool
    }
    class DbBool {
        <<enum>>
        DB_TRUE
        DB_FALSE
        DB_UNKNOWN
    }

    MarketplaceUI ..> HtttpClient : 1. on select/highlight
    HtttpClient ..> StoreController : ownership query
    StoreController ..> Db : 2. db_player_owns_character (read lock)
    Db ..> Inventory : reads owned set
    Db ..> DbBool : DB_TRUE / DB_FALSE / DB_UNKNOWN
    StoreController ..> ButtonState : 3. unowned → BUY on<br/>4. owned → Set-as-Default on
    MarketplaceUI ..> ButtonState : applies
    note for Db "read-lock read — no persisted change"
    note for MarketplaceUI "ext 2a DB_IO_ERROR →<br/>bothDisabled() + transient error;<br/>reselect to retry"
```

### CD UC-16 — Buy Theme

```mermaid
classDiagram
    direction LR

    class MarketplaceUI {
        <<boundary>>
        -playerId : PlayerId
        -tab : StoreTab
        -selected : ItemId
        +selectTab(t : StoreTab) void
        +selectTheme(tid : ItemId) void
        +showPreview(t : Theme) void
        +pressBuy() void
        +show(msg : String) void
    }
    class ButtonState {
        <<value object>>
        -buyEnabled : bool
        -setDefaultEnabled : bool
        +ownedState()$ ButtonState
        +unownedState()$ ButtonState
    }
    class HtttpClient {
        <<boundary>>
        +buyTheme(playerId : PlayerId, tid : ItemId) HtttpResponse
    }
    class StoreController {
        <<control>>
        +buyTheme(playerId : PlayerId, tid : ItemId) HtttpStatus
    }
    class Theme {
        -themeId : ItemId
        -name : String
        -costPoints : long
        -description : String
        +palette() ColorScheme
    }
    class Wallet {
        -points : long
        +canAfford(cost : long) bool
        +debit(cost : long) void
    }
    class Inventory {
        -ownedThemes : ItemId[]
        -ownedThemesCount : int
        +owns(tid : ItemId) bool
        +isFull() bool
        +add(tid : ItemId) void
    }
    class Db {
        <<interface>>
        libmacminidb
        +db_buy_theme(id, tid) DbResult
        +db_get_theme(tid) Theme
    }
    class DbResult {
        <<enum>>
        DB_OK
        DB_EXISTS
        DB_INSUFFICIENT
        DB_FULL
        DB_NOT_FOUND
        DB_IO_ERROR
    }

    MarketplaceUI ..> HtttpClient : 3. pressBuy
    MarketplaceUI ..> ButtonState : 2. + 5. «include» UC-16a
    MarketplaceUI ..> Theme : 2. colour scheme + nickname/pfp
    HtttpClient ..> StoreController : BUY /store/theme/<tid>
    StoreController ..> Db : 4. db_buy_theme «include» UC-17
    Db ..> DbResult : outcome → HTTTP status
    Db ..> Wallet : debits under write lock
    Db ..> Inventory : grants under the same lock
    note for StoreController "identical result mapping to UC-15 —<br/>only the collection differs"
```

### CD UC-16a — Determine Theme Button State

```mermaid
classDiagram
    direction LR

    class MarketplaceUI {
        <<boundary>>
        -selected : ItemId
        +selectTheme(tid : ItemId) void
        +applyButtonState(s : ButtonState) void
        +show(msg : String) void
    }
    class ButtonState {
        <<value object>>
        -buyEnabled : bool
        -setDefaultEnabled : bool
        +ownedState()$ ButtonState
        +unownedState()$ ButtonState
        +bothDisabled()$ ButtonState
    }
    class HtttpClient {
        <<boundary>>
        +ownsTheme(playerId : PlayerId, tid : ItemId) HtttpResponse
    }
    class StoreController {
        <<control>>
        +ownsTheme(playerId : PlayerId, tid : ItemId) DbBool
    }
    class Inventory {
        -ownedThemes : ItemId[]
        +owns(tid : ItemId) bool
    }
    class Db {
        <<interface>>
        libmacminidb
        +db_player_owns_theme(id, tid) DbBool
    }
    class DbBool {
        <<enum>>
        DB_TRUE
        DB_FALSE
        DB_UNKNOWN
    }

    MarketplaceUI ..> HtttpClient : 1. on select/highlight
    HtttpClient ..> StoreController : ownership query
    StoreController ..> Db : 2. db_player_owns_theme (read lock)
    Db ..> Inventory : reads owned set
    Db ..> DbBool : DB_TRUE / DB_FALSE / DB_UNKNOWN
    StoreController ..> ButtonState : 3. unowned → BUY on<br/>4. owned → Set-as-Default on
    MarketplaceUI ..> ButtonState : applies
    note for MarketplaceUI "ext 2a DB_IO_ERROR →<br/>bothDisabled() + transient error"
```

### CD UC-17 — Deduct Wallet Points

```mermaid
classDiagram
    direction LR

    class Db {
        <<interface>>
        libmacminidb
        +db_buy_character(id, cid) DbResult
        +db_buy_theme(id, tid) DbResult
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
    class PlayerRecord {
        -playerId : PlayerId
        -walletPoints : long
        -ownedCharacters : ItemId[]
        -ownedThemes : ItemId[]
        +debit(cost : long) void
        +grant(itemId : ItemId) void
    }
    class Wallet {
        -points : long
        +canAfford(cost : long) bool
        +debit(cost : long) void
    }
    class Inventory {
        -count : int
        -cap : int
        +owns(itemId : ItemId) bool
        +isFull() bool
        +add(itemId : ItemId) void
    }
    class WalLog {
        +append(rec : PlayerRecord) DbResult
        +flush() void
    }
    class DbResult {
        <<enum>>
        DB_OK
        DB_EXISTS
        DB_INSUFFICIENT
        DB_FULL
        DB_NOT_FOUND
        DB_IO_ERROR
    }

    Db ..> PurchaseTxn : 1. delegates the payment step
    PurchaseTxn ..> WriteLock : 1. acquire / 4. release
    PurchaseTxn ..> Inventory : 2. exists? owned? full?
    PurchaseTxn ..> Wallet : 2. points >= cost?
    PurchaseTxn ..> PlayerRecord : 3. debit + grant (same record)
    PurchaseTxn ..> WalLog : 4. append whole record (LWW)
    PurchaseTxn ..> DbResult : returns
    PlayerRecord *--> "1" Wallet
    PlayerRecord *--> "2" Inventory : characters + themes
    WalLog ..> DbResult : DB_IO_ERROR on append failure
    note for PurchaseTxn "ext 2a any precheck fails →<br/>return, NO wallet/inventory change"
    note for WalLog "background flusher fdatasyncs <= 1 s;<br/>no blocking syscall under the lock"
```

### CD UC-18 — Set Default Character

```mermaid
classDiagram
    direction LR

    class MarketplaceUI {
        <<boundary>>
        +pressSetAsDefaultCharacter(cid : ItemId) void
        +show(msg : String) void
    }
    class SettingsUI {
        <<boundary>>
        +pressChangeDefaultCharacter(cid : ItemId) void
        +showDefaultCharacter(c : Character) void
        +showProfilePicture(c : Character) void
        +show(msg : String) void
    }
    class HtttpClient {
        <<boundary>>
        +equipCharacter(playerId : PlayerId, cid : ItemId) HtttpResponse
    }
    class EquipController {
        <<control>>
        +equipCharacter(playerId : PlayerId, cid : ItemId) HtttpStatus
    }
    class Loadout {
        -equippedCharacter : ItemId
        -equippedTheme : ItemId
        +setCharacter(cid : ItemId) void
        +character() ItemId
    }
    class Inventory {
        -ownedCharacters : ItemId[]
        +owns(cid : ItemId) bool
    }
    class Character {
        -characterId : ItemId
        -name : String
        -abilities : bitfield
    }
    class Db {
        <<interface>>
        libmacminidb
        +db_equip_character(id, cid) DbResult
    }
    class DbResult {
        <<enum>>
        DB_OK
        DB_NOT_OWNED
        DB_NOT_FOUND
    }

    MarketplaceUI ..> HtttpClient : 2. Set as Default Character
    SettingsUI ..> HtttpClient : 2. Change Default Character
    HtttpClient ..> EquipController : EQUIP /player/<pid>/character/<cid>
    EquipController ..> Db : 3. db_equip_character
    Db ..> Inventory : ownership gate
    Db ..> Loadout : 3. current_equipped_character
    Db ..> DbResult
    SettingsUI ..> Character : 4. name + portrait
    note for EquipController "ext 3a DB_NOT_OWNED → 403;<br/>Player must Buy (UC-15) first"
    note for MarketplaceUI "two entry points, one use case"
```

### CD UC-19 — Set Default Theme

```mermaid
classDiagram
    direction LR

    class MarketplaceUI {
        <<boundary>>
        +pressSetAsDefaultTheme(tid : ItemId) void
        +show(msg : String) void
    }
    class SettingsUI {
        <<boundary>>
        +pressChangeDefaultTheme(tid : ItemId) void
        +showCurrentTheme(t : Theme) void
        +show(msg : String) void
    }
    class HtttpClient {
        <<boundary>>
        +equipTheme(playerId : PlayerId, tid : ItemId) HtttpResponse
    }
    class EquipController {
        <<control>>
        +equipTheme(playerId : PlayerId, tid : ItemId) HtttpStatus
    }
    class Loadout {
        -equippedCharacter : ItemId
        -equippedTheme : ItemId
        +setTheme(tid : ItemId) void
        +theme() ItemId
    }
    class Inventory {
        -ownedThemes : ItemId[]
        +owns(tid : ItemId) bool
    }
    class Theme {
        -themeId : ItemId
        -name : String
        -description : String
        +palette() ColorScheme
    }
    class GameplayUI {
        <<boundary>>
        +applyTheme(p : ColorScheme) void
    }
    class Db {
        <<interface>>
        libmacminidb
        +db_equip_theme(id, tid) DbResult
    }
    class DbResult {
        <<enum>>
        DB_OK
        DB_NOT_OWNED
        DB_NOT_FOUND
    }

    MarketplaceUI ..> HtttpClient : 2. Set as Default Theme
    SettingsUI ..> HtttpClient : 2. Change Default Theme
    HtttpClient ..> EquipController : EQUIP /player/<pid>/theme/<tid>
    EquipController ..> Db : 3. db_equip_theme
    Db ..> Inventory : ownership gate
    Db ..> Loadout : 3. current_equipped_theme
    Db ..> DbResult
    SettingsUI ..> Theme : 4. Current Theme
    Theme ..> GameplayUI : 4. palette applied at next game start
    note for EquipController "ext 3a DB_NOT_OWNED → 403;<br/>Player must Buy (UC-16) first"
```

---

## Combined Domain Class Diagram

### Concept extraction

| Source (UC + step) | Concept surfaced | Class |
|---|---|---|
| UC-15.1 "Characters tab / Themes tab" | which half of the store is showing | `StoreTab` `«enum»` |
| UC-15.1 "a specific character (Princess, Halloween, …)" | the priced catalogue item granting abilities | `Character` |
| UC-15.2 "shows the preview and abilities (Ability 1, Ability 2, …)" | one of four levels a character grants | `Ability` `«value object»` |
| UC-15.2 "determines BUY / Set as Default button state" | the two-button enable/disable pair | `ButtonState` `«value object»` |
| UC-15 DB mapping "`cost_points`" | the item's price | `Character.costPoints` / `Theme.costPoints` |
| UC-15 post "wallet debited by `cost_points`" | the Player's spendable balance | `Wallet` |
| UC-15 post "character added to `owned_characters`" | the owned sets, one per item kind | `Inventory` |
| UC-15 ext 4c "`owned_characters_count` is at `DB_MAX_OWNED`" | the ownership cap | `Inventory.cap` |
| UC-15a.2 "`db_player_owns_character(id, cid)`" | ownership test, read-lock only | `Inventory.owns()` |
| UC-16.1 "a specific theme (Default, Haaland, John Cena, …)" | the priced cosmetic item | `Theme` |
| UC-16.2 "the theme's colour scheme" | the palette a theme carries | `ColorScheme` `«value object»` |
| UC-16.2 "character nickname/profile-picture details" | theme copy shown in the preview | `Theme.description` |
| UC-17.1 "takes the DB write lock" | the single-writer critical section | `WriteLock` |
| UC-17.2 "checks, in order" | the ordered precheck-then-settle purchase | `PurchaseTxn` |
| UC-17.3 "debits **and** adds … on the same player record" | the whole-record unit of write | `PlayerRecord` |
| UC-17.4 "appends the whole updated record to the log" | append-only LWW log + 1 s flusher | `WalLog` |
| UC-18 post "`current_equipped_character` is updated" | what the Player currently has equipped | `Loadout` |
| UC-18.4 "reflects the change in Settings … and in-game" | the settings/profile screen | `SettingsUI` |
| UC-19.4 "loads it on the next game start" | the render surface that adopts the palette | `GameplayUI` |
| UC-15/16 DB mapping "`DB_OK` / `DB_EXISTS` / …" | the store outcome vocabulary | `DbResult` `«enum»` |

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
        +walletPoints : long
        +debit(cost : long)
        +grant(itemId : ItemId)
    }
    class Wallet {
        +points : long
        +canAfford(cost : long) bool
        +debit(cost : long)
    }
    class Inventory {
        +items : ItemId[]
        +count : int
        +cap : int
        +owns(itemId : ItemId) bool
        +isFull() bool
        +add(itemId : ItemId)
    }
    class Loadout {
        +equippedCharacter : ItemId
        +equippedTheme : ItemId
        +setCharacter(cid : ItemId)
        +setTheme(tid : ItemId)
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
        +grants(level : int) Ability
    }
    class Theme {
        +themeId : ItemId
        +description : String
        +palette() ColorScheme
    }
    class Ability {
        <<value object>>
        +level : int
        +cost : int
        +name : String
    }
    class ColorScheme {
        <<value object>>
        +cellColors : Color[7]
        +accent : Color
    }
    class Catalogue {
        +characters : Character[]
        +themes : Theme[]
        +findCharacter(cid : ItemId) Character
        +findTheme(tid : ItemId) Theme
    }

    class PurchaseTxn {
        +playerId : PlayerId
        +itemId : ItemId
        +cost : long
        +run() DbResult
        +precheck() DbResult
        +settle()
    }
    class WriteLock {
        +acquire()
        +release()
    }
    class WalLog {
        +append(rec : PlayerRecord) DbResult
        +flush()
    }

    class ButtonState {
        <<value object>>
        +buyEnabled : bool
        +setDefaultEnabled : bool
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
        DB_INSUFFICIENT
        DB_NOT_OWNED
        DB_FULL
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
    Character ..> "4" Ability : grants
    Theme ..> ColorScheme : palette

    Inventory o--> "0..64" StoreItem : owned, capped at DB_MAX_OWNED
    Loadout --> "1" Character : equipped
    Loadout --> "1" Theme : equipped

    PurchaseTxn ..> WriteLock : one atom
    PurchaseTxn ..> Wallet : affordability + debit
    PurchaseTxn ..> Inventory : ownership + cap + grant
    PurchaseTxn ..> StoreItem : reads costPoints
    PurchaseTxn ..> WalLog : append whole record
    PurchaseTxn ..> DbResult : outcome
    WalLog ..> PlayerRecord : LWW replay on boot

    ButtonState ..> Inventory : derived from owns()
    ButtonState o--> StoreTab : per-tab selection
    Player ..> ButtonState : sees BUY / Set-as-Default
```

---

## Sequence Diagrams

### Layering conventions

| Participant | Layer | Process |
|---|---|---|
| `:MarketplaceUI`, `:SettingsUI`, `:GameplayUI` | UI | tetrisu |
| `:HtttpClient` | Boundary | tetrisu |
| `:StoreController`, `:EquipController` | Controller | tetrisd |
| `:PurchaseTxn`, `:WriteLock`, `:WalLog` | Domain (internal to the store) | `libmacminidb` |
| `:Db` | Persistence façade | `libmacminidb` |

All five use cases are **persisted** — UC-15/16/17 take the DB write lock, UC-15a/16a
take the read lock, UC-18/19 take the write lock for a single field.

### SD UC-15 — Buy Character (includes UC-15a, UC-17)

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :MarketplaceUI
    participant C as :HtttpClient
    participant SC as :StoreController
    participant DB as :Db

    P->>UI: selectTab(CHARACTERS)
    activate UI
    P->>UI: selectCharacter(cid)

    rect rgb(240, 240, 240)
        note over UI,DB: «include» UC-15a Determine Character Button State  (step 2)
        UI->>C: ownsCharacter(playerId, cid)
        activate C
        C->>SC: ownership query
        activate SC
        SC->>DB: db_player_owns_character(id, cid)
        activate DB
        DB-->>SC: DB_FALSE
        deactivate DB
        SC-->>C: unowned
        deactivate SC
        C-->>UI: unowned
        deactivate C
        UI->>UI: applyButtonState(unownedState())
    end

    UI-->>P: showPreview(portrait, Ability 1..4, costPoints)

    P->>UI: pressBuy()
    UI->>C: buyCharacter(playerId, cid)
    activate C
    C->>SC: BUY /store/character/<cid>  Player-Id: <pid>
    activate SC

    rect rgb(240, 240, 240)
        note over SC,DB: «include» UC-17 Deduct Wallet Points  (step 4)
        SC->>DB: db_buy_character(id, cid)
        activate DB
        DB->>DB: writeLock.acquire()
        DB->>DB: precheck: exists → owned → affordable → cap
        DB->>DB: wallet -= cost_points, owned_characters += cid
        DB->>DB: walLog.append(playerRecord)  %% LWW
        DB->>DB: writeLock.release()
    end

    alt DB_OK  (step 5)
        DB-->>SC: DB_OK
        SC-->>C: 200 OK {walletPoints, ownedCharacters}
        C-->>UI: bought(cid)
        UI->>UI: applyButtonState(ownedState())
        note right of UI: «include» UC-15a again —<br/>BUY off, Set as Default on
        UI-->>P: show("purchased") → UC-18 now reachable
    else DB_EXISTS — already owned  (ext 4a)
        DB-->>SC: DB_EXISTS
        SC-->>C: 200 OK  %% no-op
        C-->>UI: alreadyOwned(cid)
        note right of DB: no debit, no change
        UI->>UI: applyButtonState(ownedState())
    else DB_INSUFFICIENT  (ext 4b)
        DB-->>SC: DB_INSUFFICIENT
        SC-->>C: 403 Forbidden
        C-->>UI: error(403)
        UI-->>P: show("Error: Insufficient points")
        note right of DB: wallet unchanged
    else DB_FULL  (ext 4c)
        DB-->>SC: DB_FULL
        SC-->>C: 409 Conflict
        C-->>UI: error(409)
        UI-->>P: show("Error: Inventory full")
    else DB_NOT_FOUND  (ext 4d)
        DB-->>SC: DB_NOT_FOUND
        SC-->>C: 404 Not Found
        C-->>UI: error(404)
        UI-->>P: show("Error: No such character")
    else DB_IO_ERROR  (E1)
        DB-->>SC: DB_IO_ERROR
        SC-->>C: 500
        C-->>UI: error(500)
        UI-->>P: show("purchase failed — wallet unchanged")
    end

    deactivate DB
    deactivate SC
    deactivate C
    deactivate UI
```

### SD UC-15a — Determine Character Button State

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :MarketplaceUI
    participant C as :HtttpClient
    participant SC as :StoreController
    participant DB as :Db

    P->>UI: selectCharacter(cid)
    activate UI
    UI->>C: ownsCharacter(playerId, cid)
    activate C
    C->>SC: ownership query
    activate SC
    SC->>DB: db_player_owns_character(id, cid)
    activate DB
    note right of DB: read lock only —<br/>no persisted state change

    alt not owned  (step 3)
        DB-->>SC: DB_FALSE
        SC-->>C: unowned
        C-->>UI: unowned
        UI->>UI: applyButtonState(unownedState())
        UI-->>P: BUY enabled, Set as Default disabled
    else owned  (step 4)
        DB-->>SC: DB_TRUE
        SC-->>C: owned
        C-->>UI: owned
        UI->>UI: applyButtonState(ownedState())
        UI-->>P: BUY disabled, Set as Default enabled
    else read fails — DB_IO_ERROR  (ext 2a)
        DB-->>SC: DB_IO_ERROR
        SC-->>C: 500
        C-->>UI: error(500)
        UI->>UI: applyButtonState(bothDisabled())
        UI-->>P: show(transient error) — reselect to retry
    end

    deactivate DB
    deactivate SC
    deactivate C
    deactivate UI
```

### SD UC-16 — Buy Theme (includes UC-16a, UC-17)

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :MarketplaceUI
    participant C as :HtttpClient
    participant SC as :StoreController
    participant DB as :Db

    P->>UI: selectTab(THEMES)
    activate UI
    P->>UI: selectTheme(tid)

    rect rgb(240, 240, 240)
        note over UI,DB: «include» UC-16a Determine Theme Button State  (step 2)
        UI->>C: ownsTheme(playerId, tid)
        activate C
        C->>SC: ownership query
        activate SC
        SC->>DB: db_player_owns_theme(id, tid)
        activate DB
        DB-->>SC: DB_FALSE
        deactivate DB
        SC-->>C: unowned
        deactivate SC
        C-->>UI: unowned
        deactivate C
        UI->>UI: applyButtonState(unownedState())
    end

    UI-->>P: showPreview(colourScheme, nickname/pfp, costPoints)

    P->>UI: pressBuy()
    UI->>C: buyTheme(playerId, tid)
    activate C
    C->>SC: BUY /store/theme/<tid>  Player-Id: <pid>
    activate SC

    rect rgb(240, 240, 240)
        note over SC,DB: «include» UC-17 Deduct Wallet Points  (step 4)
        SC->>DB: db_buy_theme(id, tid)
        activate DB
        DB->>DB: writeLock.acquire()
        DB->>DB: precheck: exists → owned → affordable → cap
        DB->>DB: wallet -= cost_points, owned_themes += tid
        DB->>DB: walLog.append(playerRecord)  %% LWW
        DB->>DB: writeLock.release()
    end

    alt DB_OK  (step 5)
        DB-->>SC: DB_OK
        SC-->>C: 200 OK {walletPoints, ownedThemes}
        C-->>UI: bought(tid)
        UI->>UI: applyButtonState(ownedState())
        UI-->>P: show("purchased") → UC-19 now reachable
    else DB_EXISTS — already owned  (ext 4a)
        DB-->>SC: DB_EXISTS
        SC-->>C: 200 OK  %% no-op
        C-->>UI: alreadyOwned(tid)
        UI->>UI: applyButtonState(ownedState())
    else DB_INSUFFICIENT  (ext 4b)
        DB-->>SC: DB_INSUFFICIENT
        SC-->>C: 403 Forbidden
        C-->>UI: error(403)
        UI-->>P: show("Error: Insufficient points")
    else DB_FULL  (ext 4c)
        DB-->>SC: DB_FULL
        SC-->>C: 409 Conflict
        C-->>UI: error(409)
        UI-->>P: show("Error: Inventory full")
    else DB_NOT_FOUND  (ext 4d)
        DB-->>SC: DB_NOT_FOUND
        SC-->>C: 404 Not Found
        C-->>UI: error(404)
        UI-->>P: show("Error: No such theme")
    else DB_IO_ERROR  (E1)
        DB-->>SC: DB_IO_ERROR
        SC-->>C: 500
        C-->>UI: error(500)
        UI-->>P: show("purchase failed — wallet unchanged")
    end

    deactivate DB
    deactivate SC
    deactivate C
    deactivate UI
```

### SD UC-16a — Determine Theme Button State

```mermaid
sequenceDiagram
    actor P as Player
    participant UI as :MarketplaceUI
    participant C as :HtttpClient
    participant SC as :StoreController
    participant DB as :Db

    P->>UI: selectTheme(tid)
    activate UI
    UI->>C: ownsTheme(playerId, tid)
    activate C
    C->>SC: ownership query
    activate SC
    SC->>DB: db_player_owns_theme(id, tid)
    activate DB
    note right of DB: read lock only

    alt not owned  (step 3)
        DB-->>SC: DB_FALSE
        SC-->>C: unowned
        C-->>UI: unowned
        UI->>UI: applyButtonState(unownedState())
        UI-->>P: BUY enabled, Set as Default disabled
    else owned  (step 4)
        DB-->>SC: DB_TRUE
        SC-->>C: owned
        C-->>UI: owned
        UI->>UI: applyButtonState(ownedState())
        UI-->>P: BUY disabled, Set as Default enabled
    else read fails — DB_IO_ERROR  (ext 2a)
        DB-->>SC: DB_IO_ERROR
        SC-->>C: 500
        C-->>UI: error(500)
        UI->>UI: applyButtonState(bothDisabled())
        UI-->>P: show(transient error) — reselect to retry
    end

    deactivate DB
    deactivate SC
    deactivate C
    deactivate UI
```

### SD UC-17 — Deduct Wallet Points

```mermaid
sequenceDiagram
    participant SC as :StoreController
    participant DB as :Db
    participant TX as :PurchaseTxn
    participant L as :WriteLock
    participant W as :Wallet
    participant I as :Inventory
    participant R as :PlayerRecord
    participant LOG as :WalLog

    note over SC,DB: entered from UC-15 step 4 (db_buy_character)<br/>or UC-16 step 4 (db_buy_theme)

    SC->>DB: db_buy_*(id, itemId)
    activate DB
    DB->>TX: run()
    activate TX

    TX->>L: acquire()
    activate L
    note right of L: the whole check-and-act is ONE atom

    rect rgb(240, 240, 240)
        note over TX,I: step 2 — ordered prechecks
        TX->>R: lookup(playerId), catalogue.find(itemId)
        activate R
        R-->>TX: record | absent
        deactivate R
        TX->>I: owns(itemId)
        activate I
        I-->>TX: bool
        deactivate I
        TX->>W: canAfford(cost)
        activate W
        W-->>TX: bool
        deactivate W
        TX->>I: isFull()
        activate I
        I-->>TX: bool
        deactivate I
    end

    alt all checks pass  (step 3)
        TX->>R: debit(cost)
        activate R
        R->>W: debit(cost)
        R->>I: add(itemId)
        R-->>TX: updated record
        deactivate R
        note right of R: debit AND grant on the<br/>same player record — one write

        TX->>LOG: append(playerRecord)  %% step 4, whole record, LWW
        activate LOG
        alt append ok
            LOG-->>TX: DB_OK
            TX->>L: release()
            deactivate L
            TX-->>DB: DB_OK
            note over LOG: background flusher fdatasyncs <= 1 s —<br/>no blocking syscall under the lock
        else append fails  (E1)
            LOG-->>TX: DB_IO_ERROR
            TX-->>DB: DB_IO_ERROR
        end
        deactivate LOG
    else a precheck fails  (ext 2a)
        TX->>L: release()
        TX-->>DB: DB_NOT_FOUND | DB_EXISTS | DB_INSUFFICIENT | DB_FULL
        note right of TX: NO change to wallet or inventory
    end

    deactivate TX
    DB-->>SC: DbResult
    deactivate DB
```

### SD UC-18 — Set Default Character

```mermaid
sequenceDiagram
    actor P as Player
    participant MU as :MarketplaceUI
    participant SU as :SettingsUI
    participant C as :HtttpClient
    participant EC as :EquipController
    participant DB as :Db

    alt entry from Marketplace  (after UC-15)
        P->>MU: selectCharacter(cid) → pressSetAsDefaultCharacter(cid)
        activate MU
        MU->>C: equipCharacter(playerId, cid)
        deactivate MU
    else entry from Settings  (UC-20)
        P->>SU: pressChangeDefaultCharacter(cid)
        activate SU
        SU->>C: equipCharacter(playerId, cid)
        deactivate SU
    end

    activate C
    C->>EC: EQUIP /player/<pid>/character/<cid>
    activate EC
    EC->>DB: db_equip_character(id, cid)
    activate DB
    DB->>DB: writeLock.acquire()
    DB->>DB: owns(cid)?

    alt DB_OK  (step 3)
        DB->>DB: current_equipped_character = cid
        DB->>DB: walLog.append(playerRecord) then writeLock.release()
        DB-->>EC: DB_OK
        EC-->>C: 200 OK {equippedCharacter}
        C-->>SU: equipped(cid)
        SU-->>P: showDefaultCharacter(c) + showProfilePicture(c)
        note right of SU: step 4 — also used in-game<br/>(single-player HUD portrait, UC-10)
    else DB_NOT_OWNED  (ext 3a)
        DB->>DB: writeLock.release()
        DB-->>EC: DB_NOT_OWNED
        EC-->>C: 403 Forbidden
        C-->>MU: error(403)
        MU-->>P: show("not owned — buy it first")
        note right of MU: Player must Buy via UC-15
    else DB_NOT_FOUND
        DB-->>EC: DB_NOT_FOUND
        EC-->>C: 404 Not Found
        C-->>MU: error(404)
        MU-->>P: show("Error: No such character")
    end

    deactivate DB
    deactivate EC
    deactivate C
```

### SD UC-19 — Set Default Theme

```mermaid
sequenceDiagram
    actor P as Player
    participant MU as :MarketplaceUI
    participant SU as :SettingsUI
    participant GU as :GameplayUI
    participant C as :HtttpClient
    participant EC as :EquipController
    participant DB as :Db

    alt entry from Marketplace  (after UC-16)
        P->>MU: selectTheme(tid) → pressSetAsDefaultTheme(tid)
        activate MU
        MU->>C: equipTheme(playerId, tid)
        deactivate MU
    else entry from Settings  (UC-20)
        P->>SU: pressChangeDefaultTheme(tid)
        activate SU
        SU->>C: equipTheme(playerId, tid)
        deactivate SU
    end

    activate C
    C->>EC: EQUIP /player/<pid>/theme/<tid>
    activate EC
    EC->>DB: db_equip_theme(id, tid)
    activate DB
    DB->>DB: writeLock.acquire()
    DB->>DB: owns(tid)?

    alt DB_OK  (step 3)
        DB->>DB: current_equipped_theme = tid
        DB->>DB: walLog.append(playerRecord) then writeLock.release()
        DB-->>EC: DB_OK
        EC-->>C: 200 OK {equippedTheme}
        C-->>SU: equipped(tid)
        SU-->>P: showCurrentTheme(t)
        note right of SU: step 4 — Settings reflects it now
        opt next game start  (UC-10 / UC-11 / UC-12)
            C->>GU: applyTheme(theme.palette())
            GU-->>P: board + UI adopt the colour scheme
        end
    else DB_NOT_OWNED  (ext 3a)
        DB->>DB: writeLock.release()
        DB-->>EC: DB_NOT_OWNED
        EC-->>C: 403 Forbidden
        C-->>MU: error(403)
        MU-->>P: show("not owned — buy it first")
        note right of MU: Player must Buy via UC-16
    else DB_NOT_FOUND
        DB-->>EC: DB_NOT_FOUND
        EC-->>C: 404 Not Found
        C-->>MU: error(404)
        MU-->>P: show("Error: No such theme")
    end

    deactivate DB
    deactivate EC
    deactivate C
```

---

## Solution Class Diagram

### Method inventory by use case

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

### Solution diagram

```mermaid
classDiagram
    direction TB

    %% ---------------- UI layer (tetrisu) ----------------
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
        +pressChangeDefaultCharacter(cid : ItemId) void
        +pressChangeDefaultTheme(tid : ItemId) void
        +showDefaultCharacter(c : Character) void
        +showProfilePicture(c : Character) void
        +showCurrentTheme(t : Theme) void
        +show(msg : String) void
    }
    class GameplayUI {
        <<boundary>>
        +applyTheme(p : ColorScheme) void
    }

    %% ---------------- Boundary ----------------
    class HtttpClient {
        <<boundary>>
        +buyCharacter(playerId : PlayerId, cid : ItemId) HtttpResponse
        +buyTheme(playerId : PlayerId, tid : ItemId) HtttpResponse
        +ownsCharacter(playerId : PlayerId, cid : ItemId) HtttpResponse
        +ownsTheme(playerId : PlayerId, tid : ItemId) HtttpResponse
        +equipCharacter(playerId : PlayerId, cid : ItemId) HtttpResponse
        +equipTheme(playerId : PlayerId, tid : ItemId) HtttpResponse
    }

    %% ---------------- Controllers (tetrisd, stateless) ----------------
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

    %% ---------------- Domain (libmacminidb internals) ----------------
    class PlayerRecord {
        -playerId : PlayerId
        -username : String
        -walletPoints : long
        -gamesPlayed : int
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
    class WalLog {
        -path : String
        +append(rec : PlayerRecord) DbResult
        +flush() void
        +replay() void
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
        -abilities : bitfield
        +grants(level : int) Ability
    }
    class Theme {
        -themeId : ItemId
        -description : String
        +palette() ColorScheme
    }

    %% ---------------- Value objects ----------------
    class ButtonState {
        <<value object>>
        -buyEnabled : bool
        -setDefaultEnabled : bool
        +ownedState()$ ButtonState
        +unownedState()$ ButtonState
        +bothDisabled()$ ButtonState
    }
    class Ability {
        <<value object>>
        -level : int
        -cost : int
        -name : String
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
        +db_buy_character(id, cid) DbResult
        +db_buy_theme(id, tid) DbResult
        +db_equip_character(id, cid) DbResult
        +db_equip_theme(id, tid) DbResult
        +db_player_owns_character(id, cid) DbBool
        +db_player_owns_theme(id, tid) DbBool
        +db_get_character(cid) Character
        +db_get_theme(tid) Theme
    }

    %% ---------------- Enums ----------------
    class StoreTab {
        <<enum>>
        CHARACTERS
        THEMES
    }
    class DbResult {
        <<enum>>
        DB_OK
        DB_EXISTS
        DB_INSUFFICIENT
        DB_NOT_OWNED
        DB_FULL
        DB_NOT_FOUND
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
        FORBIDDEN_403
        NOT_FOUND_404
        CONFLICT_409
        SERVER_ERROR_500
    }

    %% ---------------- Relationships ----------------
    MarketplaceUI ..> HtttpClient : uses
    SettingsUI ..> HtttpClient : uses
    MarketplaceUI o--> StoreTab
    MarketplaceUI ..> ButtonState : UC-15a / UC-16a
    MarketplaceUI ..> Character : preview
    MarketplaceUI ..> Theme : preview
    SettingsUI ..> Character : default + pfp
    SettingsUI ..> Theme : current theme
    Theme ..> ColorScheme : palette
    GameplayUI ..> ColorScheme : applied at game start

    HtttpClient ..> StoreController : HTTTP BUY /store/...
    HtttpClient ..> EquipController : HTTTP EQUIP /player/...

    StoreController ..> Db : db_buy_* / db_player_owns_*
    StoreController ..> HtttpStatus : maps DbResult
    EquipController ..> Db : db_equip_*
    EquipController ..> HtttpStatus : maps DbResult

    Db ..> PurchaseTxn : delegates UC-17
    Db ..> Loadout : equip writes
    Db ..> Catalogue : item lookup
    Db ..> DbResult
    Db ..> DbBool : probe answers (owns / unknown)

    PurchaseTxn ..> WriteLock : one atom
    PurchaseTxn ..> Wallet : canAfford / debit
    PurchaseTxn ..> Inventory : owns / isFull / add
    PurchaseTxn ..> PlayerRecord : settle
    PurchaseTxn ..> WalLog : append whole record
    PurchaseTxn ..> DbResult : outcome

    PlayerRecord *--> "1" Wallet
    PlayerRecord *--> "2" Inventory : characters + themes
    PlayerRecord *--> "1" Loadout
    WalLog ..> PlayerRecord : LWW replay on boot

    StoreItem <|-- Character
    StoreItem <|-- Theme
    Catalogue o--> "0..*" Character : config/*.cfg
    Catalogue o--> "0..*" Theme
    Character ..> "4" Ability : grants
    Inventory o--> "0..64" StoreItem : capped at DB_MAX_OWNED
    Loadout --> "1" Character : equipped
    Loadout --> "1" Theme : equipped
    ButtonState ..> Inventory : derived from owns()
```
