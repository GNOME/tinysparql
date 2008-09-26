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
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libtracker/tracker.h>

#ifdef G_OS_WIN32
#include <trackerd/mingw-compat.h>
#endif /* G_OS_WIN32 */

static gint	     limit = 512;
static gint	     offset;
static gchar	   **add;
static gchar	   **remove;
static gchar	   **search;
static gchar	   **files;
static gboolean      remove_all;
static gboolean      list;

static GOptionEntry entries[] = {
	{ "add", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &add,
	  N_("Add specified tag to a file"),
	  N_("TAG")
	},
	{ "remove", 'r', 0, G_OPTION_ARG_STRING_ARRAY, &remove,
	  N_("Remove specified tag from a file"),
	  N_("TAG")
	},
	{ "remove-all", 'R', 0, G_OPTION_ARG_NONE, &remove_all,
	  N_("Remove all tags from a file"),
	  NULL
	},
	{ "list", 't', 0, G_OPTION_ARG_NONE, &list,
	  N_("List all defined tags"),
	  NULL
	},
	{ "limit", 'l', 0, G_OPTION_ARG_INT, &limit,
	  N_("Limit the number of results shown"),
	  N_("512")
	},
	{ "offset", 'o', 0, G_OPTION_ARG_INT, &offset,
	  N_("Offset the results"),
	  N_("0")
	},
	{ "search", 's', 0, G_OPTION_ARG_STRING_ARRAY, &search,
	  N_("Search for files with specified tag"),
	  N_("TAG")
	},
	{ G_OPTION_REMAINING, 0,
	  G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING_ARRAY, &files,
	  N_("FILE..."),
	  NULL},
	{ NULL }
};

