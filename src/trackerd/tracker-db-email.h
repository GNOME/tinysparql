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


off_t	tracker_db_email_get_last_mbox_offset		(DBConnection *db_con, const char *mail_file_path);
void	tracker_db_email_update_mbox_offset		(DBConnection *db_con, MailFile *mf);
gboolean	tracker_db_email_save_email			(DBConnection *db_con, MailMessage *mm);
void	tracker_db_email_update_email			(DBConnection *db_con, MailMessage *mm);
void	tracker_db_email_delete_emails_of_mbox		(DBConnection *db_con, const char *mbox_file_path);
void	tracker_db_email_delete_email 			(DBConnection *db_con, const char *uri);
int	tracker_db_email_get_mbox_id 			(DBConnection *db_con, const char *mbox_uri);
void	tracker_db_email_insert_junk 			(DBConnection *db_con, const char *mbox_uri, guint32 uid);
char *** tracker_db_email_get_mbox_junk 		(DBConnection *db_con);
void	tracker_db_email_reset_mbox_junk 		(DBConnection *db_con, const char *mbox_uri);
void	tracker_db_email_flag_mbox_junk 		(DBConnection *db_con, const char *mbox_uri);
void	tracker_db_email_register_mbox 			(DBConnection *db_con, MailApplication mail_app, MailType mail_type, const char *path, const char *filename, const char *uri_prefix);
char *	tracker_db_email_get_mbox_path 			(DBConnection *db_con, const char *filename);

void
tracker_db_email_set_message_counts (DBConnection *db_con, const char *dir_path, int mail_count, int junk_count, int delete_count);

void
tracker_db_email_get_message_counts (DBConnection *db_con, const char *mbox_file_path, int *mail_count, int *junk_count, int *delete_count);

char *
tracker_db_email_get_mbox_uri_prefix (DBConnection *db_con, const char *mbox_uri);


gboolean
tracker_db_email_lookup_junk (DBConnection *db_con, const char *mbox_id, int uid);

void
tracker_db_email_free_mail_store (MailStore *store);

MailStore *
tracker_db_email_get_mbox_details (DBConnection *db_con, const char *mbox_uri);

char ***
tracker_db_email_get_mboxes (DBConnection *db_con);

#endif
