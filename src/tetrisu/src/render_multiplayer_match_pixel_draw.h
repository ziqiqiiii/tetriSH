#ifndef RENDER_MULTIPLAYER_MATCH_PIXEL_DRAW_H
# define RENDER_MULTIPLAYER_MATCH_PIXEL_DRAW_H

# include "tetrisu.h"

bool	mp_match_pixel_load_portrait(t_render_ctx *ctx, const char *path);
void	mp_match_pixel_draw_visual(uint32_t *pixels, int width, int height,
			struct ncvisual *visual, const t_mp_rect *rect);
void	mp_match_pixel_draw_portrait(t_render_ctx *ctx, uint32_t *pixels,
			int width, int height, const t_mp_rect *rect);
bool	mp_match_pixel_load_popover(t_render_ctx *ctx, const char *path);
void	mp_match_pixel_draw_popover_art(t_render_ctx *ctx, uint32_t *pixels,
			int width, int height, const t_mp_rect *rect);
void	mp_match_pixel_draw_panel(uint32_t *pixels, int width, int height,
			const t_mp_rect *rect, t_color edge, unsigned alpha);
void	mp_match_pixel_draw_circle(uint32_t *pixels, int width, int height,
			int center_x, int center_y, int radius, t_color tint, unsigned alpha);
void	mp_match_pixel_fill_rect(uint32_t *pixels, int width, int height,
			const t_mp_rect *rect, t_color tint, unsigned alpha);
void	mp_match_pixel_outline_rect(uint32_t *pixels, int width, int height,
			const t_mp_rect *rect, int thickness, t_color tint, unsigned alpha);
void	mp_match_pixel_draw_text_box(t_render_ctx *ctx, uint32_t *pixels,
			int width, int height, const char *text, const t_mp_rect *rect,
			int preferred, t_color tint, bool centered);

#endif
