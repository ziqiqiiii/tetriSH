#include "tetrisu.h"

// Static Functions
static t_tetrisu_pixel_policy	detect_pixel_policy(const t_render_ctx *ctx);
static void	refresh_cell_geometry(t_render_ctx *ctx);
static void	fit_background_to_terminal(t_render_ctx *ctx,
	int std_rows, int std_cols);
static int	max_int(int a, int b);
static ncblitter_e	preferred_blitter(const t_render_ctx *ctx,
	int rows, int cols);
static void	set_opaque_backdrop(struct ncplane *plane);
static int	replace_visual_scaled(t_render_ctx *ctx, struct ncvisual *ncv,
				bool stretch, ncscale_e scaling, ncblitter_e blitter,
				uint64_t flags);
static void	capture_backdrop(t_render_ctx *ctx, struct ncvisual *ncv);
static void	backdrop_fit(t_render_ctx *ctx, bool stretch);
static t_backdrop_cache	*backdrop_find(t_render_ctx *ctx, const char *path,
				bool exact, bool stretch);
static bool	backdrop_is_cached(const t_render_ctx *ctx,
				const struct ncplane *plane);
static bool	backdrop_restack(t_render_ctx *ctx, const char *path,
				bool exact, bool stretch);
static void	backdrop_remember(t_render_ctx *ctx, const char *path,
				bool exact, bool stretch);
static t_backdrop_cache	*backdrop_evict(t_render_ctx *ctx);
static void	backdrop_keep_snapshot(t_render_ctx *ctx,
				t_backdrop_cache *entry);
static void	backdrop_restore_snapshot(t_render_ctx *ctx,
				const t_backdrop_cache *entry);
static void	backdrop_cache_clear(t_render_ctx *ctx);
static void	backdrop_park(t_render_ctx *ctx, struct ncplane *plane);
static void	read_backdrop_pixels(t_render_ctx *ctx, struct ncvisual *ncv,
				int width, int height);
static bool	take_parsed_event(t_render_ctx *ctx, ncinput *event,
				uint32_t *key);
static int	next_input_wait_ms(const t_render_ctx *ctx);
static int	min_wait_ms(const t_render_ctx *ctx, int deadline_ms);

/**
 * @brief Starts notcurses and renders the initial background image.
 *
 * The background owns a child plane below menu/game overlays so later modes
 * can replace it without restarting the terminal session.
 *
 * @param image_path Image rendered behind the home screen.
 * @return Fully initialised render context; exits when setup cannot recover.
 */
t_render_ctx	render_init(const char *image_path)
{
	t_render_ctx		ctx;
	notcurses_options	opts;
	const char			*term;

	memset(&ctx, 0, sizeof(ctx));
	memset(&opts, 0, sizeof(opts));
	tetrisu_theme_apply(&ctx, "Classic");
	tetrisu_character_apply(&ctx, "Mirurun");
	ctx.visual_selection_initialized = false;
	ctx.nc = notcurses_init(&opts, NULL);
	if (ctx.nc == NULL)
	{
		term = getenv("TERM");
		fprintf(stderr, "tetrisu: notcurses_core_init failed (TERM=%s) — "
			"check terminfo for this terminal type\n",
			term != NULL ? term : "unset");
		exit(1);
	}
	ctx.std = notcurses_stdplane(ctx.nc);
	ctx.pixels = detect_pixel_policy(&ctx);
	if (render_geometry_refresh(&ctx, false) < 0)
	{
		notcurses_stop(ctx.nc);
		fprintf(stderr, "tetrisu: could not read terminal geometry\n");
		exit(1);
	}
	if (render_background_replace(&ctx, image_path, false) < 0)
	{
		notcurses_stop(ctx.nc);
		fprintf(stderr, "tetrisu: failed to load image %s\n", image_path);
		exit(1);
	}
	return (ctx);
}

/**
 * @brief Reports whether bitmap planes may be moved and overlapped freely.
 *
 * Only the Kitty-protocol tier can restack a sprixel or slide it a cell without
 * tearing, so per-piece board planes and the animated selector are limited to
 * it. Sixel and the Linux framebuffer still draw bitmaps, but stationary ones.
 *
 * @param ctx Active render context.
 * @return true when bitmap planes may move, overlap, and restack.
 */
bool	render_pixel_planes_reliable(const t_render_ctx *ctx)
{
	return (ctx != NULL && ctx->pixels == TETRISU_PIXELS_MOVABLE);
}

/**
 * @brief Reports whether this session may draw bitmaps at all.
 *
 * @param ctx Active render context.
 * @return true for every tier above the terminal-cell renderer.
 */
bool	render_pixels_available(const t_render_ctx *ctx)
{
	return (ctx != NULL && ctx->pixels != TETRISU_PIXELS_NONE);
}

/**
 * @brief Reports whether a live plane already occupies the wanted geometry.
 *
 * Reusing a plane instead of replacing it is what keeps the stationary tier
 * cheap: destroying a sprixel plane damages every cell it covered, so the
 * bitmap underneath has to be retransmitted. Writing into a plane that is
 * already the right size and place touches only that plane.
 *
 * @param plane Candidate plane, which may be NULL.
 * @param y Wanted top row, in terminal cells.
 * @param x Wanted left column, in terminal cells.
 * @param rows Wanted height in cells.
 * @param cols Wanted width in cells.
 * @return true when @p plane can be written in place.
 */
bool	render_plane_geometry_matches(struct ncplane *plane, int y, int x,
	unsigned rows, unsigned cols)
{
	int			plane_y;
	int			plane_x;
	unsigned	plane_rows;
	unsigned	plane_cols;

	if (plane == NULL)
		return (false);
	ncplane_yx(plane, &plane_y, &plane_x);
	ncplane_dim_yx(plane, &plane_rows, &plane_cols);
	return (plane_y == y && plane_x == x && plane_rows == rows
		&& plane_cols == cols);
}

