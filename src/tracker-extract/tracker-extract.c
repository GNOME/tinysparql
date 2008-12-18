/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#define _XOPEN_SOURCE
#include <time.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gmodule.h>

#ifndef G_OS_WIN32
#include <sys/resource.h>
#endif

#include <libtracker-common/tracker-os-dependant.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-thumbnailer.h>

#include "tracker-extract.h"

#define MAX_MEM       128
#define MAX_MEM_AMD64 512


#define DISABLE_DEBUG

#ifdef G_HAVE_ISO_VARARGS
#  ifdef DISABLE_DEBUG
#    define debug(...)
#  else
#    define debug(...) debug_impl (__VA_ARGS__)
#  endif
#elif defined(G_HAVE_GNUC_VARARGS)
#  if DISABLE_DEBUG
#    define debug(fmt...)
#  else
#    define debug(fmt...) debug_impl(fmt)
#  endif
#else
#  if DISABLE_DEBUG
#    define debug(x)
#  else
#    define debug debug_impl
#  endif
#endif

static GArray *extractors = NULL;
static guint   shutdown_timeout_id = 0;

#ifndef DISABLE_DEBUG

static void
debug_impl (const gchar *msg, ...)
{
	va_list args;

	va_start (args, msg);
	g_vfprintf (stderr, msg, args);
	va_end (args);

	g_fprintf (stderr, "\n");
}

#endif /* DISABLE_DEBUG */

static void
initialize_extractors (void)
{
	GDir	    *dir;
	GError	    *error;
	const gchar *name;
	GArray	    *generic_extractors;

	if (extractors != NULL) {
		return;
	}

	if (!g_module_supported ()) {
		g_error ("Modules are not supported for this platform");
		return;
	}

	error = NULL;

	extractors = g_array_sized_new (FALSE,
					TRUE,
					sizeof (TrackerExtractorData),
					10);

	/* This array is going to be used to store
	 * temporarily extractors with mimetypes such as "audio / *"
	 */
	generic_extractors = g_array_sized_new (FALSE,
						TRUE,
						sizeof (TrackerExtractorData),
						10);

	dir = g_dir_open (MODULESDIR, 0, &error);

	if (!dir) {
		g_error ("Error opening modules directory: %s", error->message);
		g_error_free (error);
		g_array_free (extractors, TRUE);
		extractors = NULL;
		g_array_free (generic_extractors, TRUE);
		return;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		GModule			 *module;
		gchar			 *module_path;
		TrackerExtractorDataFunc func;
		TrackerExtractorData	 *data;

		if (!g_str_has_suffix (name, "." G_MODULE_SUFFIX)) {
			continue;
		}

		module_path = g_build_filename (MODULESDIR, name, NULL);

		module = g_module_open (module_path, G_MODULE_BIND_LOCAL);

		if (!module) {
			g_warning ("Could not load module '%s': %s", name, g_module_error ());
			g_free (module_path);
			continue;
		}

		g_module_make_resident (module);

		if (g_module_symbol (module, "tracker_get_extractor_data", (gpointer *) &func)) {
			data = (func) ();

			while (data->mime) {
				if (strchr (data->mime, '*') != NULL) {
					g_array_append_val (generic_extractors, *data);
				} else {
					g_array_append_val (extractors, *data);
				}

				data++;
			}
		}

		g_free (module_path);
	}

	/* Append the generic extractors at the end of
	 * the list, so the specific ones are used first
	 */
	g_array_append_vals (extractors,
			     generic_extractors->data,
			     generic_extractors->len);
	g_array_free (generic_extractors, TRUE);
}

static GHashTable *
tracker_get_file_metadata (const gchar *uri,
			   const gchar *mime)
{
	GHashTable *meta_table;
	gchar	   *uri_in_locale;

	if (!uri) {
		return NULL;
	}

	debug ("Extractor - Getting metadata from file:'%s' with mime:'%s'",
	       uri,
	       mime);

	uri_in_locale = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (!uri_in_locale) {
		g_warning ("Could not convert uri:'%s' from UTF-8 to locale", uri);
		return NULL;
	}

	if (!g_file_test (uri_in_locale, G_FILE_TEST_EXISTS)) {
		g_warning ("File does not exist '%s'", uri_in_locale);
		g_free (uri_in_locale);
		return NULL;
	}

	meta_table = g_hash_table_new_full (g_str_hash,
					    g_str_equal,
					    g_free,
					    g_free);

	if (mime) {
		guint i;
		TrackerExtractorData *data;

		for (i = 0; i < extractors->len; i++) {
			data = &g_array_index (extractors, TrackerExtractorData, i);

			if (g_pattern_match_simple (data->mime, mime)) {
				(*data->extractor) (uri_in_locale, meta_table);

				if (g_hash_table_size (meta_table) == 0) {
					continue;
				}

				g_free (uri_in_locale);

				debug ("Extractor - Found %d metadata items",
				       g_hash_table_size (meta_table));

				return meta_table;
			}
		}

		debug ("Extractor - Could not find any extractors to handle metadata type.");
	} else {
		debug ("Extractor - No mime available, not extracting data");
	}

	g_free (uri_in_locale);

	return NULL;
}

