#include "../tetrisu.h"

static void	test_show_clamps_and_replaces(void);
static void	test_stack_drops_oldest(void);
static void	test_hold_fade_and_expiry(void);
static void	test_next_wake_deadlines(void);
static void	test_volume_percent(void);
static void	test_ownership_message(void);

int	main(void)
{
	test_show_clamps_and_replaces();
	test_stack_drops_oldest();
	test_hold_fade_and_expiry();
	test_next_wake_deadlines();
	test_volume_percent();
	test_ownership_message();
	return (0);
}

static void	test_show_clamps_and_replaces(void)
{
	ui_notification_stack_t	stack;

	ui_notification_stack_init(&stack);
	ui_notification_show(&stack, "MUSIC", 140, 100);
	assert(stack.count == 1);
	assert(stack.items[0].kind == UI_NOTIFICATION_VOLUME);
	assert(strcmp(stack.items[0].title, "MUSIC") == 0);
	assert(stack.items[0].percent == 100);
	ui_notification_show(&stack, "MUSIC", -5, 250);
	assert(stack.count == 1);
	assert(stack.items[0].percent == 0);
	assert(stack.items[0].shown_at_ms == 250);
	puts("PASS test_show_clamps_and_replaces");
}

static void	test_stack_drops_oldest(void)
{
	ui_notification_stack_t	stack;

	ui_notification_stack_init(&stack);
	ui_notification_show(&stack, "ONE", 10, 1);
	ui_notification_show(&stack, "TWO", 20, 2);
	ui_notification_show(&stack, "THREE", 30, 3);
	ui_notification_show(&stack, "FOUR", 40, 4);
	assert(stack.count == UI_NOTIFICATION_STACK_MAX);
	assert(strcmp(stack.items[0].title, "FOUR") == 0);
	assert(strcmp(stack.items[1].title, "THREE") == 0);
	assert(strcmp(stack.items[2].title, "TWO") == 0);
	puts("PASS test_stack_drops_oldest");
}

static void	test_hold_fade_and_expiry(void)
{
	ui_notification_stack_t	stack;
	uint64_t				start;

	start = 5000;
	ui_notification_stack_init(&stack);
	ui_notification_show(&stack, "MUSIC", 50, start);
	assert(ui_notification_opacity(&stack.items[0], start) == 255);
	assert(ui_notification_opacity(&stack.items[0],
			start + UI_NOTIFICATION_HOLD_MS) == 255);
	assert(ui_notification_opacity(&stack.items[0],
			start + UI_NOTIFICATION_HOLD_MS
			+ UI_NOTIFICATION_FADE_MS / 2) >= 127);
	assert(ui_notification_opacity(&stack.items[0],
			start + UI_NOTIFICATION_HOLD_MS
			+ UI_NOTIFICATION_FADE_MS) == 0);
	assert(ui_notification_update(&stack, start
			+ UI_NOTIFICATION_HOLD_MS + UI_NOTIFICATION_FADE_MS));
	assert(stack.count == 0);
	puts("PASS test_hold_fade_and_expiry");
}

static void	test_next_wake_deadlines(void)
{
	ui_notification_stack_t	stack;
	uint64_t				start;

	start = 1000;
	ui_notification_stack_init(&stack);
	assert(ui_notification_next_wake_ms(&stack, start) == -1);
	ui_notification_show(&stack, "MUSIC", 75, start);
	assert(ui_notification_next_wake_ms(&stack, start)
		== UI_NOTIFICATION_HOLD_MS);
	assert(ui_notification_next_wake_ms(&stack,
			start + UI_NOTIFICATION_HOLD_MS) == UI_NOTIFICATION_FRAME_MS);
	assert(ui_notification_next_wake_ms(&stack,
			start + UI_NOTIFICATION_HOLD_MS
			+ UI_NOTIFICATION_FADE_MS - 10) == 10);
	puts("PASS test_next_wake_deadlines");
}

static void	test_volume_percent(void)
{
	assert(ui_notification_volume_percent(-10) == 0);
	assert(ui_notification_volume_percent(0) == 0);
	assert(ui_notification_volume_percent(64) == 50);
	assert(ui_notification_volume_percent(128) == 100);
	assert(ui_notification_volume_percent(500) == 100);
	puts("PASS test_volume_percent");
}

static void	test_ownership_message(void)
{
	ui_notification_stack_t	stack;

	ui_notification_stack_init(&stack);
	ui_notification_show_ownership(&stack, 700);
	assert(stack.count == 1);
	assert(stack.items[0].kind == UI_NOTIFICATION_OWNERSHIP);
	assert(strcmp(stack.items[0].title,
			UI_NOTIFICATION_OWNERSHIP_TITLE) == 0);
	assert(strcmp(stack.items[0].message,
			UI_NOTIFICATION_OWNERSHIP_MESSAGE) == 0);
	assert(stack.items[0].shown_at_ms == 700);
	ui_notification_show_ownership(&stack, 900);
	assert(stack.count == 1);
	assert(stack.items[0].shown_at_ms == 900);
	puts("PASS test_ownership_message");
}
