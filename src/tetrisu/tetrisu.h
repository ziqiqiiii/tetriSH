# ifndef TETRISU_H
# define TETRISU_H

# include <assert.h>
# include <ctype.h>
# include <errno.h>
# include <inttypes.h>
# include <poll.h>
# include <stdint.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# include <unistd.h>
# include <sys/wait.h>
# include <sys/select.h>
# include <sys/ioctl.h>
# include <notcurses/notcurses.h>
# include "tetrisbrain.h"

# ifndef TETRISU_ENABLE_AUDIO
# define TETRISU_ENABLE_AUDIO	0
# endif

# if TETRISU_ENABLE_AUDIO
# include <SDL.h>
# include <SDL_mixer.h>
# endif

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

# define SPLASH_ASSET_PATH	ASSET_DIR "/updated_homepage.png"
# define AUTH_BACKGROUND_PATH \
	ASSET_DIR "/default_theme/auth_screen.png"
# define BUNNY_ASSET_PATH \
	ASSET_DIR "/default_theme/default_bunny_ghost_pointer.png"
# define INTRO_VIDEO_PATH	ASSET_DIR "/intro.mp4"
# define INTRO_AUDIO_PATH	ASSET_DIR "/intro.mp3"
# define HOME_BGM_PATH		ASSET_DIR "/tetris_theme.mp3"
# define DANGER_BGM_PATH	ASSET_DIR "/tetris_danger.mp3"
# define HOME_BGM_START_VOLUME	48
# define SOLO_BACKGROUND_PATH \
	ASSET_DIR "/default_theme/default_background_4x3.png"
# define DEFAULT_HUD_PATH	ASSET_DIR "/default_theme/default_board.png"
# define DEFAULT_TILE_PATH	ASSET_DIR "/default_theme/default_tile.png"
# define DEFAULT_MIRURUN_PATH	ASSET_DIR "/default_theme/default_mirurun.png"
# define VOLUME_NOTIFICATION_PATH \
	ASSET_DIR "/default_theme/volume_notification.png"
# define ABILITY_POPOVER_PATH \
	ASSET_DIR "/default_theme/ability_popover.png"
# define SHARED_FONT_MASK_PATH	ASSET_DIR "/shared_font_mask.png"
# define SHARED_NUMBERS_MASK_PATH	ASSET_DIR "/shared_numbers_mask.png"
# define MENU_MOVE_SFX_PATH \
	ASSET_DIR "/General Sounds/se_sys_cursor2.wav"
# define MENU_SELECT_SFX_PATH \
	ASSET_DIR "/General Sounds/se_sys_dialog.wav"
# define GENERAL_SFX_DIR	ASSET_DIR "/General Sounds"
# define MENU_ITEM_COUNT	5
# define APP_TEXT_MAX	64
# define AUTH_FIELD_MAX	128
# define AUTH_STATUS_MAX	96
# define AUTH_PASSWORD_MIN	4
# define APP_CATALOGUE_MAX_ITEMS	8
# define APP_LEADERBOARD_MAX_ENTRIES	10
# define APP_LOBBY_MAX_ROOMS	8
# define APP_ROOM_MAX_PLAYERS	8
# define SOLO_NEXT_COUNT	3
# define SOLO_CRYSTAL_CAPACITY	10
# define SOLO_CRYSTAL_LINES_PER_CHARGE	2
# define SOLO_ABILITY_COUNT	4
# define SOLO_ABILITY_FEEDBACK_MS	1000
# define SOLO_POPOVER_FADE_IN_MS	120
# define SOLO_POPOVER_FADE_OUT_MS	180
# define SOLO_POPOVER_ROWS	5
# define SOLO_POPOVER_COLS	38
# define SOLO_POPOVER_ART_COLS	9
# define SOLO_DANGER_ENTER_ROW	3
# define SOLO_DANGER_EXIT_ROW	5
# define SOLO_DANGER_EXIT_HOLD_MS	1500
# define SOLO_DANGER_FADE_MS	300
# define SOLO_DANGER_DIM_MAX	150
# define SOLO_DANGER_BOARD_PAD	4
# define SOLO_TOP_OUT_REVEAL_MS	350
# define SOLO_PERSONAL_BEST_PULSE_MS	720
# define SOLO_PERSONAL_BEST_FADE_MS	300
# define SOLO_PERSONAL_BEST_FRAME_MS	33
# define SOLO_SCORE_EVENT_PULSE_MS	420
# define SOLO_SCORE_EVENT_FADE_MS	300
# define SOLO_ABILITY_READY_PULSE_MS	540
# define SOLO_ABILITY_READY_FADE_MS	300
# define SOLO_ABILITY_RESULT_FADE_MS	300
# define SOLO_COUNTDOWN_STEP_MS	600
# define SOLO_COUNTDOWN_STEPS	4
# define SOLO_EVENT_ANIMATION_FRAME_MS	33
# define SOLO_LOCK_DELAY_MS	500
# define SOLO_LOCK_RESET_LIMIT	15
# define SOLO_DEFAULT_DAS_MS	167
# define SOLO_DEFAULT_ARR_MS	33
# define SOLO_DEFAULT_SOFT_DROP_FACTOR	20
# define SOLO_HANDLING_ACTION_CAP	64

