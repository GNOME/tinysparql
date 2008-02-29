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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <glib/gstdio.h>

#include "tracker-cache.h"
#include "tracker-db-email.h"
#include "tracker-dbus.h"
#include "tracker-email-utils.h"
#include "tracker-email-evolution.h"
#include "tracker-email-thunderbird.h"
#include "tracker-email-kmail.h"
#include "tracker-watch.h"


extern Tracker *tracker;


//static void	mh_watch_mail_messages_in_dir	(DBConnection *db_con, const gchar *dir_path);
static GMimeStream *new_gmime_stream_from_file	(const gchar *path, gint flags, off_t start, off_t end);

static GSList *	add_gmime_references		(GSList *list, GMimeMessage *message, const gchar *header);
static GSList *	add_recipients			(GSList *list, GMimeMessage *message, const gchar *type);
static void	find_attachment			(GMimeObject *obj, gpointer data);




/********************************************************************************************
 Public functions
*********************************************************************************************/

void
email_watch_directory (const gchar *dir, const gchar *service)
{
	tracker_log ("Registering path %s as belonging to service %s", dir, service);
	tracker_add_service_path (service, dir);
}


void
email_watch_directories (const GSList *dirs, const gchar *service)
{
	const GSList *tmp;

	for (tmp = dirs; tmp; tmp = tmp->next) {
		const gchar *dir = tmp->data;
		email_watch_directory (dir, service);
	}
}


gboolean
email_parse_and_save_mail_message (DBConnection *db_con, MailApplication mail_app, const char *path,
                                   ReadMailHelperFct read_mail_helper, gpointer read_mail_user_data)
{
	MailMessage *mail_msg;

	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (path, FALSE);

	mail_msg = email_parse_mail_message_by_path (mail_app, path,
                                                     read_mail_helper, read_mail_user_data);

	if (!mail_msg) {
		return FALSE;
	}

	tracker_db_email_save_email (db_con, mail_msg);

	email_free_mail_message (mail_msg);

	return TRUE;
}


gboolean
email_parse_mail_file_and_save_new_emails (DBConnection *db_con, MailApplication mail_app, const char *path,
                                           ReadMailHelperFct read_mail_helper, gpointer read_mail_user_data,
                                           MakeURIHelperFct uri_helper, gpointer make_uri_user_data,
                                           MailStore *store)
{
	MailFile    *mf;
	MailMessage *mail_msg;
	gint        indexed = 0, junk = 0, deleted = 0;

	if (!tracker->is_running) {
                return FALSE;
        }

	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (path, FALSE);
	g_return_val_if_fail (store, FALSE);

	mf = email_open_mail_file_at_offset (mail_app, path, store->offset, TRUE);

	tracker->mbox_count++;
	tracker_dbus_send_index_progress_signal ("Emails", path);

	while ((mail_msg = email_mail_file_parse_next (mf, read_mail_helper, read_mail_user_data))) {

		if (!tracker->is_running) {
			email_free_mail_message (mail_msg);
			email_free_mail_file (mf);
			return TRUE;
		}

		mail_msg->is_mbox = TRUE;
		mail_msg->store = store;

		/* set uri */
                if (!uri_helper) {
                        if (mail_msg->uri) {
                                g_free (mail_msg->uri);
                        }
                        gchar *str_id = tracker_int_to_str (mail_msg->id);
                        mail_msg->uri = g_strconcat (store->uri_prefix, str_id, NULL);
                        g_free (str_id);
                } else {
                        mail_msg->uri = (*uri_helper) (mail_msg, make_uri_user_data);
                }

		indexed++;

		if (mail_msg->junk) {
			junk++;
		} else 	if (mail_msg->deleted) {
			deleted++;
		}

		tracker_db_email_save_email (db_con, mail_msg);
		tracker_db_email_update_mbox_offset (db_con, mf);

		email_free_mail_message (mail_msg);

		if (!tracker_cache_process_events (db_con->data, TRUE) ) {
			tracker->shutdown = TRUE;
			return FALSE;	
		}

		if (tracker_db_regulate_transactions (db_con->data, 500)) {

			if (tracker->verbosity == 1) {
				tracker_log ("indexing #%d - Emails in %s", tracker->index_count, path);
			}

			if (tracker->index_count % 2500 == 0) {
				tracker_db_end_index_transaction (db_con->data);
				tracker_db_refresh_all (db_con->data);
				tracker_db_start_index_transaction (db_con->data);
			}
			tracker_dbus_send_index_progress_signal ("Emails", path);			
			
		}	

	
	}

	email_free_mail_file (mf);
	tracker->mbox_processed++;
	tracker_dbus_send_index_progress_signal ("Emails", path);

	if (indexed > 0) {
		tracker_info ("Indexed %d emails in email store %s and ignored %d junk and %d deleted emails",
                              indexed, path, junk, deleted);

		return TRUE;
	}

	return FALSE;
}


