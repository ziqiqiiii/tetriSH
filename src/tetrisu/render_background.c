#include "tetrisu.h"

/**
 * @brief Starts notcurses and blits image_path across the standard plane.
 *
 * The standard plane is notcurses' full-screen base plane; blitting the
 * image directly onto it (rather than a separate plane) makes it the
 * backdrop everything else renders on top of. notcurses installs its own
 * signal handlers by default (restores the screen on SIGINT/SIGTERM/etc.
 * before chaining to the previous handler), so no custom SIGINT handling
 * is needed here.
 *
 * @param image_path Path to the image file to render as the background.
 * @return A render_ctx_t with nc/std populated; menu_plane and bunny_plane
 * are NULL until render_menu_create() is called.
 */
render_ctx_t	render_init(const char *image_path)
{
	render_ctx_t			ctx;
	notcurses_options		opts;
	struct ncvisual			*ncv;
	struct ncvisual_options	vopts;

	memset(&opts, 0, sizeof(opts));
	ctx.nc = notcurses_init(&opts, NULL);
	if (ctx.nc == NULL)
	{
		fprintf(stderr, "tetrisu: notcurses_core_init failed (TERM=%s) — "
			"check terminfo for this terminal type\n", getenv("TERM"));
		exit(1);
	}
	ctx.std = notcurses_stdplane(ctx.nc);
	ctx.menu_plane = NULL;
	ctx.bunny_plane = NULL;
	ctx.menu_row = 0;
	ctx.menu_col = 0;
	ncv = ncvisual_from_file(image_path);
	if (ncv == NULL)
	{
		notcurses_stop(ctx.nc);
		fprintf(stderr, "tetrisu: failed to load image %s\n", image_path);
		exit(1);
	}
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = ctx.std;
	vopts.scaling = NCSCALE_SCALE;
	ncvisual_blit(ctx.nc, ncv, &vopts);
	ncvisual_destroy(ncv);
	notcurses_render(ctx.nc);
	return (ctx);
}

/**
 * @brief Blocks until one input event is available, returning its key id.
 *
 * @param ctx Pointer to the render context.
 * @return The Unicode codepoint or NCKEY_* constant for the event, or
 * (uint32_t)-1 on input error.
 */
uint32_t	render_wait_key(render_ctx_t *ctx)
{
	ncinput	ni;

	return (notcurses_get(ctx->nc, NULL, &ni));
}

/**
 * @brief Stops notcurses, restoring the terminal to normal mode.
 *
 * @param ctx Pointer to the render context to tear down.
 */
void	render_teardown(render_ctx_t *ctx)
{
	notcurses_stop(ctx->nc);
}