/* AUDIO.C */
# define AUDIO_DEFAULT_VOLUME	96
# define AUDIO_VOLUME_STEP		8
# define AUDIO_MAX_VOLUME		128
# define AUDIO_MUSIC_TRANSITION_MS	300
# define AUDIO_PATH_MAX			512
# define AUDIO_SFX_QUIET_VOLUME	40
# define AUDIO_SFX_NORMAL_VOLUME	72
# define AUDIO_MENU_MOVE_VOLUME	36
# define AUDIO_MENU_SELECT_VOLUME	48

/* UI_NOTIFICATION.C */
# define UI_NOTIFICATION_STACK_MAX	3
# define UI_NOTIFICATION_TITLE_MAX	15
# define UI_NOTIFICATION_HOLD_MS	900
# define UI_NOTIFICATION_FADE_MS	180
# define UI_NOTIFICATION_FRAME_MS	33
# define UI_NOTIFICATION_BAR_STEPS	16

/* RENDER_BACKGROUND.C */
# define BACKGROUND_SOURCE_PIXELS_Y	1086
# define BACKGROUND_SOURCE_PIXELS_X	1448
# define RENDER_RESIZE_POLL_MS	100
# define COMPATIBILITY_BADGE_TEXT	":: COMPATIBILITY MODE ::"
# define COMPATIBILITY_BADGE_SHORT	":: CELL MODE ::"

/* RENDER_MENU.C */
# define MENU_PANEL_X_RATIO		0.425
# define MENU_PANEL_Y_RATIO		0.700
# define MENU_PANEL_WIDTH_RATIO	0.515
# define MENU_PANEL_HEIGHT_RATIO	0.290
# define MENU_LABEL_LEFT_X_RATIO	0.520
# define MENU_FIRST_Y_RATIO		0.740
# define MENU_STEP_Y_RATIO		0.055
# define BUNNY_LABEL_X_RATIO		0.505
# define BUNNY_TEXT_GAP_COLS		1
# define BUNNY_ROWS_RATIO		0.055
# define BUNNY_SOURCE_PIXELS_Y	160
# define BUNNY_SOURCE_PIXELS_X	150
# define BUNNY_MIN_ROWS			3
# define BUNNY_MAX_ROWS			7
# define COMPAT_SELECTOR_ROWS	1
# define COMPAT_SELECTOR_COLS	3

/* RENDER_SOLO.C */
# define SOLO_CANVAS_WIDTH		512
# define SOLO_CANVAS_HEIGHT		384
# define SOLO_CONTENT_HEIGHT	368
# define SOLO_CONTENT_TILE_ROWS	23
# define SOLO_TERMINAL_CONTROLS_ROWS	1
# define SOLO_MIN_CANVAS_ROWS	24
# define SOLO_MIN_CANVAS_COLS	64
# define HUD_NEXT_X				80
# define HUD_NEXT_Y				4
# define HUD_NEXT_WIDTH			160
# define HUD_NEXT_HEIGHT		36
# define HUD_HOLD_X				24
# define HUD_HOLD_Y				4
# define HUD_HOLD_WIDTH			48
# define HUD_HOLD_HEIGHT		35
# define HOLD_PREVIEW_TILE_SIZE	10
# define HOLD_USED_OPACITY		140
# define HUD_BOARD_X				80
# define HUD_BOARD_Y				48
# define HUD_TILE_SIZE			16
# define HUD_METER_X				40
# define HUD_METER_Y				42
# define HUD_METER_WIDTH			16
# define HUD_METER_HEIGHT		320
# define HUD_MIRURUN_X			272
# define HUD_MIRURUN_Y			33
# define HUD_MIRURUN_SIZE		160
# define HUD_SCORE_X				272
# define HUD_SCORE_Y				203
# define HUD_SCORE_WIDTH			160
# define HUD_SCORE_STAT_RIGHT_INSET	20
# define HUD_SCORE_STAT_FIRST_Y	258
# define HUD_SCORE_STAT_ROW_STEP	22
# define HUD_CONTROLS_X			80
# define HUD_CONTROLS_Y			376
# define HUD_CONTROLS_WIDTH		352
# define HUD_ART_BOARD_LEFT		77
# define HUD_ART_BOARD_TOP		40
# define HUD_ART_BOARD_CONTENT_TOP	43
# define HUD_ART_BOARD_RIGHT		242
# define HUD_ART_BOARD_BOTTOM	365
# define HUD_ART_BOARD_SHIFT_Y	(HUD_BOARD_Y - HUD_ART_BOARD_CONTENT_TOP)
# define HUD_ART_CONTROLS_LEFT	94
# define HUD_ART_CONTROLS_TOP	367
# define HUD_ART_CONTROLS_RIGHT	417
# define HUD_ART_CONTROLS_BOTTOM	383
# define TILE_SOURCE_SIZE		16
# define TILE_SOURCE_STRIDE		18
# define TILE_ATLAS_COUNT		10
# define TILE_GARBAGE			7
# define TILE_CLEAR_FIRST		8
# define TILE_CLEAR_SECOND		9
# define PREVIEW_TILE_SIZE		12
# define FONT_COLUMNS			16
# define FONT_ROWS				6
# define FONT_GLYPH_WIDTH		8
# define FONT_GLYPH_HEIGHT		16
# define FONT_INK_Y				4
# define FONT_INK_HEIGHT			8
# define NUMBER_SLOT_WIDTH		10
# define NUMBER_GLYPH_WIDTH		8
# define NUMBER_GLYPH_HEIGHT		16
# define NUMBER_GLYPH_COUNT		14
# define GHOST_OUTLINE_BLEND		96u
# define GHOST_INTERIOR_BLEND	24u
# define SOLO_MAX_CATCHUP_MS		1000
# define SOLO_RENDER_INTERVAL_MS	33
# define SOLO_INPUT_BATCH_MAX		64
/* The dynamic top strip spans one HOLD box and the three-piece NEXT box. */
# define SOLO_NEXT_X				16
# define SOLO_NEXT_Y				0
# define SOLO_NEXT_WIDTH			240
# define SOLO_NEXT_HEIGHT		48
# define SOLO_METER_X			32
# define SOLO_METER_Y			32
# define SOLO_METER_WIDTH		48
# define SOLO_METER_HEIGHT		336
# define SOLO_MIRURUN_X			272
# define SOLO_MIRURUN_Y			32
# define SOLO_MIRURUN_WIDTH		160
# define SOLO_MIRURUN_HEIGHT	160
# define SOLO_SCORE_HEADER_Y		192
# define SOLO_SCORE_HEADER_HEIGHT	32
# define SOLO_SCORE_VALUE_Y		224
# define SOLO_SCORE_VALUE_HEIGHT	32
# define SOLO_SCORE_STATS_Y		256
# define SOLO_SCORE_STATS_HEIGHT	64
# define SOLO_SCORE_EVENT_Y		320
# define SOLO_SCORE_EVENT_HEIGHT	48
# define SOLO_CONTROLS_X			80
# define SOLO_CONTROLS_WIDTH		352
# define SOLO_BOARD_WIDTH		160
# define SOLO_BOARD_HEIGHT		320
# define SOLO_MAX_ROW_RUNS		((BOARD_WIDTH + 1) / 2)
# define SOLO_ABILITY_CIRCLE_RADIUS	12
# define SOLO_ABILITY_HITBOX_HALF_WIDTH	20
# define SOLO_ABILITY_HITBOX_HALF_HEIGHT	20

