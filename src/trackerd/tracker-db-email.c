/* Tracker
 * routines for emails with databases
 * Copyright (C) 2006, Laurent Aguerreche (laurent.aguerreche@free.fr)
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
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <string.h>

#include "tracker-db-email.h"


static void
add_new_mbox (DBConnection *db_con, const char *mbox_uri)
{
	tracker_exec_proc (db_con, "InsertMboxDetails", 2, mbox_uri, "0");
}


int
get_mbox_id (DBConnection *db_con, const char *mbox_uri)
{
	char	***res;
	char	**row;
	int	id;

	res = tracker_exec_proc (db_con, "GetMboxID", 1, mbox_uri);

	if (!res) {
		return -1;
	}

	row = tracker_db_get_row (res, 0);

	if (!(row && row[0])) {
		return -1;
	} else {
		id = atoi (row[0]);
	}

	tracker_db_free_result (res);

	return id;
}


static off_t
get_mbox_offset (DBConnection *db_con, const char *mbox_uri)
{
	char	***res;
	char	**row;
	off_t	offset;

	res = tracker_exec_proc (db_con, "GetMBoxDetails", 1, mbox_uri);

	if (!res) {
		return -1;
	}

	row = tracker_db_get_row (res, 0);

	if (!(row && row[0] && row[1])) {
		return -1;
	} else {
		offset = atoi (row[1]);
	}

	tracker_db_free_result (res);

	return offset;
}


off_t
tracker_db_email_get_last_mbox_offset (DBConnection *db_con, const char *mbox_file_path)
{
	off_t offset;

	g_return_val_if_fail (db_con, 0);
	g_return_val_if_fail (mbox_file_path, 0);

	offset = get_mbox_offset (db_con, mbox_file_path);

	if (offset == -1) {
		/* we need to add this mbox */
		add_new_mbox (db_con, mbox_file_path);

		offset = get_mbox_offset (db_con, mbox_file_path);

		if (offset == -1) {
			/* There is really a problem with this mbox... */
			tracker_log ("ERROR: Could not create entry in DB for mbox file \"%s\"", mbox_file_path);
			return 0;
		}
	}

	return offset;
}


guint
tracker_db_email_get_nb_emails_in_mbox (DBConnection *db_con, const char *mbox_file_path)
{
	char	***res;
	char	**row;
	int	count;

	g_return_val_if_fail (db_con, 0);
	g_return_val_if_fail (mbox_file_path, 0);

	res = tracker_exec_proc (db_con, "GetMboxCount", 1, mbox_file_path);

	if (!res) {
		return 0;
	}

	row = tracker_db_get_row (res, 0);

	if (!(row && row[0])) {
		return 0;
	} else {
		count = atoi (row[0]);
	}

	tracker_db_free_result (res);

	return count;
}


guint
tracker_db_email_get_nb_emails_in_dir (DBConnection *db_con, const char *dir_path)
{
	return tracker_db_email_get_nb_emails_in_mbox (db_con, dir_path);
}


void
tracker_db_email_update_mbox_offset (DBConnection *db_con, MailFile *mf)
{
	char *str_offset;

	g_return_if_fail (db_con);
	g_return_if_fail (mf);

	if (!mf->path) {
		tracker_log ("Error invalid mbox (empty path!)");
		return;
	}

	str_offset = tracker_uint_to_str (mf->next_email_offset);
	str_offset = tracker_uint_to_str (mf->next_email_offset);

	/* make sure mbox is registered in DB before doing an update */
	if (get_mbox_offset (db_con, mf->path) != -1) {

		str_offset = tracker_uint_to_str (mf->next_email_offset);
		tracker_exec_proc (db_con, "UpdateMboxDetails", 6, str_offset, " ", "0", "0", "0", mf->path);

		g_free (str_offset);

	} else {
		tracker_log ("Error invalid mbox \"%s\"", mf->path);
	}


}




