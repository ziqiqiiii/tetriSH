// Unit tests for player.c — player_t <-> on-disk byte image round-trip.
// player_serialise / player_deserialise are internal, so this suite pulls in
// internal.h directly (the library's other modules are still stubs).
#include "internal.h"
#include <assert.h>

#define PLAYER_BUF	4096

// Static Functions
static t_player	sample_player(void);

void	test_roundtrip_preserves_all_fields(void)
{
	t_player	in;
	t_player	out;
	uint8_t		buf[PLAYER_BUF];
	size_t		n;

	in = sample_player();
	n = player_serialise(&in, buf, sizeof(buf));
	assert(n > 0);
	assert(player_deserialise(buf, n, &out) == DB_OK);
	assert(memcmp(&in, &out, sizeof(t_player)) == 0);
	printf("PASS test_roundtrip_preserves_all_fields\n");
}

void	test_empty_owned_lists_roundtrip(void)
{
	t_player	in;
	t_player	out;
	uint8_t		buf[PLAYER_BUF];
	size_t		n;

	in = sample_player();
	in.owned_characters_count = 0;
	in.owned_themes_count = 0;
	memset(in.owned_characters, 0, sizeof(in.owned_characters));
	memset(in.owned_themes, 0, sizeof(in.owned_themes));
	n = player_serialise(&in, buf, sizeof(buf));
	assert(n > 0);
	assert(player_deserialise(buf, n, &out) == DB_OK);
	assert(out.owned_characters_count == 0);
	assert(out.owned_themes_count == 0);
	assert(memcmp(&in, &out, sizeof(t_player)) == 0);
	printf("PASS test_empty_owned_lists_roundtrip\n");
}

void	test_serialise_zero_when_over_cap(void)
{
	t_player	in;
	uint8_t		buf[8];

	in = sample_player();
	assert(player_serialise(&in, buf, sizeof(buf)) == 0);
	printf("PASS test_serialise_zero_when_over_cap\n");
}

void	test_deserialise_rejects_short_buffer(void)
{
	t_player	in;
	t_player	out;
	uint8_t		buf[PLAYER_BUF];
	size_t		n;

	in = sample_player();
	n = player_serialise(&in, buf, sizeof(buf));
	assert(n > 1);
	assert(player_deserialise(buf, n - 1, &out) == DB_INVALID);
	assert(player_deserialise(buf, 0, &out) == DB_INVALID);
	printf("PASS test_deserialise_rejects_short_buffer\n");
}

void	test_deserialise_rejects_oversized_owned_count(void)
{
	t_player	in;
	t_player	out;
	uint8_t		buf[PLAYER_BUF];
	size_t		n;
	size_t		owned_off;

	in = sample_player();
	n = player_serialise(&in, buf, sizeof(buf));
	// owned_characters count sits right after the two equipped u32s, which
	// follow id(8) + username + hash + salt + two score u64s.
	owned_off = 8 + DB_MAX_USERNAME + DB_HASH_LEN + DB_SALT_LEN + 16 + 8;
	buf[owned_off] = 0xFF;
	buf[owned_off + 1] = 0xFF;
	buf[owned_off + 2] = 0xFF;
	buf[owned_off + 3] = 0xFF;
	assert(player_deserialise(buf, n, &out) == DB_INVALID);
	printf("PASS test_deserialise_rejects_oversized_owned_count\n");
}

int	main(void)
{
	test_roundtrip_preserves_all_fields();
	test_empty_owned_lists_roundtrip();
	test_serialise_zero_when_over_cap();
	test_deserialise_rejects_short_buffer();
	test_deserialise_rejects_oversized_owned_count();
	return (0);
}

static t_player	sample_player(void)
{
	t_player	p;
	size_t		i;

	memset(&p, 0, sizeof(p));
	p.player_id = 0xABCDEF0123456789ULL;
	memcpy(p.username, "macmini_player", 14);
	memset(p.password_hashed, 'h', DB_HASH_LEN);
	memset(p.salt, 's', DB_SALT_LEN);
	p.leaderboard_score = -42;
	p.wallet_points = 1337;
	p.current_equipped_character = 3;
	p.current_equipped_theme = 7;
	p.owned_characters_count = 3;
	i = 0;
	while (i < p.owned_characters_count)
	{
		p.owned_characters[i] = (t_item_id)(i + 1);
		i++;
	}
	p.owned_themes_count = 2;
	p.owned_themes[0] = 10;
	p.owned_themes[1] = 20;
	p.games_played = 99;
	p.games_won = 41;
	return (p);
}