typedef enum e_app_screen
{
	APP_SCREEN_ENTRY,
	APP_SCREEN_LOGIN,
	APP_SCREEN_SIGN_UP,
	APP_SCREEN_HOME,
	APP_SCREEN_SOLO,
	APP_SCREEN_MARKETPLACE,
	APP_SCREEN_SETTINGS,
	APP_SCREEN_LEADERBOARD,
	APP_SCREEN_LOBBY,
	APP_SCREEN_CREATE_ROOM_MODAL,
	APP_SCREEN_WAITING_ROOM,
	APP_SCREEN_DOUBLE,
	APP_SCREEN_BATTLE_ROYALE,
	APP_SCREEN_QUIT,
	APP_SCREEN_COUNT
}	app_screen_t;

typedef enum e_app_nav_action
{
	APP_NAV_NONE,
	APP_NAV_OPEN_LOGIN,
	APP_NAV_OPEN_SIGN_UP,
	APP_NAV_PLAY_OFFLINE,
	APP_NAV_AUTHENTICATED,
	APP_NAV_OPEN_SOLO,
	APP_NAV_OPEN_MARKETPLACE,
	APP_NAV_OPEN_SETTINGS,
	APP_NAV_OPEN_LEADERBOARD,
	APP_NAV_OPEN_LOBBY,
	APP_NAV_OPEN_CREATE_ROOM,
	APP_NAV_OPEN_WAITING_ROOM,
	APP_NAV_START_DOUBLE,
	APP_NAV_START_BATTLE_ROYALE,
	APP_NAV_BACK,
	APP_NAV_QUIT
}	app_nav_action_t;

typedef enum e_app_data_status
{
	APP_DATA_IDLE,
	APP_DATA_LOADING,
	APP_DATA_READY,
	APP_DATA_EMPTY,
	APP_DATA_UNAVAILABLE,
	APP_DATA_ERROR
}	app_data_status_t;

typedef enum e_app_provider_result
{
	APP_PROVIDER_OK,
	APP_PROVIDER_EMPTY,
	APP_PROVIDER_UNAVAILABLE,
	APP_PROVIDER_INVALID,
	APP_PROVIDER_ERROR
}	app_provider_result_t;

typedef enum e_app_catalogue_kind
{
	APP_CATALOGUE_CHARACTERS,
	APP_CATALOGUE_THEMES
}	app_catalogue_kind_t;

typedef enum e_app_game_mode
{
	APP_GAME_MODE_NONE,
	APP_GAME_MODE_DOUBLE,
	APP_GAME_MODE_BATTLE_ROYALE
}	app_game_mode_t;

typedef struct s_app_navigation
{
	app_screen_t	current;
	app_screen_t	previous;
	bool			offline;
}	app_navigation_t;

typedef struct s_app_auth_view_model
{
	bool	signed_in;
	char	username[APP_TEXT_MAX];
	char	message[APP_TEXT_MAX];
}	app_auth_view_model_t;

typedef struct s_app_profile_view_model
{
	bool		signed_in;
	char		username[APP_TEXT_MAX];
	char		character[APP_TEXT_MAX];
	char		theme[APP_TEXT_MAX];
	uint64_t	score;
	int			wallet_points;
	int			rank;
}	app_profile_view_model_t;

typedef struct s_app_catalogue_item_view_model
{
	char	id[APP_TEXT_MAX];
	char	name[APP_TEXT_MAX];
	int		price;
	bool	owned;
	bool	equipped;
}	app_catalogue_item_view_model_t;

typedef struct s_app_catalogue_view_model
{
	app_catalogue_kind_t			kind;
	int							count;
	app_catalogue_item_view_model_t	items[APP_CATALOGUE_MAX_ITEMS];
}	app_catalogue_view_model_t;

