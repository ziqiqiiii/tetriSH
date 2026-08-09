# ifndef TETRISBRAIN_H
# define TETRISBRAIN_H

# include <stdbool.h>
# include <stdint.h>
# include <stdlib.h>
# include <string.h>

# define BOARD_WIDTH			10
# define BOARD_HEIGHT			20
# define BRAIN_BAG_SIZE			7
# define BRAIN_MAX_CLEAR_LINES	4
# define LINES_PER_CHARGE		2
# define BAG_FALLBACK_SEED		0x6D2B79F5u
# define LOCKDOWN_DELAY_MS		500
# define LOCKDOWN_MAX_RESETS	15

/* what's in a cell */
typedef enum
{
	CELL_EMPTY		= 0,
	CELL_FILLED		= 1,
	CELL_GARBAGE	= 2
}	t_cell_type;

typedef struct
{
	t_cell_type	type;
	uint8_t		color;
}	t_cell;

typedef struct
{
	t_cell	cells[BOARD_HEIGHT][BOARD_WIDTH];
}	t_board;

/* 7 tetromino types */
typedef enum
{
	PIECE_I,
	PIECE_O,
	PIECE_T,
	PIECE_S,
	PIECE_Z,
	PIECE_J,
	PIECE_L
}	t_piece_type;

typedef struct
{
	t_piece_type	type;
	int				col;
	int				row;
	int				rotation;
}	t_piece;

/* Guideline Extended Placement lock down: a piece that has landed is still
 * the player's for LOCKDOWN_DELAY_MS, and every move or rotation buys that
 * half-second back - up to LOCKDOWN_MAX_RESETS times, refilled whenever the
 * piece falls past the lowest row it has occupied so far. Without the cap a
 * player could hold a piece above the stack forever. */
typedef struct
{
	int	elapsed_ms;
	int	resets;
	int	lowest_row;
}	t_lockdown;

/* row-down (col, row) offset from a piece's anchor; used by pieces.c's
 * per-rotation shape and wall-kick tables */
typedef struct
{
	int8_t	dcol;
	int8_t	drow;
}	t_cell_offset;

/* caller-owned 7-bag state; no global RNG is used */
typedef struct
{
	t_piece_type	pieces[BRAIN_BAG_SIZE];
	uint8_t			next;
	uint32_t		rng_state;
}	t_piece_bag;

typedef enum
{
	T_SPIN_NONE,
	T_SPIN_MINI,
	T_SPIN_FULL
}	t_spin_type;

/* Battle Royale targeting modes. The values are the wire representation
 * carried in HTTTP bodies, so they are pinned explicitly and must not be
 * reordered. */
typedef enum
{
	TARGET_RANDOM		= 0,
	TARGET_ATTACKERS	= 1,
	TARGET_KO			= 2,
	TARGET_TOP_SCORE	= 3
}	t_target_mode;

typedef struct
{
	uint64_t	total;
	int			combo;
	bool		back_to_back;
}	t_score_state;

typedef struct
{
	uint64_t	action_points;
	uint64_t	combo_points;
	uint64_t	perfect_clear_points;
	uint64_t	total_awarded;
	bool		difficult;
}	t_score_result;

/* UC-14 ability meter: every two cleared lines bank one charge */
typedef struct
{
	int	charges;
	int	line_remainder;
}	t_charge_state;

/* Gaiden status effects that outlive the activating request */
typedef enum
{
	EFFECT_PARALYSIS,
	EFFECT_INVERSION,
	EFFECT_NUE,
	EFFECT_THWACK,
	EFFECT_FRY,
	EFFECT_DARK,
	EFFECT_PALS,
	EFFECT_MIRROR
}	t_status_effect;

/* per-player status state; counters run down as that player's pieces lock */
typedef struct
{
	int		no_rotate_pieces;
	int		inverted_pieces;
	int		no_fastdrop_pieces;
	int		thwack_pieces;
	int		fry_rows;
	bool	blackout;
	bool	pals;
	bool	mirror_armed;
}	t_effect_state;

/* return codes */
typedef enum
{
	BRAIN_OK,
	BRAIN_BLOCKED,
	BRAIN_LOCKED,
	BRAIN_GAME_OVER,
	BRAIN_CLEARED
}	t_brain_result;

