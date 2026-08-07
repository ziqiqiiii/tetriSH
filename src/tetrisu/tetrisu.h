# ifndef TETRISU_H
# define TETRISU_H

# include <assert.h>
# include <ctype.h>
# include <errno.h>
# include <inttypes.h>
# include <limits.h>
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
# define LEADERBOARD_BACKGROUND_PATH \
	ASSET_DIR "/default_theme/leaderboard_background.png"
# define SETTINGS_BACKGROUND_PATH \
	ASSET_DIR "/default_theme/settings_profile_background_v2.png"
/*
 * The Marketplace backdrop is a scene rather than an authored frame: the
 * renderer draws every plate and border itself, so a missing file degrades to
 * the Settings artwork instead of failing the screen.
 */
# define MARKETPLACE_BACKGROUND_PATH \
	ASSET_DIR "/default_theme/marketplace_background.png"
# define AUTH_BACKGROUND_PATH \
	ASSET_DIR "/default_theme/auth_screen.png"
# define AUTH_LOGIN_BACKGROUND_PATH \
	ASSET_DIR "/default_theme/auth_login.png"
# define AUTH_SIGNUP_BACKGROUND_PATH \
	ASSET_DIR "/default_theme/auth_signup.png"
# define AUTH_FONT_ATLAS_PATH \
	ASSET_DIR "/default_theme/auth_font_atlas.png"
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
# define HALLOWEEN_PORTRAIT_PATH \
	ASSET_DIR "/default_theme/character_halloween.png"
# define PRINCESS_PORTRAIT_PATH \
	ASSET_DIR "/default_theme/character_princess.png"
# define WOLFMAN_PORTRAIT_PATH \
	ASSET_DIR "/default_theme/character_wolfman.png"
# define SETTINGS_THEME_CLASSIC_PREVIEW_PATH \
	ASSET_DIR "/settings_previews/theme_classic.png"
# define SETTINGS_THEME_DESIGN_AI_UNIVERSITY_PREVIEW_PATH \
	ASSET_DIR "/settings_previews/theme_design_ai_university.png"
# define SETTINGS_THEME_SNOWMAN_PREVIEW_PATH \
	ASSET_DIR "/settings_previews/theme_snowman.png"
# define SETTINGS_THEME_HAALAND_PREVIEW_PATH \
	ASSET_DIR "/settings_previews/theme_haaland.png"
# define SETTINGS_THEME_AL_MERQAEDES_PREVIEW_PATH \
	ASSET_DIR "/settings_previews/theme_al_merqaedes.png"
# define SETTINGS_THEME_NUCLEAR_GHANDI_PREVIEW_PATH \
	ASSET_DIR "/settings_previews/theme_nuclear_ghandi.png"
# define SETTINGS_THEME_CLAUDING_PREVIEW_PATH \
	ASSET_DIR "/settings_previews/theme_clauding.png"
# define VOLUME_NOTIFICATION_PATH \
	ASSET_DIR "/default_theme/volume_notification.png"
# define OWNERSHIP_NOTIFICATION_PATH \
	ASSET_DIR "/default_theme/ownership_notification.png"
# define ABILITY_POPOVER_PATH \
	ASSET_DIR "/default_theme/ability_popover.png"
# define MULTIPLAYER_ASSET_PATH \
	ASSET_DIR "/default_theme/multiplayer_background.png"
# define SHARED_FONT_MASK_PATH	ASSET_DIR "/shared_font_mask.png"
# define SHARED_NUMBERS_MASK_PATH	ASSET_DIR "/shared_numbers_mask.png"
# define MENU_MOVE_SFX_PATH \
	ASSET_DIR "/General Sounds/se_sys_cursor2.wav"
# define MENU_SELECT_SFX_PATH \
	ASSET_DIR "/General Sounds/se_sys_dialog.wav"
# define GENERAL_SFX_DIR	ASSET_DIR "/General Sounds"
# define MENU_ITEM_COUNT	5
# define APP_TEXT_MAX	64
# define APP_ASSET_PATH_MAX	512
# define APP_ABILITY_TEXT_MAX	192
# define APP_CHARACTER_ABILITY_COUNT	4
# define AUTH_FIELD_MAX	128
# define AUTH_STATUS_MAX	96
# define AUTH_OVERLAY_PLANE_MAX	16
# define AUTH_PASSWORD_MIN	4
# define APP_CATALOGUE_MAX_ITEMS	8
# define APP_LEADERBOARD_MAX_ENTRIES	10
# define LEADERBOARD_PIXEL_BUTTON_COUNT	2
# define LEADERBOARD_PANEL_MIN_COLS	44
# define LEADERBOARD_PANEL_MIN_ROWS	20
# define APP_LOBBY_MAX_ROOMS	8
# define LOBBY_VISIBLE_ROOMS	6
# define APP_ROOM_MAX_PLAYERS	99
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
# define UI_NOTIFICATION_MESSAGE_MAX	31
# define UI_NOTIFICATION_OWNERSHIP_TITLE	"ITEM NOT OWNED"
# define UI_NOTIFICATION_OWNERSHIP_MESSAGE	"VISIT MARKETPLACE TO BUY"
# define UI_NOTIFICATION_HOLD_MS	900
# define UI_NOTIFICATION_FADE_MS	180
# define UI_NOTIFICATION_FRAME_MS	33
# define UI_NOTIFICATION_BAR_STEPS	16

/* RENDER_BACKGROUND.C */
# define BACKGROUND_SOURCE_PIXELS_Y	1086
# define BACKGROUND_SOURCE_PIXELS_X	1448
# define SETTINGS_REFERENCE_WIDTH	1448
# define SETTINGS_REFERENCE_HEIGHT	1086
# define SETTINGS_BUTTON_COUNT	4
/*
 * The two inventory panels are navigated as row-major grids of this width, so
 * the focus arithmetic in settings_screen.c and the slot arithmetic in
 * draw_inventory() must agree on the same column count.
 */
# define SETTINGS_INVENTORY_COLUMNS	4
# define SETTINGS_CHARACTER_SLOTS	4
# define SETTINGS_THEME_SLOTS	7
# define SETTINGS_REF_PORTRAIT_X	270
# define SETTINGS_REF_PORTRAIT_Y	145
# define SETTINGS_REF_PORTRAIT_WIDTH	240
# define SETTINGS_REF_PORTRAIT_HEIGHT	255
# define SETTINGS_REF_PROFILE_X	594
# define SETTINGS_REF_PROFILE_Y	137
# define SETTINGS_REF_PROFILE_WIDTH	598
# define SETTINGS_REF_PROFILE_HEIGHT	312
# define SETTINGS_REF_CHARACTERS_X	242
# define SETTINGS_REF_CHARACTERS_Y	530
# define SETTINGS_REF_CHARACTERS_WIDTH	432
# define SETTINGS_REF_CHARACTERS_HEIGHT	150
# define SETTINGS_REF_THEMES_X	754
# define SETTINGS_REF_THEMES_Y	530
# define SETTINGS_REF_THEMES_WIDTH	432
# define SETTINGS_REF_THEMES_HEIGHT	150
# define SETTINGS_REF_WALLET_X	230
# define SETTINGS_REF_WALLET_Y	731
# define SETTINGS_REF_WALLET_WIDTH	300
# define SETTINGS_REF_WALLET_HEIGHT	77
# define SETTINGS_REF_SCORE_X	574
# define SETTINGS_REF_SCORE_Y	731
# define SETTINGS_REF_SCORE_WIDTH	300
# define SETTINGS_REF_SCORE_HEIGHT	77
# define SETTINGS_REF_RANK_X	918
# define SETTINGS_REF_RANK_Y	731
# define SETTINGS_REF_RANK_WIDTH	300
# define SETTINGS_REF_RANK_HEIGHT	77
/*
 * The crystal-powers card is inset inside the authored profile frame, so it
 * never paints over the gold border. Four abilities of one name row plus two
 * wrapped description lines fit inside SETTINGS_REF_CARD_HEIGHT at this step.
 */
# define SETTINGS_REF_CARD_X	(SETTINGS_REF_PROFILE_X + 8)
# define SETTINGS_REF_CARD_Y	(SETTINGS_REF_PROFILE_Y + 8)
# define SETTINGS_REF_CARD_WIDTH	(SETTINGS_REF_PROFILE_WIDTH - 16)
# define SETTINGS_REF_CARD_HEIGHT	(SETTINGS_REF_PROFILE_HEIGHT - 16)
# define SETTINGS_REF_CARD_STEP	62
/*
 * Region rectangles for the planes the renderer keeps above the static frame.
 * Bitmap planes must never overlap on a stationary protocol, so these are kept
 * beside each other here and checked for disjointness by the layout tests. The
 * volume readout has two homes and the signed-in one lands inside the powers
 * card, so those two regions are mutually exclusive rather than disjoint.
 */
# define SETTINGS_REF_CONTROLS_X	200
# define SETTINGS_REF_CONTROLS_Y	850
# define SETTINGS_REF_CONTROLS_WIDTH	1080
# define SETTINGS_REF_CONTROLS_HEIGHT	205
# define SETTINGS_REF_VOLUME_X	(SETTINGS_REF_PROFILE_X + 168)
# define SETTINGS_REF_VOLUME_Y	(SETTINGS_REF_PROFILE_Y + 251)
# define SETTINGS_REF_VOLUME_WIDTH	112
# define SETTINGS_REF_VOLUME_HEIGHT	42
# define SETTINGS_REF_VOLUME_OFFLINE_X	(SETTINGS_REF_WALLET_X + 60)
# define SETTINGS_REF_VOLUME_OFFLINE_Y	(SETTINGS_REF_WALLET_Y + 30)
# define SETTINGS_REF_VOLUME_OFFLINE_WIDTH	230
# define SETTINGS_REF_VOLUME_OFFLINE_HEIGHT	42

