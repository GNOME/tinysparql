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
		tracker_debug ("registering new mbox/dir for emails for %s", mbox_file_path);
		add_new_mbox (db_con, mbox_file_path);

		offset = get_mbox_offset (db_con, mbox_file_path);

		if (offset == -1) {
			/* There is really a problem with this mbox... */
			tracker_log ("Error: Could not create entry in DB for mbox file \"%s\"", mbox_file_path);
			return 0;
		}
	}

	return offset;
}


guint
tracker_db_email_get_nb_emails_in_mbox (DBConnection *db_con, const char *mbox_file_path)
{
	char	***res;void
tracker_db_email_update_nb_emails_in_dir (DBConnection *db_con, const char *dir_path, int count)
{
	g_return_if_fail (db_con);
	g_return_if_fail (dir_path);

	
	/* make sure dir_path is registered in DB before doing an update */
	if (get_mbox_offset (db_con, dir_path) != -1) {
		char *str_offset;

		str_offset = tracker_uint_to_str (count);
		tracker_exec_proc (db_con, "UpdateMboxDetails", 6, str_offset, "n/a", "0", "0", "0", dir_path);

		tracker_debug ("updated no. of emails in %s to %s",  dir_path, str_offset);

		g_free (str_offset);

	} else {
		tracker_log ("Error: invalid dir_path \"%s\"", dir_path);
	}
}
	char	**row;
	int	count;

	g_return_val_if_fail (db_con, 0);
	g_return_val_if_fail (mbox_file_path, 0);

/*	res = tracker_exec_proc (db_con, "GetMboxCount", 1, mbox_file_path);

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
*/

	

	return count;
}


guint
tracker_db_email_get_nb_emails_in_dir (DBConnection *db_con, const char *dir_path)
{
	return tracker_db_email_get_last_mbox_offset (db_con, dir_path);
}


void
tracker_db_email_update_mbox_offset (DBConnection *db_con, MailFile *mf)
{
	g_return_if_fail (db_con);
	g_return_if_fail (mf);

	if (!mf->path) {
		tracker_log ("Error: invalid mbox (empty path!)");
		return;
	}

	/* make sure mbox is registered in DB before doing an update */
	if (get_mbox_offset (db_con, mf->path) != -1) {
		char *str_offset;

		str_offset = tracker_uint_to_str (mf->next_email_offset);
		tracker_exec_proc (db_con, "UpdateMboxDetails", 6, str_offset, " ", "0", "0", "0", mf->path);

		g_free (str_offset);

	} else {
		tracker_log ("Error: invalid mbox \"%s\"", mf->path);
	}
}



void
tracker_db_email_update_nb_emails_in_dir (DBConnection *db_con, const char *dir_path, int count)
{
	g_return_if_fail (db_con);
	g_return_if_fail (dir_path);

	
	/* make sure dir_path is registered in DB before doing an update */
	if (get_mbox_offset (db_con, dir_path) != -1) {
		char *str_offset;

		str_offset = tracker_uint_to_str (count);
		tracker_exec_proc (db_con, "UpdateMboxDetails", 6, str_offset, "n/a", "0", "0", "0", dir_path);

		tracker_debug ("updated no. of emails in %s to %s",  dir_path, str_offset);

		g_free (str_offset);

	} else {
		tracker_log ("Error: invalid dir_path \"%s\"", dir_path);
	}
}


static char *
get_service_name (MailApplication app)
{
	if (app == MAIL_APP_EVOLUTION) {
		return g_strdup ("EvolutionEmails");
	} else if (app == MAIL_APP_KMAIL) {
		return g_strdup ("KMailEmails");
	} else if (app == MAIL_APP_THUNDERBIRD) {
		return g_strdup ("ThunderbirdEmails");
	}

	return g_strdup ("OtherEmails");
}


static char *
get_mime (MailApplication app)
{
	if (app == MAIL_APP_EVOLUTION) {
		return g_strdup ("Evolution Email");
	} else if (app == MAIL_APP_KMAIL) {
		return g_strdup ("KMail Email");
	} else if (app == MAIL_APP_THUNDERBIRD) {
		return g_strdup ("Thunderbird Email");
	}

	return g_strdup ("Other Email");
}

static char *
get_attachment_service_name (MailApplication app)
{
	if (app == MAIL_APP_EVOLUTION) {
		return g_strdup ("EvolutionAttachments");
	} else if (app == MAIL_APP_KMAIL) {
		return g_strdup ("KMailAttachments");
	} else if (app == MAIL_APP_THUNDERBIRD) {
		return g_strdup ("ThunderbirdAttachments");
	}
	
	return g_strdup ("OtherAttachments");
	
}


