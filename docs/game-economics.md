# Game Economics Design Decisions

---

## Purpose

tetriSH tetris game that consist of Single Player Mode, Double Players Mode, and Battle Royale Mode. After each game, players earn **points**, and
those points feed two things we track for each player:

| Value | What it is | What it's for |
|---|---|---|
| `leaderboard_scores` | The player's personal highest (best) single-game score | Ranking / bragging rights only |
| `wallet_points` | The sum of points earned per game ÷ 1000 | The currency to buy new characters or themes |

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

For all the pricing math below, we assume the **average score is 20,000 points per game** (Middle of Average Tier).

---

## Wallet economy

```
wallet_points = Sum of points earned per game / 1000
```

At an average game of 20,000 points, that's about **20 wallet_points per game**.

---

## Store pricing

### Items

Each item costs a flat number of `wallet_points`.

**Characters:**

Each character is worth 30 wallet_points (about 1.5 average games).

**Themes:**

| Theme | Cost (wallet_points) | Est. games to get it |
|---|---|---|
| Default | — | Free (starting theme) |
| Design and AI | Free for SUTDents | — |
| Haaland | 20 | ~1 game |
| John Cena | 30 | ~1.5 games |
| Claude-ing | 25 | ~1.25 games |
| Al-Merqaedes | 35 | ~1.75 games |
| Nuclear Gandhi | 35 | ~1.75 games |
| We Celebrate our Differences | 30 | ~1.5 games |

The "est. games" column assumes the 20,000-point (≈ 20 wallet_points) average game.

### Why these prices

| Reason | Rationale |
|---|---|
| **1–2 games per unlock** | Players unlock something after a couple of games. |
| **Flat pricing** | No scaling or discounts keeps the store simple. |
| **Free tiers** | Everyone has a different theme choice with zero games played. |

### The demo is short, so prices are low

Here's what we're planning around:

- **~300 people.** We're assuming 170 students (CSD cohort size)  for demo day, and to be safe we decided to double it, so ~300.
- **A short window.** The demo is probably only going to last a few hours (maybe 2–3 h, or even 1 h). In that time, not many games can be played.

Since not much can be played, we decided to reduce the price for the characters and themes so players can actually afford them. The goal is that an average player can unlock their first item in roughly 1 ~ 2 games.