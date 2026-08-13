#include "tetrisu.h"

#include <assert.h>
#include <string.h>

/*
** Noticing that something has been done to you.
**
** Every effect in the catalogue was enforced by tetrisd and invisible here.
** Paralysis worked perfectly and looked exactly like a rotate key that had
** stopped responding; the server was right every time and the player had no
** way to know it. The client now compares what it was last told against what
** just arrived, and anything that went up is news.
**
** A comparison rather than an event on the wire, deliberately: a snapshot
** says what is true now, and the STATE lane is a latest-wins mailbox, so an
** event stream on it could lose one. A comparison that misses a frame costs
** at worst one card and never a wrong one.
*/

// Static Functions
static void	test_nothing_is_not_news(void);
static void	test_a_rising_count_is_an_arrival(void);
static void	test_a_falling_count_is_not(void);
static void	test_a_refresh_at_the_same_value_is_not(void);
static void	test_one_card_at_a_time_worst_first(void);
static void	test_every_effect_has_something_to_say(void);
static void	test_null_arguments_are_refused(void);

int	main(void)
{
	test_nothing_is_not_news();
	test_a_rising_count_is_an_arrival();
	test_a_falling_count_is_not();
	test_a_refresh_at_the_same_value_is_not();
	test_one_card_at_a_time_worst_first();
	test_every_effect_has_something_to_say();
	test_null_arguments_are_refused();
	return (0);
}

/*
** The overwhelmingly common frame: nothing is riding on the player and
** nothing was before. A card raised here would appear on every tick.
*/
static void	test_nothing_is_not_news(void)
{
	t_solo_effects	now;
	t_solo_effects	last;
	char			title[UI_NOTIFICATION_TITLE_MAX + 1];
	char			message[UI_NOTIFICATION_MESSAGE_MAX + 1];

	memset(&now, 0, sizeof(now));
	memset(&last, 0, sizeof(last));
	assert(!solo_effects_take_arrival(&now, &last, title, sizeof(title),
			message, sizeof(message)));
	printf("PASS test_nothing_is_not_news\n");
}

/*
** A count that went up is something that just landed, and the player is told
** what it stops them doing rather than which character sent it.
*/
static void	test_a_rising_count_is_an_arrival(void)
{
	t_solo_effects	now;
	t_solo_effects	last;
	char			title[UI_NOTIFICATION_TITLE_MAX + 1];
	char			message[UI_NOTIFICATION_MESSAGE_MAX + 1];

	memset(&now, 0, sizeof(now));
	memset(&last, 0, sizeof(last));
	now.paralysis = 3;
	assert(solo_effects_take_arrival(&now, &last, title, sizeof(title),
			message, sizeof(message)));
	assert(strcmp(title, "PARALYSED") == 0);
	assert(strstr(message, "3") != NULL);
	/* Taking it remembers it, so the same frame is not news twice. */
	assert(last.paralysis == 3);
	assert(!solo_effects_take_arrival(&now, &last, title, sizeof(title),
			message, sizeof(message)));
	printf("PASS test_a_rising_count_is_an_arrival\n");
}

/*
** A count running down is the effect expiring. Announcing that would put a
** card up for each of the three pieces it takes Paralysis to wear off, which
** is three cards saying nothing arrived.
*/
static void	test_a_falling_count_is_not(void)
{
	t_solo_effects	now;
	t_solo_effects	last;
	char			title[UI_NOTIFICATION_TITLE_MAX + 1];
	char			message[UI_NOTIFICATION_MESSAGE_MAX + 1];

	memset(&now, 0, sizeof(now));
	memset(&last, 0, sizeof(last));
	last.paralysis = 3;
	now.paralysis = 2;
	assert(!solo_effects_take_arrival(&now, &last, title, sizeof(title),
			message, sizeof(message)));
	assert(last.paralysis == 2);
	now.paralysis = 0;
	assert(!solo_effects_take_arrival(&now, &last, title, sizeof(title),
			message, sizeof(message)));
	printf("PASS test_a_falling_count_is_not\n");
}

