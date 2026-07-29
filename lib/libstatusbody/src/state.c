#include "statusbody.h"

/**
 * @brief Serialises one STATE snapshot into the application/tetris-state
 * body (key order and board block per the header comment).
 *
 * Deterministic: identical frames produce identical bytes.
 *
 * @param in The frame to serialise.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length in bytes, or -1 with errno = EINVAL (NULL args or a
 *         field out of range) or ERANGE (cap too small).
 */
int	sb_state_encode(const t_sb_state *in, char *out, size_t cap)
{
	/* TODO: validate args + ranges (phase, charge 0-10, clearing_count
	   0-4, cell type/color nibbles); snprintf key lines in order; board
	   block as 20 rows x 10 hex pairs. */
	(void)in;
	(void)out;
	(void)cap;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Parses an application/tetris-state body back into a frame.
 *
 * Strict: keys must appear in encode order, every key present, exactly 20
 * board rows of 20 hex chars, and nothing after the last row.
 *
 * @param buf The received body bytes (need not be NUL-terminated).
 * @param len Number of body bytes.
 * @param out Receives the decoded frame, fully overwritten on success.
 * @return 0 on success, -1 with errno = EINVAL (NULL args) or EBADMSG
 *         (missing/misordered key, malformed value, out-of-range number,
 *         bad board row, trailing junk).
 */
int	sb_state_decode(const char *buf, size_t len, t_sb_state *out)
{
	/* TODO: zero out; parse fixed key sequence within len; range-check
	   every number; hex-decode the board block; reject leftovers. */
	(void)buf;
	(void)len;
	(void)out;
	errno = ENOSYS;
	return (-1);
}
