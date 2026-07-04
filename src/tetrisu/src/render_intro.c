#include "tetrisu.h"

typedef struct
{
	render_ctx_t	*ctx;
	int				skipped;
}	intro_stream_t;

static int	intro_streamer(struct ncvisual *ncv,
	struct ncvisual_options *vopts, const struct timespec *tspec, void *curry)
{
	intro_stream_t	*intro;
	ncinput			ni;
	uint32_t		key;
	int				ret;

	intro = (intro_stream_t *)curry;
	ret = ncvisual_simple_streamer(ncv, vopts, tspec, NULL);
	if (ret != 0)
		return (ret);
	key = notcurses_get_nblock(intro->ctx->nc, &ni);
	if (key != 0 && key != (uint32_t)-1 && ni.evtype != NCTYPE_RELEASE)
	{
		intro->skipped = 1;
		return (1);
	}
	return (0);
}

static struct ncplane	*create_intro_plane(render_ctx_t *ctx)
{
	ncplane_options	opts;

	memset(&opts, 0, sizeof(opts));
	opts.y = ctx->bg_row;
	opts.x = ctx->bg_col;
	opts.rows = ctx->bg_rows;
	opts.cols = ctx->bg_cols;
	return (ncplane_create(ctx->std, &opts));
}

/**
 * @brief Plays the splash intro video, with optional best-effort audio.
 *
 * AI-assisted: notcurses owns video timing while SDL2_mixer owns audio; the
 * two start together and are intentionally only approximately synchronized.
 *
 * @param ctx Render context with the homepage background already drawn.
 * @param audio Audio context; can be disabled for silent video fallback.
 * @param video_path MP4 path to stream on top of the background.
 * @param audio_path MP3 path to play once alongside the video.
 * @return 1 if skipped by input, 0 if finished or skipped by missing assets.
 */
int	render_intro_play(render_ctx_t *ctx, audio_ctx_t *audio,
	const char *video_path, const char *audio_path)
{
	struct ncvisual			*ncv;
	struct ncplane			*plane;
	struct ncvisual_options	vopts;
	intro_stream_t			intro;
	int						ret;

	if (ctx == NULL || video_path == NULL)
		return (0);
	ncv = ncvisual_from_file(video_path);
	if (ncv == NULL)
		return (0);
	plane = create_intro_plane(ctx);
	if (plane == NULL)
	{
		ncvisual_destroy(ncv);
		return (0);
	}
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = plane;
	vopts.scaling = NCSCALE_STRETCH;
	/* A simpler blitter gives smoother terminal video than the dense bunny
	 * sprite path; the intro is moving media, so frame rate matters more. */
	vopts.blitter = NCBLIT_2x2;
	vopts.flags = NCVISUAL_OPTION_NOINTERPOLATE;
	intro.ctx = ctx;
	intro.skipped = 0;
	audio_play_once(audio, audio_path);
	ret = ncvisual_stream(ctx->nc, ncv, 1.0f, intro_streamer, &vopts, &intro);
	audio_stop_music(audio);
	ncvisual_destroy(ncv);
	ncplane_destroy(plane);
	notcurses_render(ctx->nc);
	if (ret > 0 || intro.skipped)
		return (1);
	return (0);
}
