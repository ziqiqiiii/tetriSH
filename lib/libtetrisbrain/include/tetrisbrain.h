#ifndef TETRISBRAIN_H
# define TETRISBRAIN_H

# include <stdbool.h>
# include <stdint.h>
# include <stdlib.h>

# define BOARD_WIDTH	10
# define BOARD_HEIGHT	20
# define BRAIN_BAG_SIZE	7
# define BRAIN_MAX_CLEAR_LINES	4

/* what's in a cell */
typedef enum e_cell_type
{
	CELL_EMPTY = 0,
	CELL_FILLED = 1,
	CELL_GARBAGE = 2
}	t_cell_type;

typedef struct s_cell
{
	t_cell_type	type;
	uint8_t		color;
}	t_cell;

typedef struct s_board
{
	t_cell	cells[BOARD_HEIGHT][BOARD_WIDTH];
}	t_board;

/* 7 tetromino types */
typedef enum e_piece_type
{
	PIECE_I,
	PIECE_O,
	PIECE_T,
	PIECE_S,
	PIECE_Z,
	PIECE_J,
	PIECE_L
}	t_piece_type;

typedef struct s_piece
{
	t_piece_type	type;
	int				col;
	int				row;
	int				rotation;
}	t_piece;

/* caller-owned 7-bag state; no global RNG is used */
typedef struct s_piece_bag
{
	t_piece_type	pieces[BRAIN_BAG_SIZE];
	uint8_t		next;
	uint32_t	rng_state;
}	t_piece_bag;

typedef enum e_t_spin_type
{
	T_SPIN_NONE,
	T_SPIN_MINI,
	T_SPIN_FULL
}	t_spin_type;

typedef struct s_score_state
{
	uint64_t	total;
	int			combo;
	bool		back_to_back;
}	t_score_state;

typedef struct s_score_result
{
	uint64_t	action_points;
	uint64_t	combo_points;
	uint64_t	perfect_clear_points;
	uint64_t	total_awarded;
	bool		difficult;
}	t_score_result;

/* return codes */
typedef enum e_brain_result
{
	BRAIN_OK,
	BRAIN_BLOCKED,
	BRAIN_LOCKED,
	BRAIN_GAME_OVER,
	BRAIN_CLEARED
}	t_brain_result;

/* BOARD */

void			board_init(t_board *b);
t_cell			board_get(const t_board *b, int col, int row);
void			board_set(t_board *b, int col, int row, t_cell cell);
bool			board_in_bounds(int col, int row);
void			board_inject_garbage(t_board *b, int lines, int hole_col);
void			board_copy(t_board *dst, const t_board *src);

/* PIECES */

t_piece			piece_spawn(t_piece_type type);
bool			piece_is_valid(const t_board *b, const t_piece *p);
bool			piece_cells(const t_piece *p, int cols[4], int rows[4]);
t_brain_result	piece_move(const t_board *b, t_piece *p, int dcol, int drow);
t_brain_result	piece_rotate(const t_board *b, t_piece *p, int dir);
t_brain_result	piece_rotate_with_kick(const t_board *b, t_piece *p, int dir,
					int *kick_index);
t_spin_type	piece_t_spin_type(const t_board *b, const t_piece *p,
					int kick_index);
void			piece_stamp(t_board *b, const t_piece *p);

/* RANDOM GENERATOR */

void			piece_bag_init(t_piece_bag *bag, uint32_t seed);
t_piece_type	piece_bag_next(t_piece_bag *bag);

/* GRAVITY */

t_brain_result	gravity_tick(const t_board *b, t_piece *p);
t_brain_result	piece_soft_drop(const t_board *b, t_piece *p);
void			piece_hard_drop(const t_board *b, t_piece *p);
int				piece_drop_distance(const t_board *b, const t_piece *p);

/* LINECLEAR */

int				board_clear_lines(t_board *b);
int				board_find_full_lines(const t_board *b,
					int rows[BRAIN_MAX_CLEAR_LINES]);
bool			board_is_empty(const t_board *b);

/* SCORING */

int				score_on_clear(int lines_cleared, int level);
int				level_from_lines(int total_lines);
int				gravity_interval_ms(int level);
void			score_state_init(t_score_state *state);
t_score_result	score_apply_clear(t_score_state *state, int lines_cleared,
					int level, t_spin_type spin, bool perfect_clear);
uint64_t		score_add_drop(t_score_state *state, int cells, bool hard_drop);

/* ABILITIES */

void			board_cut_top(t_board *b, int n);
void			board_cut_bottom(t_board *b, int n);
void			board_apply_gravity(t_board *b);
void			board_invert(t_board *b);
void			board_fill_rows(t_board *b, int n, int hole_col);
void			board_clear_cells(t_board *b, int cols[], int rows[], int count);
void			board_delete_columns(t_board *b, int start_col, int end_col);

#endif