/* Inventory slot geometry, shared by the bitmap and cell inventory drawing. */
# define SETTINGS_REF_SLOT_INSET	10
# define SETTINGS_REF_CHARACTER_SLOT_FIRST_Y	38
# define SETTINGS_REF_CHARACTER_SLOT_STEP_Y	108
# define SETTINGS_REF_CHARACTER_SLOT_THUMB_WIDTH	74
# define SETTINGS_REF_CHARACTER_SLOT_THUMB_HEIGHT	58
# define SETTINGS_REF_CHARACTER_SLOT_NAME_Y	63
# define SETTINGS_REF_THEME_SLOT_FIRST_Y	38
# define SETTINGS_REF_THEME_SLOT_STEP_Y	48
# define SETTINGS_REF_THEME_SLOT_THUMB_WIDTH	74
# define SETTINGS_REF_THEME_SLOT_THUMB_HEIGHT	30
# define SETTINGS_REF_THEME_SLOT_NAME_Y	32
# define SETTINGS_REF_SLOT_GLYPH	9
# define SETTINGS_REF_INVENTORY_TITLE_Y	3
# define SETTINGS_REF_INVENTORY_TITLE_GLYPH	12
# define SETTINGS_REF_INVENTORY_DETAIL_Y	18
# define SETTINGS_REF_INVENTORY_DETAIL_GLYPH	11
# define SETTINGS_REF_INVENTORY_HINT_GLYPH	9
# define SETTINGS_REF_SLOT_PAD_X	2
# define SETTINGS_REF_SLOT_PAD_Y	3
# define SETTINGS_REF_BUTTON_BACK_X	265
# define SETTINGS_REF_BUTTON_MARKET_X	500
# define SETTINGS_REF_BUTTON_VOLUME_DOWN_X	745
# define SETTINGS_REF_BUTTON_VOLUME_UP_X	985
# define SETTINGS_REF_BUTTON_BACK_CENTER_X	354
# define SETTINGS_REF_BUTTON_MARKET_CENTER_X	596
# define SETTINGS_REF_BUTTON_VOLUME_DOWN_CENTER_X	846
# define SETTINGS_REF_BUTTON_VOLUME_UP_CENTER_X	1082
# define SETTINGS_REF_BUTTON_Y	875
# define SETTINGS_REF_BUTTON_WIDTH	190
# define SETTINGS_REF_BUTTON_HEIGHT	135
# define RENDER_RESIZE_POLL_MS	100
# define COMPATIBILITY_BADGE_TEXT	":: COMPATIBILITY MODE ::"
# define COMPATIBILITY_BADGE_SHORT	":: CELL MODE ::"

/* MARKETPLACE_LAYOUT.C / RENDER_MARKETPLACE_FONT.C */
/*
 * The Marketplace shares the Settings reference contract: a 4:3 backdrop
 * measured in 1448x1086 units, mapped into fitted pixels once per geometry.
 * Its own art carries no authored frames, so every plate and border below is
 * drawn by the renderer and the numbers here are the single source of truth
 * for the bitmap compositor, the hit tests, and the layout tests.
 */
# define MARKETPLACE_REFERENCE_WIDTH	1448
# define MARKETPLACE_REFERENCE_HEIGHT	1086
# define MARKETPLACE_BUTTON_COUNT	4
# define MARKETPLACE_INVENTORY_COLUMNS	4
# define MARKETPLACE_CHARACTER_SLOTS	4
# define MARKETPLACE_THEME_SLOTS	7
/*
 * The backdrop is a shop interior whose shelves, lanterns and pumpkins crowd
 * the left and right edges. Every drawn panel is opaque, so the content band
 * is inset to the quiet dark wall between them - x 262..1186 - and nothing the
 * renderer paints ever cuts through the artwork.
 */
# define MARKETPLACE_REF_CONTENT_X	262
# define MARKETPLACE_REF_CONTENT_WIDTH	924
# define MARKETPLACE_REF_TITLE_Y	22
# define MARKETPLACE_REF_TITLE_HEIGHT	54
/*
 * Region rectangles. Bitmap planes must never overlap on a stationary
 * protocol, and every plane is expanded out to whole cells before it is
 * created, so the bands below are separated by 60 reference units vertically
 * and 96 horizontally. test_region_planes_never_overlap sweeps the supported
 * geometries and asserts that those gaps survive coarse cell sizes.
 *
 * The wallet card is a region rather than part of the static frame because a
 * purchase changes it. Anything a purchase can change has to live above the
 * full-screen plane: rebuilding that plane re-emits a full-screen bitmap, and
 * on a stationary protocol it lands over every region plane and blanks the
 * screen until the next keystroke. With the wallet up here nothing the user
 * can do inside the Marketplace moves the static signature at all.
 *
 * Only the leftmost card is a region, and that is deliberate. Notification
 * cards are planes too, and they are raised into the top right corner, so a
 * region reaching that far along the top row would overlap one - which is the
 * same forbidden overlap, and it was what re-emitted the full-screen bitmap
 * and blanked the screen after a purchase. Best score and rank cannot change
 * inside the Marketplace, so they stay in the static frame and the region
 * stops well left of anything a notification can reach.
 */
# define MARKETPLACE_REF_STATS_X	262
# define MARKETPLACE_REF_STATS_Y	88
# define MARKETPLACE_REF_STATS_HEIGHT	72
# define MARKETPLACE_REF_STAT_WIDTH	300
# define MARKETPLACE_REF_STAT_STEP_X	312
# define MARKETPLACE_REF_CHARACTERS_X	262
# define MARKETPLACE_REF_CHARACTERS_Y	220
# define MARKETPLACE_REF_CHARACTERS_WIDTH	414
# define MARKETPLACE_REF_CHARACTERS_HEIGHT	230
# define MARKETPLACE_REF_THEMES_X	772
# define MARKETPLACE_REF_THEMES_Y	220
# define MARKETPLACE_REF_THEMES_WIDTH	414
# define MARKETPLACE_REF_THEMES_HEIGHT	230
# define MARKETPLACE_REF_DETAIL_X	262
# define MARKETPLACE_REF_DETAIL_Y	510
# define MARKETPLACE_REF_DETAIL_WIDTH	924
# define MARKETPLACE_REF_DETAIL_HEIGHT	300
# define MARKETPLACE_REF_CONTROLS_X	262
# define MARKETPLACE_REF_CONTROLS_Y	870
# define MARKETPLACE_REF_CONTROLS_WIDTH	924
# define MARKETPLACE_REF_CONTROLS_HEIGHT	180
# define MARKETPLACE_REF_BUTTON_FIRST_X	262
# define MARKETPLACE_REF_BUTTON_STEP_X	238
# define MARKETPLACE_REF_BUTTON_Y	884
# define MARKETPLACE_REF_BUTTON_WIDTH	210
# define MARKETPLACE_REF_BUTTON_HEIGHT	92
# define MARKETPLACE_REF_FEEDBACK_Y	984
# define MARKETPLACE_REF_HINT_Y	1016
/*
 * Inventory slot geometry. Each panel is sized so its rows fill it: the
 * characters panel draws one row of four and the themes panel two, so they
 * start at different heights and step differently inside the same box.
 */
# define MARKETPLACE_REF_PANEL_TITLE_Y	8
# define MARKETPLACE_REF_PANEL_TITLE_GLYPH	20
# define MARKETPLACE_REF_SLOT_INSET	10
# define MARKETPLACE_REF_SLOT_PAD_X	3
# define MARKETPLACE_REF_SLOT_PAD_Y	4
# define MARKETPLACE_REF_SLOT_NAME_GLYPH	10
# define MARKETPLACE_REF_SLOT_PRICE_GLYPH	10
# define MARKETPLACE_REF_CHARACTER_FIRST_Y	48
# define MARKETPLACE_REF_CHARACTER_STEP_Y	170
# define MARKETPLACE_REF_CHARACTER_THUMB_INSET	8
# define MARKETPLACE_REF_CHARACTER_THUMB_HEIGHT	118
# define MARKETPLACE_REF_CHARACTER_NAME_Y	126
# define MARKETPLACE_REF_CHARACTER_PRICE_Y	150
# define MARKETPLACE_REF_THEME_FIRST_Y	42
# define MARKETPLACE_REF_THEME_STEP_Y	86
# define MARKETPLACE_REF_THEME_THUMB_INSET	10
# define MARKETPLACE_REF_THEME_THUMB_HEIGHT	46
# define MARKETPLACE_REF_THEME_NAME_Y	52
# define MARKETPLACE_REF_THEME_PRICE_Y	72
/*
 * Detail card interior, measured from the card origin. A character is sold on
 * four powers, so its preview stays narrow and leaves the width to the text; a
 * theme has three lines to say and the artwork is the product, so its preview
 * takes the space the powers would have used.
 */
# define MARKETPLACE_REF_DETAIL_PREVIEW_X	18
# define MARKETPLACE_REF_DETAIL_PREVIEW_Y	26
# define MARKETPLACE_REF_DETAIL_PREVIEW_HEIGHT	244
# define MARKETPLACE_REF_DETAIL_PREVIEW_CHARACTER_W	226
# define MARKETPLACE_REF_DETAIL_PREVIEW_THEME_W	300
# define MARKETPLACE_REF_DETAIL_TEXT_CHARACTER_X	262
# define MARKETPLACE_REF_DETAIL_TEXT_THEME_X	336
# define MARKETPLACE_REF_DETAIL_TEXT_INSET	20
# define MARKETPLACE_REF_DETAIL_NAME_Y	18
# define MARKETPLACE_REF_DETAIL_STATUS_Y	58
/*
 * Four powers, each a name row plus two wrapped description lines, fit inside
 * MARKETPLACE_REF_DETAIL_HEIGHT at this step, and the last line ends at 262 of
 * the card's 270, clear of its three-pixel border.
 *
 * The row spacing is not the glyph height. The shared atlas draws its whole
 * ink band, so a glyph reaches from a quarter of its size above the baseline
 * to about 1.4 times its size below it - descenders on g, j, p, q, y and the
 * comma live down there. Spacing rows by glyph height alone let a wrapped
 * description overlap the next power's name; every step below clears the
 * previous row's descenders instead.
 */
# define MARKETPLACE_REF_DETAIL_BODY_Y	82
# define MARKETPLACE_REF_DETAIL_STEP_Y	46
# define MARKETPLACE_REF_DETAIL_NAME_GLYPH	11
# define MARKETPLACE_REF_DETAIL_DESC_Y	18
# define MARKETPLACE_REF_DETAIL_DESC_GLYPH	8
# define MARKETPLACE_REF_DETAIL_DESC_STEP	13