typedef struct s_app_leaderboard_entry_view_model
{
	int			position;
	char		username[APP_TEXT_MAX];
	uint64_t	score;
}	app_leaderboard_entry_view_model_t;

typedef struct s_app_leaderboard_view_model
{
	int								count;
	app_leaderboard_entry_view_model_t	entries[
		APP_LEADERBOARD_MAX_ENTRIES];
}	app_leaderboard_view_model_t;

typedef struct s_app_room_summary_view_model
{
	char			id[APP_TEXT_MAX];
	char			owner[APP_TEXT_MAX];
	app_game_mode_t	mode;
	int				players;
	int				capacity;
}	app_room_summary_view_model_t;

typedef struct s_app_lobby_view_model
{
	int							count;
	app_room_summary_view_model_t	rooms[APP_LOBBY_MAX_ROOMS];
}	app_lobby_view_model_t;

typedef struct s_app_room_player_view_model
{
	char	username[APP_TEXT_MAX];
	bool	owner;
	bool	ready;
}	app_room_player_view_model_t;

typedef struct s_app_room_view_model
{
	char						id[APP_TEXT_MAX];
	app_game_mode_t				mode;
	int							required_players;
	int							player_count;
	app_room_player_view_model_t	players[APP_ROOM_MAX_PLAYERS];
}	app_room_view_model_t;

typedef struct s_app_match_view_model
{
	char			room_id[APP_TEXT_MAX];
	app_game_mode_t	mode;
	int				player_count;
	char			status[APP_TEXT_MAX];
}	app_match_view_model_t;

typedef union u_app_screen_data
{
	app_auth_view_model_t			auth;
	app_profile_view_model_t		profile;
	app_catalogue_view_model_t		catalogue;
	app_leaderboard_view_model_t	leaderboard;
	app_lobby_view_model_t			lobby;
	app_room_view_model_t			room;
	app_match_view_model_t			match;
}	app_screen_data_t;

typedef struct s_app_screen_view_model
{
	app_screen_t		screen;
	app_data_status_t	status;
	bool				local_preview;
	char				title[APP_TEXT_MAX];
	char				subtitle[APP_TEXT_MAX];
	app_screen_data_t	data;
}	app_screen_view_model_t;

typedef struct s_app_data_provider
{
	const char	*name;
	bool		local_fixtures;
	void		*userdata;
	app_provider_result_t	(*login)(void *userdata, const char *username,
			const char *password, const char *domain,
			app_auth_view_model_t *view);
	app_provider_result_t	(*sign_up)(void *userdata, const char *username,
			const char *password, const char *domain,
			app_auth_view_model_t *view);
	app_provider_result_t	(*load_profile)(void *userdata,
			app_profile_view_model_t *view);
	app_provider_result_t	(*load_catalogue)(void *userdata,
			app_catalogue_kind_t kind, app_catalogue_view_model_t *view);
	app_provider_result_t	(*load_leaderboard)(void *userdata,
			app_leaderboard_view_model_t *view);
	app_provider_result_t	(*load_lobby)(void *userdata,
			app_lobby_view_model_t *view);
	app_provider_result_t	(*load_room)(void *userdata, const char *room_id,
			app_room_view_model_t *view);
}	app_data_provider_t;

typedef enum e_tetrisu_renderer_mode
{
	TETRISU_RENDERER_AUTO,
	TETRISU_RENDERER_CELL,
	TETRISU_RENDERER_STATIONARY,
	TETRISU_RENDERER_PIXEL
}	tetrisu_renderer_mode_t;

// How far the terminal can be trusted with bitmap graphics. NONE is the
// terminal-cell compatibility renderer; STATIONARY draws bitmaps but never
// moves or overlaps them; MOVABLE allows freely moved and restacked sprixels.
typedef enum e_tetrisu_pixel_policy
{
	TETRISU_PIXELS_NONE,
	TETRISU_PIXELS_STATIONARY,
	TETRISU_PIXELS_MOVABLE
}	tetrisu_pixel_policy_t;

typedef struct
{
	int	selected;
}	menu_selection_t;

typedef enum e_auth_form_mode
{
	AUTH_FORM_LOGIN,
	AUTH_FORM_SIGN_UP
}	auth_form_mode_t;

typedef enum e_auth_focus
{
	AUTH_FOCUS_USERNAME,
	AUTH_FOCUS_PASSWORD,
	AUTH_FOCUS_CONFIRM,
	AUTH_FOCUS_DOMAIN,
	AUTH_FOCUS_PRIMARY,
	AUTH_FOCUS_SECONDARY,
	AUTH_FOCUS_OFFLINE
}	auth_focus_t;

typedef enum e_auth_feedback
{
	AUTH_FEEDBACK_IDLE,
	AUTH_FEEDBACK_LOADING,
	AUTH_FEEDBACK_SUCCESS,
	AUTH_FEEDBACK_ERROR
}	auth_feedback_t;

typedef enum e_auth_server_state
{
	AUTH_SERVER_UNVERIFIED,
	AUTH_SERVER_CHECKING,
	AUTH_SERVER_ONLINE,
	AUTH_SERVER_OFFLINE
}	auth_server_state_t;

typedef enum e_auth_action
{
	AUTH_ACTION_NONE,
	AUTH_ACTION_CHECK_SERVER,
	AUTH_ACTION_SUBMIT_LOGIN,
	AUTH_ACTION_SUBMIT_SIGN_UP,
	AUTH_ACTION_OPEN_LOGIN,
	AUTH_ACTION_OPEN_SIGN_UP,
	AUTH_ACTION_PLAY_OFFLINE,
	AUTH_ACTION_QUIT
}	auth_action_t;

