#include "statusbody.h"

#include <assert.h>
#include <stdio.h>

// Static Functions
static void	make_profile(t_sb_profile *p);

static void	make_profile(t_sb_profile *p)
{
	memset(p, 0, sizeof(*p));
	strcpy(p->username, "alice");
	p->wallet = 500;
	p->score = 1200;
	p->rank = 3;
	p->equipped_character = 2;
	p->equipped_theme = 1;
	p->owned_characters[0] = 1;
	p->owned_characters[1] = 2;
	p->owned_characters[2] = 4;
	p->owned_character_count = 3;
	p->owned_themes[0] = 1;
	p->owned_themes[1] = 3;
	p->owned_theme_count = 2;
}

void	test_profile_round_trip_full(void)
{
	t_sb_profile	in;
	t_sb_profile	back;
	char			out[4096];
	int				n;

	make_profile(&in);
	n = body_profile_encode(&in, out, sizeof(out));
	assert(n > 0);
	memset(&back, 0, sizeof(back));
	assert(body_profile_decode(out, (size_t)n, &back) == 0);
	assert(memcmp(&in, &back, sizeof(in)) == 0);
	printf("PASS test_profile_round_trip_full\n");
}

void	test_profile_round_trip_empty_owned_lists(void)
{
	t_sb_profile	in;
	t_sb_profile	back;
	char			out[4096];
	int				n;

	make_profile(&in);
	in.owned_character_count = 0;
	in.owned_theme_count = 0;
	memset(in.owned_characters, 0, sizeof(in.owned_characters));
	memset(in.owned_themes, 0, sizeof(in.owned_themes));
	n = body_profile_encode(&in, out, sizeof(out));
	assert(n > 0);
	memset(&back, 0, sizeof(back));
	assert(body_profile_decode(out, (size_t)n, &back) == 0);
	assert(back.owned_character_count == 0);
	assert(back.owned_theme_count == 0);
	assert(strcmp(back.username, "alice") == 0);
	printf("PASS test_profile_round_trip_empty_owned_lists\n");
}

void	test_profile_decode_rejects_owned_overflow(void)
{
	t_sb_profile	back;
	char			body[8192];
	char			num[16];
	int				i;

	strcpy(body, "username alice\nwallet 500\nscore 1200\nrank 3\n"
		"equipped_character 2\nequipped_theme 1\nowned_characters 65");
	i = 0;
	while (i < 65) // one more id than BODY_OWNED_MAX
	{
		snprintf(num, sizeof(num), " %d", i + 1);
		strcat(body, num);
		i++;
	}
	strcat(body, "\nowned_themes 0\n");
	assert(body_profile_decode(body, strlen(body), &back) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_profile_decode_rejects_owned_overflow\n");
}

void	test_profile_decode_rejects_missing_or_overlong_username(void)
{
	t_sb_profile	back;
	const char		*no_username;
	const char		*long_username;

	no_username = "wallet 500\nscore 1200\nrank 3\nequipped_character 2\n"
		"equipped_theme 1\nowned_characters 0\nowned_themes 0\n";
	assert(body_profile_decode(no_username, strlen(no_username), &back) == -1);
	assert(errno == EBADMSG);
	long_username = "username abcdefghijklmnopqrstuvwxyz0123456\n"
		"wallet 500\nscore 1200\nrank 3\nequipped_character 2\n"
		"equipped_theme 1\nowned_characters 0\nowned_themes 0\n";
	assert(body_profile_decode(long_username, strlen(long_username),
			&back) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_profile_decode_rejects_missing_or_overlong_username\n");
}

int	main(void)
{
	test_profile_round_trip_full();
	test_profile_round_trip_empty_owned_lists();
	test_profile_decode_rejects_owned_overflow();
	test_profile_decode_rejects_missing_or_overlong_username();
	return (0);
}
