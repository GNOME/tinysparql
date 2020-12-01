/*
 * Copyright (C) 2020, Sam Thursfield <sam@afuera.me.uk>
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

#include "tracker-sparql.h"
#include "tracker-color.h"

#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-common/tracker-common.h>
#include <libtracker-data/tracker-data.h>

static gchar *database_path;
static gchar *dbus_service;
static gchar *remote_service;
static gboolean show_graphs;
static gchar **iris;
static gchar *data_type = NULL;
static gboolean keyfile = FALSE;

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
	{ "show-graphs", 'g', 0, G_OPTION_ARG_NONE, &show_graphs,
	  N_("Output TriG format which includes named graph information"),
	  NULL
	},
        { "2to3", 0, 0, G_OPTION_ARG_STRING, &data_type,
          NULL,
          NULL
        },
        { "keyfile", 0, 0, G_OPTION_ARG_NONE, &keyfile,
          NULL,
          NULL
        },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &iris,
	  N_("IRI"),
	  N_("IRI")},
	{ NULL }
};

static TrackerSparqlConnection *
create_connection (GError **error)
{
	if (database_path && !dbus_service && !remote_service) {
		GFile *file;

		file = g_file_new_for_commandline_arg (database_path);
		return tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_READONLY,
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
		g_printerr (_("Specify one “--database”, “--dbus-service” or “--remote-service” option"));
		exit (EXIT_FAILURE);
	}
}

/* format a URI for Turtle; if it has a prefix, display uri
 * as prefix:rest_of_uri; if not, display as <uri>
 */
inline static gchar *
format_urn (GHashTable  *prefixes,
            const gchar *urn,
            gboolean     full_namespaces)
{
	gchar *urn_out;

	if (full_namespaces) {
		urn_out = g_strdup_printf ("<%s>", urn);
	} else {
		gchar *shorthand = tracker_sparql_get_shorthand (prefixes, urn);

		/* If the shorthand is the same as the urn passed, we
		 * assume it is a resource and pass it in as one,
		 *
		 *   e.g.: http://purl.org/dc/elements/1.1/date
		 *     to: http://purl.org/dc/elements/1.1/date
		 *
		 * Otherwise, we use the shorthand version instead.
		 *
		 *   e.g.: http://www.w3.org/1999/02/22-rdf-syntax-ns
		 *     to: rdf
		 */
		if (g_strcmp0 (shorthand, urn) == 0) {
			urn_out = g_strdup_printf ("<%s>", urn);
			g_free (shorthand);
		} else {
			urn_out = shorthand;
		}
	}

	return urn_out;
}

/* print a URI prefix in Turtle format */
static void
print_prefix (gpointer key,
              gpointer value,
              gpointer user_data)
{
	g_print ("@prefix %s: <%s#> .\n", (gchar *) value, (gchar *) key);
}

/* Print triples in Turtle format */
static void
print_turtle (TrackerSparqlCursor *cursor,
              GHashTable          *prefixes,
              gboolean             full_namespaces)
{
	gchar *predicate;
	gchar *object;

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		const gchar *resource = tracker_sparql_cursor_get_string (cursor, 1, NULL);
		const gchar *key = tracker_sparql_cursor_get_string (cursor, 2, NULL);
		const gchar *value = tracker_sparql_cursor_get_string (cursor, 3, NULL);
		const gchar *value_is_resource = tracker_sparql_cursor_get_string (cursor, 4, NULL);

		if (!resource || !key || !value || !value_is_resource) {
			continue;
		}

		/* Don't display nie:plainTextContent */
		//if (!plain_text_content && strcmp (key, "http://tracker.api.gnome.org/ontology/v3/nie#plainTextContent") == 0) {
		//	continue;
		//}

		predicate = format_urn (prefixes, key, full_namespaces);

		if (g_ascii_strcasecmp (value_is_resource, "true") == 0) {
			object = g_strdup_printf ("<%s>", value);
		} else {
			gchar *escaped_value;

			/* Escape value and make sure it is encapsulated properly */
			escaped_value = tracker_sparql_escape_string (value);
			object = g_strdup_printf ("\"%s\"", escaped_value);
			g_free (escaped_value);
		}

		/* Print final statement */
		g_print ("<%s> %s %s .\n", resource, predicate, object);

		g_free (predicate);
		g_free (object);
	}
}