typedef struct s_auth_form
{
	auth_form_mode_t	mode;
	auth_focus_t		focus;
	auth_feedback_t	feedback;
	auth_server_state_t	server_state;
	char				username[AUTH_FIELD_MAX];
	char				password[AUTH_FIELD_MAX];
	char				confirm[AUTH_FIELD_MAX];
	char				domain[AUTH_FIELD_MAX];
	char				status[AUTH_STATUS_MAX];
}	auth_form_t;

typedef enum e_audio_sfx
{
	AUDIO_SFX_MOVE,
	AUDIO_SFX_ROTATE,
	AUDIO_SFX_SOFT_DROP,
	AUDIO_SFX_HARD_DROP,
	AUDIO_SFX_LANDING,
	AUDIO_SFX_HOLD,
	AUDIO_SFX_SINGLE,
	AUDIO_SFX_DOUBLE,
	AUDIO_SFX_TRIPLE,
	AUDIO_SFX_TETRIS,
	AUDIO_SFX_PERFECT_CLEAR,
	AUDIO_SFX_ABILITY_READY,
	AUDIO_SFX_ABILITY_ACTIVATED,
	AUDIO_SFX_ABILITY_REJECTED,
	AUDIO_SFX_COUNTDOWN_TICK,
	AUDIO_SFX_COUNTDOWN_GO,
	AUDIO_SFX_PAUSE,
	AUDIO_SFX_LEVEL_UP,
	AUDIO_SFX_PERSONAL_BEST,
	AUDIO_SFX_COUNT
}	audio_sfx_t;

typedef struct
{
	int		enabled;
	int		music_volume;
	int		music_transition_phase;
	int		music_transition_elapsed_ms;
	int		music_transition_duration_ms;
	void	*music;
	void	*menu_move_sfx;
	void	*menu_select_sfx;
	void	*game_sfx[AUDIO_SFX_COUNT];
	char	music_path[AUDIO_PATH_MAX];
	char	pending_music_path[AUDIO_PATH_MAX];
}	audio_ctx_t;

typedef struct s_ui_notification
{
	char		title[UI_NOTIFICATION_TITLE_MAX + 1];
	int			percent;
	uint64_t	shown_at_ms;
}	ui_notification_t;

typedef struct s_ui_notification_stack
{
	ui_notification_t	items[UI_NOTIFICATION_STACK_MAX];
	int					count;
}	ui_notification_stack_t;

// Bundles every notcurses handle the render layer needs across calls. The
// background geometry records the rendered image size, so menu overlays can
// follow the art even when notcurses scales it to different terminals.
typedef struct
{
	struct notcurses	*nc;
	struct ncplane		*std;
	struct ncplane		*bg_plane;
	struct ncplane		*menu_plane;
	struct ncplane		*menu_labels_plane;
	struct ncplane		*auth_labels_plane;
	struct ncplane		*screen_plane;
	struct ncplane		*bunny_plane;
	struct ncvisual		*bunny_visual;
	struct ncplane		*compatibility_plane;
	struct ncplane		*notification_art_planes[UI_NOTIFICATION_STACK_MAX];
	struct ncplane		*notification_planes[UI_NOTIFICATION_STACK_MAX];
	ui_notification_stack_t	notifications;
	int					bg_row;
	int					bg_col;
	int					bg_rows;
	int					bg_cols;
	int					cell_px_y;
	int					cell_px_x;
	int					menu_row;
	int					menu_col;
	int					bunny_rows;
	int					bunny_cols;
	uint64_t			auth_labels_signature;
	tetrisu_pixel_policy_t	pixels;
}	render_ctx_t;

typedef struct s_intro_stream
{
	render_ctx_t	*ctx;
	int				skipped;
}	intro_stream_t;

typedef struct s_pixel_asset
{
	uint32_t	*pixels;
	int			width;
	int			height;
}	pixel_asset_t;

typedef struct s_color
{
	unsigned	r;
	unsigned	g;
	unsigned	b;
}	color_t;

typedef struct s_piece_geometry
{
	int	cols[4];
	int	rows[4];
	int	min_col;
	int	max_col;
	int	min_row;
	int	max_row;
}	piece_geometry_t;

typedef struct s_piece_bounds
{
	int	min_col;
	int	max_col;
	int	min_row;
	int	max_row;
}	piece_bounds_t;

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
	SOLO_HARD_DROP,
	SOLO_HOLD
}	solo_action_t;

typedef enum e_solo_event
{
	SOLO_EVENT_MOVE = 1u << 0,
	SOLO_EVENT_ROTATE = 1u << 1,
	SOLO_EVENT_SOFT_DROP = 1u << 2,
	SOLO_EVENT_HARD_DROP = 1u << 3,
	SOLO_EVENT_LOCK = 1u << 4,
	SOLO_EVENT_HOLD = 1u << 5,
	SOLO_EVENT_SINGLE = 1u << 6,
	SOLO_EVENT_DOUBLE = 1u << 7,
	SOLO_EVENT_TRIPLE = 1u << 8,
	SOLO_EVENT_TETRIS = 1u << 9,
	SOLO_EVENT_PERFECT_CLEAR = 1u << 10,
	SOLO_EVENT_ABILITY_READY = 1u << 11,
	SOLO_EVENT_ABILITY_ACTIVATED = 1u << 12,
	SOLO_EVENT_ABILITY_REJECTED = 1u << 13,
	SOLO_EVENT_PAUSE = 1u << 14,
	SOLO_EVENT_LEVEL_UP = 1u << 15,
	SOLO_EVENT_PERSONAL_BEST = 1u << 16,
	SOLO_EVENT_COUNTDOWN_TICK = 1u << 17,
	SOLO_EVENT_COUNTDOWN_GO = 1u << 18
}	solo_event_t;

