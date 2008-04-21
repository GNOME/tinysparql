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

#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#include <libtracker-common/tracker-log.h>

#include "tracker-db-email.h"

#define MAX_ADDRESSES 255

static gint
tracker_db_email_get_mbox_offset (DBConnection *db_con, const gchar *mbox_uri)
{
	TrackerDBResultSet *result_set;
	gint  offset;

	result_set = tracker_exec_proc (db_con, "GetMBoxDetails", mbox_uri, NULL);

	if (!result_set) {
		return -1;
	}

	tracker_db_result_set_get (result_set, 4, &offset, -1);
	g_object_unref (result_set);

	return offset;
}


void
tracker_db_email_register_mbox (DBConnection    *db_con,
                                MailApplication mail_app,
                                MailType        mail_type,
                                const gchar     *path,
                                const gchar     *filename,
                                const gchar     *uri_prefix)
{
	gchar *types[6] = {"MBOX", "IMAP", "IMAP4", "MAIL_TYPE_MAILDIR", "MAIL_TYPE_MH", "MAIL_TYPE_POP"};

	gchar *str_mail_app = tracker_int_to_str (mail_app);
	gchar *str_mail_type = tracker_int_to_str (mail_type);

	tracker_exec_proc (db_con, "InsertMboxDetails", str_mail_app, str_mail_type, filename, path, uri_prefix, NULL);

	tracker_log ("Registered email store %s of type %s", filename, types[mail_type]);

	g_free (str_mail_app);
	g_free (str_mail_type);
}


void
tracker_db_email_flag_mbox_junk (DBConnection *db_con, const gchar *mbox_uri)
{
	tracker_exec_proc (db_con, "SetJunkMbox", "1", mbox_uri, NULL);
}


void
tracker_db_email_reset_mbox_junk (DBConnection *db_con, const gchar *mbox_uri)
{
	tracker_exec_proc (db_con, "SetJunkMbox", "0", mbox_uri, NULL);
}


TrackerDBResultSet *
tracker_db_email_get_mboxes (DBConnection *db_con)
{
	return tracker_exec_proc (db_con, "GetMboxes", NULL);
}


void
tracker_db_email_insert_junk (DBConnection *db_con, const gchar *mbox_uri, guint32 uid)
{
	gint mbox_id = tracker_db_email_get_mbox_id (db_con, mbox_uri);
		
	if (mbox_id == -1) {
		return;
	}

	gchar *str_mbox_id = tracker_int_to_str (mbox_id);
	gchar *str_uid = tracker_uint_to_str (uid);

	if (!tracker_db_email_lookup_junk (db_con, str_mbox_id, uid)) {

		tracker_exec_proc (db_con, "InsertJunk", str_uid, str_mbox_id, NULL);
	}

	g_free (str_uid);	
	g_free (str_mbox_id);
}


