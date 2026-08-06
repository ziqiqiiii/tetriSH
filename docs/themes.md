# tetriSH Themes

There are **4 different characters** available in the game: **Princess**, **Halloween**, **Wolf-man**, and **Mirurun**. See the [Tetris Battle Gaiden character list](https://tetris.wiki/Tetris_Battle_Gaiden) for reference.

These 4 characters remain default for the default theme. For every other theme, each character gets a different **nickname** and **character image**, while the character's **actual name and abilities remain the same**.

## Theme List

| # | Theme |
|---|---|
| 1 | Default |
| 2 | Design and AI |
| 3 | Do u wanna build a snowman? |
| 4 | Haaland |
| 5 | John Cena |
| 6 | Claude-ing |
| 7 | Al-Merqaedes |
| 8 | Nuclear Gandhi |

---

## 1. Default Theme

### Default Characters

The default theme uses each character's canonical name and full ability set.

A **Target** is the player an offensive ability lands on, as defined in
[CONTEXT.md](CONTEXT.md): Single mode has no Target and offensive abilities are
unavailable there; Double implies the one other player; Battle Royale draws one
per resolution from the room's seeded random source, among players still in the
game. Every cross-player effect is queued against its Target and applied at that
player's next piece lock — see
[ADR-0009](adr/0009-cross-player-effects-resolve-at-piece-lock.md).

`docs/use_cases.md` carries a second table of the same abilities, phrased as
server-enforced effects. The two are kept in step; this one is the source of
truth for ability text.

#### 1. Princess

| Level | Ability | Description |
|---|---|---|
| 1 | Sol | Fires a beam of light that clears three adjacent columns off the player's field. Can be directed. Has a 3-second timer before fired automatically. |
| 2 | Mirror | Steals the next crystal power used against the player. |
| 3 | Paralysis | Stops the Target from rotating their next 3 pieces. |
| 4 | Copy | Replaces the player's field with a copy of a Target's. |

#### 2. Halloween

| Level | Ability | Description |
|---|---|---|
| 1 | Fry | Fills the bottom 3 rows with blocks. These lines are cleared once the next piece is placed, and the lines are sent to the Target. Crystal blocks are converted into normal blocks and are not collected. |
| 2 | Dark | Blacks out the Target's playfield, and only a small section under the active piece is visible. |
| 3 | Vampire | Steals the Target's crystals. |
| 4 | Bomb | Destroys random blocks on the Target's field. |

#### 3. Wolf-man

| Level | Ability | Description |
|---|---|---|
| 1 | Cut | Clears the top 4 rows of the player's field. |
| 2 | Nue | Stops the Target from fast-dropping their next 4 pieces. |
| 3 | Pals | Lines sent by any opponent will lower the player's stack for a short time. That excludes lines sent through powers. |
| 4 | Thwack | For the player's next 4 pieces, non-crystal blocks will drop from any line cleared, allowing lower incomplete lines to also be cleared. |

#### 4. Mirurun

| Level | Ability | Description |
|---|---|---|
| 1 | Mirurun | Removes the bottom 4 rows of the player's field. The lines removed are not sent to a Target. |
| 2 | Inversion | Inverts the Target's controls for their next 3 pieces. |
| 3 | Pentaris | Sends five lines of garbage to the Target. |
| 4 | Sirtet | All rows containing blocks on the Target's field are inverted, so that spaces are converted to blocks and non-crystal blocks are converted to spaces. |

---

## 2. Design and AI

### Theme Color Scheme

| Color | Hex | RGB |
|---|---|---|
| White | `#FFFFFF` | (255, 255, 255) |
| Black | `#000000` | (0, 0, 0) |
| SUTD Red | `#A90B2C` | (169, 11, 44) |

> "We're the first Design and AI uni"
>
> "We don't teach AI as a tool, we teach it as a mindset that shaped by human-centered design"
> ([Reference](https://www.youtube.com/watch?v=ZF1BvtWpzc4))

### Character Nicknames

| Character | Nickname |
|---|---|
| Princess | Princess Fab.io |
| Halloween | Fab.io Halloween Edition |
| Wolf-man | Wolf-man Fab.io |
| Mirurun | Cute Fab.io |

---

## 3. Do u wanna build a snowman?

### Character Nicknames

| Character | Nickname |
|---|---|
| Princess | Princess Elsa |
| Halloween | Sven |
| Wolf-man | Olaf |
| Mirurun | Princess Anna |

---

## 4. Haaland

### Character Nicknames

| Character | Nickname |
|---|---|
| Princess | Princess Haaland 🎀 |
| Halloween | Haalandween |
| Wolf-man | Viking Haaland |
| Mirurun | Cute Netherlands supporters |

---

## 5. John Cena

### Character Nicknames

| Character | Nickname |
|---|---|
| Princess | Princess Cena 🎀 |
| Halloween | Halloween Cena |
| Wolf-man | Wolf-man Cena |
| Mirurun | Blushing Cena 👉👈 |

---

## 6. Claude-ing

### Theme Color Scheme

| Color | Hex | RGB |
|---|---|---|
| White | `#FFFFFF` | (255, 255, 255) |
| Dark | `#191919` | (25, 25, 25) |
| Claude Orange | `#C15F3C` | (193, 95, 60) |

### Character Nicknames

| Character | Nickname |
|---|---|
| Princess | Sister Gemini |
| Halloween | Cousin ChatGPT |
| Wolf-man | Uncle Deepseek |
| Mirurun | Brother Claude |

---

## 7. Al-Merqaedes

### Character Nicknames

| Character | Nickname |
|---|---|
| Princess | Princess Antonelli 🎀 |
| Halloween | Osama Bin Russel |
| Wolf-man | Toto Wolff-man |
| Mirurun | Blushing Talibanatolli 👉👈 |

---

## 8. Nuclear Gandhi

### Character Nicknames

| Character | Nickname |
|---|---|
| Princess | Princess Gandhi 🎀 |
| Halloween | Nuclear Gandhi |
| Wolf-man | Wolf-man Gandhi |
| Mirurun | Cute Gandhi 😙 |