typedef struct s_solo_handling_config
{
	int	das_ms;
	int	arr_ms;
	int	soft_drop_factor;
}	solo_handling_config_t;

typedef struct s_solo_handling_state
{
	uint64_t	sequence;
	uint64_t	left_order;
	uint64_t	right_order;
	int			horizontal_direction;
	int			horizontal_wait_ms;
	int			soft_drop_wait_ms;
	bool		left_held;
	bool		right_held;
	bool		down_held;
}	solo_handling_state_t;

typedef enum e_solo_ability
{
	SOLO_ABILITY_NONE,
	SOLO_ABILITY_MIRURUN,
	SOLO_ABILITY_INVERSION,
	SOLO_ABILITY_PENTARIS,
	SOLO_ABILITY_SIRTET
}	solo_ability_t;

typedef enum e_solo_ability_result
{
	SOLO_ABILITY_RESULT_NONE,
	SOLO_ABILITY_RESULT_ACTIVATED,
	SOLO_ABILITY_RESULT_NO_CHARGE,
	SOLO_ABILITY_RESULT_UNAVAILABLE,
	SOLO_ABILITY_RESULT_BLOCKED,
	SOLO_ABILITY_RESULT_INVALID
}	solo_ability_result_t;

typedef enum e_solo_popover_phase
{
	SOLO_POPOVER_HIDDEN,
	SOLO_POPOVER_FADING_IN,
	SOLO_POPOVER_VISIBLE,
	SOLO_POPOVER_FADING_OUT
}	solo_popover_phase_t;

typedef struct s_solo_game
{
	t_board			board;
	t_piece			active;
	t_piece_type	hold;
	t_piece_type	next[SOLO_NEXT_COUNT];
	t_piece_bag	bag;
	t_score_state	scoring;
	t_score_result	last_score;
	uint64_t		personal_best;
	solo_phase_t	phase;
	t_spin_type	pending_spin;
	t_spin_type	last_spin;
	int				clear_rows[BRAIN_MAX_CLEAR_LINES];
	int				clear_count;
	int				last_lines;
	int				total_lines;
	int				level;
	int				crystal_charge;
	int				crystal_line_progress;
	solo_ability_t	last_ability;
	solo_ability_result_t	ability_result;
	int				ability_feedback_elapsed_ms;
	int				gravity_elapsed_ms;
	int				lock_elapsed_ms;
	int				clear_elapsed_ms;
	int				top_out_elapsed_ms;
	int				personal_best_elapsed_ms;
	int				score_event_elapsed_ms;
	int				ability_ready_elapsed_ms;
	int				countdown_elapsed_ms;
	int				danger_safe_elapsed_ms;
	int				danger_fade_elapsed_ms;
	uint32_t		pending_events;
	int				lock_resets;
	int				last_kick_index;
	bool			last_action_was_rotation;
	bool			last_perfect_clear;
	bool			has_hold;
	bool			hold_used;
	bool			paused;
	bool			danger_active;
	bool			personal_best_checked;
	bool			new_personal_best;
	bool			score_event_active;
	bool			ability_ready_active;
	bool			countdown_active;
	solo_ability_t	ready_ability;
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
	struct ncplane	*compatibility_score_plane;
	struct ncplane	*compatibility_overlay_plane;
	struct ncplane	*ability_popover_art_plane;
	struct ncplane	*ability_popover_plane;
	struct ncplane	*controls_plane;
	struct ncplane	*board_overlay_plane;
	struct ncplane	*settled_runs[BOARD_HEIGHT][(BOARD_WIDTH + 1) / 2];
	struct ncplane	*active_plane;
	struct ncplane	*ghost_plane;
	struct ncplane	*status_plane;
	uint32_t		*static_pixels;
	uint32_t		*background_pixels;
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
	int				content_rows;
	int				tile_rows;
	int				tile_cols;
	uint64_t		row_signatures[BOARD_HEIGHT];
	uint64_t		next_signature;
	uint64_t		meter_signature;
	uint64_t		score_value_signature;
	uint64_t		score_stats_signature;
	uint64_t		score_event_signature;
	uint64_t		popover_signature;
	uint64_t		overlay_signature;
	uint64_t		danger_signature;
	uint64_t		active_shape_signature;
	uint64_t		ghost_shape_signature;
	int				settled_run_counts[BOARD_HEIGHT];
	solo_ability_t	hovered_ability;
	solo_ability_t	popover_ability;
	solo_popover_phase_t	popover_phase;
	int				popover_opacity;
	int				popover_fade_start_opacity;
	int				popover_fade_elapsed_ms;
	bool			popover_feedback_active;
	bool			piece_planes_combined;
	bool			layout_valid;
	bool			assets_ready;
	bool			planes_ready;
	bool			composite_board;
	bool			cell_board;
	bool			board_plane_cells;
	char			asset_error[160];
}	solo_render_t;

/* APP_STATE.C */
void			app_navigation_init(app_navigation_t *navigation,
					app_screen_t initial);
