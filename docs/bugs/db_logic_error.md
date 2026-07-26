# Bug Note — Predicate answers mixed with error codes in db

## What the bug was

`libmacminidb` had one return vocabulary, `t_db_result`, whose values all answer
the same question: *did the operation succeed, and if not, why?*

```c
DB_OK, DB_NOT_FOUND, DB_EXISTS, DB_BAD_CREDS, DB_INSUFFICIENT,
DB_NOT_OWNED, DB_IO_ERROR, DB_FULL, DB_INVALID
```

But two functions answer a different question — *is this statement true?*

```c
bool  db_player_owns_character(t_db *db, t_player_id id, t_item_id cid);
bool  db_player_owns_theme(t_db *db, t_player_id id, t_item_id tid);
```

They returned plain `bool`, which collapses three distinct states into two:
owns it, does not own it, and *could not tell*. There was no way to report the
third, because `false` already meant "does not own it".

The mismatch showed up as documentation drifting away from the code:

- `docs/use_cases.md` UC-15a/16a extension 2a specified a `DB_IO_ERROR` path
  through a function whose return type cannot express one.
- `cd_sd_uc10-uc14.md` typed the same function as returning `DbResult` and drew
  `DB_OK | DB_NOT_OWNED`.
- `cd_sd_uc15-uc19.md` typed it as `bool`.

Three descriptions of one function, none of them matching each other.

## Why it matters

The `bool` compiles and behaves correctly today, so this is a design defect
rather than a crash. The cost is in what it invites later:

- **Silent dead branches.** The house idiom is `if (r == DB_OK)`. Against a
  `bool` return that is a type mismatch a compiler may accept, and the branch
  never runs.
- **Conflating "no" with "broken".** `!= DB_TRUE` treats a legitimate "does not
  own it" and a null-handle bug identically — so a server-side defect surfaces
  to the player as *"you don't own that"* instead of an error.
- **Docs that cannot be trusted.** Once a diagram shows a return value the code
  cannot produce, every other diagram becomes suspect too.

No caller existed yet — `tetrisd` is unwritten. Cheap to fix now, expensive
after a dozen call sites depend on it.

## The fix

A second, separate enum. `t_db_result` is untouched.

```c
typedef enum e_db_bool
{
	DB_FALSE = 0,		/* predicate does not hold */
	DB_TRUE,			/* predicate holds */
	DB_UNKNOWN			/* could not determine (bad handle) */
}	t_db_bool;
```

The two probes now return `t_db_bool`. A missing player is `DB_FALSE` — a real
answer. Only a `NULL` handle is `DB_UNKNOWN`. `DB_FALSE = 0` keeps a bare truth
test reading correctly.

Changed: the header, `src/db_read.c`, three asserts in `tests/test_db.c` (plus
new `DB_UNKNOWN` coverage), both diagram files, `README.md`, `CLAUDE.md`, and
the affected `use_cases.md` rows. Builds clean under `-Wall -Wextra -Werror`;
8/8 suites pass; valgrind clean.

## Why this is good practice

**A type should encode which question was asked.** "Did it work?" and "Is it
true?" are different questions, so they get different types. The type name tells
a reader which one they're holding without going to the docs.

**Failure and negation are not the same thing.** A probe answering "no"
succeeded. Folding "no" into an error enum loses that distinction exactly where
it matters — deciding whether to show the user a refusal or an error.

**Make the invalid state unrepresentable.** With one merged enum, `DB_OK` is
comparable to a probe result and always false. With two enums, that comparison
is a type error a reviewer can see.

**Documentation is a consumer of the API.** Three docs disagreeing was the
symptom that surfaced the design problem. Treat that divergence as a signal, not
as a formatting chore.

## What to take from this

1. **Writing the diagrams found the bug.** Stating the return type in a class
   diagram forced the question "what can this actually return?" — which the code
   answered differently from two of the specs. Modelling is a review technique,
   not paperwork.

2. **A `bool` return is worth a second look.** Any predicate that consults
   external state has a third outcome: *unable to answer*. If the signature has
   no room for it, that case gets silently folded into `false`.

3. **Fix interface shape before there are callers.** The same change after
   `tetrisd` lands means touching every call site and re-reviewing each one.

4. **The cheapest fix is not always the right one.** Adding `DB_TRUE`/`DB_FALSE`
   to `t_db_result` was fewer lines and kept one vocabulary — but it made
   `if (r == DB_OK)` a permanent trap. Two clean types beat one convenient one.

5. **Note the limit of this fix.** `t_db_bool` makes failure *expressible*, not
   reachable — these probes still walk an in-memory index and cannot do I/O. The
   related question of whether UC-15a/16a extension 2a should describe a
   *transport* failure at the client rather than a DB failure is left open
   deliberately; it lives at a different layer.
