#include "tetrisroom.h"

#include <assert.h>
#include <stdio.h>

// Static Variables
static t_lobby	g_lobby;

// Static Functions
static t_lobby	*fresh_lobby(void);
static t_room	*add_room_with_status(t_lobby *l, t_player_id owner, t_room_status status);

static t_lobby	*fresh_lobby(void)
{
	memset(&g_lobby, 0, sizeof(g_lobby));
	assert(lobby_init(&g_lobby, 8, 8) == 0);
	return (&g_lobby);
}

static t_room	*add_room_with_status(t_lobby *l, t_player_id owner, t_room_status status)
{
	t_room	*r;

	assert(lobby_create_room(l, MODE_DOUBLE, &r) == 0);
	assert(room_seat(r, owner, "owner", NULL, NULL) == 1);
	r->status = status;
	return (r);
}

void	test_lobby_init_clamps_caps(void)
{
	memset(&g_lobby, 0, sizeof(g_lobby));
	assert(lobby_init(&g_lobby, 0, 8) == 0);
	assert(g_lobby.max_rooms >= 1);
	memset(&g_lobby, 0, sizeof(g_lobby));
	assert(lobby_init(&g_lobby, LOBBY_MAX_ROOMS + 50, 8) == 0);
	assert(g_lobby.max_rooms == LOBBY_MAX_ROOMS);
	assert(lobby_room_count(&g_lobby) == 0);
	printf("PASS test_lobby_init_clamps_caps\n");
}

void	test_create_room_registers_with_unique_generated_ids(void)
{
	t_lobby	*l;
	t_room	*a;
	t_room	*b;

	l = fresh_lobby();
	assert(lobby_create_room(l, MODE_DOUBLE, &a) == 0);
	assert(lobby_create_room(l, MODE_DOUBLE, &b) == 0);
	assert(a->id == 1 && b->id == 2); // that mode's counter starts at 1
	assert(strcmp(a->name, "D-01") == 0);
	assert(strcmp(b->name, "D-02") == 0);
	assert(lobby_find_room(l, a->name) == a); // same instance, not a copy
	assert(lobby_find_room(l, b->name) == b);
	assert(lobby_room_count(l) == 2);
	printf("PASS test_create_room_registers_with_unique_generated_ids\n");
}

void	test_create_room_ids_run_per_mode(void)
{
	t_lobby	*l;
	t_room	*s1;
	t_room	*d1;
	t_room	*br1;
	t_room	*d2;

	l = fresh_lobby();
	assert(lobby_create_room(l, MODE_SINGLE, &s1) == 0);
	assert(lobby_create_room(l, MODE_DOUBLE, &d1) == 0);
	assert(lobby_create_room(l, MODE_BATTLE_ROYALE, &br1) == 0);
	assert(lobby_create_room(l, MODE_DOUBLE, &d2) == 0);
	// each mode counts independently, so ids repeat across modes...
	assert(s1->id == 1 && d1->id == 1 && br1->id == 1);
	assert(d2->id == 2);
	// ...and only the prefixed name is unique lobby-wide
	assert(strcmp(s1->name, "S-01") == 0);
	assert(strcmp(d1->name, "D-01") == 0);
	assert(strcmp(br1->name, "BR-01") == 0);
	assert(strcmp(d2->name, "D-02") == 0);
	assert(lobby_find_room(l, "S-01") == s1);
	assert(lobby_find_room(l, "D-01") == d1);
	printf("PASS test_create_room_ids_run_per_mode\n");
}

void	test_destroyed_room_id_is_not_recycled(void)
{
	t_lobby	*l;
	t_room	*a;
	t_room	*b;

	l = fresh_lobby();
	assert(lobby_create_room(l, MODE_DOUBLE, &a) == 0);
	assert(lobby_destroy_room(l, "D-01") == 0);
	// the counter is monotonic: a stale rejoin of D-01 finds nothing
	assert(lobby_create_room(l, MODE_DOUBLE, &b) == 0);
	assert(b->id == 2);
	assert(strcmp(b->name, "D-02") == 0);
	assert(lobby_find_room(l, "D-01") == NULL);
	printf("PASS test_destroyed_room_id_is_not_recycled\n");
}

void	test_create_room_does_not_seat_owner(void)
{
	t_lobby	*l;
	t_room	*r;

	l = fresh_lobby();
	assert(lobby_create_room(l, MODE_DOUBLE, &r) == 0);
	assert(r->number_of_players == 0); // the controller seats afterwards
	assert(r->status == ROOM_WAITING);
	printf("PASS test_create_room_does_not_seat_owner\n");
}

