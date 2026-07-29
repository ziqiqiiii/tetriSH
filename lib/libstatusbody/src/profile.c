#include "statusbody.h"

/**
 * @brief Serialises the UC-20 ProfileView body: one key per line, owned
 * lists count-prefixed (format per the header comment).
 *
 * @param in The profile to serialise.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length in bytes, or -1 with errno = EINVAL (NULL args,
 *         empty username, owned counts > SB_OWNED_MAX) or ERANGE (cap
 *         too small).
 */
int	sb_profile_encode(const t_sb_profile *in, char *out, size_t cap)
{
	/* TODO: snprintf the fixed key lines in order; owned lists as
	   "<count> <id>...". */
	(void)in;
	(void)out;
	(void)cap;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Parses a ProfileView body back into a profile.
 *
 * Strict: keys in encode order, every key present, owned counts within
 * SB_OWNED_MAX, username within SB_USER_MAX.
 *
 * @param buf The received body bytes (need not be NUL-terminated).
 * @param len Number of body bytes.
 * @param out Receives the decoded profile, fully overwritten on success.
 * @return 0 on success, -1 with errno = EINVAL (NULL args) or EBADMSG
 *         (missing key, malformed value, overlong name, list overflow).
 */
int	sb_profile_decode(const char *buf, size_t len, t_sb_profile *out)
{
	/* TODO: zero out; parse fixed key sequence within len; enforce name
	   and owned-list caps. */
	(void)buf;
	(void)len;
	(void)out;
	errno = ENOSYS;
	return (-1);
}
