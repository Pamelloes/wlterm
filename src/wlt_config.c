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
#include <stdlib.h>
#include <string.h>
#include "wlterm.h"

struct wlt_config {
	unsigned long ref;

	gboolean show_dirty;
	gboolean snap_size;
	gint sb_size;
	gchar *palette;

	gchar *font_name;
	gint font_size;
	gboolean bold;
	gboolean underline;
	gboolean italics;
	gboolean blink;

	gint cursor_mode;
	gboolean cursor_blink;
	gint64 cursor_color;
};

const char DEFAULT_FONT[] = "monospace";

static int load_bool(GKeyFile *keyf, const char *sect, const char *name,
                     gboolean *output, GError **err)
{
	GError *e = NULL;
	gboolean blvl = g_key_file_get_boolean(keyf, sect, name, &e);
	if (!e) {
		*output = blvl;
		return 0;
	}

	if (g_error_matches(e, G_KEY_FILE_ERROR,
	                    G_KEY_FILE_ERROR_KEY_NOT_FOUND) ||
	    g_error_matches(e, G_KEY_FILE_ERROR,
	                    G_KEY_FILE_ERROR_GROUP_NOT_FOUND)) {
		g_error_free(e);
		return 0;
	}

	g_propagate_error(err, e);
	return -EINVAL;
}

static int load_int(GKeyFile *keyf, const char *sect, const char *name, 
                    gint *output, GError **err)
{
	GError *e = NULL;
	gint ivl = g_key_file_get_integer(keyf, sect, name, &e);
	if (!e) {
		*output = ivl;
		return 0;
	}

	if (g_error_matches(e, G_KEY_FILE_ERROR,
	                    G_KEY_FILE_ERROR_KEY_NOT_FOUND) ||
	    g_error_matches(e, G_KEY_FILE_ERROR,
	                    G_KEY_FILE_ERROR_GROUP_NOT_FOUND)) {
		g_error_free(e);
		return 0;
	}

	g_propagate_error(err, e);
	return -EINVAL;
}

static int load_str(GKeyFile *keyf, const char *sect, const char *name,
                    char **output, GError **err)
{
	GError *e = NULL;
	char *svl = g_key_file_get_string(keyf, sect, name, &e);
	if (!e) {
		g_free(*output);
		*output = svl;
		return 0;
	}

	if (g_error_matches(e, G_KEY_FILE_ERROR,
	                    G_KEY_FILE_ERROR_KEY_NOT_FOUND) ||
	    g_error_matches(e, G_KEY_FILE_ERROR,
	                    G_KEY_FILE_ERROR_GROUP_NOT_FOUND)) {
		g_error_free(e);
		return 0;
	}

	g_propagate_error(err, e);
	return -EINVAL;
}

static int load_int64(GKeyFile *keyf, const char *sect, const char *name,
                      gint64 *output, GError **err)
{
	GError *e = NULL;
	char *ptr = NULL;
	long int val = 0;
	char *svl = g_key_file_get_string(keyf, sect, name, &e);
	if (e) {
		if (g_error_matches(e, G_KEY_FILE_ERROR,
		                    G_KEY_FILE_ERROR_KEY_NOT_FOUND) ||
		    g_error_matches(e, G_KEY_FILE_ERROR,
		                    G_KEY_FILE_ERROR_GROUP_NOT_FOUND)) {
			g_error_free(e);
			return 0;
		} else {
			g_propagate_error(err, e);
			return -EINVAL;
		}
	}

	errno = 0;
	val = strtol(svl, &ptr, 0);
	if (errno || *ptr != '\0') {
		g_set_error(err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE,
		            "en Couldn't parse int64!");
		g_free(svl);
		return -EINVAL;
	}
	g_free(svl);

	*output = val;
	return 0;
}

