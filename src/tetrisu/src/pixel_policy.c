#include "tetrisu.h"

/**
 * @brief Decides whether repeatedly (re)transmitted bitmaps stay bounded.
 *
 * Pure policy split out of render_pixels_leak_safe() so it can be unit tested
 * without a live notcurses context or terminal. A moving or frequently redrawn
 * pixel plane is memory-safe where the terminal animates the bitmap in place
 * (Kitty's animated / self-referential protocol) or frees each replaced static
 * image. The pixel backend enum alone cannot separate terminals that free from
 * those that leak (Ghostty and WezTerm both report static Kitty), so static
 * backends are gated by a name allowlist of terminals measured to free.
 *
 * @param backend Pixel backend reported by notcurses_check_pixel_support().
 * @param term Detected terminal name, or NULL when unknown.
 * @return true when pixel graphics may move or redraw here without leaking.
 */
bool	tetrisu_pixel_backend_leak_safe(ncpixelimpl_e backend, const char *term)
{
	if (backend == NCPIXEL_KITTY_ANIMATED || backend == NCPIXEL_KITTY_SELFREF)
		return (true);
	if (backend != NCPIXEL_KITTY_STATIC)
		return (false);
	return (term != NULL && (strstr(term, "ghostty") != NULL
				|| strstr(term, "Ghostty") != NULL));
}
