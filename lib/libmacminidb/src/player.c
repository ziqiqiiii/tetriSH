/* ************************************************************************** */
/*                                                                            */
/*   player.c — player_t <-> on-disk record (§4 document schema)              */
/*                                                                            */
/*   One player document serialises to one whole-record log entry and back.   */
/*   The payload is the byte image read/written by log.c.                     */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// TODO: player_serialise   (player_t -> bytes; return 0 if it would exceed cap)
// TODO: player_deserialise (bytes -> player_t; DB_INVALID on a short/bad buffer)
