# Game Economics Design Decisions

---

## Purpose

tetriSH tetris game that consist of Single Player Mode, Double Players Mode, and Battle Royale Mode. After each game, players earn **points**, and
those points feed two things we track for each player:

| Value | What it is | What it's for |
|---|---|---|
| `leaderboard_scores` | The player's personal highest (best) single-game score | Ranking / bragging rights only |
| `lifetime_points` | Every point ever scored, added up | Nothing ranks on it; it is what the wallet's rate is charged against |
| `wallet_points` | `lifetime_points ÷ 100`, less everything bought | The currency to buy new characters or themes |

The first two are deliberately separate. Ranking on a running total ranks
whoever played *most*, not whoever played *best* — a player grinding
100-point games passes one who scored 5,000 once, and skill cannot catch up
with persistence. The wallet still needs a running total, so it gets its own.

---

## Scoring model

This is the points allocation for the game, scaled by the current `level`:

| Action | Points |
|---|---|
| Single | 100 × level |
| Double | 300 × level |
| Triple | 500 × level |
| Tetris | 800 × level *(difficult)* |
| Mini T-Spin, no lines | 100 × level |
| T-Spin, no lines | 400 × level |
| Mini T-Spin Single | 200 × level *(difficult)* |
| T-Spin Single | 800 × level *(difficult)* |
| Mini T-Spin Double *(if present)* | 400 × level *(difficult)* |
| T-Spin Double | 1200 × level *(difficult)* |
| T-Spin Triple | 1600 × level *(difficult)* |
| Back-to-Back difficult line clears | Action score × 1.5 *(excluding soft drop and hard drop)* |
| Combo | 50 × combo count × level |
| Soft drop | 1 per cell |
| Hard drop | 2 per cell |

---

## Score expectations by skill

This is the rough estimation of points allocation based on experience levels. We
use it to sanity-check the wallet pricing below.

| Category | Points per game |
|---|---|
| Beginner | ~1,000 – 10,000 |
| Average | ~10,000 – 30,000 |
| Good | ~30,000 – 100,000 |
| Best | ~100,000 – 200,000 |
| Elite | ≳ 200,000 |

For all the pricing math below, we assume the everyone is around beginner level (and also not much game can be play), so **average score is 5,000 points per game** (Middle of Beginner Tier).

That table is a full sitting, though, and a demo game is not one. Somebody who
walks up to a terminal, plays for two minutes and tops out lands well under the
bottom of the Beginner row — nearer **500 points** — which is the figure the
wallet rate and the prices below are actually sized against.

---

## Wallet economy

```
wallet_points earned = lifetime_points / 100
```

The sum comes first and the division second, which is the whole rule: a game
is paid the difference between what the running total was worth before it and
what it is worth after, so the remainder stays on the account instead of being
rounded away. Three games of 90 points each pay 0, 1, then 1 — not 0, 0, 0.

That matters because the games actually played here are short. A demo game
lasts a couple of minutes and lands nearer **500 points** than the 5,000 the
skill table above calls a beginner game, and at a rate of one point per
thousand every one of them would have earned nothing at all. One point per
hundred puts a short game at about **5 wallet_points**, which is what the
pricing below is sized against.

---

## Store pricing

### Items

Each item costs a flat number of `wallet_points`.

**Characters:**

| Character | Cost (wallet_points) | Est. games to get it |
|---|---|---|
| Halloween | — | Free (starting character) |
| Mirurun | Free | — |
| Princess | 10 | ~2 games |
| Wolf-man | 10 | ~2 games |

Mirurun is free for the same reason "Design and AI" is: a player who has never
finished a game has an empty wallet, and one paid character against one free
one is the difference between a marketplace they can look at and one they can
use. Halloween is granted at signup and never appears as a purchase.

**Themes:**

| Theme | Cost (wallet_points) | Est. games to get it |
|---|---|---|
| Default | — | Free (starting theme) |
| Design and AI | Free for SUTDents | — |
| Do u wanna build a snowman? | 5 | ~1 game |
| Haaland | 8 | ~1.6 games |
| Claude-ing | 7 | ~1.4 games |
| Al-Merqaedes | 15 | ~3 games |
| Nuclear Gandhi | 15 | ~3 games |

The "est. games" column assumes the short demo game — about 500 points, so
about 5 wallet_points — described under [Wallet economy](#wallet-economy).

The John Cena theme was costed here at 10 but never drawn, so it is cut rather
than shipped as a purchase with no artwork behind it. Its catalogue id (5) is
left as a gap in `themes.cfg` instead of being reassigned — ids are persisted
in every player's `owned_themes` list, so renumbering would re-point what
somebody already bought at a different theme.

### Why these prices

| Reason | Rationale |
|---|---|
| **1–3 games per unlock** | Players unlock something after a couple of games. |
| **Flat pricing** | No scaling or discounts keeps the store simple. |
| **Free tiers** | Everyone has a different theme choice with zero games played. |

### The demo is short, so prices are low

Here's what we're planning around:

- **~300 people.** We're assuming 170 students (CSD cohort size)  for demo day, and to be safe we decided to double it, so ~300.
- **A short window.** The demo is probably only going to last a few hours (maybe 2–3 h, or even 1 h). In that time, not many games can be played.

Since not much can be played, we decided to reduce the price for the characters and themes so players can actually afford them. The goal is that an average player can unlock their first item in roughly 1 ~ 3 games.