/* MULTIPLAYER_LAYOUT.C / RENDER_MP_*.C */
/*
 * The four multiplayer surfaces - mode select, lobby, create-room and waiting
 * room - share the Settings reference contract: a 4:3 backdrop measured in
 * 1448x1086 units and mapped into fitted pixels once per geometry. Every
 * rectangle below is the single source of truth for the bitmap compositors and
 * for the disjointness sweeps in tests/test_multiplayer_layout.c.
 *
 * Region rectangles are separated by at least the coarsest supported cell:
 * 1086/20 = 55 units vertically and 1448/44 = 33 horizontally. Every gap below
 * is well past that, because map_rect() truncation costs another unit or two on
 * top of the cell rounding.
 */
# define MULTIPLAYER_REFERENCE_WIDTH	1448
# define MULTIPLAYER_REFERENCE_HEIGHT	1086

/* Mode select: one opaque panel over the home artwork, one focus region. */
# define MP_MODE_REF_PANEL_X	120
# define MP_MODE_REF_PANEL_Y	560
# define MP_MODE_REF_PANEL_WIDTH	1208
# define MP_MODE_REF_PANEL_HEIGHT	480
# define MP_MODE_REF_TITLE_Y	592
# define MP_MODE_REF_TITLE_HEIGHT	54
# define MP_MODE_REF_SUBTITLE_Y	652
# define MP_MODE_REF_SUBTITLE_HEIGHT	34
# define MP_MODE_REF_CARDS_X	160
# define MP_MODE_REF_CARDS_Y	700
# define MP_MODE_REF_CARDS_WIDTH	1128
# define MP_MODE_REF_CARDS_HEIGHT	240
/*
 * The card strip's plane also carries the control legend and the result line.
 * A separate plane for them would sit 28 units under the cards, well inside the
 * one-cell rounding floor, and two planes that round onto the same cell blank
 * each other on a stationary protocol.
 */
# define MP_MODE_REF_REGION_HEIGHT	320
# define MP_MODE_REF_CARD_WIDTH	540
# define MP_MODE_REF_CARD_STEP_X	588
# define MP_MODE_REF_CARD_NAME_Y	40
# define MP_MODE_REF_CARD_NAME_GLYPH	22
# define MP_MODE_REF_CARD_PLAYERS_Y	104
# define MP_MODE_REF_CARD_BODY_Y	150
# define MP_MODE_REF_CARD_BODY_GLYPH	11
# define MP_MODE_REF_CARD_BODY_STEP	26
# define MP_MODE_REF_CONTROLS_Y	968
# define MP_MODE_REF_CONTROLS_HEIGHT	48

/* Lobby: room table on the left, join-by-id on the right, status underneath. */
# define LOBBY_REF_CONTENT_X	120
# define LOBBY_REF_CONTENT_WIDTH	1208
# define LOBBY_REF_TITLE_Y	34
# define LOBBY_REF_TITLE_HEIGHT	58
# define LOBBY_REF_IDENTITY_Y	46
# define LOBBY_REF_IDENTITY_HEIGHT	38
# define LOBBY_REF_DIVIDER_Y	114
# define LOBBY_REF_ROOMS_X	120
# define LOBBY_REF_ROOMS_Y	160
# define LOBBY_REF_ROOMS_WIDTH	700
# define LOBBY_REF_ROOMS_HEIGHT	580
# define LOBBY_REF_ROOMS_TITLE_Y	178
# define LOBBY_REF_ROOMS_HEADER_Y	224
# define LOBBY_REF_LIST_X	132
# define LOBBY_REF_LIST_Y	168
# define LOBBY_REF_LIST_WIDTH	676
# define LOBBY_REF_LIST_HEIGHT	552
/* Offsets inside the list region: its own heading, then the column names. */
# define LOBBY_REF_LIST_TITLE_Y	10
# define LOBBY_REF_LIST_HEADER_Y	56
# define LOBBY_REF_LIST_FIRST_ROW_Y	88
# define LOBBY_REF_LIST_ROW_STEP	56
# define LOBBY_REF_LIST_ROW_HEIGHT	48
/* Table columns, as offsets inside the list rectangle. */
# define LOBBY_REF_COL_MARK	0
# define LOBBY_REF_COL_ID	26
# define LOBBY_REF_COL_MODE	228
# define LOBBY_REF_COL_PLAYERS	330
# define LOBBY_REF_COL_STATE	440
# define LOBBY_REF_COL_OWNER	560
# define LOBBY_REF_JOIN_X	880
# define LOBBY_REF_JOIN_Y	160
# define LOBBY_REF_JOIN_WIDTH	448
# define LOBBY_REF_JOIN_HEIGHT	580
# define LOBBY_REF_JOIN_TITLE_Y	178
# define LOBBY_REF_FIELD_X	896
# define LOBBY_REF_FIELD_Y	256
# define LOBBY_REF_FIELD_WIDTH	416
# define LOBBY_REF_FIELD_HEIGHT	170
# define LOBBY_REF_FIELD_BOX_Y	36
# define LOBBY_REF_FIELD_BOX_HEIGHT	62
# define LOBBY_REF_JOIN_HINT_Y	470
# define LOBBY_REF_STATUS_X	120
# define LOBBY_REF_STATUS_Y	800
# define LOBBY_REF_STATUS_WIDTH	1208
# define LOBBY_REF_STATUS_HEIGHT	64
# define LOBBY_REF_LEGEND_Y	896
# define LOBBY_REF_CONTROLS_Y	948
# define LOBBY_REF_CONTROLS_HEIGHT	48
/*
 * Opaque bands behind the two strips that fall outside the panels. The backdrop
 * is only quiet in its middle, so the title row and the control legend need one
 * or they land on the skull frieze and the tetromino rubble respectively.
 */
# define LOBBY_REF_BAND_TOP_Y	16
# define LOBBY_REF_BAND_TOP_HEIGHT	108
# define LOBBY_REF_BAND_BOTTOM_Y	782
# define LOBBY_REF_BAND_BOTTOM_HEIGHT	228
# define LOBBY_REF_HEADING_GLYPH	13
# define LOBBY_REF_ROW_GLYPH	13

/* Create room: a modal-looking panel drawn as its own screen, one region. */
# define CREATE_REF_PANEL_X	260
# define CREATE_REF_PANEL_Y	300
# define CREATE_REF_PANEL_WIDTH	928
# define CREATE_REF_PANEL_HEIGHT	480
# define CREATE_REF_TITLE_Y	334
# define CREATE_REF_PROMPT_Y	400
# define CREATE_REF_OPTIONS_X	292
# define CREATE_REF_OPTIONS_Y	452
# define CREATE_REF_OPTIONS_WIDTH	864
# define CREATE_REF_OPTIONS_HEIGHT	216
# define CREATE_REF_OPTIONS_FEEDBACK_Y	180
# define CREATE_REF_OPTION_STEP_Y	86
# define CREATE_REF_OPTION_HEIGHT	76
# define CREATE_REF_OPTION_NAME_X	40
# define CREATE_REF_OPTION_COUNT_X	400
# define CREATE_REF_OPTION_TAG_X	640
# define CREATE_REF_DIVIDER_Y	678
# define CREATE_REF_CONTROLS_Y	708
# define CREATE_REF_CONTROLS_HEIGHT	48

/* Waiting room: slots and status on the left, chat column on the right. */
# define ROOM_REF_CONTENT_X	120
# define ROOM_REF_TITLE_Y	34
# define ROOM_REF_TITLE_HEIGHT	58
# define ROOM_REF_SHARE_Y	108
# define ROOM_REF_SHARE_HEIGHT	34
# define ROOM_REF_DIVIDER_Y	158
# define ROOM_REF_SLOTS_PLATE_X	120
# define ROOM_REF_SLOTS_PLATE_Y	190
# define ROOM_REF_SLOTS_PLATE_WIDTH	760
# define ROOM_REF_SLOTS_PLATE_HEIGHT	520
# define ROOM_REF_SLOTS_TITLE_Y	208
# define ROOM_REF_SLOTS_X	134
# define ROOM_REF_SLOTS_Y	196
# define ROOM_REF_SLOTS_WIDTH	732
# define ROOM_REF_SLOTS_HEIGHT	508
/* Offsets inside the slots region: its own heading, then the seat rows. */
# define ROOM_REF_SLOTS_HEADING_Y	10
# define ROOM_REF_SLOT_FIRST_Y	62
# define ROOM_REF_SLOT_STEP_Y	54
# define ROOM_REF_SLOT_HEIGHT	46
# define ROOM_REF_SLOT_BADGE_X	472
# define ROOM_REF_STATUS_X	120
# define ROOM_REF_STATUS_Y	780
# define ROOM_REF_STATUS_WIDTH	760
# define ROOM_REF_STATUS_HEIGHT	92
# define ROOM_REF_STATUS_FEEDBACK_Y	52
# define ROOM_REF_CHAT_PLATE_X	940
# define ROOM_REF_CHAT_PLATE_Y	190
# define ROOM_REF_CHAT_PLATE_WIDTH	388
# define ROOM_REF_CHAT_PLATE_HEIGHT	676
# define ROOM_REF_CHAT_TITLE_Y	208
# define ROOM_REF_CHAT_X	950
# define ROOM_REF_CHAT_Y	246
# define ROOM_REF_CHAT_WIDTH	368
# define ROOM_REF_CHAT_HEIGHT	610
# define ROOM_REF_CHAT_LINE_STEP	28
# define ROOM_REF_CHAT_GLYPH	10
# define ROOM_REF_CHAT_COMPOSE_HEIGHT	58
# define ROOM_REF_CONTROLS_Y	920
# define ROOM_REF_CONTROLS_HEIGHT	48
# define ROOM_REF_BAND_TOP_Y	16
# define ROOM_REF_BAND_TOP_HEIGHT	152
# define ROOM_REF_BAND_BOTTOM_Y	758
# define ROOM_REF_BAND_BOTTOM_HEIGHT	234
# define ROOM_REF_HEADING_GLYPH	13
# define ROOM_REF_ROW_GLYPH	12

