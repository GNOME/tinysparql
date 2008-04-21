/* Tracker
 * routines for emails with databases
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

#ifndef _TRACKER_DB_EMAIL_BASE_H_
#define _TRACKER_DB_EMAIL_BASE_H_

#include "tracker-db.h"
#include "tracker-email-utils.h"


off_t	  tracker_db_email_get_last_mbox_offset  (DBConnection *db_con, const gchar *mail_file_path);
void	  tracker_db_email_update_mbox_offset    (DBConnection *db_con, MailFile *mf);
gboolean  tracker_db_email_is_up_to_date         (DBConnection *db_con, const gchar *uri, guint32 *id);
gboolean  tracker_db_email_save_email	         (DBConnection *db_con, MailMessage *mm, MailApplication mail_app);
gboolean  tracker_db_email_is_saved_email_file   (DBConnection *db_con, const gchar *uri);
void	  tracker_db_email_update_email	         (DBConnection *db_con, MailMessage *mm);
gboolean  tracker_db_email_delete_email_file     (DBConnection *db_con, const gchar *uri);
gboolean  tracker_db_email_delete_emails_of_mbox (DBConnection *db_con, const gchar *mbox_file_path);
gboolean  tracker_db_email_delete_email          (DBConnection *db_con, const gchar *uri);
gint	  tracker_db_email_get_mbox_id 	         (DBConnection *db_con, const gchar *mbox_uri);
void	  tracker_db_email_insert_junk 	         (DBConnection *db_con, const gchar *mbox_uri, guint32 uid);
void	  tracker_db_email_reset_mbox_junk       (DBConnection *db_con, const gchar *mbox_uri);
void	  tracker_db_email_flag_mbox_junk        (DBConnection *db_con, const gchar *mbox_uri);
void	  tracker_db_email_register_mbox         (DBConnection *db_con, MailApplication mail_app, MailType mail_type,
                                                  const gchar *path, const gchar *filename, const gchar *uri_prefix);
gchar *	  tracker_db_email_get_mbox_path         (DBConnection *db_con, const gchar *filename);

void
tracker_db_email_set_message_counts (DBConnection *db_con, const gchar *dir_path,
                                     gint mail_count, gint junk_count, gint delete_count);

void
tracker_db_email_get_message_counts (DBConnection *db_con, const gchar *mbox_file_path,
                                     gint *mail_count, gint *junk_count, gint *delete_count);

gchar *
tracker_db_email_get_mbox_uri_prefix (DBConnection *db_con, const gchar *mbox_uri);

gboolean
tracker_db_email_lookup_junk (DBConnection *db_con, const gchar *mbox_id, gint uid);

void
tracker_db_email_free_mail_store (MailStore *store);

MailStore *
tracker_db_email_get_mbox_details (DBConnection *db_con, const gchar *mbox_uri);

TrackerDBResultSet *
tracker_db_email_get_mboxes (DBConnection *db_con);

#endif
