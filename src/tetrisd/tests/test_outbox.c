/* ************************************************************************** */
/*                                                                            */
/*   test_outbox.c - the per-client send queue                                */
/*                                                                            */
/*   The outbox is the seam that keeps one slow client from hurting anybody   */
/*   else: responses queue in order and overflow closes the connection,       */
/*   while STATE snapshots overwrite a single mailbox so a slow client never  */
/*   holds up the tick that produced them. Chat is the third lane and fails   */
/*   a third way - it drops its oldest and never closes anything. Popping     */
/*   never blocks, so an empty outbox answers exactly the way a closed one    */
/*   does.                                                                    */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisd.h"

#include <assert.h>

// Static Functions
static void	test_responses_come_out_in_order(void);
static void	test_overflow_is_reported_not_grown(void);
static void	test_state_mailbox_keeps_only_the_latest(void);
static void	test_responses_outrank_state(void);
static void	test_close_frees_everything_pending(void);
static void	test_chat_drops_its_oldest_and_never_overflows(void);
static void	test_chat_sits_between_responses_and_state(void);
static void	test_chat_does_not_consume_the_response_queue(void);
static void	test_close_frees_pending_chat(void);

static int	push_text(t_outbox *ob, const char *text, bool as_state);
static int	push_chat_text(t_outbox *ob, const char *text);
static void	expect_text(t_outbox *ob, const char *text);

int	main(void)
{
	test_responses_come_out_in_order();
	test_overflow_is_reported_not_grown();
	test_state_mailbox_keeps_only_the_latest();
	test_responses_outrank_state();
	test_close_frees_everything_pending();
	test_chat_drops_its_oldest_and_never_overflows();
	test_chat_sits_between_responses_and_state();
	test_chat_does_not_consume_the_response_queue();
	test_close_frees_pending_chat();
	return (0);
}

static void	test_responses_come_out_in_order(void)
{
	t_outbox	ob;

	assert(outbox_init(&ob) == 0);
	assert(push_text(&ob, "first", false) == 0);
	assert(push_text(&ob, "second", false) == 0);
	expect_text(&ob, "first");
	expect_text(&ob, "second");
	outbox_destroy(&ob);
	printf("PASS test_responses_come_out_in_order\n");
}

static void	test_overflow_is_reported_not_grown(void)
{
	t_outbox	ob;
	int			i;

	assert(outbox_init(&ob) == 0);
	i = 0;
	while (i < TETRISD_OUTBOX_CAPACITY)
	{
		assert(push_text(&ob, "payload", false) == 0);
		i++;
	}
	assert(push_text(&ob, "one too many", false) == -1);
	assert(ob.overflowed == true);
	expect_text(&ob, "payload");
	assert(push_text(&ob, "room again", false) == 0);
	outbox_destroy(&ob);
	printf("PASS test_overflow_is_reported_not_grown\n");
}

static void	test_state_mailbox_keeps_only_the_latest(void)
{
	t_outbox	ob;

	assert(outbox_init(&ob) == 0);
	assert(push_text(&ob, "snapshot 1", true) == 0);
	assert(push_text(&ob, "snapshot 2", true) == 0);
	assert(push_text(&ob, "snapshot 3", true) == 0);
	expect_text(&ob, "snapshot 3");
	outbox_destroy(&ob);
	printf("PASS test_state_mailbox_keeps_only_the_latest\n");
}

static void	test_responses_outrank_state(void)
{
	t_outbox	ob;

	assert(outbox_init(&ob) == 0);
	assert(push_text(&ob, "snapshot", true) == 0);
	assert(push_text(&ob, "response", false) == 0);
	expect_text(&ob, "response");
	expect_text(&ob, "snapshot");
	outbox_destroy(&ob);
	printf("PASS test_responses_outrank_state\n");
}

static void	test_close_frees_everything_pending(void)
{
	t_outbox	ob;
	t_outbound_message	msg;

	assert(outbox_init(&ob) == 0);
	assert(push_text(&ob, "never sent", false) == 0);
	assert(push_text(&ob, "never sent either", true) == 0);
	outbox_close(&ob);
	assert(outbox_pop(&ob, &msg) == -1);
	assert(push_text(&ob, "after close", false) == -1);
	outbox_destroy(&ob);
	printf("PASS test_close_frees_everything_pending\n");
}

