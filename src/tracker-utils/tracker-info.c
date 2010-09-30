/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008-2010, Nokia <ivan.frade@nokia.com>
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

static gchar **filenames;
static gboolean full_namespaces;
static gboolean print_version;

static GOptionEntry entries[] = {
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
	  NULL,
	},
	{ "full-namespaces", 'f', 0, G_OPTION_ARG_NONE, &full_namespaces,
	  N_("Show full namespaces (i.e. don't use nie:title, use full URLs)"),
	  NULL,
	},
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames,
	  N_("FILE"),
	  N_("FILE")},
	{ NULL }
};

static gchar *
get_shorthand (GHashTable  *prefixes,
               const gchar *namespace)
{
	gchar *hash;

	hash = strrchr (namespace, '#');

	if (hash) {
		gchar *property;
		const gchar *prefix;

		property = hash + 1;
		*hash = '\0';

		prefix = g_hash_table_lookup (prefixes, namespace);

		return g_strdup_printf ("%s:%s", prefix, property);
	}

	return g_strdup (namespace);
}

static gboolean
has_valid_uri_scheme (const gchar *uri)
{
	const gchar *s;

	s = uri;

	if (!g_ascii_isalpha (*s)) {
		return FALSE;
	}

	do {
		s++;
	} while (g_ascii_isalnum (*s) || *s == '+' || *s == '.' || *s == '-');

	return (*s == ':');
}

static GHashTable *
get_prefixes (TrackerSparqlConnection *connection)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GHashTable *retval;
	const gchar *query;

	retval = g_hash_table_new_full (g_str_hash,
	                                g_str_equal,
	                                NULL,
	                                NULL);

	/* FIXME: Would like to get this in the same SPARQL that we
	 * use to get the info, but doesn't seem possible at the
	 * moment with the limited string manipulation features we
	 * support in SPARQL.
	 */
	query = "SELECT ?ns ?prefix "
	        "WHERE {"
	        "  ?ns a tracker:Namespace ;"
	        "  tracker:prefix ?prefix "
	        "}";

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		const gchar *key, *value;

		key = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		value = tracker_sparql_cursor_get_string (cursor, 1, NULL);

		if (!key || !value) {
			continue;
		}

		g_hash_table_insert (retval,
		                     g_strndup (key, strlen (key) - 1),
		                     g_strdup (value));
	}

	if (cursor) {
		g_object_unref (cursor);
	}

	return retval;
}

int
main (int argc, char **argv)
{
	TrackerSparqlConnection *connection;
	GOptionContext *context;
	GError *error = NULL;
	GHashTable *prefixes;
	gchar **p;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the  */
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>     */
	context = g_option_context_new (_("- Get all information about one or more files"));

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.                              */
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (print_version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		g_option_context_free (context);

		return EXIT_SUCCESS;
	}

	if (!filenames) {
		gchar *help;

		g_printerr ("%s\n\n",
		            _("One or more files have not been specified"));

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

	connection = tracker_sparql_connection_get (NULL, &error);

	if (!connection) {
		g_printerr ("%s: %s\n",
		            _("Could not establish a connection to Tracker"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	prefixes = get_prefixes (connection);

	for (p = filenames; *p; p++) {
		TrackerSparqlCursor *cursor;
		GError *error = NULL;
		gchar *uri;
		gchar *query;
		gchar *urn;

		g_print ("%s:'%s'\n", _("Querying information for entity"), *p);

		/* support both, URIs and local file paths */
		if (has_valid_uri_scheme (*p)) {
			uri = g_strdup (*p);
		} else {
			GFile *file;

			file = g_file_new_for_commandline_arg (*p);
			uri = g_file_get_uri (file);
			g_object_unref (file);
		}

		/* First check whether there's some entity with nie:url like this */
		query = g_strdup_printf ("SELECT ?urn WHERE { ?urn nie:url \"%s\" }", uri);
		cursor = tracker_sparql_connection_query (connection, query, NULL, &error);
		g_free (query);

		if (error) {
			g_printerr ("  %s, %s\n",
			            _("Unable to retrieve URN for URI"),
			            error->message);

			g_clear_error (&error);
			continue;
		}

		if (!cursor || !tracker_sparql_cursor_next (cursor, NULL, &error)) {
			if (error) {
				g_printerr ("  %s, %s\n",
				            _("Unable to retrieve data for URI"),
					    error->message);

				g_object_unref (cursor);
				g_clear_error (&error);

				continue;
			}

			/* No URN matches, use uri as URN */
			urn = g_strdup (uri);
		} else {
			urn = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
			g_print ("  '%s'\n", urn);

			g_object_unref (cursor);
		}

		query = g_strdup_printf ("SELECT ?predicate ?object WHERE { <%s> ?predicate ?object }", urn);

		cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

		g_free (uri);
		g_free (query);

		if (error) {
			g_printerr ("  %s, %s\n",
			            _("Unable to retrieve data for URI"),
			            error->message);

			g_clear_error (&error);
			continue;
		}

		if (!cursor) {
			g_print ("  %s\n",
			         _("No metadata available for that URI"));
		} else {
			g_print ("%s:\n", _("Results"));

			while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
				const gchar *key = tracker_sparql_cursor_get_string (cursor, 0, NULL);
				const gchar *value = tracker_sparql_cursor_get_string (cursor, 1, NULL);

				if (!key || !value) {
					continue;
				}

				/* Don't display nie:plainTextContent */
				if (strcmp (key, "http://www.semanticdesktop.org/ontologies/2007/01/19/nie#plainTextContent") == 0) {
					continue;
				}

				if (G_UNLIKELY (full_namespaces)) {
					g_print ("  '%s' = '%s'\n", key, value);
				} else {
					gchar *shorthand;

					shorthand = get_shorthand (prefixes, key);
					g_print ("  '%s' = '%s'\n", shorthand, value);
					g_free (shorthand);
				}
			}

			g_print ("\n");

			g_object_unref (cursor);
		}

		g_print ("\n");
	}

	g_hash_table_unref (prefixes);
	g_object_unref (connection);

	return EXIT_SUCCESS;
}
