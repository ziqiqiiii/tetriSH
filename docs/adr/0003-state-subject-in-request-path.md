# STATE names its subject in the request path, not the body

A STATE body (`t_sb_state`) carries no player or room identifier. The subject
is carried by the resource path instead: `STATE /room/<id>/player/<pid>`. This
corrects the `STATE /room/<id>` examples in `docs/use_cases.md` and
`lib/libhtttp/README.md`.

The reason is separation of concerns, not cost of change: a body is a
*projection of one player's game* — board, piece, score, charge — and says
nothing about whose it is or where it lives. Addressing belongs to the
envelope, exactly as it does in HTTP. Keeping identity out of the body also
means the same encoder serves a snapshot regardless of how it is routed, and
`sb_state_decode` stays strict (fixed key order, every key required) without
having to reserve keys for addressing.

## Amended 2026-07-30

This ADR originally justified the decision by arguing that adding an
identifier field "would be a breaking change to a shipped, tested library
(`libstatusbody`)". **That premise was false.** `libstatusbody` is not
shipped: every one of its eight encode/decode functions is a stub returning
`ENOSYS`, and all five of its test suites fail. No migration cost existed to
avoid.

The decision itself survives on the separation-of-concerns argument above,
which is why it is restated rather than reversed. The lesson is recorded
because the original reasoning would have justified almost anything: "the
library already ships" is only an argument when it is true, and it was never
checked.

## Consequences

- The body format is decided on its own merits and may still change freely,
  since `libstatusbody` has no implementation to migrate.
- Any future ADR appealing to "already implemented" or "already tested" should
  cite the passing suite it means.