/* MULTIPLAYER_SCREEN.C / LOBBY_SCREEN.C / WAITING_ROOM_SCREEN.C */
# define LOBBY_ROOM_ID_MAX	24
# define APP_ROOM_CHAT_MAX	24
# define APP_ROOM_CHAT_TEXT_MAX	96
# define MP_MODE_CARD_COUNT	2
/* One second per step, matching the visible "starting in N" copy. */
# define WAITING_ROOM_COUNTDOWN_START	5
# define WAITING_ROOM_COUNTDOWN_STEP_MS	1000
# define WAITING_ROOM_DOUBLE_PLAYERS	2
# define WAITING_ROOM_ROYALE_MIN_PLAYERS	4
/* The authored waiting-room roster has room for eight legible rows. */
# define WAITING_ROOM_VISIBLE_PLAYERS	8

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
/*
 * The shared 8x16 atlas anchors every cap height at FONT_INK_Y and sizes
 * glyphs from FONT_INK_HEIGHT, so those two drive layout. Ink itself runs
 * wider than the cap band: dots on i and j sit at FONT_INK_TOP, and the
 * tails of g j p q y and the comma reach FONT_INK_BOTTOM. Sampling only the
 * cap band silently turns "player" into "olauer", so blitters draw the full
 * ink band while keeping the cap row on the baseline the layout expects.
 */
# define FONT_INK_Y				4
# define FONT_INK_HEIGHT			8
# define FONT_INK_TOP			2
# define FONT_INK_BOTTOM			15
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
	APP_SCREEN_MULTIPLAYER_MODE,
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
	APP_NAV_OPEN_MULTIPLAYER_MODE,
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

typedef enum e_tetrisu_renderer_mode
{
	TETRISU_RENDERER_AUTO,
	TETRISU_RENDERER_CELL,
	TETRISU_RENDERER_STATIONARY,
	TETRISU_RENDERER_PIXEL
}	tetrisu_renderer_mode_t;

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
	char		portrait_asset[APP_ASSET_PATH_MAX];
	uint64_t	score;
	int			wallet_points;
	int			rank;
}	app_profile_view_model_t;

typedef struct s_app_character_ability_view_model
{
	char	name[APP_TEXT_MAX];
	char	description[APP_ABILITY_TEXT_MAX];
}	app_character_ability_view_model_t;

typedef struct s_app_catalogue_item_view_model
{
	char	id[APP_TEXT_MAX];
	char	name[APP_TEXT_MAX];
	char	portrait_asset[APP_ASSET_PATH_MAX];
	int		price;
	bool	owned;
	bool	equipped;
	app_character_ability_view_model_t	abilities[
		APP_CHARACTER_ABILITY_COUNT];
}	app_catalogue_item_view_model_t;

typedef struct s_app_catalogue_view_model
{
	app_catalogue_kind_t			kind;
	int							count;
	app_catalogue_item_view_model_t	items[APP_CATALOGUE_MAX_ITEMS];
}	app_catalogue_view_model_t;

typedef struct s_app_settings_view_model
{
	bool					signed_in;
	bool					offline;
	app_profile_view_model_t	profile;
	app_catalogue_view_model_t	characters;
	app_catalogue_view_model_t	themes;
	int					music_volume;
	tetrisu_renderer_mode_t	renderer_mode;
	char					local_status[APP_TEXT_MAX];
}	app_settings_view_model_t;

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

typedef enum e_leaderboard_focus
{
	LEADERBOARD_FOCUS_BACK,
	LEADERBOARD_FOCUS_REFRESH
}	leaderboard_focus_t;

typedef enum e_leaderboard_action
{
	LEADERBOARD_ACTION_NONE,
	LEADERBOARD_ACTION_BACK,
	LEADERBOARD_ACTION_REFRESH,
	LEADERBOARD_ACTION_QUIT
}	leaderboard_action_t;

typedef struct s_leaderboard_state
{
	leaderboard_focus_t	focus;
}	leaderboard_state_t;

/*
 * Settings is navigated as three stacked regions rather than one focus ring:
 * the two inventory grids sit side by side above the control row, so Left and
 * Right cross between them at the panel edges and Up/Down step between rows
 * before falling through to the controls.
 */
typedef enum e_settings_section
{
	SETTINGS_SECTION_CHARACTERS,
	SETTINGS_SECTION_THEMES,
	SETTINGS_SECTION_CONTROLS
}	settings_section_t;

typedef enum e_settings_focus
{
	SETTINGS_FOCUS_BACK,
	SETTINGS_FOCUS_MARKETPLACE,
	SETTINGS_FOCUS_VOLUME_DOWN,
	SETTINGS_FOCUS_VOLUME_UP
}	settings_focus_t;

typedef enum e_settings_action
{
	SETTINGS_ACTION_NONE,
	SETTINGS_ACTION_BACK,
	SETTINGS_ACTION_MARKETPLACE,
	SETTINGS_ACTION_VOLUME_DOWN,
	SETTINGS_ACTION_VOLUME_UP,
	SETTINGS_ACTION_CHARACTER_PREVIOUS,
	SETTINGS_ACTION_CHARACTER_NEXT,
	SETTINGS_ACTION_EQUIP_CHARACTER,
	SETTINGS_ACTION_EQUIP_THEME,
	SETTINGS_ACTION_QUIT
}	settings_action_t;

typedef enum e_settings_equip_result
{
	SETTINGS_EQUIP_INVALID,
	SETTINGS_EQUIP_UNCHANGED,
	SETTINGS_EQUIP_CHANGED,
	SETTINGS_EQUIP_LOCKED
}	settings_equip_result_t;

typedef struct s_settings_state
{
	settings_section_t	section;
	settings_focus_t	focus;
	int			character_slot;
	int			theme_slot;
	int			character_slots;
	int			theme_slots;
	bool			signed_in;
	bool			ability_info_visible;
}	settings_state_t;

typedef struct s_settings_rect
{
	int	x;
	int	y;
	int	width;
	int	height;
} settings_rect_t;

/* Reference-coordinate geometry shared by bitmap composition and hit tests. */
typedef struct s_settings_layout
{
	int			origin_y;
	int			origin_x;
	int			rows;
	int			cols;
	int			pixel_width;
	int			pixel_height;
	int			cell_px_x;
	int			cell_px_y;
	bool			opaque_background;
	settings_rect_t	portrait;
	settings_rect_t	profile;
	settings_rect_t	characters;
	settings_rect_t	themes;
	settings_rect_t	stats[3];
	settings_rect_t	buttons[SETTINGS_BUTTON_COUNT];
	settings_rect_t	controls;
	settings_rect_t	card;
	settings_rect_t	volume;
	settings_rect_t	volume_offline;
} settings_layout_t;

/*
 * The Marketplace needs exactly what Settings needs: the profile the wallet
 * balance comes from, both catalogues with prices and ownership, and the local
 * controls. Aliasing the type keeps one provider path and one set of catalogue
 * helpers instead of two that can drift apart.
 */
typedef app_settings_view_model_t	app_marketplace_view_model_t;

/*
 * The Marketplace is navigated as the same three stacked regions Settings uses
 * - two inventory grids side by side above a control row - so Left and Right
 * cross between the panels at their edges and Up/Down step between rows before
 * falling through to the controls.
 */
typedef enum e_marketplace_section
{
	MARKETPLACE_SECTION_CHARACTERS,
	MARKETPLACE_SECTION_THEMES,
	MARKETPLACE_SECTION_CONTROLS
}	marketplace_section_t;

typedef enum e_marketplace_focus
{
	MARKETPLACE_FOCUS_BACK,
	MARKETPLACE_FOCUS_BUY,
	MARKETPLACE_FOCUS_VOLUME_DOWN,
	MARKETPLACE_FOCUS_VOLUME_UP
}	marketplace_focus_t;

typedef enum e_marketplace_action
{
	MARKETPLACE_ACTION_NONE,
	MARKETPLACE_ACTION_BACK,
	MARKETPLACE_ACTION_BUY,
	MARKETPLACE_ACTION_EQUIP,
	MARKETPLACE_ACTION_VOLUME_DOWN,
	MARKETPLACE_ACTION_VOLUME_UP,
	MARKETPLACE_ACTION_QUIT
}	marketplace_action_t;

/*
 * Marketplace feedback is a line inside the control region, not a floating
 * notification card. A notification is a plane of its own raised over the
 * screen, and raising or dropping one damages the cells it covers, which makes
 * a stationary protocol retransmit the full-screen bitmap underneath - over
 * every region plane, blanking the shelves, the card and the buttons until the
 * next keystroke. Keeping every pixel this screen draws inside its own regions
 * removes that whole class of failure, and the outcome of a purchase is
 * already visible in the wallet and the tile anyway.
 */
typedef enum e_marketplace_feedback
{
	MARKETPLACE_FEEDBACK_NONE,
	MARKETPLACE_FEEDBACK_BOUGHT,
	MARKETPLACE_FEEDBACK_EQUIPPED,
	MARKETPLACE_FEEDBACK_OWNED,
	MARKETPLACE_FEEDBACK_INSUFFICIENT,
	MARKETPLACE_FEEDBACK_LOCKED,
	MARKETPLACE_FEEDBACK_VOLUME
}	marketplace_feedback_t;

typedef enum e_marketplace_purchase_result
{
	MARKETPLACE_PURCHASE_INVALID,
	MARKETPLACE_PURCHASE_OWNED,
	MARKETPLACE_PURCHASE_INSUFFICIENT,
	MARKETPLACE_PURCHASE_BOUGHT
}	marketplace_purchase_result_t;

/*
 * preview records the last inventory panel the cursor visited. The detail card
 * keeps describing that item while focus sits in the control row, so stepping
 * down to Buy never blanks the thing being bought.
 */
typedef struct s_marketplace_state
{
	marketplace_section_t	section;
	marketplace_section_t	preview;
	marketplace_focus_t		focus;
	marketplace_feedback_t	feedback;
	int						feedback_value;
	int						character_slot;
	int						theme_slot;
	int						character_slots;
	int						theme_slots;
	bool					signed_in;
}	marketplace_state_t;

typedef struct s_marketplace_rect
{
	int	x;
	int	y;
	int	width;
	int	height;
}	marketplace_rect_t;