static int load_config_file(struct wlt_config *conf, char *fname)
{
	GKeyFile *keyf;
	GError *err = NULL;
	int r;

	keyf = g_key_file_new();
	if (fname) {
		if (!g_key_file_load_from_file(keyf, fname, G_KEY_FILE_NONE, &err))
		{
			r = -EINVAL;
			goto error;
		}
	} else {
		const char **dirs;
		const char * const * sysdirs = g_get_system_config_dirs();
		int len = 0;
		bool res;

		while (sysdirs[len++]) ;

		dirs = calloc(len + 1, sizeof(char *)); 
		if (!dirs) {
			r = -ENOMEM;
			goto keyfile;
		}

		dirs[0] = g_get_user_config_dir();
		memcpy(dirs + 1, sysdirs, len * sizeof(char *));

		res = g_key_file_load_from_dirs(keyf, "wlterm/wlterm.conf", dirs,
		                                NULL, G_KEY_FILE_NONE, &err);
		free(dirs);

		if (!res) {
			r = g_error_matches(err, G_KEY_FILE_ERROR, 
			                    G_KEY_FILE_ERROR_NOT_FOUND) ? 0 : -EINVAL;
			goto error;
		}
	}

	r = load_bool(keyf, "terminal", "show_dirty", &conf->show_dirty, &err);
	if (r < 0)
		goto error;

	r = load_bool(keyf, "terminal", "snap_size", &conf->snap_size, &err);
	if (r < 0)
		goto error;

	r = load_int(keyf, "terminal", "sb_size", &conf->sb_size, &err);
	if (r < 0)
		goto error;

	r = load_str(keyf, "terminal", "palette", &conf->palette, &err);
	if (r < 0)
		goto error;

	r = load_str(keyf, "font", "name", &conf->font_name, &err);
	if (r < 0)
		goto error;

	r = load_int(keyf, "font", "size", &conf->font_size, &err);
	if (r < 0)
		goto error;

	r = load_bool(keyf, "font", "bold", &conf->bold, &err);
	if (r < 0)
		goto error;

	r = load_bool(keyf, "font", "underline", &conf->underline, &err);
	if (r < 0)
		goto error;

	r = load_bool(keyf, "font", "italics", &conf->italics, &err);
	if (r < 0)
		goto error;

	r = load_bool(keyf, "font", "blink", &conf->blink, &err);
	if (r < 0)
		goto error;

	r = load_int(keyf, "cursor", "mode", &conf->cursor_mode, &err);
	if (r < 0)
		goto error;

	r = load_bool(keyf, "cursor", "blink", &conf->cursor_blink, &err);
	if (r < 0)
		goto error;

	r = load_int64(keyf, "cursor", "color", &conf->cursor_color, &err);
	if (r < 0)
		goto error;

	return 0;

error:
	g_print("Could not load configuration: %s\n", err->message);
	g_error_free(err);
keyfile:
	g_key_file_free(keyf);
	return r;
}

