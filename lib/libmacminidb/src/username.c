/* ************************************************************************** */
/*                                                                            */
/*   username.c — what a username is allowed to be                            */
/*                                                                            */
/*   A username is not free text: it is written into every body that names a   */
/*   player, and those bodies are lines of space-separated fields. So the      */
/*   rule is the format's, not a policy - a name that cannot be put on the     */
/*   wire is a name the store must never hand back.                            */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Reports whether a name can be stored and put on the wire.
 *
 * Printable ASCII with no space. The space is the load-bearing half: the
 * leaderboard body is `<rank> <username> <score>` and its decoder reads the
 * fields with a whitespace-delimited scan, so one player called "amber lee"
 * shifted every field of that row and the whole leaderboard was rejected as
 * malformed - for everybody, not just for them. The room and profile bodies
 * refuse the space outright, and the chat body refuses it in a sender, which
 * is a message the sender was nevertheless told had been sent.
 *
 * Control characters go with it. A feed and a roster are drawn into a
 * terminal, and an escape sequence in a name is somebody else's cursor.
 *
 * The rule lives here because the store is what persists the name: validating
 * only at the edge would leave a bad row already on disk the next time an
 * edge forgot to ask. tetrisu keeps its own copy for its sign-up form - the
 * client cannot link this archive, and a refusal a player reads while they
 * can still edit the field beats a 400 they have to interpret.
 *
 * @param username Candidate name, NUL-terminated.
 * @return true when the name is non-empty, short enough, and representable.
 */
bool	db_username_valid(const char *username)
{
	size_t	len;
	size_t	index;

	if (!username)
		return (false);
	len = strnlen(username, DB_MAX_USERNAME);
	if (len == 0 || len >= DB_MAX_USERNAME)
		return (false);
	index = 0;
	while (index < len)
	{
		if ((unsigned char)username[index] <= 0x20
			|| (unsigned char)username[index] >= 0x7f)
			return (false);
		index++;
	}
	return (true);
}