void	test_create_room_at_capacity_fails(void)
{
	t_lobby	*l;
	t_room	*r;
	size_t	i;

	l = fresh_lobby();
	i = 0;
	while (i < l->max_rooms)
	{
		assert(lobby_create_room(l, MODE_DOUBLE, &r) == 0);
		i++;
	}
	assert(lobby_create_room(l, MODE_DOUBLE, &r) == -1);
	assert(lobby_room_count(l) == l->max_rooms);
	printf("PASS test_create_room_at_capacity_fails\n");
}

void	test_find_room_unknown_name_returns_null(void)
{
	t_lobby	*l;
	t_room	*r;

	l = fresh_lobby();
	assert(lobby_create_room(l, MODE_DOUBLE, &r) == 0);
	assert(lobby_find_room(l, "UNKNOWN") == NULL);
	assert(lobby_find_room(l, "S-01") == NULL); // right id, wrong mode
	assert(lobby_room_count(l) == 1); // no negative-cache insertion
	printf("PASS test_find_room_unknown_name_returns_null\n");
}

void	test_destroy_room_removes_only_that_room(void)
{
	t_lobby	*l;
	t_room	*a;
	t_room	*b;
	char	gone[ROOM_NAME_MAX];

	l = fresh_lobby();
	assert(lobby_create_room(l, MODE_DOUBLE, &a) == 0);
	assert(lobby_create_room(l, MODE_DOUBLE, &b) == 0);
	memcpy(gone, a->name, sizeof(gone));
	assert(lobby_destroy_room(l, gone) == 0);
	assert(lobby_find_room(l, gone) == NULL);
	assert(lobby_find_room(l, b->name) == b); // the other room untouched
	assert(lobby_room_count(l) == 1);
	printf("PASS test_destroy_room_removes_only_that_room\n");
}

void	test_destroy_unknown_room_fails(void)
{
	t_lobby	*l;

	l = fresh_lobby();
	assert(lobby_destroy_room(l, "UNKNOWN") == -1);
	assert(lobby_room_count(l) == 0);
	printf("PASS test_destroy_unknown_room_fails\n");
}

void	test_list_open_excludes_in_game_and_finished(void)
{
	t_lobby			*l;
	t_room_summary	out[8];
	size_t			n;
	size_t			i;

	l = fresh_lobby();
	add_room_with_status(l, 11, ROOM_WAITING);
	add_room_with_status(l, 12, ROOM_READY);
	add_room_with_status(l, 13, ROOM_IN_GAME);
	add_room_with_status(l, 14, ROOM_FINISHED);
	n = lobby_list_open(l, out, 8);
	assert(n == 2);
	i = 0;
	while (i < n)
	{
		assert(out[i].status == ROOM_WAITING || out[i].status == ROOM_READY);
		i++;
	}
	assert(lobby_room_count(l) == 4); // listing never mutates the lobby
	printf("PASS test_list_open_excludes_in_game_and_finished\n");
}

void	test_list_open_respects_caller_cap(void)
{
	t_lobby			*l;
	t_room_summary	out[2];

	l = fresh_lobby();
	add_room_with_status(l, 11, ROOM_WAITING);
	add_room_with_status(l, 12, ROOM_WAITING);
	add_room_with_status(l, 13, ROOM_WAITING);
	assert(lobby_list_open(l, out, 2) == 2);
	printf("PASS test_list_open_respects_caller_cap\n");
}

void	test_list_all_includes_every_status(void)
{
	t_lobby			*l;
	t_room_snapshot	out[8];

	l = fresh_lobby();
	add_room_with_status(l, 11, ROOM_WAITING);
	add_room_with_status(l, 12, ROOM_READY);
	add_room_with_status(l, 13, ROOM_IN_GAME);
	add_room_with_status(l, 14, ROOM_FINISHED);
	assert(lobby_list_all(l, out, 8) == 4); // admin sees everything
	printf("PASS test_list_all_includes_every_status\n");
}

int	main(void)
{
	test_lobby_init_clamps_caps();
	test_create_room_registers_with_unique_generated_ids();
	test_create_room_ids_run_per_mode();
	test_create_room_does_not_seat_owner();
	test_create_room_at_capacity_fails();
	test_find_room_unknown_name_returns_null();
	test_destroy_room_removes_only_that_room();
	test_destroyed_room_id_is_not_recycled();
	test_destroy_unknown_room_fails();
	test_list_open_excludes_in_game_and_finished();
	test_list_open_respects_caller_cap();
	test_list_all_includes_every_status();
	return (0);
}
