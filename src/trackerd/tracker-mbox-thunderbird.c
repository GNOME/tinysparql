/* Tracker
 * mbox routines
 * Copyright (C) 2006, Laurent Aguerreche
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
#include <glib/gstdio.h>

#include "tracker-mbox-thunderbird.h"

#ifdef HAVE_INOTIFY
#   include "tracker-inotify.h"
#else
#   ifdef HAVE_FAM
#      include "tracker-fam.h"
#   endif
#endif


extern Tracker *tracker;


#define THUNDERBIRD_MAIL_DIR ".mozilla-thunderbird"


typedef struct {
	GSList	*mail_dirs;
} ThunderbirdPrefs;


static void
free_thunderbirdprefs_struct (ThunderbirdPrefs *prefs)
{
	if (!prefs) {
		return;
	}

	if (prefs->mail_dirs) {
		g_slist_foreach (prefs->mail_dirs, (GFunc) g_free, NULL);
		g_slist_free (prefs->mail_dirs);
	}

	g_free (prefs);
}


/*
 * Currently parser is only looking for server hostnames. Here it means that we
 * only want to know folders containing emails currently used by user.
 */
static ThunderbirdPrefs *
prefjs_parser (const char *filename)
{
	ThunderbirdPrefs *prefs;
	FILE		 *f;
	const char	 *prefix;
	size_t		 len_prefix;

	if (!filename) {
		return NULL;
	}

	prefs = g_new0 (ThunderbirdPrefs, 1);

	prefs->mail_dirs = NULL;

	f = g_fopen (filename, "r");

	prefix = "user_pref(\"mail.server.server";

	len_prefix = strlen (prefix);

	if (f) {
		char buf[512];

		while (fgets (buf, 512, f)) {

			if (g_str_has_prefix (buf, prefix)) {
				size_t	 i;
				char	 *end, *folder;

				/* at this point, we find an ID for the server:
				 *   mail.server.server.1    with 1 as ID.
				 * Complete string is something like:
				 *   "mail.server.server1.hostname", "foo.bar");
				 * so we are looking for "hostname\", \"" now...
				 */

				i = len_prefix + 1;

				/* ignore ID */
				while (buf[i] != '.') {
					if (buf[i] == ' ' || i >= 511) {
						goto error_continue;
					}
					i++;
				}

				/* we must have ".hostname\"" now */
				if (strncmp (buf + i, ".hostname\"", 10)) {
					goto error_continue;
				}

				i += 10;

				/* ignore characters until '"' */
				while (buf[i] != '"') {
					if (i >= 511) {
						goto error_continue;
					}
					i++;
				}

				/* find termination of line */
				end = strstr (buf + i + 1, "\");");

				if (!end) {
					goto error_continue;
				}

				/* and now we can read the hostname! */
				folder = g_strndup (buf + i + 1, end - (buf + i + 1));

				prefs->mail_dirs = g_slist_prepend (prefs->mail_dirs, folder);

			error_continue:
				continue;
			}
		}

		fclose (f);
	}

	return prefs;
}


static GSList *
add_inbox_mbox_of_dir (GSList *list_of_mboxes, const char *dir)
{
	GQueue *queue;

	queue = g_queue_new ();

	g_queue_push_tail (queue, g_strdup (dir));

	while (!g_queue_is_empty (queue)) {
		char *m_dir;
		GDir *dirp;

		m_dir = g_queue_pop_head (queue);

		if ((dirp = g_dir_open (m_dir, 0, NULL))) {
			const char *file;

			while ((file = g_dir_read_name (dirp))) {
				char *tmp_inbox, *inbox;

				if (!g_str_has_suffix (file, ".msf")) {
					continue;
				}

				/* we've found a mork file so it has a corresponding Inbox file and there
				   is perhaps a new directory to explore */

				tmp_inbox = g_strndup (file, strstr (file, ".msf") - file);

				inbox = g_build_filename (m_dir, tmp_inbox, NULL);

				g_free (tmp_inbox);

				if (inbox) {
					char *dir_to_explore;

					if (!g_access (inbox, F_OK) && !g_access (inbox, R_OK)) {
						list_of_mboxes = g_slist_prepend (list_of_mboxes, inbox);
					}

					dir_to_explore = g_strdup_printf ("%s.sbd", inbox);

					g_queue_push_tail (queue, dir_to_explore);
				}
			}

			g_dir_close (dirp);
		}

		g_free (m_dir);
	}

	g_queue_free (queue);

	return list_of_mboxes;
}


