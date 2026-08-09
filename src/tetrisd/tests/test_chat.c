/* ************************************************************************** */
/*                                                                            */
/*   test_chat.c - the room feed, both authors                                */
/*                                                                            */
/*   Narration and player chat share one lane, one body and one counter, so   */
/*   they are asserted together: what the server says when a room changes     */
/*   hands, what a player may say, and the four ways CHAT is refused. The     */
/*   ordering the test plan fixes is here too - a status reaches the client   */
/*   before the narration about it, and a refused join narrates nothing.      */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_creating_a_room_narrates_join_then_ownership(void);
static void	test_joining_narrates_to_everyone_seated(void);
static void	test_leaving_narrates_departure_and_succession(void);
static void	test_a_refused_join_narrates_nothing(void);
static void	test_chat_reaches_the_room_including_its_sender(void);
static void	test_chat_outside_your_room_is_not_found(void);
static void	test_chat_refuses_empty_and_unprintable_text(void);
static void	test_feed_sequence_numbers_one_room_in_order(void);

static int		player(t_fixture *fx, t_harness *hc, const char *name);
static int		simple(t_harness *hc, const char *method, const char *path,
					const char *body);
static int		say(t_harness *hc, const char *room, const char *text);
static void		expect_narration(t_harness *hc, const char *text);
static void		drain(t_harness *hc);

int	main(void)
{
	test_creating_a_room_narrates_join_then_ownership();
	test_joining_narrates_to_everyone_seated();
	test_leaving_narrates_departure_and_succession();
	test_a_refused_join_narrates_nothing();
	test_chat_reaches_the_room_including_its_sender();
	test_chat_outside_your_room_is_not_found();
	test_chat_refuses_empty_and_unprintable_text();
	test_feed_sequence_numbers_one_room_in_order();
	return (0);
}

/*
** UC-04.7 asks for both lines and the test plan fixes their order: a feed
** that announced the join without the ownership would leave the room looking
** like nobody could start it.
*/
static void	test_creating_a_room_narrates_join_then_ownership(void)
{
	t_fixture	fx;
	t_harness	hc;
	char		room[ROOM_NAME_MAX];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(hc_join_new(&hc, "double", room, sizeof(room)) == 201);
	expect_narration(&hc, "PLAYER amber joined the room D-01");
	expect_narration(&hc, "PLAYER amber set as owner");
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_creating_a_room_narrates_join_then_ownership\n");
}

static void	test_joining_narrates_to_everyone_seated(void)
{
	t_fixture	fx;
	t_harness	owner;
	t_harness	guest;
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &owner, "amber") == 0);
	assert(hc_join_new(&owner, "double", room, sizeof(room)) == 201);
	drain(&owner);
	assert(player(&fx, &guest, "blake") == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&guest, "JOIN", path, NULL) == 200);
	/* the seated player hears it, and so does the one who just arrived */
	expect_narration(&owner, "PLAYER blake joined the room D-01");
	expect_narration(&guest, "PLAYER blake joined the room D-01");
	hc_close(&guest);
	hc_close(&owner);
	fx_stop(&fx);
	printf("PASS test_joining_narrates_to_everyone_seated\n");
}

/*
** UT-49: the successor is named after the role has already changed, so a
** client that redraws on this line never draws a room with no owner.
*/
static void	test_leaving_narrates_departure_and_succession(void)
{
	t_fixture	fx;
	t_harness	owner;
	t_harness	guest;
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &owner, "amber") == 0);
	assert(hc_join_new(&owner, "double", room, sizeof(room)) == 201);
	assert(player(&fx, &guest, "blake") == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&guest, "JOIN", path, NULL) == 200);
	drain(&guest);
	assert(simple(&owner, "LEAVE", path, NULL) == 200);
	expect_narration(&guest, "PLAYER amber left the room D-01");
	expect_narration(&guest, "PLAYER blake set as the owner");
	hc_close(&guest);
	hc_close(&owner);
	fx_stop(&fx);
	printf("PASS test_leaving_narrates_departure_and_succession\n");
}

/*
** The test plan asserts zero narrations on a refused join. The room is full,
** so the only thing the seated players should hear is nothing - which is
** checked by asking for a message and requiring the wait to time out.
*/
static void	test_a_refused_join_narrates_nothing(void)
{
	t_fixture	fx;
	t_harness	owner;
	t_harness	guest;
	t_body_chat	chat;
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &owner, "amber") == 0);
	assert(hc_join_new(&owner, "single", room, sizeof(room)) == 201);
	drain(&owner);
	assert(player(&fx, &guest, "blake") == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&guest, "JOIN", path, NULL) == 409);
	assert(hc_wait_chat(&owner, &chat, 200) == -1);
	hc_close(&guest);
	hc_close(&owner);
	fx_stop(&fx);
	printf("PASS test_a_refused_join_narrates_nothing\n");
}

