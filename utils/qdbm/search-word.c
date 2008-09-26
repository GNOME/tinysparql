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

#include <glib.h>

#include <depot.h>

#include <libtracker-db/tracker-db-index-item.h>

static gchar	    *filename;
static gchar	    *word;

static GOptionEntry  entries[] = {
	{ "index-file", 'f', 0,
	  G_OPTION_ARG_STRING, &filename,
	  "QDBM index file",
	  NULL },
	{ "word", 'w', 0,
	  G_OPTION_ARG_STRING, &word,
	  "Print service ID and service type ID of every hit for this word",
	  NULL },
	{ NULL }
};

static TrackerDBIndexItem *
get_word_hits (DEPOT	   *index,
	       const gchar *word,
	       guint	   *count)
{
	TrackerDBIndexItem *items;
	gchar		   *tmp;
	gint		    tsiz;

	g_return_val_if_fail (word != NULL, NULL);

	items = NULL;

	if (count) {
		*count = 0;
	}

	if ((tmp = dpget (index, word, -1, 0, 1000000, &tsiz)) != NULL) {
		if (tsiz >= (gint) sizeof (TrackerDBIndexItem)) {
			items = (TrackerDBIndexItem *) tmp;

			if (count) {
				*count = tsiz / sizeof (TrackerDBIndexItem);
			}
		}
	}

	return items;
}

static void
show_term_in_index (const gchar *filename,
		    const gchar *word)
{
    TrackerDBIndexItem *items;
    DEPOT	       *depot;
    guint		hits, i;

    hits = 0;

    depot = dpopen (filename, DP_OREADER, -1);

    if (depot == NULL) {
	   g_print ("Unable to open file: %s "
		    "(Could be a lock problem: is tracker running?)\n",
		    filename);
	   g_print ("Using version %s of qdbm\n",
		    dpversion);
	   return;
    }

    items = get_word_hits (depot, word, &hits);

    if (hits < 1 ) {
	    g_print ("No results for %s\n", word);
	    return;
    }

    g_print (" - %s ", word);

    for (i = 0; i < hits; i++) {
	    g_print (" (id:%d  t:%d) ",
		     items[i].id,
		     tracker_db_index_item_get_service_type (&items[i]));
    }

    g_print ("\n");

    g_print ("Total: %d terms.\n", dprnum (depot));
    dpclose (depot);
}

int
main (gint argc, gchar** argv)
{
	GOptionContext *context;
	GError	       *error = NULL;

	context = g_option_context_new ("- QDBM index searcher");
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

	if (!filename || !word) {
		gchar *help;

		help = g_option_context_get_help (context, TRUE, NULL);
		g_printerr (help);

		g_free (help);
		g_option_context_free (context);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	show_term_in_index (filename, word);

	return 0;
}
