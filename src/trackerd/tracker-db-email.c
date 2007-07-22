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
#include <glib/gstdio.h>

#include "tracker-db-email.h"




static int
tracker_db_email_get_mbox_offset (DBConnection *db_con, const char *mbox_uri)
{
	char	***res;
	char	**row;
	int	offset;

	res = tracker_exec_proc (db_con, "GetMBoxDetails", 1, mbox_uri);

	if (!res) {
		return -1;
	}

	row = tracker_db_get_row (res, 0);

	if (!(row && row[0] && row[4])) {
		return -1;
	} else {
		offset = atoi (row[4]);
	}

	tracker_db_free_result (res);

	return offset;
}

void
tracker_db_email_register_mbox (DBConnection *db_con, MailApplication mail_app, MailType mail_type, const char *path, const char *filename, const char *uri_prefix)
{
	char *types[5] = {"MBOX", "IMAP", "IMAP4", "MAIL_TYPE_MAILDIR",	"MAIL_TYPE_MH"};

	char *str_mail_app = tracker_int_to_str (mail_app);
	char *str_mail_type = tracker_int_to_str (mail_type);

	tracker_exec_proc (db_con, "InsertMboxDetails", 5, str_mail_app, str_mail_type, filename, path, uri_prefix);



	tracker_log ("Registered email store %s of type %s", filename, types[mail_type]);

	g_free (str_mail_app);
	g_free (str_mail_type);
}


void
tracker_db_email_flag_mbox_junk (DBConnection *db_con, const char *mbox_uri)
{
	tracker_exec_proc (db_con, "SetJunkMbox", 2, "1", mbox_uri);
}

void
tracker_db_email_reset_mbox_junk (DBConnection *db_con, const char *mbox_uri)
{
	tracker_exec_proc (db_con, "SetJunkMbox", 2, "0", mbox_uri);
}


char ***
tracker_db_email_get_mboxes (DBConnection *db_con)
{
	return tracker_exec_proc (db_con, "GetMboxes", 0);
}


void
tracker_db_email_insert_junk (DBConnection *db_con, const char *mbox_uri, guint32 uid)
{
	int mbox_id = tracker_db_email_get_mbox_id (db_con, mbox_uri);
		
	if (mbox_id == -1) {
		return;
	}

	char *str_mbox_id = tracker_int_to_str (mbox_id);
	char *str_uid = tracker_uint_to_str (uid);

	tracker_exec_proc (db_con, "InsertJunk", 2, str_uid, str_mbox_id);


	g_free (str_uid);	
	g_free (str_mbox_id);
}


MailStore *
tracker_db_email_get_mbox_details (DBConnection *db_con, const char *mbox_uri)
{
        char      ***res;
        char      **row;
        MailStore *mail_store;

        res = tracker_exec_proc (db_con, "GetMBoxDetails", 1, mbox_uri);

        if (!res) {
                return NULL;
        }

        mail_store = NULL;

        row = tracker_db_get_row (res, 0);

        if (!(row && row[3] && row[4])) {
                return NULL;
        } else {

                mail_store = g_slice_new0 (MailStore);

                mail_store->offset = atoi (row[4]);
                mail_store->mail_count = atoi (row[6]);
                mail_store->junk_count = atoi (row[7]);
                mail_store->delete_count = atoi (row[8]);
                mail_store->uri_prefix = g_strdup (row[3]);
                mail_store->type = atoi (row[1]);
        }

        tracker_db_free_result (res);

        return mail_store;
}



void
tracker_db_email_free_mail_store (MailStore *store)
{

        g_return_if_fail (store);

        if (store->uri_prefix) {
                g_free (store->uri_prefix);
        }

        g_slice_free (MailStore, store);
}




int
tracker_db_email_get_mbox_id (DBConnection *db_con, const char *mbox_uri)
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





char *
tracker_db_email_get_mbox_uri_prefix (DBConnection *db_con, const char *mbox_uri)
{
	char	***res;
	char	**row;
	char	*uri;

	res = tracker_exec_proc (db_con, "GetMBoxDetails", 1, mbox_uri);

	if (!res) {
		return NULL;
	}

	row = tracker_db_get_row (res, 0);

	if (!(row && row[3])) {
		return NULL;
	} else {
		uri = g_strdup (row[3]);
	}

	tracker_db_free_result (res);

	return uri;
}