void
tracker_db_email_save_email (DBConnection *db_con, MailMessage *mm)
{
/* 	GString	     *s;  */
	const GSList	*tmp;
	char		/* *to_print, */ *name, *path;
	int		mbox_id, id;
	GHashTable	*index_table;

	g_return_if_fail (db_con);
	g_return_if_fail (mm);

	if (!mm->uri || mm->deleted || mm->junk || mm->parent_mail_file || mm->parent_mail_file->path) {
		return;
	}

	mbox_id = get_mbox_id (db_con, mm->parent_mail_file->path);

	if (mbox_id == -1) {
		tracker_log ("No mbox is registered for email %s", mm->uri);
		return;
	}

	name = tracker_get_vfs_name (mm->uri);
	path = tracker_get_vfs_path (mm->uri);
	
	tracker_db_start_transaction (db_con);

	tracker_db_create_service (db_con, path, name, "Emails", "email", 0, FALSE, FALSE, mm->offset, 0, mbox_id);

	id = tracker_db_get_file_id (db_con, mm->uri);

	if (id != -1) {
		char *str_id, *str_date;

		tracker_log ("saving email with uri \"%s\" and subject \"%s\" from \"%s\"", mm->uri, mm->subject, mm->from);

		index_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		str_id = tracker_int_to_str (id);
		str_date = tracker_int_to_str (mm->date);

		tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:Body", mm->body, index_table);
		tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:Date", str_date, index_table);
		tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:Sender", mm->from, index_table);
		tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:Subject", mm->subject, index_table);

		g_free (str_date);

		for (tmp = mm->to; tmp; tmp = tmp->next) {
			const MailPerson *mp;
			char		 *str;

			mp = tmp->data;

			str = g_strconcat (mp->name, ":", mp->addr, NULL);
			tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:SentTo", str, index_table);
			g_free (str);
		}

		for (tmp = mm->cc; tmp; tmp = tmp->next) {
			const MailPerson *mp;
			char		 *str;

			mp = tmp->data;

			str = g_strconcat (mp->name, ":", mp->addr, NULL);
			tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:CC", str, index_table);
			g_free (str);
		}

		for (tmp = mm->attachments; tmp; tmp = tmp->next) {
			const MailAttachment *ma;

			ma = tmp->data;

			tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:Attachments", ma->attachment_name, index_table);

			/* delimit attachment names so hyphens and underscores are removed so that they can be indexed separately */
			if (strchr (ma->attachment_name, '_') || strchr (ma->attachment_name, '-')) {
				char *delimited;

				delimited = g_strdup (ma->attachment_name);
				delimited =  g_strdelimit (delimited, "-_" , ' ');
				tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:AttachmentsDelimted", delimited, index_table);
				g_free (delimited);
			}
		}

		tracker_db_refresh_all_display_metadata (db_con, str_id);

		tracker_db_end_transaction (db_con);

		tracker_db_update_indexes_for_new_service (id, tracker_get_id_for_service ("Emails"), index_table);

		g_free (str_id);

		return;
	}

	tracker_db_end_transaction (db_con);

	/* sometimes we will create a new record, sometimes we will update previous entries */


	/*
	 * FIXME
	 */

/* 	s = g_string_new (""); */

/* 	g_string_append_printf (s, */
/* 				"Saving email with mbox's path \"%s\" and id \"%s\":\n" */
/* 				"- path: %s\n" */
/* 				"- uri: %s\n" */
/* 				"- offset: %lld\n" */
/* 				"- reply-to: %s\n" */
/* 				"- deleted?: %d\n" */
/* 				"- junk?: %d\n", */
/* 				mm->parent_mail_file ? mm->parent_mail_file->path : "no-parent-mail-file", */
/* 				mm->message_id, */
/* 				mm->path, */
/* 				mm->uri, */
/* 				mm->offset, */
/* 				mm->reply_to, */
/* 				mm->deleted, */
/* 				mm->junk); */

/* 	g_string_append (s, "- references: "); */
/* 	for (tmp = mm->references; tmp; tmp = tmp->next) { */
/* 		g_string_append_printf (s, "%s * ", (char *) tmp->data); */
/* 	} */
/* 	g_string_append (s, "\n"); */

/* 	g_string_append (s, "- in-reply-to-ids: "); */
/* 	for (tmp = mm->in_reply_to_ids; tmp; tmp = tmp->next) { */
/* 		g_string_append_printf (s, "%s * ", (char *) tmp->data); */
/* 	} */
/* 	g_string_append (s, "\n"); */

/* 	g_string_append_printf (s, */
/* 				"- date: %s\n" */
/* 				"- mail_from: %s\n", */
/* 				ctime (&mm->date), */
/* 				mm->from); */

/* 	g_string_append (s, "- mail_to: "); */
/* 	for (tmp = mm->to; tmp; tmp = tmp->next) { */
/* 		const MailPerson *mp; */
/* 		mp = tmp->data; */
/* 		g_string_append_printf (s, "name:%s, addr:%s ** ", mp->name, mp->addr); */
/* 	} */
/* 	g_string_append (s, "\n"); */

/* 	g_string_append (s, "- mail_cc: "); */
/* 	for (tmp = mm->cc; tmp; tmp = tmp->next) { */
/* 		const MailPerson *mp; */
/* 		mp = tmp->data; */
/* 		g_string_append_printf (s, "name:%s, addr:%s ** ", mp->name, mp->addr); */
/* 	} */
/* 	g_string_append (s, "\n"); */

/* 	g_string_append (s, "- mail_bcc: "); */
/* 	for (tmp = mm->bcc; tmp; tmp = tmp->next) { */
/* 		const MailPerson *mp; */
/* 		mp = tmp->data; */
/* 		g_string_append_printf (s, "name:%s, addr:%s ** ", mp->name, mp->addr); */
/* 	} */
/* 	g_string_append (s, "\n"); */

/* 	g_string_append_printf (s, */
/* 				"- subject: %s\n" */
/* 				"- content_type: %s\n" */
/* 				"- body: %s\n" */
/* 				"- path_to_attachments: %s\n", */
/* 				mm->subject, */
/* 				mm->content_type, */
/* 				mm->body, */
/* 				mm->path_to_attachments); */

/* 	g_string_append (s, "- attachments: "); */
/* 	for (tmp = mm->attachments; tmp; tmp = tmp->next) { */
/* 		MailAttachment *ma; */

/* 		ma = (MailAttachment *) tmp->data; */

/* 		g_string_append_printf (s, "%s * ", ma->attachment_name); */
/* 	} */
/* 	g_string_append (s, "\n"); */


/* 	to_print = g_string_free (s, FALSE); */

/* 	tracker_log ("%s\n", to_print); */

/* 	g_free (to_print); */
}


void
tracker_db_email_update_email (DBConnection *db_con, MailMessage *mm)
{
	g_return_if_fail (db_con);
	g_return_if_fail (mm);

	tracker_log ("update email with uri \"%s\" and subject \"%s\" from \"%s\"", mm->uri, mm->subject, mm->from);

	/* FIXME: add code... */
}


void
tracker_db_email_delete_emails_of_mbox (DBConnection *db_con, const char *mbox_file_path)
{
	g_return_if_fail (db_con);
	g_return_if_fail (mbox_file_path);

	/* FIXME: add code... */
}


void
tracker_db_email_delete_emails_of_dir (DBConnection *db_con, const char *dir_path)
{
	g_return_if_fail (db_con);
	g_return_if_fail (dir_path);

	tracker_db_email_delete_emails_of_mbox (db_con, dir_path);


}


