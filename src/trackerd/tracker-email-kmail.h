/* Tracker
 * routines for emails with KMail
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

#ifndef _TRACKER_EMAIL_KMAIL_H_
#define _TRACKER_EMAIL_KMAIL_H_

#include <glib.h>
#include <gmime/gmime.h>

#include "config.h"

#ifdef USING_SQLITE
#   include "tracker-db-sqlite.h"
#else
#   include "tracker-db-mysql.h"
#endif


/*
 * These functions are supposed to be used only with tracker-email.c
 *
 */

gboolean	kmail_init_module		(void);
gboolean	kmail_module_is_running		(void);
gboolean	kmail_finalize_module		(void);
void		kmail_watch_emails		(DBConnection *db_con);
gboolean	kmail_file_is_interesting	(DBConnection *db_con, FileInfo *info);
void		kmail_index_file		(DBConnection *db_con, FileInfo *info);

#endif
