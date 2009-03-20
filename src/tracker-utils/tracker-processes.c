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

static GSList *
get_pids (void)
{
	GError *error = NULL;
	GDir *dir;
	GSList *pids = NULL;
	const gchar *name;

	dir = g_dir_open ("/proc", 0, &error);
	if (error) {
		g_printerr ("Could not open /proc, %s\n",
			    error ? error->message : "no error given");
		g_clear_error (&error);
		return NULL;
	}

	while ((name = g_dir_read_name (dir)) != NULL) { 
		gchar c;
		gboolean is_pid = TRUE;

		for (c = *name; c && c != ':' && is_pid; c++) {		
			is_pid &= g_ascii_isdigit (c);
		}

		if (!is_pid) {
			continue;
		}

		pids = g_slist_prepend (pids, g_strdup (name));
	}

	g_dir_close (dir);

	return g_slist_reverse (pids);
}

int
main (int argc, char **argv)
{
	GOptionContext *context;
	GError	       *error = NULL;
	GSList         *pids;
	GSList         *l;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new (_(" - Show the processes the tracker project is using"));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	pids = get_pids ();
	g_print ("Found %d pids...\n", g_slist_length (pids));

	for (l = pids; l; l = l->next) {
		gchar *filename;
		gchar *contents = NULL;
		gchar **strv;

		filename = g_build_filename ("/proc", l->data, "cmdline", NULL);
		g_file_get_contents (filename, &contents, NULL, &error);

		if (error) {
			g_printerr ("Could not open '%s', %s\n",
				    filename,
				    error ? error->message : "no error given");
			g_clear_error (&error);
			g_free (contents);
			g_free (filename);

			continue;
		}
		
		strv = g_strsplit (contents, "^@", 2);
		if (strv && strv[0]) {
			gchar *basename;

			basename = g_path_get_basename (strv[0]);
			if (g_str_has_prefix (basename, "tracker")) {
				g_print ("Found process ID:%s for '%s'\n",
					 (gchar*) l->data,
					 strv[0]);
			}

			g_free (basename);
		}

		g_strfreev (strv);
		g_free (contents);
		g_free (filename);
	}

	g_slist_foreach (pids, (GFunc) g_free, NULL);
	g_slist_free (pids);

	return EXIT_SUCCESS;
}