/**
 * @brief Blits an RGBA surface into an existing plane at zero offset.
 *
 * @param ctx Active render context.
 * @param plane Destination plane.
 * @param pixels First pixel of the surface.
 * @param width Surface width in pixels.
 * @param height Surface height in pixels.
 * @param row_stride Source row width in pixels, which differs from @p width
 * whenever the surface is a window cut from a larger canvas.
 * @return true when the surface reached the plane.
 */
bool	render_plane_blit_rgba(t_render_ctx *ctx, struct ncplane *plane,
	const uint32_t *pixels, int width, int height, int row_stride)
{
	struct ncvisual			*ncv;
	struct ncvisual_options	vopts;

	ncv = ncvisual_from_rgba(pixels, height,
			row_stride * (int)sizeof(*pixels), width);
	if (ncv == NULL)
		return (false);
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = plane;
	vopts.scaling = NCSCALE_NONE;
	vopts.blitter = NCBLIT_PIXEL;
	vopts.flags = NCVISUAL_OPTION_NOINTERPOLATE | NCVISUAL_OPTION_NODEGRADE;
	if (ncvisual_blit(ctx->nc, ncv, &vopts) == NULL)
	{
		ncvisual_destroy(ncv);
		return (false);
	}
	ncvisual_destroy(ncv);
	return (true);
}

/**
 * @brief Reports whether this session uses the terminal-native renderer.
 *
 * Automatic mode only falls back to cells when the terminal reports no bitmap
 * support. TETRISU_RENDERER=cell makes the same polished path deterministic.
 *
 * @param ctx Active render context.
 * @return true when all changing UI surfaces must remain terminal cells.
 */
bool	render_compatibility_mode(const t_render_ctx *ctx)
{
	return (ctx != NULL && ctx->pixels == TETRISU_PIXELS_NONE);
}

/**
 * @brief Shows the compact terminal-native mode badge at the top of the grid.
 *
 * The badge is intentionally opaque and high-contrast so users understand why
 * the presentation differs from bitmap-capable screenshots.
 *
 * @param ctx Active render context.
 */
void	render_compatibility_badge_refresh(t_render_ctx *ctx)
{
	ncplane_options	opts;
	const char		*text;
	uint64_t		channels;
	unsigned		rows;
	unsigned		cols;
	int				width;

	render_compatibility_badge_hide(ctx);
	if (!render_compatibility_mode(ctx) || ctx->std == NULL)
		return ;
	ncplane_dim_yx(ctx->std, &rows, &cols);
	if (rows == 0 || cols < 12)
		return ;
	text = COMPATIBILITY_BADGE_TEXT;
	if (cols < strlen(text) + 4)
		text = COMPATIBILITY_BADGE_SHORT;
	width = (int)strlen(text) + 4;
	if (width > (int)cols)
		width = (int)cols;
	memset(&opts, 0, sizeof(opts));
	opts.y = 0;
	opts.x = ((int)cols - width) / 2;
	opts.rows = 1;
	opts.cols = width;
	ctx->compatibility_plane = ncplane_create(ctx->std, &opts);
	if (ctx->compatibility_plane == NULL)
		return ;
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, 255, 203, 102);
	(void)ncchannels_set_bg_rgb8(&channels, 28, 13, 39);
	(void)ncplane_set_base(ctx->compatibility_plane, " ", 0, channels);
	ncplane_erase(ctx->compatibility_plane);
	(void)ncplane_set_fg_rgb8(ctx->compatibility_plane, 255, 203, 102);
	(void)ncplane_set_bg_rgb8(ctx->compatibility_plane, 28, 13, 39);
	(void)ncplane_on_styles(ctx->compatibility_plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_aligned(ctx->compatibility_plane, 0,
		NCALIGN_CENTER, text);
	ncplane_move_top(ctx->compatibility_plane);
}

/**
 * @brief Removes the compatibility badge when a screen has no spare top row.
 *
 * @param ctx Active render context.
 */
void	render_compatibility_badge_hide(t_render_ctx *ctx)
{
	if (ctx != NULL && ctx->compatibility_plane != NULL)
	{
		ncplane_destroy(ctx->compatibility_plane);
		ctx->compatibility_plane = NULL;
	}
}

/**
 * @brief Refreshes terminal and background geometry.
 *
 * @param ctx Context whose dimensions are updated.
 * @param repaint Whether notcurses must query and repaint the terminal first.
 * @return 0 on success, -1 when notcurses refresh fails.
 */
int	render_geometry_refresh(t_render_ctx *ctx, bool repaint)
{
	unsigned	rows;
	unsigned	cols;

	if (repaint)
	{
		if (notcurses_refresh(ctx->nc, &rows, &cols) != 0)
			return (-1);
	}
	else
		ncplane_dim_yx(ctx->std, &rows, &cols);
	refresh_cell_geometry(ctx);
	fit_background_to_terminal(ctx, (int)rows, (int)cols);
	return (0);
}

/**
 * @brief Detects terminal geometry changes missing from the input stream.
 *
 * Ghostty can resize its drawable grid without emitting NCKEY_RESIZE. Comparing
 * the tty geometry with Notcurses' current standard plane lets every screen
 * recover without requiring a key press.
 *
 * @param ctx Pointer to the render context.
 * @return true when the tty and standard-plane dimensions differ.
 */
