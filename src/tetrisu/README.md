# tetrisu

The terminal game client for tetriSH, implemented in C on notcurses. It renders
the image-based home screen, a local playable Endless Solo Battle, and the
complete multiplayer approach — mode picker, room browser, create-room panel and
waiting room with chat and a pre-match countdown — while the authoritative
`tetrisd` game loop is being built.

---

## Table of Contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Build](#build)
- [Run](#run)
- [Controls](#controls)
- [Menu Items](#menu-items)
- [Assets](#assets)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Testing](#testing)
- [Adding a Screen](#adding-a-screen)
- [Authors](#authors)

---

## Features

- Image background rendered as a letterboxed 4 x 2 cell backdrop, with the
  selector bunny kept on its own high-resolution pixel plane
- Splash intro video streamed over the background, skippable with any key
- Bunny-sprite menu selector positioned from the rendered background geometry;
  scales with terminal size
- Kernel-sleeping keyboard input through notcurses' pollable input descriptor;
  arrow-key selection wraps around
- Optional background music and menu SFX via SDL2_mixer, with runtime volume control
- Cutesy Mirurun-and-speaker pixel-art volume feedback on Home and Solo:
  `MUSIC`, a rounded percentage, and a 16-step crystal bar. Movable and
  stationary bitmap renderers use the authored artwork, true cell mode uses an
  alpha-safe framed terminal presentation, and silent-audio builds retain the
  visual feedback
- Best-effort audio — missing device, assets, or SDL libraries degrade to silent, never fatal
- Audio compiled out entirely (`-DTETRISU_ENABLE_AUDIO=0`) when SDL2/SDL2_mixer are absent
- Endless 10 x 20 Solo play with SRS, seven-bag generation, next-three preview,
  ghost piece, Guideline move-reset lock delay (500 ms, 15 resets, from
  `libtetrisbrain` so the offline rules and `tetrisd` cannot disagree),
  modern scoring, and a guaranteed 350 ms
  view of the final board before any top-out panel appears
- Top-out panel that states the result: `NEW PERSONAL BEST` on a record,
  `BEST <score>` otherwise. The record pulses once to announce itself and then
  rests lit for as long as the panel is up, rather than fading away while the
  player is still reading it. Signed in, the record is the account's — read
  from `PROFILE`, written by `tetrisd` when it records the game, and the same
  number the leaderboard ranks on. Offline it falls back to this machine's own
  file under `$XDG_STATE_HOME`
- Responsive 4:3 Solo layout built from one 512 x 384 master canvas, fitted to
  the terminal without changing the HUD aspect ratio
- Transparent image HUD with exact `#2E222F` authored borders, 16 x 16
  tetromino sprites, custom text/number masks, and centered Mirurun art
- Low-resolution scenery plus independently refreshed high-resolution HUD
  planes; bitmap-safe backends use atomic pixel-piece planes with the exact
  authored tile sprites, while unsafe or unsupported backends use a
  true-colour quadrant-cell fallback
- Attractive compatibility mode for terminals without safe bitmap rendering:
  a visible mode badge, terminal-font menu labels, native terminal selector,
  4 x 2
  HOLD/NEXT/HUD art, a true-colour cell board, and native terminal text for
  score statistics and pause/top-out instructions
- Bounded input batches and a 30 FPS presentation ceiling coalesce rapid
  movement and rotation without delaying gameplay state or flooding the PTY
- Terminal-aware press/release handling with immediate taps, 167 ms DAS,
  33 ms ARR, 20x soft drop, last-pressed direction priority, and safe
  terminal-repeat fallback when release events are unavailable
- Dirty row/HUD signatures rebuild only changed content, while the compact
  control legend uses one crisp terminal-font row
- Responsive PTY geometry checks reflow both home and Solo between compact and
  full layouts without busy-waiting when a terminal does not report resize as
  input
- Interactive ten-segment Mirurun crystal meter that gains one charge per two
  cleared lines, exposes evenly spaced `2 / 4 / 6 / 8` ability thresholds, and
  supports hover descriptions, mouse clicks, and `1`-`4` hotkeys
- Local Mirurun level-one activation removes the bottom four settled rows;
  opponent-targeted levels two-four currently spend charge and show a clearly
  labelled Solo test effect without mutating the board
- Two-frame progressive clear animation: 200 ms through level 6, then
  175/150/125 ms at levels 7/8/9 and 100 ms from level 10 onward
- Dedicated Settings/Profile screen with live fixture profile, portrait,
  equipped character/theme, full catalogues, wallet, score, rank, and
  Settings-to-Marketplace routing
- Signed-in Settings navigates as a grid: the two inventory panels sit above
  the control row, arrows step through them, and `Enter` equips the focused
  character or theme. `[` / `]` remain a shortcut for cycling characters.
- Inventory slots render catalogue thumbnails from their asset paths. The
  character inventory is a four-column, one-row grid; the theme inventory is a
  four-column, two-row grid, with the seventh theme occupying the partial
  final row.
- Inventory focus uses a centred gold-outlined plate and label; equipped state
  is shown independently by a star. The focused item's full name appears in a
  compact panel header, while narrow theme slots use short fitted captions.
- The preview fixture intentionally locks Princess, Wolf-man, Haaland, and
  Clauding. Locked thumbnails are desaturated but remain focusable for visual
  inspection; `Enter` refuses to equip them and raises an `ITEM NOT OWNED`
  notification directing the user to the Marketplace. Every bitmap renderer,
  including foot/Sixel, uses dedicated Mirurun marketplace art; true cell mode
  uses an alpha-safe framed terminal presentation.
- Dedicated Marketplace screen over its own haunted shop-interior backdrop.
  The art is a scene rather than an authored frame, so every plate, border and
  button on the screen is drawn by the renderer against the quiet dark centre
  of the shop and nothing paints over the shelves at its edges.
- The Marketplace shows the wallet, best score and rank above two shelf panels
  - four characters and seven themes - a detail card for whatever the cursor
  last touched, and a Back / Buy / Volume control row. It navigates exactly
  like Settings: arrows step the grids, `↑` enters the shelves from the
  controls, `Tab` walks everything in one order.
- Every shelf tile states what it would cost: `FREE`, a price, `OWNED`, or
  `EQUIPPED`. Prices the wallet cannot cover are drawn in red and the tile
  caption is dimmed. The detail card spells the arithmetic out in full -
  price, current balance, and either the balance after the purchase or how
  much is still missing.
- `Enter` on a shelf tile buys a locked item and equips an owned one, which is
  what its caption and the Buy button's second line both promise. A purchase
  debits the wallet exactly once and never changes the equipped loadout;
  equipping stays a separate, deliberate step.
- Both go to `tetrisd` and the screen redraws from the profile it answers with,
  so the wallet, the owned flags and the equipped flags always move together
  and a purchase survives the screen closing. A refusal writes nothing at all.
- Results are reported on a line inside the control row rather than on a
  floating notification card, and volume changes are reported there too. A
  notification is a plane raised over the screen, and raising or dropping one
  damages the cells it covers, which makes a stationary protocol retransmit
  the full-screen bitmap underneath it - over every region plane, blanking the
  shelves, the card and the buttons until the next keystroke. Keeping every
  pixel this screen draws inside its own regions removes that failure, and the
  outcome of a purchase is already visible in the wallet and the tile.
- Nothing the user can do inside the Marketplace rebuilds its full-screen
  layer. The wallet is a region plane of its own for exactly that reason,
  while best score and rank - which cannot change here - stay in the static
  frame. That wallet region is also the leftmost card only, so no region ever
  reaches the top-right corner a notification would be raised into.
- Locked artwork is desaturated on the shelf and in the detail card, so the
  panels read as stock rather than as inventory.
- Multiplayer is four screens rather than a single lobby. `Multiplayer` opens a
  mode picker drawn on the home artwork; choosing Double or Battle Royale opens
  the room browser filtered to that mode, `M` cycles the filter without leaving
  the screen, and `C` opens a create-room panel that arrives with the mode
  already chosen.
- The room browser lists id, mode, players, state and owner, and refuses a join
  that cannot succeed with the reason on its status line - full, or already in
  game. A room id can also be typed directly, which ignores the list filter: an
  id is how a friend shares a room, and a filter the player happens to have set
  must not hide the room they were invited to. Both renderer tiers use the
  same six-row scrolling viewport, so every modeled room remains visibly
  selectable when the filter is set to every open room.
- The waiting room shows eight seats at a time with ready badges plus the full
  occupied/capacity count; Up/Down and Page Up/Page Down scroll the complete
  roster. It also has a status line and a chat column on the right. `C` opens
  the composer, and while it is open every printable key is text - so a message
  containing "s" cannot start the match.
- That chat column is the server's when there is a server. `net_chat.c` holds
  every pushed `CHAT` in a drop-oldest ring, and the panel draws the tail of
  it. Posting sends and appends **nothing** locally: `tetrisd` echoes the
  sender their own line along with everybody else's, so a client that also
  appended it would draw the message twice, and the copy it drew first would
  be the one without the server's ordering. The same column carries the
  server's own narration - who joined, who left, who owns the room now - with
  no author, because it is one feed with two writers rather than two lists to
  merge. With no such provider - an offline room - the local append is the
  feed, which is all an offline room can have.
- A line that crosses a reply is still filed. `net_request` reads the socket
  too, so a loop that only asked `net_pump` "did anything arrive?" would lose
  most of the feed - the same trap `applied_seq` documents for snapshots. The
  ring therefore counts every line ever received rather than the ones it still
  holds, and a screen compares that against what it last drew.
- A Double room auto-starts when both seats are filled and ready. Battle Royale
  supports capacities from 4 through 99, requires every occupied player to be
  ready, and waits for the room owner to press `S`. Both paths run the same
  five-second countdown, and un-readying during it cancels it.
- Entering a waiting room plays the short dialog acknowledgement once. It uses
  the menu-select mix level (48 rather than the normal gameplay 72), so the cue
  stays softer than match effects and remains governed by the shared volume.
- Lobby and waiting-room volume keys use the same floating music-volume card
  as Home and Solo; they do not replace room-list, readiness, countdown, or
  chat feedback.
- The countdown and the live room snapshot advance without input. The client
  refreshes the authoritative roster and room state every 500 ms, and the
  existing region signatures repaint only the seats or status that changed;
  the countdown keeps its own small once-per-second plane.
- The lobby, the create-room panel and the waiting room share one duel-hall
  backdrop and one set of region planes. Switching between them destroys the
  planes the previous screen owned, because leaving one behind would strand a
  region of the old screen on top of the new one.
- Notification cards are sized from the artwork's own aspect rather than a
  fixed column count, and their lettering comes from the shared glyph atlas
  fitted to the plaque interior measured from the art. On the stationary tier
  the card is composited over a snapshot of the backdrop before it is blitted:
  Sixel cannot draw one bitmap through another, so a card with transparent
  margins would otherwise punch its whole plane out to the terminal
  background.
- Moving the focus onto a character opens its powers card automatically, the
  way the Solo ability meters describe themselves. `I` still pins the card for
  the equipped character from anywhere. The four canonical crystal powers are
  sourced from
  [Tetris.wiki's Tetris Battle Gaiden reference](https://tetris.wiki/Tetris_Battle_Gaiden).
- Settings is keyboard-only: it disables pointer reporting on entry, so no
  hover or click reaches it, and the screens that use the pointer turn it back
  on when they are entered
- Settings caches its composed static layer — the v2 backdrop, profile,
  portrait, and stats — so a focus change never re-reads the portrait or
  redraws the text above it. The movable tier cuts region planes from that
  layer; the stationary tier stamps a full frame from it, because a Sixel
  plane laid over another sprixel is re-emitted whenever the plane beneath it
  is damaged and the two then blank each other out. Queued key repeats are
  drained into the state before the repaint, so a held arrow cannot outrun the
  screen.
- Settings keeps the large authored frame cached and repaints only compact
  controls, inventory focus/equipped overlays, ability card, or volume value
  when they change. Character and theme thumbnails are cached independently
  by their catalogue asset path.
- Offline Settings keeps only current-run local controls/status and never
  invents account identity, inventory, wallet, score, or rank
- `+` / `-` drive one volume. Menu and gameplay effects are scaled by it
  alongside the music, so the per-effect constants stay purely as the mix
  balance; at full volume the balance is exactly what it was before

HOLD is implemented under temporary local Solo authority. The
[migration guide](../../docs/tetrisu-local-to-tetrisd.md) describes how it
becomes server-authoritative without rewriting the renderer.

---

## Prerequisites

notcurses 3.0.5 or newer (render) is **required**; SDL2 and SDL2_mixer (audio) are
**optional** and compiled out when absent. This Makefile owns those
component-only dependencies. The repository Makefile owns GCC, make,
pkg-config, OpenSSL, Readline, and ncurses.

```bash
make deps                          # check/install tetrisu render + audio deps
make check-tetrisu-deps            # check only; compiles a tiny notcurses probe
```

Package installation may request sudo access. Use the following when system changes are not allowed.

```bash
make deps AUTO_INSTALL_DEPS=0      # check-only; fail instead of installing
```

If APT has no compatible `libnotcurses-dev`—including Debian releases whose
package is older than 3.0.5—the default `INSTALL_NOTCURSES_FROM_SOURCE=1` builds
notcurses from source. Ubuntu may need its `universe` repository;
RHEL-compatible systems need EPEL/CRB; Fedora and openSUSE Tumbleweed ship
`notcurses-devel`. Build it explicitly with:

```bash
make install-notcurses-from-source NOTCURSES_VERSION=v3.0.12
```

---

## Build

Build the `bin/tetrisu` binary with `make`:

```bash
make
```

Plain `make` first runs repository-level `make deps` unless invoked with
`DEPS_READY=1`, then checks Tetrisu dependencies and compiles with
`-Wall -Wextra -Werror`.

Makefile targets:

| Command      | Description                                        |
|--------------|----------------------------------------------------|
| `make`       | Check dependencies, then build `bin/tetrisu`       |
| `make run`   | Build and launch `tetrisu` immediately             |
| `make test`  | Build and run the unit tests                       |
| `make deps`  | Check/install render + audio dependencies          |
| `make clean` | Remove object files and test binaries              |
| `make fclean`| Remove object files, test binaries, and `bin/`     |
| `make re`    | Full rebuild (`fclean` + `all`)                    |

---

## Run

```bash
./bin/tetrisu
```

For deterministic UI testing of the signed-in fixture path, opt in explicitly:

```bash
TETRISU_UI_PREVIEW=1 TETRISU_RENDERER=cell ./bin/tetrisu
```

On Login, move focus to the visible `PREVIEW` button and press `Enter`, click
it, or press `P` while an action button is focused. This creates the marked
`LOCAL UI PREVIEW` session with `navigation.offline=false`, so Home,
Leaderboard, Settings, and Settings → Marketplace are reachable. Without
`TETRISU_UI_PREVIEW=1`, the preview action is not rendered or accepted; real
server sign-in and Play Offline retain their existing behavior.

Every screen below Home is several keystrokes deep, which makes checking one in
a real terminal slow and an automated visual pass fragile. `TETRISU_START_SCREEN`
boots straight into one — the real screen, loading its real model through the
provider and leaving through its own Back route, not a preview of it:

```bash
TETRISU_UI_PREVIEW=1 TETRISU_START_SCREEN=lobby ./bin/tetrisu
```

It accepts `solo`, `marketplace`, `settings`, `leaderboard`, `multiplayer`,
`lobby`, `create`, `room`, `double`, and `royale`; anything else is ignored and
the app boots at Login as usual. `TETRISU_MATCH_PREVIEW=double|battle` is the
older, narrower gate for the two match screens, and
`TETRISU_MATCH_PREVIEW_SKIP_SELECTION=1` skips their character select.

By default the network adapter stays disconnected, so `CHECK SERVER` reports
offline and `LOGIN`/`SIGN UP` stay refused; `PLAY OFFLINE` and the preview gate
above are the two ways into Home. A refused button says which case applies on
the status line rather than doing nothing. Opting into a real `tetrisd` session
is described in [Network mode](#network-mode) below.

Or build and run in one step:

```bash
make run
```

Renderer selection defaults to `auto`, which probes the terminal once and
picks one of three tiers:

| Tier | Chosen for | Presentation |
|---|---|---|
| movable | kitty and Ghostty — Kitty-protocol terminals measured to free a replaced image | Authored bitmaps everywhere; the selector and the falling piece are their own planes and slide |
| stationary | Sixel and the Linux framebuffer (foot, XTerm, mlterm, VTE ≥ 0.78, `/dev/fb0`), plus other image-registry terminals such as WezTerm, iTerm2, Konsole, and contour | The same authored bitmaps, but no bitmap is ever moved: the board is flattened into one image redrawn in place, and the selector is destroyed and blitted again at its new row |
| cell | Terminals reporting no bitmap support | True-colour terminal cells throughout, with the `:: COMPATIBILITY MODE ::` badge |

Two independent properties decide this, and conflating them is what previously
sent perfectly capable terminals to the cell renderer:

**Can a bitmap plane move?** Only on the Kitty protocols. notcurses implements
sprixel movement in `kitty_move` and leaves `ti->pixel_move` NULL for both
`setup_sixel_bitmaps()` and `setup_fbcon_bitmaps()`, and redisplaying a Sixel
cannot write transparency over what is already on screen. So Sixel terminals
draw bitmaps, just never moving ones — no version of foot changes that, since
foot implements the Kitty *keyboard* protocol but not the graphics one. Note
that this bans `ncplane_move_yx()` on a bitmap plane, not bitmaps that change
position: destroying a plane and blitting a new one damages the cells the old
one held and repaints them from the layer below, which is an ordinary render
rather than the sprixel wipe Sixel cannot honour. Both the board and the menu
selector rely on that distinction to keep their artwork here.

**Does memory stay bounded?** Only the Kitty and iTerm2 protocols hand the
terminal an image registry that a buggy terminal can grow without bound. Sixel
and the framebuffer paint straight into the grid and keep nothing. The backend
enum cannot grade the registry terminals, because notcurses reserves
`NCPIXEL_KITTY_ANIMATED` and `NCPIXEL_KITTY_SELFREF` for kitty itself and drops
every other Kitty-graphics terminal onto `NCPIXEL_KITTY_STATIC`. Measured-good
terminals move bitmaps; every other detected registry backend takes the
stationary tier, which retransmits on board change rather than once per frame.

Any tier can be forced, which is how an unmeasured terminal is tried or a
suspected rendering bug is bisected:

```bash
TETRISU_RENDERER=cell make run        # terminal cells only
TETRISU_RENDERER=stationary make run  # bitmaps, never moved
TETRISU_RENDERER=pixel make run       # bitmaps, freely moved
```

The accepted values are `auto`, `cell`, `stationary`, and `pixel`. Missing,
empty, or unrecognised values behave like `auto`. Forcing `pixel` on a terminal
whose bitmap registry is not measured is exactly the case the automatic gate
avoids: watch the process's memory while a game runs before trusting it.

`tetrisu` requires a real terminal: notcurses queries palette, pixel geometry,
and graphics-protocol support at startup. It exits with
`notcurses_core_init failed` when `$TERM` has no usable terminfo entry.

### Network mode

Opt the client into a live `tetrisd` session with `TETRISU_NET=1`:

```bash
make certs                                 # dev CA + server cert (once)
make run                                   # tetrish sources .tetrishrc, starts tetrisd
TETRISU_NET=1 ./bin/tetrisu                # opt into the network provider
```

`make run` sources `.tetrishrc`, which exports `TETRISU_HOST`, `TETRISU_PORT`,
and `TETRISU_CA_PATH` and starts `tetrisd` on `4242`; launching tetrisu from
inside `tetrish` inherits them. Outside `tetrish`, source `.tetrishrc` first
or export the three keys by hand. The auth screen's `domain` field overrides
the host (or `host:port`) typed there, falling back to the env defaults when
blank.

`CHECK SERVER` opens the TCP session and runs the libtetrissh handshake; once
it reports `SERVER ONLINE`, `SIGN UP` registers a player and `LOGIN` binds the
connection. Single Player then plays Solo against `tetrisd` — every board
mutation is server-authoritative and the renderer draws only `STATE` snapshots.
Multiplayer's lobby and create/join paths are driven by `LIST`/`JOIN`. Once
seated, the waiting room polls the read-only `LIST /room/<name>` snapshot,
sends `LEAVE` before navigating back, and sends `START` before launching a
match. The snapshot carries the ordered roster, owner, readiness, capacity,
mode, and room state; refresh never joins the room again.

| Screen | Server reach today |
|---|---|
| Login / Sign Up, Settings, Marketplace, catalogues, Single Player, Leaderboard, Multiplayer lobby, create/join, and waiting-room roster/start/leave | Driven by `tetrisd` |
| Double / Battle Royale match presentation | Match screens remain scaffolded; room start and server game allocation are live |

Settings and the Marketplace read `LIST /store` for the catalogue and its
prices and `PROFILE` for the wallet, rank, inventory and loadout; `Enter` on a
shelf tile sends `BUY` or `EQUIP` and redraws from the profile the server
answers with. The client sends an item id and nothing else — it never names a
price, asserts what it owns, or decides what is equipped, because the equipped
character is what `tetrisd` reads to decide which Gaiden abilities a player
has.

Artwork is the one half that stays local: portraits, theme previews and
ability copy are files on this machine, matched to catalogue rows by id in
`catalogue_art.c`. Id rather than name, because a theme can be renamed
server-side, and rather than position, because catalogue ids carry gaps.

Signing in twice is a supported path, not an accident of backing out: identity
belongs to the connection, so `tetrisd` refuses a second `LOGIN` on a bound
socket with `409`, and the provider dials a fresh one rather than passing that
refusal to the form as a bad password
([`docs/bugs/`](../../docs/bugs/the_auth_screen_kept_a_session_it_could_not_reuse.md)).

`net_client.c` mutes `stdout` and `stderr` for the length of the handshake.
The certificate report is printed by the frozen `common.c`, which cannot be
changed, and it lands on the screen notcurses owns
([`docs/bugs/`](../../docs/bugs/the_handshake_printed_onto_the_board.md)). Set
`TETRISU_NET_LOG=<path>` to keep it instead of discarding it.

---

## Controls

| Key | Action |
|---|---|
| Any key | Dismiss the splash and enter the main menu |
| `↑` / `↓` | Move the menu selection (wraps at the ends) |
| `Enter` | Select the highlighted item |
| Mouse hover | Highlight the menu item under the pointer |
| Left click | Select the menu item under the pointer |
| `Esc` | Return to the parent screen |
| `Esc` in sign-in modal | Dismiss the modal |
| `Tab` / `←` / `→` in modal | Toggle focus between Back and Sign In |
| `Enter` in modal | Confirm the focused button |
| Mouse click in modal | Confirm the clicked button |
| `L` / `S` / `O` on Entry | Open Login / Sign Up / play offline |
| `←` / `→` or `1` / `2` on the mode picker | Choose Double or Battle Royale |
| `Enter` on the mode picker | Open the lobby filtered to that mode |
| `↑` / `↓` in the Lobby | Move the room cursor (clamps at the ends) |
| `Enter` in the Lobby | Join the room under the cursor |
| `→` / `Tab` in the Lobby | Focus the join-by-id field; every printable key is then text |
| `Enter` in the join field | Join the typed room id, whatever the list filter is |
| `Esc` in the join field | Step back to the room table |
| `C` / `R` / `M` / `B` in the Lobby | Create room / refresh / cycle the mode filter / back |
| `↑` / `↓` or `1` / `2` in Create Room | Choose the room's mode |
| `Enter` / `Esc` in Create Room | Create the room / cancel back to the lobby |
| `R` in the Waiting Room | Toggle your ready flag |
| `S` in the Waiting Room | Start, when the room's conditions are met and you own it |
| `↑` / `↓` or Page Up / Page Down in the Waiting Room | Scroll the player roster |
| `C` or `Enter` in the Waiting Room | Open the chat composer; `Esc` closes it |
| `Enter` in the chat composer | Post the message |
| `L` or `Esc` in the Waiting Room | Ask for confirmation, then leave back to the lobby only on Yes |
| `+` / `=` | Raise music volume one step |
| `-` / `_` | Lower music volume one step |
| `q` | Open the safe-default No/Yes quit confirmation; exit only on Yes |
| `←` / `→` in the Settings control row | Cycle Back, Marketplace, Volume -, Volume + (Settings takes no mouse input) |
| `↑` from the Settings control row | Focus the characters catalogue |
| `←` / `→` at an inventory panel edge | Cross between the characters and themes panels |
| `↓` past the last inventory row | Drop back to the control row |
| `Enter` on an inventory slot | Equip an owned item; locked items show Marketplace guidance |
| Focus on a character slot | Opens that character's powers card automatically |
| `Tab` in Settings | Step through every slot and control in one order |
| `M` in signed-in Settings | Open Marketplace; `Enter` on the visible button does the same |
| `[` / `]` (or `,` / `.`) in signed-in Settings | Select the previous / next owned character |
| `I` in signed-in Settings | Toggle the selected character's four-power info card |
| `←` / `→` in the Marketplace control row | Cycle Back, Buy, Volume -, Volume + (the Marketplace takes no mouse input) |
| `↑` from the Marketplace control row | Focus the character shelf |
| `Enter` on a Marketplace shelf tile | Buy it when locked, equip it when already owned |
| `B` in the Marketplace | Buy the item the detail card is describing |
| `E` in the Marketplace | Equip the item the detail card is describing |
| `Tab` in the Marketplace | Step through every shelf tile and control in one order |
| `P` on Login with `TETRISU_UI_PREVIEW=1` | Sign into the clearly marked local UI fixture |
| `Enter` on Single Player | Start local Endless Solo |
| `←` / `→` | Move the active piece |
| `↑` or `X` | Rotate clockwise |
| `Z` | Rotate counter-clockwise |
| `↓` | Soft drop; 1 point per descended cell |
| `Space` | Hard drop and lock; 2 points per descended cell |
| `C` | Hold or swap the active piece once before it locks |
| `1` | Mirurun (2 charge): remove the bottom four settled rows |
| `2` | Inversion (4 charge): visual-only Solo test activation |
| `3` | Pentaris (6 charge): visual-only Solo test activation |
| `4` | Sirtet (8 charge): visual-only Solo test activation |
| Mouse hover/click | Show an ability description / activate its meter circle |
| `P` | Pause/resume Solo |
| `R` | Restart after top-out |
| `Esc` or `Q` | Ask for confirmation, then return from Solo to Home only on Yes |

Single Player opens the playable local mode. In offline mode, Multiplayer,
Marketplace, and Leaderboard open a sign-in-required modal with Escape to
dismiss and Sign In to navigate to the Login screen. When authenticated or in
fixture mode, those items open their dedicated or scaffold screens marked
`LOCAL UI PREVIEW`. Leaderboard has its complete top-ten presentation:
`R` refreshes, `Esc` returns Home, Left/Right or Tab changes the focused button,
and both Back and Refresh support mouse hover/click.

---

## Menu Items

| Item | Status |
|---|---|
| `Single Player` | Playable local Endless mode; can remain as offline play |
| `Multiplayer` | Mode picker, room browser, create-room panel and waiting room with chat and a pre-match countdown; the match itself is still a scaffold |
| `Marketplace` | Dedicated shop screen: browse both catalogues, buy with wallet points, equip what is owned |
| `Leaderboard` | Complete top-three podium and positions 4–10, with refresh/error states |
| `Settings` | Dedicated live Profile/Settings screen; offline mode shows local controls only |

---

## Assets

Asset paths are compile-time macros resolved against `ASSET_DIR`, which the
Makefile sets to `src/tetrisu/assets`. The client loads:

| Macro | Role |
|---|---|
| `SPLASH_ASSET_PATH` | Clean home-screen artwork; five exact labels are rasterized from the shared pixel font at runtime |
| `LEADERBOARD_BACKGROUND_PATH` | Dedicated 1448 x 1086 leaderboard backdrop; live data and controls are composited over it with the shared pixel font |
| `MULTIPLAYER_ASSET_PATH` | Dedicated 1448 x 1086 duel-hall backdrop shared by the lobby, the create-room panel and the waiting room; the mode picker keeps the home artwork instead |
| `BUNNY_ASSET_PATH` | Bunny selector sprite (PNG with alpha) |
| `INTRO_VIDEO_PATH` | MP4 splash intro streamed over the background |
| `INTRO_AUDIO_PATH` | MP3 played once alongside the intro |
| `HOME_BGM_PATH` | Looping home-screen background music |
| `SOLO_BACKGROUND_PATH` | Full-screen Solo Battle background |
| `DEFAULT_HUD_PATH` | Transparent 512 x 384 Solo HUD/frame |
| `DEFAULT_TILE_PATH` | Guideline-color tiles, garbage, and two clear frames |
| `DEFAULT_MIRURUN_PATH` | Solo character portrait, centered in its panel |
| `SETTINGS_BACKGROUND_PATH` | Cached 1448 x 1086 Settings/Profile backdrop (`default_theme/settings_profile_background_v2.png`) |
| `HALLOWEEN_PORTRAIT_PATH`, `PRINCESS_PORTRAIT_PATH`, `WOLFMAN_PORTRAIT_PATH` | Character inventory thumbnails; Mirurun uses `DEFAULT_MIRURUN_PATH` |
| `SETTINGS_THEME_CLASSIC_PREVIEW_PATH` | `settings_previews/theme_classic.png` (192 x 192 Classic thumbnail) |
| `SETTINGS_THEME_DESIGN_AI_UNIVERSITY_PREVIEW_PATH` | `settings_previews/theme_design_ai_university.png` (192 x 192 Design AI University thumbnail) |
| `SETTINGS_THEME_SNOWMAN_PREVIEW_PATH` | `settings_previews/theme_snowman.png` (192 x 192 Do You Wanna Build a Snowman thumbnail) |
| `SETTINGS_THEME_HAALAND_PREVIEW_PATH` | `settings_previews/theme_haaland.png` (192 x 192 Haaland thumbnail) |
| `SETTINGS_THEME_AL_MERQAEDES_PREVIEW_PATH` | `settings_previews/theme_al_merqaedes.png` (192 x 192 Al Merqaedes F1 Team thumbnail) |
| `SETTINGS_THEME_NUCLEAR_GHANDI_PREVIEW_PATH` | `settings_previews/theme_nuclear_ghandi.png` (192 x 192 Nuclear Ghandi thumbnail) |
| `SETTINGS_THEME_CLAUDING_PREVIEW_PATH` | `settings_previews/theme_clauding.png` (192 x 192 Clauding thumbnail) |
| `MARKETPLACE_BACKGROUND_PATH` | `default_theme/marketplace_background.png` (1448 x 1086 haunted shop interior; quiet dark centre, detail at the edges) |
| `VOLUME_NOTIFICATION_PATH` | Mirurun-and-speaker pixel-art volume card |
| `OWNERSHIP_NOTIFICATION_PATH` | Mirurun marketplace-stall pixel-art ownership-error card |
| `SHARED_FONT_MASK_PATH` | White alpha mask for all HUD text |
| `SHARED_NUMBERS_MASK_PATH` | White alpha mask for digits and `+`/`-` |
| `MENU_MOVE_SFX_PATH` | Sound on up/down selection movement |
| `MENU_SELECT_SFX_PATH` | Sound on selection confirmation |

Missing assets are non-fatal: missing leaderboard art falls back to its
self-contained cell presentation, a missing bunny sprite falls back to a text
marker, and missing audio files are silently skipped. The exact authored-HUD
geometry, including the board-border export contract, is documented in
[`assets/solo_hud_art_template.md`](assets/solo_hud_art_template.md).

---

## Architecture

`main()` builds the render context, plays the intro, then dispatches input to
the render and audio modules:

```
render_init (background)
     │
     ▼
render_intro_play          stream INTRO_VIDEO_PATH; skippable, best-effort audio
     │
     ▼
render_auth_show           Login / Sign Up / Play Offline; native-text fields
     │
     ▼
render_menu_create         after authentication/offline entry, draw home menu
     │
     ▼
  screen loop              render_wait_input → validated navigation:
     ├── ↑/↓   menu_move_selection + render_menu_move_bunny + move SFX
     ├── Enter Single Player -> solo_mode_run -> return to menu
     ├── Enter Leaderboard -> dedicated top-ten screen + refresh/back
     ├── Enter Settings -> live Profile/Settings screen + local controls
     ├── Enter Multiplayer -> mode picker -> lobby -> create room / waiting room
     ├── Enter other item -> typed LOCAL UI PREVIEW scaffold
     ├── Esc    explicit parent screen
     ├── +/-   audio_volume_up / audio_volume_down
     └── q     APP_SCREEN_QUIT
```

Modules (each a `.c` under `src/`):

| File | Responsibility |
|---|---|
| `main.c` | Entry point; wires render + audio and runs the input loop |
| `app_state.c` | Validate the complete screen graph, Back routes, menu state, and labels |
| `app_provider.c` | Typed screen models and marked local fixture provider |
| `settings_screen.c` | Pure Settings focus traversal and semantic actions |
| `render_settings.c` | Live profile/inventory/settings panel; chooses the pixel renderer or self-contained cell fallback |
| `render_settings_font.c` | Composes the cached v2 Settings frame, catalogue thumbnails, and focus/equipped overlays |
| `leaderboard_screen.c` | Pure leaderboard focus and action handling |
| `multiplayer_screen.c` | Pure mode-picker and create-room state, actions, and card copy |
| `lobby_screen.c` | Pure room-browser state: filter, cursor, join-by-id field, and join guards |
| `waiting_room_screen.c` | Pure ready/start policy, chat transcript, and the pre-match countdown |
| `net_chat.c` | The room feed: the drop-oldest ring pushed lines land in, and posting one |
| `confirmation.c` | Pure safe-default Yes/No confirmation state and copy |
| `render_confirmation.c` | Persistent confirmation plane and keyboard interaction loop |
| `multiplayer_layout.c` | Reference-space geometry for all four multiplayer surfaces |
| `render_multiplayer.c` | Chooses the multiplayer compositor or the self-contained cell fallback |
| `render_multiplayer_font.c` | Composes the static frame and the region planes for all four multiplayer screens |
| `leaderboard_presentation.c` | Pure leaderboard background policy, reference-space geometry, and pointer hit testing |
| `auth_form.c` | UTF-8 auth input, masking, focus, validation, and provider submission |
| `render_background.c` | notcurses init, background blit, `render_wait_key`, teardown |
| `renderer_policy.c` | renderer environment parsing and forced compatibility policy |
| `render_intro.c` | Splash video streaming with skip-on-input |
| `render_menu.c` | Bunny selector plane and on-screen messages |
| `render_auth.c` | Pixel-art login/sign-up frame and terminal-only fallback |
| `render_screen.c` | Shared native-terminal shell and plane cleanup |
| `render_leaderboard.c` | Selects the high-fidelity pixel presentation or the themed cell-only fallback |
| `render_leaderboard_font.c` | Composes the shared-font title, podium cards, ranked rows, statuses, and controls into the leaderboard bitmap |
| `audio.c` | Optional SDL2_mixer music and SFX; no-ops when audio is compiled out |
| `solo_game.c` | Pure local session state/timing; temporary authority boundary |
| `solo_abilities.c` | Mirurun metadata, charge spending, board transform, and mouse geometry |
| `render_solo.c` | Own terminal planes, dirty signatures, and responsive layout |
| `render_solo_canvas.c` | Load assets and compose pixel-perfect HUD/board canvases |
| `solo_mode.c` | Run the poll-driven local Solo input and render loop |

`app_state.c` and `app_provider.c` are pure logic with no rendering or audio
dependencies. They join `solo_game.c` in `obj/logic.a`, which unit tests link
without notcurses or SDL.

---

## Project Structure

```
tetrisu/
├── src/
│   ├── main.c                 Entry point + input loop → bin/tetrisu
│   ├── app_state.c            Pure validated screen graph → logic.a
│   ├── app_provider.c         Typed models + local fixture provider
│   ├── leaderboard_screen.c   Pure leaderboard focus/actions → logic.a
│   ├── leaderboard_presentation.c  Pure bitmap geometry/hit-test policy → logic.a
│   ├── multiplayer_screen.c   Pure mode-picker + create-room state → logic.a
│   ├── lobby_screen.c         Pure room-browser state → logic.a
│   ├── waiting_room_screen.c  Pure ready/chat/countdown policy → logic.a
│   ├── net_chat.c             Room feed ring + CHAT send → logic.a
│   ├── multiplayer_layout.c   Pure multiplayer bitmap geometry → logic.a
│   ├── auth_form.c            Pure authentication form state → logic.a
│   ├── render_background.c    notcurses init, background, input, teardown
│   ├── renderer_policy.c      Renderer environment and compatibility policy
│   ├── render_intro.c         Splash video streamer
│   ├── render_menu.c          Bunny selector + messages
│   ├── render_auth.c          Auth artwork overlay + cell fallback
│   ├── render_screen.c        Native-terminal screen scaffold
│   ├── render_settings.c      Profile/settings screen + controls
│   ├── render_multiplayer.c   Pixel/cell multiplayer renderer selection
│   ├── render_multiplayer_font.c   Multiplayer static frame + region planes
│   ├── render_leaderboard.c   Pixel/cell leaderboard renderer selection
│   ├── render_leaderboard_font.c  Full shared-font leaderboard compositor
│   ├── render_solo.c          Solo planes, layout, and dirty-region updates
│   ├── render_solo_canvas.c   Asset loading + pixel-canvas composition
│   ├── solo_mode.c            Poll-driven local Solo loop
│   ├── solo_game.c            Pure local gameplay session
│   └── audio.c                Optional SDL2_mixer audio
├── scripts/
│   ├── install_deps.sh        Install render/audio packages per OS
│   ├── install_notcurses.sh   Build + install notcurses from source
│   ├── check_deps.sh          Compile/link probe for render/audio deps
│   ├── deps.sh                Check/install/re-check dependency orchestration
│   └── run_tests.sh           Formatted unit-test runner
├── assets/                    Images / video / audio (ASSET_DIR)
├── tests/test_*.c             Unit tests (each with its own main)
├── obj/                       Generated objects + logic.a
├── bin/tetrisu                Generated binary
├── tetrisu.h                  Public header (reached via -I.)
└── Makefile
```

---

## Testing

```bash
make test
```

The Settings and preview-auth tests are included in that command. For a
manual signed-in Settings check, run the preview command above, choose
`PREVIEW` on Login, and select Settings on Home. Confirm the v2 profile
backdrop and portrait appear, each inventory slot shows its thumbnail, and the
character panel is one row of four while the theme panel is two rows of four
with a three-slot final row. Move focus between slots and verify the focused
selection plate/gold label is distinct from the equipped star, equip a
character and a theme with `Enter`, then press `Enter` on Princess, Wolf-man,
Haaland, or Clauding and confirm the item stays unequipped while the top-right
Marketplace notice appears. Repeat with `TETRISU_RENDERER=cell` to confirm its
compatibility presentation. Hold an arrow to confirm focus tracks the last
key, press `I`, confirm the mouse does nothing there, resize the terminal, and
open Marketplace before pressing Back. For offline separation, omit the
environment variable, choose Play Offline, open Settings, and confirm that
only local status, renderer mode, and music volume appear.

For the Marketplace, open it from Home or from Settings and confirm the shop
backdrop appears with the wallet, both shelves, the detail card, and the
control row drawn over its dark centre rather than over the shelf artwork.
Step across both grids and confirm the detail card follows the cursor and
keeps describing the same item after `↓` drops focus to the control row. Buy
Princess, confirm the wallet drops by exactly 10, the tile turns `OWNED`,
and the equipped character does **not** change; press `E` to equip it. Then
buy Wolf-man to empty the wallet and confirm a further purchase is refused
with the `NOT ENOUGH` card and no balance change. Resize mid-screen, then
repeat the whole pass under `TETRISU_RENDERER=cell`.

Buying, equipping and changing the volume must each leave the shelves, the
detail card and the buttons on screen. None of them may repaint the whole
screen: on a Sixel terminal a full-screen bitmap re-emitted over the region
planes blanks them until the next keystroke, which is the fifth bug in
`docs/adding-a-screen.md`.

The unit suite covers the notcurses/SDL-free app and Solo game state. Three
integration suites boot a throwaway `tetrisd` — one server bring-up, shared by
all three as [`tests/integration/lib/tetrisd_fixture.sh`](tests/integration/lib/tetrisd_fixture.sh) —
and drive a real socket against it:

| Suite | Client | What it covers |
|---|---|---|
| `test_net_solo.sh` | `net_smoke` | The wire: `JOIN`/`START`, every gameplay action, `STATE` decoded into the Solo view model |
| `test_net_provider.sh` | `net_provider_smoke` | The provider vtable the UI calls: CHECK SERVER, SIGN UP, LOGIN, signing in *again*, profile, leaderboard, lobby, create/join, live roster refresh, owner-only start, leave, and room re-entry |
| `test_solo_authority.sh` | `solo_authority_smoke` | The layer `solo_mode.c` calls: who owns the board, and the hold that stops `tetrisd`'s clock for the length of the client's 3-2-1 |
| `test_net_chat.sh` | `chat_smoke` | The room feed: narration nobody asked for, a line echoed back to its sender, a line that crosses a reply still being filed, the server's refusals, and the provider seam the waiting room calls |

The fixture walks its port upward from the suite's base rather than using a
fixed one: a fixed port inside the kernel's ephemeral range collides with
whatever else the machine is doing, and the suite fails for a reason that has
nothing to do with `tetrisu`.

Run the strict component build without allowing dependency installation with:

```bash
make clean DEPS_READY=1 AUTO_INSTALL_DEPS=0
make all test DEPS_READY=1 AUTO_INSTALL_DEPS=0
```

For a native macOS ownership check, launch the full client through Apple
`leaks`, exercise Solo repeatedly, then quit normally:

```bash
MallocStackLogging=1 leaks -atExit -- ./bin/tetrisu
```

On Linux, run the mandatory PR ownership check with Valgrind and quit the
interactive client normally:

```bash
valgrind --leak-check=full --show-leak-kinds=all \
  --error-exitcode=1 ./bin/tetrisu
```

The Solo suite includes a 100,000-rotation state stress case and passes under
AddressSanitizer and UndefinedBehaviorSanitizer. The live WezTerm regression
also drives a 10,000-rotation burst followed by sustained paced movement and
rotation. Process memory remains bounded, the terminal queue drains normally,
and Apple `leaks` reports `0 leaks for 0 total leaked bytes`. The renderer
requires a real terminal for ownership checks because notcurses queries pixel
geometry and graphics-protocol capabilities during startup.

---

## Adding a Screen

[`docs/adding-a-screen.md`](docs/adding-a-screen.md) is the guide for building a
new screen that stays responsive on every renderer tier. It covers splitting a
screen into a cached static layer and small region planes, the signature rules
that decide what recomposes, why bitmap planes are written in place instead of
replaced, input coalescing, and the five rendering bugs the Settings and
Leaderboard screens hit on the way there.

The short version: a keystroke must repaint one region, never the screen.
Whole-screen repaints cost 30–80 ms on a stationary protocol, and because the
input loop is serial that cost becomes the input rate ceiling.

---

## Authors

| Role | Member | Owns |
|---|---|---|
| Tetrisu / Tetrisd | Sanjan Krishna Sarat | Client rendering, shell, game server |
| Tetrisu / Core libraries | Thong Zi Qi | Client integration, protocols, shared libraries |

---

*50.003 × 50.005 CoreStack Challenge — SUTD, Summer 2026*