char *
tracker_db_email_get_mbox_path (DBConnection *db_con, const char *filename)
{
	char	***res;
	char	**row;
	char	*path;

	res = tracker_exec_proc (db_con, "GetMBoxPath", 1, filename);

	if (!res) {
		return NULL;
	}

	row = tracker_db_get_row (res, 0);

	if (!(row && row[0])) {
		return NULL;
	} else {
		path = g_strdup (row[0]);
	}

	tracker_db_free_result (res);

	return path;
}



void
tracker_db_email_get_message_counts (DBConnection *db_con, const char *mbox_file_path, int *mail_count, int *junk_count, int *delete_count)
{
	char	***res;
	char	**row;

	*mail_count = 0;
	*junk_count = 0;
	*delete_count = 0;

	res = tracker_exec_proc (db_con, "GetMBoxDetails", 1, mbox_file_path);

	if (!res) {
		return;
	}

	row = tracker_db_get_row (res, 0);

	if (!(row && row[6] && row[7] && row[8] )) {
		return;
	} else {
		*mail_count = atoi (row[6]);
		*junk_count = atoi (row[7]);
		*delete_count = atoi (row[8]);
		
	}

	tracker_db_free_result (res);

}

void
tracker_db_email_set_message_counts (DBConnection *db_con, const char *dir_path, int mail_count, int junk_count, int delete_count)
{
	g_return_if_fail (db_con);
	g_return_if_fail (dir_path);

	
	/* make sure dir_path is registered in DB before doing an update */
	if (tracker_db_email_get_mbox_offset (db_con, dir_path) != -1) {
		char *str_mail_count, *str_junk_count, *str_delete_count;

		str_mail_count = tracker_uint_to_str (mail_count);
		str_junk_count = tracker_uint_to_str (junk_count);
		str_delete_count = tracker_uint_to_str (delete_count);

		tracker_exec_proc (db_con, "UpdateMboxCounts", 4, str_mail_count, str_junk_count, str_delete_count, dir_path);

		g_free (str_mail_count);
		g_free (str_junk_count);
		g_free (str_delete_count);

	} else {
		tracker_error ("ERROR: invalid dir_path \"%s\"", dir_path);
	}
}




off_t
tracker_db_email_get_last_mbox_offset (DBConnection *db_con, const char *mbox_file_path)
{
	off_t offset;

	g_return_val_if_fail (db_con, 0);
	g_return_val_if_fail (mbox_file_path, 0);

	offset = tracker_db_email_get_mbox_offset (db_con, mbox_file_path);

	if (offset == -1) {
		/* we need to add this mbox */
		tracker_error ("ERROR: mbox/dir for emails for %s is not registered", mbox_file_path);
	}

	return offset;
}



