/* Tracker
 * routines for emails
 * Copyright (C) 2006, Laurent Aguerreche (laurent.aguerreche@free.fr)
 *
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

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <glib/gstdio.h>

#include "tracker-db-email.h"
#include "tracker-email-utils.h"
#include "tracker-email-evolution.h"
#include "tracker-email-thunderbird.h"
#include "tracker-email-kmail.h"

#ifdef HAVE_INOTIFY
#   include "tracker-inotify.h"
#else
#   ifdef HAVE_FAM
#      include "tracker-fam.h"
#   endif
#endif


extern Tracker *tracker;

static char	*root_path_for_attachments = NULL;


static void	mh_watch_mail_messages_in_dir	(DBConnection *db_con, const char *dir_path);
static GSList *	add_gmime_references		(GSList *list, GMimeMessage *message, const char *header);
static GSList *	add_recipients			(GSList *list, GMimeMessage *message, const char *type);
static void	find_attachment			(GMimeObject *obj, gpointer data);




/********************************************************************************************
 Public functions
*********************************************************************************************/

void
email_set_root_path_for_attachments (const char *path)
{
	if (root_path_for_attachments) {
		g_free (root_path_for_attachments);
	}

	root_path_for_attachments = g_strdup (path);
}


const char *
email_get_root_path_for_attachments (void)
{
	return root_path_for_attachments;
}


void
email_free_root_path_for_attachments (void)
{
	if (root_path_for_attachments) {
		g_free (root_path_for_attachments);
		root_path_for_attachments = NULL;
	}
}


gboolean
email_parse_and_save_mail_message (DBConnection *db_con, MailApplication mail_app, const char *path, LoadHelperFct load_helper)
{
	MailMessage *mail_msg;

	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (path, FALSE);

	mail_msg = email_parse_mail_message_by_path (mail_app, path, load_helper);

	if (!mail_msg) {
		return FALSE;
	}

	tracker_db_email_save_email (db_con, mail_msg);

	email_index_each_email_attachment (db_con, mail_msg);

	email_free_mail_message (mail_msg);

	return TRUE;
}


gboolean
email_parse_mail_file_and_save_new_emails (DBConnection *db_con, MailApplication mail_app, const char *path, LoadHelperFct load_helper)
{
	off_t		offset;
	MailFile	*mf;
	MailMessage	*mail_msg;

	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (path, FALSE);

	offset = tracker_db_email_get_last_mbox_offset (db_con, path);

	mf = email_open_mail_file_at_offset (mail_app, path, offset, TRUE);

	while ((mail_msg = email_mail_file_parse_next (mf, load_helper))) {

		if (!tracker->is_running) {
			email_free_mail_message (mail_msg);
			email_free_mail_file (mf);

			return TRUE;
		}

		tracker_db_email_update_mbox_offset (db_con, mf);

		tracker_db_email_save_email (db_con, mail_msg);

		email_index_each_email_attachment (db_con, mail_msg);

		email_free_mail_message (mail_msg);
	}

	email_free_mail_file (mf);

	return TRUE;
}


gboolean
email_mh_is_in_a_mh_dir (const char *path)
{
	/* We only take care of files into directory "inbox", "sent" or "trash".
	   So we check that string "/inbox/" is into the path or that path ends with "/inbox" for instance. */

	return (strstr (path, G_DIR_SEPARATOR_S "inbox" G_DIR_SEPARATOR_S) != NULL || g_str_has_suffix (path, G_DIR_SEPARATOR_S "inbox") ||
		strstr (path, G_DIR_SEPARATOR_S "sent" G_DIR_SEPARATOR_S) != NULL  || g_str_has_suffix (path, G_DIR_SEPARATOR_S "sent") ||
		strstr (path, G_DIR_SEPARATOR_S "trash" G_DIR_SEPARATOR_S) != NULL || g_str_has_suffix (path, G_DIR_SEPARATOR_S "trash"));
}