gboolean
email_is_in_a_mh_dir (const gchar *path)
{
	/* We only take care of files into directory "inbox", "sent" or "trash".
	   So we check that string "/inbox/" is into the path or that path ends with "/inbox" for instance. */

	return (strstr (path, G_DIR_SEPARATOR_S "inbox" G_DIR_SEPARATOR_S) != NULL || g_str_has_suffix (path, G_DIR_SEPARATOR_S "inbox") ||
		strstr (path, G_DIR_SEPARATOR_S "sent" G_DIR_SEPARATOR_S) != NULL  || g_str_has_suffix (path, G_DIR_SEPARATOR_S "sent") ||
		strstr (path, G_DIR_SEPARATOR_S "trash" G_DIR_SEPARATOR_S) != NULL || g_str_has_suffix (path, G_DIR_SEPARATOR_S "trash"));
}


/* void */
/* email_mh_watch_mail_messages (DBConnection *db_con, const gchar *path) */
/* { */
/* 	gchar *mail_dirs[] = {"inbox", "sent", "trash", NULL}; */
/* 	gchar **dir_name; */

/* 	g_return_if_fail (db_con); */
/* 	g_return_if_fail (path); */

/* 	if (tracker_file_is_no_watched (path)) { */
/* 		return; */
/* 	} */

/* 	for (dir_name = mail_dirs; *dir_name; dir_name++) { */
/* 		gchar *dir_path = g_build_filename (path, *dir_name, NULL); */

/* 		if (tracker_is_directory (dir_path)) { */
/* 			mh_watch_mail_messages_in_dir (db_con, dir_path); */
/* 		} */

/* 		g_free (dir_path); */
/* 	} */
/* } */


gboolean
email_is_in_a_maildir_dir (const gchar *path)
{
	/* We only take care of files into directory "cur".
	   So we check that string "/cur/" is into the path or that path ends with "/cur". */

	return (strstr (path, G_DIR_SEPARATOR_S "cur" G_DIR_SEPARATOR_S) != NULL ||
		g_str_has_suffix (path, G_DIR_SEPARATOR_S "cur"));
}


/* void */
/* email_maildir_watch_mail_messages (DBConnection *db_con, const gchar *path) */
/* { */
/* 	gchar *dir_cur; */

/* 	g_return_if_fail (db_con); */
/* 	g_return_if_fail (path); */

/* 	if (tracker_file_is_no_watched (path)) { */
/* 		return; */
/* 	} */

/* 	dir_cur = g_build_filename (path, "cur", NULL); */

/* 	if (tracker_is_directory (dir_cur)) { */
/* 		GSList       *files; */
/* 		const GSList *file; */
/* 		GPatternSpec *pattern; */

/* 		if (!tracker_is_directory_watched (dir_cur, db_con)) { */
/* 			tracker_add_watch_dir (dir_cur, db_con); */
/* 		} */

/* 		files = tracker_get_files (dir_cur, FALSE); */

/* 		/\* We only want files that contain mail message so we check their names. *\/ */
/* 		pattern = g_pattern_spec_new ("*.*.*"); */

/* 		for (file = files; file; file = file->next) { */
/* 			const gchar *file_path = file->data; */

/* 			if (g_pattern_match_string (pattern, file_path)) { */
/* 				tracker_db_insert_pending_file (db_con, 0, file_path, NULL, 0, TRACKER_ACTION_CHECK, FALSE, FALSE, -1); */
/* 			} */
/* 		} */

/* 		g_pattern_spec_free (pattern); */

/* 		g_slist_foreach (files, (GFunc) g_free, NULL); */
/* 		g_slist_free (files); */
/* 	} */

/* 	g_free (dir_cur); */
/* } */


