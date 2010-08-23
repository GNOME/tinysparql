/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <sys/param.h>
#include <stdlib.h>
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

static gboolean parse_list_notifies (const gchar  *option_name,
                                     const gchar  *value,
                                     gpointer      data,
                                     GError      **error);

static gchar *file;
static gchar *query;
static gboolean update;
static gboolean list_classes;
static gboolean list_class_prefixes;
static gchar *list_properties;
static gchar *list_notifies;
static gboolean print_version;
static gchar *search;

static GOptionEntry   entries[] = {
	{ "file", 'f', 0, G_OPTION_ARG_FILENAME, &file,
	  N_("Path to use to run a query or update from file"),
	  N_("FILE"),
	},
	{ "query", 'q', 0, G_OPTION_ARG_STRING, &query,
	  N_("SPARQL query"),
	  N_("SPARQL"),
	},
	{ "update", 'u', 0, G_OPTION_ARG_NONE, &update,
	  N_("This is used with --query and for database updates only."),
	  NULL,
	},
	{ "list-classes", 'c', 0, G_OPTION_ARG_NONE, &list_classes,
	  N_("Retrieve classes"),
	  NULL,
	},
	{ "list-class-prefixes", 'x', 0, G_OPTION_ARG_NONE, &list_class_prefixes,
	  N_("Retrieve class prefixes"),
	  NULL,
	},
	{ "list-properties", 'p', 0, G_OPTION_ARG_STRING, &list_properties,
	  N_("Retrieve properties for a class, prefixes can be used too (e.g. rdfs:Resource)"),
	  N_("CLASS"),
	},
	{ "list-notifies", 'n', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, parse_list_notifies,
	  N_("Retrieve classes which notify changes in the database (CLASS is optional)"),
	  N_("CLASS"),
	},
	{ "search", 's', 0, G_OPTION_ARG_STRING, &search,
	  N_("Search for a class or property and display more information (e.g. Document)"),
	  N_("CLASS/PROPERTY"),
	},
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
	  NULL,
	},
	{ NULL }
};

static gchar *
get_class_from_prefix (TrackerSparqlConnection *connection,
                       const gchar             *prefix)
{
	GError *error = NULL;
	TrackerSparqlCursor *cursor;
	const gchar *query;
	gchar *found = NULL;

	query = "SELECT ?prefix ?ns "
		"WHERE {"
		"  ?ns a tracker:Namespace ;"
		"  tracker:prefix ?prefix "
		"}";

	/* We have namespace prefix, get full name */
	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not get namespace prefixes"),
		            error->message);
		g_error_free (error);

		return NULL;
	}

	if (!cursor) {
		g_printerr ("%s\n",
		            _("No namespace prefixes were found"));

		return NULL;
	}

	while (tracker_sparql_cursor_next (cursor, NULL, NULL) && !found) {
		const gchar *class_prefix, *class_name;

		class_prefix = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		class_name = tracker_sparql_cursor_get_string (cursor, 1, NULL);

		if (strcmp (class_prefix, prefix) == 0) {
			found = g_strdup (class_name);
		}
	}

	g_object_unref (cursor);

	return found;
}

static gboolean
parse_list_notifies (const gchar  *option_name,
                     const gchar  *value,
                     gpointer      data,
                     GError      **error)
{
	if (!value) {
		list_notifies = g_strdup ("");
	} else {
		list_notifies = g_strdup (value);
	}

	return TRUE;
}

