/* Tracker
 * routines for emails
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

#include <glib.h>
#include <glib/gstdio.h>

#include "tracker-email.h"
#include "tracker-email-utils.h"
#include "tracker-email-evolution.h"
#include "tracker-email-thunderbird.h"
#include "tracker-email-kmail.h"


extern Tracker *tracker;


/* must be called before any work on files containing mails */
void
tracker_email_add_service_directories (DBConnection *db_con)
{

	g_mime_init (0);

	if (tracker->index_evolution_emails) {
		if (evolution_init_module ()) {
			evolution_watch_emails (db_con);
		}
	}

	if (tracker->index_kmail_emails) {
		if (kmail_init_module ()) {
			kmail_watch_emails (db_con);
		}
	}

	if (tracker->index_thunderbird_emails) {
		if (thunderbird_init_module ()) {
			thunderbird_watch_emails ();
		}
	}
}


void
tracker_email_end_email_watching (void)
{
	//email_free_root_path_for_attachments ();

	if (evolution_module_is_running ()) {
		evolution_finalize_module ();
	}

	if (kmail_module_is_running ()) {
		kmail_finalize_module ();
	}

	if (thunderbird_module_is_running ()) {
		thunderbird_finalize_module ();
	}

	g_mime_shutdown ();
} 


gboolean
tracker_email_file_is_interesting (FileInfo *info, const char *service)
{
	g_return_val_if_fail (info, FALSE);

	if (g_str_has_prefix (service, "Evolution") && evolution_module_is_running ()) {
		return evolution_file_is_interesting (info, service);
	
	} else if (g_str_has_prefix (service, "KMail") && kmail_module_is_running ()) {
		return kmail_file_is_interesting (info, service);

	} else if (g_str_has_prefix (service, "Thunderbird") && thunderbird_module_is_running ()) {
		return thunderbird_file_is_interesting (info, service);
	
	} else {
		return FALSE;
	}


}


gboolean
tracker_email_index_file (DBConnection *db_con, FileInfo *info, const char *service)
{
	gboolean has_been_handled;

	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (info, FALSE);

	has_been_handled = TRUE;

	if (evolution_module_is_running () && evolution_file_is_interesting (info, service)) {
		evolution_index_file (db_con, info);

	} else if (kmail_module_is_running () && kmail_file_is_interesting (info, service)) {
		kmail_index_file (db_con, info);

	} else if (thunderbird_module_is_running () && thunderbird_file_is_interesting (info, service)) {
		thunderbird_index_file (db_con, info);

	} else {
		has_been_handled = FALSE;
	}

	return has_been_handled;
}



