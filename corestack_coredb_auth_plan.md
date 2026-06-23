# CoreStack Custom Account DB Plan

## Purpose

Build a small custom database layer for CoreStack user accounts and marketplace state.

This DB is **not** the same as `tetrislogd` logging. `tetrislogd` writes human-readable/system logs. This DB stores durable user data: accounts, password hashes, points, inventory, equipped theme, and high score.

The goal is to create a serious, explainable C systems component without blocking the main `tetrisd` game loop.

---

## Current decision

Use a new self-contained library:

```text
lib/libcoredb/
```

Reason: the database now stores more than marketplace data. It includes account authentication, player state, points, cosmetics, and high score. Calling it `libmarketcore` would make the responsibility too narrow.

Later:

```text
marketd      -> uses libcoredb for points/inventory/theme equip
chatd        -> probably does not need libcoredb initially
tetrisd      -> may use libcoredb for login/high-score or query marketd depending on final architecture
```

---

## Architecture

```text
Disk append-only DB file
        |
        | replay on startup
        v
In-memory hash table
        |
        | later: bulk rebuild
        v
B+ tree leaderboard index
```

### Source of truth

```text
Disk file        = durable source of truth
Hash table       = current live user state
B+ tree          = rebuildable read index for leaderboard/category queries
```

The B+ tree is not the source of truth. It can be deleted and rebuilt from the hash table.

---

## Runtime write path

Initial implementation can use one DB mutex.

```text
coredb_create_user / coredb_credit_points / coredb_equip_theme
        |
        v
lock db->mutex
        |
        v
append event to disk
        |
        v
fsync/fdatasync or fflush depending on first version
        |
        v
apply event to hash table
        |
        v
unlock db->mutex
```

Later, when integrated into `tetrisd` or `marketd`, this can become:

```text
client_thread / marketd store_thread
        |
        v
db_submit_request()
        |
        v
db_worker_thread
        |
        v
append disk event -> update hash table -> return result
```

Important rule: never hold `room->mutex` while waiting for DB work or doing disk I/O.

Correct pattern:

```text
lock room
copy player_id / score / points delta into local request
unlock room

call coredb function or submit DB request
```

---

## Data model

### User record in memory

```c
typedef struct s_coredb_user
{
	uint32_t	player_id;
	char		username[32];

	uint8_t		password_salt[16];
	uint8_t		password_hash[32];
	uint32_t	password_iters;

	uint32_t	points;
	uint64_t	owned_characters;
	uint64_t	owned_themes;
	uint32_t	equipped_theme;
	uint32_t	high_score;
}   t_coredb_user;
```

### Notes

- Do **not** store raw passwords.
- Store `password_salt`, `password_hash`, and `password_iters`.
- Use OpenSSL PBKDF2-HMAC-SHA256 or another OpenSSL-provided password hashing/key derivation function.
- Do not implement your own cryptographic hash.
- `owned_characters` and `owned_themes` are bitfields.
- `equipped_theme` is durable account state.
- The chosen character is per-game state in `tetrisu`/`tetrisd`; `libcoredb` only stores character ownership.
- `high_score` is enough for the first leaderboard version.

---

## Disk event model

The disk file is append-only. Each change is stored as an event.

### Event header

```c
typedef struct s_coredb_record_header
{
	uint32_t	magic;
	uint16_t	version;
	uint16_t	event_type;
	uint64_t	seq_no;
	uint64_t	timestamp_ms;
	uint32_t	payload_len;
	uint32_t	payload_crc;
}   t_coredb_record_header;
```

### Event types

```c
typedef enum e_coredb_event_type
{
	COREDB_EV_USER_CREATE = 1,
	COREDB_EV_PASSWORD_SET = 2,
	COREDB_EV_POINTS_DELTA = 3,
	COREDB_EV_GRANT_CHARACTER = 4,
	COREDB_EV_GRANT_THEME = 5,
	COREDB_EV_EQUIP_THEME = 6,
	COREDB_EV_SET_HIGH_SCORE = 7
}   t_coredb_event_type;
```

### Event payloads

#### User create

```c
typedef struct s_coredb_ev_user_create
{
	uint32_t	player_id;
	char		username[32];
	uint8_t		password_salt[16];
	uint8_t		password_hash[32];
	uint32_t	password_iters;
}   t_coredb_ev_user_create;
```

#### Points delta

