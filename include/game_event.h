/*
 * include/game_event.h - shared cross-application event contract
 *
 * Published by tetrisd (src/tetrisd/event_pub.c) over Unix SOCK_DGRAM
 * sockets and consumed by chatd and marketd, each via their own
 * event_consumer_thread. See docs/architecture.md and
 * docs/corestack.md SS4.5 for the full pipeline description.
 *
 * FROZEN as of Week 4 (end of Sprint 2 - see docs/corestack.md SS4.5 and
 * AGENTS.md rule 6):
 *   - Existing field offsets/types are immutable.
 *   - New event types may only be APPENDED to game_event_type_t.
 *   - New payload fields may only be APPENDED to the payload union.
 *   - Any other change requires a joint decision documented in a GitHub
 *     issue before the commit is made, and review from both members
 *     (AGENTS.md rule 7).
 */
#ifndef GAME_EVENT_H
#define GAME_EVENT_H

#include <stdint.h>

typedef enum {
  GE_PLAYER_JOINED      = 0,
  GE_PLAYER_LEFT        = 1,
  GE_PLAYER_ELIMINATED  = 2,
  GE_GAME_STARTED       = 3,
  GE_GAME_ENDED         = 4,
  GE_LINE_CLEARED       = 5,
  GE_GARBAGE_SENT       = 6,
  GE_ABILITY_USED       = 7,
  GE_ITEM_PURCHASED     = 8,
  GE_CHARACTER_EQUIPPED = 9,
  /* append-only after Week 4 freeze - never renumber */
} game_event_type_t;

/* dest_mask bitmask - an event may fan out to multiple destinations */
#define GE_DEST_CHATD   (1 << 0) /* route to chatd.sock */
#define GE_DEST_MARKETD (1 << 1) /* route to marketd.sock */

/* envelope format version - bump only if version 1 stops being readable */
#define GAME_EVENT_VERSION 1

typedef struct {
  uint8_t version;     /* envelope version, see GAME_EVENT_VERSION */
  uint8_t event_type;  /* game_event_type_t */
  uint32_t seq;        /* monotonic per-room sequence number */
  uint32_t room_id;
  uint32_t player_id;
  uint16_t dest_mask; /* GE_DEST_* bitmask for fan-out routing */
  union {
    struct {
      uint8_t lines;
      uint32_t target_room_id;
    } garbage; /* GE_GARBAGE_SENT */
    struct {
      uint8_t ability_id;
      uint32_t target_player_id;
    } ability; /* GE_ABILITY_USED */
    struct {
      uint32_t points;
    } points_earned; /* GE_LINE_CLEARED, etc. - credited by marketd */
    struct {
      uint32_t item_id;
    } item; /* GE_ITEM_PURCHASED, GE_CHARACTER_EQUIPPED */
  } payload;
} game_event_t;

#endif /* GAME_EVENT_H */
