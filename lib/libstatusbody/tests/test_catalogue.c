// Unit tests for catalogue.c — the LIST /store body: both catalogues, each
// count-prefixed, one `<id> <price> <name>` line per item. The cases that
// matter are the two things this body does that no other one does: names run
// to end of line and may contain spaces, and ids are labels rather than
// positions, so a gap in them must survive the round trip.
#include "statusbody.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>

// Static Functions
static void	set_item(t_body_catalogue_item *item, uint32_t id, uint64_t price,
				const char *name);
static void	make_store(t_body_catalogue *store);

static void	set_item(t_body_catalogue_item *item, uint32_t id, uint64_t price,
		const char *name)
{
	memset(item, 0, sizeof(*item));
	item->id = id;
	item->price = price;
	snprintf(item->name, BODY_ITEM_NAME_MAX, "%s", name);
}

// The shipped roster: four characters at a flat 10, and themes whose ids skip
// the cut John Cena theme at 5.
static void	make_store(t_body_catalogue *store)
{
	memset(store, 0, sizeof(*store));
	store->character_count = 2;
	set_item(&store->characters[0], 1, 10, "Halloween");
	set_item(&store->characters[1], 4, 10, "Wolf-man");
	store->theme_count = 3;
	set_item(&store->themes[0], 1, 0, "Default");
	set_item(&store->themes[1], 3, 5, "Do u wanna build a snowman?");
	set_item(&store->themes[2], 6, 7, "Claude-ing");
}

void	test_catalogue_encode_is_two_counted_sections(void)
{
	t_body_catalogue	store;
	char				out[1024];
	int					n;

	make_store(&store);
	n = body_catalogue_encode(&store, out, sizeof(out));
	assert(n > 0);
	assert(strcmp(out,
			"characters 2\n"
			"1 10 Halloween\n"
			"4 10 Wolf-man\n"
			"themes 3\n"
			"1 0 Default\n"
			"3 5 Do u wanna build a snowman?\n"
			"6 7 Claude-ing\n") == 0);
	assert(n == (int)strlen(out));
	printf("PASS test_catalogue_encode_is_two_counted_sections\n");
}

// decode(encode(x)) == x, spaces and id gaps included.
void	test_catalogue_roundtrip(void)
{
	t_body_catalogue	store;
	t_body_catalogue	back;
	char				out[1024];
	int					n;

	make_store(&store);
	n = body_catalogue_encode(&store, out, sizeof(out));
	assert(n > 0);
	assert(body_catalogue_decode(out, (size_t)n, &back) == 0);
	assert(back.character_count == 2 && back.theme_count == 3);
	assert(back.characters[0].id == 1 && back.characters[0].price == 10);
	assert(strcmp(back.characters[1].name, "Wolf-man") == 0);
	// The name kept its spaces and its question mark.
	assert(strcmp(back.themes[1].name, "Do u wanna build a snowman?") == 0);
	assert(back.themes[1].price == 5);
	// Id 5 is a gap in the catalogue, not a missing array slot: the third
	// theme is still id 6 after the trip.
	assert(back.themes[2].id == 6);
	printf("PASS test_catalogue_roundtrip\n");
}

// A store with nothing in it is an answer, not a failure.
void	test_catalogue_empty_sections(void)
{
	t_body_catalogue	store;
	t_body_catalogue	back;
	char				out[64];
	int					n;

	memset(&store, 0, sizeof(store));
	n = body_catalogue_encode(&store, out, sizeof(out));
	assert(n > 0);
	assert(strcmp(out, "characters 0\nthemes 0\n") == 0);
	assert(body_catalogue_decode(out, (size_t)n, &back) == 0);
	assert(back.character_count == 0 && back.theme_count == 0);
	printf("PASS test_catalogue_empty_sections\n");
}

