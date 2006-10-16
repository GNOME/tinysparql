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

#include <stdlib.h>
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

enum {
	EVOLUTION_MESSAGE_ANSWERED	= 1 << 0,
	EVOLUTION_MESSAGE_DELETED	= 1 << 1,
	EVOLUTION_MESSAGE_DRAFT		= 1 << 2,
	EVOLUTION_MESSAGE_FLAGGED	= 1 << 3,
	EVOLUTION_MESSAGE_SEEN		= 1 << 4,
	EVOLUTION_MESSAGE_ATTACHMENTS	= 1 << 5,
	EVOLUTION_MESSAGE_ANSWERED_ALL	= 1 << 6,
	EVOLUTION_MESSAGE_JUNK		= 1 << 7,
	EVOLUTION_MESSAGE_SECURE	= 1 << 8
};


static GSList *
find_evolution_mboxes (const char *evolution_dir)
{
	GSList *file_list;
	char   *filenames[] = {"Inbox", "Sent", NULL};
	char   **filename;

	if (!evolution_dir) {
		return NULL;
	}

	file_list = NULL;

	for (filename = filenames; *filename; filename++) {
		char *file;

		file = g_build_filename (evolution_dir, "local", *filename, NULL);

		if (!g_access (file, F_OK) && !g_access (file, R_OK)) {
			file_list = g_slist_prepend (file_list, file);
		} else {
			g_free (file);
		}
	}

	return file_list;
}


void
init_evolution_mboxes_module (void)
{
}


void
finalize_evolution_mboxes_module (void)
{
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


void
get_status_of_evolution_email (GMimeMessage *g_m_message, MailMessage *msg)
{
	const char	  *field;
	char		  **parts;
	unsigned long int flags;

	if (!g_m_message || !msg) {
		return;
	}


	field = g_mime_message_get_header (g_m_message, "X-Evolution");

	if (!field) {
		return;
	}

	/* we want to split lines with that form: 00001fd3-0100 into 00001fd3 and 0100 */
	parts = g_strsplit (field, "-", -1);

	if (!parts || !parts[0] || !parts[1]) {
		g_strfreev (parts);
		return;
	}

	flags = strtoul (parts[0], NULL, 16);

	g_strfreev (parts);

	if ((flags & EVOLUTION_MESSAGE_DELETED) == EVOLUTION_MESSAGE_DELETED) {
		msg->deleted = TRUE;
	}

	if ((flags & EVOLUTION_MESSAGE_JUNK) == EVOLUTION_MESSAGE_JUNK) {
		msg->junk = TRUE;
	}
}


void
get_uri_of_evolution_email (GMimeMessage *g_m_message, MailMessage *msg)
{
	const char	  *field;
	char		  **parts;
	unsigned long int  uid;
	char		  *mbox_name;

	if (!g_m_message || !msg || !msg->parent_mbox) {
		return;
	}

	field = g_mime_message_get_header (g_m_message, "X-Evolution");

	if (!field) {
		return;
	}

	/* we want to split lines with that form: 00001fd3-0100 into 00001fd3 and 0100 */
	parts = g_strsplit (field, "-", -1);

	if (!parts || !parts[0] || !parts[1]) {
		g_strfreev (parts);
		return;
	}

	uid = strtoul (parts[1], NULL, 16);

	if (!uid) {
		return;
	}

	g_strfreev (parts);

	mbox_name = g_path_get_basename (msg->parent_mbox->mbox_uri);

	msg->uri = g_strdup_printf ("email://local@local/%s;uid=%X", mbox_name, (unsigned int) uid);

	g_free (mbox_name);
}


gboolean
is_in_a_evolution_mail_dir (const char *uri)
{
	char	 *path;
	gboolean ret;

	if (!uri) {
		return FALSE;
	}

	path = g_build_filename (g_get_home_dir (), EVOLUTION_MAIL_DIR, NULL);

	ret = g_str_has_prefix (uri, path);

	g_free (path);

	return ret;
}