/* Reference-coordinate geometry shared by bitmap composition and the tests. */
typedef struct s_marketplace_layout
{
	int					origin_y;
	int					origin_x;
	int					rows;
	int					cols;
	int					pixel_width;
	int					pixel_height;
	int					cell_px_x;
	int					cell_px_y;
	bool				opaque_background;
	marketplace_rect_t	title;
	marketplace_rect_t	wallet_card;
	marketplace_rect_t	stats[3];
	marketplace_rect_t	characters;
	marketplace_rect_t	themes;
	marketplace_rect_t	detail;
	marketplace_rect_t	controls;
	marketplace_rect_t	buttons[MARKETPLACE_BUTTON_COUNT];
}	marketplace_layout_t;

typedef enum e_app_room_state
{
	APP_ROOM_STATE_WAITING,
	APP_ROOM_STATE_READY,
	APP_ROOM_STATE_IN_GAME,
	APP_ROOM_STATE_FINISHED
}	app_room_state_t;

typedef struct s_app_room_summary_view_model
{
	char				id[APP_TEXT_MAX];
	char				owner[APP_TEXT_MAX];
	app_game_mode_t		mode;
	app_room_state_t	state;
	int					players;
	int					capacity;
}	app_room_summary_view_model_t;

/*
 * The lobby header carries the same username, score and rank the wireframe
 * shows, so the profile travels with the room list rather than being fetched
 * again by the renderer.
 */
typedef struct s_app_lobby_view_model
{
	app_profile_view_model_t		profile;
	int							count;
	app_room_summary_view_model_t	rooms[APP_LOBBY_MAX_ROOMS];
}	app_lobby_view_model_t;

typedef struct s_app_room_player_view_model
{
	char	username[APP_TEXT_MAX];
	bool	owner;
	bool	ready;
}	app_room_player_view_model_t;

/*
 * Chat lives on the room model rather than beside it: the waiting room is the
 * only screen that shows it, and a server push will replace the whole room
 * snapshot at once. system entries are room events (joins, leaves, countdown)
 * and are drawn without an author.
 */
typedef struct s_app_room_chat_view_model
{
	char	author[APP_TEXT_MAX];
	char	text[APP_ROOM_CHAT_TEXT_MAX];
	bool	system;
}	app_room_chat_view_model_t;

typedef struct s_app_room_view_model
{
	char						id[APP_TEXT_MAX];
	app_game_mode_t				mode;
	app_room_state_t			state;
	int							required_players;
	int							capacity;
	int							player_count;
	int							local_slot;
	app_room_player_view_model_t	players[APP_ROOM_MAX_PLAYERS];
	int							chat_count;
	app_room_chat_view_model_t	chat[APP_ROOM_CHAT_MAX];
}	app_room_view_model_t;

typedef struct s_app_match_view_model
{
	char			room_id[APP_TEXT_MAX];
	app_game_mode_t	mode;
	int				player_count;
	char			status[APP_TEXT_MAX];
}	app_match_view_model_t;

/*
 * Multiplayer mode select. Two cards over the home artwork; the chosen mode
 * becomes the lobby's list filter, so picking Double lands on a lobby showing
 * duel rooms with Battle Royale one keystroke away rather than hidden.
 */
typedef enum e_mp_mode_focus
{
	MP_MODE_FOCUS_DOUBLE,
	MP_MODE_FOCUS_BATTLE_ROYALE
}	mp_mode_focus_t;

typedef enum e_mp_mode_action
{
	MP_MODE_ACTION_NONE,
	MP_MODE_ACTION_SELECT,
	MP_MODE_ACTION_BACK,
	MP_MODE_ACTION_VOLUME_DOWN,
	MP_MODE_ACTION_VOLUME_UP,
	MP_MODE_ACTION_QUIT
}	mp_mode_action_t;

typedef enum e_mp_mode_feedback
{
	MP_MODE_FEEDBACK_NONE,
	MP_MODE_FEEDBACK_VOLUME
}	mp_mode_feedback_t;

typedef struct s_mp_mode_state
{
	mp_mode_focus_t		focus;
	mp_mode_feedback_t	feedback;
	int					feedback_value;
}	mp_mode_state_t;

/*
 * Create room is its own screen rather than a plane raised over the lobby. A
 * raised plane damages the cells it covers, and on a stationary protocol that
 * retransmits the full-screen bitmap underneath over every region above it -
 * the failure that blanked the Marketplace. Drawing the panel into a screen of
 * its own looks identical and cannot blank anything.
 */
typedef enum e_create_room_action
{
	CREATE_ROOM_ACTION_NONE,
	CREATE_ROOM_ACTION_CREATE,
	CREATE_ROOM_ACTION_CANCEL,
	CREATE_ROOM_ACTION_VOLUME_DOWN,
	CREATE_ROOM_ACTION_VOLUME_UP,
	CREATE_ROOM_ACTION_QUIT
}	create_room_action_t;

typedef struct s_create_room_state
{
	app_game_mode_t		mode;
	mp_mode_feedback_t	feedback;
	int					feedback_value;
}	create_room_state_t;

/*
 * The lobby is navigated as two sections side by side. While the join field
 * holds focus every printable key is text, so the single-letter commands are
 * only live in the room table; Escape steps out of the field rather than off
 * the screen.
 */
typedef enum e_lobby_section
{
	LOBBY_SECTION_ROOMS,
	LOBBY_SECTION_JOIN
}	lobby_section_t;

typedef enum e_lobby_feedback
{
	LOBBY_FEEDBACK_NONE,
	LOBBY_FEEDBACK_REFRESHED,
	LOBBY_FEEDBACK_FULL,
	LOBBY_FEEDBACK_IN_GAME,
	LOBBY_FEEDBACK_FINISHED,
	LOBBY_FEEDBACK_EMPTY_LIST,
	LOBBY_FEEDBACK_EMPTY_ID,
	LOBBY_FEEDBACK_UNKNOWN_ID,
	LOBBY_FEEDBACK_INVALID_ROOM,
	LOBBY_FEEDBACK_FILTER
}	lobby_feedback_t;

typedef enum e_lobby_action
{
	LOBBY_ACTION_NONE,
	LOBBY_ACTION_JOIN,
	LOBBY_ACTION_JOIN_BY_ID,
	LOBBY_ACTION_CREATE,
	LOBBY_ACTION_REFRESH,
	LOBBY_ACTION_BACK,
	LOBBY_ACTION_VOLUME_DOWN,
	LOBBY_ACTION_VOLUME_UP,
	LOBBY_ACTION_QUIT
}	lobby_action_t;

typedef struct s_lobby_state
{
	lobby_section_t		section;
	lobby_feedback_t	feedback;
	int					feedback_value;
	int					selected;
	int					list_offset;
	int					visible_count;
	app_game_mode_t		filter;
	char				room_id[LOBBY_ROOM_ID_MAX];
	int					room_id_length;
}	lobby_state_t;

typedef enum e_room_feedback
{
	ROOM_FEEDBACK_NONE,
	ROOM_FEEDBACK_READY,
	ROOM_FEEDBACK_NOT_READY,
	ROOM_FEEDBACK_NEED_PLAYERS,
	ROOM_FEEDBACK_NEED_READY,
	ROOM_FEEDBACK_NOT_OWNER,
	ROOM_FEEDBACK_INVALID_ROOM,
	ROOM_FEEDBACK_UNAVAILABLE,
	ROOM_FEEDBACK_CANCELLED,
	ROOM_FEEDBACK_CHAT_SENT,
	ROOM_FEEDBACK_CHAT_EMPTY,
	ROOM_FEEDBACK_CHAT_FULL,
	ROOM_FEEDBACK_VOLUME
}	room_feedback_t;

typedef enum e_room_action
{
	ROOM_ACTION_NONE,
	ROOM_ACTION_TOGGLE_READY,
	ROOM_ACTION_START,
	ROOM_ACTION_SEND_CHAT,
	ROOM_ACTION_LEAVE,
	ROOM_ACTION_LAUNCH,
	ROOM_ACTION_VOLUME_DOWN,
	ROOM_ACTION_VOLUME_UP,
	ROOM_ACTION_QUIT
}	room_action_t;

/*
 * countdown is the only value on this screen that moves without a keystroke, so
 * it lives in the status region alone: one small plane repaints per second and
 * nothing else on the screen is touched.
 */
typedef struct s_waiting_room_state
{
	bool			chatting;
	bool			counting_down;
	int				countdown;
	int				roster_offset;
	room_feedback_t	feedback;
	int				feedback_value;
	char			compose[APP_ROOM_CHAT_TEXT_MAX];
	int				compose_length;
}	waiting_room_state_t;

typedef struct s_mp_rect
{
	int	x;
	int	y;
	int	width;
	int	height;
}	mp_rect_t;

/* Reference-coordinate geometry shared by every multiplayer compositor. */
typedef struct s_mp_layout
{
	int			origin_y;
	int			origin_x;
	int			rows;
	int			cols;
	int			pixel_width;
	int			pixel_height;
	int			cell_px_x;
	int			cell_px_y;
	bool		opaque_background;
	mp_rect_t	panel;
	mp_rect_t	cards;
	mp_rect_t	card_slots[MP_MODE_CARD_COUNT];
	mp_rect_t	rooms_plate;
	mp_rect_t	list;
	mp_rect_t	join_plate;
	mp_rect_t	field;
	mp_rect_t	status;
	mp_rect_t	options;
	mp_rect_t	slots_plate;
	mp_rect_t	slots;
	mp_rect_t	chat_plate;
	mp_rect_t	chat;
	mp_rect_t	controls;
}	mp_layout_t;

typedef union u_app_screen_data
{
	app_auth_view_model_t			auth;
	app_profile_view_model_t		profile;
	app_settings_view_model_t	settings;
	app_marketplace_view_model_t	marketplace;
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
	app_provider_result_t	(*load_settings)(void *userdata,
			app_settings_view_model_t *view);
	app_provider_result_t	(*load_catalogue)(void *userdata,
			app_catalogue_kind_t kind, app_catalogue_view_model_t *view);
	app_provider_result_t	(*preview_login)(void *userdata,
			app_auth_view_model_t *view);
	app_provider_result_t	(*load_leaderboard)(void *userdata,
			app_leaderboard_view_model_t *view);
	app_provider_result_t	(*load_lobby)(void *userdata,
			app_lobby_view_model_t *view);
	app_provider_result_t	(*load_room)(void *userdata, const char *room_id,
			app_room_view_model_t *view);
	/*
	 * Creating and joining are separate calls because the room they produce
	 * differs: a created room holds its owner alone and cannot start, a joined
	 * one already has the players the lobby listed. A server will need the same
	 * split, so the seam is the same shape now as it will be then.
	 */
	app_provider_result_t	(*create_room)(void *userdata, app_game_mode_t mode,
			app_room_view_model_t *view);
}	app_data_provider_t;

