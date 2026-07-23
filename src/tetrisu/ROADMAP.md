# tetriSU Incremental UI/UX Roadmap

This is the agreed delivery roadmap for tetriSU. It is deliberately
incremental: every approved change is isolated, checked, visually reviewed in
a real terminal, committed, and pushed before work begins on the next one.

## Delivery rules

- Preserve user-authored Aseprite and PNG HUD frames exactly. Never reset or
  overwrite unrelated work in the dirty tree.
- Keep each increment uncommitted until it is complete and visually approved.
- For visual work: implement, run automated checks, test in a real terminal,
  correct any issues, then commit and push to `origin/feat/tetrisu`.
- Every commit must pass the full build, unit tests, `git diff --check`, and
  appropriate resize and renderer-fallback checks.
- No glow is enabled by default.

## Current state

Phase 1, item 1 is complete and was approved in commit
`94a42b8 [tetrisu] stabilize the five-item home menu`.

- Five labels are visible, left aligned, and stationary.
- The bunny supports keyboard, pointer hover, and click selection.
- The moving selector avoids the terminal bitmap-placement leak found under
  rapid repeat in Kitty and WezTerm.
- Remaining items below are planned; do not begin them without an explicit
  request.

## Phase 1 — fix the currently broken experience

1. **Stabilize the five-item home menu** — complete

   - Labels: Single Player, Multiplayer, Marketplace, Leaderboard, Settings.
   - Compose opaque pixel-font labels once when the screen is created.
   - Keep the Gaiden background stationary and at the bottom of the stack.
   - Arrow input changes only the selector; it does not rebuild labels,
     reload fonts, or move the background.
   - Keep source padding so glyph ink and shadows cannot clip.
   - Support pointer hover and click alongside keyboard navigation.

2. **Add tested offline HOLD rules**

   - HOLD is local Solo authority.
   - First HOLD stores the active piece and advances NEXT once.
   - Later HOLD swaps without advancing NEXT.
   - Held pieces respawn in canonical orientation.
   - HOLD is usable once per active piece and rearms after lock.

3. **Render HOLD in the authored HUD frame**

   - Commit the supplied `.aseprite` and `.png` with the implementation.
   - Use the dedicated HOLD interior around `x=24, y=4, w=48, h=35`.
   - Render HOLD and NEXT through one aligned top strip covering
     `x=16..255, y=0..47`.
   - Use 10-pixel HOLD preview tiles and retain three NEXT pieces.
   - Bind HOLD to `C`; dim it to 55% after use for the current turn.
   - Preserve board alignment and do not redraw the supplied frame.

4. **Simplify the in-game control legend**

   - Primary legend: `ARROWS  X/Z ROTATE  SPACE DROP  C HOLD`.
   - Use shorter variants at narrow widths.
   - Put pause, home, and ability controls in their relevant overlays.

## Phase 2 — responsive Solo controls and rendering

5. **Add terminal-aware handling controls**

   - Replace key-code-only gameplay input with complete `ncinput` events.
   - Where supported: immediate tap, 167 ms DAS, 33 ms ARR, 20x soft-drop,
     and last-pressed priority for opposing directions.
   - In legacy terminals without releases, retain safe terminal-repeat
     behaviour instead of guessing a key is still held.

6. **Make clear-animation timing progressive**

   - Levels 1–6: 200 ms; 7: 175 ms; 8: 150 ms; 9: 125 ms; 10+: 100 ms.
   - Preserve the two-frame animation by splitting each duration at midpoint.
   - Keep the current Tetris Worlds gravity curve through 20G at level 19.

7. **Complete the forced cell-renderer fallback**

   - Add `TETRISU_RENDERER=auto|cell`.
   - Verify home, selector, HOLD/NEXT, board, overlays, and text without
     Kitty/iTerm bitmap support.
   - Missing image support must reduce decoration, never hide information or
     controls.

## Phase 3 — feedback, sound, and danger mode

8. **Add a global notification overlay**

   - Reusable top-right notification stack using monotonic timers.
   - Volume notification: `MUSIC`, percentage, and a 16-step bar.
   - Show for 900 ms, then fade for 180 ms.
   - Work on home, gameplay, future screens, and silent-audio builds.

9. **Replace ability help with a fading popover**

   - Hover opens a dark terminal-font box with ability name, charge cost, and
     short description.
   - Fade in over 120 ms and out over 180 ms.
   - Activation/rejection feedback takes priority for one second.
   - Keyboard activation shows the same feedback.

10. **Add event-driven retro sound effects**

    - Events: HOLD, normal clear, Tetris, perfect clear, ability ready,
      activated, rejected, countdown tick/go, top-out, and personal best.
    - Generate concise retro waveforms in memory; retain the silent fallback
      when SDL audio is unavailable.

11. **Add persistent offline personal bests**

    - Store only the Solo best score in a versioned state file under
      `XDG_STATE_HOME` with the standard local-state fallback.
    - Write atomically; failures never interrupt play.
    - Record completed top-out games only, and show `NEW PERSONAL BEST` with a
      short pulse/fade.