/* Print graphs and triples in TriG format */
static void
print_trig (TrackerSparqlCursor *cursor,
            GHashTable          *prefixes,
            gboolean             full_namespaces)
{
	gchar *predicate;
	gchar *object;
	gchar *previous_graph = NULL;
	const gchar *graph = NULL;

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		graph = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		const gchar *resource = tracker_sparql_cursor_get_string (cursor, 1, NULL);
		const gchar *key = tracker_sparql_cursor_get_string (cursor, 2, NULL);
		const gchar *value = tracker_sparql_cursor_get_string (cursor, 3, NULL);
		const gchar *value_is_resource = tracker_sparql_cursor_get_string (cursor, 4, NULL);

		if (!resource || !key || !value || !value_is_resource) {
			continue;
		}

		if (g_strcmp0 (previous_graph, graph) != 0) {
			if (previous_graph != NULL) {
				/* Close previous graph */
				g_print ("}\n");
				g_free (previous_graph);
			}
			previous_graph = g_strdup (graph);
			g_print ("GRAPH <%s>\n{\n", graph);
		}

		/* Don't display nie:plainTextContent */
		//if (!plain_text_content && strcmp (key, "http://tracker.api.gnome.org/ontology/v3/nie#plainTextContent") == 0) {
		//	continue;
		//}

		predicate = format_urn (prefixes, key, full_namespaces);

		if (g_ascii_strcasecmp (value_is_resource, "true") == 0) {
			object = g_strdup_printf ("<%s>", value);
		} else {
			gchar *escaped_value;

			/* Escape value and make sure it is encapsulated properly */
			escaped_value = tracker_sparql_escape_string (value);
			object = g_strdup_printf ("\"%s\"", escaped_value);
			g_free (escaped_value);
		}

		/* Print final statement */
		g_print ("  <%s> %s %s .\n", resource, predicate, object);

		g_free (predicate);
		g_free (object);
	}

	if (graph != NULL) {
		g_print ("}\n");
	}
	g_free (previous_graph);
}

static void
print_keyfile (TrackerSparqlCursor *cursor)
{
	GKeyFile *key_file;
	gchar *data;

	key_file = g_key_file_new ();

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		const gchar *resource = tracker_sparql_cursor_get_string (cursor, 1, NULL);
		const gchar *key = tracker_sparql_cursor_get_string (cursor, 2, NULL);
		const gchar *value = tracker_sparql_cursor_get_string (cursor, 3, NULL);
		GStrv values;
		gsize n_items;

		values = g_key_file_get_string_list (key_file, resource, key, &n_items, NULL);

		if (values) {
			GArray *array;

			array = g_array_new (TRUE, TRUE, sizeof (char*));
			g_array_append_vals (array, values, n_items);
			g_array_append_val (array, value);

			g_key_file_set_string_list (key_file, resource, key,
			                            (const gchar * const *) array->data,
			                            array->len);
		} else {
			g_key_file_set_string (key_file, resource, key, value);
		}

		g_strfreev (values);
	}

	data = g_key_file_to_data (key_file, NULL, NULL);
	g_print ("%s\n", data);
}

static int
export_run_default (void)
{
	g_autoptr(TrackerSparqlConnection) connection = NULL;
	g_autoptr(TrackerSparqlCursor) cursor = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GHashTable) prefixes = NULL;
	g_autoptr(GString) query = NULL;
	guint i;

	connection = create_connection (&error);

	if (!connection) {
		g_printerr ("%s: %s\n",
		            _("Could not establish a connection to Tracker"),
		            error ? error->message : _("No error given"));
		return EXIT_FAILURE;
	}

	prefixes = tracker_sparql_get_prefixes (connection);

	query = g_string_new (NULL);
	g_string_append (query,
	                 "SELECT ?g ?u ?p ?v "
	                 "       (EXISTS { ?p rdfs:range [ rdfs:subClassOf rdfs:Resource ] }) AS ?is_resource "
	                 "{ "
	                 "    GRAPH ?g { "
	                 "        ?u ?p ?v ");

	if (iris) {
		g_string_append (query, "FILTER (?u IN (");

		for (i = 0; iris[i]; i++) {
			if (i != 0)
				g_string_append_c (query, ',');
			g_string_append_printf (query, "<%s>", iris[i]);
		}

		g_string_append (query, "))");
	} else {
		g_string_append (query,
		                 "FILTER NOT EXISTS { ?u a rdf:Property } "
		                 "FILTER NOT EXISTS { ?u a rdfs:Class } "
		                 "FILTER NOT EXISTS { ?u a nrl:Namespace } ");
	}

	g_string_append (query,
	                 "    } "
	                 "} ORDER BY ?g ?u");

	cursor = tracker_sparql_connection_query (connection, query->str, NULL, &error);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not run query"),
		            error->message);
		return EXIT_FAILURE;
	}

	tracker_term_pipe_to_pager ();

	g_hash_table_foreach (prefixes, (GHFunc) print_prefix, NULL);
	g_print ("\n");

	if (show_graphs) {
		print_trig (cursor, prefixes, FALSE);
	} else {
		print_turtle (cursor, prefixes, FALSE);
	}

	tracker_term_pager_close ();

	return EXIT_SUCCESS;
}

/* Execute a query and export the resulting triples or quads to stdout.
 *
 * The query should return quads (graph, subject, predicate, object) plus an extra
 * boolean column that is false when the 'object' value is a simple type or a resource.
 */
