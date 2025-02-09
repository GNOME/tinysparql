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
#include <gio/gunixoutputstream.h>

#include "tracker-sparql.h"
#include "tracker-color.h"

#include <tinysparql.h>
#include <tracker-common.h>

static gchar *database_path;
static gchar *dbus_service;
static gchar *remote_service;
static gchar *output_format;
static gboolean show_graphs;
static gchar **iris;

static GOptionEntry entries[] = {
	{ "database", 'd', 0, G_OPTION_ARG_FILENAME, &database_path,
	  N_("Location of the database"),
	  N_("FILE")
	},
	{ "dbus-service", 'b', 0, G_OPTION_ARG_STRING, &dbus_service,
	  N_("Connects to a DBus service"),
	  N_("DBus service name")
	},
	{ "output", 'o', 0, G_OPTION_ARG_STRING, &output_format,
	  N_("Output results format: “turtle”, “trig” or “json-ld”"),
	  N_("RDF_FORMAT")
	},
	{ "remote-service", 'r', 0, G_OPTION_ARG_STRING, &remote_service,
	  N_("Connects to a remote service"),
	  N_("Remote service URI")
	},
	{ "show-graphs", 'g', 0, G_OPTION_ARG_NONE, &show_graphs,
	  N_("Output TriG format which includes named graph information"),
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

static void
serialize_cb (GObject      *object,
              GAsyncResult *res,
              gpointer      user_data)
{
	GInputStream *istream;
	GOutputStream *ostream;
	GError *error = NULL;

	istream = tracker_sparql_connection_serialize_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                      res, &error);
	if (istream) {
		ostream = g_unix_output_stream_new (STDOUT_FILENO, FALSE);
		g_output_stream_splice (ostream, istream, G_OUTPUT_STREAM_SPLICE_NONE, NULL, &error);
		g_output_stream_close (ostream, NULL, NULL);
		g_object_unref (ostream);
	}

	if (error)
		g_printerr ("%s\n", error ? error->message : _("No error given"));

	g_object_unref (istream);
	g_main_loop_quit (user_data);
}

static int
export_run_default (void)
{
	g_autoptr(TrackerSparqlConnection) connection = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GString) query = NULL;
	g_autoptr(GMainLoop) loop = NULL;
	TrackerRdfFormat format;
	guint i;

	connection = create_connection (&error);

	if (!connection) {
		g_printerr ("%s: %s\n",
		            _("Could not establish a connection to Tracker"),
		            error ? error->message : _("No error given"));
		return EXIT_FAILURE;
	}

	query = g_string_new ("DESCRIBE ");

	if (iris) {
		for (i = 0; iris[i] != NULL; i++)
			g_string_append_printf (query, "<%s> ", iris[i]);
	} else {
		g_string_append (query,
		                 "?u {"
		                 "  ?u a rdfs:Resource . "
		                 "  FILTER NOT EXISTS { ?u a rdf:Property } "
		                 "  FILTER NOT EXISTS { ?u a rdfs:Class } "
		                 "  FILTER NOT EXISTS { ?u a nrl:Namespace } "
		                 "}");
	}

	loop = g_main_loop_new (NULL, FALSE);

	if (output_format) {
		/* Matches TrackerRdfFormat */
		const gchar *formats[] = {
			"turtle",
			"trig",
			"json-ld",
		};
		guint i;
		gboolean found = FALSE;

		G_STATIC_ASSERT (G_N_ELEMENTS (formats) == TRACKER_N_RDF_FORMATS);

		for (i = 0; i < G_N_ELEMENTS (formats); i++) {
			if (g_strcmp0 (formats[i], output_format) == 0) {
				format = i;
				found = TRUE;
				break;
			}
		}

		if (!found) {
			g_printerr (_("Unsupported serialization format “%s”\n"), output_format);
			return EXIT_FAILURE;
		}
	} else if (show_graphs) {
		format = TRACKER_RDF_FORMAT_TRIG;
	} else {
		format = TRACKER_RDF_FORMAT_TURTLE;
	}

	tracker_term_pipe_to_pager ();

	tracker_sparql_connection_serialize_async (connection,
	                                           TRACKER_SERIALIZE_FLAGS_NONE,
	                                           format,
	                                           query->str,
	                                           NULL, serialize_cb, loop);
	g_main_loop_run (loop);

	tracker_term_pager_close ();

	return EXIT_SUCCESS;
}

int
tracker_export (int argc, const char **argv)
{
	GOptionContext *context;
	GError *error = NULL;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);

	argv[0] = "tinysparql export";

	if (!g_option_context_parse (context, &argc, (char***) &argv, &error)) {
		g_printerr ("%s, %s\n", _("Unrecognized options"), error->message);
		g_error_free (error);
		g_option_context_free (context);
		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	return export_run_default ();
}