// How far the terminal can be trusted with bitmap graphics. NONE is the
// terminal-cell compatibility renderer; STATIONARY draws bitmaps but never
// moves or overlaps them; MOVABLE allows freely moved and restacked sprixels.
typedef enum e_tetrisu_pixel_policy
{
	TETRISU_PIXELS_NONE,
	TETRISU_PIXELS_STATIONARY,
	TETRISU_PIXELS_MOVABLE
}	tetrisu_pixel_policy_t;

typedef enum e_leaderboard_background_action
{
	LEADERBOARD_BACKGROUND_CELL,
	LEADERBOARD_BACKGROUND_REUSE,
	LEADERBOARD_BACKGROUND_REPLACE_EXACT
}	leaderboard_background_action_t;

typedef struct s_leaderboard_layout
{
	int		y;
	int		x;
	int		rows;
	int		cols;
	bool	compact;
}	leaderboard_layout_t;

typedef struct s_leaderboard_pixel_rect
{
	int	x;
	int	y;
	int	width;
	int	height;
}	leaderboard_pixel_rect_t;

typedef struct s_leaderboard_pixel_layout
{
	int					origin_y;
	int					origin_x;
	int					rows;
	int					cols;
	int					pixel_width;
	int					pixel_height;
	int					cell_px_x;
	int					cell_px_y;
	leaderboard_pixel_rect_t	buttons[LEADERBOARD_PIXEL_BUTTON_COUNT];
	leaderboard_pixel_rect_t	controls;
}	leaderboard_pixel_layout_t;

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
	AUTH_FOCUS_PREVIEW,
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
	AUTH_ACTION_PREVIEW_LOGIN,
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

typedef enum e_ui_notification_kind
{
	UI_NOTIFICATION_VOLUME,
	UI_NOTIFICATION_OWNERSHIP
}	ui_notification_kind_t;

typedef struct s_ui_notification
{
	ui_notification_kind_t	kind;
	char		title[UI_NOTIFICATION_TITLE_MAX + 1];
	char		message[UI_NOTIFICATION_MESSAGE_MAX + 1];
	int			percent;
	uint64_t	shown_at_ms;
}	ui_notification_t;

typedef struct s_ui_notification_stack
{
	ui_notification_t	items[UI_NOTIFICATION_STACK_MAX];
	int					count;
}	ui_notification_stack_t;

// One decoded RGBA surface held in memory: glyph sheets, card art, thumbnails.
typedef struct s_pixel_asset
{
	uint32_t	*pixels;
	int			width;
	int			height;
}	pixel_asset_t;

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
	struct ncplane		*screen_plane;
	struct ncplane		*settings_portrait_plane;
	struct ncplane		*settings_controls_plane;
	struct ncplane		*settings_characters_plane;
	struct ncplane		*settings_themes_plane;
	struct ncplane		*settings_volume_plane;
	struct ncplane		*settings_ability_plane;
	struct ncplane		*marketplace_stats_plane;
	struct ncplane		*marketplace_characters_plane;
	struct ncplane		*marketplace_themes_plane;
	struct ncplane		*marketplace_detail_plane;
	struct ncplane		*marketplace_controls_plane;
	/*
	 * One set of region planes serves all four multiplayer screens. They never
	 * coexist - the loop tears the previous screen down before the next one
	 * draws - so naming them per role rather than per screen keeps the teardown
	 * path single and total.
	 */
	struct ncplane		*mp_cards_plane;
	struct ncplane		*mp_list_plane;
	struct ncplane		*mp_field_plane;
	struct ncplane		*mp_status_plane;
	struct ncplane		*mp_options_plane;
	struct ncplane		*mp_slots_plane;
	struct ncplane		*mp_chat_plane;
	struct ncplane		*auth_overlay_planes[AUTH_OVERLAY_PLANE_MAX];
	struct ncplane		*bunny_plane;
	struct ncvisual		*bunny_visual;
	struct ncvisual		*auth_background_visual;
	struct ncvisual		*auth_font_visual;
	struct ncvisual		*settings_font_visual;
	struct ncvisual		*marketplace_font_visual;
	struct ncvisual		*mp_font_visual;
	/*
	 * The stationary tier recomposes the whole frame on every focus change, so
	 * the equipped portrait is kept decoded rather than re-read from disk each
	 * time. The source path is the cache key.
	 */
	struct ncvisual		*settings_portrait_visual;
	char				settings_portrait_source[APP_ASSET_PATH_MAX];
	struct ncvisual		*settings_thumbnail_visuals[
			APP_CATALOGUE_MAX_ITEMS * 2];
	char				settings_thumbnail_sources[
			APP_CATALOGUE_MAX_ITEMS * 2][APP_ASSET_PATH_MAX];
	/*
	 * The Marketplace keeps its own thumbnail cache: it draws the same artwork
	 * at two sizes (slot tile and detail preview) and outlives no Settings
	 * session, so sharing one cache would thrash whenever the two screens
	 * alternate.
	 */
	struct ncvisual		*marketplace_thumbnail_visuals[
			APP_CATALOGUE_MAX_ITEMS * 2];
	char				marketplace_thumbnail_sources[
			APP_CATALOGUE_MAX_ITEMS * 2][APP_ASSET_PATH_MAX];
	struct ncplane		*compatibility_plane;
	struct ncplane		*notification_art_planes[UI_NOTIFICATION_STACK_MAX];
	struct ncplane		*notification_planes[UI_NOTIFICATION_STACK_MAX];
	pixel_asset_t		leaderboard_font;
	uint32_t			*leaderboard_pixels;
	int					leaderboard_pixels_width;
	int					leaderboard_pixels_height;
	bool				leaderboard_pixel_active;
	struct ncplane		*leaderboard_controls_plane;
	uint64_t			leaderboard_static_signature;
	uint64_t			leaderboard_controls_signature;
	pixel_asset_t		notification_font;
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
	uint64_t			auth_background_signature;
	bool				settings_background_ready;
	int				settings_background_rows;
	int				settings_background_cols;
	/*
	 * Two full-resolution RGBA caches keep Settings responsive on the
	 * stationary tier. settings_background_pixels holds the resized backdrop
	 * so recomposing the static frame is a memcpy instead of a per-pixel walk
	 * of an ncvisual, and settings_static_pixels holds the composed frame so
	 * every small region plane can be prefilled from it without redrawing or
	 * reblitting the whole screen on a focus change.
	 */
	/*
	 * Stationary bitmap protocols cannot compose one sprixel over another:
	 * notcurses clears the cells an overlay covers, so a transparent overlay
	 * pixel exposes the terminal background rather than the artwork beneath.
	 * Global overlays therefore composite themselves against this snapshot of
	 * the fitted backdrop before they are blitted. It is anchored at
	 * bg_row/bg_col like every other backdrop-relative surface.
	 */
	uint32_t			*backdrop_pixels;
	int				backdrop_width;
	int				backdrop_height;
	uint32_t			*settings_background_pixels;
	uint32_t			*settings_static_pixels;
	int				settings_pixels_width;
	int				settings_pixels_height;
	uint64_t			settings_static_signature;
	uint64_t			settings_controls_signature;
	uint64_t			settings_characters_signature;
	uint64_t			settings_themes_signature;
	uint64_t			settings_volume_signature;
	uint64_t			settings_ability_signature;
	uint32_t			*marketplace_background_pixels;
	uint32_t			*marketplace_static_pixels;
	int					marketplace_pixels_width;
	int					marketplace_pixels_height;
	bool				marketplace_background_ready;
	int					marketplace_background_rows;
	int					marketplace_background_cols;
	uint64_t			marketplace_static_signature;
	uint64_t			marketplace_stats_signature;
	uint64_t			marketplace_characters_signature;
	uint64_t			marketplace_themes_signature;
	uint64_t			marketplace_detail_signature;
	uint64_t			marketplace_controls_signature;
	/*
	 * mp_screen and mp_background_source are part of the static cache key: the
	 * four multiplayer screens share these buffers but not their artwork, so a
	 * screen change must invalidate the composed frame even when the terminal
	 * geometry has not moved.
	 */
	uint32_t			*mp_background_pixels;
	uint32_t			*mp_static_pixels;
	int					mp_pixels_width;
	int					mp_pixels_height;
	bool				mp_background_ready;
	int					mp_background_rows;
	int					mp_background_cols;
	char				mp_background_source[APP_ASSET_PATH_MAX];
	app_screen_t		mp_screen;
	uint64_t			mp_static_signature;
	uint64_t			mp_cards_signature;
	uint64_t			mp_list_signature;
	uint64_t			mp_field_signature;
	uint64_t			mp_status_signature;
	uint64_t			mp_options_signature;
	uint64_t			mp_slots_signature;
	uint64_t			mp_chat_signature;
	uint64_t			auth_overlay_signatures[AUTH_OVERLAY_PLANE_MAX];
	int					auth_overlay_count;
	tetrisu_pixel_policy_t	pixels;
}	render_ctx_t;

typedef struct s_intro_stream
{
	render_ctx_t	*ctx;
	int				skipped;
}	intro_stream_t;

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

/* HOME_ROUTING.C */
typedef enum e_home_route_action
{
	HOME_ROUTE_NAVIGATE,
	HOME_ROUTE_SIGN_IN_REQUIRED,
	HOME_ROUTE_BLOCKED
}	home_route_action_t;

typedef struct s_home_route
{
	home_route_action_t	action;
	app_nav_action_t	nav_action;
	const char			*label;
}	home_route_t;

home_route_t	home_menu_route(int selected, bool offline);
void			home_sign_in_body_line1(const char *label, char *line1,
					size_t size);
void			home_sign_in_body_line2(char *line2, size_t size);

/* RENDER_SIGN_IN.C */
# define SIGN_IN_MODAL_COLS	58
# define SIGN_IN_MODAL_ROWS	14

