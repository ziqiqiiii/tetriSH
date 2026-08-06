#include "tetrisu.h"
#include <assert.h>
#include <stdio.h>

static void	test_animated_and_selfref_are_movable(void);
static void	test_ghostty_static_is_movable(void);
static void	test_wezterm_and_iterm_registries_use_cells(void);
static void	test_unmeasured_registry_terminals_are_stationary(void);
static void	test_sixel_and_framebuffer_are_stationary(void);
static void	test_only_no_bitmap_backend_uses_cells(void);
static void	test_forced_modes_override_the_probe(void);
static void	test_renderer_mode_value_parsing(void);

int	main(void)
{
	test_animated_and_selfref_are_movable();
	test_ghostty_static_is_movable();
	test_wezterm_and_iterm_registries_use_cells();
	test_unmeasured_registry_terminals_are_stationary();
	test_sixel_and_framebuffer_are_stationary();
	test_only_no_bitmap_backend_uses_cells();
	test_forced_modes_override_the_probe();
	test_renderer_mode_value_parsing();
	return (0);
}

/**
 * @brief In-place animation backends may move and restack bitmap planes.
 */
static void	test_animated_and_selfref_are_movable(void)
{
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_ANIMATED, "Kitty 0.47.4",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_MOVABLE);
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_SELFREF, "kitty",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_MOVABLE);
	/* Animated backends do not depend on the terminal name. */
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_ANIMATED, NULL,
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_MOVABLE);
	printf("PASS test_animated_and_selfref_are_movable\n");
}

/**
 * @brief Ghostty reports static Kitty yet frees replaced frames, so it moves.
 *
 * Notcurses reserves the animated and self-referential levels for kitty
 * itself, so every other Kitty-graphics terminal arrives here as static.
 */
static void	test_ghostty_static_is_movable(void)
{
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_STATIC, "ghostty 1.2.3",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_MOVABLE);
	/* The name match must not depend on how the terminal cases itself. */
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_STATIC, "Ghostty",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_MOVABLE);
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_STATIC, "GHOSTTY",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_MOVABLE);
	/* Pre-0.20.0 kitty reports static rather than animated. */
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_STATIC, "kitty 0.19.3",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_MOVABLE);
	printf("PASS test_ghostty_static_is_movable\n");
}

/**
 * @brief Image-registry terminals measured to never free fall back to cells.
 */
static void	test_wezterm_and_iterm_registries_use_cells(void)
{
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_STATIC,
			"WezTerm 20240203-110809-5046fc22", TETRISU_RENDERER_AUTO)
		== TETRISU_PIXELS_NONE);
	assert(tetrisu_pixel_policy_for(NCPIXEL_ITERM2, "iTerm2 3.6.11",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_NONE);
	printf("PASS test_wezterm_and_iterm_registries_use_cells\n");
}

/**
 * @brief A registry terminal nobody measured is unproven, not known-broken.
 *
 * Konsole and contour answer the Kitty graphics query and so arrive as static
 * Kitty. Demoting them to cells would throw away working bitmap support over
 * a leak that has only ever been observed in two named terminals.
 */
static void	test_unmeasured_registry_terminals_are_stationary(void)
{
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_STATIC, "Konsole 24.12.0",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_STATIONARY);
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_STATIC, "contour 0.4.3",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_STATIONARY);
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_STATIC, NULL,
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_STATIONARY);
	printf("PASS test_unmeasured_registry_terminals_are_stationary\n");
}

/**
 * @brief Registry-free bitmap backends draw pixels, but only stationary ones.
 */
static void	test_sixel_and_framebuffer_are_stationary(void)
{
	assert(tetrisu_pixel_policy_for(NCPIXEL_SIXEL, "foot",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_STATIONARY);
	assert(tetrisu_pixel_policy_for(NCPIXEL_SIXEL, "XTerm(400)",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_STATIONARY);
	assert(tetrisu_pixel_policy_for(NCPIXEL_SIXEL, NULL,
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_STATIONARY);
	assert(tetrisu_pixel_policy_for(NCPIXEL_LINUXFB, "linux",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_STATIONARY);
	printf("PASS test_sixel_and_framebuffer_are_stationary\n");
}

/**
 * @brief Only a terminal reporting no bitmap support is demoted to cells.
 */
static void	test_only_no_bitmap_backend_uses_cells(void)
{
	assert(tetrisu_pixel_policy_for(NCPIXEL_NONE, "Terminal.app",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_NONE);
	assert(tetrisu_pixel_policy_for(NCPIXEL_NONE, "alacritty",
			TETRISU_RENDERER_AUTO) == TETRISU_PIXELS_NONE);
	printf("PASS test_only_no_bitmap_backend_uses_cells\n");
}

/**
 * @brief Each forced tier wins over whatever the terminal probe reported.
 */
static void	test_forced_modes_override_the_probe(void)
{
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_SELFREF, "kitty",
			TETRISU_RENDERER_CELL) == TETRISU_PIXELS_NONE);
	assert(tetrisu_pixel_policy_for(NCPIXEL_SIXEL, "foot",
			TETRISU_RENDERER_CELL) == TETRISU_PIXELS_NONE);
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_SELFREF, "kitty",
			TETRISU_RENDERER_STATIONARY) == TETRISU_PIXELS_STATIONARY);
	/* pixel is the documented escape hatch for a demoted registry. */
	assert(tetrisu_pixel_policy_for(NCPIXEL_KITTY_STATIC, "WezTerm",
			TETRISU_RENDERER_PIXEL) == TETRISU_PIXELS_MOVABLE);
	/* Sixel cannot move a sprixel at all, so the probe is not the limit
	 * here: forcing pixel is the caller overriding notcurses itself. */
	assert(tetrisu_pixel_policy_for(NCPIXEL_SIXEL, "foot",
			TETRISU_RENDERER_PIXEL) == TETRISU_PIXELS_MOVABLE);
	printf("PASS test_forced_modes_override_the_probe\n");
}

/**
 * @brief Only the documented values force a tier; anything else stays auto.
 */
static void	test_renderer_mode_value_parsing(void)
{
	assert(tetrisu_renderer_mode_from_value(NULL) == TETRISU_RENDERER_AUTO);
	assert(tetrisu_renderer_mode_from_value("") == TETRISU_RENDERER_AUTO);
	assert(tetrisu_renderer_mode_from_value("auto") == TETRISU_RENDERER_AUTO);
	assert(tetrisu_renderer_mode_from_value("CELL") == TETRISU_RENDERER_AUTO);
	assert(tetrisu_renderer_mode_from_value("cell") == TETRISU_RENDERER_CELL);
	assert(tetrisu_renderer_mode_from_value("stationary")
		== TETRISU_RENDERER_STATIONARY);
	assert(tetrisu_renderer_mode_from_value("pixel")
		== TETRISU_RENDERER_PIXEL);
	printf("PASS test_renderer_mode_value_parsing\n");
}