/*
** A second Paralysis while one is running restarts the counter at the same
** value, which reads as unchanged. That is the one arrival this cannot see,
** and it is the right trade: the alternative is announcing every frame of an
** effect that has not moved.
*/
static void	test_a_refresh_at_the_same_value_is_not(void)
{
	t_solo_effects	now;
	t_solo_effects	last;
	char			title[UI_NOTIFICATION_TITLE_MAX + 1];
	char			message[UI_NOTIFICATION_MESSAGE_MAX + 1];

	memset(&now, 0, sizeof(now));
	memset(&last, 0, sizeof(last));
	last.nue = 4;
	now.nue = 4;
	assert(!solo_effects_take_arrival(&now, &last, title, sizeof(title),
			message, sizeof(message)));
	printf("PASS test_a_refresh_at_the_same_value_is_not\n");
}

/*
** Two at once is rare and possible. The second is reported on the next frame
** rather than stacked, because a player buried under four cards can no longer
** see the board the cards are about.
*/
static void	test_one_card_at_a_time_worst_first(void)
{
	t_solo_effects	now;
	t_solo_effects	last;
	char			title[UI_NOTIFICATION_TITLE_MAX + 1];
	char			message[UI_NOTIFICATION_MESSAGE_MAX + 1];

	memset(&now, 0, sizeof(now));
	memset(&last, 0, sizeof(last));
	now.paralysis = 3;
	now.dark = 4;
	assert(solo_effects_take_arrival(&now, &last, title, sizeof(title),
			message, sizeof(message)));
	assert(strcmp(title, "PARALYSED") == 0);
	/* Both were remembered, so the second does not arrive a frame later. */
	assert(last.dark == 4);
	assert(!solo_effects_take_arrival(&now, &last, title, sizeof(title),
			message, sizeof(message)));
	printf("PASS test_one_card_at_a_time_worst_first\n");
}

/*
** Every one of the eight has a title and a message. An effect that landed
** with an empty card would be worse than no card: the player would know
** something happened and not what.
*/
static void	test_every_effect_has_something_to_say(void)
{
	t_solo_effects	now;
	t_solo_effects	last;
	char			title[UI_NOTIFICATION_TITLE_MAX + 1];
	char			message[UI_NOTIFICATION_MESSAGE_MAX + 1];
	int				*field;
	int				index;

	index = 0;
	while (index < SOLO_EFFECT_COUNT)
	{
		memset(&now, 0, sizeof(now));
		memset(&last, 0, sizeof(last));
		field = &now.paralysis;
		field[index] = 2;
		assert(solo_effects_take_arrival(&now, &last, title, sizeof(title),
				message, sizeof(message)));
		assert(title[0] != '\0' && message[0] != '\0');
		index++;
	}
	printf("PASS test_every_effect_has_something_to_say\n");
}

/*
** Called from a render loop, so a missing model must not be a crash.
*/
static void	test_null_arguments_are_refused(void)
{
	t_solo_effects	now;
	t_solo_effects	last;
	char			title[UI_NOTIFICATION_TITLE_MAX + 1];
	char			message[UI_NOTIFICATION_MESSAGE_MAX + 1];

	memset(&now, 0, sizeof(now));
	memset(&last, 0, sizeof(last));
	assert(!solo_effects_take_arrival(NULL, &last, title, sizeof(title),
			message, sizeof(message)));
	assert(!solo_effects_take_arrival(&now, NULL, title, sizeof(title),
			message, sizeof(message)));
	assert(!solo_effects_take_arrival(&now, &last, NULL, 0,
			message, sizeof(message)));
	assert(!solo_effects_take_arrival(&now, &last, title, sizeof(title),
			NULL, 0));
	printf("PASS test_null_arguments_are_refused\n");
}
