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
 * from the command line and from various files accross the system.
 * The settings are initially populated using hard coded defaults.
 * Then, we try to find a config file and, in the event one is found,
 * override the default settings with those specified in the config
 * file. Finally, any settings specified on the command line take
 * highest precedence and will override those in the config file.
 *
 * To find the config file, we search for wlterm/wlterm.conf in
 * $XDG_CONFIG_HOME followed by the directories in $XDF_CONFIG_DIRS
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
	gboolean bold;
	gboolean underline;
	gboolean italics;

	gchar *palette;
};

const char DEFAULT_FONT[] = "monospace";

static void load_config_file(struct wlt_config *conf, char *fname)
{
}

// Loads in all of the settings. To handle the two stage loading (file
// then command line), we need to use some indirection.
static int init_config(struct wlt_config *config, int argc, char **argv)
{
	GOptionContext *opt;
	GError *e = NULL;

	char *cfg = NULL;

	int show_dirty = 2;
	int snap_size = 2;
	int sb_size = -1;

	char *font_name = NULL;
	int font_size = 0;
	int bold = 2;
	int underline = 2;
	int italics = 2;

	char *palette = NULL;

	GOptionEntry opts[] = {
		{ "config",        'c', G_OPTION_FLAG_NONE,    G_OPTION_ARG_FILENAME, 
			&cfg,        "Specify the configuration file",         NULL},

		{ "show-dirty",    0,   G_OPTION_FLAG_NONE,    G_OPTION_ARG_NONE, 
			&show_dirty, "Mark dirty cells during redraw",         NULL },
		{ "no-show-dirty", 0,   G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, 
			&show_dirty, "Don't mark dirty cells during redraw",   NULL },
		{ "snap-size",     0,   G_OPTION_FLAG_NONE,    G_OPTION_ARG_NONE, 
			&snap_size,  "Snap to next cell-size when resizing",   NULL },
		{ "no-snap-size",  0,   G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, 
			&snap_size,  "Snap to next cell-size when resizing",   NULL },
		{ "sb-size",       0,   G_OPTION_FLAG_NONE,    G_OPTION_ARG_INT, 
			&sb_size,    "Scroll-back buffer size in lines",       NULL },

		{ "font",          0,   G_OPTION_FLAG_NONE,    G_OPTION_ARG_STRING, 
			&font_name,  "Typeface name; defaults to 'monospace'", NULL },
		{ "font-size",     0,   G_OPTION_FLAG_NONE,    G_OPTION_ARG_INT, 
			&font_size,  "Font size; defaults to 10",              NULL },
		{ "bold",          'b', G_OPTION_FLAG_NONE,    G_OPTION_ARG_NONE, 
			&bold,       "Enable bold text",                       NULL },
		{ "no-bold",       0,   G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, 
			&bold,       "Disable bold text",                      NULL },
		{ "underline",     'u', G_OPTION_FLAG_NONE,    G_OPTION_ARG_NONE, 
			&underline,  "Enable undelined text",                  NULL },
		{ "no-underline",  0,   G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, 
			&underline,  "Disable undelined text",                 NULL },
		{ "italics",       'i', G_OPTION_FLAG_NONE,    G_OPTION_ARG_NONE, 
			&italics,    "Enable italicized text",                 NULL },
		{ "no-italics",    0,   G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, 
			&italics,    "Disable italicized text",                NULL },

		{ "palette",       'p', G_OPTION_FLAG_NONE,    G_OPTION_ARG_STRING, 
			&palette,    "The terminal's color palette",           NULL },
		{ NULL }
	};

	opt = g_option_context_new("- Wayland Terminal Emulator");
	g_option_context_add_main_entries(opt, opts, NULL);
	g_option_context_add_group(opt, gtk_get_option_group(TRUE));
	if (!g_option_context_parse(opt, &argc, &argv, &e)) {
		g_print("cannot parse arguments: %s\n", e->message);
		g_error_free(e);
		return -EINVAL;
	}

	load_config_file(config, cfg);
	g_free(cfg);

	if (show_dirty != 2)
		config->show_dirty = show_dirty;
	if (snap_size != 2)
		config->snap_size = snap_size;
	if (sb_size >= 0)
		config->snap_size = snap_size;
	if (font_name != NULL) {
		g_free(config->font_name);
		config->font_name = font_name;
	}
	if (font_size > 0)
		config->font_size = font_size;
	if (bold != 2)
		config->bold = bold;
	if (underline != 2)
		config->underline = underline;
	if (italics != 2)
		config->italics = italics;
	if (palette != NULL)
	{
		g_free(config->palette);
		config->palette = palette;
	}

	// Since font_name gets a dynamically allocated
	// string if allocated made using g_malloc, we have
	// to do the same for the default vallue to make cleanup
	// consistant.
	if (!config->font_name) {
		config->font_name = g_malloc(sizeof(DEFAULT_FONT));
		memcpy(config->font_name, DEFAULT_FONT, sizeof(DEFAULT_FONT));
	}

	return 0;
}

int wlt_config_new(struct wlt_config **out, int argc, char **argv)
{
	struct wlt_config *config;
	int r;

	config = calloc(1, sizeof(*config));
	if (!config)
		return -ENOMEM;
	config->ref = 1;

	// Default values
	config->sb_size = 2000;
	config->font_size = 10;

	r = init_config(config, argc, argv);
	if (r < 0)
		goto error;

	*out = config;
	return 0;

	error:
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

bool wlt_config_get_bold(struct wlt_config *config)
{
	return config->bold;
}

bool wlt_config_get_underline(struct wlt_config *config)
{
	return config->underline;
}

bool wlt_config_get_italics(struct wlt_config *config)
{
	return config->italics;
}

const char *wlt_config_get_palette(struct wlt_config *config)
{
	return config->palette;
}