void	test_catalogue_encode_rejects_bad_input(void)
{
	t_body_catalogue	store;
	char				out[1024];
	char				tiny[8];

	make_store(&store);
	assert(body_catalogue_encode(NULL, out, sizeof(out)) == -1
		&& errno == EINVAL);
	assert(body_catalogue_encode(&store, NULL, sizeof(out)) == -1
		&& errno == EINVAL);
	// A count past the array would read rows that are not there.
	store.character_count = BODY_CATALOGUE_MAX + 1;
	assert(body_catalogue_encode(&store, out, sizeof(out)) == -1
		&& errno == EINVAL);
	make_store(&store);
	// A nameless item would encode a line the decoder cannot read back.
	store.characters[0].name[0] = '\0';
	assert(body_catalogue_encode(&store, out, sizeof(out)) == -1
		&& errno == EINVAL);
	make_store(&store);
	// A newline inside a name would forge an extra row.
	store.themes[0].name[3] = '\n';
	assert(body_catalogue_encode(&store, out, sizeof(out)) == -1
		&& errno == EINVAL);
	make_store(&store);
	assert(body_catalogue_encode(&store, tiny, sizeof(tiny)) == -1
		&& errno == ERANGE);
	printf("PASS test_catalogue_encode_rejects_bad_input\n");
}

void	test_catalogue_decode_rejects_malformed(void)
{
	t_body_catalogue	back;
	const char			*missing_themes = "characters 0\n";
	const char			*short_count = "characters 2\n1 10 Halloween\nthemes 0\n";
	const char			*wrong_key = "chars 0\nthemes 0\n";
	const char			*trailing = "characters 0\nthemes 0\njunk\n";
	const char			*no_name = "characters 1\n1 10\nthemes 0\n";
	const char			*bad_price = "characters 1\n1 -4 Halloween\nthemes 0\n";
	const char			*huge_count = "characters 99\nthemes 0\n";

	assert(body_catalogue_decode(NULL, 4, &back) == -1 && errno == EINVAL);
	assert(body_catalogue_decode("x", 1, NULL) == -1 && errno == EINVAL);
	assert(body_catalogue_decode(missing_themes, strlen(missing_themes),
			&back) == -1 && errno == EBADMSG);
	// The count line promised two rows; only one followed, so the themes
	// header was eaten as an item and the body ran out.
	assert(body_catalogue_decode(short_count, strlen(short_count), &back) == -1
		&& errno == EBADMSG);
	assert(body_catalogue_decode(wrong_key, strlen(wrong_key), &back) == -1
		&& errno == EBADMSG);
	assert(body_catalogue_decode(trailing, strlen(trailing), &back) == -1
		&& errno == EBADMSG);
	assert(body_catalogue_decode(no_name, strlen(no_name), &back) == -1
		&& errno == EBADMSG);
	assert(body_catalogue_decode(bad_price, strlen(bad_price), &back) == -1
		&& errno == EBADMSG);
	// A count past BODY_CATALOGUE_MAX is refused before a row is written.
	assert(body_catalogue_decode(huge_count, strlen(huge_count), &back) == -1
		&& errno == EBADMSG);
	printf("PASS test_catalogue_decode_rejects_malformed\n");
}

// An overlong name must not be written past the field it lands in.
void	test_catalogue_decode_rejects_overlong_name(void)
{
	t_body_catalogue	back;
	char				body[256];
	char				name[BODY_ITEM_NAME_MAX + 8];

	memset(name, 'x', sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';
	snprintf(body, sizeof(body), "characters 1\n1 10 %s\nthemes 0\n", name);
	assert(body_catalogue_decode(body, strlen(body), &back) == -1
		&& errno == EBADMSG);
	printf("PASS test_catalogue_decode_rejects_overlong_name\n");
}

int	main(void)
{
	test_catalogue_encode_is_two_counted_sections();
	test_catalogue_roundtrip();
	test_catalogue_empty_sections();
	test_catalogue_encode_rejects_bad_input();
	test_catalogue_decode_rejects_malformed();
	test_catalogue_decode_rejects_overlong_name();
	return (0);
}
