/* Tracker
 * mbox routines
 * Copyright (C) 2005, Mr Jamie McCracken
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <unistd.h>
#include <glib/gstdio.h>

#include "tracker-mbox-evolution.h"

#ifdef HAVE_INOTIFY
#   include "tracker-inotify.h"
#else
#   ifdef HAVE_FAM
#      include "tracker-fam.h"
#   endif
#endif


extern Tracker *tracker;


#define EVOLUTION_MAIL_DIR ".evolution/mail"


static GSList *
find_evolution_mboxes (const char *evolution_dir)
{
	GSList *file_list;
	char   *filenames[] = {"Inbox", "Sent", NULL};
	char   *dir, **filename;
	size_t len;

	if (!evolution_dir) {
		return NULL;
	}

	len = strlen (evolution_dir);

	if (len && evolution_dir[len - 1] == G_DIR_SEPARATOR) {
		dir = g_strndup (evolution_dir, len - 1);
	} else {
		dir = g_strdup (evolution_dir);
	}

	file_list = NULL;

	for (filename = filenames; *filename; filename++) {
		char *file;

		file = g_build_filename (dir, "local", *filename, NULL);

		if (!g_access (file, F_OK) && !g_access (file, R_OK)) {
			file_list = g_slist_prepend (file_list, file);
		}
	}

	g_free (dir);

	return file_list;
}


GSList *
watch_emails_of_evolution (DBConnection *db_con)
{
	char	     *evolution_dir;
	GSList	     *list;
	const GSList *tmp;

	evolution_dir = g_build_filename (g_get_home_dir (), EVOLUTION_MAIL_DIR, NULL);

	list = find_evolution_mboxes (evolution_dir);

	g_free (evolution_dir);

	for (tmp = list; tmp; tmp = g_slist_next (tmp)) {
		const char *uri;
		char	   *base_dir;
		FileInfo   *info;

		uri = (char *) tmp->data;

		base_dir = g_path_get_dirname (uri);

		if (!tracker_is_directory_watched (base_dir, db_con)) {
			tracker_add_watch_dir (base_dir, db_con);
		}

		g_free (base_dir);


		info = tracker_create_file_info (uri, TRACKER_ACTION_CHECK, 0, WATCH_OTHER);

		info->is_directory = FALSE;
		info->mime = g_strdup ("application/mbox");

		g_async_queue_push (tracker->file_process_queue, info);
	}

	return list;
}


gboolean
is_in_a_evolution_mail_dir (const char *uri)
{
	char	 *path;
	int	 len;
	gboolean ret;

	path = g_build_filename (g_get_home_dir (), EVOLUTION_MAIL_DIR, NULL);

	len = strlen (path);

	ret = (strncmp (uri, path, len) == 0);

	g_free (path);

	return ret;
}
