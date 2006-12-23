/* Tracker
 * mbox routines
 * Copyright (C) 2006, Laurent Aguerreche
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

#include "tracker-mbox.h"
#include "tracker-mbox-evolution.h"
#include "tracker-mbox-thunderbird.h"
#include "tracker-mbox-kmail.h"


extern Tracker *tracker;


static char   *base_path_for_attachments = NULL;
static GSList *mboxes = NULL;


/* must be called before any work on mbox files */
void
tracker_watch_emails (DBConnection *db_con)
{
	g_return_if_fail (tracker && tracker->sys_tmp_root_dir);

	if (!tracker->index_evolution_emails && !tracker->index_thunderbird_emails && !tracker->index_kmail_emails) {
		return;
	}

	if (!base_path_for_attachments) {
		char *pid;

		pid = g_strdup_printf ("%d", getpid ());
		base_path_for_attachments = g_build_filename (tracker->sys_tmp_root_dir, "mail-attachments", NULL);
		g_free (pid);
	}

	g_mime_init (0);


	mboxes = NULL;

	if (tracker->index_evolution_emails) {
		init_evolution_mboxes_module ();
		mboxes = g_slist_concat (mboxes, watch_emails_of_evolution (db_con));
	}

	if (tracker->index_thunderbird_emails) {
		init_thunderbird_mboxes_module ();
		mboxes = g_slist_concat (mboxes, watch_emails_of_thunderbird (db_con));
	}

	if (tracker->index_kmail_emails) {
		init_kmail_mboxes_module ();
		mboxes = g_slist_concat (mboxes, watch_emails_of_kmail (db_con));
	}
}


void
tracker_end_email_watching (void)
{
	g_slist_foreach (mboxes, (GFunc) g_free, NULL);
	g_slist_free (mboxes);
	mboxes = NULL;

	if (base_path_for_attachments) {
		g_free (base_path_for_attachments);
		base_path_for_attachments = NULL;
	}

	if (tracker->index_evolution_emails) {
		finalize_evolution_mboxes_module ();
	}

	if (tracker->index_thunderbird_emails) {
		finalize_thunderbird_mboxes_module ();
	}

	if (tracker->index_kmail_emails) {
		finalize_kmail_mboxes_module ();
	}

	g_mime_shutdown ();
}


gboolean
tracker_is_in_a_application_mail_dir (const char *uri)
{
	return (is_in_a_evolution_mail_dir (uri) ||
		is_in_a_thunderbird_mail_dir (uri) ||
		is_in_a_kmail_mail_dir (uri));
}


