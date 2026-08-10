/* Room feed line codec tests - player chat and system narration. */
#include "statusbody.h"

#include <assert.h>
#include <stdio.h>

static void	seed_chat(t_body_chat *chat);

void	test_chat_round_trip_preserves_a_player_message(void)
{
	t_body_chat	in;
	t_body_chat	out;
	char		body[1024];
	int			len;

	seed_chat(&in);
	len = body_chat_encode(&in, body, sizeof(body));
	assert(len > 0);
	assert(body_chat_decode(body, (size_t)len, &out) == 0);
	assert(out.seq == 7 && out.at == 1786294796099ull);
	assert(!out.system);
	assert(strcmp(out.sender, "amber") == 0);
	assert(strcmp(out.text, "good luck all") == 0);
	printf("PASS test_chat_round_trip_preserves_a_player_message\n");
}

void	test_chat_system_message_carries_no_sender(void)
{
	t_body_chat	in;
	t_body_chat	out;
	char		body[1024];
	int			len;

	seed_chat(&in);
	in.system = true;
	in.sender[0] = '\0';
	snprintf(in.text, sizeof(in.text), "PLAYER amber joined the room BR-12");
	len = body_chat_encode(&in, body, sizeof(body));
	assert(len > 0);
	assert(strstr(body, "kind system\n") != NULL);
	assert(strstr(body, "sender") == NULL);
	assert(body_chat_decode(body, (size_t)len, &out) == 0);
	assert(out.system && out.sender[0] == '\0');
	assert(strcmp(out.text, "PLAYER amber joined the room BR-12") == 0);
	printf("PASS test_chat_system_message_carries_no_sender\n");
}

void	test_chat_text_keeps_its_spaces_and_fills_the_field(void)
{
	t_body_chat	in;
	t_body_chat	out;
	char		body[1024];
	int			len;
	size_t		index;

	seed_chat(&in);
	index = 0;
	while (index < BODY_CHAT_TEXT_MAX - 1)
	{
		in.text[index] = (char)('a' + index % 26);
		if (index % 8 == 7)
			in.text[index] = ' ';
		index++;
	}
	in.text[index] = '\0';
	len = body_chat_encode(&in, body, sizeof(body));
	assert(len > 0);
	assert(body_chat_decode(body, (size_t)len, &out) == 0);
	assert(strcmp(out.text, in.text) == 0);
	printf("PASS test_chat_text_keeps_its_spaces_and_fills_the_field\n");
}

void	test_chat_encode_rejects_control_characters(void)
{
	t_body_chat	chat;
	char		body[1024];

	seed_chat(&chat);
	snprintf(chat.text, sizeof(chat.text), "line one\nkind system");
	assert(body_chat_encode(&chat, body, sizeof(body)) == -1);
	seed_chat(&chat);
	snprintf(chat.text, sizeof(chat.text), "colour \033[31mme");
	assert(body_chat_encode(&chat, body, sizeof(body)) == -1);
	seed_chat(&chat);
	chat.text[0] = '\0';
	assert(body_chat_encode(&chat, body, sizeof(body)) == -1);
	printf("PASS test_chat_encode_rejects_control_characters\n");
}

void	test_chat_encode_rejects_mismatched_authors(void)
{
	t_body_chat	chat;
	char		body[1024];

	seed_chat(&chat);
	chat.system = true;
	assert(body_chat_encode(&chat, body, sizeof(body)) == -1);
	seed_chat(&chat);
	chat.sender[0] = '\0';
	assert(body_chat_encode(&chat, body, sizeof(body)) == -1);
	seed_chat(&chat);
	snprintf(chat.sender, sizeof(chat.sender), "two words");
	assert(body_chat_encode(&chat, body, sizeof(body)) == -1);
	seed_chat(&chat);
	chat.seq = 0;
	assert(body_chat_encode(&chat, body, sizeof(body)) == -1);
	assert(body_chat_encode(NULL, body, sizeof(body)) == -1);
	printf("PASS test_chat_encode_rejects_mismatched_authors\n");
}

void	test_chat_encode_small_cap_returns_erange(void)
{
	t_body_chat	chat;
	char		body[8];

	seed_chat(&chat);
	errno = 0;
	assert(body_chat_encode(&chat, body, sizeof(body)) == -1);
	assert(errno == ERANGE);
	printf("PASS test_chat_encode_small_cap_returns_erange\n");
}

void	test_chat_decode_rejects_malformed_bodies(void)
{
	static const char	no_sender[] =
		"seq 1\nat 2\nkind player\ntext hello\n";
	static const char	system_with_sender[] =
		"seq 1\nat 2\nkind system\nsender amber\ntext hello\n";
	static const char	trailing[] =
		"seq 1\nat 2\nkind player\nsender amber\ntext hello\nseq 2\n";
	static const char	bad_kind[] =
		"seq 1\nat 2\nkind shout\nsender amber\ntext hello\n";
	static const char	zero_seq[] =
		"seq 0\nat 2\nkind player\nsender amber\ntext hello\n";
	t_body_chat			chat;

	assert(body_chat_decode(no_sender, sizeof(no_sender) - 1, &chat) == -1);
	assert(body_chat_decode(system_with_sender,
			sizeof(system_with_sender) - 1, &chat) == -1);
	assert(body_chat_decode(trailing, sizeof(trailing) - 1, &chat) == -1);
	assert(body_chat_decode(bad_kind, sizeof(bad_kind) - 1, &chat) == -1);
	assert(body_chat_decode(zero_seq, sizeof(zero_seq) - 1, &chat) == -1);
	assert(body_chat_decode(NULL, 0, &chat) == -1);
	printf("PASS test_chat_decode_rejects_malformed_bodies\n");
}

void	test_chat_decode_rejects_oversized_text(void)
{
	char		body[BODY_CHAT_TEXT_MAX + 64];
	t_body_chat	chat;
	size_t		offset;

	offset = (size_t)snprintf(body, sizeof(body),
			"seq 1\nat 2\nkind system\ntext ");
	while (offset < sizeof(body) - 2)
		body[offset++] = 'x';
	body[offset++] = '\n';
	assert(body_chat_decode(body, offset, &chat) == -1);
	printf("PASS test_chat_decode_rejects_oversized_text\n");
}

int	main(void)
{
	test_chat_round_trip_preserves_a_player_message();
	test_chat_system_message_carries_no_sender();
	test_chat_text_keeps_its_spaces_and_fills_the_field();
	test_chat_encode_rejects_control_characters();
	test_chat_encode_rejects_mismatched_authors();
	test_chat_encode_small_cap_returns_erange();
	test_chat_decode_rejects_malformed_bodies();
	test_chat_decode_rejects_oversized_text();
	return (0);
}

static void	seed_chat(t_body_chat *chat)
{
	memset(chat, 0, sizeof(*chat));
	chat->seq = 7;
	chat->at = 1786294796099ull;
	chat->system = false;
	snprintf(chat->sender, sizeof(chat->sender), "amber");
	snprintf(chat->text, sizeof(chat->text), "good luck all");
}
