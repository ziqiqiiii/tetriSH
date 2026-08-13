# Project Progress

## Completed package: Al Merqaedes Settings preview normalization

- Route/state: Medium route, deployment state.
- Goal: Normalize the newly supplied Al Merqaedes artwork to the established
  Settings theme-preview contract and verify that it is mapped and rendered.
- Scope: the user-modified preview PNG, its provider mapping and documentation,
  focused asset/model checks, strict build, full unit verification, and review.
- Protected behavior: preserve the supplied artwork content, every other theme
  preview, Settings navigation, and compatibility-mode behavior.

### Acceptance criteria

1. The complete supplied image is deterministically stretched/compressed to
   the canonical 192x192 RGBA Settings preview without cropping its subject.
2. The Al Merqaedes fixture item continues to resolve that exact asset path;
   other previews and character portraits are unchanged.
3. Focused asset checks, Settings tests, strict build, all unit suites, final
   diff hygiene, and an honest real-terminal visual limitation are recorded.

### Ownership

- Main agent: implementation, tests, docs, and final review.
- Persistent Luna explorer companion: read-only supplementary investigation.

### Completion status

- Deterministically normalized the complete user-supplied 433x576 RGBA image
  to the established 192x192 RGBA preview contract using Lanczos resampling.
  No crop, repaint, filename change, or provider remapping was introduced.
- The existing `al_merqaedes` fixture mapping continues to resolve the exact
  Settings theme-preview asset. Other theme assets and the cell fallback are
  untouched; the latter intentionally renders catalogue names without images.
- Added a focused PNG IHDR regression covering all seven theme previews. It
  verifies the PNG signature, 192x192 dimensions, eight-bit depth, and RGBA
  colour type without depending on Notcurses terminal capability discovery.
- Verified the focused Settings suite, strict full binary build, all 19 unit
  suites (0 failed), asset metadata, visual transparency/content inspection,
  and final diff hygiene. The persistent Luna companion found no rendering or
  compatibility code change necessary beyond asset normalization.
- No blocker remains. The bitmap thumbnail still merits the documented
  real-terminal visual smoke test because Notcurses capability probing cannot
  be automated reliably in the scripted PTY.

## Completed package: Lobby volume card and Home-label restoration

- Route/state: Medium route, deployment state.
- Goal: Make Lobby volume keys use the shared notification card and restore
  every Home menu label after leaving the Multiplayer Mode picker with Escape.
- Scope: Lobby volume action routing, Multiplayer-to-Home backdrop/menu-layer
  lifecycle, focused regressions, full verification, and final review.
- Protected behavior: preserve Mode Picker navigation, the optimized fast Back
  path where safe, room confirmation behavior, and unrelated user work.

### Acceptance criteria

1. Lobby volume changes display the standard volume notification card without
   replacing the Lobby inline status.
2. Escaping the Multiplayer Mode picker restores all Home menu names and the
   selector on both bitmap and compatibility paths.
3. The Back path avoids unnecessary terminal refresh while rebuilding every
   Home-owned presentation layer that was destroyed on Multiplayer entry.
4. Focused regressions, strict build, all unit suites, sanitizer-relevant
   checks, final diff review, and honest manual-terminal limitations pass.

### Ownership

- Main agent: implementation, tests, docs, and final review.
- Persistent Luna explorer companion: read-only supplementary investigation.

### Completion status

- Lobby volume Up/Down now routes through the shared floating notification
  card and preserves the room browser's existing inline status. The obsolete
  inline volume-feedback state and formatter branch were removed.
- Multiplayer Mode Picker Escape now tears down Multiplayer, re-emits the Home
  splash, and recreates the menu labels, selector, and bunny. It still skips
  the terminal geometry/capability refresh that caused the earlier Back delay.
- Added a focused regression proving Lobby volume actions do not overwrite
  inline feedback. Also closed the companion review's adjacent confirmation
  plane cleanup path when initial dialog rendering fails.
- Verified the focused Lobby suite, the strict full binary build, all 19 unit
  suites (0 failed), and final whitespace hygiene. The persistent Luna
  companion found no remaining confirmed defect in this package.
- No blocker remains. Notcurses integration cannot be automated in the
  scripted PTY because its capability probe waits for real-terminal replies,
  so final bitmap/cell appearance still needs the documented manual smoke test.

## Completed package: Room UI, navigation, and confirmation fixes

- Route/state: Medium route, deployment state.
- Goal: Repair the waiting-room presentation shown in the user capture, make
  all-room lobby navigation complete in both renderer tiers, remove avoidable
  back-navigation latency, and require confirmation before destructive exits.