static GSList *
find_thunderbird_mboxes (GSList *list_of_mboxes, const char *profile_dir)
{
	char		 *profile;
	ThunderbirdPrefs *prefs;
	const GSList	 *mail_dir;

	if (!profile_dir) {
		return NULL;
	}

	profile = g_build_filename (profile_dir, "prefs.js", NULL);

	prefs = prefjs_parser (profile);

	g_free (profile);

	if (!prefs) {
		return NULL;
	}


	for (mail_dir = prefs->mail_dirs; mail_dir; mail_dir = g_slist_next (mail_dir)) {
		const char *tmp_dir;
		char	   *dir, *inbox, *sent, *inbox_dir;

		tmp_dir = (char *) mail_dir->data;

		dir = g_build_filename (profile_dir, "Mail", tmp_dir, NULL);

		/* on first level, we should have Inbox and Sent as mboxes, so we add them if they are present */

		/* Inbox */
		inbox = g_build_filename (dir, "Inbox", NULL);

		if (!g_access (inbox, F_OK) && !g_access (inbox, R_OK)) {
			list_of_mboxes = g_slist_prepend (list_of_mboxes, inbox);
		} else {
			g_free (inbox);
		}

		/* Sent */
		sent = g_build_filename (dir, "Sent", NULL);

		if (!g_access (sent, F_OK) && !g_access (sent, R_OK)) {
			list_of_mboxes = g_slist_prepend (list_of_mboxes, sent);
		} else {
			g_free (sent);
		}

		/* now we recursively find other "Inbox" mboxes */
		inbox_dir = g_build_filename (dir, "Inbox.sbd", NULL);

		list_of_mboxes = add_inbox_mbox_of_dir (list_of_mboxes, inbox_dir);

		g_free (inbox_dir);

		g_free (dir);
	}

	free_thunderbirdprefs_struct (prefs);

	return list_of_mboxes;
}


void
init_thunderbird_mboxes_module (void)
{
}


void
finalize_thunderbird_mboxes_module (void)
{
}


GSList *
watch_emails_of_thunderbird (DBConnection *db_con)
{
	char	     *thunderbird_dir, *thunderbird_profile_file;
	GKeyFile     *key_file;
	int	     i;
	GSList	     *profile_dirs, *list_of_mboxes;
	const GSList *tmp;

	if (!db_con) {
		return NULL;
	}

	thunderbird_dir = g_build_filename (g_get_home_dir (), THUNDERBIRD_MAIL_DIR, NULL);

	thunderbird_profile_file = g_build_filename (thunderbird_dir, "profiles.ini", NULL);

	if (!g_file_test (thunderbird_profile_file, G_FILE_TEST_EXISTS)) {
		g_free (thunderbird_profile_file);
		g_free (thunderbird_dir);
		return NULL;
	}


	key_file = g_key_file_new ();

	g_key_file_load_from_file (key_file, thunderbird_profile_file, G_KEY_FILE_NONE, NULL);

	g_free (thunderbird_profile_file);


	profile_dirs = NULL;

	for (i = 0; ; i++) {
		char *str_id, *profile_id;

		str_id = g_strdup_printf ("%d", i);

		profile_id = g_strconcat ("Profile", str_id, NULL);

		g_free (str_id);

		if (!g_key_file_has_group (key_file, profile_id)) {
			/* there isn't probably more profiles */
			g_free (profile_id);
			break;
		}

		if (g_key_file_has_key (key_file, profile_id, "Path", NULL)) {
			gboolean path_is_relative;
			char	 *path;

			path = g_key_file_get_string (key_file, profile_id, "Path", NULL);

			if (!path) {
				g_free (profile_id);
				continue;
			}

			if (g_key_file_has_key (key_file, profile_id, "IsRelative", NULL)) {
				path_is_relative = g_key_file_get_boolean (key_file, profile_id, "IsRelative", NULL);
			} else {
				path_is_relative = !g_path_is_absolute (path);
			}

			if (path_is_relative) {
				char *absolute_path;

				absolute_path = g_build_filename (thunderbird_dir, path, NULL);

				profile_dirs = g_slist_prepend (profile_dirs, absolute_path);
			} else {
				profile_dirs = g_slist_prepend (profile_dirs, g_strdup (path));
			}

			g_free (path);
		}

		g_free (profile_id);
	}

	g_free (thunderbird_dir);


	list_of_mboxes = NULL;

	for (tmp = profile_dirs; tmp; tmp = g_slist_next (tmp)) {
		const char *dir;

		dir = (char *) tmp->data;

		list_of_mboxes = find_thunderbird_mboxes (list_of_mboxes, dir);
	}


	for (tmp = list_of_mboxes; tmp; tmp = g_slist_next (tmp)) {
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

	return list_of_mboxes;
}


void
get_status_of_thunderbird_email (GMimeMessage *g_m_message, MailMessage *msg)
{
}


void
get_uri_of_thunderbird_email (GMimeMessage *g_m_message, MailMessage *msg)
{
}


gboolean
is_in_a_thunderbird_mail_dir (const char *uri)
{
	char	 *path;
	gboolean ret;

	if (!uri) {
		return FALSE;
	}

	path = g_build_filename (g_get_home_dir (), THUNDERBIRD_MAIL_DIR, NULL);

	ret = g_str_has_prefix (uri, path);

	g_free (path);

	return ret;
}
