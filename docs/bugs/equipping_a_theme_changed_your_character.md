# Bug Note — Equipping a theme changed your character

## What the bug was

`db_equip_theme` wrote the wrong field.

```c
else
{
    tmp = *p;
    tmp.current_equipped_character = tid;   /* should be _theme */
    r = db_persist(db, &tmp);
    if (r == DB_OK)
        *p = tmp;
}
```

It checked the right list — `owned_themes` — and then assigned the theme id to
`current_equipped_character`. So equipping a theme did two wrong things at
once: it left the equipped theme untouched, and it silently repointed the
player's equipped **character** at whatever number the theme happened to carry.

That second half is the one that matters. A theme is cosmetic; a character is
not. `tetrisd` reads `current_equipped_character` out of the store to decide
what an `ABILITY` level means (`handlers_game.c`, `equipped_character`), so:

```text
player owns character 1 (Halloween), theme 3 (snowman)
EQUIP /player/7/theme/3
  -> owned_themes contains 3, so accepted
  -> current_equipped_character = 3      (Princess)
  -> current_equipped_theme     = 1      (unchanged)
ABILITY level 1
  -> reads character 3 -> fires Sol, not Fry
```

A player who changed their board's colour scheme was handed a different
character's crystal powers — one they had never bought, which `db_buy_*` would
have refused to sell them. Ownership was enforced on the way in and then
discarded by the write itself.

## Why it was not caught

`test_buy_and_equip` covered the equip path, but only ever called
`db_equip_character`. For themes it called the *ownership probes*
(`db_player_owns_theme`), never the equip. So the suite exercised both
functions' names and only one function's behaviour.

The two functions are near-identical twins — same shape, same locking, same
persist-then-commit — and the copy that produced the second one carried the
first one's assignment. That is exactly the kind of defect a test written per
*function* catches and a test written per *feature* misses: nothing in the
suite ever asked "after equipping a theme, what character am I?", because no
single function is responsible for that answer.

It also could not surface through the client. Until the Marketplace was made
server-authoritative, `tetrisu` equipped by flipping a flag in its own view
model and never called `db_equip_theme` at all, so the store's opinion of your
loadout was never read back. The bug was reachable only once something actually
persisted an equip and then asked for it again.

## The fix

Assign the field the function is named after:

```c
tmp.current_equipped_theme = tid;
```

## The lesson

**A test that names a function is not a test that covers a feature.** The
regression added for this asserts the invariant across the pair rather than
inside either one:

```c
assert(db_equip_theme(db, id, 3) == DB_OK);
assert(db_get_player(db, id, &out) == DB_OK);
assert(out.current_equipped_theme == 3);
assert(out.current_equipped_character == 2);   /* and back the other way */
```

Both directions, because the twin could have carried the mistake either way.
Where two functions differ only in which field they touch, the thing worth
asserting is that each one leaves the other's field alone — a symmetric pair
needs a symmetric test, and neither half of it belongs to one function.