bool	render_terminal_geometry_changed(const t_render_ctx *ctx)
{
	struct winsize	terminal;
	unsigned		plane_rows;
	unsigned		plane_cols;

	if (ctx == NULL || ctx->std == NULL)
		return (false);
	memset(&terminal, 0, sizeof(terminal));
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminal) != 0
		|| terminal.ws_row == 0 || terminal.ws_col == 0)
		return (false);
	ncplane_dim_yx(ctx->std, &plane_rows, &plane_cols);
	return (terminal.ws_row != plane_rows || terminal.ws_col != plane_cols);
}

/**
 * @brief Destroys the current backdrop and every retained one.
 *
 * Callers reach for this when the backdrop must genuinely stop existing rather
 * than be covered: the intro cannot cover a sprixel with cell-blitted video,
 * and Solo owns the screen outright. Retained planes cannot survive that, so
 * the cache is emptied with them and the next visit rebuilds honestly.
 *
 * @param ctx Context whose background pointer is cleared.
 */
void	render_background_destroy(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	backdrop_cache_clear(ctx);
	if (ctx->bg_plane != NULL)
	{
		ncplane_destroy(ctx->bg_plane);
		ctx->bg_plane = NULL;
	}
}

/**
 * @brief Drops retained screen variants while preserving the live backdrop.
 *
 * A theme change gives every screen a new asset path. Keeping planes from the
 * previous theme wastes the bounded cache and eventually forces unsafe
 * eviction churn in bitmap terminals, so theme equip calls reset it first.
 *
 * @param ctx Context whose inactive backdrop planes are released.
 */
void	render_background_cache_reset(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	backdrop_cache_clear(ctx);
	ctx->backdrop_tick = 0;
}

/**
 * @brief Sets bg_row/col/rows/cols to the geometry a backdrop would occupy.
 *
 * The cache is consulted before any plane is built, so the wanted geometry has
 * to be known first. replace_visual_scaled() derives the same values the same
 * way; computing them twice is a few divisions against a transfer measured in
 * seconds.
 */
static void	backdrop_fit(t_render_ctx *ctx, bool stretch)
{
	unsigned	std_rows;
	unsigned	std_cols;

	ncplane_dim_yx(ctx->std, &std_rows, &std_cols);
	refresh_cell_geometry(ctx);
	if (stretch)
	{
		ctx->bg_row = 0;
		ctx->bg_col = 0;
		ctx->bg_rows = (int)std_rows;
		ctx->bg_cols = (int)std_cols;
	}
	else
		fit_background_to_terminal(ctx, (int)std_rows, (int)std_cols);
}

/**
 * @brief Finds the entry holding one artwork built the same way.
 */
static t_backdrop_cache	*backdrop_find(t_render_ctx *ctx, const char *path,
	bool exact, bool stretch)
{
	int	index;

	index = 0;
	while (index < BACKDROP_CACHE_MAX)
	{
		if (ctx->backdrops[index].plane != NULL
			&& ctx->backdrops[index].exact == exact
			&& ctx->backdrops[index].stretch == stretch
			&& strcmp(ctx->backdrops[index].path, path) == 0)
			return (&ctx->backdrops[index]);
		index++;
	}
	return (NULL);
}

/**
 * @brief Reports whether a plane is retained, and so must not be destroyed.
 */
static bool	backdrop_is_cached(const t_render_ctx *ctx,
	const struct ncplane *plane)
{
	int	index;

	index = 0;
	while (index < BACKDROP_CACHE_MAX)
	{
		if (plane != NULL && ctx->backdrops[index].plane == plane)
			return (true);
		index++;
	}
	return (false);
}

/**
 * @brief Brings a retained backdrop back to the front without retransferring.
 *
 * @return true when the screen now shows the wanted artwork.
 */
static bool	backdrop_restack(t_render_ctx *ctx, const char *path,
	bool exact, bool stretch)
{
	t_backdrop_cache	*entry;
	struct ncplane		*stale;

	entry = backdrop_find(ctx, path, exact, stretch);
	if (entry == NULL || entry->rows != ctx->bg_rows
		|| entry->cols != ctx->bg_cols)
		return (false);
	entry->used = ++ctx->backdrop_tick;
	if (entry->plane == ctx->bg_plane)
		return (true);
	stale = ctx->bg_plane;
	backdrop_park(ctx, stale);
	(void)ncplane_move_yx(entry->plane, ctx->bg_row, ctx->bg_col);
	(void)ncplane_move_above(entry->plane, ctx->std);
	ctx->bg_plane = entry->plane;
	set_opaque_backdrop(ctx->std);
	backdrop_restore_snapshot(ctx, entry);
	if (notcurses_render(ctx->nc) != 0)
		return (false);
	if (stale != NULL && !backdrop_is_cached(ctx, stale))
		ncplane_destroy(stale);
	return (true);
}

/**
 * @brief Retains the freshly built backdrop under its artwork and construction.
 */
static void	backdrop_remember(t_render_ctx *ctx, const char *path,
	bool exact, bool stretch)
{
	t_backdrop_cache	*entry;

	/*
	 * Retaining a backdrop is only ever worth it because the plane can later be
	 * parked and restacked, and that is precisely what the stationary tier
	 * forbids: sixel and the Linux framebuffer may write a sprixel in place but
	 * not move one. Refusing to remember anything there leaves every other part
	 * of the cache inert - restack finds nothing, park skips what it was not
	 * given - so those tiers keep the plain destroy-and-rebuild behaviour that
	 * predates the cache. The cell tier has no sprixels to save and no reason
	 * to hold six full-screen snapshots in memory.
	 */
	if (!render_pixel_planes_reliable(ctx))
		return ;
	if (ctx->bg_plane == NULL || strlen(path) >= BACKDROP_PATH_MAX)
		return ;
	entry = backdrop_find(ctx, path, exact, stretch);
	if (entry == NULL)
		entry = backdrop_evict(ctx);
	if (entry == NULL)
		return ;
	if (entry->plane != NULL && entry->plane != ctx->bg_plane)
		ncplane_destroy(entry->plane);
	free(entry->pixels);
	entry->pixels = NULL;
	entry->pixels_width = 0;
	entry->pixels_height = 0;
	snprintf(entry->path, sizeof(entry->path), "%s", path);
	entry->exact = exact;
	entry->stretch = stretch;
	entry->plane = ctx->bg_plane;
	entry->rows = ctx->bg_rows;
	entry->cols = ctx->bg_cols;
	entry->used = ++ctx->backdrop_tick;
	backdrop_keep_snapshot(ctx, entry);
}