static void
print_cursor (TrackerSparqlCursor *cursor,
              const gchar         *none_found,
              const gchar         *heading,
              gboolean             only_first_col)
{
	if (!cursor) {
		g_print ("%s\n", none_found);
	} else {
		gint count = 0;

		g_print ("%s:\n", heading);

		if (only_first_col) {
			while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
				g_print ("  %s\n", tracker_sparql_cursor_get_string (cursor, 0, NULL));
				count++;
			}
		} else {
			while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
				gint col;

				for (col = 0; col < tracker_sparql_cursor_get_n_columns (cursor); col++) {
					g_print ("%c %s",
					         col == 0 ? ' ' : ',',
					         tracker_sparql_cursor_get_string (cursor, col, NULL));
				}

				g_print ("\n");

				count++;
			}
		}

		if (count == 0) {
			g_print ("  %s\n", _("None"));
		}

		g_print ("\n");

		g_object_unref (cursor);
	}
}

int
main (int argc, char **argv)
{
	TrackerSparqlConnection *connection;
	TrackerSparqlCursor *cursor;
	GOptionContext *context;
	GError *error = NULL;
	const gchar *error_message;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (_("- Query or update using SPARQL"));

	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (print_version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		g_option_context_free (context);

		return EXIT_SUCCESS;
	}

	if (!list_classes && !list_class_prefixes && !list_properties &&
	    !list_notifies && !search && !file && !query) {
		error_message = _("An argument must be supplied");
	} else if (file && query) {
		error_message = _("File and query can not be used together");
	} else {
		error_message = NULL;
	}

	if (error_message) {
		gchar *help;

		g_printerr ("%s\n\n", error_message);

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	connection = tracker_sparql_connection_get (&error);

	if (!connection) {
		g_printerr ("%s: %s\n",
		            _("Could not establish a connection to Tracker"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	if (list_classes) {
		const gchar *query;

		query = "SELECT ?c WHERE { ?c a rdfs:Class }";
		cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not list classes"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);

			return EXIT_FAILURE;
		}

		print_cursor (cursor, _("No classes were found"), _("Classes"), TRUE);
	}

	if (list_class_prefixes) {
		const gchar *query;

		query = "SELECT ?prefix ?ns "
		        "WHERE {"
		        "  ?ns a tracker:Namespace ;"
		        "  tracker:prefix ?prefix "
		        "}";

		cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not list class prefixes"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);

			return EXIT_FAILURE;
		}

		print_cursor (cursor, _("No class prefixes were found"), _("Prefixes"), FALSE);
	}

	if (list_properties) {
		gchar *query;
		gchar *class_name;

		if (g_str_has_prefix (list_properties, "http://")) {
			/* We have full class name */
			class_name = g_strdup (list_properties);
		} else {
			gchar *p;
			gchar *prefix, *property;
			gchar *class_name_no_property;

			prefix = g_strdup (list_properties);
			p = strchr (prefix, ':');

			if (!p) {
				g_printerr ("%s\n",
				            _("Could not find property for class prefix, "
				              "e.g. :Resource in 'rdfs:Resource'"));
				g_free (prefix);
				g_object_unref (connection);

				return EXIT_FAILURE;
			}

			property = g_strdup (p + 1);
			*p = '\0';

			class_name_no_property = get_class_from_prefix (connection, prefix);
			g_free (prefix);

			if (!class_name_no_property) {
				g_free (property);
				g_object_unref (connection);

				return EXIT_FAILURE;
			}

			class_name = g_strconcat (class_name_no_property, property, NULL);
			g_free (class_name_no_property);
			g_free (property);
		}

		query = g_strdup_printf ("SELECT ?p "
		                         "WHERE {"
		                         "  ?p a rdf:Property ;"
		                         "  rdfs:domain <%s>"
		                         "}",
		                         class_name);

		cursor = tracker_sparql_connection_query (connection, query, NULL, &error);
		g_free (query);
		g_free (class_name);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not list properties"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);

			return EXIT_FAILURE;
		}

		print_cursor (cursor, _("No properties were found"), _("Properties"), TRUE);
	}

	if (list_notifies) {
		gchar *query;

		/* First list classes */
		if (*list_notifies == '\0') {
			query = g_strdup_printf ("SELECT ?c "
			                         "WHERE {"
			                         "  ?c a rdfs:Class ."
			                         "  ?c tracker:notify true ."
			                         "}");
		} else {
			query = g_strdup_printf ("SELECT ?c "
			                         "WHERE {"
			                         "  ?c a rdfs:Class ."
			                         "  ?c tracker:notify true "
			                         "  FILTER regex (?c, \"%s\", \"i\") "
			                         "}",
			                         list_notifies);
		}

		cursor = tracker_sparql_connection_query (connection, query, NULL, &error);
		g_free (query);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not find notify classes"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);

			return EXIT_FAILURE;
		}

		print_cursor (cursor, _("No notifies were found"), _("Notifies"), TRUE);
	}

	if (search) {
		gchar *query;

		/* First list classes */
		query = g_strdup_printf ("SELECT ?c "
		                         "WHERE {"
		                         "  ?c a rdfs:Class"
		                         "  FILTER regex (?c, \"%s\", \"i\") "
		                         "}",
		                         search);
		cursor = tracker_sparql_connection_query (connection, query, NULL, &error);
		g_free (query);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not search classes"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);

			return EXIT_FAILURE;
		}

		print_cursor (cursor, _("No classes were found to match search term"), _("Classes"), TRUE);

		/* Second list properties */
		query = g_strdup_printf ("SELECT ?p "
		                         "WHERE {"
		                         "  ?p a rdf:Property"
		                         "  FILTER regex (?p, \"%s\", \"i\") "
		                         "}",
		                         search);

		cursor = tracker_sparql_connection_query (connection, query, NULL, &error);
		g_free (query);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not search properties"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);

			return EXIT_FAILURE;
		}

		print_cursor (cursor, _("No properties were found to match search term"), _("Properties"), TRUE);
	}

	if (file) {
		gchar *path_in_utf8;
		gsize size;

		path_in_utf8 = g_filename_to_utf8 (file, -1, NULL, NULL, &error);
		if (error) {
			g_printerr ("%s:'%s', %s\n",
			            _("Could not get UTF-8 path from path"),
			            file,
			            error->message);
			g_error_free (error);
			g_object_unref (connection);

			return EXIT_FAILURE;
		}

		g_file_get_contents (path_in_utf8, &query, &size, &error);
		if (error) {
			g_printerr ("%s:'%s', %s\n",
			            _("Could not read file"),
			            path_in_utf8,
			            error->message);
			g_error_free (error);
			g_free (path_in_utf8);
			g_object_unref (connection);

			return EXIT_FAILURE;
		}

		g_free (path_in_utf8);
	}

	if (query) {
		if (G_UNLIKELY (update)) {
			tracker_sparql_connection_update (connection, query, 0, NULL, &error);

			if (error) {
				g_printerr ("%s, %s\n",
				            _("Could not run update"),
				            error->message);
				g_error_free (error);

				return EXIT_FAILURE;
			}

			g_print ("%s\n", _("Done"));

#if 0
			if (results) {
				GPtrArray *insert;
				GHashTable *solution;
				GHashTableIter iter;
				gpointer key, value;
				gint i, s, n;

				for (i = 0; i < results->len; i++) {
					insert = results->pdata[i];

					for (s = 0; s < insert->len; s++) {
						solution = insert->pdata[s];

						g_hash_table_iter_init (&iter, solution);
						n = 0;
						while (g_hash_table_iter_next (&iter, &key, &value)) {
							g_print ("%s%s: %s",
							         n > 0 ? ", " : "",
							         (const gchar *) key,
							         (const gchar *) value);
							n++;
						}
						g_print ("\n");
					}
				}
			}
#endif
		} else {
			cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

			if (error) {
				g_printerr ("%s, %s\n",
				            _("Could not run query"),
				            error->message);
				g_error_free (error);

				return EXIT_FAILURE;
			}

			print_cursor (cursor, _("No results found matching your query"), _("Results"), FALSE);
		}
	}

	g_object_unref (connection);

	return EXIT_SUCCESS;
}

