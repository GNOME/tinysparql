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

#include <libtracker-client/tracker-client.h>
#include <libtracker-common/tracker-common.h>

#define ABOUT	  \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE	  \
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

static void
print_property_value (gpointer data,
		      gpointer user_data)
{
	GHashTable *prefixes;
	gchar **pair;

	prefixes = user_data;
	pair = data;

	if (!pair[0] || !pair[1]) {
		return;
	}

	if (G_UNLIKELY (full_namespaces)) {
		g_print ("  '%s' = '%s'\n", pair[0], pair[1]);
	} else {
		gchar *shorthand;

		shorthand = get_shorthand (prefixes, pair[0]);
		g_print ("  '%s' = '%s'\n", shorthand, pair[1]);
		g_free (shorthand);
	}
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
get_prefixes (TrackerClient *client)
{
	GPtrArray *results;
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
	query = "SELECT ?prefix ?ns "
		"WHERE {"
		"  ?ns a tracker:Namespace ;"
		"  tracker:prefix ?prefix "
		"}";

	results = tracker_resources_sparql_query (client, query, NULL);

	if (results) {
		gint i;

		/* First remove all parts after and including '#' */
		for (i = 0; i < results->len; i++) {
			gchar **data;
			gchar *key, *value;
			
			data = g_ptr_array_index (results, i);

			if (!data || !data[1]) {
				continue;
			}

			key = g_strndup (data[1], strlen (data[1]) - 1);
			value = g_strdup (data[0]);

			g_hash_table_insert (retval, key, value);
		}

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}

	return retval;
}

int
main (int argc, char **argv)
{
	TrackerClient *client;
	GOptionContext *context;
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

	client = tracker_client_new (0, G_MAXINT);

	if (!client) {
		g_printerr ("%s\n",
		            _("Could not establish a D-Bus connection to Tracker"));
		return EXIT_FAILURE;
	}

	prefixes = get_prefixes (client);

	for (p = filenames; *p; p++) {
		GPtrArray *results;
		GError *error = NULL;
		gchar *uri;
		gchar *query;
		gchar *urn;

		g_print ("%s:'%s'\n",
		         _("Querying information for entity"),
		         *p);

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
		results = tracker_resources_sparql_query (client, query, &error);
		g_free (query);

		if (error) {
			g_printerr ("  %s, %s\n",
			            _("Unable to retrieve URN for URI"),
			            error->message);

			g_error_free (error);
			continue;
		}

		if (!results || results->len == 0) {
			/* No URN matches, use uri as URN */
			urn = g_strdup (uri);
		} else {
			gchar **args;

			args = g_ptr_array_index (results, 0);
			urn = g_strdup (args[0]);

			g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
			g_ptr_array_free (results, TRUE);

			g_print ("  '%s'\n", urn);
		}

		query = g_strdup_printf ("SELECT ?predicate ?object WHERE { <%s> ?predicate ?object }", urn);

		results = tracker_resources_sparql_query (client, query, &error);

		g_free (uri);
		g_free (query);

		if (error) {
			g_printerr ("  %s, %s\n",
			            _("Unable to retrieve data for URI"),
			            error->message);

			g_error_free (error);
			continue;
		}

		if (!results) {
			g_print ("  %s\n",
			         _("No metadata available for that URI"));
		} else {
			gint length;

			length = results->len;

			g_print (g_dngettext (NULL,
			                      "Result: %d",
			                      "Results: %d",
			                      length),
			         length);
			g_print ("\n");

			g_ptr_array_foreach (results, (GFunc) print_property_value, prefixes);
			g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
			g_ptr_array_free (results, TRUE);
		}

		g_print ("\n");
	}

	g_hash_table_unref (prefixes);
	g_object_unref (client);

	return EXIT_SUCCESS;
}
