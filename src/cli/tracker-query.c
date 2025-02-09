/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2014, Softathome <philippe.judge@softathome.com>
 * Copyright (C) 2014, Lanedo <martyn@lanedo.com>
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

#include <tinysparql.h>
#include <tracker-common.h>

#include "tracker-sparql.h"
#include "tracker-color.h"

#define SPARQL_OPTIONS_ENABLED() \
	(file || \
	 query)

static gchar *file;
static gchar *query;
static gboolean update;
static gchar *database_path;
static gchar *dbus_service;
static gchar *remote_service;
static gchar **args;

static GOptionEntry entries[] = {
	{ "database", 'd', 0, G_OPTION_ARG_FILENAME, &database_path,
	  N_("Location of the database"),
	  N_("FILE")
	},
	{ "dbus-service", 'b', 0, G_OPTION_ARG_STRING, &dbus_service,
	  N_("Connects to a DBus service"),
	  N_("DBus service name")
	},
	{ "remote-service", 'r', 0, G_OPTION_ARG_STRING, &remote_service,
	  N_("Connects to a remote service"),
	  N_("Remote service URI")
	},
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
	{ "arg", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &args,
	  N_("Provides an argument for a query parameter."),
	  N_("PARAMETER:TYPE:VALUE"),
	},
	{ NULL }
};

static TrackerSparqlConnection *
create_connection (GError **error)
{
	if (database_path && !dbus_service && !remote_service) {
		GFile *file;

		file = g_file_new_for_commandline_arg (database_path);
		return tracker_sparql_connection_new (update ?
						      TRACKER_SPARQL_CONNECTION_FLAGS_NONE :
						      TRACKER_SPARQL_CONNECTION_FLAGS_READONLY,
		                                      file, NULL, NULL, error);
	} else if (dbus_service && !database_path && !remote_service) {
		GDBusConnection *dbus_conn;

		dbus_conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
		if (!dbus_conn)
			return NULL;

		return tracker_sparql_connection_bus_new (dbus_service, NULL, dbus_conn, error);
	} else if (remote_service && !database_path && !dbus_service) {
		return tracker_sparql_connection_remote_new (remote_service);
	} else {
		/* TRANSLATORS: Those are commandline arguments */
		g_printerr ("%s\n", _("Specify one “--database”, “--dbus-service” or “--remote-service” option"));
		exit (EXIT_FAILURE);
	}
}

static gchar *
get_shorthand_str_for_offsets (TrackerNamespaceManager *namespaces,
                               const gchar             *str)
{
	GString *result = NULL;
	gchar **properties;
	gint i;

	if (!str) {
		return NULL;
	}

	properties = g_strsplit (str, ",", -1);
	if (!properties) {
		return NULL;
	}

	for (i = 0; properties[i] != NULL && properties[i + 1] != NULL; i += 2) {
		const gchar *property;
		const gchar *offset;
		gchar *shorthand;

		property = properties[i];
		offset = properties[i + 1];

		if (!property || !offset) {
			g_warning ("Expected property AND offset to be valid for fts:offset results");
			continue;
		}

		shorthand = tracker_namespace_manager_compress_uri (namespaces,
		                                                    property);

		if (!result) {
			result = g_string_new ("");
		} else {
			result = g_string_append_c (result, ' ');
		}

		g_string_append_printf (result, "%s:%s", shorthand, offset);
		g_free (shorthand);
	}

	g_strfreev (properties);

	return result ? g_string_free (result, FALSE) : NULL;
}

static void
print_cursor_with_ftsoffsets (TrackerSparqlCursor *cursor,
                              const gchar         *none_found,
                              const gchar         *heading,
                              gboolean             only_first_col)
{
	if (!cursor) {
		g_print ("%s\n", none_found);
	} else {
		TrackerSparqlConnection *conn;
		TrackerNamespaceManager *namespaces;
		gint count = 0;

		g_print ("%s:\n", heading);

		conn = tracker_sparql_cursor_get_connection (cursor);
		namespaces = tracker_sparql_connection_get_namespace_manager (conn);

		if (only_first_col) {
			while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
				const gchar *str;
				gchar *shorthand;

				str = tracker_sparql_cursor_get_string (cursor, 0, NULL);
				shorthand = get_shorthand_str_for_offsets (namespaces, str);
				g_print ("  %s\n", shorthand ? shorthand : str);
				g_free (shorthand);
				count++;
			}
		} else {
			while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
				gint col;

				for (col = 0; col < tracker_sparql_cursor_get_n_columns (cursor); col++) {
					const gchar *str;
					gchar *shorthand;

					str = tracker_sparql_cursor_get_string (cursor, col, NULL);
					shorthand = get_shorthand_str_for_offsets (namespaces, str);
					g_print ("%c %s",
					         col == 0 ? ' ' : ',',
					         shorthand ? shorthand : str);
					g_free (shorthand);
				}

				g_print ("\n");

				count++;
			}
		}

		if (count == 0) {
			/* To translators: This is to say there are no
			 * search results found. We use a "foo: None"
			 * with multiple print statements, where "foo"
			 * may be Music or Images, etc. */
			g_print ("  %s\n", _("None"));
		}

		g_print ("\n");

		g_object_unref (cursor);
	}
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
			/* To translators: This is to say there are no
			 * search results found. We use a "foo: None"
			 * with multiple print statements, where "foo"
			 * may be Music or Images, etc. */
			g_print ("  %s\n", _("None"));
		}

		g_print ("\n");

		g_object_unref (cursor);
	}
}

