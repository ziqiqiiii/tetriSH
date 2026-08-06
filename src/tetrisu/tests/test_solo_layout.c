#include "tetrisu.h"

static void	test_content_and_controls_share_the_height(void);
static void	test_kitty_210_by_71_accepts_the_larger_layout(void);
static void	test_layout_boundaries(void);

int	main(void)
{
	test_content_and_controls_share_the_height();
	test_kitty_210_by_71_accepts_the_larger_layout();
	test_layout_boundaries();
	return (0);
}

/**
 * @brief The 23-row artwork plus one text row preserves the 24-row minimum.
 */
static void	test_content_and_controls_share_the_height(void)
{
	assert(solo_layout_content_rows(1) == 23);
	assert(solo_layout_canvas_rows(1) == 24);
	assert(solo_layout_canvas_rows(2) == 47);
	assert(solo_layout_canvas_rows(3) == 70);
	printf("PASS test_content_and_controls_share_the_height\n");
}

/**
 * @brief Reproduces the Kitty geometry which previously fell back to 128x48.
 */
static void	test_kitty_210_by_71_accepts_the_larger_layout(void)
{
	assert(solo_layout_terminal_fits(71, 210, 3, 5));
	assert(5 * 32 == 160);
	assert(solo_layout_canvas_rows(3) == 70);
	printf("PASS test_kitty_210_by_71_accepts_the_larger_layout\n");
}

/**
 * @brief One missing row or column rejects the same integer-grid candidate.
 */
static void	test_layout_boundaries(void)
{
	assert(!solo_layout_terminal_fits(69, 210, 3, 5));
	assert(!solo_layout_terminal_fits(71, 159, 3, 5));
	assert(!solo_layout_terminal_fits(71, 210, 0, 5));
	printf("PASS test_layout_boundaries\n");
}