```c
typedef struct s_coredb_ev_points_delta
{
	uint32_t	player_id;
	int32_t		points_delta;
	uint32_t	reason;
}   t_coredb_ev_points_delta;
```

#### Grant item

```c
typedef struct s_coredb_ev_grant_item
{
	uint32_t	player_id;
	uint32_t	item_id;
}   t_coredb_ev_grant_item;
```

#### Equip theme

```c
typedef struct s_coredb_ev_equip_item
{
	uint32_t	player_id;
	uint32_t	item_id;
}   t_coredb_ev_equip_item;
```

#### Set high score

```c
typedef struct s_coredb_ev_high_score
{
	uint32_t	player_id;
	uint32_t	high_score;
}   t_coredb_ev_high_score;
```

---

## Startup replay

On `coredb_open()`:

```text
1. Open DB file, creating it if missing.
2. Initialise in-memory hash table.
3. Read event header.
4. Verify magic/version/payload_len.
5. Read payload.
6. Verify payload CRC/checksum.
7. Apply event to hash table.
8. Repeat until EOF.
9. Seek to end so future events append.
```

If the last record is incomplete or corrupted:

```text
First version: stop replay at last good record and return warning.
Later version: truncate file to last good offset.
```

---

## Hash table design

Use separate chaining.

```text
bucket index = hash(username) % bucket_count
```

Need two lookup paths eventually:

```text
username -> user
player_id -> user
```

For tonight, use username as the primary lookup. Add player_id lookup if time allows.

### Resizing

Implement resizing now so it is not forgotten later.

Start with:

```text
bucket_count = 256
```

Resize rule:

```c
if ((user_count + 1) * 4 >= bucket_count * 3)
	resize_to(bucket_count * 2);
```

That is a 75% load factor threshold.

Resize process:

```text
1. Allocate new bucket array.
2. Walk every old bucket.
3. Rehash each node into the new bucket array.
4. Free old bucket array.
5. Update db->buckets and db->bucket_count.
```

This is only in-memory. It does not change disk format.

---

## Public API draft

```c
int	coredb_open(t_coredb **db, const char *path);
int	coredb_close(t_coredb *db);

int	coredb_create_user(t_coredb *db, const char *username,
		const char *password, uint32_t *out_player_id);

int	coredb_verify_user(t_coredb *db, const char *username,
		const char *password, uint32_t *out_player_id);

int	coredb_credit_points(t_coredb *db, uint32_t player_id,
		int32_t delta, uint32_t reason);

int	coredb_get_balance(t_coredb *db, uint32_t player_id,
		uint32_t *out_balance);

int	coredb_grant_character(t_coredb *db, uint32_t player_id,
		uint32_t character_id);

int	coredb_grant_theme(t_coredb *db, uint32_t player_id,
		uint32_t theme_id);

int	coredb_equip_theme(t_coredb *db, uint32_t player_id,
		uint32_t theme_id);

int	coredb_set_high_score(t_coredb *db, uint32_t player_id,
		uint32_t score);

int	coredb_get_user(t_coredb *db, uint32_t player_id,
		t_coredb_user *out_user);
```

Return convention:

```text
EXIT_SUCCESS on success
EXIT_FAILURE on failure
```

For more detailed errors later, add an enum:

```text
COREDB_OK
COREDB_ERR_EXISTS
COREDB_ERR_NOT_FOUND
COREDB_ERR_BAD_PASSWORD
COREDB_ERR_NOT_OWNED
COREDB_ERR_IO
```

---

## File structure

```text
lib/libcoredb/
├── Makefile
├── include/
│   └── coredb.h
├── src/
│   ├── db.c              # open/close/init lifecycle
│   ├── db_storage.c      # append/read binary events
│   ├── db_replay.c       # replay disk file into hash table
│   ├── db_record.c       # record serialisation/checksum helpers
│   ├── db_hash.c         # hash table insert/find/resize
│   ├── db_auth.c         # salt generation + password hashing + verify
│   ├── db_user.c         # create user / verify user
│   ├── db_points.c       # credit/get balance
	│   ├── db_inventory.c    # grant character/theme + equip theme
│   └── db_score.c        # high score update
└── tests/
    ├── test_coredb_auth.c
    ├── test_coredb_replay.c
    ├── test_coredb_hash.c
    ├── test_coredb_points.c
    └── test_coredb_inventory.c
```

---

## Tonight scope

### Must finish

