# Project Diary

## 2026-08-06 — Stationary bitmap notifications are authored art

- Treat Sixel and other stationary bitmap backends as capable of displaying
  notification artwork. Notcurses emits transparent Sixels with DCS P2=1, and
  foot enables Sixel by default; a movable-only gate unnecessarily demoted foot
  to the cell presentation.
- Account for stationary lifecycle limitations in the notification renderer:
  keep the art opaque, avoid frame-by-frame fade replacement, and retransmit it
  after a lower full-screen bitmap is emitted. Preserve the terminal-native
  framed card exclusively for the no-bitmap policy.
- Settings input batching may collapse only identical navigation repeats. The
  first semantically different command must observe a repaint boundary so it
  always targets the state that the user can see.
