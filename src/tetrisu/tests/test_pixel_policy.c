#include "tetrisu.h"
#include <assert.h>
#include <stdio.h>

static void	test_animated_and_selfref_are_safe(void);
static void	test_ghostty_static_is_safe(void);
static void	test_wezterm_and_iterm_static_leak(void);
static void	test_non_kitty_backends_use_cells(void);

int	main(void)
{
	test_animated_and_selfref_are_safe();
	test_ghostty_static_is_safe();
	test_wezterm_and_iterm_static_leak();
	test_non_kitty_backends_use_cells();
	return (0);
}

/**
 * @brief In-place animation backends are always leak-safe for the pixel board.
 */
static void	test_animated_and_selfref_are_safe(void)
{
	assert(tetrisu_pixel_backend_leak_safe(NCPIXEL_KITTY_ANIMATED,
			"Kitty 0.47.4"));
	assert(tetrisu_pixel_backend_leak_safe(NCPIXEL_KITTY_SELFREF, "kitty"));
	/* Animated backends do not depend on the terminal name. */
	assert(tetrisu_pixel_backend_leak_safe(NCPIXEL_KITTY_ANIMATED, NULL));
	printf("PASS test_animated_and_selfref_are_safe\n");
}

/**
 * @brief Ghostty reports static Kitty yet frees replaced frames, so it is safe.
 */
static void	test_ghostty_static_is_safe(void)
{
	assert(tetrisu_pixel_backend_leak_safe(NCPIXEL_KITTY_STATIC,
			"ghostty 1.2.3"));
	assert(tetrisu_pixel_backend_leak_safe(NCPIXEL_KITTY_STATIC, "Ghostty"));
	printf("PASS test_ghostty_static_is_safe\n");
}

/**
 * @brief WezTerm and iTerm2 report static Kitty but never free, so use cells.
 */
static void	test_wezterm_and_iterm_static_leak(void)
{
	assert(!tetrisu_pixel_backend_leak_safe(NCPIXEL_KITTY_STATIC,
			"WezTerm 20240203-110809-5046fc22"));
	assert(!tetrisu_pixel_backend_leak_safe(NCPIXEL_KITTY_STATIC,
			"iTerm2 3.6.11"));
	/* Unknown static terminals are treated as unsafe until measured. */
	assert(!tetrisu_pixel_backend_leak_safe(NCPIXEL_KITTY_STATIC, NULL));
	assert(!tetrisu_pixel_backend_leak_safe(NCPIXEL_KITTY_STATIC, "xterm"));
	printf("PASS test_wezterm_and_iterm_static_leak\n");
}

/**
 * @brief Non-Kitty pixel backends fall back to cells for the moving board.
 */
static void	test_non_kitty_backends_use_cells(void)
{
	assert(!tetrisu_pixel_backend_leak_safe(NCPIXEL_NONE, "Terminal.app"));
	assert(!tetrisu_pixel_backend_leak_safe(NCPIXEL_SIXEL, "xterm"));
	assert(!tetrisu_pixel_backend_leak_safe(NCPIXEL_LINUXFB, "linux"));
	assert(!tetrisu_pixel_backend_leak_safe(NCPIXEL_ITERM2, "iTerm2"));
	printf("PASS test_non_kitty_backends_use_cells\n");
}
