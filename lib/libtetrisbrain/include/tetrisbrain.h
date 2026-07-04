#ifndef TETRISBRAIN_H
# define TETRISBRAIN_H

# include <stdbool.h>
# include <stdint.h>
# include <stdlib.h>

# define BOARD_WIDTH	10
# define BOARD_HEIGHT	20

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
t_brain_result	piece_move(const t_board *b, t_piece *p, int dcol, int drow);
t_brain_result	piece_rotate(const t_board *b, t_piece *p, int dir);
void			piece_stamp(t_board *b, const t_piece *p);

/* GRAVITY */

t_brain_result	gravity_tick(const t_board *b, t_piece *p);
t_brain_result	piece_soft_drop(const t_board *b, t_piece *p);
void			piece_hard_drop(const t_board *b, t_piece *p);

/* LINECLEAR */

int				board_clear_lines(t_board *b);

/* SCORING */

int				score_on_clear(int lines_cleared, int level);
int				level_from_lines(int total_lines);
int				gravity_interval_ms(int level);

/* ABILITIES */

void			board_cut_top(t_board *b, int n);
void			board_cut_bottom(t_board *b, int n);
void			board_apply_gravity(t_board *b);
void			board_invert(t_board *b);
void			board_fill_rows(t_board *b, int n, int hole_col);
void			board_clear_cells(t_board *b, int cols[], int rows[], int count);
void			board_delete_columns(t_board *b, int start_col, int end_col);

#endif
