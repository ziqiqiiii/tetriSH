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

# define SPLASH_ASSET_PATH	ASSET_DIR "/homepage.png"
# define BUNNY_ASSET_PATH	ASSET_DIR "/bunny_ghost_pointer.png"
# define MENU_ITEM_COUNT	4

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

/* APP_STATE.C */
app_state_t		app_handle_key(app_state_t current, uint32_t key);
void			menu_move_selection(menu_selection_t *m, uint32_t key);
const char		*menu_item_label(int index);
const char		*menu_stub_text(int selected_index);

/* RENDER_BACKGROUND.C */
render_ctx_t	render_init(const char *image_path);
uint32_t		render_wait_key(render_ctx_t *ctx);
void			render_teardown(render_ctx_t *ctx);

/* RENDER_MENU.C */
void			render_menu_create(render_ctx_t *ctx);
void			render_menu_move_bunny(render_ctx_t *ctx, const menu_selection_t *m);
void			render_menu_show_message(render_ctx_t *ctx, const char *msg);

# endif
