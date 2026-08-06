#include "tetrisu.h"

// Static Functions
static t_tetrisu_pixel_policy	registry_policy(const char *term);
static bool	backend_uses_image_registry(ncpixelimpl_e backend);
static bool	contains_fold(const char *haystack, const char *needle);

/**
 * @brief Chooses how far this terminal can be trusted with bitmap graphics.
 *
 * Pure policy split out of render_init() so it can be unit tested without a
 * live notcurses context or terminal. Three properties are decided together:
 *
 * - Bitmaps at all. Only NCPIXEL_NONE genuinely cannot draw them; every other
 *   backend gets pixel output rather than the terminal-cell renderer.
 * - Movable planes. Notcurses only implements sprixel movement for the Kitty
 *   protocols: setup_sixel_bitmaps() and setup_fbcon_bitmaps() both leave
 *   ti->pixel_move NULL, and redisplaying a Sixel cannot write transparency
 *   over what is already there. Those two backends therefore draw stationary
 *   bitmaps only, however capable the terminal itself is.
 * - Bounded memory under repeated retransmission. The Kitty and iTerm2
 *   protocols hand the terminal an image registry, and a terminal that never
 *   frees a replaced entry climbs to gigabytes while the board redraws. The
 *   backend enum cannot separate the good from the bad, because notcurses
 *   only reports the animated and self-referential Kitty levels for kitty
 *   itself: every other terminal answering the Kitty graphics query lands on
 *   NCPIXEL_KITTY_STATIC (see the generic bitmap fallback in termdesc.c). So
 *   registry terminals are split by name, and an unrecognised one gets the
 *   stationary tier, which retransmits on board change rather than per frame.
 *   Sixel and the framebuffer keep no registry, so nothing accumulates there.
 *
 * @param backend Pixel backend reported by notcurses_check_pixel_support().
 * @param term Detected terminal name, or NULL when unknown.
 * @param forced Tier requested through TETRISU_RENDERER.
 * @return The capability tier the renderer must stay within.
 */
t_tetrisu_pixel_policy	tetrisu_pixel_policy_for(ncpixelimpl_e backend,
	const char *term, t_tetrisu_renderer_mode forced)
{
	if (forced == TETRISU_RENDERER_CELL)
		return (TETRISU_PIXELS_NONE);
	if (forced == TETRISU_RENDERER_PIXEL)
		return (TETRISU_PIXELS_MOVABLE);
	if (forced == TETRISU_RENDERER_STATIONARY)
		return (TETRISU_PIXELS_STATIONARY);
	if (backend == NCPIXEL_NONE)
		return (TETRISU_PIXELS_NONE);
	if (backend == NCPIXEL_KITTY_ANIMATED || backend == NCPIXEL_KITTY_SELFREF)
		return (TETRISU_PIXELS_MOVABLE);
	if (backend_uses_image_registry(backend))
		return (registry_policy(term));
	if (backend == NCPIXEL_SIXEL || backend == NCPIXEL_LINUXFB)
		return (TETRISU_PIXELS_STATIONARY);
	/* Unknown future backend: cells always render, so degrade rather than
	 * guess at retransmission or z-order behaviour we have not measured. */
	return (TETRISU_PIXELS_NONE);
}

/**
 * @brief Grades a terminal that keeps an image registry by measured behaviour.
 *
 * Only two answers are earned by measurement. Terminals observed to free a
 * replaced image move bitmaps freely; terminals observed to retain every frame
 * are the one case worth giving up bitmaps for entirely. Everything else is
 * merely unmeasured, not known-broken, so it draws stationary bitmaps: those
 * retransmit when the board changes instead of once per frame, which is both
 * the presentation the terminal deserves and a far smaller bet if it turns out
 * to leak. TETRISU_RENDERER pins any of the three explicitly.
 *
 * @param term Detected terminal name, or NULL when unknown.
 * @return The tier this registry terminal has earned.
 */
static t_tetrisu_pixel_policy	registry_policy(const char *term)
{
	if (term == NULL)
		return (TETRISU_PIXELS_STATIONARY);
	if (contains_fold(term, "ghostty") || contains_fold(term, "kitty"))
		return (TETRISU_PIXELS_MOVABLE);
	if (contains_fold(term, "wezterm") || contains_fold(term, "iterm"))
		return (TETRISU_PIXELS_NONE);
	return (TETRISU_PIXELS_STATIONARY);
}

/**
 * @brief Reports whether the backend stores images in a terminal-side registry.
 *
 * NCPIXEL_KITTY_STATIC is not only old kitty: notcurses reserves the animated
 * and self-referential levels for kitty itself, and drops every other terminal
 * answering the Kitty graphics query onto the static level.
 *
 * @param backend Pixel backend reported by notcurses_check_pixel_support().
 * @return true for the Kitty static and iTerm2 protocols.
 */
static bool	backend_uses_image_registry(ncpixelimpl_e backend)
{
	return (backend == NCPIXEL_KITTY_STATIC || backend == NCPIXEL_ITERM2);
}

/**
 * @brief Case-insensitive substring search over ASCII terminal names.
 *
 * @param haystack String searched, never NULL.
 * @param needle Lowercase ASCII pattern, never NULL or empty.
 * @return true when needle occurs in haystack ignoring ASCII case.
 */
static bool	contains_fold(const char *haystack, const char *needle)
{
	size_t	i;
	size_t	j;

	i = 0;
	while (haystack[i] != '\0')
	{
		j = 0;
		while (needle[j] != '\0'
			&& tolower((unsigned char)haystack[i + j])
			== tolower((unsigned char)needle[j]))
			j++;
		if (needle[j] == '\0')
			return (true);
		i++;
	}
	return (false);
}