/**
 * @brief Moves an idle backdrop entirely out of the rendered area.
 *
 * Leaving it stacked under the live backdrop is what made a revisit expensive:
 * a covered sprixel is torn down and sent again when it resurfaces. A plane
 * that is merely somewhere else was never covered, so nothing has to be
 * rebuilt to bring it back - the docs are explicit that a sprixel survives
 * being moved.
 */
static void	backdrop_park(t_render_ctx *ctx, struct ncplane *plane)
{
	if (plane == NULL || !backdrop_is_cached(ctx, plane))
		return ;
	(void)ncplane_move_yx(plane, -(int)ncplane_dim_y(plane), 0);
}

/**
 * @brief Returns a free slot, or frees the least recently shown one.
 *
 * The plane on screen is never evicted: it is the one thing that cannot be
 * rebuilt without the screen going blank first.
 */
static t_backdrop_cache	*backdrop_evict(t_render_ctx *ctx)
{
	t_backdrop_cache	*oldest;
	int					index;

	oldest = NULL;
	index = 0;
	while (index < BACKDROP_CACHE_MAX)
	{
		if (ctx->backdrops[index].plane == NULL)
			return (&ctx->backdrops[index]);
		if (ctx->backdrops[index].plane != ctx->bg_plane
			&& (oldest == NULL || ctx->backdrops[index].used < oldest->used))
			oldest = &ctx->backdrops[index];
		index++;
	}
	return (oldest);
}

/**
 * @brief Copies the stationary tier's overlay snapshot into the entry.
 *
 * Only that tier fills backdrop_pixels, so this is a no-op elsewhere. Where it
 * does apply, the shared snapshot belongs to whichever backdrop was built last,
 * and a restacked screen would otherwise composite its overlays over another
 * screen's art.
 */
static void	backdrop_keep_snapshot(t_render_ctx *ctx, t_backdrop_cache *entry)
{
	size_t	count;

	if (ctx->backdrop_pixels == NULL || ctx->backdrop_width <= 0
		|| ctx->backdrop_height <= 0)
		return ;
	count = (size_t)ctx->backdrop_width * (size_t)ctx->backdrop_height;
	entry->pixels = malloc(count * sizeof(*entry->pixels));
	if (entry->pixels == NULL)
		return ;
	memcpy(entry->pixels, ctx->backdrop_pixels,
		count * sizeof(*entry->pixels));
	entry->pixels_width = ctx->backdrop_width;
	entry->pixels_height = ctx->backdrop_height;
}

/**
 * @brief Puts a retained overlay snapshot back in front of the shared one.
 */
static void	backdrop_restore_snapshot(t_render_ctx *ctx,
	const t_backdrop_cache *entry)
{
	size_t	count;

	render_backdrop_forget(ctx);
	if (entry->pixels == NULL)
		return ;
	count = (size_t)entry->pixels_width * (size_t)entry->pixels_height;
	ctx->backdrop_pixels = malloc(count * sizeof(*ctx->backdrop_pixels));
	if (ctx->backdrop_pixels == NULL)
		return ;
	memcpy(ctx->backdrop_pixels, entry->pixels,
		count * sizeof(*ctx->backdrop_pixels));
	ctx->backdrop_width = entry->pixels_width;
	ctx->backdrop_height = entry->pixels_height;
}

/**
 * @brief Frees every retained backdrop, leaving the live one to the caller.
 */
static void	backdrop_cache_clear(t_render_ctx *ctx)
{
	int	index;

	index = 0;
	while (index < BACKDROP_CACHE_MAX)
	{
		if (ctx->backdrops[index].plane != NULL
			&& ctx->backdrops[index].plane != ctx->bg_plane)
			ncplane_destroy(ctx->backdrops[index].plane);
		ctx->backdrops[index].plane = NULL;
		free(ctx->backdrops[index].pixels);
		ctx->backdrops[index].pixels = NULL;
		ctx->backdrops[index].pixels_width = 0;
		ctx->backdrops[index].pixels_height = 0;
		ctx->backdrops[index].path[0] = '\0';
		index++;
	}
}

/**
 * @brief Replaces only the backdrop within the active notcurses session.
 *
 * AI-assisted: the replacement is rendered before the old plane is destroyed,
 * so a failed image transfer leaves the previous screen recoverable.
 *
 * @param ctx Active render context.
 * @param image_path Image to load into the replacement plane.
 * @param stretch Whether to fill the entire terminal instead of letterboxing.
 * @return 0 on success, -1 when loading, allocation, or rendering fails.
 */
int	render_background_replace(t_render_ctx *ctx, const char *image_path,
	bool stretch)
{
	struct ncvisual			*ncv;
	int						result;

	backdrop_fit(ctx, stretch);
	if (backdrop_restack(ctx, image_path, false, stretch))
		return (0);
	ncv = ncvisual_from_file(image_path);
	if (ncv == NULL)
		return (-1);
	result = render_background_replace_visual(ctx, ncv, stretch);
	/* This visual is ours to consume, so it can be reshaped for the cache. */
	if (result == 0)
		capture_backdrop(ctx, ncv);
	if (result == 0)
		backdrop_remember(ctx, image_path, false, stretch);
	ncvisual_destroy(ncv);
	return (result);
}

