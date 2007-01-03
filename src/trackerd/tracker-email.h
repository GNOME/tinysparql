/* Tracker
 * routines for emails
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

#ifndef _TRACKER_EMAIL_H_
#define _TRACKER_EMAIL_H_

#include "tracker-utils.h"

#include "config.h"

#ifdef USING_SQLITE
#   include "tracker-db-sqlite.h"
#else
#   include "tracker-db-mysql.h"
#endif

void		tracker_email_watch_emails		(DBConnection *db_con);
void		tracker_email_end_email_watching	(void);
gboolean	tracker_email_file_is_interesting	(DBConnection *db_con, FileInfo *info);
gboolean	tracker_email_index_file		(DBConnection *db_con, FileInfo *info);
gboolean	tracker_email_is_an_attachment		(FileInfo *info);
void		tracker_email_unlink_email_attachment	(const char *uri);

#endif