typedef enum e_sign_in_focus
{
	SIGN_IN_FOCUS_DISMISS,
	SIGN_IN_FOCUS_LOGIN
}	sign_in_focus_t;

typedef struct s_sign_in_modal
{
	bool				visible;
	sign_in_focus_t		focus;
	const char			*label;
	struct ncplane		*text_plane;
}	sign_in_modal_t;

typedef enum e_sign_in_result
{
	SIGN_IN_RESULT_NONE,
	SIGN_IN_RESULT_DISMISS,
	SIGN_IN_RESULT_LOGIN
}	sign_in_result_t;

/* CONFIRMATION.C / RENDER_CONFIRMATION.C */
# define CONFIRMATION_MODAL_COLS	58
# define CONFIRMATION_MODAL_ROWS	12

typedef enum e_confirmation_kind
{
	CONFIRM_QUIT_APP,
	CONFIRM_LEAVE_ROOM,
	CONFIRM_LEAVE_MATCH
}	confirmation_kind_t;

typedef enum e_confirmation_focus
{
	CONFIRM_FOCUS_NO,
	CONFIRM_FOCUS_YES
}	confirmation_focus_t;

typedef enum e_confirmation_result
{
	CONFIRM_RESULT_NONE,
	CONFIRM_RESULT_NO,
	CONFIRM_RESULT_YES
}	confirmation_result_t;

typedef struct s_confirmation_dialog
{
	bool				visible;
	confirmation_kind_t	kind;
	confirmation_focus_t	focus;
	struct ncplane			*plane;
}	confirmation_dialog_t;

void			confirmation_dialog_init(confirmation_dialog_t *dialog,
					confirmation_kind_t kind);
confirmation_result_t	confirmation_dialog_handle_key(
					confirmation_dialog_t *dialog, uint32_t key);
const char		*confirmation_title(confirmation_kind_t kind);
const char		*confirmation_body(confirmation_kind_t kind);
bool			confirmation_prompt_run(render_ctx_t *ctx,
					audio_ctx_t *audio, confirmation_kind_t kind);

void			sign_in_modal_init(sign_in_modal_t *modal);
sign_in_result_t	sign_in_modal_handle_key(sign_in_modal_t *modal,
						uint32_t key);
sign_in_result_t	sign_in_modal_handle_mouse(sign_in_modal_t *modal,
						const render_ctx_t *ctx, const ncinput *input,
						uint32_t key);
bool			render_sign_in_show(render_ctx_t *ctx,
					sign_in_modal_t *modal);
bool			render_sign_in_refresh(render_ctx_t *ctx,
					sign_in_modal_t *modal);
void			render_sign_in_destroy(render_ctx_t *ctx,
					sign_in_modal_t *modal);

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
bool			app_ui_preview_enabled(void);

/* APP_PROVIDER.C */
void			app_fixture_provider_init(app_data_provider_t *provider);
app_provider_result_t	app_provider_preview_sign_in(
					const app_data_provider_t *provider,
					app_auth_view_model_t *view);
app_provider_result_t	app_screen_view_load(
					const app_data_provider_t *provider,
					app_screen_t screen, app_screen_view_model_t *view);
app_provider_result_t	app_screen_view_load_for_session(
					const app_data_provider_t *provider,
					app_screen_t screen, bool offline,
					app_screen_view_model_t *view);
void			app_settings_apply_local_controls(
					app_settings_view_model_t *view, int music_volume,
					tetrisu_renderer_mode_t renderer_mode);
app_provider_result_t	app_room_view_load(
					const app_data_provider_t *provider, const char *room_id,
					app_screen_view_model_t *view);
app_provider_result_t	app_room_view_create(
					const app_data_provider_t *provider, app_game_mode_t mode,
					app_screen_view_model_t *view);
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
bool			ui_notification_show(ui_notification_stack_t *stack,
					const char *title, int percent, uint64_t now_ms);
bool			ui_notification_show_ownership(
					ui_notification_stack_t *stack, uint64_t now_ms);
bool			ui_notification_update(ui_notification_stack_t *stack,
					uint64_t now_ms);
int				ui_notification_opacity(const ui_notification_t *notification,
					uint64_t now_ms);
int				ui_notification_next_wake_ms(
					const ui_notification_stack_t *stack, uint64_t now_ms);
int				ui_notification_next_expiry_ms(
					const ui_notification_stack_t *stack, uint64_t now_ms);
int				ui_notification_volume_percent(int volume);
uint64_t		ui_notification_now_ms(void);

/* RENDER_BACKGROUND.C */
render_ctx_t	render_init(const char *image_path);
uint32_t		render_wait_key(render_ctx_t *ctx);
uint32_t		render_wait_input(render_ctx_t *ctx, ncinput *input);
uint32_t		render_wait_input_timeout(render_ctx_t *ctx, ncinput *input,
					int timeout_ms);
void			render_teardown(render_ctx_t *ctx);
int				render_background_replace(render_ctx_t *ctx,
					const char *image_path, bool stretch);
int				render_background_replace_exact(render_ctx_t *ctx,
					const char *image_path, bool stretch);
int				render_background_replace_visual(render_ctx_t *ctx,
					struct ncvisual *ncv, bool stretch);
void				render_background_destroy(render_ctx_t *ctx);
void				render_backdrop_forget(render_ctx_t *ctx);
const uint32_t		*render_backdrop_pixels(const render_ctx_t *ctx,
					int *width, int *height);
int				render_geometry_refresh(render_ctx_t *ctx, bool repaint);
bool				render_terminal_geometry_changed(const render_ctx_t *ctx);
bool				render_pixel_planes_reliable(const render_ctx_t *ctx);
bool				render_pixels_available(const render_ctx_t *ctx);
bool				render_plane_geometry_matches(struct ncplane *plane, int y,
					int x, unsigned rows, unsigned cols);
bool				render_plane_blit_rgba(render_ctx_t *ctx,
					struct ncplane *plane, const uint32_t *pixels, int width,
					int height, int row_stride);
bool				render_compatibility_mode(const render_ctx_t *ctx);
void				render_compatibility_badge_refresh(render_ctx_t *ctx);
void				render_compatibility_badge_hide(render_ctx_t *ctx);
void				render_notification_show_volume(render_ctx_t *ctx,
					int volume);
void				render_notification_queue_volume(render_ctx_t *ctx,
					int volume);
void				render_notification_queue_ownership(render_ctx_t *ctx);
void				render_notification_tick(render_ctx_t *ctx);
int					render_notification_next_wake_ms(
					const render_ctx_t *ctx);
void				render_notification_reflow(render_ctx_t *ctx);
void				render_notification_raise(render_ctx_t *ctx);
void				render_notification_destroy(render_ctx_t *ctx);
tetrisu_pixel_policy_t	tetrisu_pixel_policy_for(ncpixelimpl_e backend,
					const char *term, tetrisu_renderer_mode_t forced);
bool				tetrisu_pixel_policy_supports_notification_art(
					tetrisu_pixel_policy_t policy);
bool				tetrisu_pixel_policy_notification_needs_reemit(
					tetrisu_pixel_policy_t policy);

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
bool			render_font_mask_load(pixel_asset_t *font);
void			render_font_mask_free(pixel_asset_t *font);

/* RENDER_SCREEN.C */
bool			render_screen_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view);
void			render_screen_destroy(render_ctx_t *ctx);

/* LEADERBOARD_SCREEN.C */
void			leaderboard_state_init(leaderboard_state_t *state);
bool			leaderboard_navigation_keys_coalesce(uint32_t active_key,
					uint32_t queued_key);
bool			leaderboard_action_leaves_screen(leaderboard_action_t action);
leaderboard_action_t	leaderboard_handle_key(leaderboard_state_t *state,
					uint32_t key);
void			leaderboard_set_focus(leaderboard_state_t *state,
					leaderboard_focus_t focus);

/* LEADERBOARD_PRESENTATION.C */
leaderboard_background_action_t	leaderboard_background_action_for(
					tetrisu_pixel_policy_t pixels, bool rebuild_requested);
bool			leaderboard_layout_resolve(int terminal_rows,
					int terminal_cols, bool compatibility,
					leaderboard_layout_t *layout);
void			leaderboard_pixel_layout_build(int origin_y, int origin_x,
					int rows, int cols, int cell_px_y, int cell_px_x,
					leaderboard_pixel_layout_t *layout);
bool			leaderboard_pixel_hit_test(
					const leaderboard_pixel_layout_t *layout,
					int input_y, int input_x, leaderboard_focus_t *focus);
const app_leaderboard_entry_view_model_t	*leaderboard_entry_for_position(
					const app_leaderboard_view_model_t *leaderboard,
					int position);

/* SETTINGS_SCREEN.C */
void			settings_state_init(settings_state_t *state, bool signed_in,
					int character_slots, int theme_slots);
void			settings_state_focus_next(settings_state_t *state);
void			settings_state_focus_previous(settings_state_t *state);
settings_action_t	settings_handle_key(settings_state_t *state,
					uint32_t key);
bool			settings_state_view_changed(const settings_state_t *before,
					const settings_state_t *after);
bool			settings_navigation_keys_coalesce(uint32_t active_key,
					uint32_t queued_key);
bool			settings_action_leaves_screen(settings_action_t action);
int				settings_owned_count(
					const app_catalogue_view_model_t *catalogue, int limit);
int				settings_catalogue_count(
					const app_catalogue_view_model_t *catalogue, int limit);
int				settings_slot_rows(int slots);
bool			settings_select_character(app_settings_view_model_t *settings,
					int direction);
settings_equip_result_t	settings_equip_character_slot(
					app_settings_view_model_t *settings, int slot);
settings_equip_result_t	settings_equip_theme_slot(
					app_settings_view_model_t *settings, int slot);
const app_catalogue_item_view_model_t	*settings_card_character(
					const app_settings_view_model_t *settings,
					const settings_state_t *state);
bool			settings_card_visible(const settings_state_t *state);
void			settings_layout_build(int origin_y, int origin_x, int rows,
					int cols, int cell_px_y, int cell_px_x,
					settings_layout_t *layout);

/* MARKETPLACE_SCREEN.C */
void			marketplace_state_init(marketplace_state_t *state,
					bool signed_in, int character_slots, int theme_slots);
