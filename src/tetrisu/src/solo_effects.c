#include "tetrisu.h"

/*
** Noticing that something has been done to you.
**
** Every effect in the catalogue was enforced by tetrisd and invisible to
** tetrisu. Paralysis worked perfectly and looked exactly like a rotate key
** that had stopped responding; Inversion looked like the arrow keys had been
** swapped by the terminal; Nue looked like a stuck spacebar. The server was
** right every time and the player had no way to know it.
**
** So the client compares the counts it was last told against the ones that
** just arrived, and anything that went up is something that just landed. It
** is a comparison rather than an event on the wire on purpose: a snapshot is
** a statement of what is true now, not a log of what happened, and every
** other part of this client reads it that way. A missed frame then costs at
** worst one card and never a wrong one - which an event stream on a
** latest-wins mailbox could not promise.
**
** Nothing here enforces anything. The server refuses the rotation; this only
** says why.
*/

// Static Functions
static const char	*effect_message(int index, int count);

// Static Variables

/*
** In the order t_solo_effects declares them. Mirror is the one that is good
** news, and it is still worth a card: a player who does not know they are
** holding one cannot decide to spend a piece staying alive until somebody
** wastes an ability on them.
*/
static const char *const	g_effect_titles[] = {
	"PARALYSED",
	"CONTROLS INVERTED",
	"NUE",
	"THWACK",
	"FRIED",
	"BLACKOUT",
	"PALS",
	"MIRROR READY"
};

/**
 * @brief Reports the first effect that has newly landed, and remembers it.
 *
 * "Newly" means the count went up. A count that merely ran down is the effect
 * expiring, which needs no announcement, and a count that was refreshed at the
 * same value is the same effect still running - so only a rise is news.
 *
 * One card per call rather than a queue of them: two abilities landing on the
 * same lock is rare, the second is reported on the next frame, and a player
 * buried under four stacked cards can no longer see the board the cards are
 * about.
 *
 * @param now The effects the latest snapshot carried.
 * @param last The effects last reported; updated to `now` before returning.
 * @param title Receives the effect's name.
 * @param title_cap Capacity of title.
 * @param message Receives what it does, in the player's terms.
 * @param message_cap Capacity of message.
 * @return true when something landed and the buffers were written.
 */
bool	solo_effects_take_arrival(const t_solo_effects *now,
		t_solo_effects *last, char *title, size_t title_cap,
		char *message, size_t message_cap)
{
	const int	*fresh;
	const int	*previous;
	int			index;
	bool		found;

	if (now == NULL || last == NULL || title == NULL || message == NULL)
		return (false);
	fresh = &now->paralysis;
	previous = &last->paralysis;
	index = 0;
	found = false;
	while (index < SOLO_EFFECT_COUNT && !found)
	{
		if (fresh[index] > previous[index])
		{
			snprintf(title, title_cap, "%s", g_effect_titles[index]);
			snprintf(message, message_cap, "%s",
				effect_message(index, fresh[index]));
			found = true;
		}
		index++;
	}
	*last = *now;
	return (found);
}

/**
 * @brief Says what one effect does, in pieces rather than in mechanics.
 *
 * A player mid-piece needs to know what they cannot do and for how long, not
 * which character sent it. The four piece-counted effects therefore carry
 * their count and the rest do not, because the rest have no number a player
 * could act on.
 *
 * @param index Effect index, in t_solo_effects order.
 * @param count The count the snapshot carried.
 * @return A short line, never NULL.
 */
static const char	*effect_message(int index, int count)
{
	static char	line[UI_NOTIFICATION_MESSAGE_MAX + 1];

	if (index == 0)
		snprintf(line, sizeof(line), "NO ROTATION FOR %d PIECES", count);
	else if (index == 1)
		snprintf(line, sizeof(line), "LEFT IS RIGHT FOR %d PIECES", count);
	else if (index == 2)
		snprintf(line, sizeof(line), "NO FAST DROP FOR %d PIECES", count);
	else if (index == 3)
		snprintf(line, sizeof(line), "BLOCKS CASCADE FOR %d PIECES", count);
	else if (index == 4)
		snprintf(line, sizeof(line), "%d ROWS BURN AT THE NEXT LOCK", count);
	else if (index == 5)
		snprintf(line, sizeof(line), "YOUR FIELD IS DARK FOR %d PIECES",
			count);
	else if (index == 6)
		snprintf(line, sizeof(line), "GARBAGE LOWERS YOUR STACK");
	else
		snprintf(line, sizeof(line), "THE NEXT ABILITY REBOUNDS");
	return (line);
}