- Scope: waiting-room volume notification routing and frame geometry; lobby
  viewport/selection behavior; compatibility-mode parity; multiplayer-to-home
  lifecycle profiling; reusable Yes/No confirmation for global Quit and
  waiting-room Leave; regressions and broad verification.
- Protected behavior: preserve the accepted Mode Picker route, 4–99 Battle
  Royale contract, Double/owner-start policy, unrelated user work, and existing
  provider/gameplay boundaries.

### Acceptance criteria

1. Waiting-room volume changes use the standard volume notification instead of
   replacing the room's inline action feedback.
2. Slot, chat, status, input, and control frames fit and close cleanly at
   supported bitmap and compatibility geometries.
3. The ALL lobby filter can scroll to, visibly highlight, and join every room;
   filtered views and direct-id joining remain correct in both renderer tiers.
4. Back navigation performs only the lifecycle work required for its target,
   with the measured avoidable latency removed.
5. Global `Q` exits only after explicit Yes, and waiting-room `Esc`/Leave
   returns to the lobby only after explicit Yes; No restores the current screen.
6. Focused regressions, compatibility checks, strict build, all unit suites,
   sanitizer checks, final diff review, and honest manual-check limitations.

### Ownership

- Main agent: implementation, tests, docs, and final review.
- Persistent Luna explorer companion: read-only supplementary investigation.

### Completion status

- Waiting-room volume keys now use the shared floating music-volume card and
  leave readiness/countdown/chat feedback untouched.
- Lobby and waiting-room bitmap regions now own their complete plates, so
  coarse cell-aligned crops cannot overwrite a border drawn on another plane.
  The compatibility renderer fits all eight roster rows at its documented
  58x20 minimum without colliding with chat or the control legend.
- Added a shared six-row lobby viewport in both renderer tiers. Filter changes
  resynchronize their counts immediately, selection scrolls rows 7-8 into
  view, and the selected model entry remains the room joined by Enter.
- Removed the non-resize terminal refresh from ordinary returns, reuses the
  already-decoded Home backdrop when the Multiplayer Mode screen owns it, and
  bounds stale-input draining to 256 events instead of an unbounded loop.
- Added a persistent safe-default No/Yes confirmation surface. User-triggered
  Q is confirmed across normal screens, the sign-in-required modal, and Solo;
  waiting-room Esc/L confirms Leave Room. Resize is returned to the owning
  screen's normal reflow path, and cancelling during a countdown resets its
  next tick rather than rapidly catching up prompt time.
- Added confirmation, full eight-room viewport, and complete-frame ownership
  regressions. Verified the strict full binary build, all 19 unit suites,
  independent AddressSanitizer/UndefinedBehaviorSanitizer builds and runs of
  all 19 suites (LeakSanitizer disabled for the environment), focused reruns,
  and `git diff --check`.
- No blocker remains. The scripted PTY cannot answer Notcurses capability
  probes, as documented by the project, so final protocol-specific appearance
  still needs a real-terminal visual pass in cell and bitmap modes.

## Completed package: Multiplayer and leaderboard defect fixes

- Route/state: Medium route, deployment state.
- Goal: Fix the confirmed leaderboard and multiplayer defects while preserving
  the accepted Multiplayer Mode Picker navigation.
- Scope: leaderboard exit-input cleanup and small-terminal fallback; Create
  Room selection semantics; Double-only automatic countdown; owner-triggered
  Battle Royale countdown; room-entry audio; related room-state/readiness
  contracts; deterministic regressions and broad verification.
- Protected behavior: keep the Mode Picker and Lobby parent route, retain the
  validated 4–99 player capacity and roster paging, preserve unrelated user
  work, and avoid changing match gameplay scaffolds beyond required contracts.

### Acceptance criteria

1. Leaving Leaderboard cannot leak queued input, and a sub-20x44 terminal shows
   a stable resize notice rather than quitting.
2. Create Room digits only select a mode; Enter performs creation.
3. Double begins its countdown automatically when ready. Battle Royale begins
   only after its owner explicitly starts a valid ready room.
4. Entering a waiting room produces one softer, non-blocking sound cue through
   the existing audio subsystem, with no-audio builds remaining valid.
5. Relevant room-state/readiness behavior matches the accepted use-case
   contract or any intentionally deferred mismatch is reported explicitly.
6. Focused regressions, strict build, all unit suites, sanitizer checks, and
   final diff review pass.

### Ownership