void
email_mh_watch_mail_messages (DBConnection *db_con, const char *path)
{
	char *mail_dirs[] = {"inbox", "sent", "trash", NULL};
	char **dir_name;

	g_return_if_fail (db_con);
	g_return_if_fail (path);

	if (tracker_file_is_no_watched (path)) {
		return;
	}

	for (dir_name = mail_dirs; *dir_name; dir_name++) {
		char *dir_path;

		dir_path = g_build_filename (path, *dir_name, NULL);

		if (tracker_is_directory (dir_path)) {
			mh_watch_mail_messages_in_dir (db_con, dir_path);
		}

		g_free (dir_path);
	}

}


gboolean
email_maildir_is_in_a_maildir_dir (const char *path)
{
	/* We only take care of files into directory "cur".
	   So we check that string "/cur/" is into the path or that path ends with "/cur". */

	return (strstr (path, G_DIR_SEPARATOR_S "cur" G_DIR_SEPARATOR_S) != NULL ||
		g_str_has_suffix (path, G_DIR_SEPARATOR_S "cur"));
}


void
email_maildir_watch_mail_messages (DBConnection *db_con, const char *path)
{
	char *dir_cur;

	g_return_if_fail (db_con);
	g_return_if_fail (path);

	if (tracker_file_is_no_watched (path)) {
		return;
	}

	dir_cur = g_build_filename (path, "cur", NULL);

	if (tracker_is_directory (dir_cur)) {
		GSList		*files;
		const GSList	*file;
		GPatternSpec	*pattern;

		if (!tracker_is_directory_watched (dir_cur, db_con)) {
			tracker_add_watch_dir (dir_cur, db_con);
		}

		files = tracker_get_files (dir_cur, FALSE);

		/* We only want files that contain mail message so we check their names. */
		pattern = g_pattern_spec_new ("*.*.*");

		for (file = files; file; file = file->next) {
			const char *file_path;

			file_path = file->data;

			if (g_pattern_match_string (pattern, file_path)) {
				tracker_db_insert_pending_file (db_con, 0, file_path, NULL, 0, TRACKER_ACTION_CHECK, FALSE, FALSE, -1);
			}
		}

		g_pattern_spec_free (pattern);

		g_slist_foreach (files, (GFunc) g_free, NULL);
		g_slist_free (files);
	}

	g_free (dir_cur);
}


MailFile *
email_open_mail_file_at_offset (MailApplication mail_app, const char *path, off_t offset, gboolean scan_from_for_mbox)
{
	char		*path_in_locale;
	FILE		*f;
	GMimeStream	*stream;
	GMimeParser	*parser;
	MailFile	*mf;

	g_return_val_if_fail (path, NULL);

	path_in_locale = g_filename_from_utf8 (path, -1, NULL, NULL, NULL);

	if (!path_in_locale) {
		tracker_log ("******ERROR**** path could not be converted to locale format");
		g_free (path_in_locale);
		return NULL;
	}

	f = g_fopen (path_in_locale, "r");

	g_free (path_in_locale);

	if (!f) {
		return NULL;
	}

	stream = g_mime_stream_file_new_with_bounds (f, offset, -1);

	if (!stream) {
		fclose (f);
		return NULL;
	}

	parser = g_mime_parser_new_with_stream (stream);
	g_object_unref (stream);

	if (!parser) {
		return NULL;
	}

	g_mime_parser_set_scan_from (parser, scan_from_for_mbox);


	mf = g_new0 (MailFile, 1);

	mf->path = g_strdup (path);
	mf->mail_app = mail_app;
	mf->parser = parser;
	mf->next_email_offset = offset;

	return mf;
}


void
email_free_mail_file (MailFile *mf)
{
	if (!mf) {
		return;
	}

	if (mf->path) {
		g_free (mf->path);
	}

	g_object_unref (mf->parser);

	g_free (mf);
}


MailPerson *
email_allocate_mail_person (void)
{
	return g_new0 (MailPerson, 1);
}


void
email_free_mail_person (MailPerson *mp)
{
	if (!mp) {
		return;
	}

	g_free (mp->name);
	g_free (mp->addr);

	g_free (mp);
}