/*
** The property the whole lane exists for: a room that keeps narrating at a
** client which is not reading must never be the reason that client is closed.
** Overflowing the response FIFO sets `overflowed`, and registry_enqueue turns
** that into a shutdown - so the assertion that matters here is that chat
** leaves that flag alone no matter how far past capacity it is pushed.
*/
static void	test_chat_drops_its_oldest_and_never_overflows(void)
{
	t_outbox	ob;
	char		line[32];
	int			i;

	assert(outbox_init(&ob) == 0);
	i = 0;
	while (i < TETRISD_CHAT_CAPACITY + 4)
	{
		snprintf(line, sizeof(line), "line %d", i);
		assert(push_chat_text(&ob, line) == 0);
		i++;
	}
	assert(ob.overflowed == false);
	assert(ob.chat_dropped == 4);
	assert(ob.chat_count == TETRISD_CHAT_CAPACITY);
	i = 4;
	while (i < TETRISD_CHAT_CAPACITY + 4)
	{
		snprintf(line, sizeof(line), "line %d", i);
		expect_text(&ob, line);
		i++;
	}
	assert(outbox_idle(&ob));
	outbox_destroy(&ob);
	printf("PASS test_chat_drops_its_oldest_and_never_overflows\n");
}

static void	test_chat_sits_between_responses_and_state(void)
{
	t_outbox	ob;

	assert(outbox_init(&ob) == 0);
	assert(push_text(&ob, "snapshot", true) == 0);
	assert(push_chat_text(&ob, "amber: hello") == 0);
	assert(push_text(&ob, "response", false) == 0);
	expect_text(&ob, "response");
	expect_text(&ob, "amber: hello");
	expect_text(&ob, "snapshot");
	outbox_destroy(&ob);
	printf("PASS test_chat_sits_between_responses_and_state\n");
}

/*
** Chat and responses are separate rings, so a feed running hot leaves the
** response FIFO exactly as empty as it found it. Were they one queue, this
** many chat lines would have overflowed it and killed the connection.
*/
static void	test_chat_does_not_consume_the_response_queue(void)
{
	t_outbox	ob;
	int			i;

	assert(outbox_init(&ob) == 0);
	i = 0;
	while (i < TETRISD_CHAT_CAPACITY * 2)
	{
		assert(push_chat_text(&ob, "chatter") == 0);
		i++;
	}
	i = 0;
	while (i < TETRISD_OUTBOX_CAPACITY)
	{
		assert(push_text(&ob, "response", false) == 0);
		i++;
	}
	assert(ob.overflowed == false);
	outbox_destroy(&ob);
	printf("PASS test_chat_does_not_consume_the_response_queue\n");
}

static void	test_close_frees_pending_chat(void)
{
	t_outbox	ob;
	t_outbound_message	msg;

	assert(outbox_init(&ob) == 0);
	assert(push_chat_text(&ob, "never delivered") == 0);
	assert(push_chat_text(&ob, "nor this one") == 0);
	outbox_close(&ob);
	assert(outbox_pop(&ob, &msg) == -1);
	assert(push_chat_text(&ob, "after close") == -1);
	outbox_destroy(&ob);
	printf("PASS test_close_frees_pending_chat\n");
}

static int	push_text(t_outbox *ob, const char *text, bool as_state)
{
	unsigned char	*bytes;
	size_t			len;
	int				rc;

	len = strlen(text);
	bytes = malloc(len + 1);
	assert(bytes != NULL);
	memcpy(bytes, text, len + 1);
	if (as_state)
		rc = outbox_push_state(ob, bytes, len);
	else
		rc = outbox_push(ob, bytes, len);
	if (rc != 0)
		free(bytes);
	return (rc);
}

static int	push_chat_text(t_outbox *ob, const char *text)
{
	unsigned char	*bytes;
	size_t			len;
	int				rc;

	len = strlen(text);
	bytes = malloc(len + 1);
	assert(bytes != NULL);
	memcpy(bytes, text, len + 1);
	rc = outbox_push_chat(ob, bytes, len);
	if (rc != 0)
		free(bytes);
	return (rc);
}

static void	expect_text(t_outbox *ob, const char *text)
{
	t_outbound_message	msg;

	assert(outbox_pop(ob, &msg) == 0);
	assert(msg.len == strlen(text));
	assert(memcmp(msg.bytes, text, msg.len) == 0);
	free(msg.bytes);
}