static gboolean
bind_arguments (TrackerSparqlStatement  *stmt,
                gchar                  **args)
{
	int i;

	for (i = 0; args && args[i]; i++) {
		gchar **pair;

		pair = g_strsplit (args[i], ":", 3);
		if (g_strv_length (pair) != 3) {
			g_printerr (_("Invalid argument string %s"), args[i]);
			g_printerr ("\n");
			return FALSE;
		}

		if (strlen (pair[1]) != 1 ||
		    (pair[1][0] != 'i' &&
		     pair[1][0] != 'd' &&
		     pair[1][0] != 'b' &&
		     pair[1][0] != 's')) {
			g_printerr (_("Invalid parameter type for argument %s"), pair[0]);
			g_printerr ("\n");
			g_strfreev (pair);
			return FALSE;
		}

		switch (pair[1][0]) {
		case 'i': {
			gint64 val;

			val = strtol (pair[2], NULL, 10);
			tracker_sparql_statement_bind_int (stmt, pair[0], val);
			break;
		}
		case 'd': {
			gdouble val;

			val = strtod (pair[2], NULL);
			tracker_sparql_statement_bind_int (stmt, pair[0], val);
			break;
		}
		case 'b': {
			gboolean val;

			val = pair[2][0] == 't' || pair[2][0] == 'T' || pair[2][0] == '1';
			tracker_sparql_statement_bind_boolean (stmt, pair[0], val);
			break;
		}
		case 's':
			tracker_sparql_statement_bind_string (stmt, pair[0], pair[2]);
			break;
		}

		g_strfreev (pair);
	}

	return TRUE;
}

static int
sparql_run (void)
{
	TrackerSparqlConnection *connection;
	TrackerSparqlStatement *stmt;
	TrackerSparqlCursor *cursor = NULL;
	GError *error = NULL;
	gint retval = EXIT_SUCCESS;

	connection = create_connection (&error);

	if (!connection) {
		g_printerr ("%s: %s\n",
		            _("Could not establish a connection to Tracker"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	tracker_term_pipe_to_pager ();

	if (file) {
		gchar *path_in_utf8;
		gsize size;

		path_in_utf8 = g_filename_to_utf8 (file, -1, NULL, NULL, &error);
		if (!path_in_utf8) {
			g_assert (error != NULL);
			g_printerr ("%s:'%s', %s\n",
			            _("Could not get UTF-8 path from path"),
			            file,
			            error->message);
			g_error_free (error);
			g_object_unref (connection);
			retval = EXIT_FAILURE;
			goto out;
		}

		if (!g_file_get_contents (path_in_utf8, &query, &size, &error)) {
			g_assert (error != NULL);
			g_printerr ("%s:'%s', %s\n",
			            _("Could not read file"),
			            path_in_utf8,
			            error->message);
			g_error_free (error);
			g_free (path_in_utf8);
			g_object_unref (connection);
			retval = EXIT_FAILURE;
			goto out;
		}

		g_free (path_in_utf8);
	}

	if (query) {
		if (G_UNLIKELY (update)) {
			stmt = tracker_sparql_connection_update_statement (connection,
			                                                   query,
			                                                   NULL,
			                                                   &error);

			if (stmt) {
				if (!bind_arguments (stmt, args)) {
					retval = EXIT_FAILURE;
					goto out;
				}

				tracker_sparql_statement_update (stmt, NULL, &error);
			}

			if (error) {
				g_printerr ("%s, %s\n",
				            _("Could not run update"),
				            error->message);
				g_error_free (error);
				retval = EXIT_FAILURE;
				goto out;
			}

			g_print ("%s\n", _("Done"));
		} else {
			stmt = tracker_sparql_connection_query_statement (connection,
			                                                  query,
			                                                  NULL,
			                                                  &error);

			if (stmt) {
				if (!bind_arguments (stmt, args)) {
					retval = EXIT_FAILURE;
					goto out;
				}

				cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
			}

			if (error) {
				g_printerr ("%s, %s\n",
				            _("Could not run query"),
				            error->message);
				g_error_free (error);

				retval = EXIT_FAILURE;
				goto out;
			}

			if (G_UNLIKELY (strstr (query, "fts:offsets")))
				print_cursor_with_ftsoffsets (cursor, _("No results found matching your query"), _("Results"), FALSE);
			else
				print_cursor (cursor, _("No results found matching your query"), _("Results"), FALSE);
		}
	}

	g_object_unref (connection);

out:
	tracker_term_pager_close ();

	return retval;
}

static int
sparql_run_default (void)
{
	GOptionContext *context;
	gchar *help;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);
	help = g_option_context_get_help (context, FALSE, NULL);
	g_option_context_free (context);
	g_printerr ("%s\n", help);
	g_free (help);

	return EXIT_FAILURE;
}

static gboolean
sparql_options_enabled (void)
{
	return SPARQL_OPTIONS_ENABLED ();
}

int
tracker_query (int argc, const char **argv)
{
	GOptionContext *context;
	GError *error = NULL;
	const gchar *failed;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);

	argv[0] = "tinysparql query";

	if (!g_option_context_parse (context, &argc, (char***) &argv, &error)) {
		g_printerr ("%s, %s\n", _("Unrecognized options"), error->message);
		g_error_free (error);
		g_option_context_free (context);
		return EXIT_FAILURE;
	}

	if (argc > 1)
		query = g_strdup (argv[1]);

	g_option_context_free (context);

	if (file && query) {
		failed = _("File and query can not be used together");
	} else {
		failed = NULL;
	}

	if (failed) {
		g_printerr ("%s\n\n", failed);
		return EXIT_FAILURE;
	}

	if (sparql_options_enabled ()) {
		return sparql_run ();
	}

	return sparql_run_default ();
}