// Loads in all of the settings. To handle the two stage loading (file
// then command line), we need to use some indirection.
static int init_config(struct wlt_config *config, int argc, char **argv)
{
	GOptionContext *opt;
	GError *e = NULL;
	int r;

	char *cfg = NULL;

	int show_dirty = 2;
	int snap_size = 2;
	int sb_size = -1;
	char *palette = NULL;

	char *font_name = NULL;
	int font_size = 0;
	int bold = 2;
	int underline = 2;
	int italics = 2;
	int blink = 2;

	int ptr_mode = -1;
	int ptr_blink = 2;
	gint64 ptr_color = -1;

	GOptionEntry opts[] = {
		{ "config",        'c', G_OPTION_FLAG_NONE,    G_OPTION_ARG_FILENAME, 
			&cfg,        "Specify the configuration file",             NULL },

		{ "show-dirty",    0,   G_OPTION_FLAG_NONE,    G_OPTION_ARG_NONE, 
			&show_dirty, "Mark dirty cells during redraw",             NULL },
		{ "no-show-dirty", 0,   G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, 
			&show_dirty, "Don't mark dirty cells during redraw",       NULL },
		{ "snap-size",     0,   G_OPTION_FLAG_NONE,    G_OPTION_ARG_NONE, 
			&snap_size,  "Snap to next cell-size when resizing",       NULL },
		{ "no-snap-size",  0,   G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, 
			&snap_size,  "Don't snap to next cell-size when resizing", NULL },
		{ "sb-size",       0,   G_OPTION_FLAG_NONE,    G_OPTION_ARG_INT, 
			&sb_size,    "Scroll-back buffer size in lines",           NULL },
		{ "palette",       'p', G_OPTION_FLAG_NONE,    G_OPTION_ARG_STRING, 
			&palette,    "Set the terminal's color palette",           NULL },

		{ "font-name",     'f', G_OPTION_FLAG_NONE,    G_OPTION_ARG_STRING, 
			&font_name,  "Typeface name; defaults to 'monospace'",     NULL },
		{ "font-size",     's', G_OPTION_FLAG_NONE,    G_OPTION_ARG_INT, 
			&font_size,  "Font size; defaults to 10",                  NULL },
		{ "bold",          'b', G_OPTION_FLAG_NONE,    G_OPTION_ARG_NONE, 
			&bold,       "Enable bold text",                           NULL },
		{ "no-bold",       'B', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, 
			&bold,       "Disable bold text",                          NULL },
		{ "underline",     'u', G_OPTION_FLAG_NONE,    G_OPTION_ARG_NONE, 
			&underline,  "Enable underlined text",                     NULL },
		{ "no-underline",  'U', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, 
			&underline,  "Disable underlined text",                    NULL },
		{ "italics",       'i', G_OPTION_FLAG_NONE,    G_OPTION_ARG_NONE, 
			&italics,    "Enable italicized text",                     NULL },
		{ "no-italics",    'I', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, 
			&italics,    "Disable italicized text",                    NULL },
		{ "blink",         'l', G_OPTION_FLAG_NONE,    G_OPTION_ARG_NONE, 
			&blink,      "Enable blinking text",                       NULL },
		{ "no-blink",      'L', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, 
			&blink,      "Disable blinking text",                      NULL },

		{ "ptr-mode",      0,   G_OPTION_FLAG_NONE,    G_OPTION_ARG_INT, 
			&ptr_mode,   "Set the cursor mode; 0: █ 1: _",             NULL },
		{ "ptr-blink",     0,   G_OPTION_FLAG_NONE,   G_OPTION_ARG_NONE, 
			&ptr_blink,  "Enable blinking cursor",                     NULL },
		{ "no-ptr-blink",  0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, 
			&ptr_blink,  "Disable blinking cursor",                    NULL },
		{ "ptr-color",     0,   G_OPTION_FLAG_NONE,    G_OPTION_ARG_INT64, 
			&ptr_color,  "Set the cursor color",                       NULL },

		{ NULL }
	};

	opt = g_option_context_new("- Wayland Terminal Emulator");
	g_option_context_add_main_entries(opt, opts, NULL);
	g_option_context_add_group(opt, gtk_get_option_group(TRUE));
	if (!g_option_context_parse(opt, &argc, &argv, &e)) {
		g_print("cannot parse arguments: %s\n", e->message);
		g_error_free(e);
		r = -EINVAL;
		goto opt_error;
	}

	r = load_config_file(config, cfg);
	g_free(cfg);
	if (r < 0)
		goto file_error;

	if (show_dirty != 2)
		config->show_dirty = show_dirty;
	if (snap_size != 2)
		config->snap_size = snap_size;
	if (sb_size >= 0)
		config->snap_size = snap_size;
	if (palette != NULL)
	{
		g_free(config->palette);
		config->palette = palette;
	}

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
	if (blink != 2)
		config->blink = blink;

	if (ptr_mode >= 0)
		config->cursor_mode = ptr_mode;
	if (ptr_blink != 2)
		config->cursor_blink = ptr_blink;
	if (ptr_color >= 0)
		config->cursor_color = ptr_color;

	// Since font_name gets a dynamically allocated
	// string if allocated made using g_malloc, we have
	// to do the same for the default vallue to make cleanup
	// consistant.
	if (!config->font_name) {
		config->font_name = g_malloc(sizeof(DEFAULT_FONT));
		memcpy(config->font_name, DEFAULT_FONT, sizeof(DEFAULT_FONT));
	}

	return 0;

file_error:
	// We need to free the config values in case an error occured
	// after loading one.
	g_free(config->font_name);
	g_free(config->palette);
	// We need to free the command line values as they have not yet
	// been assigned.
	g_free(font_name);
	g_free(palette);
opt_error:
	g_option_context_free(opt);
	return r;
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
	g_free(config->palette);
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
