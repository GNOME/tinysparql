/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <depot.h>

#include <glib.h>

#include <libtracker-db/tracker-db-index-item.h>

#define USAGE "Usage: print -f qdbm-file\n"

static gchar	    *filename;
static gboolean      print_services;

static GOptionEntry  entries[] = {
	{ "index-file", 'f', 0,
	  G_OPTION_ARG_STRING, &filename,
	  "QDBM index file",
	  NULL },
	{ "print-services", 's', 0,
	  G_OPTION_ARG_NONE, &print_services,
	  "Print service ID and service type ID for each word",
	  NULL },
	{ NULL }
};

static TrackerDBIndexItem *
get_word_hits (DEPOT	   *index,
	       const gchar *word,
	       guint	   *count)
{
	TrackerDBIndexItem *details;
	gint		    tsiz;
	gchar		   *tmp;

	g_return_val_if_fail (word != NULL, NULL);

	details = NULL;

	if (count) {
		*count = 0;
	}

	if ((tmp = dpget (index, word, -1, 0, 100, &tsiz)) != NULL) {
		if (tsiz >= (gint) sizeof (TrackerDBIndexItem)) {
			details = (TrackerDBIndexItem *) tmp;

			if (count) {
				*count = tsiz / sizeof (TrackerDBIndexItem);
			}
		}
	}

	return details;
}

static void
load_terms_from_index (gchar *filename)
{
    DEPOT	       *depot;
    gchar	       *key;
    guint		hits, i;
    TrackerDBIndexItem *results;

    depot = dpopen (filename, DP_OREADER | DP_ONOLCK, -1);

    if (depot == NULL) {
	   g_print ("Unable to open file: %s "
		    "(Could be a lock problem: is tracker running?)\n",
		    filename);
	   g_print ("Using version %s of qdbm\n",
		    dpversion);
	   return;
    }

    dpiterinit (depot);

    key = dpiternext (depot, NULL);

    while (key != NULL) {
	    g_print (" - %s ", key);

	    if (print_services) {
		    results = get_word_hits (depot, key, &hits);
		    for (i = 0; i < hits; i++) {
			    g_print (" (id:%d  t:%d s:%d) ",
				     tracker_db_index_item_get_id (&results[i]),
				     tracker_db_index_item_get_service_type (&results[i]),
				     tracker_db_index_item_get_score (&results[i]));
		    }
	    }

	    g_print ("\n");
	    g_free (key);
	    key = dpiternext (depot, NULL);
    }

    g_print ("Total: %d terms.\n", dprnum (depot));
    dpclose (depot);
}

int
main (gint argc, gchar** argv)
{
	GOptionContext *context;
	GError	       *error = NULL;

	context = g_option_context_new ("- QDBM index printer");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);

	if (error) {
		gchar *help;

		g_printerr ("Invalid arguments, %s\n", error->message);

		help = g_option_context_get_help (context, TRUE, NULL);
		g_printerr (help);

		g_free (help);
		g_clear_error (&error);
		g_option_context_free (context);

		return EXIT_FAILURE;
	}

	if (!filename) {
		gchar *help;

		help = g_option_context_get_help (context, TRUE, NULL);
		g_printerr (help);

		g_free (help);
		g_option_context_free (context);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	load_terms_from_index (filename);

	return EXIT_SUCCESS;
}
