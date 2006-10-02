/* Tracker Tag
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)	
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
#include <stdlib.h>
#include <glib.h>

#include "../libtracker/tracker.h" 

static gchar **add = NULL;
static gchar **delete = NULL;
static gchar **search = NULL;
static gchar **files = NULL;
static gboolean remove_all = FALSE;
static gboolean list = FALSE;

static GOptionEntry entries[] = {
	{"add", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &add, "add tags to a file", "tag"},
	{"remove", 'r', 0, G_OPTION_ARG_STRING_ARRAY, &delete, "remove tags from a file", "tag"},
	{"remove-all", 'R', 0, G_OPTION_ARG_NONE, &remove_all, "remove all tags from  a file", NULL},
	{"list", 'l', 0, G_OPTION_ARG_NONE, &list, "list tags", NULL},
	{"search", 's', 0, G_OPTION_ARG_STRING_ARRAY, &search, "search for files with tags", "tag"},
	{G_OPTION_REMAINING, 0, G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING_ARRAY, &files, "files to tag", NULL},
	{NULL}
};


static void
get_meta_table_data (gpointer value)
		    
{
	char **meta, **meta_p;

	meta = (char **)value;

	int i = 0;
	for (meta_p = meta; *meta_p; meta_p++) {

		if (i == 0) {
			g_print ("%s : ", *meta_p);

		} else {
			g_print ("%s ", *meta_p);
		}
		i++;
	}
	g_print ("\n");
}

int 
main (int argc, char **argv) 
{
	TrackerClient *client = NULL;
	GOptionContext *context = NULL;
	GError *error = NULL;
	int i = 0;

	setlocale (LC_ALL, "");

	context = g_option_context_new ("file1 file2 ... - manipulate tags on files");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);

	if (error) {
		g_printerr ("%s\n", error->message);
		return 1;
	}

	if (((add || delete || remove_all) && !files) || (remove_all && (add || delete)) || (search && files)) {
		g_printerr ("invalid arguments - type 'tracker-tag --help' for info\n");
		return 1;
	}


	client = tracker_connect (FALSE);

	if (!client) {
		g_printerr ("could not initialise Tracker\n");
		return 1;
	}


	if (files)
		for (i = 0; files[i] != NULL; i++) {
			gchar *tmp = realpath (files[i], NULL);
			gchar *utf = g_filename_to_utf8(tmp, -1, NULL, NULL, NULL);
			g_free(files[i]);
			g_free(tmp);
			files[i] = utf;
		}

	if (add || delete || remove_all) {

		if (add)
			for (i = 0; add[i] != NULL; i++) {
				gchar *tmp = g_locale_to_utf8 (add[i], -1, NULL, NULL, NULL);
				g_free (add[i]);
				add[i] = tmp;
			}

		if (delete)
			for (i = 0; delete[i] != NULL; i++) {
				gchar *tmp = g_locale_to_utf8 (delete[i], -1, NULL, NULL, NULL);
				g_free (delete[i]);
				delete[i] = tmp;
			}


		for (i = 0; files[i] != NULL; i++) {

			if (remove_all) {
				tracker_keywords_remove_all (client, SERVICE_FILES, files[i], &error);

				if (error)
					g_printerr ("tracker threw error: %s\n", error->message);
			}

			if (add) {
				tracker_keywords_add (client, SERVICE_FILES, files[i], add, &error);

				if (error)
					g_printerr ("tracker threw error: %s\n", error->message);
			}

			if (delete) {
				tracker_keywords_remove (client, SERVICE_FILES, files[i], delete, &error);

				if (error)
					g_printerr ("tracker threw error: %s\n", error->message);
			}

		}

	}

	if (((list && !files) || (!files && (!remove_all && !delete && !add))) && !search) {

		GPtrArray *out_array = NULL;

		out_array = tracker_keywords_get_list (client, SERVICE_FILES, &error);

		if (error)
			goto error;

		if (out_array) {
			g_ptr_array_foreach (out_array, (GFunc)get_meta_table_data, NULL);
			g_ptr_array_free (out_array, TRUE);
		}

	}

	if ((list && files) || (files && (!remove_all && !delete && !add))) {
			
		int i = 0;

		for (i = 0; files[i] != NULL; i++) {

			int j = 0;
			gchar **tags = tracker_keywords_get (client, SERVICE_FILES, files[i], &error);

			if (error)
				g_printerr ("tracker threw error: %s\n", error->message);

			if (!tags)
				continue;

			g_print ("%s:", files[i]);
			for (j = 0; tags[j] != NULL; j++)
				g_print (" %s", tags[j]);
			g_print ("\n");

			g_strfreev (tags);

		}

	}

	if (search) {

		int i = 0;

		for (i = 0; search[i] != NULL; i++) {
			gchar *tmp = g_locale_to_utf8 (search[i], -1, NULL, NULL, NULL);
			g_free (search[i]);
			search[i] = tmp;
		}

		gchar **results = tracker_keywords_search (client, -1, SERVICE_FILES, search, 0, 512, &error);

		if (error)
			goto error;

		if (!results)
			g_print ("no results found matching your query\n");
		else
			for (i = 0; results[i] != NULL; i++)
				g_print ("%s\n", results[i]);

		g_strfreev (results);

	}

	tracker_disconnect (client);
	return 0;

error:
	g_printerr ("tracker threw error: %s\n", error->message);
	tracker_disconnect (client);
	return 1;	
}
