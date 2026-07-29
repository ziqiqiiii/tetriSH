#include "statusbody.h"

/**
 * @brief Serialises the LIST /rooms body: one line per row in the form
 * `<name> <mode> <players>/<slots> <status> <owner>`.
 *
 * An empty list (count 0) is a valid empty body - the UC-03 empty state.
 *
 * @param rows The rows to serialise; may be NULL when count is 0.
 * @param count Number of rows.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length in bytes (0 for an empty list), or -1 with errno =
 *         EINVAL (NULL args with count > 0, bad enum value) or ERANGE
 *         (cap too small).
 */
int	sb_rooms_encode(const t_sb_room_row *rows, size_t count, char *out, size_t cap)
{
	/* TODO: per row, snprintf name, mode token (SINGLE/DOUBLE/
	   BATTLE_ROYALE), players/slots, status token (WAITING/READY/
	   IN_GAME/FINISHED), owner. */
	(void)rows;
	(void)count;
	(void)out;
	(void)cap;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Parses a LIST /rooms body into rows.
 *
 * An empty buffer decodes to zero rows, not an error.
 *
 * @param buf The received body bytes (need not be NUL-terminated).
 * @param len Number of body bytes.
 * @param rows Caller array receiving the decoded rows.
 * @param cap Capacity of rows.
 * @param count Receives the number of rows decoded.
 * @return 0 on success, -1 with errno = EINVAL (NULL args), EBADMSG
 *         (malformed line, unknown mode/status token, overlong name/owner),
 *         or ERANGE (more rows than cap).
 */
int	sb_rooms_decode(const char *buf, size_t len, t_sb_room_row *rows, size_t cap, size_t *count)
{
	/* TODO: split lines within len; parse the five fields per line with
	   length caps; map tokens back to enums. */
	(void)buf;
	(void)len;
	(void)rows;
	(void)cap;
	(void)count;
	errno = ENOSYS;
	return (-1);
}