/**
 * @brief Drops the cached backdrop snapshot.
 *
 * Callers that replace the backdrop through a visual they still own cannot
 * hand it over for caching, so they invalidate instead of caching stale art.
 *
 * @param ctx Active render context.
 */
void	render_backdrop_forget(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	free(ctx->backdrop_pixels);
	ctx->backdrop_pixels = NULL;
	ctx->backdrop_width = 0;
	ctx->backdrop_height = 0;
}

/**
 * @brief Returns the backdrop pixels a stationary overlay must composite over.
 *
 * Active screen compositors keep richer caches than the plain backdrop — each
 * includes the live content an overlay may cross — so the active leaderboard
 * and then Settings win when their geometry still matches. Every candidate is
 * anchored at bg_row/bg_col.
 *
 * @param ctx Active render context.
 * @param width Receives the snapshot width in pixels.
 * @param height Receives the snapshot height in pixels.
 * @return Borrowed pixels, or NULL when no snapshot matches the geometry.
 */
const uint32_t	*render_backdrop_pixels(const t_render_ctx *ctx,
	int *width, int *height)
{
	if (ctx == NULL || ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0
		|| ctx->bg_rows <= 0 || ctx->bg_cols <= 0)
		return (NULL);
	*width = ctx->bg_cols * ctx->cell_px_x;
	*height = ctx->bg_rows * ctx->cell_px_y;
	if (ctx->leaderboard_pixel_active && ctx->leaderboard_pixels != NULL
		&& ctx->leaderboard_pixels_width == *width
		&& ctx->leaderboard_pixels_height == *height)
		return (ctx->leaderboard_pixels);
	if (ctx->settings_static_pixels != NULL
		&& ctx->settings_pixels_width == *width
		&& ctx->settings_pixels_height == *height)
		return (ctx->settings_static_pixels);
	if (ctx->settings_background_pixels != NULL
		&& ctx->settings_background_rows == ctx->bg_rows
		&& ctx->settings_background_cols == ctx->bg_cols)
		return (ctx->settings_background_pixels);
	if (ctx->backdrop_pixels != NULL && ctx->backdrop_width == *width
		&& ctx->backdrop_height == *height)
		return (ctx->backdrop_pixels);
	return (NULL);
}

/**
 * @brief Flattens the fitted backdrop into the reusable overlay snapshot.
 *
 * Only the stationary tier needs it, and only that tier pays for it: the walk
 * is one ncvisual_at_yx() per pixel, which is why it runs on backdrop changes
 * rather than per frame. The visual is resized in place, so callers must own
 * it and must not reuse it afterwards.
 *
 * @param ctx Active render context.
 * @param ncv Disposable visual holding the new backdrop.
 */
static void	capture_backdrop(t_render_ctx *ctx, struct ncvisual *ncv)
{
	int	width;
	int	height;

	render_backdrop_forget(ctx);
	if (ctx->pixels != TETRISU_PIXELS_STATIONARY || ctx->cell_px_x <= 0
		|| ctx->cell_px_y <= 0 || ctx->bg_rows <= 0 || ctx->bg_cols <= 0
		|| ctx->bg_rows > INT_MAX / ctx->cell_px_y
		|| ctx->bg_cols > INT_MAX / ctx->cell_px_x)
		return ;
	width = ctx->bg_cols * ctx->cell_px_x;
	height = ctx->bg_rows * ctx->cell_px_y;
	if ((size_t)width > SIZE_MAX / (size_t)height / sizeof(uint32_t))
		return ;
	if (ncvisual_resize(ncv, height, width) != 0)
		return ;
	ctx->backdrop_pixels = malloc((size_t)width * (size_t)height
			* sizeof(*ctx->backdrop_pixels));
	if (ctx->backdrop_pixels == NULL)
		return ;
	read_backdrop_pixels(ctx, ncv, width, height);
	ctx->backdrop_width = width;
	ctx->backdrop_height = height;
}

/**
 * @brief Copies a fitted visual into the snapshot buffer as opaque pixels.
 */
static void	read_backdrop_pixels(t_render_ctx *ctx, struct ncvisual *ncv,
	int width, int height)
{
	uint32_t	pixel;
	int			y;
	int			x;

	y = 0;
	while (y < height)
	{
		x = 0;
		while (x < width)
		{
			if (ncvisual_at_yx(ncv, (unsigned)y, (unsigned)x, &pixel) < 0)
				pixel = ncpixel(8, 8, 31);
			ncpixel_set_a(&pixel, 255u);
			ctx->backdrop_pixels[(size_t)y * width + x] = pixel;
			x++;
		}
		y++;
	}
}

/**
 * @brief Replaces the background through an exact-size bitmap plane.
 *
 * Auth, Settings, and Leaderboard artwork contains authored detail that cannot
 * survive conversion to a 4x2 terminal-cell mosaic. The visual is resized once
 * to the physical pixel geometry of its fitted plane, then transferred without
 * another scale.
 * NODEGRADE keeps this path honest: unsupported terminals fall back through
 * the caller's native renderer instead of quietly degrading the artwork.
 *
 * @param ctx Active render context.
 * @param image_path Image to load into the replacement plane.
 * @param stretch Whether to fill the terminal instead of letterboxing.
 * @return 0 on success, -1 when exact bitmap rendering is unavailable.
 */