bool			app_navigation_dispatch(app_navigation_t *navigation,
					app_nav_action_t action);
app_screen_t	app_screen_parent(app_screen_t screen);
const char		*app_screen_name(app_screen_t screen);
app_screen_t	app_handle_key(app_screen_t current, uint32_t key);
void			menu_move_selection(menu_selection_t *m, uint32_t key);
const char		*menu_item_label(int index);
const char		*menu_stub_text(int selected_index);

/* APP_PROVIDER.C */
void			app_fixture_provider_init(app_data_provider_t *provider);
app_provider_result_t	app_screen_view_load(
					const app_data_provider_t *provider,
					app_screen_t screen, app_screen_view_model_t *view);
const char		*app_data_status_name(app_data_status_t status);
const char		*app_game_mode_name(app_game_mode_t mode);

/* AUTH_FORM.C */
void			auth_form_init(auth_form_t *form, auth_form_mode_t mode);
void			auth_form_set_mode(auth_form_t *form, auth_form_mode_t mode);
void			auth_form_focus_next(auth_form_t *form);
void			auth_form_focus_previous(auth_form_t *form);
auth_action_t	auth_form_handle_key(auth_form_t *form, uint32_t key);
bool			auth_form_begin_server_check(auth_form_t *form);
void			auth_form_finish_server_check(auth_form_t *form, bool online);
bool			auth_form_online_enabled(const auth_form_t *form);
bool			auth_form_validate(auth_form_t *form);
app_provider_result_t	auth_form_submit(auth_form_t *form,
					const app_data_provider_t *provider,
					app_auth_view_model_t *view);
bool			auth_form_mask_password(const char *password, char *masked,
					size_t size);

/* UI_NOTIFICATION.C */
void			ui_notification_stack_init(ui_notification_stack_t *stack);
void			ui_notification_show(ui_notification_stack_t *stack,
					const char *title, int percent, uint64_t now_ms);
bool			ui_notification_update(ui_notification_stack_t *stack,
					uint64_t now_ms);
int				ui_notification_opacity(const ui_notification_t *notification,
					uint64_t now_ms);
int				ui_notification_next_wake_ms(
					const ui_notification_stack_t *stack, uint64_t now_ms);
int				ui_notification_volume_percent(int volume);
uint64_t		ui_notification_now_ms(void);

/* RENDER_BACKGROUND.C */
render_ctx_t	render_init(const char *image_path);
uint32_t		render_wait_key(render_ctx_t *ctx);
uint32_t		render_wait_input(render_ctx_t *ctx, ncinput *input);
void			render_teardown(render_ctx_t *ctx);
int				render_background_replace(render_ctx_t *ctx,
					const char *image_path, bool stretch);
void				render_background_destroy(render_ctx_t *ctx);
int				render_geometry_refresh(render_ctx_t *ctx, bool repaint);
bool				render_terminal_geometry_changed(const render_ctx_t *ctx);
bool				render_pixel_planes_reliable(const render_ctx_t *ctx);
bool				render_pixels_available(const render_ctx_t *ctx);
bool				render_compatibility_mode(const render_ctx_t *ctx);
void				render_compatibility_badge_refresh(render_ctx_t *ctx);
void				render_compatibility_badge_hide(render_ctx_t *ctx);
void				render_notification_show_volume(render_ctx_t *ctx,
					int volume);
void				render_notification_tick(render_ctx_t *ctx);
int					render_notification_next_wake_ms(
					const render_ctx_t *ctx);
void				render_notification_reflow(render_ctx_t *ctx);
void				render_notification_raise(render_ctx_t *ctx);
void				render_notification_destroy(render_ctx_t *ctx);
tetrisu_pixel_policy_t	tetrisu_pixel_policy_for(ncpixelimpl_e backend,
					const char *term, tetrisu_renderer_mode_t forced);

/* RENDERER_POLICY.C */
tetrisu_renderer_mode_t	tetrisu_renderer_mode_from_value(const char *value);
tetrisu_renderer_mode_t	tetrisu_renderer_mode_requested(void);

/* SOLO_LAYOUT.C */
int				solo_layout_content_rows(int tile_rows);
int				solo_layout_canvas_rows(int tile_rows);
bool				solo_layout_terminal_fits(int terminal_rows,
					int terminal_cols, int tile_rows, int tile_cols);

/* RENDER_MENU.C */
void			render_menu_create(render_ctx_t *ctx);
void			render_menu_move_bunny(render_ctx_t *ctx, const menu_selection_t *m);
bool			render_menu_hit_test(const render_ctx_t *ctx,
					const ncinput *input, int *selected);
void			render_menu_show_message(render_ctx_t *ctx, const char *msg);
void			render_menu_destroy(render_ctx_t *ctx);
struct ncplane	*render_menu_labels_create(render_ctx_t *ctx);
int				render_menu_label_y(const render_ctx_t *ctx, int index);

/* RENDER_SCREEN.C */
bool			render_screen_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view);
void			render_screen_destroy(render_ctx_t *ctx);

/* RENDER_AUTH.C */
bool			render_auth_show(render_ctx_t *ctx, const auth_form_t *form,
					bool rebuild_background);
bool			render_auth_hit_test(const render_ctx_t *ctx,
					const auth_form_t *form, const ncinput *input,
					auth_focus_t *focus);
void			render_auth_destroy(render_ctx_t *ctx);
bool			render_auth_pixel_labels_refresh(render_ctx_t *ctx,
					const auth_form_t *form, bool force);
