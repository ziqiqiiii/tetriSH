# ifndef TETRISU_H
# define TETRISU_H

# include <assert.h>
# include <stdint.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/wait.h>
# include <sys/select.h>
# include <sys/ioctl.h>
# include <notcurses/notcurses.h>
# include "tetrisbrain.h"

# ifdef __APPLE__
# include <util.h>
# else
# include <pty.h>
# endif

# ifndef ASSET_DIR
# define ASSET_DIR	"."
# endif

# ifndef TETRISU_BIN_PATH
# define TETRISU_BIN_PATH	"./bin/tetrisu"
# endif

# ifndef TETRISU_ENABLE_AUDIO
# define TETRISU_ENABLE_AUDIO	0
# endif

# define SPLASH_ASSET_PATH	ASSET_DIR "/default_theme/default_homepage.png"
# define BUNNY_ASSET_PATH	ASSET_DIR "/default_theme/default_bunny_ghost_pointer.png"
# define INTRO_VIDEO_PATH	ASSET_DIR "/intro.mp4"
# define INTRO_AUDIO_PATH	ASSET_DIR "/intro.mp3"
# define HOME_BGM_PATH		ASSET_DIR "/tetris_theme.mp3"
# define HOME_BGM_START_VOLUME	48
# define SOLO_BACKGROUND_PATH	ASSET_DIR "/default_theme/default_background_4x3.png"
# define DEFAULT_HUD_PATH	ASSET_DIR "/default_theme/default_board.png"
# define DEFAULT_TILE_PATH	ASSET_DIR "/default_theme/default_tile.png"
# define DEFAULT_MIRURUN_PATH	ASSET_DIR "/default_theme/default_mirurun.png"
# define SHARED_FONT_MASK_PATH	ASSET_DIR "/shared_font_mask.png"
# define SHARED_NUMBERS_MASK_PATH	ASSET_DIR "/shared_numbers_mask.png"
# define MENU_MOVE_SFX_PATH	ASSET_DIR "/menu_move.wav"
# define MENU_SELECT_SFX_PATH	ASSET_DIR "/menu_select.wav"
# define MENU_ITEM_COUNT	4
# define SOLO_NEXT_COUNT	3
# define SOLO_CRYSTAL_CAPACITY	10
# define SOLO_CLEAR_ANIMATION_MS	200
# define SOLO_TOP_OUT_REVEAL_MS	350
# define SOLO_LOCK_DELAY_MS	500
# define SOLO_LOCK_RESET_LIMIT	15

typedef enum
{
  APP_SPLASH,
  APP_MAIN_MENU,
  APP_QUIT,
}	app_state_t;

typedef struct
{
  int	selected;
}	menu_selection_t;

typedef struct
{
  int	enabled;
  int	music_volume;
  void	*music;
  void	*menu_move_sfx;
  void	*menu_select_sfx;
}	audio_ctx_t;

// Bundles every notcurses handle the render layer needs across calls. The
// background geometry records the rendered image size, so menu overlays can
// follow the art even when notcurses scales it to different terminals.
typedef struct
{
  struct notcurses	*nc;
  struct ncplane	*std;
  struct ncplane	*bg_plane;
  struct ncplane	*menu_plane;
  struct ncplane	*bunny_plane;
  int				bg_row;
  int				bg_col;
  int				bg_rows;
  int				bg_cols;
  int				cell_px_y;
  int				cell_px_x;
  int				menu_row;
  int				menu_col;
  int				bunny_rows;
  int				bunny_cols;
}	render_ctx_t;

typedef enum e_solo_phase
{
	SOLO_ACTIVE,
	SOLO_CLEARING,
	SOLO_TOP_OUT_REVEAL,
	SOLO_GAME_OVER
}	solo_phase_t;

typedef enum e_solo_action
{
	SOLO_MOVE_LEFT,
	SOLO_MOVE_RIGHT,
	SOLO_ROTATE_CW,
	SOLO_ROTATE_CCW,
	SOLO_SOFT_DROP,
	SOLO_HARD_DROP
}	solo_action_t;

typedef struct s_solo_game
{
	t_board			board;
	t_piece			active;
	t_piece_type	next[SOLO_NEXT_COUNT];
	t_piece_bag	bag;
	t_score_state	scoring;
	t_score_result	last_score;
	solo_phase_t	phase;
	t_spin_type	pending_spin;
	t_spin_type	last_spin;
	int				clear_rows[BRAIN_MAX_CLEAR_LINES];
	int				clear_count;
	int				last_lines;
	int				total_lines;
	int				level;
	int				crystal_charge;
	int				gravity_elapsed_ms;
	int				lock_elapsed_ms;
	int				clear_elapsed_ms;
	int				top_out_elapsed_ms;
	int				lock_resets;
	int				last_kick_index;
	bool			last_action_was_rotation;
	bool			last_perfect_clear;
	bool			paused;
}	solo_game_t;