MailMessage *
email_allocate_mail_message (void)
{
	return g_new0 (MailMessage, 1);
}


void
email_free_mail_message (MailMessage *mail_msg)
{
	if (!mail_msg) {
		return;
	}

	/* we do not free parent_mail_file of course... */

	if (mail_msg->path) {
		g_free (mail_msg->path);
	}

	if (mail_msg->uri) {
		g_free (mail_msg->uri);
	}

	if (mail_msg->message_id) {
		g_free (mail_msg->message_id);
	}

	if (mail_msg->reply_to) {
		g_free (mail_msg->reply_to);
	}

	if (mail_msg->references) {
		g_slist_foreach (mail_msg->references, (GFunc) g_free, NULL);
		g_slist_free (mail_msg->references);
	}

	if (mail_msg->in_reply_to_ids) {
		g_slist_foreach (mail_msg->in_reply_to_ids, (GFunc) g_free, NULL);
		g_slist_free (mail_msg->in_reply_to_ids);
	}

	if (mail_msg->from) {
		g_free (mail_msg->from);
	}

	if (mail_msg->to) {
		g_slist_foreach (mail_msg->to, (GFunc) email_free_mail_person, NULL);
		g_slist_free (mail_msg->to);
	}

	if (mail_msg->cc) {
		g_slist_foreach (mail_msg->cc, (GFunc) email_free_mail_person, NULL);
		g_slist_free (mail_msg->cc);
	}

	if (mail_msg->bcc) {
		g_slist_foreach (mail_msg->bcc, (GFunc) email_free_mail_person, NULL);
		g_slist_free (mail_msg->bcc);
	}

	if (mail_msg->subject) {
		g_free (mail_msg->subject);
	}

	if (mail_msg->content_type) {
		g_free (mail_msg->content_type);
	}

	if (mail_msg->body) {
		g_free (mail_msg->body);
	}

	if (mail_msg->path_to_attachments) {
		g_free (mail_msg->path_to_attachments);
	}

	if (mail_msg->attachments) {
		GSList *tmp;

		for (tmp = mail_msg->attachments; tmp; tmp = tmp->next) {
			MailAttachment *ma;

			ma = tmp->data;

			if (ma->attachment_name) {
				g_free (ma->attachment_name);
			}

			if (ma->mime) {
				g_free (ma->mime);
			}

			if (ma->tmp_decoded_file) {
				g_free (ma->tmp_decoded_file);
			}

			g_free (ma);
		}

		g_slist_free (mail_msg->attachments);
	}

	g_free (mail_msg);
}


