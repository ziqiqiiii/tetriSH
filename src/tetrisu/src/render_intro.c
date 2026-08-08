#include "tetrisu.h"

// Static Functions
static struct ncplane	*create_intro_plane(t_render_ctx *ctx);
static int	intro_streamer(struct ncvisual *ncv,
	struct ncvisual_options *vopts, const struct timespec *tspec, void *curry);
static bool	intro_skip_requested(uint32_t key, const ncinput *input);

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
int	render_intro_play(t_render_ctx *ctx, t_audio_ctx *audio,
	const char *video_path, const char *audio_path)
{
	struct ncvisual			*ncv;
	struct ncplane			*plane;
	struct ncvisual_options	vopts;
	t_intro_stream			intro;
	int						ret;

	if (ctx == NULL || video_path == NULL)
		return (0);
	ncv = ncvisual_from_file(video_path);
	if (ncv == NULL)
		return (0);
	/*
	 * The initial home artwork can be a Kitty bitmap (sprixel). A cell-blitted
	 * video cannot reliably cover an existing sprixel even when its plane is
	 * higher in the Notcurses pile, so remove the placeholder before streaming.
	 * The auth screen installs its own background as soon as playback ends.
	 */
	render_background_destroy(ctx);
	plane = create_intro_plane(ctx);
	if (plane == NULL)
	{
		ncvisual_destroy(ncv);
		return (0);
	}
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = plane;
	vopts.scaling = NCSCALE_STRETCH;
	/* NCBLIT_PIXEL was tried here and made playback slow-motion: every frame
	 * becomes a full bitmap transfer instead of cheap glyph diffing, and the
	 * terminal can't encode/send them fast enough to keep up with real time.
	 * NCBLIT_4x2 (octants) stays glyph/cell-based like 2x2, just denser, so
	 * it shouldn't carry that same cost. */
	vopts.blitter = NCBLIT_4x2;
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

/**
 * @brief Creates the plane that bounds streamed intro frames.
 *
 * @param ctx Render context containing fitted background geometry.
 * @return New child plane, or NULL when allocation fails.
 */
static struct ncplane	*create_intro_plane(t_render_ctx *ctx)
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
 * @brief Presents one timed intro frame and checks for skip input.
 *
 * @param ncv Decoded notcurses visual stream.
 * @param vopts Blit options targeting the intro plane.
 * @param tspec Absolute frame presentation deadline.
 * @param curry Intro state supplied by render_intro_play().
 * @return 0 to continue, 1 to stop on input, or a renderer error code.
 */
static int	intro_streamer(struct ncvisual *ncv,
	struct ncvisual_options *vopts, const struct timespec *tspec, void *curry)
{
	t_intro_stream	*intro;
	ncinput			ni;
	uint32_t		key;
	int				ret;

	intro = (t_intro_stream *)curry;
	ret = ncvisual_simple_streamer(ncv, vopts, tspec, NULL);
	if (ret != 0)
		return (ret);
	key = notcurses_get_nblock(intro->ctx->nc, &ni);
	if (intro_skip_requested(key, &ni))
	{
		intro->skipped = 1;
		return (1);
	}
	return (0);
}

/**
 * @brief Accepts only explicit intro-skip controls.
 *
 * Kitty can emit an ESC while negotiating terminal state. Treating arbitrary
 * key events as a skip made startup nondeterministic, so only Space, Enter,
 * or a primary-button press may leave the intro early.
 */
static bool	intro_skip_requested(uint32_t key, const ncinput *input)
{
	if (input == NULL || key == 0 || key == (uint32_t)-1
		|| input->evtype == NCTYPE_RELEASE || key == NCKEY_INVALID
		|| key == NCKEY_RESIZE || key == NCKEY_SIGNAL
		|| key == NCKEY_MOTION)
		return (false);
	if (nckey_mouse_p(key))
		return (key == NCKEY_BUTTON1
			&& (input->evtype == NCTYPE_PRESS
				|| input->evtype == NCTYPE_UNKNOWN));
	return (key == ' ' || key == '\n' || key == '\r'
		|| key == NCKEY_ENTER);
}
