/* Tracker Search
 * Copyright (C) 2005, Mr Jamie McCracken (jamiemcc@gnome.org)	
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA. 
 */

#include <locale.h>
#include <glib.h>

#include "../libtracker/tracker.h" 

static GMainLoop *loop;

static gint limit = 0;
static gchar **terms = NULL;

static GOptionEntry entries[] = {
	{"limit", 'l', 0, G_OPTION_ARG_INT, &limit, "limit the number of results showed", "limit"},
	{G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &terms, "search terms", NULL},
	{NULL}
};

static void
callback (char **result, GError *error, gpointer user_data)
{
	
	char **p_strarray;
	
	if (error) {
		g_printerr ("tracker raised error: %s\n", error->message);
		g_error_free (error);
		return;
	}

	if (!result) {
		g_printerr ("no results found\n");
		return;
	}
	
	for (p_strarray = result; *p_strarray; p_strarray++) {

		char *s = g_locale_from_utf8 (*p_strarray, -1, NULL, NULL, NULL);

		if (!s)
			continue;

		g_print ("%s\n", *p_strarray);
		g_free (s);

	}

	g_strfreev (result);

	g_main_loop_quit (loop);
}


int 
main (int argc, char **argv) 
{
	GOptionContext *context = NULL;
	TrackerClient *client = NULL;
	GError *error = NULL;
	gchar *search;

	setlocale (LC_ALL, "");

	context = g_option_context_new ("search terms ... - search files for certain terms (ANDed)");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);

	if (error) {
		g_printerr ("invalid arguments: %s\n", error->message);
		return 1;
	}

	if (!terms) {
		g_printerr ("missing search terms, try --help for help\n");
		return 1;
	}

	if (limit <= 0)
		limit = 512;

	client = tracker_connect (FALSE);

	if (!client) {
		g_printerr ("could not connect to Tracker\n");
		return 1;
	}

	search = g_strjoinv (" ", terms);
	tracker_search_text_async (client, -1, SERVICE_FILES, search, 0, limit, FALSE, callback, NULL);
	g_free (search);

	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);

	tracker_disconnect (client);
	return 0;
}