typedef struct s_solo_render
{
	struct ncplane	*background_plane;
	struct ncplane	*next_plane;
	struct ncplane	*meter_plane;
	struct ncplane	*mirurun_plane;
	struct ncplane	*score_header_plane;
	struct ncplane	*score_value_plane;
	struct ncplane	*score_stats_plane;
	struct ncplane	*score_event_plane;
	struct ncplane	*controls_plane;
	struct ncplane	*board_overlay_plane;
	struct ncplane	*settled_runs[BOARD_HEIGHT][(BOARD_WIDTH + 1) / 2];
	struct ncplane	*active_plane;
	struct ncplane	*ghost_plane;
	struct ncplane	*status_plane;
	uint32_t		*static_pixels;
	uint32_t		*frame_pixels;
	uint32_t		*tile_pixels;
	uint32_t		*font_pixels;
	uint32_t		*number_pixels;
	int				tile_width;
	int				font_width;
	int				number_width;
	int				canvas_row;
	int				canvas_col;
	int				canvas_rows;
	int				canvas_cols;
	int				tile_rows;
	int				tile_cols;
	uint64_t		row_signatures[BOARD_HEIGHT];
	uint64_t		next_signature;
	uint64_t		meter_signature;
	uint64_t		score_value_signature;
	uint64_t		score_stats_signature;
	uint64_t		score_event_signature;
	uint64_t		overlay_signature;
	uint64_t		active_shape_signature;
	uint64_t		ghost_shape_signature;
	int				settled_run_counts[BOARD_HEIGHT];
	bool			piece_planes_combined;
	bool			layout_valid;
	bool			assets_ready;
	bool			planes_ready;
	bool			composite_board;
	char			asset_error[160];
}	solo_render_t;

/* APP_STATE.C */
app_state_t		app_handle_key(app_state_t current, uint32_t key);
void			menu_move_selection(menu_selection_t *m, uint32_t key);
const char		*menu_item_label(int index);
const char		*menu_stub_text(int selected_index);

/* RENDER_BACKGROUND.C */
render_ctx_t	render_init(const char *image_path);
uint32_t		render_wait_key(render_ctx_t *ctx);
void			render_teardown(render_ctx_t *ctx);
int				render_background_replace(render_ctx_t *ctx,
					const char *image_path, bool stretch);
void				render_background_destroy(render_ctx_t *ctx);
int				render_geometry_refresh(render_ctx_t *ctx, bool repaint);
bool				render_pixel_planes_reliable(const render_ctx_t *ctx);

/* RENDER_MENU.C */
void			render_menu_create(render_ctx_t *ctx);
void			render_menu_move_bunny(render_ctx_t *ctx, const menu_selection_t *m);
void			render_menu_show_message(render_ctx_t *ctx, const char *msg);
void			render_menu_destroy(render_ctx_t *ctx);

/* RENDER_INTRO.C */
int				render_intro_play(render_ctx_t *ctx, audio_ctx_t *audio,
					const char *video_path, const char *audio_path);

/* AUDIO.C */
int				audio_init(audio_ctx_t *audio);
void			audio_play_music(audio_ctx_t *audio, const char *path);
void			audio_play_once(audio_ctx_t *audio, const char *path);
void			audio_stop_music(audio_ctx_t *audio);
void			audio_load_menu_sfx(audio_ctx_t *audio, const char *move_path,
					const char *select_path);
void			audio_play_menu_move(audio_ctx_t *audio);
void			audio_play_menu_select(audio_ctx_t *audio);
void			audio_set_music_volume(audio_ctx_t *audio, int volume);
void			audio_volume_up(audio_ctx_t *audio);
void			audio_volume_down(audio_ctx_t *audio);
void			audio_teardown(audio_ctx_t *audio);

/* SOLO_GAME.C — pure local authority, replaced by tetrisd STATE later */
void			solo_game_init(solo_game_t *game, uint32_t seed);
bool			solo_game_apply_action(solo_game_t *game, solo_action_t action);
bool			solo_game_update(solo_game_t *game, int elapsed_ms);
int				solo_game_next_wake_ms(const solo_game_t *game);
t_piece			solo_game_ghost(const solo_game_t *game);
bool			solo_game_row_is_clearing(const solo_game_t *game, int row);
void			solo_game_toggle_pause(solo_game_t *game);

/* RENDER_SOLO.C */
void			render_solo_create(render_ctx_t *ctx, solo_render_t *solo);
void			render_solo_draw(render_ctx_t *ctx, solo_render_t *solo,
					const solo_game_t *game);
void			render_solo_resize(render_ctx_t *ctx, solo_render_t *solo);
void			render_solo_destroy(solo_render_t *solo);
int				solo_mode_run(render_ctx_t *ctx, audio_ctx_t *audio);

# endif