static gboolean
export_2to3_with_query (const gchar  *query,
                        gboolean      show_graphs,
                        GError      **error)
{
	g_autoptr(TrackerDBManager) db_manager = NULL;
	TrackerDBInterface *iface = NULL;
	TrackerDBStatement *stmt = NULL;
	TrackerSparqlCursor *cursor = NULL;
	GError *inner_error = NULL;
	g_autoptr(GFile) store = NULL;
	g_autofree char *path = NULL;

	path = g_build_filename (g_get_user_cache_dir (),
	                         "tracker", NULL);
	store = g_file_new_for_path (path);

	db_manager = tracker_db_manager_new (TRACKER_DB_MANAGER_READONLY |
	                                     TRACKER_DB_MANAGER_SKIP_VERSION_CHECK,
	                                     store,
	                                     NULL, FALSE,
	                                     1, 1, NULL, NULL, NULL, NULL, &inner_error);

	if (inner_error) {
		g_propagate_prefixed_error (error, inner_error,
		                            "%s: ", _("Could not run query"));
		g_object_unref (db_manager);
		return FALSE;
	}

	iface = tracker_db_manager_get_writable_db_interface (db_manager);

	stmt = tracker_db_interface_create_statement (iface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              &inner_error,
	                                              query);
	if (!stmt) {
		g_propagate_prefixed_error (error, inner_error,
		                            "%s: ", _("Could not run query"));
		g_object_unref (db_manager);
		return FALSE;
	}

	cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, &inner_error));
	g_object_unref (stmt);

	if (!cursor) {
		g_propagate_prefixed_error (error, inner_error,
		                            "%s: ", _("Could not run query"));
		g_object_unref (db_manager);
		return FALSE;
	}

	if (keyfile) {
		print_keyfile (cursor);
	} if (show_graphs) {
		print_trig (cursor, NULL, FALSE);
	} else {
		print_turtle (cursor, NULL, FALSE);
	}

	g_object_unref (cursor);

	return TRUE;
}

static int
export_2to3_run_files_starred (void)
{
	const gchar *query;
	g_autoptr(GError) error = NULL;

	query = "SELECT "
		"  \"\" ,"
		"  COALESCE ((SELECT \"nie:url\" FROM \"nie:DataObject\" WHERE ID = \"v_u\" ) ,"
		"            (SELECT Uri FROM Resource WHERE ID = \"v_u\" ) ) ,"
		"  \"v_p\","
		"  \"v_v\","
		"  'true' "
		"FROM ("
		"  SELECT * "
		"  FROM (("
		"    SELECT "
		"      \"v_u\" ,"
		"      'rdf:type' AS \"v_p\" ,"
		"      'nfo:FileDataObject' AS \"v_v\""
		"    FROM ("
		"      SELECT"
		"        \"nfo:FileDataObject1\".\"ID\" AS \"v_u\""
		"      FROM \"nfo:FileDataObject\" AS \"nfo:FileDataObject1\" ,"
		"           \"rdfs:Resource_nao:hasTag\" AS \"rdfs:Resource_nao:hasTag2\""
		"      WHERE \"nfo:FileDataObject1\".\"ID\" = \"rdfs:Resource_nao:hasTag2\".\"ID\""
		"        AND \"rdfs:Resource_nao:hasTag2\".\"nao:hasTag\" ="
		"            COALESCE ((SELECT ID FROM Resource WHERE Uri = 'urn:gnome:nautilus:starred' ), 0) ) ) )"
		"  UNION ALL"
		"  SELECT *"
		"  FROM (("
		"    SELECT"
		"      \"v_u\" ,"
		"      \"nao:hasTag\" AS \"v_p\" ,"
		"      \"urn:gnome:nautilus:starred\" AS \"v_v\""
		"    FROM (SELECT \"nfo:FileDataObject1\".\"ID\" AS \"v_u\""
		"          FROM \"nfo:FileDataObject\" AS \"nfo:FileDataObject1\" ,"
		"               \"rdfs:Resource_nao:hasTag\" AS \"rdfs:Resource_nao:hasTag2\""
		"          WHERE \"nfo:FileDataObject1\".\"ID\" = \"rdfs:Resource_nao:hasTag2\".\"ID\""
		"            AND \"rdfs:Resource_nao:hasTag2\".\"nao:hasTag\" ="
		"                COALESCE ((SELECT ID FROM Resource WHERE Uri = 'urn:gnome:nautilus:starred' ), 0) ) ) ) )"
		"ORDER BY (SELECT Uri FROM Resource WHERE ID = \"v_u\" )";

	export_2to3_with_query (query, FALSE, &error);

	if (error) {
		g_printerr ("%s\n", error->message);
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}

static gint
export_2to3_run (void)
{
	if (strcmp (data_type, "files-starred") == 0) {
		return export_2to3_run_files_starred ();
	}

	g_print ("Options: files-starred\n");

	return EXIT_FAILURE;
}

int
tracker_export (int argc, const char **argv)
{
	GOptionContext *context;
	GError *error = NULL;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);

	argv[0] = "tracker export";

	if (!g_option_context_parse (context, &argc, (char***) &argv, &error)) {
		g_printerr ("%s, %s\n", _("Unrecognized options"), error->message);
		g_error_free (error);
		g_option_context_free (context);
		return EXIT_FAILURE;
	}

	g_option_context_free (context);

        if (data_type) {
		return export_2to3_run ();
        }

	return export_run_default ();
}
