/*
 * wlterm - configuration
 *
 * Copyright (c) 2017 Pamelloes <pamelloes@gmail.com>
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
 * Configuration
 * This file contains the code for parsing configuration options
 * from the command line and possibly from a file in the future.
 */

#include <cairo.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <string.h>
#include "wlterm.h"

struct wlt_config {
	unsigned long ref;
	gboolean show_dirty;
	gboolean snap_size;
	gint sb_size;

	gchar *font_name;
	gint font_size;
	gboolean no_bold;
	gboolean no_underline;
	gboolean no_italics;
};

const char DEFAULT_FONT[] = "monospace";

int wlt_config_new(struct wlt_config **out, int argc, char **argv)
{
	struct wlt_config *config;
	GOptionContext *opt;
	GError *e = NULL;
	int r;

	config = calloc(1, sizeof(*config));
	if (!config)
		return -ENOMEM;
	config->ref = 1;

	// Default values
	config->sb_size = 2000;
	config->font_size = 10;

	GOptionEntry opts[] = {
		{ "show-dirty", 0, 0, G_OPTION_ARG_NONE, &config->show_dirty, "Mark dirty cells during redraw", NULL },
		{ "snap-size", 0, 0, G_OPTION_ARG_NONE, &config->snap_size, "Snap to next cell-size when resizing", NULL },
		{ "sb-size", 0, 0, G_OPTION_ARG_INT, &config->sb_size, "Scroll-back buffer size in lines", NULL },
		{ "font", 0, 0, G_OPTION_ARG_STRING, &config->font_name, "Typeface name; defaults to 'monospace'", NULL },
		{ "font-size", 0, 0, G_OPTION_ARG_INT, &config->font_size, "Font size; defaults to 10", NULL },
		{ "no-bold", 0, 0, G_OPTION_ARG_NONE, &config->no_bold, "Disable bold text", NULL },
		{ "no-underline", 0, 0, G_OPTION_ARG_NONE, &config->no_underline, "Disable undelined text", NULL },
		{ "no-italics", 0, 0, G_OPTION_ARG_NONE, &config->no_italics, "Disable italicized text", NULL },
		{ NULL }
	};

	opt = g_option_context_new("- Wayland Terminal Emulator");
	g_option_context_add_main_entries(opt, opts, NULL);
	g_option_context_add_group(opt, gtk_get_option_group(TRUE));
	if (!g_option_context_parse(opt, &argc, &argv, &e)) {
		g_print("cannot parse arguments: %s\n", e->message);
		g_error_free(e);
		r = -EINVAL;
		goto error;
	}

	// Since font_name gets a dynamically allocated
	// string if allocated made using g_malloc, we have
	// to do the same for the default vallue to make cleanup
	// consistant.
	if (!config->font_name) {
		config->font_name = g_malloc(sizeof(DEFAULT_FONT));
		memcpy(config->font_name, DEFAULT_FONT, sizeof(DEFAULT_FONT));
	}

	*out = config;
	return 0;

	error:
		g_free(config->font_name);
		free(config);
		return r;
}

void wlt_config_ref(struct wlt_config *config)
{
	if (!config || !config->ref)
		return;

	++config->ref;
}

void wlt_config_unref(struct wlt_config *config)
{
	if (!config || !config->ref || --config->ref)
		return;

	g_free(config->font_name);
	free(config);
}

bool wlt_config_get_show_dirty(struct wlt_config *config)
{
	return config->show_dirty;
}

bool wlt_config_get_snap_size(struct wlt_config *config)
{
	return config->snap_size;
}

int wlt_config_get_sb_size(struct wlt_config *config)
{
	return config->sb_size;
}

const char *wlt_config_get_font_name(struct wlt_config *config)
{
	return config->font_name;
}

int wlt_config_get_font_size(struct wlt_config *config)
{
	return config->font_size;
}

bool wlt_config_get_no_bold(struct wlt_config *config)
{
	return config->no_bold;
}

bool wlt_config_get_no_underline(struct wlt_config *config)
{
	return config->no_underline;
}

bool wlt_config_get_no_italics(struct wlt_config *config)
{
	return config->no_italics;
}
