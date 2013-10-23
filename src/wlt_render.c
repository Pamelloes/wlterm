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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wlterm.h"

struct wlt_renderer {
	unsigned int width;
	unsigned int height;
	int stride;
	uint8_t *data;
	cairo_surface_t *surface;
};

static int wlt_renderer_realloc(struct wlt_renderer *rend, unsigned int width,
				unsigned int height)
{
	int stride;
	uint8_t *data;
	cairo_surface_t *surface;

	stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	data = malloc(abs(stride * height));
	if (!data)
		return -ENOMEM;

	surface = cairo_image_surface_create_for_data(data,
						      CAIRO_FORMAT_ARGB32,
						      width,
						      height,
						      stride);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		free(data);
		return -ENOMEM;
	}

	if (rend->data) {
		cairo_surface_destroy(rend->surface);
		free(rend->data);
	}

	rend->width = width;
	rend->height = height;
	rend->stride = stride;
	rend->data = data;
	rend->surface = surface;
	return 0;
}

int wlt_renderer_new(struct wlt_renderer **out, unsigned int width,
		     unsigned int height)
{
	struct wlt_renderer *rend;
	int r;

	rend = calloc(1, sizeof(*rend));
	if (!rend)
		return -ENOMEM;

	r = wlt_renderer_realloc(rend, width, height);
	if (r < 0)
		goto err_rend;

	*out = rend;
	return 0;

err_rend:
	free(rend);
	return r;
}

void wlt_renderer_free(struct wlt_renderer *rend)
{
	if (!rend)
		return;

	cairo_surface_destroy(rend->surface);
	free(rend->data);
	free(rend);
}

int wlt_renderer_resize(struct wlt_renderer *rend, unsigned int width,
			unsigned int height)
{
	return wlt_renderer_realloc(rend, width, height);
}

static void wlt_renderer_fill(struct wlt_renderer *rend,
			      unsigned int x, unsigned int y,
			      unsigned int width, unsigned int height,
			      uint8_t br, uint8_t bg, uint8_t bb)
{
	unsigned int i, tmp;
	uint8_t *dst;
	uint32_t out;

	/* clip width */
	tmp = x + width;
	if (tmp <= x || x >= rend->width)
		return;
	if (tmp > rend->width)
		width = rend->width - x;

	/* clip height */
	tmp = y + height;
	if (tmp <= y || y >= rend->height)
		return;
	if (tmp > rend->height)
		height = rend->height - y;

	/* prepare */
	dst = rend->data;
	dst = &dst[y * rend->stride + x * 4];
	out = (0xff << 24) | (br << 16) | (bg << 8) | bb;

	/* fill buffer */
	while (height--) {
		for (i = 0; i < width; ++i)
			((uint32_t*)dst)[i] = out;

		dst += rend->stride;
	}
}

static void wlt_renderer_blend(struct wlt_renderer *rend,
			       const struct wlt_glyph *glyph,
			       unsigned int x, unsigned int y,
			       uint8_t fr, uint8_t fg, uint8_t fb,
			       uint8_t br, uint8_t bg, uint8_t bb)
{
	unsigned int i, tmp, width, height;
	const uint8_t *src;
	uint8_t *dst;
	uint32_t out;
	uint_fast32_t r, g, b;

	/* clip width */
	tmp = x + glyph->width;
	if (tmp <= x || x >= rend->width)
		return;
	if (tmp > rend->width)
		width = rend->width - x;
	else
		width = glyph->width;

	/* clip height */
	tmp = y + glyph->height;
	if (tmp <= y || y >= rend->height)
		return;
	if (tmp > rend->height)
		height = rend->height - y;
	else
		height = glyph->height;

	/* prepare */
	dst = rend->data;
	dst = &dst[y * rend->stride + x * 4];
	src = glyph->buffer;

	/* blend buffer */
	while (height--) {
		for (i = 0; i < width; ++i) {
			if (src[i] == 0) {
				r = br;
				g = bg;
				b = bb;
			} else if (src[i] == 255) {
				r = fr;
				g = fg;
				b = fb;
			} else {
				/* Division by 255 (t /= 255) is done with:
				 *   t += 0x80
				 *   t = (t + (t >> 8)) >> 8
				 * This speeds up the computation by ~20% as
				 * the division is skipped. */
				r = fr * src[i] + br * (255 - src[i]);
				r += 0x80;
				r = (r + (r >> 8)) >> 8;

				g = fg * src[i] + bg * (255 - src[i]);
				g += 0x80;
				g = (g + (g >> 8)) >> 8;

				b = fb * src[i] + bb * (255 - src[i]);
				b += 0x80;
				b = (b + (b >> 8)) >> 8;
			}

			out = (0xff << 24) | (r << 16) | (g << 8) | b;
			((uint32_t*)dst)[i] = out;
		}

		dst += rend->stride;
		src += glyph->stride;
	}
}

static int wlt_renderer_draw_cell(struct tsm_screen *screen, uint32_t id,
				  const uint32_t *ch, size_t len,
				  unsigned int cwidth, unsigned int posx,
				  unsigned int posy,
				  const struct tsm_screen_attr *attr,
				  tsm_age_t age, void *data)
{
	const struct wlt_draw_ctx *ctx = data;
	uint8_t fr, fg, fb, br, bg, bb;
	unsigned int x, y;
	struct wlt_glyph *glyph;
	int r;

	/* invert colors if requested */
	if (attr->inverse) {
		fr = attr->br;
		fg = attr->bg;
		fb = attr->bb;
		br = attr->fr;
		bg = attr->fg;
		bb = attr->fb;
	} else {
		fr = attr->fr;
		fg = attr->fg;
		fb = attr->fb;
		br = attr->br;
		bg = attr->bg;
		bb = attr->bb;
	}

	x = posx * ctx->cell_width;
	y = posy * ctx->cell_height;

	/* !len means background-only */
	if (!len) {
		wlt_renderer_fill(ctx->rend, x, y, ctx->cell_width * cwidth,
				  ctx->cell_height, br, bg, bb);
		return 0;
	}

	r = wlt_face_render(ctx->face, &glyph, id, ch, len, cwidth);
	if (r < 0)
		return r;

	wlt_renderer_blend(ctx->rend, glyph, x, y,
			   fr, fg, fb, br, bg, bb);

	return 0;
}

void wlt_renderer_draw(const struct wlt_draw_ctx *ctx)
{
	struct wlt_renderer *rend = ctx->rend;

	/* cairo is *way* too slow to render all masks efficiently. Therefore,
	 * we render all glyphs into a shadow buffer on the CPU and then tell
	 * cairo to blit it into the gtk buffer. This way we get two mem-writes
	 * but at least it's fast enough to render a whole screen. */

	cairo_surface_flush(rend->surface);
	tsm_screen_draw(ctx->screen, wlt_renderer_draw_cell, (void*)ctx);
	cairo_surface_mark_dirty(rend->surface);

	cairo_set_source_surface(ctx->cr, rend->surface, 0, 0);
	cairo_paint(ctx->cr);
}
