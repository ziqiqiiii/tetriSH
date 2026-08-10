# tetriSH Themes

The Settings catalogue currently exposes seven theme labels and their preview
thumbnails. This document records the canonical labels and the four-character
power reference used by the client fixture. Preview ownership is deliberately
mixed so the Settings screen can demonstrate its Marketplace flow: Haaland and
Clauding are locked, while the other five themes are owned. These fixture flags
do not add theme gameplay behavior.

## Theme List

The **catalogue id** is the one that travels: it is what `BUY` and `EQUIP`
name, what is written into a player's `owned_themes`, and what `tetrisu` keys
artwork on. It is never reused or renumbered, so it carries a gap — id `5` was
the John Cena theme, cut before it was drawn — and it is therefore not a
position in this table. The catalogue's spelling is `config/themes.cfg`'s.

| Catalogue id | Theme (catalogue spelling) | Cost |
|---|---|---|
| 1 | Default | Free (starting theme) |
| 2 | Design and AI | Free for SUTDents |
| 3 | Do u wanna build a snowman? | 5 |
| 4 | Haaland | 8 |
| 6 | Claude-ing | 7 |
| 7 | Al-Merqaedes | 15 |
| 8 | Nuclear Gandhi | 15 |

## Classic character and power reference

The four character entries and their four powers are the canonical character
material used by the Settings fixture. Mirurun and Halloween are owned in the
preview; Princess and Wolf-man remain visible but locked. See the
[Tetris Battle Gaiden character list](https://tetris.wiki/Tetris_Battle_Gaiden)
for the reference source.

A **Target** is the player an offensive ability lands on, as defined in
[CONTEXT.md](CONTEXT.md): Single mode has no Target and offensive abilities are
unavailable there; Double implies the one other player; Battle Royale draws one
per resolution from the room's seeded random source, among players still in the
game. Every cross-player effect is queued against its Target and applied at that
player's next piece lock.

`docs/use_cases.md` carries a second table of the same abilities, phrased as
server-enforced effects. The two are kept in step; this one is the source of
truth for ability text. The Settings fixture in `src/tetrisu/src/app_provider.c`
carries an abbreviated form of the same descriptions, sized for its card layout.

### 1. Princess

| Level | Ability | Description |
|---|---|---|
| 1 | Sol | Fires a beam of light that clears three adjacent columns off the player's field. Can be directed. Has a 3-second timer before fired automatically. |
| 2 | Mirror | Steals the next crystal power used against the player. |
| 3 | Paralysis | Stops the Target from rotating their next 3 pieces. |
| 4 | Copy | Replaces the player's field with a copy of a Target's. |

### 2. Halloween

| Level | Ability | Description |
|---|---|---|
| 1 | Fry | Fills the bottom 3 rows with blocks. These lines are cleared once the next piece is placed, and the lines are sent to the Target. Crystal blocks are converted into normal blocks and are not collected. |
| 2 | Dark | Blacks out the Target's playfield, and only a small section under the active piece is visible. |
| 3 | Vampire | Steals the Target's crystals. |
| 4 | Bomb | Destroys random blocks on the Target's field. |

### 3. Wolf-man

| Level | Ability | Description |
|---|---|---|
| 1 | Cut | Clears the top 4 rows of the player's field. |
| 2 | Nue | Stops the Target from fast-dropping their next 4 pieces. |
| 3 | Pals | Lines sent by any opponent will lower the player's stack for a short time. That excludes lines sent through powers. |
| 4 | Thwack | For the player's next 4 pieces, non-crystal blocks will drop from any line cleared, allowing lower incomplete lines to also be cleared. |

### 4. Mirurun

| Level | Ability | Description |
|---|---|---|
| 1 | Mirurun | Removes the bottom 4 rows of the player's field. The lines removed are not sent to a Target. |
| 2 | Inversion | Inverts the Target's controls for their next 3 pieces. |
| 3 | Pentaris | Sends five lines of garbage to the Target. |
| 4 | Sirtet | All rows containing blocks on the Target's field are inverted, so that spaces are converted to blocks and non-crystal blocks are converted to spaces. |

## Design AI University

The Settings catalogue provides this theme label and its preview thumbnail. No
theme-specific mechanics, prices, colors, or nicknames are assigned here.

## Do You Wanna Build a Snowman

The Settings catalogue provides this theme label and its preview thumbnail. No
theme-specific mechanics, prices, colors, or nicknames are assigned here.

## Haaland

The Settings catalogue provides this theme label and its preview thumbnail. No
theme-specific mechanics, prices, colors, or nicknames are assigned here.

## Al Merqaedes F1 Team

The Settings catalogue provides this theme label and its preview thumbnail. No
theme-specific mechanics, prices, colors, or nicknames are assigned here.

## Nuclear Ghandi

The Settings catalogue provides this theme label and its preview thumbnail. No
theme-specific mechanics, prices, colors, or nicknames are assigned here.

## Clauding

The Settings catalogue provides this theme label and its preview thumbnail. No
theme-specific mechanics, prices, colors, or nicknames are assigned here.