int	render_background_replace_exact(t_render_ctx *ctx,
	const char *image_path, bool stretch)
{
	struct ncvisual	*ncv;
	int				pixel_rows;
	int				pixel_cols;
	int				result;

	if (ctx == NULL || image_path == NULL || !render_pixels_available(ctx))
		return (-1);
	backdrop_fit(ctx, stretch);
	if (backdrop_restack(ctx, image_path, true, stretch))
		return (0);
	ncv = ncvisual_from_file(image_path);
	if (ncv == NULL)
		return (-1);
	if (ctx->cell_px_y <= 0 || ctx->cell_px_x <= 0
		|| ctx->bg_rows > INT_MAX / ctx->cell_px_y
		|| ctx->bg_cols > INT_MAX / ctx->cell_px_x)
	{
		ncvisual_destroy(ncv);
		return (-1);
	}
	pixel_rows = ctx->bg_rows * ctx->cell_px_y;
	pixel_cols = ctx->bg_cols * ctx->cell_px_x;
	if (ncvisual_resize(ncv, pixel_rows, pixel_cols) != 0)
	{
		ncvisual_destroy(ncv);
		return (-1);
	}
	result = replace_visual_scaled(ctx, ncv, stretch, NCSCALE_NONE,
			NCBLIT_PIXEL, NCVISUAL_OPTION_NODEGRADE);
	if (result == 0)
		capture_backdrop(ctx, ncv);
	if (result == 0)
		backdrop_remember(ctx, image_path, true, stretch);
	ncvisual_destroy(ncv);
	return (result);
}

/**
 * @brief Replaces the background with an already composed visual.
 *
 * The caller retains ownership of ncv. Keeping the sizing and replacement
 * logic here ensures synthesized visuals obey the same exact plane contract as
 * file-backed artwork.
 */
int	render_background_replace_visual(t_render_ctx *ctx,
	struct ncvisual *ncv, bool stretch)
{
	/*
	 * The caller keeps ncv, so it cannot be reshaped into a snapshot here.
	 * Dropping the old one is the honest outcome: overlays fall back to
	 * plain transparency rather than compositing over a stale backdrop.
	 */
	render_backdrop_forget(ctx);
	return (replace_visual_scaled(ctx, ncv, stretch, NCSCALE_STRETCH,
			preferred_blitter(ctx, 0, 0), NCVISUAL_OPTION_NOINTERPOLATE));
}

static int	replace_visual_scaled(t_render_ctx *ctx, struct ncvisual *ncv,
	bool stretch, ncscale_e scaling, ncblitter_e blitter, uint64_t flags)
{
	struct ncvisual_options	vopts;
	ncplane_options			bg_opts;
	struct ncplane			*new_plane;
	struct ncplane			*old_plane;
	unsigned				std_rows;
	unsigned				std_cols;

	if (ctx == NULL || ncv == NULL)
		return (-1);
	ncplane_dim_yx(ctx->std, &std_rows, &std_cols);
	refresh_cell_geometry(ctx);
	if (stretch)
	{
		ctx->bg_row = 0;
		ctx->bg_col = 0;
		ctx->bg_rows = (int)std_rows;
		ctx->bg_cols = (int)std_cols;
	}
	else
		fit_background_to_terminal(ctx, (int)std_rows, (int)std_cols);
	memset(&bg_opts, 0, sizeof(bg_opts));
	bg_opts.y = ctx->bg_row;
	bg_opts.x = ctx->bg_col;
	bg_opts.rows = ctx->bg_rows;
	bg_opts.cols = ctx->bg_cols;
	new_plane = ncplane_create(ctx->std, &bg_opts);
	if (new_plane == NULL)
		return (-1);
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = new_plane;
	vopts.scaling = scaling;
	vopts.blitter = blitter;
	vopts.flags = flags;
	if (ncvisual_blit(ctx->nc, ncv, &vopts) == NULL)
	{
		ncplane_destroy(new_plane);
		return (-1);
	}
	old_plane = ctx->bg_plane;
	(void)ncplane_move_above(new_plane, ctx->std);
	set_opaque_backdrop(ctx->std);
	/*
	 * The outgoing backdrop has to leave the rendered area before the frame is
	 * drawn, not after. Stacking the new plane over it and rendering costs the
	 * whole of the old bitmap again: a covered sprixel is torn down and
	 * re-sent, and a cell plane cannot occlude one however high it sits. That
	 * single misordered render was 31 s of the first sign-in, because the
	 * bitmap it re-sent was the full-screen login artwork.
	 *
	 * A retained backdrop is parked rather than destroyed so its sprixel
	 * survives for the screen that will reclaim it; one nobody kept is dropped
	 * here, since leaving it to be destroyed after the render would still have
	 * paid to transmit it. If the render then fails the frame is left showing
	 * the opaque standard plane, which is the honest outcome on a path whose
	 * only caller treats the failure as fatal.
	 */
	if (old_plane != NULL && !backdrop_is_cached(ctx, old_plane))
	{
		ncplane_destroy(old_plane);
		ctx->bg_plane = NULL;
	}
	else
		backdrop_park(ctx, old_plane);
	if (notcurses_render(ctx->nc) != 0)
	{
		ncplane_destroy(new_plane);
		(void)notcurses_render(ctx->nc);
		return (-1);
	}
	ctx->bg_plane = new_plane;
	return (0);
}

/**
 * @brief Consumes one already-parsed event from the Notcurses input queue.
 *
 * Notcurses parses a whole terminal read burst into its own queue but signals
 * the input-ready descriptor only while that queue is non-empty at poll time.
 * A loop that sleeps on the descriptor after taking a single event therefore
 * strands the rest of a burst until the *next* keystroke wakes the descriptor,
 * which delivers every press one keystroke late. The queue must be exhausted
 * before poll() is allowed to sleep again.
 *
 * @param ctx Pointer to the render context.
 * @param event Destination for the complete Notcurses input event.
 * @param key Receives the key id when an event is taken.
 * @return true when @p key holds an event the caller must handle.
 */