void			render_auth_pixel_labels_destroy(render_ctx_t *ctx);

/* RENDER_INTRO.C */
int				render_intro_play(render_ctx_t *ctx, audio_ctx_t *audio,
					const char *video_path, const char *audio_path);

/* AUDIO.C */
int				audio_init(audio_ctx_t *audio);
void			audio_play_music(audio_ctx_t *audio, const char *path);
void			audio_transition_music(audio_ctx_t *audio, const char *path,
					int duration_ms);
bool			audio_update(audio_ctx_t *audio, int elapsed_ms);
int				audio_next_wake_ms(const audio_ctx_t *audio);
void			audio_play_once(audio_ctx_t *audio, const char *path);
void			audio_stop_music(audio_ctx_t *audio);
void			audio_load_menu_sfx(audio_ctx_t *audio, const char *move_path,
					const char *select_path);
void			audio_load_game_sfx(audio_ctx_t *audio);
void			audio_play_sfx(audio_ctx_t *audio, audio_sfx_t sfx);
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
bool			solo_game_update_danger(solo_game_t *game, int elapsed_ms);
unsigned		solo_game_danger_dim(const solo_game_t *game);
uint32_t		solo_game_take_events(solo_game_t *game);
void			solo_game_set_personal_best(solo_game_t *game,
					uint64_t score);
bool			solo_game_finish_personal_best(solo_game_t *game);
unsigned		solo_game_personal_best_opacity(const solo_game_t *game);
void			solo_game_start_countdown(solo_game_t *game);
int				solo_game_countdown_value(const solo_game_t *game);
unsigned		solo_game_countdown_opacity(const solo_game_t *game);
unsigned		solo_game_score_event_opacity(const solo_game_t *game);
unsigned		solo_game_ability_ready_opacity(const solo_game_t *game);
unsigned		solo_game_ability_result_opacity(const solo_game_t *game);
int				solo_game_next_wake_ms(const solo_game_t *game);
int				solo_clear_duration_ms(int level);
t_piece			solo_game_ghost(const solo_game_t *game);
bool			solo_game_row_is_clearing(const solo_game_t *game, int row);
void			solo_game_toggle_pause(solo_game_t *game);

/* SOLO_PERSISTENCE.C */
uint64_t		solo_best_load(void);
bool			solo_best_store(uint64_t score);

/* SOLO_HANDLING.C — terminal-aware DAS, ARR, and soft-drop timing */
solo_handling_config_t	solo_handling_default_config(void);
void			solo_handling_reset(solo_handling_state_t *state);
bool			solo_handling_event(solo_handling_state_t *state,
					const solo_handling_config_t *config, uint32_t key,
					ncintype_e event_type, solo_action_t *action);
int				solo_handling_update(solo_handling_state_t *state,
					const solo_handling_config_t *config, int gravity_ms,
					int elapsed_ms, solo_action_t *actions, int capacity);
int				solo_handling_next_wake_ms(
					const solo_handling_state_t *state,
					const solo_handling_config_t *config, int gravity_ms);

/* SOLO_ABILITIES.C — temporary local ability authority for Solo testing */
int				solo_ability_cost(solo_ability_t ability);
const char		*solo_ability_name(solo_ability_t ability);
const char		*solo_ability_description(solo_ability_t ability);
int				solo_ability_center_y(solo_ability_t ability);
solo_ability_t	solo_ability_at_canvas(int x, int y);
bool				solo_mouse_canvas_position(const render_ctx_t *ctx,
					const solo_render_t *solo, const ncinput *input,
					int *canvas_x, int *canvas_y);
solo_ability_result_t	solo_game_activate_ability(solo_game_t *game,
					solo_ability_t ability);

/* SOLO_POPOVER.C — timed hover presentation shared by every renderer */
bool			solo_popover_set_hover(solo_render_t *solo,
					solo_ability_t ability);
bool			solo_popover_update(solo_render_t *solo,
					const solo_game_t *game, int elapsed_ms);
int				solo_popover_next_wake_ms(const solo_render_t *solo);
solo_ability_t	solo_popover_displayed_ability(const solo_render_t *solo,
					const solo_game_t *game);
int				solo_popover_displayed_opacity(const solo_render_t *solo,
					const solo_game_t *game);

/* RENDER_SOLO_CANVAS.C */
bool			solo_canvas_load(solo_render_t *solo);
int				solo_canvas_piece_tile(t_piece_type type);
uint32_t		solo_canvas_ghost_tile_pixel(uint32_t pixel, int x, int y);
bool			solo_canvas_buffer_bytes(int width, int height, size_t *bytes);
void			solo_canvas_set_error(solo_render_t *solo, const char *message,
					const char *path);
void			solo_canvas_compose_hud(solo_render_t *solo,
					const solo_game_t *game);
void			solo_canvas_compose_board(solo_render_t *solo,
					const solo_game_t *game);
void			solo_canvas_compose_danger_background(solo_render_t *solo,
					const solo_game_t *game);

/* RENDER_SOLO.C */
void			render_solo_create(render_ctx_t *ctx, solo_render_t *solo);
void			render_solo_draw(render_ctx_t *ctx, solo_render_t *solo,
					const solo_game_t *game);
void			render_solo_resize(render_ctx_t *ctx, solo_render_t *solo);
void			render_solo_destroy(solo_render_t *solo);

/* SOLO_MODE.C */
int				solo_mode_run(render_ctx_t *ctx, audio_ctx_t *audio);

# endif