static void	test_chat_reaches_the_room_including_its_sender(void)
{
	t_fixture	fx;
	t_harness	owner;
	t_harness	guest;
	t_body_chat	heard;
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &owner, "amber") == 0);
	assert(hc_join_new(&owner, "double", room, sizeof(room)) == 201);
	assert(player(&fx, &guest, "blake") == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&guest, "JOIN", path, NULL) == 200);
	drain(&owner);
	drain(&guest);
	assert(say(&guest, room, "good luck all") == 200);
	assert(hc_wait_chat(&owner, &heard, HC_TIMEOUT_MS) == 0);
	assert(!heard.system);
	assert(strcmp(heard.sender, "blake") == 0);
	assert(strcmp(heard.text, "good luck all") == 0);
	assert(hc_wait_chat(&guest, &heard, HC_TIMEOUT_MS) == 0);
	assert(strcmp(heard.sender, "blake") == 0);
	hc_close(&guest);
	hc_close(&owner);
	fx_stop(&fx);
	printf("PASS test_chat_reaches_the_room_including_its_sender\n");
}

static void	test_chat_outside_your_room_is_not_found(void)
{
	t_fixture	fx;
	t_harness	hc;
	char		room[ROOM_NAME_MAX];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(say(&hc, "S-99", "anyone there") == 404);
	assert(hc_join_new(&hc, "single", room, sizeof(room)) == 201);
	drain(&hc);
	assert(say(&hc, "S-99", "still not there") == 404);
	assert(say(&hc, room, "but here I am") == 200);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_chat_outside_your_room_is_not_found\n");
}

/*
** An escape sequence arriving as chat is somebody else's cursor, and a
** newline would end the body's line early and re-decode as another key.
** Both are refused at the door rather than escaped.
*/
static void	test_chat_refuses_empty_and_unprintable_text(void)
{
	t_fixture		fx;
	t_harness		hc;
	t_htttp_message	resp;
	char			room[ROOM_NAME_MAX];
	char			path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(hc_join_new(&hc, "single", room, sizeof(room)) == 201);
	drain(&hc);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&hc, "CHAT", path, "text \n") == 400);
	assert(simple(&hc, "CHAT", path, "room S-01\n") == 400);
	assert(hc_request(&hc, "CHAT", path, "text \033[2Jgotcha\n", &resp) == 0);
	assert(resp.status_code == 400);
	htttp_message_free(&resp);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_chat_refuses_empty_and_unprintable_text\n");
}

/*
** One room, one counter, no gaps: a client that receives 1 then 3 knows its
** chat ring dropped something, which is the only reason the number is on the
** wire at all.
*/
static void	test_feed_sequence_numbers_one_room_in_order(void)
{
	t_fixture	fx;
	t_harness	hc;
	t_body_chat	heard;
	char		room[ROOM_NAME_MAX];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(hc_join_new(&hc, "single", room, sizeof(room)) == 201);
	assert(hc_wait_chat(&hc, &heard, HC_TIMEOUT_MS) == 0);
	assert(heard.seq == 1 && heard.system);
	assert(hc_wait_chat(&hc, &heard, HC_TIMEOUT_MS) == 0);
	assert(heard.seq == 2);
	assert(say(&hc, room, "third line") == 200);
	assert(hc_wait_chat(&hc, &heard, HC_TIMEOUT_MS) == 0);
	assert(heard.seq == 3 && !heard.system);
	assert(heard.at > 0);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_feed_sequence_numbers_one_room_in_order\n");
}

static int	player(t_fixture *fx, t_harness *hc, const char *name)
{
	if (hc_connect(hc, fx) != 0)
		return (-1);
	if (hc_signup(hc, name, "hunter2") != 201)
		return (-1);
	if (hc_login(hc, name, "hunter2") != 200)
		return (-1);
	return (0);
}

static int	simple(t_harness *hc, const char *method, const char *path,
			const char *body)
{
	t_htttp_message	resp;
	int				status;

	if (hc_request(hc, method, path, body, &resp) != 0)
		return (-1);
	status = (int)resp.status_code;
	htttp_message_free(&resp);
	return (status);
}

static int	say(t_harness *hc, const char *room, const char *text)
{
	char	path[64];
	char	body[512];

	snprintf(path, sizeof(path), "/room/%s", room);
	snprintf(body, sizeof(body), "text %s\n", text);
	return (simple(hc, "CHAT", path, body));
}

static void	expect_narration(t_harness *hc, const char *text)
{
	t_body_chat	chat;

	assert(hc_wait_chat(hc, &chat, HC_TIMEOUT_MS) == 0);
	assert(chat.system);
	assert(chat.sender[0] == '\0');
	assert(strcmp(chat.text, text) == 0);
}

/*
** Swallows the narration a set-up step produced, so the assertion that
** follows is about the message the test is actually for.
*/
static void	drain(t_harness *hc)
{
	t_body_chat	chat;

	while (hc_wait_chat(hc, &chat, 150) == 0)
		;
}