static bool	take_parsed_event(t_render_ctx *ctx, ncinput *event, uint32_t *key)
{
	while (1)
	{
		memset(event, 0, sizeof(*event));
		errno = 0;
		*key = notcurses_get_nblock(ctx->nc, event);
		if (*key == 0)
			return (false);
		if (*key == (uint32_t)-1)
		{
			if (errno == EINTR)
				continue ;
			return (true);
		}
		/* Ignore keyboard key-up events so one arrow tap moves once. Mouse
		 * motion can legitimately arrive with release/no-button state and
		 * must still reach the menu for hover selection. */
		if (event->evtype != NCTYPE_RELEASE || nckey_mouse_p(*key))
			return (true);
	}
}

/**
 * @brief Computes the poll timeout honouring the notification wake deadline.
 *
 * @param ctx Pointer to the render context.
 * @return Milliseconds to wait before the next timer-driven repaint.
 */
static int	next_input_wait_ms(const t_render_ctx *ctx)
{
	int	notification_wait_ms;

	notification_wait_ms = render_notification_next_wake_ms(ctx);
	if (notification_wait_ms >= 0
		&& notification_wait_ms < RENDER_RESIZE_POLL_MS)
		return (notification_wait_ms);
	return (RENDER_RESIZE_POLL_MS);
}

/**
 * @brief Blocks until one input event is available, returning its key id.
 *
 * @param ctx Pointer to the render context.
 * @return The Unicode codepoint or NCKEY_* constant for the event, or
 * (uint32_t)-1 on input error.
 */
uint32_t	render_wait_key(t_render_ctx *ctx)
{
	return (render_wait_input(ctx, NULL));
}

/**
 * @brief Blocks until one non-release input event is available.
 *
 * Supplying the full event lets screens use mouse coordinates and, later,
 * terminal protocols with distinct press/repeat/release states. Callers which
 * only need a key id can continue using render_wait_key().
 *
 * @param ctx Pointer to the render context.
 * @param input Optional destination for the complete Notcurses input event.
 * @return The Unicode codepoint or NCKEY_* constant for the event, or
 * (uint32_t)-1 on input error.
 */
uint32_t	render_wait_input(t_render_ctx *ctx, ncinput *input)
{
	ncinput		local;
	ncinput		*event;
	struct pollfd	input_fd;
	int			poll_result;
	uint32_t	key;

	event = input;
	if (event == NULL)
		event = &local;
	memset(&input_fd, 0, sizeof(input_fd));
	input_fd.fd = notcurses_inputready_fd(ctx->nc);
	input_fd.events = POLLIN;
	while (1)
	{
		if (render_notification_next_wake_ms(ctx) == 0)
			render_notification_tick(ctx);
		if (take_parsed_event(ctx, event, &key))
			return (key);
		poll_result = poll(&input_fd, 1, next_input_wait_ms(ctx));
		if (poll_result < 0)
		{
			if (errno == EINTR)
				continue ;
			return ((uint32_t)-1);
		}
		if (render_notification_next_wake_ms(ctx) == 0)
			render_notification_tick(ctx);
		if (poll_result == 0)
		{
			if (render_terminal_geometry_changed(ctx))
				return (NCKEY_RESIZE);
			continue ;
		}
		if ((input_fd.revents & POLLIN) == 0)
			return ((uint32_t)-1);
	}
}

/**
 * @brief Waits for one input event, giving up after timeout_ms milliseconds.
 *
 * The waiting room's countdown is the only value in the client that advances
 * without a keystroke, so it needs a wait that returns on its own deadline
 * rather than only on input. 0 is the "nothing arrived" answer because that is
 * already what notcurses_get_nblock() returns for an empty queue, so callers
 * that drain the queue and callers that time out test the same value.
 *
 * @param ctx Pointer to the render context.
 * @param input Optional destination for the complete Notcurses input event.
 * @param timeout_ms Deadline in milliseconds; negative waits indefinitely.
 * @return The key id, 0 when the deadline elapsed, or (uint32_t)-1 on error.
 */
uint32_t	render_wait_input_timeout(t_render_ctx *ctx, ncinput *input,
	int timeout_ms)
{
	ncinput			local;
	ncinput			*event;
	struct pollfd	input_fd;
	int				remaining_ms;
	int				poll_result;
	uint32_t		key;

	event = input;
	if (event == NULL)
		event = &local;
	if (timeout_ms < 0)
		return (render_wait_input(ctx, event));
	memset(&input_fd, 0, sizeof(input_fd));
	input_fd.fd = notcurses_inputready_fd(ctx->nc);
	input_fd.events = POLLIN;
	remaining_ms = timeout_ms;
	while (1)
	{
		if (render_notification_next_wake_ms(ctx) == 0)
			render_notification_tick(ctx);
		if (take_parsed_event(ctx, event, &key))
			return (key);
		if (remaining_ms <= 0)
			return (0);
		poll_result = poll(&input_fd, 1, min_wait_ms(ctx, remaining_ms));
		if (poll_result < 0)
		{
			if (errno == EINTR)
				continue ;
			return ((uint32_t)-1);
		}
		if (render_notification_next_wake_ms(ctx) == 0)
			render_notification_tick(ctx);
		if (poll_result == 0)
		{
			/*
			 * The poll slice is capped by the resize and notification deadlines,
			 * so a zero return is not necessarily this call's timeout. Charge the
			 * slice against the remaining budget and only give up once it runs
			 * out.
			 */
			remaining_ms -= min_wait_ms(ctx, remaining_ms);
			if (render_terminal_geometry_changed(ctx))
				return (NCKEY_RESIZE);
			continue ;
		}
		if ((input_fd.revents & POLLIN) == 0)
			return ((uint32_t)-1);
	}
}