- Main agent: implementation, tests, docs, and final review.
- Persistent Luna explorer companion: read-only supplementary investigation.

### Completion status

- Leaderboard Back/Quit now discards queued input before navigation, and its
  renderer degrades to a 44x20 resize notice instead of treating a small
  terminal as a fatal screen error.
- Create Room number keys now select Double/Battle Royale without submitting;
  Enter remains the creation action, and both renderers advertise the same
  controls.
- Double auto-arms its countdown as soon as both seats are ready. Battle Royale
  requires at least four players, every occupied seat ready, and an explicit
  owner Start; non-owners and terminal room states are rejected.
- Added READY and FINISHED room states, reconciled fixture JOIN/CREATE ready
  flags, labels, join guards, status copy, and the accepted Mode Picker/use-case
  documentation while leaving the Mode Picker route intact.
- Added one non-blocking room-entry acknowledgement through the existing
  300 ms dialog clip at the softer menu-select mix level. Silent/no-audio
  configurations remain valid; the fixture provider has no remote-player push
  stream yet, so future server arrival events should invoke the same audio API.
- Added regressions for queue-exit classification, select-then-Enter creation,
  Double/BR start policy, all-ready 99-player behavior, state transitions,
  terminal-state guards, and provider contracts.
- Verified the strict full binary build, all 18 unit suites, independent
  AddressSanitizer/UndefinedBehaviorSanitizer builds and runs of all 18 suites,
  and `git diff --check`. LeakSanitizer cannot run under this environment's
  ptrace wrapper, so ASan was rerun with leak detection disabled.
- Final main-agent and Luna companion reviews found no additional confirmed
  defect in the scoped flows. Real terminal rendering and audible playback
  remain environment-specific manual checks.

## Completed package: Multiplayer and leaderboard audit

- Route/state: Medium route, deployment state.
- Goal: Audit the recently implemented multiplayer, leaderboard, Battle Royale,
  and screen integration work for concrete defects, and expand Battle Royale
  capacity from 4–8 players to 4–99 players.
- Scope: relevant recent changes, screen-registration guidance and contracts,
  multiplayer configuration and validation, Battle Royale presentation and
  documentation, leaderboard behavior, deterministic regressions, and broad
  verification.
- Protected areas: preserve unrelated user changes and do not change audited
  behavior beyond the requested Battle Royale capacity unless a required
  compatibility fix is discovered and verified.

### Acceptance criteria

1. The audit reports evidence-backed bugs or explicitly distinguishes risks and
   missing coverage from confirmed defects.
2. Every active Battle Royale player-count constraint accepts 4 through 99,
   rejects values outside that range, and presents the new range consistently.
3. Focused tests cover the lower and upper boundaries and regressions found in
   the changed code.
4. Relevant focused suites, the broader project verification gates, and final
   diff checks complete with honest results.
5. Deployment documentation reflects the verified outcome and any remaining
   environment-specific validation or blockers.

### Ordered work

1. Map the recent feature changes and the documented screen integration model.
2. Trace multiplayer, leaderboard, and Battle Royale interfaces and tests;
   record concrete findings.
3. Implement the 4–99 capacity update at its authoritative boundaries and
   update tests and durable/public documentation where appropriate.
4. Run focused and broad verification, review integration boundaries, and
   reconcile this package status.

### Ownership

- Main agent: audit decisions, implementation, tests, docs, and final review.
- Persistent Luna explorer companion: read-only supplementary investigation.

### Completion status

- Expanded the shared room model and Battle Royale capacity contract to 99,
  centralized mode/capacity validation, rejected malformed lobby and waiting-
  room snapshots, and updated fixture/provider presentation consistently.
- Added an eight-row roster window with Up/Down and Page Up/Page Down paging so
  all 99 configured seats remain reachable in both bitmap and cell renderers.
  The paging offset invalidates only the roster region.
- Added deterministic coverage for 4/99 acceptance, 3/100 rejection, invalid
  player counts, complete roster paging, fixture capacities, and lobby join
  rejection.
- Verified the strict full binary build, all 18 unit suites, an independent
  AddressSanitizer/UndefinedBehaviorSanitizer build and run of all 18 suites,
  removal of obsolete 4–8 literals, and `git diff --check`.
- Identified follow-on defects in Leaderboard exit/small-terminal handling,
  Create Room digit semantics, and multiplayer readiness/state behavior; the
  completed package above subsequently resolves them.
- No blocker remains for the scoped 4–99 change. A real Kitty/Sixel session is
  still required for protocol-specific visual confirmation.