void
tracker_db_email_save_email (DBConnection *db_con, MailMessage *mm)
{
	char	*name, *path;
	int	mbox_id, type_id, id;
	char *service, *attachment_service, *mime;


	g_return_if_fail (db_con);
	g_return_if_fail (mm);

	if (!mm->uri || mm->deleted || mm->junk || (mm->parent_mail_file && !mm->parent_mail_file->path)) {
		return;
	}

	if (mm->parent_mail_file) {
		
		if (mm->is_mbox) {
		
			mbox_id = get_mbox_id (db_con, mm->parent_mail_file->path);

			if (mbox_id == -1) {
				tracker_log ("No mbox is registered for email %s", mm->uri);
				return;
			}
		}
	
		service = get_service_name (mm->parent_mail_file->mail_app);
		mime = get_mime (mm->parent_mail_file->mail_app);
		attachment_service = get_attachment_service_name (mm->parent_mail_file->mail_app);

		
	} else {
		mbox_id = 0;
		mm->offset =0;
		service = g_strdup ("EvolutionEmails");
		mime = g_strdup ("Evolution/Email");
		attachment_service = g_strdup ("EvolutionAttachments");
	}

	
	name = tracker_get_vfs_name (mm->uri);
	path = tracker_get_vfs_path (mm->uri);

	type_id = tracker_get_id_for_service (service);
	if (type_id == -1) {
		tracker_log ("Error service %s not found", service);
		g_free (path);
		g_free (name);
		g_free (attachment_service);
		g_free (service);
		return;
	}

	tracker_db_start_transaction (db_con);


	id = -1;

	id = tracker_db_create_service (db_con, path, name, service, mime, 0, FALSE, FALSE, mm->offset, 0, mbox_id);

	if (id != -1) {
		GHashTable	*index_table;
		char		*str_id, *str_date;
		const GSList	*tmp;


		tracker_info ("saving email service %d with uri \"%s\" and subject \"%s\" from \"%s\"", type_id, mm->uri, mm->subject, mm->from);

		index_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		str_id = tracker_int_to_str (id);
		str_date = tracker_int_to_str (mm->date);


		if (mm->body) {
			tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:Body", mm->body, index_table);
		}

		if (str_date) {
			tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:Date", str_date, index_table);
		}

		if (mm->from) {
			tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:Sender", mm->from, index_table);
		}

		if (mm->subject) {
			tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:Subject", mm->subject, index_table);
		}

		g_free (str_date);

		for (tmp = mm->to; tmp; tmp = tmp->next) {
			const MailPerson *mp;
			char		 *str;

			mp = tmp->data;
			
			if (!mp->addr) {
				continue;
			}

			if (mp->name) {
				str = g_strconcat (mp->name, ":", mp->addr, NULL);
			} else {
				str = g_strdup (mp->addr);
			}
			tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:SentTo", str, index_table);
			g_free (str);
		}

		for (tmp = mm->cc; tmp; tmp = tmp->next) {
			const MailPerson *mp;
			char		 *str;

			mp = tmp->data;

			if (!mp->addr) {
				continue;
			}

			if (mp->name) {
				str = g_strconcat (mp->name, ":", mp->addr, NULL);
			} else {
				str = g_strdup (mp->addr);
			}

			tracker_db_insert_embedded_metadata (db_con, "Emails", str_id, "Email:CC", str, index_table);
			g_free (str);
		}


		for (tmp = mm->attachments; tmp; tmp = tmp->next) {
			const MailAttachment *ma;

			ma = tmp->data;

			if (!ma->attachment_name) {
				continue;
			}

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

		tracker_debug ("storing email with id %d and type_id %d with %d words", id, type_id, g_hash_table_size (index_table));
		tracker_db_update_indexes_for_new_service (id, type_id, index_table);

		g_hash_table_destroy (index_table);

		g_free (str_id);

		/* index attachments */
		for (tmp = mm->attachments; tmp; tmp = tmp->next) {

			const MailAttachment	*ma;
			FileInfo		*info;
			char 			*uri;
			ma = tmp->data;

			info = tracker_create_file_info (ma->tmp_decoded_file, TRACKER_ACTION_CHECK, 0, WATCH_OTHER);
			info->is_directory = FALSE;
			info->mime = g_strdup (ma->mime);

			uri = g_strconcat (mm->uri, "/", ma->attachment_name, NULL);
			tracker_info ("indexing attachement with uri %s and mime %s", uri, info->mime);
			tracker_db_index_file (db_con, info, uri, attachment_service);
			tracker_dec_info_ref (info);
			g_free (uri);
		}
	

	} else {
		tracker_log ("ERROR : Failed to save email %s", mm->uri);
		tracker_db_end_transaction (db_con);
	}

	

	g_free (path);
	g_free (name);
	g_free (attachment_service);
	g_free (service);
	g_free (mime);


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