/**
 * @brief Returns the shorter of the shared poll slice and one caller deadline.
 */
static int	min_wait_ms(const t_render_ctx *ctx, int deadline_ms)
{
	int	wait_ms;

	wait_ms = next_input_wait_ms(ctx);
	if (deadline_ms < wait_ms)
		return (deadline_ms);
	return (wait_ms);
}

/**
 * @brief Stops notcurses, restoring the terminal to normal mode.
 *
 * @param ctx Pointer to the render context to tear down.
 */
void	render_teardown(t_render_ctx *ctx)
{
	if (ctx->nc != NULL)
	{
		render_notification_destroy(ctx);
		render_backdrop_forget(ctx);
		render_compatibility_badge_hide(ctx);
		render_auth_pixel_overlay_destroy(ctx);
		render_auth_pixel_background_reset(ctx);
		render_leaderboard_pixel_destroy(ctx);
		render_settings_pixel_destroy(ctx);
		render_confirmation_font_release(ctx);
		if (ctx->auth_font_visual != NULL)
		{
			ncvisual_destroy(ctx->auth_font_visual);
			ctx->auth_font_visual = NULL;
		}
		/* Runs before notcurses_stop() because the menu owns a decoded sprite
		 * that no plane teardown would release. */
		render_menu_destroy(ctx);
		notcurses_stop(ctx->nc);
		ctx->nc = NULL;
		ctx->std = NULL;
		ctx->bg_plane = NULL;
		ctx->menu_plane = NULL;
		ctx->menu_labels_plane = NULL;
		ctx->auth_background_signature = 0;
		ctx->bunny_plane = NULL;
		ctx->pixels = TETRISU_PIXELS_NONE;
	}
}

/**
 * @brief Probes the terminal once and resolves the renderer capability tier.
 *
 * @param ctx Render context holding a started notcurses instance.
 * @return The tier every later render decision is derived from.
 */
static t_tetrisu_pixel_policy	detect_pixel_policy(const t_render_ctx *ctx)
{
	t_tetrisu_pixel_policy	policy;
	char					*term;

	term = notcurses_detected_terminal(ctx->nc);
	policy = tetrisu_pixel_policy_for(notcurses_check_pixel_support(ctx->nc),
			term, tetrisu_renderer_mode_requested());
	free(term);
	return (policy);
}

/**
 * @brief Records the terminal cell dimensions in physical pixels.
 *
 * @param ctx Render context updated with a portable 2:1 fallback when the
 * terminal exposes no bitmap geometry.
 */
static void	refresh_cell_geometry(t_render_ctx *ctx)
{
	unsigned	cell_px_y;
	unsigned	cell_px_x;

	cell_px_y = 0;
	cell_px_x = 0;
	ncplane_pixel_geom(ctx->std, NULL, NULL, &cell_px_y, &cell_px_x,
		NULL, NULL);
	/* A non-bitmap terminal may not report pixels. A 2:1 cell is the safest
	 * portable fallback for keeping the 4:3 art physically proportional. */
	if (cell_px_y == 0)
		cell_px_y = 2;
	if (cell_px_x == 0)
		cell_px_x = 1;
	ctx->cell_px_y = (int)cell_px_y;
	ctx->cell_px_x = (int)cell_px_x;
}

/**
 * @brief Fits the authored background inside terminal geometry.
 *
 * @param ctx Context receiving fitted origin and dimensions.
 * @param std_rows Available terminal rows.
 * @param std_cols Available terminal columns.
 */
static void	fit_background_to_terminal(t_render_ctx *ctx,
	int std_rows, int std_cols)
{
	double	image_ratio;
	int		rows;
	int		cols;

	image_ratio = (double)BACKGROUND_SOURCE_PIXELS_X
		/ BACKGROUND_SOURCE_PIXELS_Y;
	cols = std_cols;
	rows = (int)((double)cols * ctx->cell_px_x
		/ (image_ratio * ctx->cell_px_y) + 0.5);
	if (rows > std_rows)
	{
		rows = std_rows;
		cols = (int)((double)rows * ctx->cell_px_y * image_ratio
			/ ctx->cell_px_x + 0.5);
	}
	ctx->bg_rows = max_int(rows, 1);
	ctx->bg_cols = max_int(cols, 1);
	ctx->bg_row = (std_rows - ctx->bg_rows) / 2;
	ctx->bg_col = (std_cols - ctx->bg_cols) / 2;
}

/**
 * @brief Returns the greater of two integers.
 *
 * @param a First value.
 * @param b Second value.
 * @return The greater value.
 */
static int	max_int(int a, int b)
{
	if (a > b)
		return (a);
	return (b);
}

/**
 * @brief Selects the low-bandwidth blitter used for decorative backgrounds.
 *
 * @param ctx Active render context (unused).
 * @param rows Destination row count (unused).
 * @param cols Destination column count (unused).
 * @return The 4x2 cell blitter.
 */
static ncblitter_e	preferred_blitter(const t_render_ctx *ctx,
	int rows, int cols)
{
	(void)ctx;
	(void)rows;
	(void)cols;
	/* Decorative backdrops are intentionally cell-rendered. This leaves the
	 * bitmap layer for stationary crisp characters, pieces, and text. */
	return (NCBLIT_4x2);
}

/**
 * @brief Clears a plane to the playfield fallback colour.
 *
 * @param plane Plane whose base cell and contents are replaced.
 */
static void	set_opaque_backdrop(struct ncplane *plane)
{
	uint64_t	channels;

	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, 7, 13, 23);
	(void)ncchannels_set_bg_rgb8(&channels, 7, 13, 23);
	(void)ncplane_set_base(plane, " ", 0, channels);
	ncplane_erase(plane);
}