static void
print_meta_table_data (gpointer key,
		       gpointer value,
		       gpointer user_data)
{
	gchar *value_utf8;

	g_return_if_fail (key != NULL);
	g_return_if_fail (value != NULL);

	value_utf8 = g_locale_to_utf8 (value, -1, NULL, NULL, NULL);

	if (value_utf8) {
		value_utf8 = g_strstrip (value_utf8);

		debug ("Extractor - Found '%s' = '%s'",
		       (gchar*) key,
		       value_utf8);

		g_print ("%s=%s;\n", (gchar*) key, value_utf8);

		g_free (value_utf8);
	}
}

static gboolean
shutdown_app_timeout (gpointer user_data)
{
	GMainLoop *main_loop;

	debug ("Extractor - Timed out, shutting down");

	main_loop = (GMainLoop *) user_data;
	g_main_loop_quit (main_loop);

	return FALSE;
}

static void
reset_shutdown_timeout (GMainLoop *main_loop)
{
	debug ("Extractor - Resetting timeout");

	if (shutdown_timeout_id != 0) {
		g_source_remove (shutdown_timeout_id);
	}

	shutdown_timeout_id = g_timeout_add (30000, shutdown_app_timeout, main_loop);
}

static void
print_file_metadata (const gchar *filename,
		     const gchar *mimetype)
{
	GHashTable *meta;

	if (mimetype && *mimetype) {
		meta = tracker_get_file_metadata (filename, mimetype);
	} else {
		meta = tracker_get_file_metadata (filename, NULL);
	}

	if (meta) {
		g_hash_table_foreach (meta, print_meta_table_data, NULL);
		g_hash_table_destroy (meta);
	}

	/* Add an empty line so the indexer
	 * knows when to stop reading
	 */
	g_print ("\n");

	debug ("Extractor - Waiting for work");
}

static gboolean
process_input_cb (GIOChannel   *channel,
		  GIOCondition	condition,
		  gpointer	user_data)
{
	gchar *filename, *mimetype;

	debug ("Extractor - Processing input");

	reset_shutdown_timeout ((GMainLoop *) user_data);

	g_io_channel_read_line (channel, &filename, NULL, NULL, NULL);
	g_io_channel_read_line (channel, &mimetype, NULL, NULL, NULL);

	g_strstrip (filename);

	if (mimetype) {
		g_strstrip (mimetype);
	}

	print_file_metadata (filename, mimetype);

	g_free (filename);
	g_free (mimetype);

	return TRUE;
}

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GMainLoop      *main_loop;
	GIOChannel     *input;
	gchar          *summary;
	TrackerConfig  *config;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new (_("- Extract file meta data"));

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.				*/
	summary = g_strconcat (_("This command works two ways:"),
			       "\n",
			       "\n",
			       _(" - Passing arguments:"),
			       "\n",
			       "     tracker-extract [filename] [mime-type]\n",
			       "\n",
			       _(" - Reading filename/mime-type pairs from STDIN"),
			       "\n",
			       "     echo -e \"/home/foo/bar/baz.mp3\\naudio/x-mpeg\" | tracker-extract",
			       NULL);

	g_option_context_set_summary (context, summary);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	g_free (summary);

	debug ("Extractor - Initializing...");
	tracker_memory_setrlimits ();

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	g_set_application_name ("tracker-extract");

	setlocale (LC_ALL, "");

	initialize_extractors ();

	if (argc >= 2) {
		if (argc >= 3) {
			print_file_metadata (argv[1], argv[2]);
		} else {
			print_file_metadata (argv[1], NULL);
		}

		return EXIT_SUCCESS;
	}

	config = tracker_config_new ();
	tracker_thumbnailer_init (config);

	main_loop = g_main_loop_new (NULL, FALSE);

	tracker_thumbnailer_shutdown ();
	g_object_unref (config);

	input = g_io_channel_unix_new (STDIN_FILENO);
	g_io_add_watch (input, G_IO_IN, process_input_cb, main_loop);

	reset_shutdown_timeout (main_loop);

	debug ("Extractor - Waiting for work");
	g_main_loop_run (main_loop);

	debug ("Extractor - Finished");

	return EXIT_SUCCESS;
}
