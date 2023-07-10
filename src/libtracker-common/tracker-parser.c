/*
 * Copyright (C) 2023, Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include <gio/gio.h>
#include <gmodule.h>

#include "tracker-parser.h"

#include "tracker-debug.h"

static TrackerParser * (*parser_new) (void);
static void (*parser_free) (TrackerParser *parser);
static void (*parser_reset) (TrackerParser *parser,
                             const gchar   *txt,
                             gint           txt_size,
                             guint          max_word_length,
                             gboolean       enable_stemmer,
                             gboolean       enable_unaccent,
                             gboolean       ignore_numbers);
static const gchar * (*parser_next) (TrackerParser *parser,
                                     gint          *position,
                                     gint          *byte_offset_start,
                                     gint          *byte_offset_end,
                                     gint          *word_length);
static gpointer (*collation_init) (void);
static void (*collation_shutdown) (gpointer collator);
static gint (*collation_utf8) (gpointer      collator,
                               gint          len1,
                               gconstpointer str1,
                               gint          len2,
                               gconstpointer str2);
static gunichar2 * (*util_tolower) (const gunichar2 *input,
                                   gsize            len,
                                   gsize           *len_out);
static gunichar2 * (*util_toupper) (const gunichar2 *input,
                                    gsize            len,
                                    gsize           *len_out);
static gunichar2 * (*util_casefold) (const gunichar2 *input,
                                     gsize            len,
                                     gsize           *len_out);
static gunichar2 * (*util_normalize) (const gunichar2 *input,
                                      GNormalizeMode   mode,
                                      gsize            len,
                                      gsize           *len_out);
static gunichar2 * (*util_unaccent) (const gunichar2 *input,
                                     gsize            len,
                                     gsize           *len_out);

static void
ensure_init_parser (void)
{
	static GModule *module = NULL;

	if (module == NULL) {
		const gchar *modules[] = {

			"libtracker-parser-libicu.so",
			"libtracker-parser-libunistring.so"
		};
		gchar *module_path;
		guint i;

		g_assert (g_module_supported ());

		for (i = 0; i < G_N_ELEMENTS (modules); i++) {
			gchar *current_dir;

			current_dir = g_get_current_dir ();
			if (g_strcmp0 (current_dir, BUILDROOT) == 0) {
				/* Detect in-build runtime of this code, this may happen
				 * building introspection information or running tests.
				 * We want the in-tree modules to be loaded then.
				 */
				module_path = g_strdup_printf (BUILD_LIBDIR "/%s", modules[i]);
			} else {
				module_path = g_strdup_printf (PRIVATE_LIBDIR "/%s", modules[i]);
			}

			module = g_module_open (module_path,
			                               G_MODULE_BIND_LAZY |
			                               G_MODULE_BIND_LOCAL);
			g_free (module_path);
			g_free (current_dir);

			if (module)
				break;
		}

		g_assert (module != NULL);

		if (!g_module_symbol (module, "tracker_parser_new", (gpointer *) &parser_new) ||
		    !g_module_symbol (module, "tracker_parser_free", (gpointer *) &parser_free) ||
		    !g_module_symbol (module, "tracker_parser_reset", (gpointer *) &parser_reset) ||
		    !g_module_symbol (module, "tracker_parser_next", (gpointer *) &parser_next) ||
		    !g_module_symbol (module, "tracker_collation_init", (gpointer *) &collation_init) ||
		    !g_module_symbol (module, "tracker_collation_shutdown", (gpointer *) &collation_shutdown) ||
		    !g_module_symbol (module, "tracker_collation_utf8", (gpointer *) &collation_utf8) ||
		    !g_module_symbol (module, "tracker_parser_tolower", (gpointer *) &util_tolower) ||
		    !g_module_symbol (module, "tracker_parser_toupper", (gpointer *) &util_toupper) ||
		    !g_module_symbol (module, "tracker_parser_casefold", (gpointer *) &util_casefold) ||
		    !g_module_symbol (module, "tracker_parser_normalize", (gpointer *) &util_normalize) ||
		    !g_module_symbol (module, "tracker_parser_unaccent", (gpointer *) &util_unaccent)) {
			g_printerr ("Could not initialize parser functions: %s\n",
			            g_module_error ());
		}

		TRACKER_NOTE (COLLATION, g_message ("Initialized collator %s", g_module_name (module)));

		g_module_make_resident (module);
		g_module_close (module);
	}
}

TrackerParser *
tracker_parser_new (void)
{
	ensure_init_parser ();

	return parser_new ();
}

void
tracker_parser_free (TrackerParser *parser)
{
	parser_free (parser);
}

void
tracker_parser_reset (TrackerParser *parser,
                      const gchar   *txt,
                      gint           txt_size,
                      guint          max_word_length,
                      gboolean       enable_stemmer,
                      gboolean       enable_unaccent,
                      gboolean       ignore_numbers)
{
	parser_reset (parser, txt, txt_size,
	              max_word_length,
	              enable_stemmer,
	              enable_unaccent,
	              ignore_numbers);
}

const gchar *
tracker_parser_next (TrackerParser *parser,
                     gint          *position,
                     gint          *byte_offset_start,
                     gint          *byte_offset_end,
                     gint          *word_length)
{
	return parser_next (parser, position,
	                    byte_offset_start,
	                    byte_offset_end,
	                    word_length);
}

gpointer
tracker_collation_init (void)
{
	ensure_init_parser ();

	return collation_init ();
}

void
tracker_collation_shutdown (gpointer collator)
{
	collation_shutdown (collator);
}

gint
tracker_collation_utf8 (gpointer      collator,
                        gint          len1,
                        gconstpointer str1,
                        gint          len2,
                        gconstpointer str2)
{
	return collation_utf8 (collator, len1, str1, len2, str2);
}

gunichar2 *
tracker_parser_tolower (const gunichar2 *input,
			gsize            len,
			gsize           *len_out)
{
	ensure_init_parser ();

	return util_tolower (input, len, len_out);
}

gunichar2 *
tracker_parser_toupper (const gunichar2 *input,
			gsize            len,
			gsize           *len_out)
{
	ensure_init_parser ();

	return util_toupper (input, len, len_out);
}

gunichar2 *
tracker_parser_casefold (const gunichar2 *input,
			 gsize            len,
			 gsize           *len_out)
{
	ensure_init_parser ();

	return util_casefold (input, len, len_out);
}

gunichar2 *
tracker_parser_normalize (const gunichar2 *input,
                          GNormalizeMode   mode,
			  gsize            len,
			  gsize           *len_out)
{
	ensure_init_parser ();

	return util_normalize (input, mode, len, len_out);
}

gunichar2 *
tracker_parser_unaccent (const gunichar2 *input,
			 gsize            len,
			 gsize           *len_out)
{
	ensure_init_parser ();

	return util_unaccent (input, len, len_out);
}