12. **Add restrained event animation**

    - Pulses/fades for clears, perfect clears, ability-ready transitions,
      activation results, countdown, and personal best.
    - Render at 30 FPS only while an animation is active.
    - No bloom or glow.

13. **Add no-glow danger presentation**

    - Enter when locked blocks reach rows 0–3.
    - Exit only after the highest locked block remains below row 5 for 1.5 s.
    - Fade the surrounding environment darker over 300 ms while preserving a
      readable, emphasized board.
    - Do not shake, scale, move, or glow the board.

14. **Add authored danger-music transition** — asset-gated

    - Cross-fade normal and supplied high-intensity tracks over 300 ms.
    - If the intense track is absent, retain normal music silently.
    - Do not add `audio-stretch`; an authored second track is safer and cleaner
      than a custom SDL streaming/time-scaling path.

## Phase 4 — screen framework and UI-first app flow

15. **Add screen navigation and typed view models**

    - States: entry, login, sign-up, home, Solo, marketplace, settings,
      leaderboard, lobby, create-room modal, waiting room, Double, Battle
      Royale, quit.
    - Provider interface for authentication, profile, catalogue, leaderboard,
      and room data, with marked local fixtures for UI work.

16. **Add login, sign-up, and offline entry screens**

    - Startup: Login, Sign Up, Play Offline.
    - UTF-8 field editing, password masking, focus order, validation, loading,
      error, back, and resize support.

17. **Route all five home actions**

    - Offline Single Player works locally.
    - Offline Multiplayer, Marketplace, and Leaderboard show a polished
      sign-in-required notification.
    - Settings stays available locally.
    - Fixture mode opens server-dependent UIs with a visible
      `LOCAL UI PREVIEW` marker.

18. **Add the leaderboard screen**

    - Top-three podium; positions 4–10; loading, empty, unavailable, refresh,
      and back states.

19. **Add the settings and profile screen**

    - Profile portrait, username, equipped character/theme, owned lists,
      wallet points, score, rank, and marketplace routing.
    - Offline mode shows local settings without invented account statistics.

20. **Add the character and theme marketplace**

    - Character/theme tabs, selection, preview, abilities, price, ownership,
      equip, buy, insufficient-points, and unavailable states.

21. **Add the multiplayer lobby**

    - Identity/score/rank header, room table, refresh, join-by-ID,
      create-room, empty/error states, and back navigation.

22. **Add the create-room modal**

    - Double or Battle Royale, required player count, confirm/cancel, and
      validation.

23. **Add the waiting-room screen**

    - Room ID, mode, slots, owner, per-player ready state, owner-only start,
      leave, chat entry, ownership transfer, and insufficient-player fixtures.

24. **Add the Double gameplay layout**

    - Own board with HOLD/NEXT, opponent board, usernames, scores, countdown,
      status overlays, and fixture snapshot updates.
    - Rendering only; authority remains server-side later.

25. **Add the Battle Royale gameplay layout**

    - Central playable board with scalable opponent-board tiles, elimination,
      ranking, target/status, and narrow-terminal fallbacks.

26. **Add the real configuration screen** — backburner

    - Persist DAS, ARR, SDF, music/SFX volume, renderer mode, and
      reduced-motion preference in a versioned `key=value` configuration.
    - Defaults, reset, validation, and atomic saves.
    - Glow remains absent until a separate visual prototype is approved.

## Shared interfaces

- `render_wait_input(..., ncinput *)` for input where event state matters.
- `solo_handling_config_t` and `solo_handling_state_t` for DAS/ARR/SDF
  independent of gameplay authority.
- `solo_clear_duration_ms(level)` as the single source for clear timing.
- `ui_notification_t`, `ui_event_t`, and `audio_sfx_t` to connect gameplay
  events with overlays and audio without coupling rules to rendering.
- `app_screen_t` and `app_data_provider_t` to separate screens from the future
  network adapter.
- HOLD remains in the local Solo snapshot and later maps directly into the
  server-provided gameplay view model.

## Acceptance gates

- Unit-test menu labels/navigation, HOLD queue behaviour, handling press,
  release, and opposing-key cases, legacy fallback, clear durations, danger
  hysteresis, notification timers, personal-best persistence, screen
  transitions, and fixture failures.
- Test each screen at normal and minimum terminal sizes, during resize, after
  returning home, with audio disabled, and with `TETRISU_RENDERER=cell`.
- Home menu checklist: rapid Up/Down, both wrap directions, no background
  movement, trails, clipping, delayed bunny, or hidden labels.
- HOLD checklist: empty HOLD, first store, swap, blocked second HOLD, rearm
  after lock, restart, pause, resize, pixel renderer, and cell fallback.
- Screen commits require keyboard navigation, back behaviour, focus
  visibility, loading/error states, and resize support.

## Asset notes

- The latest HOLD frame is authoritative and is never redrawn by code.
- Marketplace/profile final polish benefits from transparent 400x400 portraits
  for Princess, Halloween, Wolf-man, and Mirurun, plus 512x384 theme previews.
- Danger-music work waits for a high-intensity track aligned with the existing
  loop.
