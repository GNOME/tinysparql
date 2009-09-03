/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include <libtracker/tracker.h>
#include <libtracker-common/tracker-common.h>

static gint	      limit = 512;
static gint	      offset;
static gchar	    **terms;
static gboolean       detailed;

static GOptionEntry   entries[] = {
	{ "limit", 'l', 0, G_OPTION_ARG_INT, &limit,
	  N_("Limit the number of results shown"),
	  N_("512")
	},
	{ "offset", 'o', 0, G_OPTION_ARG_INT, &offset,
	  N_("Offset the results"),
	  N_("0")
	},
	{ "detailed", 'd', 0, G_OPTION_ARG_NONE, &detailed,
	  N_("Show more detailed results with service and mime type"),
	  NULL
	},
	{ G_OPTION_REMAINING, 0, 0,
	  G_OPTION_ARG_STRING_ARRAY, &terms,
	  N_("search terms"),
	  N_("EXPRESSION")
	},
	{ NULL }
};

static void
get_meta_table_data (gpointer value, gpointer user_data)
{
	gboolean pdetailed = GPOINTER_TO_INT (user_data);
	gchar **meta;
	gchar **p;
	gint	i;

	meta = value;

	for (p = meta, i = 0; *p; p++, i++) {
		if (i == 0) {
			g_print ("  %s", *p);
		} else if (pdetailed) {
			g_print (", %s", *p);
		}
	}

	g_print ("\n");
}


int
main (int argc, char **argv)
{
	TrackerClient	*client;
	GOptionContext	*context;
	GError		*error = NULL;
	gchar		*search, *temp, *query;
	gchar		*summary;
	GPtrArray	*array;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new (_("- Search files for certain terms"));

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.				*/
	summary = g_strconcat (_("Specifying multiple terms apply an AND "
				 "operator to the search performed"),
			       "\n",
			       _("This means if you search for 'foo' and 'bar', "
				 "they must BOTH exist"),
			       NULL);
	g_option_context_set_summary (context, summary);
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	g_free (summary);

	if (!terms) {
		gchar *help;

		g_printerr ("%s\n\n",
			    _("Search terms are missing"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	client = tracker_connect (FALSE, -1);

	if (!client) {
		g_printerr ("%s\n",
			    _("Could not establish a DBus connection to Tracker"));
		return EXIT_FAILURE;
	}

	if (limit <= 0) {
		limit = 512;
	}

	temp = g_strjoinv (" ", terms);
	search = g_strdup (temp); /* replace with escape function */
	g_free (temp);

	if (detailed) {
		query = g_strdup_printf ("SELECT ?s ?type ?mimeType WHERE { ?s fts:match \"%s\" ; rdf:type ?type . "
					 "OPTIONAL { ?s nie:mimeType ?mimeType } } OFFSET %d LIMIT %d",
					 search, offset, limit);
	} else {
		query = g_strdup_printf ("SELECT ?s WHERE { ?s fts:match \"%s\" } OFFSET %d LIMIT %d",
					 search, offset, limit);
	}

	array = tracker_resources_sparql_query (client, query, &error);

	g_free (search);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not get find detailed results by text"),
			    error->message);

		g_error_free (error);
		tracker_disconnect (client);

		return EXIT_FAILURE;
	}

	if (!array) {
		g_print ("%s\n",
			 _("No results found matching your query"));
	} else {
		g_print (tracker_dngettext (NULL,
					    _("Result: %d"), 
					    _("Results: %d"),
					    array->len),
			 array->len);
		g_print ("\n");
			g_ptr_array_foreach (array, get_meta_table_data, 
					     GINT_TO_POINTER (detailed));
		g_ptr_array_free (array, TRUE);
	}

	tracker_disconnect (client);
	return EXIT_SUCCESS;
}