MailFile *
email_open_mail_file_at_offset (MailApplication mail_app, const gchar *path, off_t offset, gboolean scan_from_for_mbox)
{
	GMimeStream *stream;
	GMimeParser *parser;
	MailFile    *mf;

	g_return_val_if_fail (path, NULL);

#if defined(__linux__)
	stream = new_gmime_stream_from_file (path, (O_RDONLY | O_NOATIME) , offset, -1);
#else
	stream = new_gmime_stream_from_file (path, O_RDONLY , offset, -1);
#endif

	if (!stream) {
		return NULL;
	}

	parser = g_mime_parser_new_with_stream (stream);

	if (!parser) {
		g_mime_stream_close (stream);
		g_object_unref (stream);
		return NULL;
	}

	g_mime_parser_set_scan_from (parser, scan_from_for_mbox);


	mf = g_slice_new0 (MailFile);

	mf->path = g_strdup (path);
	mf->mail_app = mail_app;
	mf->parser = parser;
	mf->stream = stream;
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

	if (mf->parser) {
		g_object_unref (mf->parser);
	}

	if (mf->stream) {
		//g_mime_stream_close (mf->stream);
		g_object_unref (mf->stream);
	}

	g_slice_free (MailFile, mf);
}


MailPerson *
email_allocate_mail_person (void)
{
	return g_slice_new0 (MailPerson);
}


void
email_free_mail_person (MailPerson *mp)
{
	if (!mp) {
		return;
	}

	g_free (mp->name);
	g_free (mp->addr);

	g_slice_free (MailPerson, mp);
}


MailAttachment *
email_allocate_mail_attachment (void)
{
	return g_slice_new0 (MailAttachment);
}


void
email_free_mail_attachment (MailAttachment *ma)
{
	if (!ma) {
		return;
	}

	if (ma->attachment_name) {
		g_free (ma->attachment_name);
	}

	if (ma->mime) {
		g_free (ma->mime);
	}

	if (ma->tmp_decoded_file) {
                /* Removes tmp file */
                g_unlink (ma->tmp_decoded_file);
		g_free (ma->tmp_decoded_file);
	}

	g_slice_free (MailAttachment, ma);
}


MailMessage *
email_allocate_mail_message (void)
{
	return g_slice_new0 (MailMessage);
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

	if (mail_msg->attachments) {
		g_slist_foreach (mail_msg->attachments, (GFunc) email_free_mail_attachment, NULL);
		g_slist_free (mail_msg->attachments);
	}

	g_slice_free (MailMessage, mail_msg);
}


MailMessage *
email_mail_file_parse_next (MailFile *mf, ReadMailHelperFct read_mail_helper, gpointer read_mail_user_data)
{
	MailMessage  *mail_msg;
	guint64      msg_offset;
	GMimeMessage *g_m_message;
	time_t       date;
	gint         gmt_offset;
	gboolean     is_html;

	g_return_val_if_fail ((mf && mf->parser), NULL);

	msg_offset = g_mime_parser_tell (mf->parser);

	g_m_message = g_mime_parser_construct_message (mf->parser);

	if (!g_m_message) {
		return NULL;
	}

	mf->next_email_offset = g_mime_parser_tell (mf->parser);

	mail_msg = email_allocate_mail_message ();

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
	mail_msg->date = (glong) (date + gmt_offset);

	mail_msg->from = g_strdup (g_mime_message_get_sender (g_m_message));

	mail_msg->to = add_recipients (NULL, g_m_message, GMIME_RECIPIENT_TYPE_TO);
	mail_msg->cc = add_recipients (NULL, g_m_message, GMIME_RECIPIENT_TYPE_CC);
	mail_msg->bcc = add_recipients (NULL, g_m_message, GMIME_RECIPIENT_TYPE_BCC);

	mail_msg->subject = g_strdup (g_mime_message_get_subject (g_m_message));

	mail_msg->body = g_mime_message_get_body (g_m_message, TRUE, &is_html);
	mail_msg->content_type = g_strdup (is_html ? "text/html" : "text/plain");

	if (read_mail_helper) {
                (*read_mail_helper) (g_m_message, mail_msg, read_mail_user_data);
	}

	mail_msg->attachments = NULL;

	/* find then save attachments in sys tmp directory of Tracker and save entries in MailMessage struct */
	g_mime_message_foreach_part (g_m_message, find_attachment, mail_msg);

	g_object_unref (g_m_message);

	return mail_msg;
}