void
tracker_db_email_update_mbox_offset (DBConnection *db_con, MailFile *mf)
{
	g_return_if_fail (db_con);
	g_return_if_fail (mf);

	if (!mf->path) {
		tracker_error ("ERROR: invalid mbox (empty path!)");
		return;
	}

	/* make sure mbox is registered in DB before doing an update */
	if (tracker_db_email_get_mbox_offset (db_con, mf->path) != -1) {
		char *str_offset;

		str_offset = tracker_uint_to_str (mf->next_email_offset);
		tracker_exec_proc (db_con, "UpdateMboxOffset", 2, str_offset, mf->path);

		g_free (str_offset);

	} else {
		tracker_error ("ERROR: invalid mbox \"%s\"", mf->path);
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


gboolean
tracker_db_email_save_email (DBConnection *db_con, MailMessage *mm)
{
       	int	mbox_id, type_id, id, i, len;
	char    *service, *attachment_service, *mime;
	char 	*array[255];

        #define LIMIT_ARRAY_LENGTH(len) ((len) > 255 ? 255 : (len))

	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (mm, FALSE);

	if (!mm->uri) {
		tracker_error ("ERROR: email has no uri");
		return FALSE;
	}

	if (mm->parent_mail_file && !mm->parent_mail_file->path) {
		tracker_error ("ERROR: badly formatted email - abandoning index");
		return FALSE;
	}

	if (mm->store) {
		mm->store->mail_count++;
	} else {
		tracker_error ("WARNING: no mail store found for email");
	}

	if (mm->deleted || mm->junk) {
		if (mm->parent_mail_file) {
			tracker_db_email_insert_junk (db_con, mm->parent_mail_file->path, mm->id);
		}

		if (mm->store) {
			if (mm->deleted) {
				mm->store->delete_count++;
			} else {
				mm->store->junk_count++;
			}
		} else {
			tracker_error ("WARNING: no mail store found for email");
		}

		return TRUE;
	}

	if (mm->parent_mail_file) {
		
		if (mm->is_mbox) {
		
			mbox_id = tracker_db_email_get_mbox_id (db_con, mm->parent_mail_file->path);

			if (mbox_id == -1) {
				tracker_error ("ERROR: no mbox is registered for email %s", mm->uri);
				return TRUE;
			}
		}
	
		//service = get_service_name (mm->parent_mail_file->mail_app);
		//mime = get_mime (mm->parent_mail_file->mail_app);
		//attachment_service = get_attachment_service_name (mm->parent_mail_file->mail_app);

		service = g_strdup ("EvolutionEmails");
		mime = g_strdup ("Evolution/Email");
		attachment_service = g_strdup ("EvolutionAttachments");

		
	} else {
		mbox_id = 0;
		mm->offset =0;
		service = g_strdup ("EvolutionEmails");
		mime = g_strdup ("Evolution/Email");
		attachment_service = g_strdup ("EvolutionAttachments");
	}

	
	type_id = tracker_get_id_for_service (service);
	if (type_id == -1) {
		tracker_error ("ERROR: service %s not found", service);
		g_free (attachment_service);
		g_free (service);
		g_free (mime);
		return TRUE;
	}

	FileInfo *info = tracker_create_file_info (mm->uri, 0,0,0);

	info->mime = mime;
	info->offset = mm->offset;
	info->aux_id = mbox_id;

	id = tracker_db_create_service (db_con, service, info);

	tracker_free_file_info (info);

	if (id != 0) {
		GHashTable      *index_table;
		char		*str_id, *str_date;
		GSList          *tmp;

		tracker_db_start_transaction (db_con);

		tracker_info ("saving email service %d with uri \"%s\" and subject \"%s\" from \"%s\"", type_id, mm->uri, mm->subject, mm->from);

		index_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		str_id = tracker_int_to_str (id);
		str_date = tracker_int_to_str (mm->date);


		if (mm->body) {
			tracker_db_insert_single_embedded_metadata (db_con, service, str_id, "Email:Body", mm->body, index_table);
		}

		if (str_date) {
			tracker_db_insert_single_embedded_metadata (db_con, service, str_id, "Email:Date", str_date, index_table);
		}

		if (mm->from) {
			tracker_db_insert_single_embedded_metadata (db_con, service, str_id, "Email:Sender", mm->from, index_table);
		}

		if (mm->subject) {
			tracker_db_insert_single_embedded_metadata (db_con, service, str_id, "Email:Subject", mm->subject, index_table);
		}

		g_free (str_date);

		i = 0;
		len = g_slist_length (mm->to);
                len = LIMIT_ARRAY_LENGTH (len);
		if (len > 0) {
			array[len] = NULL;

			for (tmp = mm->to; tmp; tmp = tmp->next) {
				const MailPerson *mp;
				GString *gstr = g_string_new ("");

				mp = tmp->data;

				if (mp->addr) {
					g_string_append_printf (gstr, "%s ", mp->addr);
				}
	
				if (mp->name) {
					g_string_append (gstr, mp->name);
				}

				array[i] = g_string_free (gstr, FALSE);
				i++;	
			}

			if (i > 0) {
				tracker_db_insert_embedded_metadata (db_con, service, str_id, "Email:SentTo", array, i, index_table);
			}

			for (i--; i>-1; i--) {
				g_free (array[i]);
			}
		}

		i = 0;
		len = g_slist_length (mm->cc);
                len = LIMIT_ARRAY_LENGTH (len);
		if (len > 0) {
			array[len] = NULL;

			for (tmp = mm->cc; tmp; tmp = tmp->next) {
				const MailPerson *mp;
				GString *gstr = g_string_new ("");

				mp = tmp->data;

				if (mp->addr) {
					g_string_append_printf (gstr, "%s ", mp->addr);
				}
	
				if (mp->name) {
					g_string_append (gstr, mp->name);
				}

				array[i] = g_string_free (gstr, FALSE);
				i++;
			}

			if (i > 0) {
				tracker_db_insert_embedded_metadata (db_con, service, str_id, "Email:CC", array, i, index_table);
			}

			for (i--; i>-1; i--) {
				g_free (array[i]);
			}		
		}

		i = 0;
		len = g_slist_length (mm->attachments);
                len = LIMIT_ARRAY_LENGTH (len);
		if (len > 0) {
			array[len] = NULL;
		
			for (tmp = mm->attachments; tmp; tmp = tmp->next) {
				const MailAttachment *ma;

				ma = tmp->data;

				if (!ma->attachment_name) {
					continue;
				}

				array[i] = g_strdup (ma->attachment_name);
				i++;
			}

			if (i > 0) {
				tracker_db_insert_embedded_metadata (db_con, service, str_id, "Email:Attachments", array, i, index_table);
			}

			for (i--; i>-1; i--) {
				g_free (array[i]);
			}
		}

		tracker_db_end_transaction (db_con);

		tracker_db_update_indexes_for_new_service (id, type_id, index_table);

		g_hash_table_destroy (index_table);

		g_free (str_id);

		/* index attachments */
		for (tmp = mm->attachments; tmp; tmp = tmp->next) {
			MailAttachment	*ma;

			ma = tmp->data;

                        if (ma->tmp_decoded_file) {
                                FileInfo	*attachment_info;
                                char 		*locale_path, *uri;

        			locale_path = g_filename_from_utf8 (ma->tmp_decoded_file, -1, NULL, NULL, NULL);

                                if (!g_file_test (locale_path, G_FILE_TEST_EXISTS)) {						
                                        g_free (locale_path);
                                        continue;
                                }

                                attachment_info = tracker_create_file_info (ma->tmp_decoded_file, TRACKER_ACTION_CHECK, 0, WATCH_OTHER);
                                attachment_info->is_directory = FALSE;
                                attachment_info->mime = g_strdup (ma->mime);

                                uri = g_strconcat (mm->uri, "/", ma->attachment_name, NULL);
                                tracker_info ("indexing attachment with uri %s and mime %s", uri, attachment_info->mime);
                                tracker_db_index_file (db_con, attachment_info, uri, attachment_service);
                                g_free (uri);

                                tracker_dec_info_ref (attachment_info);

                                /* remove temporary and indexed attachment file */
                                g_unlink (locale_path);

                                g_free (locale_path);

                                /* attachment file does not exist anymore so we remove its name in MailAttachment */
                                g_free (ma->tmp_decoded_file);
                                ma->tmp_decoded_file = NULL;
                        }
		}

	} else {
		tracker_error ("ERROR: failed to save email %s", mm->uri);
	}

	g_free (attachment_service);
	g_free (service);

	return TRUE;
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
tracker_db_email_delete_email (DBConnection *db_con, const char *uri)
{
	g_return_if_fail (db_con);
	g_return_if_fail (uri);

	char *id = tracker_db_get_id (db_con, "Emails", uri);

	if (!id) {
		return;
	}

	tracker_info ("deleting email %s", uri);

	tracker_db_delete_directory (db_con, db_con->blob, atoi (id), uri);

	g_free (id);
}


gboolean
tracker_db_email_lookup_junk (DBConnection *db_con, const char *mbox_id, int uid)
{
			
	char *str_uid;
	char	***res;
	char	**row;	
														
	str_uid = tracker_uint_to_str (uid);

	res = tracker_exec_proc (db_con, "LookupJunk", 2, str_uid, mbox_id);

	if (!res) {
		g_free (str_uid);
		return FALSE;
	}

	row = tracker_db_get_row (res, 0);

	if (!(row && row[0] )) {
		tracker_db_free_result (res);
		g_free (str_uid);
		return FALSE;
	} 

	tracker_db_free_result (res);

	g_free (str_uid);

	return TRUE;
}