gboolean
tracker_is_a_handled_mbox_file (const char *uri)
{
	const GSList *tmp;

	for (tmp = mboxes; tmp; tmp = g_slist_next (tmp)) {
		const char *tmp_uri;

		tmp_uri = (char *) tmp->data;

		if (strcmp (tmp_uri, uri) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}


MailBox *
tracker_mbox_parse_from_offset (const char *uri, off_t offset)
{
	char	    *uri_in_locale;
	FILE	    *f;
	GMimeStream *stream;
	GMimeParser *parser;
	MailBox	    *mb;

	if (!uri) {
		return NULL;
	}

	uri_in_locale = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (!uri_in_locale) {
		tracker_log ("******ERROR**** uri could not be converted to locale format");
		g_free (uri_in_locale);
		return NULL;
	}

	f = g_fopen (uri_in_locale, "r");

	g_free (uri_in_locale);

	if (!f) {
		return NULL;
	}

	stream = g_mime_stream_file_new_with_bounds (f, offset, -1);

	if (!stream) {
		fclose (f);
		return NULL;
	}

	parser = g_mime_parser_new_with_stream (stream);

	if (!parser) {
		g_object_unref (stream);
		return NULL;
	}

	g_mime_parser_set_scan_from (parser, TRUE);


	mb = g_new (MailBox, 1);

	mb->mbox_uri = g_strdup (uri);

	if (is_in_a_evolution_mail_dir (uri)) {
		mb->mail_app = MAIL_APP_EVOLUTION;

	} else if (is_in_a_thunderbird_mail_dir (uri)) {
		mb->mail_app = MAIL_APP_THUNDERBIRD;

	} else if (is_in_a_kmail_mail_dir (uri)) {
		mb->mail_app = MAIL_APP_KMAIL;

	} else {
		mb->mail_app = MAIL_APP_UNKNOWN;
	}

	mb->stream = stream;
	mb->parser = parser;
	mb->next_email_offset = offset;

	return mb;
}


void
tracker_close_mbox_and_free_memory (MailBox *mb)
{
	if (!mb) {
		return;
	}

	g_free (mb->mbox_uri);
	g_object_unref (mb->parser);
	g_object_unref (mb->stream);
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
		Person *p;

		p = g_new (Person, 1);

		p->name = g_strdup (addrs_list->address->name);
		p->addr = g_strdup (addrs_list->address->value.addr);

		list = g_slist_prepend (list, p);
	}

	return list;
}


static void
find_attachment (GMimeObject *obj, gpointer data)
{
	GMimePart   *part;
	MailMessage *mail_msg;
	const char  *content_disposition;

	if (!data) {
		return;
	}

	part = (GMimePart *) obj;

	if (!GMIME_IS_PART (part)) {
		return;
	}

	mail_msg = (MailMessage *) data;

	content_disposition = g_mime_part_get_content_disposition (part);

	/* test whether it is a mail attachment */
	if (content_disposition &&
	    (strcmp (content_disposition, GMIME_DISPOSITION_ATTACHMENT) == 0 ||
	     strcmp (content_disposition, GMIME_DISPOSITION_INLINE) == 0)) {
		const GMimeContentType *content_type;
		MailAttachment	       *ma;
		FILE		       *f;

		content_type = g_mime_part_get_content_type (part);

		if (!content_type->params) {
			return;
		}


		ma = g_new (MailAttachment, 1);

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


MailMessage *
tracker_mbox_parse_next (MailBox *mb)
{
	MailMessage  *msg;
	guint64	     msg_offset;
	GMimeMessage *message;
	time_t	     date;
	int	     gmt_offset;
	gboolean     is_html;

	if (!mb || !mb->parser) {
		return NULL;
	}

	msg_offset = g_mime_parser_tell (mb->parser);

	message = g_mime_parser_construct_message (mb->parser);

	mb->next_email_offset = g_mime_parser_tell (mb->parser);

	if (!message) {
		return NULL;
	}

	msg = g_new0 (MailMessage, 1);

	msg->parent_mbox = mb;
	msg->offset = msg_offset;
	msg->message_id = g_strdup (g_mime_message_get_message_id (message));

	switch (mb->mail_app) {

	case MAIL_APP_EVOLUTION:
		get_status_of_evolution_email (message, msg);
		get_uri_of_evolution_email (message, msg);
		break;

	case MAIL_APP_KMAIL:
		get_status_of_kmail_email (message, msg);
		get_uri_of_kmail_email (message, msg);
		break;

	case MAIL_APP_THUNDERBIRD:
		get_status_of_thunderbird_email (message, msg);
		get_uri_of_thunderbird_email (message, msg);
		break;

	case MAIL_APP_UNKNOWN:
		/* we don't know anything about this email so we will be nice with it */
		msg->deleted = FALSE;
		msg->junk = FALSE;

		break;
	}


	msg->references = add_gmime_references (NULL, message, "References");


	msg->reply_to_id = add_gmime_references (NULL, message, "In-Reply-To");


	g_mime_message_get_date (message, &date, &gmt_offset);
	msg->date = (long) (date + gmt_offset);

	msg->mail_from = g_strdup (g_mime_message_get_sender (message));

	msg->mail_to = add_recipients (NULL, message, GMIME_RECIPIENT_TYPE_TO);
	msg->mail_cc = add_recipients (NULL, message, GMIME_RECIPIENT_TYPE_CC);
	msg->mail_bcc = add_recipients (NULL, message, GMIME_RECIPIENT_TYPE_BCC);

	msg->subject = g_strdup (g_mime_message_get_subject (message));

	msg->body = g_mime_message_get_body (message, TRUE, &is_html);
	msg->content_type = g_strdup (is_html ? "text/html" : "text/plain");

	/* make directory to save attachment */
	msg->path_to_attachments = g_build_filename (base_path_for_attachments, msg->parent_mbox->mbox_uri, msg->message_id, NULL);
	g_mkdir_with_parents (msg->path_to_attachments, 0700);

	msg->attachments = NULL;

	/* find then save attachments in sys tmp directory of Tracker and save entries in MailMessage struct */
	g_mime_message_foreach_part (message,
				     find_attachment,
				     msg);

	g_object_unref (message);

	if (!msg->attachments) {

		/* no attachment found, so we remove directory immediately */
		g_rmdir (msg->path_to_attachments);

		/* and we say there is not a path to attachments */
		if (msg->path_to_attachments) {
			g_free (msg->path_to_attachments);
			msg->path_to_attachments = NULL;
		}
	}

	return msg;
}


void
tracker_free_mail_message (MailMessage *msg)
{
	if (!msg) {
		return;
	}

	/* we do not free parent_mbox of course... */

	if (msg->uri) {
		g_free (msg->uri);
	}

	if (msg->message_id) {
		g_free (msg->message_id);
	}

	if (msg->references) {
		g_slist_foreach (msg->references, (GFunc) g_free, NULL);
		g_slist_free (msg->references);
	}

	if (msg->reply_to_id) {
		g_slist_foreach (msg->reply_to_id, (GFunc) g_free, NULL);
		g_slist_free (msg->reply_to_id);
	}

	if (msg->mail_from) {
		g_free (msg->mail_from);
	}

	if (msg->mail_to) {
		g_slist_foreach (msg->mail_to, (GFunc) tracker_free_person, NULL);
		g_slist_free (msg->mail_to);
	}

	if (msg->mail_cc) {
		g_slist_foreach (msg->mail_cc, (GFunc) tracker_free_person, NULL);
		g_slist_free (msg->mail_cc);
	}

	if (msg->mail_bcc) {
		g_slist_foreach (msg->mail_bcc, (GFunc) tracker_free_person, NULL);
		g_slist_free (msg->mail_bcc);
	}

	if (msg->subject) {
		g_free (msg->subject);
	}

	if (msg->content_type) {
		g_free (msg->content_type);
	}

	if (msg->body) {
		g_free (msg->body);
	}

	if (msg->path_to_attachments) {
		g_free (msg->path_to_attachments);
	}

	if (msg->attachments) {
		GSList *tmp;

		for (tmp = msg->attachments; tmp; tmp = g_slist_next (tmp)) {
			MailAttachment *ma;

			ma = (MailAttachment *) tmp->data;

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

		g_slist_free (msg->attachments);
	}

	g_free (msg);
}


void
tracker_free_person (Person *p)
{
	if (!p) {
		return;
	}

	if (p->name) {
		g_free (p->name);
	}

	if (p->addr) {
		g_free (p->addr);
	}

	g_free (p);
}


gboolean
tracker_is_an_email_attachment (const char *uri)
{
	if (!uri || !base_path_for_attachments) {
		return FALSE;
	}

	return g_str_has_prefix (uri, base_path_for_attachments);
}


void
tracker_index_each_email_attachment (DBConnection *db_con, const MailMessage *msg)
{
	const GSList *tmp;

	if (!msg) {
		return;
	}

	for (tmp = msg->attachments; tmp; tmp = g_slist_next (tmp)) {
		const MailAttachment *ma;
		FileInfo	     *info;

		ma = (MailAttachment *) tmp->data;

		info = tracker_create_file_info (ma->tmp_decoded_file, TRACKER_ACTION_CHECK, 0, WATCH_OTHER);

		info->is_directory = FALSE;
		info->mime = g_strdup (ma->mime);

		g_async_queue_push (tracker->file_process_queue, info);
		tracker_notify_file_data_available ();
	}
}


void
tracker_unlink_email_attachment (const char *uri)
{
	char *uri_in_locale;

	if (!uri || !tracker_is_an_email_attachment (uri)) {
		return;
	}

	uri_in_locale = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (!uri_in_locale) {
		tracker_log ("******ERROR**** uri could not be converted to locale format");
		g_free (uri_in_locale);
		return;
	}

	g_unlink (uri_in_locale);

	g_free (uri_in_locale);
}