MailStore *
tracker_db_email_get_mbox_details (DBConnection *db_con, const gchar *mbox_uri)
{
	TrackerDBResultSet *result_set;
        MailStore *mail_store;

        result_set = tracker_exec_proc (db_con, "GetMBoxDetails", mbox_uri, NULL);

        if (!result_set) {
                return NULL;
        }

	mail_store = g_slice_new0 (MailStore);
	tracker_db_result_set_get (result_set,
				   1, &mail_store->type,
				   3, &mail_store->uri_prefix,
				   4, &mail_store->offset,
				   6, &mail_store->mail_count,
				   7, &mail_store->junk_count,
				   8, &mail_store->delete_count,
				   -1);

	g_object_unref (result_set);

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


gint
tracker_db_email_get_mbox_id (DBConnection *db_con, const gchar *mbox_uri)
{
	TrackerDBResultSet *result_set;
	gint  id;

	result_set = tracker_exec_proc (db_con, "GetMboxID", mbox_uri, NULL);

	if (!result_set) {
		return -1;
	}

	tracker_db_result_set_get (result_set, 0, &id, -1);
	g_object_unref (result_set);

	return id;
}


gchar *
tracker_db_email_get_mbox_uri_prefix (DBConnection *db_con, const gchar *mbox_uri)
{
	TrackerDBResultSet *result_set;
	gchar *uri;

	result_set = tracker_exec_proc (db_con, "GetMBoxDetails", mbox_uri, NULL);

	if (!result_set) {
		return NULL;
	}

	tracker_db_result_set_get (result_set, 3, &uri, -1);
	g_object_unref (result_set);

	return uri;
}


gchar *
tracker_db_email_get_mbox_path (DBConnection *db_con, const gchar *filename)
{
	TrackerDBResultSet *result_set;
	gchar *path;

	result_set = tracker_exec_proc (db_con, "GetMBoxPath", filename, NULL);

	if (!result_set) {
		return NULL;
	}

	tracker_db_result_set_get (result_set, 0, &path, -1);
	g_object_unref (result_set);

	return path;
}


void
tracker_db_email_get_message_counts (DBConnection *db_con,
                                     const gchar  *mbox_file_path,
                                     gint         *mail_count,
                                     gint         *junk_count,
                                     gint         *delete_count)
{
	TrackerDBResultSet *result_set;

	*mail_count = 0;
	*junk_count = 0;
	*delete_count = 0;

	result_set = tracker_exec_proc (db_con, "GetMBoxDetails", mbox_file_path, NULL);

	if (!result_set) {
		return;
	}

	tracker_db_result_set_get (result_set,
				   6, mail_count,
				   7, junk_count,
				   8, delete_count,
				   -1);

	g_object_unref (result_set);
}


void
tracker_db_email_set_message_counts (DBConnection *db_con,
                                     const gchar  *dir_path,
                                     gint         mail_count,
                                     gint         junk_count,
                                     gint         delete_count)
{
	g_return_if_fail (db_con);
	g_return_if_fail (dir_path);
	
	/* make sure dir_path is registered in DB before doing an update */
	if (tracker_db_email_get_mbox_offset (db_con, dir_path) != -1) {
		gchar *str_mail_count, *str_junk_count, *str_delete_count;

		str_mail_count = tracker_uint_to_str (mail_count);
		str_junk_count = tracker_uint_to_str (junk_count);
		str_delete_count = tracker_uint_to_str (delete_count);

		tracker_exec_proc (db_con, "UpdateMboxCounts", str_mail_count, str_junk_count, str_delete_count, dir_path, NULL);

		g_free (str_mail_count);
		g_free (str_junk_count);
		g_free (str_delete_count);

	} else {
		tracker_error ("ERROR: invalid dir_path \"%s\"", dir_path);
	}
}


off_t
tracker_db_email_get_last_mbox_offset (DBConnection *db_con, const gchar *mbox_file_path)
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
		gchar *str_offset;

		str_offset = tracker_uint_to_str (mf->next_email_offset);
		tracker_exec_proc (db_con, "UpdateMboxOffset", str_offset, mf->path, NULL);

		g_free (str_offset);

	} else {
		tracker_error ("ERROR: invalid mbox \"%s\"", mf->path);
	}
}


static inline gchar *
get_utf8 (const gchar *str)
{
        if (!str) {
                return NULL;
        }

	gsize len = strlen (str);

	if (!g_utf8_validate (str, len, NULL)) {

		char *s = g_locale_to_utf8 (str, len, NULL, NULL, NULL);

		if (!s) {
			return g_strdup ("");
		} else {
			return s;
		}

	} else {
		return g_strdup (str);
	}
}


static gchar *
get_service_name (MailApplication app)
{
       if (app == MAIL_APP_EVOLUTION) {
               return g_strdup ("EvolutionEmails");
       } else if (app == MAIL_APP_MODEST) {
               return g_strdup ("ModestEmails");
       } else if (app == MAIL_APP_KMAIL) {
               return g_strdup ("KMailEmails");
       } else if (app == MAIL_APP_THUNDERBIRD) {
               return g_strdup ("ThunderbirdEmails");
       } else if (app == MAIL_APP_THUNDERBIRD_FEED) {
               return g_strdup ("ThunderbirdEmails");
       } else {
               return g_strdup ("OtherEmails");
       }
}


static gchar *
get_mime (MailApplication app)
{
       if (app == MAIL_APP_EVOLUTION) {
               return g_strdup ("Evolution/Email");
       } else if (app == MAIL_APP_MODEST) {
               return g_strdup ("Modest/Email");
       } else if (app == MAIL_APP_KMAIL) {
               return g_strdup ("KMail/Email");
       } else if (app == MAIL_APP_THUNDERBIRD) {
               return g_strdup ("Thunderbird/Email");
       } else if (app == MAIL_APP_THUNDERBIRD_FEED) {
               return g_strdup ("Thunderbird/Feed");
       } else {
               return g_strdup ("Other/Email");
       }
}


