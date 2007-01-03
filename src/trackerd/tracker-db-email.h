/* Tracker
 * routines for emails with databases
 * Copyright (C) 2006, Laurent Aguerreche (laurent.aguerreche@free.fr)
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

#ifndef _TRACKER_DB_EMAIL_BASE_H_
#define _TRACKER_DB_EMAIL_BASE_H_

#include "tracker-db.h"
#include "tracker-email-utils.h"

off_t	tracker_db_email_get_last_mbox_offset	(DBConnection *db_con, const char *mail_file_path);
void	tracker_db_email_update_mbox_offset	(DBConnection *db_con, MailFile *mf);
void	tracker_db_email_save_email		(DBConnection *db_con, MailMessage *mm);

#endif