MailMessage *
email_mail_file_parse_next (MailFile *mf, LoadHelperFct load_helper)
{
	MailMessage	*mail_msg;
	guint64		msg_offset;
	GMimeMessage	*g_m_message;
	time_t		date;
	int		gmt_offset;
	gboolean	is_html;

	g_return_val_if_fail (mf, NULL);

	msg_offset = g_mime_parser_tell (mf->parser);

	g_m_message = g_mime_parser_construct_message (mf->parser);

	mf->next_email_offset = g_mime_parser_tell (mf->parser);

	if (!g_m_message) {
		return NULL;
	}

	mail_msg = g_new0 (MailMessage, 1);

	mail_msg->parent_mail_file = mf;
	mail_msg->offset = msg_offset;
	mail_msg->message_id = g_strdup (g_mime_message_get_message_id (g_m_message));
	mail_msg->reply_to = g_strdup (g_mime_message_get_reply_to (g_m_message));

	/* default */
	mail_msg->deleted = FALSE;
	mail_msg->junk = FALSE;

	mail_msg->references = add_gmime_references (NULL, g_m_message, "References");
	mail_msg->in_reply_to_ids = add_gmime_references (NULL, g_m_message, "In-Reply-To");

	g_mime_message_get_date (g_m_message, &date, &gmt_offset);
	mail_msg->date = (long) (date + gmt_offset);

	mail_msg->from = g_strdup (g_mime_message_get_sender (g_m_message));

	mail_msg->to = add_recipients (NULL, g_m_message, GMIME_RECIPIENT_TYPE_TO);
	mail_msg->cc = add_recipients (NULL, g_m_message, GMIME_RECIPIENT_TYPE_CC);
	mail_msg->bcc = add_recipients (NULL, g_m_message, GMIME_RECIPIENT_TYPE_BCC);

	mail_msg->subject = g_strdup (g_mime_message_get_subject (g_m_message));

	mail_msg->body = g_mime_message_get_body (g_m_message, TRUE, &is_html);
	mail_msg->content_type = g_strdup (is_html ? "text/html" : "text/plain");

	if (load_helper) {
		load_helper (g_m_message, mail_msg);
	}

	/* make directory to save attachment */
	mail_msg->path_to_attachments = g_build_filename (email_get_root_path_for_attachments (), mail_msg->parent_mail_file->path, mail_msg->message_id, NULL);
	g_mkdir_with_parents (mail_msg->path_to_attachments, 0700);

	mail_msg->attachments = NULL;

	/* find then save attachments in sys tmp directory of Tracker and save entries in MailMessage struct */
	g_mime_message_foreach_part (g_m_message, find_attachment, mail_msg);

	g_object_unref (g_m_message);

	if (!mail_msg->attachments) {

		/* no attachment found, so we remove directory immediately */
		g_rmdir (mail_msg->path_to_attachments);

		/* and we say there is not a path to attachments */
		if (mail_msg->path_to_attachments) {
			g_free (mail_msg->path_to_attachments);
			mail_msg->path_to_attachments = NULL;
		}
	}

	return mail_msg;
}


MailMessage *
email_parse_mail_message_by_path (MailApplication mail_app, const char *path, LoadHelperFct load_helper)
{
	MailFile	*mf;
	MailMessage	*mail_msg;

	g_return_val_if_fail (path, NULL);

	mf = email_open_mail_file_at_offset (mail_app, path, 0, FALSE);
	g_return_val_if_fail (mf, NULL);

	mail_msg = email_mail_file_parse_next (mf, load_helper);

	if (mail_msg) {
		mail_msg->path = g_strdup (path);
	}

	email_free_mail_file (mf);

	/* there is not a mail file parent with a simple mail message */
	mail_msg->parent_mail_file = NULL;

	return mail_msg;
}


void
email_index_each_email_attachment (DBConnection *db_con, const MailMessage *mail_msg)
{
	const GSList *tmp;

        g_return_if_fail (db_con);
	g_return_if_fail (mail_msg);

	for (tmp = mail_msg->attachments; tmp; tmp = tmp->next) {
		const MailAttachment	*ma;
		FileInfo		*info;

		ma = tmp->data;

		info = tracker_create_file_info (ma->tmp_decoded_file, TRACKER_ACTION_CHECK, 0, WATCH_OTHER);
		info->is_directory = FALSE;
		info->mime = g_strdup (ma->mime);

		g_async_queue_push (tracker->file_process_queue, info);
		tracker_notify_file_data_available ();
	}
}




/********************************************************************************************
 Private functions
*********************************************************************************************/

static void
mh_watch_mail_messages_in_dir (DBConnection *db_con, const char *dir_path)
{
	GSList		*files;
	const GSList	*file;

	g_return_if_fail (db_con);
	g_return_if_fail (dir_path);

	if (tracker_file_is_no_watched (dir_path)) {
		return;
	}

	if (!tracker_is_directory_watched (dir_path, db_con)) {
		tracker_add_watch_dir (dir_path, db_con);
	}

	files = tracker_get_files (dir_path, FALSE);

	for (file = files; file; file = file->next) {
		const char *file_path;

		file_path = file->data;

		if (tracker_file_is_indexable (file_path)) {
			char *file_name, *p;

			/* filename must contain only digits */
			file_name = g_path_get_basename (file_path);

			for (p = file_name; *p != '\0'; p++) {
				if (!g_ascii_isdigit (*p)) {
					goto end;
				}
			}

			tracker_db_insert_pending_file (db_con, 0, file_path, NULL, 0, TRACKER_ACTION_CHECK, FALSE, FALSE, -1);

		end:
			g_free (file_name);
		}
	}

	g_slist_foreach (files, (GFunc) g_free, NULL);
	g_slist_free (files);
}


