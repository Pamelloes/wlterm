/*
 * wlterm - Wayland Terminal
 *
 * Copyright (c) 2011-2013 David Herrmann <dh.herrmann@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Terminal Rendering
 */

#include <cairo.h>
#include <errno.h>
#include <libtsm.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wlterm.h"

static int wlt_render_cell(struct tsm_screen *screen, uint32_t id,
			   const uint32_t *ch, size_t len, unsigned int cwidth,
			   unsigned int posx, unsigned int posy,
			   const struct tsm_screen_attr *attr, void *data)
{
	struct wlt_draw_ctx *ctx = data;
	cairo_t *cr = ctx->cr;
	struct wlt_glyph *glyph;
	int r;

	cairo_rectangle(cr, posx * ctx->cell_width, posy * ctx->cell_height,
			ctx->cell_width, ctx->cell_height);

	/* draw bg */
	cairo_set_source_rgb(cr,
			     attr->br / (double)256,
			     attr->bg / (double)256,
			     attr->bb / (double)256);
	cairo_fill(cr);

	/* draw fg */
	if (len) {
		r = wlt_face_render(ctx->face, &glyph, id, ch, len, cwidth);
		if (r < 0)
			return r;

		cairo_set_source_rgb(cr,
				     attr->fr / (double)256,
				     attr->fg / (double)256,
				     attr->fb / (double)256);
		cairo_mask_surface(cr, glyph->cr_surface,
				   posx * glyph->width,
				   posy * glyph->height);
	}

	return 0;
}

static void wlt_render_debug(struct wlt_draw_ctx *ctx)
{
	cairo_t *cr = ctx->cr;
	double x1, x2, y1, y2;

	if (1)
		return;

	cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
	cairo_rectangle(cr, x1, y1, x2, y2);
	cairo_set_source_rgb(cr, 1, 0, 0);
	cairo_stroke(cr);
}

void wlt_render(struct wlt_draw_ctx *ctx)
{
	tsm_screen_draw(ctx->screen, NULL, wlt_render_cell, NULL, ctx);
	wlt_render_debug(ctx);
}