MailMessage *
email_parse_mail_message_by_path (MailApplication mail_app, const gchar *path,
                                  ReadMailHelperFct read_mail_helper, gpointer read_mail_user_data)
{
	MailFile    *mf;
	MailMessage *mail_msg;

	g_return_val_if_fail (path, NULL);

	mf = email_open_mail_file_at_offset (mail_app, path, 0, FALSE);
        if (!mf) {
                return NULL;
        }

	mail_msg = email_mail_file_parse_next (mf, read_mail_helper, read_mail_user_data);

	if (mail_msg) {
		mail_msg->path = g_strdup (path);
		mail_msg->is_mbox = FALSE;
	}

	return mail_msg;
}


MimeInfos *
email_allocate_mime_infos (void)
{
	return g_slice_new0 (MimeInfos);
}


void
email_free_mime_infos (MimeInfos *infos)
{
	if (!infos) {
		return;
	}

	if (infos->type) {
		g_free (infos->type);
	}

	if (infos->subtype) {
		g_free (infos->subtype);
	}

	if (infos->name) {
		g_free (infos->name);
	}

	g_slice_free (MimeInfos, infos);
}


MimeInfos *
email_get_mime_infos_from_mime_file (const gchar *mime_file)
{
	MimeInfos   *mime_infos;
	gchar       *tmp, *mime_content;
	const gchar *pos_content_type;

	g_return_val_if_fail (mime_file, NULL);

	if (!g_file_get_contents (mime_file, &tmp, NULL, NULL)) {
		return NULL;
	}

        /* all text content in lower case for comparaisons */
	mime_content = g_ascii_strdown (tmp, -1);
        g_free (tmp);

	mime_infos = NULL;

	pos_content_type = strstr (mime_content, "content-type:");

	if (pos_content_type) {
 		size_t len;

 		len = strlen (pos_content_type);

 		if (len > 13) {	/* strlen ("content-type:") == 13 */
 			pos_content_type += 13;

			/* ignore spaces, tab or line returns */
			while (*pos_content_type != '\0' && (*pos_content_type == ' ' || *pos_content_type == '\t' || *pos_content_type == '\n')) {
				pos_content_type++;
			}

			if (*pos_content_type != '\0') {
				GMimeContentType *mime = g_mime_content_type_new_from_string (pos_content_type);

				if (mime) {
					GMimeParam   *param;
					gchar        *type, *subtype, *name;
					const gchar  *pos_encoding;
					MimeEncoding encoding;

					type	= mime->type ? g_strdup (mime->type) : g_strdup ("text");		/* NULL means text */
					subtype	= mime->subtype ? g_strdup (mime->subtype) : g_strdup ("plain");	/* NULL means plain */

					name = NULL;

					/* get name and encoding, hashtable from m->mime_infos->param_hash is buggy */
					for (param = mime->params; param; param = param->next) {
						if (strcmp (param->name, "name") == 0) {
							name = g_strdup (param->value);
						}
					}

					encoding = MIME_ENCODING_UNKNOWN;

					pos_encoding = strstr (pos_content_type, "content-transfer-encoding:");

					if (pos_encoding) {
						size_t      len_encoding;
						const gchar *pos_end_encoding;

						len_encoding = strlen (pos_encoding);
						if (len_encoding > 26) {	/* strlen ("content-transfer-encoding:") == 26 */
							pos_encoding += 26;

							/* find begining of content-transfert-encoding */
							while (*pos_encoding != '\0' && *pos_encoding == ' ') {
								pos_encoding++;
							}
							/* and now find end */;
							for (pos_end_encoding = pos_encoding;
							     *pos_end_encoding != '\0' && (*pos_end_encoding != ' ' && *pos_end_encoding != '\n' && *pos_end_encoding != '\t');
							     pos_end_encoding++)
								;
							if (pos_encoding != pos_end_encoding) {
								gchar *encoding_str = g_strndup (pos_encoding, pos_end_encoding - pos_encoding);

								if (strcmp (encoding_str, "7bit") == 0) {
									encoding = MIME_ENCODING_7BIT;
								} else if (strcmp (encoding_str, "8bit") == 0) {
									encoding = MIME_ENCODING_8BIT;
								} else if (strcmp (encoding_str, "binary") == 0) {
									encoding = MIME_ENCODING_BINARY;
								} else if (strcmp (encoding_str, "base64") == 0) {
									encoding = MIME_ENCODING_BASE64;
								} else if (strcmp (encoding_str, "quoted-printable") == 0) {
									encoding = MIME_ENCODING_QUOTEDPRINTABLE;
								} else if (strcmp (encoding_str, "x-uuencode") == 0) {
									encoding = MIME_ENCODING_UUENCODE;
								}

								g_free (encoding_str);
							}
						}
					}

					mime_infos = email_allocate_mime_infos ();

					mime_infos->type	= type;
					mime_infos->subtype	= subtype;
					mime_infos->name	= name;
					mime_infos->encoding	= encoding;
				}
			}
		}
	}

	g_free (mime_content);

	return mime_infos;
}