static GSList *
add_gmime_references (GSList *list, GMimeMessage *message, const char *header)
{
	const char *tmp;

	tmp = g_mime_message_get_header (message, header);

	if (tmp) {
		GMimeReferences *refs;
		const GMimeReferences *tmp_ref;

		refs = g_mime_references_decode (tmp);

		for (tmp_ref = refs; tmp_ref; tmp_ref = tmp_ref->next) {
			const char *msgid;

			msgid = tmp_ref->msgid;

			if (msgid && msgid[0] != '\0') {
				list = g_slist_prepend (list, g_strdup (msgid));
			}
		}

		g_mime_references_clear (&refs);
	}

	return list;
}


static GSList *
add_recipients (GSList *list, GMimeMessage *message, const char *type)
{
	const InternetAddressList *addrs_list;

	for (addrs_list = g_mime_message_get_recipients (message, type); addrs_list; addrs_list = addrs_list->next) {
		MailPerson *mp;

		mp = email_allocate_mail_person ();

		mp->name = g_strdup (addrs_list->address->name);
		mp->addr = g_strdup (addrs_list->address->value.addr);

		list = g_slist_prepend (list, mp);
	}

	return list;
}


static void
find_attachment (GMimeObject *obj, gpointer data)
{
	GMimePart   *part;
	MailMessage *mail_msg;
	const char  *content_disposition;

	g_return_if_fail (obj);
	g_return_if_fail (data);

	if (GMIME_IS_MESSAGE_PART (obj)) {
		GMimeMessage *g_msg;

		g_msg = g_mime_message_part_get_message (GMIME_MESSAGE_PART (obj));
		if (g_msg) {
			g_mime_message_foreach_part (g_msg, find_attachment, data);
			g_object_unref (g_msg);
		}
		return;

	} else if (GMIME_IS_MULTIPART (obj)) {
		g_mime_multipart_foreach (GMIME_MULTIPART (obj), find_attachment, data);
		return;

	} else if (!GMIME_IS_PART (obj)) {
		/* What's this object?! */
		g_return_if_reached ();
	}

	part = GMIME_PART (obj);
	mail_msg = data;

	content_disposition = g_mime_part_get_content_disposition (part);

	/* test whether it is a mail attachment */
	if (content_disposition &&
	    (strcmp (content_disposition, GMIME_DISPOSITION_ATTACHMENT) == 0 ||
	     strcmp (content_disposition, GMIME_DISPOSITION_INLINE) == 0)) {
		const GMimeContentType	*content_type;
		MailAttachment		*ma;
		FILE			*f;

		content_type = g_mime_part_get_content_type (part);

		if (!content_type->params) {
			/* we are unable to identify mime type */
			return;
		}

		ma = g_new0 (MailAttachment, 1);

		ma->attachment_name = g_strdup (g_mime_part_get_filename (part));
		ma->mime = g_strconcat (content_type->type, "/", content_type->subtype, NULL);
		ma->tmp_decoded_file  = g_build_filename (mail_msg->path_to_attachments, ma->attachment_name, NULL);

		f = g_fopen (ma->tmp_decoded_file, "w");

		if (f) {
			GMimeStream	 *stream_tmp_file;
			GMimeDataWrapper *wrapper;

			stream_tmp_file = g_mime_stream_file_new (f);
			wrapper = g_mime_part_get_content_object (part);
			g_mime_data_wrapper_write_to_stream (wrapper, stream_tmp_file);

			mail_msg->attachments = g_slist_prepend (mail_msg->attachments, ma);

			g_object_unref (wrapper);
			g_object_unref (stream_tmp_file);

		} else {
			g_free (ma->attachment_name);
			g_free (ma->mime);
			g_free (ma->tmp_decoded_file);
			g_free (ma);
		}
	}
}
