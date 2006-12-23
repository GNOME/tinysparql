/* Tracker
 * mbox routines
 * Copyright (C) 2006, Laurent Aguerreche
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


#ifndef _TRACKER_MBOX_KMAIL_H_
#define _TRACKER_MBOX_KMAIL_H_

#include <glib.h>
#include <gmime/gmime.h>

#include "tracker-mbox.h"

#include "config.h"

#ifdef USING_SQLITE
#   include "tracker-db-sqlite.h"
#else
#   include "tracker-db-mysql.h"
#endif


/*
 * These functions are supposed to be used only with tracker-mbox.c
 *
 */

void		init_kmail_mboxes_module	(void);
void		finalize_kmail_mboxes_module	(void);
GSList *	watch_emails_of_kmail		(DBConnection *db_con);
void		get_status_of_kmail_email	(GMimeMessage *g_m_message, MailMessage *msg);
void		get_uri_of_kmail_email		(GMimeMessage *g_m_message, MailMessage *msg);
gboolean	is_in_a_kmail_mail_dir		(const char *uri);

#endif