gboolean
email_add_saved_mail_attachment_to_mail_message (MailMessage *mail_msg, MailAttachment *ma)
{
	g_return_val_if_fail (mail_msg, FALSE);
	g_return_val_if_fail (ma, FALSE);

	mail_msg->attachments = g_slist_prepend (mail_msg->attachments, ma);

        tracker_debug ("saved email attachment \"%s\"", ma->tmp_decoded_file);

	return TRUE;
}


gchar *
email_make_tmp_name_for_mail_attachment (const gchar *filename)
{
        gchar *str_uint, *tmp_filename, *tmp_name;

        g_return_val_if_fail (filename, NULL);
        g_return_val_if_fail (tracker->email_attachements_dir, NULL);

        str_uint = tracker_uint_to_str (g_random_int ());
        tmp_filename = g_strconcat (str_uint, "-", filename, NULL);
        g_free (str_uint);
        tmp_name = g_build_filename (tracker->email_attachements_dir, tmp_filename, NULL);
        g_free (tmp_filename);

        return tmp_name;
}


gboolean
email_decode_mail_attachment_to_file (const gchar *src, const gchar *dst, MimeEncoding encoding)
{
	GMimeStream *stream_src, *stream_dst;
	GMimeFilter *filter;
	GMimeStream *filtered_stream;

	g_return_val_if_fail (src, FALSE);
	g_return_val_if_fail (dst, FALSE);

#if defined(__linux__)
	stream_src = new_gmime_stream_from_file (src, (O_RDONLY | O_NOATIME), 0, -1);
#else
	stream_src = new_gmime_stream_from_file (src, O_RDONLY, 0, -1);
#endif
	stream_dst = new_gmime_stream_from_file (dst, (O_CREAT | O_TRUNC | O_WRONLY), 0, -1);

	if (!stream_src || !stream_dst) {
		if (stream_src) {
			g_object_unref (stream_src);
		}
		if (stream_dst) {
			g_object_unref (stream_dst);
		}
		return FALSE;
	}

	filtered_stream = g_mime_stream_filter_new_with_stream (stream_src);

	switch (encoding) {
		case MIME_ENCODING_BASE64:
			filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_BASE64_DEC);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
			g_object_unref (filter);
			break;

		case MIME_ENCODING_QUOTEDPRINTABLE:
			filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_QP_DEC);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
			g_object_unref (filter);
			break;

		case MIME_ENCODING_UUENCODE:
			filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_UU_ENC);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
			g_object_unref (filter);
			break;

		default:
			break;
	}

	g_mime_stream_write_to_stream (filtered_stream, stream_dst);
	g_mime_stream_flush (filtered_stream);

	g_object_unref (filtered_stream);
	g_object_unref (stream_src);
	g_object_unref (stream_dst);

	return TRUE;
}




/********************************************************************************************
 Private functions
*********************************************************************************************/

/* static void */
/* mh_watch_mail_messages_in_dir (DBConnection *db_con, const gchar *dir_path) */
/* { */
/* 	GSList       *files; */
/* 	const GSList *file; */

/* 	g_return_if_fail (db_con); */
/* 	g_return_if_fail (dir_path); */

/* 	if (tracker_file_is_no_watched (dir_path)) { */
/* 		return; */
/* 	} */

/* 	if (!tracker_is_directory_watched (dir_path, db_con)) { */
/* 		tracker_add_watch_dir (dir_path, db_con); */
/* 	} */

/* 	files = tracker_get_files (dir_path, FALSE); */

/* 	for (file = files; file; file = file->next) { */
/* 		const gchar *file_path = file->data; */

/* 		if (tracker_file_is_indexable (file_path)) { */
/* 			gchar *file_name, *p; */

/* 			/\* filename must contain only digits *\/ */
/* 			file_name = g_path_get_basename (file_path); */

/* 			for (p = file_name; *p != '\0'; p++) { */
/* 				if (!g_ascii_isdigit (*p)) { */
/* 					goto end; */
/* 				} */
/* 			} */

/* 			tracker_db_insert_pending_file (db_con, 0, file_path, NULL, 0, TRACKER_ACTION_CHECK, FALSE, FALSE, -1); */