/* BOARD.C */
void			board_init(t_board *b);
t_cell			board_get(const t_board *b, int col, int row);
void			board_set(t_board *b, int col, int row, t_cell cell);
bool			board_in_bounds(int col, int row);
void			board_inject_garbage(t_board *b, int lines, int hole_col);
void			board_copy(t_board *dst, const t_board *src);

/* PIECES.C */
t_piece			piece_spawn(t_piece_type type);
bool			piece_is_valid(const t_board *b, const t_piece *p);
bool			piece_cells(const t_piece *p, int cols[4], int rows[4]);
t_brain_result	piece_move(const t_board *b, t_piece *p, int dcol, int drow);
t_brain_result	piece_rotate(const t_board *b, t_piece *p, int dir);
t_brain_result	piece_rotate_with_kick(const t_board *b, t_piece *p, int dir, int *kick_index);
t_spin_type		piece_t_spin_type(const t_board *b, const t_piece *p, int kick_index);
void			piece_stamp(t_board *b, const t_piece *p);

/* BAG.C */
void			piece_bag_init(t_piece_bag *bag, uint32_t seed);
t_piece_type	piece_bag_next(t_piece_bag *bag);

/* GRAVITY.C */
t_brain_result	gravity_tick(const t_board *b, t_piece *p);
t_brain_result	piece_soft_drop(const t_board *b, t_piece *p);
void			piece_hard_drop(const t_board *b, t_piece *p);
int				piece_drop_distance(const t_board *b, const t_piece *p);

/* LOCKDOWN.C */
void			lockdown_init(t_lockdown *lock, const t_piece *p);
bool			lockdown_grounded(const t_board *b, const t_piece *p);
void			lockdown_on_fall(t_lockdown *lock, const t_piece *p);
void			lockdown_on_shift(t_lockdown *lock, bool was_grounded);
bool			lockdown_tick(t_lockdown *lock, bool grounded, int elapsed_ms);

/* LINECLEAR.C */
int				board_clear_lines(t_board *b);
int				board_find_full_lines(const t_board *b, int rows[BRAIN_MAX_CLEAR_LINES]);
bool			board_is_empty(const t_board *b);

/* SCORING.C */
int				score_on_clear(int lines_cleared, int level);
int				level_from_lines(int total_lines);
int				gravity_interval_ms(int level);
int				clear_duration_ms(int level);
void			score_state_init(t_score_state *state);
t_score_result	score_apply_clear(t_score_state *state, int lines_cleared, int level, t_spin_type spin, bool perfect_clear);
uint64_t		score_add_drop(t_score_state *state, int cells, bool hard_drop);

/* ABILITIES.C */
void			board_cut_top(t_board *b, int n);
void			board_cut_bottom(t_board *b, int n);
void			board_apply_gravity(t_board *b);
void			board_invert(t_board *b);
void			board_fill_rows(t_board *b, int n, int hole_col);
void			board_clear_cells(t_board *b, int cols[], int rows[], int count);
void			board_delete_columns(t_board *b, int start_col, int end_col);
int				board_cascade_clear(t_board *b);

/* CHARGE.C */
void			charge_state_init(t_charge_state *state);
void			charge_on_clear(t_charge_state *state, int lines_cleared);
int				ability_cost(int level);
bool			charge_can_afford(const t_charge_state *state, int level);
bool			charge_deduct(t_charge_state *state, int level);
void			charge_transfer(t_charge_state *from, t_charge_state *to);

/* EFFECTS.C */
void			effect_state_init(t_effect_state *state);
void			effect_apply(t_effect_state *state, t_status_effect effect);
void			effect_clear(t_effect_state *state, t_status_effect effect);
void			effect_on_piece_lock(t_effect_state *state);
bool			effect_rotation_blocked(const t_effect_state *state);
bool			effect_fastdrop_blocked(const t_effect_state *state);
bool			effect_controls_inverted(const t_effect_state *state);
bool			effect_thwack_active(const t_effect_state *state);
int				effect_fry_rows(const t_effect_state *state);

/* GARBAGE.C */
int				garbage_lines_from_clear(int lines_cleared);

# endif
