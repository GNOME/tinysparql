/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <config.h>

#include <locale.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "../libtracker/tracker.h" 

static gchar **add = NULL;
static gchar **delete = NULL;
static gchar **search = NULL;
static gchar **files = NULL;
static gboolean remove_all = FALSE;
static gboolean list = FALSE;

static GOptionEntry entries[] = {
	{"add", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &add, N_("Add specified tag to a file"), N_("TAG")},
	{"remove", 'r', 0, G_OPTION_ARG_STRING_ARRAY, &delete, N_("Remove specified tag from a file"), N_("TAG")},
	{"remove-all", 'R', 0, G_OPTION_ARG_NONE, &remove_all, N_("Remove all tags from a file"), NULL},
	{"list", 'l', 0, G_OPTION_ARG_NONE, &list, N_("List all defined tags"), NULL},
	{"search", 's', 0, G_OPTION_ARG_STRING_ARRAY, &search, N_("Search for files with specified tag"), N_("TAG")},
	{G_OPTION_REMAINING, 0, G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING_ARRAY, &files, N_("FILE..."), NULL},
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
	gchar *example;
	int i = 0, k;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, TRACKER_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the  */
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>     */
	context = g_option_context_new (_("FILE... - manipulate tags on files"));

	example = g_strconcat ("-a ", _("TAG"), " -a ", _("TAG"), " -a ", _("TAG"), NULL);


#ifdef HAVE_RECENT_GLIB
	/* Translators: this message will appear after the usage string */
	/* and before the list of options, showing an usage example.    */
	g_option_context_set_summary (context,
				      g_strconcat(_("To add, remove, or search for multiple tags "
						    "at the same time, join multiple options like:"), 
						  "\n\n\t", 
						  example, NULL));

#endif /* HAVE_RECENT_GLIB */

	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);

	g_option_context_free (context);
	g_free (example);
	
	if (error) {
		g_printerr ("%s: %s", argv[0], error->message);
		g_printerr ("\n");
		g_printerr (_("Try \"%s --help\" for more information."), argv[0]);
		g_printerr ("\n");
		return 1;
	}

	if (((add || delete || remove_all) && !files) || (remove_all && (add || delete)) || (search && files)) {
		g_printerr (_("%s: invalid arguments"), argv[0]);
		g_printerr ("\n");
		g_printerr (_("Try \"%s --help\" for more information."), argv[0]);
		g_printerr ("\n");
		return 1;
	}


	client = tracker_connect (FALSE);

	if (!client) {
		g_printerr (_("%s: no connection to tracker daemon"), argv[0]);
		g_printerr ("\n");
		g_printerr (_("Ensure \"trackerd\" is running before launch this command."));
		g_printerr ("\n");
		return 1;
	}


	if (files)
		for (i = 0; files[i] != NULL; i++) {
			gchar *tmp = realpath (files[i], NULL);
			if (tmp) {
				gchar *utf = g_filename_to_utf8 (tmp, -1, NULL, NULL, NULL);
				g_free (files[i]);
				g_free (tmp);
				files[i] = utf;
			} else {
				g_printerr (_("%s: file %s not found"), argv[0], files[i]);
				g_printerr ("\n");
				for (k = i; files[k] != NULL; k++)
					files[k] = files[k+1];
				i--; // Make sure we run over this file again
			}
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

				if (error) {
					g_printerr (_("%s: internal tracker error: %s"), argv[0], error->message);
					g_printerr ("\n");
				}
			}

			if (add) {
				tracker_keywords_add (client, SERVICE_FILES, files[i], add, &error);

				if (error) {
					g_printerr (_("%s: internal tracker error: %s"), argv[0], error->message);
					g_printerr ("\n");
				}
			}

			if (delete) {
				tracker_keywords_remove (client, SERVICE_FILES, files[i], delete, &error);

				if (error) {
					g_printerr (_("%s: internal tracker error: %s"), argv[0], error->message);
					g_printerr ("\n");
				} 
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

			if (error) {
				g_printerr (_("%s: internal tracker error: %s"), argv[0], error->message);
				g_printerr ("\n");
			}

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

		if (!results) {
			/* FIXME!! coreutilus don't print anything on no-results */
			g_print (_("No results found matching your query"));
			g_print ("\n");
		}
		else
			for (i = 0; results[i] != NULL; i++)
				g_print ("%s\n", results[i]);

		g_strfreev (results);

	}

	tracker_disconnect (client);
	return 0;

error:
	g_printerr (_("%s: internal tracker error: %s"), argv[0], error->message);
	g_printerr ("\n");
	tracker_disconnect (client);
	return 1;	
}