```text
1. Create lib/libcoredb skeleton.
2. Add Makefile.
3. Add include/coredb.h.
4. Implement coredb_open/coredb_close.
5. Implement append-only event write.
6. Implement replay from disk.
7. Implement hash table with resizing.
8. Implement create_user and verify_user.
9. Add test: create user -> close -> reopen -> verify login works.
```

### Nice to finish

```text
10. Implement credit_points and get_balance.
11. Add test: credit points -> close -> reopen -> balance persists.
12. Implement grant character/theme and equip theme.
13. Add test: grant/equip theme -> close -> reopen -> inventory/theme persists.
```

### Do not do tonight

```text
B+ tree
marketd integration
tetrisd integration
tetrisu marketplace UI
session tokens
password reset
admin roles
full leaderboard categories
compaction
```

---

## Tests to write

### `test_coredb_auth.c`

```text
- open temp db
- create user alice/password123
- verify alice/password123 succeeds
- verify alice/wrongpassword fails
- close
- reopen
- verify alice/password123 still succeeds
```

### `test_coredb_points.c`

```text
- open temp db
- create user
- credit +100
- check balance == 100
- close
- reopen
- check balance == 100
```

### `test_coredb_inventory.c`

```text
- open temp db
- create user
- grant character 2
- equip theme 3
- close
- reopen
- check owned character/theme bits are set
- check equipped_theme == 3
```

### `test_coredb_hash.c`

```text
- create many users, enough to trigger resize
- verify every user can still be found after resize
```

---

## Later integration plan

### With `marketd`

```text
marketd starts
    -> coredb_open(var/db/users.db)
    -> replay account/market state

marketd receives purchase/theme-equip from tetrisu
    -> coredb_credit_points / coredb_grant_character / coredb_equip_theme
```

### With `tetrisd`

Option A: `tetrisd` talks to `marketd` for account ownership/theme data.

```text
tetrisd JOIN
    -> query marketd over Unix SOCK_STREAM
    -> marketd reads libcoredb state
    -> validates selected character ownership and returns equipped theme
```

Option B: `tetrisd` links `libcoredb` directly.

```text
tetrisd LOGIN/JOIN
    -> coredb_verify_user
    -> coredb_get_user for inventory/theme
```

Safer long-term design: keep marketplace/account ownership in `marketd`, and let `tetrisd` query `marketd` only at JOIN/theme/ability-validation time. Avoid making both `tetrisd` and `marketd` write the same DB file at the same time.

---

## Future B+ tree plan

Do not build this until append/replay/hash/tests are stable.

Purpose:

```text
high_score leaderboard
category leaderboard later
```

Initial leaderboard can be:

```text
scan hash table -> copy users into array -> sort by high_score -> top N
```

Future B+ tree:

```text
key = (high_score, player_id)
value = player_id
```

Use ascending key order and scan rightmost leaf for highest scores, or store inverted score:

```text
inverted_score = UINT32_MAX - high_score
```

Then top scores come from the leftmost leaf.

---

## README explanation

```text
libcoredb is our custom append-only account database. It stores durable player data:
username, salted password hash, points, owned cosmetics, equipped theme, and high
score. The disk file is authoritative. On startup, libcoredb replays the binary event
file into an in-memory hash table. Writes append a new event to disk first and then
apply the change to the hash table. The hash table resizes at a 75% load factor.

The current leaderboard path can scan and sort the hash table. A future B+ tree index
will be bulk-built from the hash table for faster ranked queries. The B+ tree is a
rebuildable read index, not the source of truth.
```

---

## Commit plan

Use meaningful commits:

```text
[libcoredb] add account database skeleton
[libcoredb] add append-only event storage
[libcoredb] replay user events into hash table
[libcoredb] add salted password verification
[libcoredb] resize user hash table at load threshold
[libcoredb] persist points and inventory events
[libcoredb] add replay tests for auth and points
```

---

## Explanation for checkoff

Say this:

```text
We built libcoredb as a custom append-only account database in C. It is separate from
tetrislogd because logs are human/system observability, while libcoredb stores durable
user state. The disk file stores binary events. On startup, those events are replayed
into an in-memory hash table. Passwords are never stored directly; we store a salt,
password hash, and iteration count. Points, inventory, equipped theme, and high score
are also event-sourced. The hash table is the live state and resizes at a 75% load
factor. Later, leaderboard reads can use a rebuildable B+ tree index built from the hash
table.
```