void			marketplace_state_focus_next(marketplace_state_t *state);
void			marketplace_state_focus_previous(marketplace_state_t *state);
marketplace_action_t	marketplace_handle_key(marketplace_state_t *state,
					uint32_t key);
bool			marketplace_state_view_changed(
					const marketplace_state_t *before,
					const marketplace_state_t *after);
bool			marketplace_navigation_keys_coalesce(uint32_t active_key,
					uint32_t queued_key);
bool			marketplace_action_leaves_screen(marketplace_action_t action);
marketplace_section_t	marketplace_focused_section(
					const marketplace_state_t *state);
int				marketplace_focused_slot(const marketplace_state_t *state);
const app_catalogue_view_model_t	*marketplace_focused_catalogue(
					const app_marketplace_view_model_t *market,
					const marketplace_state_t *state);
const app_catalogue_item_view_model_t	*marketplace_focused_item(
					const app_marketplace_view_model_t *market,
					const marketplace_state_t *state);
bool			marketplace_focused_is_character(
					const marketplace_state_t *state);
marketplace_purchase_result_t	marketplace_buy_focused(
					app_marketplace_view_model_t *market,
					const marketplace_state_t *state);
settings_equip_result_t	marketplace_equip_focused(
					app_marketplace_view_model_t *market,
					const marketplace_state_t *state);
bool			marketplace_can_afford(
					const app_marketplace_view_model_t *market,
					const app_catalogue_item_view_model_t *item);
void			marketplace_set_feedback(marketplace_state_t *state,
					marketplace_feedback_t feedback, int value);
const char		*marketplace_feedback_text(const marketplace_state_t *state,
					char *out, size_t size);

/* MARKETPLACE_LAYOUT.C */
void			marketplace_layout_build(int origin_y, int origin_x, int rows,
					int cols, int cell_px_y, int cell_px_x,
					marketplace_layout_t *layout);

/* RENDER_MARKETPLACE.C */
bool			render_marketplace_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view,
					const marketplace_state_t *state, bool rebuild_background);
void			render_marketplace_destroy(render_ctx_t *ctx);

/* RENDER_MARKETPLACE_FONT.C */
bool			render_marketplace_pixel_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view,
					const marketplace_state_t *state, bool rebuild_background);
void			render_marketplace_pixel_destroy(render_ctx_t *ctx);

/* MULTIPLAYER_SCREEN.C */
void			mp_mode_state_init(mp_mode_state_t *state);
mp_mode_action_t	mp_mode_handle_key(mp_mode_state_t *state, uint32_t key);
bool			mp_mode_state_view_changed(const mp_mode_state_t *before,
					const mp_mode_state_t *after);
bool			mp_mode_navigation_keys_coalesce(uint32_t active_key,
					uint32_t queued_key);
bool			mp_mode_action_leaves_screen(mp_mode_action_t action);
app_game_mode_t	mp_mode_focused_mode(const mp_mode_state_t *state);
const char		*mp_mode_card_name(int index);
const char		*mp_mode_card_players(int index);
const char		*mp_mode_card_line(int index, int line);
bool			multiplayer_room_capacity_valid(app_game_mode_t mode,
					int capacity);
void			create_room_state_init(create_room_state_t *state,
					app_game_mode_t mode);
create_room_action_t	create_room_handle_key(create_room_state_t *state,
					uint32_t key);
bool			create_room_state_view_changed(
					const create_room_state_t *before,
					const create_room_state_t *after);
bool			create_room_action_leaves_screen(create_room_action_t action);
int				create_room_focused_index(const create_room_state_t *state);
const char		*mp_feedback_text(mp_mode_feedback_t feedback, int value,
					char *out, size_t size);

/* LOBBY_SCREEN.C */
void			lobby_state_init(lobby_state_t *state, app_game_mode_t filter,
					const app_lobby_view_model_t *lobby);
void			lobby_state_sync(lobby_state_t *state,
					const app_lobby_view_model_t *lobby);
lobby_action_t	lobby_handle_key(lobby_state_t *state, uint32_t key);
bool			lobby_state_view_changed(const lobby_state_t *before,
					const lobby_state_t *after);
bool			lobby_navigation_keys_coalesce(uint32_t active_key,
					uint32_t queued_key);
bool			lobby_action_leaves_screen(lobby_action_t action);
int				lobby_visible_count(const app_lobby_view_model_t *lobby,
					app_game_mode_t filter);
const app_room_summary_view_model_t	*lobby_visible_room(
					const app_lobby_view_model_t *lobby,
					app_game_mode_t filter, int index);
const app_room_summary_view_model_t	*lobby_selected_room(
					const app_lobby_view_model_t *lobby,
					const lobby_state_t *state);
const app_room_summary_view_model_t	*lobby_room_by_id(
					const app_lobby_view_model_t *lobby, const char *id);
lobby_feedback_t	lobby_join_blocker(
					const app_room_summary_view_model_t *room);
const char		*lobby_mode_tag(app_game_mode_t mode);
const char		*lobby_state_tag(app_room_state_t state);
const char		*lobby_filter_name(app_game_mode_t filter);
void			lobby_set_feedback(lobby_state_t *state,
					lobby_feedback_t feedback, int value);
const char		*lobby_feedback_text(const lobby_state_t *state, char *out,
					size_t size);

/* WAITING_ROOM_SCREEN.C */
void			waiting_room_state_init(waiting_room_state_t *state);
room_action_t	waiting_room_handle_key(waiting_room_state_t *state,
					const app_room_view_model_t *room, uint32_t key);
bool			waiting_room_state_view_changed(
					const waiting_room_state_t *before,
					const waiting_room_state_t *after);
bool			waiting_room_navigation_keys_coalesce(uint32_t active_key,
					uint32_t queued_key);
bool			waiting_room_action_leaves_screen(room_action_t action);
int				waiting_room_ready_count(const app_room_view_model_t *room);
int				waiting_room_required_ready(const app_room_view_model_t *room);
int				waiting_room_slot_count(const app_room_view_model_t *room);
int				waiting_room_visible_slot_count(
					const app_room_view_model_t *room);
bool			waiting_room_can_start(const app_room_view_model_t *room);
bool			waiting_room_auto_start_allowed(
					const app_room_view_model_t *room);
bool			waiting_room_sync_state(app_room_view_model_t *room);
bool			waiting_room_local_is_owner(const app_room_view_model_t *room);
bool			waiting_room_local_ready(const app_room_view_model_t *room);
bool			waiting_room_toggle_ready(app_room_view_model_t *room);
room_feedback_t	waiting_room_start_blocker(const app_room_view_model_t *room);
bool			waiting_room_begin_countdown(waiting_room_state_t *state);
bool			waiting_room_cancel_countdown(waiting_room_state_t *state);
bool			waiting_room_tick(waiting_room_state_t *state);
bool			waiting_room_append_chat(app_room_view_model_t *room,
					const char *author, const char *text, bool system);
bool			waiting_room_send_chat(app_room_view_model_t *room,
					waiting_room_state_t *state);
const char		*waiting_room_status_text(const app_room_view_model_t *room,
					const waiting_room_state_t *state, char *out, size_t size);
const char		*waiting_room_slot_label(const app_room_view_model_t *room,
					int index, char *out, size_t size);
const char		*waiting_room_badge_text(const app_room_view_model_t *room,
					int index);
const char		*waiting_room_feedback_text(const waiting_room_state_t *state,
					char *out, size_t size);
app_nav_action_t	waiting_room_launch_action(
					const app_room_view_model_t *room);

/* MULTIPLAYER_LAYOUT.C */
void			mp_layout_build(app_screen_t screen, int origin_y, int origin_x,
					int rows, int cols, int cell_px_y, int cell_px_x,
					mp_layout_t *layout);

/* RENDER_MULTIPLAYER.C */
bool			render_mp_mode_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view,
					const mp_mode_state_t *state, bool rebuild_background);
bool			render_lobby_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view,
					const lobby_state_t *state, bool rebuild_background);
bool			render_create_room_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view,
					const create_room_state_t *state, bool rebuild_background);
bool			render_waiting_room_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view,
					const waiting_room_state_t *state, bool rebuild_background);
void			render_multiplayer_destroy(render_ctx_t *ctx);

/* RENDER_MULTIPLAYER_FONT.C */
bool			render_mp_pixel_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view, const void *state,
					bool rebuild_background);
void			render_mp_pixel_destroy(render_ctx_t *ctx);

/* RENDER_LEADERBOARD.C */
bool			render_leaderboard_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view,
					const leaderboard_state_t *state,
					bool rebuild_background);
bool			render_leaderboard_hit_test(const render_ctx_t *ctx,
					const ncinput *input, leaderboard_focus_t *focus);
void			render_leaderboard_destroy(render_ctx_t *ctx);
bool			render_leaderboard_pixel_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view,
					const leaderboard_state_t *state,
					bool rebuild_background);
void			render_leaderboard_pixel_destroy(render_ctx_t *ctx);

/* RENDER_SETTINGS.C */
bool			render_settings_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view,
					const settings_state_t *state,
					bool rebuild_background);
void			render_settings_destroy(render_ctx_t *ctx);
bool			render_settings_pixel_show(render_ctx_t *ctx,
					const app_screen_view_model_t *view,
					const settings_state_t *state, bool rebuild_background);
void			render_settings_pixel_destroy(render_ctx_t *ctx);

/* RENDER_AUTH.C */
bool			render_auth_show(render_ctx_t *ctx, const auth_form_t *form,
					bool rebuild_background);
bool			render_auth_hit_test(const render_ctx_t *ctx,
					const auth_form_t *form, const ncinput *input,
					auth_focus_t *focus);
void			render_auth_destroy(render_ctx_t *ctx);
bool			render_auth_pixel_background_refresh(render_ctx_t *ctx,
					const auth_form_t *form, bool force);
void			render_auth_pixel_background_reset(render_ctx_t *ctx);
bool			render_auth_pixel_overlay_refresh(render_ctx_t *ctx,
					const auth_form_t *form);
void			render_auth_pixel_overlay_destroy(render_ctx_t *ctx);

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
void			audio_play_room_entry(audio_ctx_t *audio);
void			audio_set_music_volume(audio_ctx_t *audio, int volume);
void			audio_apply_effect_volume(audio_ctx_t *audio);
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
