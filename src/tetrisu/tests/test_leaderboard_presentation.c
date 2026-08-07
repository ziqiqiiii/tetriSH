#include "tetrisu.h"

static void	test_background_lifecycle_policy(void);
static void	test_layout_rejects_unsupported_geometry(void);
static void	test_layout_preserves_complete_compact_surface(void);
static void	test_layout_centers_full_presentation(void);
static void	test_rank_lookup_uses_explicit_position(void);
static void	test_pixel_layout_and_hitboxes(void);

int	main(void)
{
	test_background_lifecycle_policy();
	test_layout_rejects_unsupported_geometry();
	test_layout_preserves_complete_compact_surface();
	test_layout_centers_full_presentation();
	test_rank_lookup_uses_explicit_position();
	test_pixel_layout_and_hitboxes();
	return (0);
}

static void	test_background_lifecycle_policy(void)
{
	assert(leaderboard_background_action_for(TETRISU_PIXELS_NONE, false)
		== LEADERBOARD_BACKGROUND_CELL);
	assert(leaderboard_background_action_for(TETRISU_PIXELS_NONE, true)
		== LEADERBOARD_BACKGROUND_CELL);
	assert(leaderboard_background_action_for(TETRISU_PIXELS_STATIONARY, true)
		== LEADERBOARD_BACKGROUND_REPLACE_EXACT);
	assert(leaderboard_background_action_for(TETRISU_PIXELS_MOVABLE, true)
		== LEADERBOARD_BACKGROUND_REPLACE_EXACT);
	assert(leaderboard_background_action_for(TETRISU_PIXELS_STATIONARY, false)
		== LEADERBOARD_BACKGROUND_REUSE);
	printf("PASS test_background_lifecycle_policy\n");
}

static void	test_layout_rejects_unsupported_geometry(void)
{
	leaderboard_layout_t	layout;

	assert(!leaderboard_layout_resolve(19, 44, false, &layout));
	assert(!leaderboard_layout_resolve(20, 43, true, &layout));
	assert(!leaderboard_layout_resolve(20, 44, false, NULL));
	printf("PASS test_layout_rejects_unsupported_geometry\n");
}

static void	test_layout_preserves_complete_compact_surface(void)
{
	leaderboard_layout_t	layout;

	assert(leaderboard_layout_resolve(20, 44, false, &layout));
	assert(layout.y == 0 && layout.x == 0);
	assert(layout.rows == 20 && layout.cols == 44 && layout.compact);
	assert(leaderboard_layout_resolve(24, 80, true, &layout));
	assert(layout.y == 1 && layout.x == 0);
	assert(layout.rows == 23 && layout.cols == 80 && layout.compact);
	assert(leaderboard_layout_resolve(24, 80, false, &layout));
	assert(layout.y == 2 && layout.x == 8);
	assert(layout.rows == 20 && layout.cols == 64 && layout.compact);
	printf("PASS test_layout_preserves_complete_compact_surface\n");
}

static void	test_layout_centers_full_presentation(void)
{
	leaderboard_layout_t	layout;

	assert(leaderboard_layout_resolve(40, 120, false, &layout));
	assert(layout.y == 3 && layout.x == 10);
	assert(layout.rows == 34 && layout.cols == 100 && !layout.compact);
	printf("PASS test_layout_centers_full_presentation\n");
}

static void	test_rank_lookup_uses_explicit_position(void)
{
	app_leaderboard_view_model_t	leaderboard;

	memset(&leaderboard, 0, sizeof(leaderboard));
	leaderboard.count = 3;
	leaderboard.entries[0].position = 3;
	leaderboard.entries[1].position = 1;
	leaderboard.entries[2].position = 2;
	assert(leaderboard_entry_for_position(&leaderboard, 1)
		== &leaderboard.entries[1]);
	assert(leaderboard_entry_for_position(&leaderboard, 2)
		== &leaderboard.entries[2]);
	assert(leaderboard_entry_for_position(&leaderboard, 0) == NULL);
	assert(leaderboard_entry_for_position(&leaderboard, 4) == NULL);
	assert(leaderboard_entry_for_position(NULL, 1) == NULL);
	printf("PASS test_rank_lookup_uses_explicit_position\n");
}

static void	test_pixel_layout_and_hitboxes(void)
{
	leaderboard_pixel_layout_t	layout;
	leaderboard_focus_t			focus;

	leaderboard_pixel_layout_build(2, 4, 54, 181, 20, 8, &layout);
	assert(layout.pixel_width == 1448 && layout.pixel_height == 1080);
	assert(layout.buttons[LEADERBOARD_FOCUS_BACK].x == 494);
	assert(layout.buttons[LEADERBOARD_FOCUS_REFRESH].x == 744);
	/*
	 * The controls region is the only part a focus move repaints, so it has to
	 * enclose both buttons and the hint line without covering the rank rows.
	 */
	assert(layout.controls.x <= layout.buttons[LEADERBOARD_FOCUS_BACK].x);
	assert(layout.controls.x + layout.controls.width
		>= layout.buttons[LEADERBOARD_FOCUS_REFRESH].x
		+ layout.buttons[LEADERBOARD_FOCUS_REFRESH].width);
	assert(layout.controls.y <= layout.buttons[LEADERBOARD_FOCUS_BACK].y);
	assert(layout.controls.y + layout.controls.height
		>= layout.buttons[LEADERBOARD_FOCUS_BACK].y
		+ layout.buttons[LEADERBOARD_FOCUS_BACK].height);
	assert(layout.controls.x + layout.controls.width <= layout.pixel_width);
	assert(layout.controls.y + layout.controls.height <= layout.pixel_height);
	assert(leaderboard_pixel_hit_test(&layout, 47, 70, &focus));
	assert(focus == LEADERBOARD_FOCUS_BACK);
	assert(leaderboard_pixel_hit_test(&layout, 47, 101, &focus));
	assert(focus == LEADERBOARD_FOCUS_REFRESH);
	assert(!leaderboard_pixel_hit_test(&layout, 10, 10, &focus));
	assert(!leaderboard_pixel_hit_test(&layout, 56, 70, &focus));
	assert(!leaderboard_pixel_hit_test(&layout, 47, 185, &focus));
	assert(!leaderboard_pixel_hit_test(NULL, 47, 70, &focus));
	assert(!leaderboard_pixel_hit_test(&layout, 47, 70, NULL));
	leaderboard_pixel_layout_build(0, 0, INT_MAX, INT_MAX, 2, 2,
		&layout);
	assert(layout.pixel_width == 0 && layout.pixel_height == 0);
	assert(!leaderboard_pixel_hit_test(&layout, 0, 0, &focus));
	printf("PASS test_pixel_layout_and_hitboxes\n");
}