/* 		end: */
/* 			g_free (file_name); */
/* 		} */
/* 	} */

/* 	g_slist_foreach (files, (GFunc) g_free, NULL); */
/* 	g_slist_free (files); */
/* } */


static GMimeStream *
new_gmime_stream_from_file (const gchar *path, gint flags, off_t start, off_t end)
{
	gchar       *path_in_locale;
	gint        fd;
	GMimeStream *stream;

	g_return_val_if_fail (path, NULL);

	path_in_locale = g_filename_from_utf8 (path, -1, NULL, NULL, NULL);

	if (!path_in_locale) {
		tracker_error ("ERROR: src or dst could not be converted to locale format");
		g_free (path_in_locale);
		return NULL;
	}

	fd = g_open (path_in_locale, flags, (S_IRUSR | S_IWUSR));

	g_free (path_in_locale);

	if (fd == -1) {
		return NULL;
	}

	stream = g_mime_stream_fs_new_with_bounds (fd, start, end);

	if (!stream) {
		close (fd);
		return NULL;
	}

	return stream;
}


static GSList *
add_gmime_references (GSList *list, GMimeMessage *message, const gchar *header)
{
	const gchar *tmp = g_mime_message_get_header (message, header);

	if (tmp) {
		GMimeReferences       *refs;
		const GMimeReferences *tmp_ref;

		refs = g_mime_references_decode (tmp);

		for (tmp_ref = refs; tmp_ref; tmp_ref = tmp_ref->next) {
			const gchar *msgid = tmp_ref->msgid;

			if (!tracker_is_empty_string (msgid)) {
				list = g_slist_prepend (list, g_strdup (msgid));
			}
		}

		g_mime_references_clear (&refs);
	}

	return list;
}


static GSList *
add_recipients (GSList *list, GMimeMessage *message, const gchar *type)
{
	const InternetAddressList *addrs_list;

	for (addrs_list = g_mime_message_get_recipients (message, type); addrs_list; addrs_list = addrs_list->next) {
		MailPerson *mp = email_allocate_mail_person ();

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
	const gchar *content_disposition;

	g_return_if_fail (obj);
	g_return_if_fail (data);


	if (GMIME_IS_MESSAGE_PART (obj)) {
		GMimeMessage *g_msg = g_mime_message_part_get_message (GMIME_MESSAGE_PART (obj));

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

		const GMimeContentType *content_type;
		MailAttachment         *ma;
		const gchar            *filename;
		gchar                  *attachment_uri;
		gint                   fd;

		if (! (content_type = g_mime_part_get_content_type (part)))
			return;

		filename = g_mime_part_get_filename (part);

		if (!content_type->params || !content_type->type || !content_type->subtype  || !filename) {
			/* we are unable to identify mime type or filename */
			return;
		}

		/* do not index signature attachments */
		if (g_str_has_suffix (filename, "signature.asc")) {
			return;
		} else {
			tracker_info ("attached filename is %s", filename);
		}

		attachment_uri = email_make_tmp_name_for_mail_attachment (filename);

		/* convert attachment filename from utf-8 to filesystem charset */
		gchar *locale_uri = g_filename_from_utf8 (attachment_uri, -1, NULL, NULL, NULL);

		fd = g_open (locale_uri, (O_CREAT | O_TRUNC | O_WRONLY), 0666);

		if (fd == -1) {
			tracker_error ("ERROR: failed to save attachment %s", locale_uri);
			g_free (attachment_uri);
			g_free (locale_uri);
			return;

		} else {
                        /* Decodes email attachment and stores it in tracker->email_attachements_dir
                           to index it latter.
                        */

			GMimeStream      *stream;
			GMimeDataWrapper *content;

			stream = g_mime_stream_fs_new (fd);
			content = g_mime_part_get_content_object (part);

			if (content) {
				gint x = g_mime_data_wrapper_write_to_stream (content, stream);

				if (x != -1) {
					g_mime_stream_flush (stream);

					ma = email_allocate_mail_attachment ();

					ma->attachment_name = g_strdup (filename);
					ma->mime = g_strconcat (content_type->type, "/", content_type->subtype, NULL);
					ma->tmp_decoded_file  = g_strdup (attachment_uri);

					email_add_saved_mail_attachment_to_mail_message (mail_msg, ma);
				}

				g_object_unref (content);
			}

			g_object_unref (stream);
		}

		g_free (attachment_uri);
		g_free (locale_uri);
	}
}