static gchar *
get_attachment_service_name (MailApplication app)
{
       if (app == MAIL_APP_EVOLUTION) {
               return g_strdup ("EvolutionAttachments");
       } else if (app == MAIL_APP_MODEST) {
               return g_strdup ("ModestAttachments");
       } else if (app == MAIL_APP_KMAIL) {
               return g_strdup ("KMailAttachments");
       } else if (app == MAIL_APP_THUNDERBIRD) {
               return g_strdup ("ThunderbirdAttachments");
       } else {
               return g_strdup ("OtherAttachments");
       }
}


gboolean
tracker_db_email_is_up_to_date (DBConnection *db_con, const gchar *uri, guint32 *id)
{
        return tracker_db_is_file_up_to_date (db_con->emails, uri, id);
}



static inline int
limit_address_count (int len)
{
	if (len > MAX_ADDRESSES - 1) {
		return MAX_ADDRESSES - 1;
	}

	return len;

}


gboolean
tracker_db_email_save_email (DBConnection *db_con, MailMessage *mm, MailApplication mail_app)
{
       	gint  mbox_id, type_id, id, len;
	gchar *service, *attachment_service, *mime;
	gchar *array[MAX_ADDRESSES];

     	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (mm, FALSE);

	if (!mm->uri) {
		tracker_error ("ERROR: email has no uri");
		return FALSE;
	}

	if (!mm->subject) {
		tracker_log ("WARNING: email with uri: %s has no subject",mm->uri);
		mm->subject = g_strdup("");
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
			tracker_db_email_insert_junk (db_con->index, mm->parent_mail_file->path, mm->id);
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

        mbox_id = -1; /* to have a default value */

	if (mm->parent_mail_file) {
		if (mm->is_mbox) {
			mbox_id = tracker_db_email_get_mbox_id (db_con->index, mm->parent_mail_file->path);
			if (mbox_id == -1) {
				tracker_error ("ERROR: no mbox is registered for email %s", mm->uri);
				return TRUE;
			}
		}
                service = get_service_name (mm->parent_mail_file->mail_app);
                mime = get_mime (mm->parent_mail_file->mail_app);
                attachment_service = get_attachment_service_name (mm->parent_mail_file->mail_app);

	} else {
		mbox_id = 0;
		mm->offset =0;
                service = get_service_name (mail_app);
                mime = get_mime (mail_app);
                attachment_service = get_attachment_service_name (mail_app);
	}

	type_id = tracker_service_manager_get_id_for_service (service);
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
        info->mtime = mm->mtime;

	id = tracker_db_create_service (db_con->index, service, info);

	tracker_free_file_info (info);

	if (id != 0) {
		GHashTable *index_table;
		gchar	   *str_id, *str_date;
		GSList     *tmp;


		tracker_info ("saving email service %d with uri \"%s\" and subject \"%s\" from \"%s\"", type_id, mm->uri, mm->subject, mm->from);

		index_table = g_hash_table_new (g_str_hash, g_str_equal);
		str_id = tracker_int_to_str (id);
                str_date = tracker_date_to_str (mm->date);
 
		if (mm->body) {
			gchar *value = get_utf8 (mm->body);

			if (value) {
				tracker_db_insert_single_embedded_metadata (db_con->index, service, str_id, "Email:Body", value, index_table);
				g_free (value);
			}			
		}

		if (str_date) {
			tracker_db_insert_single_embedded_metadata (db_con->index, service, str_id, "Email:Date", str_date, index_table);
		}

		if (mm->from) {
			gchar *value = get_utf8 (mm->from);
			
			if (value) {
				tracker_db_insert_single_embedded_metadata (db_con->index, service, str_id, "Email:Sender", value, index_table);
				g_free (value);
			}
		}

		if (mm->subject) {
			gchar *value = get_utf8 (mm->subject);

			if (value) {
				tracker_db_insert_single_embedded_metadata (db_con->index, service, str_id, "Email:Subject", value, index_table);
				g_free (value);
			}
		}

		g_free (str_date);

		len = g_slist_length (mm->to);
                len = limit_address_count (len);
		if (len > 0) {
                        gint i;
			array[len] = NULL;

			for (i = 0, tmp = mm->to; tmp && i < len; tmp = tmp->next, i++) {
				const MailPerson *mp = tmp->data;
				GString *gstr = g_string_new ("");

				if (mp->addr) {
					
					gchar *value = get_utf8 (mp->addr);
					
					if (value) {
						g_string_append_printf (gstr, "%s ", value);
						g_free (value);
					}
				}

				if (mp->name) {
					
					gchar *value = get_utf8 (mp->name);
					
					if (value) {
						g_string_append (gstr, value);
						g_free (value);
					}
				}

				array[i] = g_string_free (gstr, FALSE);
			}

			if (i > 0) {
				tracker_db_insert_embedded_metadata (db_con->index, service, str_id, "Email:SentTo", array, i, index_table);
			}

			for (i--; i>-1; i--) {
				g_free (array[i]);
			}
		}

		len = g_slist_length (mm->cc);
                len = limit_address_count (len);
		if (len > 0) {
                        gint i;
			array[len] = NULL;

			for (i = 0, tmp = mm->cc; tmp && i < len; tmp = tmp->next, i++) {
				const MailPerson *mp = tmp->data;
				GString *gstr = g_string_new ("");

				if (mp->addr) {
					gchar *value = get_utf8 (mp->addr);
					
					if (value) {
						g_string_append_printf (gstr, "%s ", value);
						g_free (value);
					}
				}
	
				if (mp->name) {
					gchar *value = get_utf8 (mp->name);
					
					if (value) {
						g_string_append_printf (gstr, "%s ", value);
						g_free (value);
					}
				}

				array[i] = g_string_free (gstr, FALSE);
			}

			if (i > 0) {
				tracker_db_insert_embedded_metadata (db_con->index, service, str_id, "Email:CC", array, i, index_table);
			}

			for (i--; i > -1; i--) {
				g_free (array[i]);
			}		
		}

		len = g_slist_length (mm->attachments);
                len = limit_address_count (len);
		if (len > 0) {
                        gint i;
			array[len] = NULL;
		
			for (i = 0, tmp = mm->attachments; tmp && i < len; tmp = tmp->next, i++) {
				const MailAttachment *ma = tmp->data;

				if (!ma->attachment_name) {
					continue;
				}
				gchar *value = get_utf8 (ma->attachment_name);
				
				if (value) {
					array[i] = value;
				} else {
					array[i] = g_strdup ("unknown");
				}
			}

			if (i > 0) {
				tracker_db_insert_embedded_metadata (db_con->index, service, str_id, "Email:Attachments", array, i, index_table);
			}

			for (i--; i > -1; i--) {
				g_free (array[i]);
			}
		}

		tracker_db_update_indexes_for_new_service (id, type_id, index_table);

		tracker_word_table_free (index_table);


		g_free (str_id);

               
		/* index attachments */
		for (tmp = mm->attachments; tmp; tmp = tmp->next) {
			MailAttachment *ma = tmp->data;

                        if (ma->tmp_decoded_file) {
                                FileInfo *attachment_info;
                                gchar 	 *locale_path, *uri;

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


gboolean
tracker_db_email_is_saved_email_file (DBConnection *db_con, const gchar *uri)
{
        g_return_val_if_fail (db_con, FALSE);
        g_return_val_if_fail (uri, FALSE);

        return tracker_db_get_file_id (db_con, uri) != 0 ;
}


void
tracker_db_email_update_email (DBConnection *db_con, MailMessage *mm)
{
	g_return_if_fail (db_con);
	g_return_if_fail (mm);

	tracker_log ("update email with uri \"%s\" and subject \"%s\" from \"%s\"", mm->uri, mm->subject, mm->from);

	/* FIXME: add code... */
}


gboolean
tracker_db_email_delete_emails_of_mbox (DBConnection *db_con, const gchar *mbox_file_path)
{
        g_return_val_if_fail (db_con, FALSE);
        g_return_val_if_fail (mbox_file_path, FALSE);

	/* FIXME: add code... */
        return TRUE;
}


gboolean
tracker_db_email_delete_email (DBConnection *db_con, const gchar *uri)
{
        g_return_val_if_fail (db_con, FALSE);
        g_return_val_if_fail (uri, FALSE);

	gchar *id = tracker_db_get_id (db_con, "Emails", uri);

	if (!id) {
		return FALSE;
	}

	tracker_info ("deleting email %s", uri);

	tracker_db_delete_directory (db_con, atoi (id), uri);

	g_free (id);

        return TRUE;
}


gboolean
tracker_db_email_lookup_junk (DBConnection *db_con, const gchar *mbox_id, gint uid)
{
	TrackerDBResultSet *result_set;
        gchar *str_uid;
	gint id;

	str_uid = tracker_uint_to_str (uid);

	result_set = tracker_exec_proc (db_con, "LookupJunk", str_uid, mbox_id, NULL);

	g_free (str_uid);

	if (!result_set) {
		return FALSE;
	}

	tracker_db_result_set_get (result_set, 0, &id, -1);
	g_object_unref (result_set);

	return (id > 0);
}
