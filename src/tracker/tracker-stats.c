/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker-sparql/tracker-sparql.h>

#define ABOUT \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public " \
	"License which can be viewed at:\n" \
	"\n" \
	"  http://www.gnu.org/licenses/gpl.txt\n"

static gboolean print_version;
static gboolean show_all;
static gchar **terms;

static GHashTable *common_rdf_types;

static GOptionEntry entries[] = {
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
	  NULL
	},
	{ "all", 'a', 0, G_OPTION_ARG_NONE, &show_all,
	  N_("Show statistics about ALL RDF classes, not just common ones which is the default (implied by search terms)"),
	  NULL
	},
	{ G_OPTION_REMAINING, 0, 0,
	  G_OPTION_ARG_STRING_ARRAY, &terms,
	  N_("search terms"),
	  N_("EXPRESSION")
	},
	{ NULL }
};

static gboolean
get_common_rdf_types (void)
{
	const gchar *extractors_dir, *name;
	GList *files = NULL, *l;
	GError *error = NULL;
	GDir *dir;

	if (common_rdf_types) {
		return TRUE;
	}

	extractors_dir = g_getenv ("TRACKER_EXTRACTOR_RULES_DIR");
	if (G_LIKELY (extractors_dir == NULL)) {
		extractors_dir = TRACKER_EXTRACTOR_RULES_DIR;
	}

	dir = g_dir_open (extractors_dir, 0, &error);

	if (!dir) {
		g_error ("Error opening extractor rules directory: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	common_rdf_types = g_hash_table_new_full (g_str_hash,
	                                          g_str_equal,
	                                          g_free,
	                                          NULL);

	while ((name = g_dir_read_name (dir)) != NULL) {
		files = g_list_insert_sorted (files, (gpointer) name, (GCompareFunc) g_strcmp0);
	}

	for (l = files; l; l = l->next) {
		GKeyFile *key_file;
		const gchar *name;
		gchar *path;

		name = l->data;

		if (!g_str_has_suffix (l->data, ".rule")) {
			continue;
		}

		path = g_build_filename (extractors_dir, name, NULL);
		key_file = g_key_file_new ();

		g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, &error);

		if (G_UNLIKELY (error)) {
			g_clear_error (&error);
		} else {
			gchar **rdf_types;
			gsize n_rdf_types;

			rdf_types = g_key_file_get_string_list (key_file, "ExtractorRule", "FallbackRdfTypes", &n_rdf_types, &error);

			if (G_UNLIKELY (error)) {
				g_clear_error (&error);
			} else if (rdf_types != NULL) {
				gint i;

				for (i = 0; i < n_rdf_types; i++) {
					const gchar *rdf_type = rdf_types[i];

					g_hash_table_insert (common_rdf_types, g_strdup (rdf_type), GINT_TO_POINTER(TRUE));
				}
			}

			g_strfreev (rdf_types);
		}

		g_key_file_free (key_file);
		g_free (path);
	}

	g_list_free (files);
	g_dir_close (dir);

	/* Make sure some additional RDF types are shown which are not
	 * fall backs.
	 */
	g_hash_table_insert (common_rdf_types, g_strdup ("rdfs:Resource"), GINT_TO_POINTER(TRUE));
	g_hash_table_insert (common_rdf_types, g_strdup ("rdfs:Class"), GINT_TO_POINTER(TRUE));
	g_hash_table_insert (common_rdf_types, g_strdup ("nfo:FileDataObject"), GINT_TO_POINTER(TRUE));
	g_hash_table_insert (common_rdf_types, g_strdup ("nfo:Folder"), GINT_TO_POINTER(TRUE));
	g_hash_table_insert (common_rdf_types, g_strdup ("nfo:Executable"), GINT_TO_POINTER(TRUE));
	g_hash_table_insert (common_rdf_types, g_strdup ("nco:Contact"), GINT_TO_POINTER(TRUE));
	g_hash_table_insert (common_rdf_types, g_strdup ("nao:Tag"), GINT_TO_POINTER(TRUE));
	g_hash_table_insert (common_rdf_types, g_strdup ("tracker:Volume"), GINT_TO_POINTER(TRUE));

	return TRUE;
}


int
main (int argc, char **argv)
{
	TrackerSparqlConnection *connection;
	TrackerSparqlCursor *cursor;
	GOptionContext *context;
	GError *error = NULL;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the  */
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>     */
	context = g_option_context_new (_(" - Show statistics for all Nepomuk defined ontology classes"));
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (print_version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		g_option_context_free (context);

		return EXIT_SUCCESS;
	}

	/* We use search terms on ALL ontologies not just common ones */
	if (terms && g_strv_length (terms) > 0) {
		show_all = TRUE;
	}

	g_option_context_free (context);

	connection = tracker_sparql_connection_get (NULL, &error);

	if (!connection) {
		g_printerr ("%s: %s\n",
		            _("Could not establish a connection to Tracker"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	cursor = tracker_sparql_connection_statistics (connection, NULL, &error);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not get Tracker statistics"),
		            error->message);
		g_error_free (error);
		return EXIT_FAILURE;
	}

	if (!cursor) {
		g_print ("%s\n", _("No statistics available"));
	} else {
		GString *output;

		output = g_string_new ("");

		if (!show_all) {
			get_common_rdf_types ();
		}

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			const gchar *rdf_type;
			const gchar *rdf_type_count;

			rdf_type = tracker_sparql_cursor_get_string (cursor, 0, NULL);
			rdf_type_count = tracker_sparql_cursor_get_string (cursor, 1, NULL);

			if (!show_all && !g_hash_table_contains (common_rdf_types, rdf_type)) {
				continue;
			}

			if (terms) {
				gint i, n_terms;
				gboolean show_rdf_type = FALSE;

				n_terms = g_strv_length (terms);

				for (i = 0;
				     i < n_terms && !show_rdf_type;
				     i++) {
					show_rdf_type = g_str_match_string (terms[i], rdf_type, TRUE);
				}

				if (!show_rdf_type) {
					continue;
				}
			}

			g_string_append_printf (output,
			                        "  %s = %s\n",
			                        rdf_type,
			                        rdf_type_count);
		}

		if (output->len > 0) {
			/* To translators: This is to say there are no
			 * statistics found. We use a "Statistics:
			 * None" with multiple print statements */
			g_string_prepend (output, "\n");
			g_string_prepend (output, _("Statistics:"));
		} else {
			g_string_append_printf (output,
			                        "  %s\n", _("None"));
		}

		g_print ("%s\n", output->str);
		g_string_free (output, TRUE);

		if (common_rdf_types) {
			g_hash_table_unref (common_rdf_types);
		}

		g_object_unref (cursor);
	}

	g_object_unref (connection);

	return EXIT_SUCCESS;
}