static void
get_meta_table_data (gpointer value)
{
	gchar **meta;
	gchar **p;
	gint	i;

	meta = value;

	for (p = meta, i = 0; *p; p++, i++) {
		if (i == 0) {
			g_print ("  %s", *p);
		} else {
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
	const gchar	*failed = NULL;
	gchar		*summary;
	gchar	       **files_resolved = NULL;
	gchar	       **search_resolved = NULL;
	gchar	       **tags_to_add = NULL;
	gchar	       **tags_to_remove = NULL;
	gchar		*error_str = NULL;
	gint		 i, j;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the
	 * usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>
	 */
	context = g_option_context_new (_("Add, remove or search for tags"));

	/* Translators: this message will appear after the usage string
	 * and before the list of options, showing an usage example.
	 */
	summary = g_strconcat (_("To add, remove, or search for multiple tags "
				 "at the same time, join multiple options, for example:"),
			       "\n"
			       "\n"
			       "  -a ", _("TAG"), " -a ", _("TAG"), " -a ", _("TAG"),
			       NULL);

	g_option_context_set_summary (context, summary);
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_free (summary);

	/* Check arguments */
	if ((add || remove || remove_all) && !files) {
		failed = _("No files were specified");
	} else if ((add || remove) && remove_all) {
		failed = _("Add and delete actions can not be used with remove all actions");
	} else if (search && files) {
		failed = _("Files are not needed with searching");
	} else if (!add && !remove && !remove_all && !files && !search && !list) {
		failed = _("No arguments were provided");
	}

	if (failed) {
		gchar *help;

		g_printerr ("%s\n\n", failed);

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr (help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	client = tracker_connect (FALSE);

	if (!client) {
		g_printerr ("%s\n",
			    _("Could not establish a DBus connection to Tracker"));
		return EXIT_FAILURE;
	}

	if (files) {
		files_resolved = g_new0 (gchar*, g_strv_length (files) + 1);

		for (i = 0, j = 0; files[i] != NULL; i++) {
			/* GFile *file; */
			/* gchar *path; */

			/* file = g_file_new_for_commandline_arg (files[i]); */
			/* path = g_file_get_path (file); */
			/* g_object_unref (file); */

			files_resolved[j++] = g_strdup (files[i]);
		}

		files_resolved[j] = NULL;
	}

	if (add || remove || remove_all) {
		if (add) {
			tags_to_add = g_new0 (gchar*, g_strv_length (add) + 1);

			for (i = 0, j = 0; add[i] != NULL; i++) {
				gchar *path;

				path = g_locale_to_utf8 (add[i], -1, NULL, NULL, NULL);
				if (path) {
					tags_to_add[j++] = path;
				}
			}
		}

		if (remove) {
			tags_to_remove = g_new0 (gchar*, g_strv_length (remove) + 1);

			for (i = 0, j = 0; remove[i] != NULL; i++) {
				gchar *path;

				path = g_locale_to_utf8 (remove[i], -1, NULL, NULL, NULL);
				if (path) {
					tags_to_remove[j++] = path;
				}
			}
		}

		for (i = 0; files_resolved[i] != NULL; i++) {
			if (remove_all) {
				tracker_keywords_remove_all (client,
							     SERVICE_FILES,
							     files_resolved[i],
							     &error);

				if (error) {
					gchar *str;

					str = g_strdup_printf (_("Could not remove all tags for '%s'"),
							       files_resolved[i]);
					g_printerr ("%s, %s\n",
						    str,
						    error->message);
					g_free (str);
					g_clear_error (&error);
				}
			}

			if (tags_to_add) {
				tracker_keywords_add (client,
						      SERVICE_FILES,
						      files_resolved[i],
						      tags_to_add,
						      &error);

				if (error) {
					gchar *str;

					str = g_strdup_printf (_("Could not add tags for '%s'"),
							       files_resolved[i]);
					g_printerr ("%s, %s\n",
						    str,
						    error->message);
					g_free (str);
					g_clear_error (&error);
				}
			}

			if (tags_to_remove) {
				tracker_keywords_remove (client,
							 SERVICE_FILES,
							 files_resolved[i],
							 tags_to_remove,
							 &error);

				if (error) {
					gchar *str;

					str = g_strdup_printf (_("Could not remove tags for '%s'"),
							       files_resolved[i]);
					g_printerr ("%s, %s\n",
						    str,
						    error->message);
					g_free (str);
					g_clear_error (&error);
				}
			}
		}
	}

	if (((!files && list) ||
	     (!files && (!add && !remove && !remove_all))) && !search) {
		GPtrArray *array;

		array = tracker_keywords_get_list (client,
						   SERVICE_FILES,
						   &error);

		if (error) {
			error_str = g_strdup (_("Could not get tag list"));
			goto finish;
		}

		if (!array) {
			g_print ("%s\n",
				 _("No tags found"));
		} else {
			g_print ("%s:\n",
				 _("All tags"));

			g_ptr_array_foreach (array, (GFunc) get_meta_table_data, NULL);
			g_ptr_array_free (array, TRUE);
		}
	}

	if ((files && list) ||
	    (files && (!add && !remove && !remove_all))) {
		g_print ("%s:\n",
			 _("Tags found"));

		for (i = 0, j = 0; files_resolved[i] != NULL; i++) {
			gchar **tags;

			tags = tracker_keywords_get (client,
						     SERVICE_FILES,
						     files_resolved[i],
						     &error);

			if (error) {
				error_str = g_strdup_printf (_("Could not get tags for file:'%s'"),
							     files_resolved[i]);
				goto finish;
			}

			if (!tags) {
				continue;
			}

			g_print ("  '%s': ", files_resolved[i]);

			for (j = 0; tags[j] != NULL; j++) {
				if (j > 0) {
					g_print ("|");
				}

				g_print ("%s", tags[j]);
			}

			g_print ("\n");

			g_strfreev (tags);
		}
	}

	if (search) {
		gchar **results;

		search_resolved = g_new0 (gchar*, g_strv_length (search) + 1);

		for (i = 0, j = 0; search[i] != NULL; i++) {
			gchar *str;

			str = g_locale_to_utf8 (search[i], -1, NULL, NULL, NULL);
			search_resolved[j++] = str;
		}

		search_resolved[j] = NULL;

		results = tracker_keywords_search (client, -1,
						   SERVICE_FILES,
						   search_resolved,
						   offset,
						   limit,
						   &error);

		if (error) {
			error_str = g_strdup (_("Could not search tags"));
			goto finish;
		}

		if (!results) {
			g_print ("%s\n",
				 _("No tags found"));
		} else {
			for (i = 0; results[i] != NULL; i++) {
				g_print ("  %s\n", results[i]);
			}
		}

		g_strfreev (results);
	}

finish:
	g_strfreev (tags_to_remove);
	g_strfreev (tags_to_add);
	g_strfreev (search_resolved);
	g_strfreev (files_resolved);

	tracker_disconnect (client);

	if (error_str) {
		g_printerr ("%s, %s\n",
			    error_str,
			    error->message);
		g_free (error_str);
		g_clear_error (&error);

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
