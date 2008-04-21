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
#include <gmodule.h>

#include <libtracker-common/tracker-config.h>

#include "tracker-email.h"
#include "tracker-email-utils.h"

extern Tracker *tracker;

static GModule *module = NULL;


/* must be called before any work on files containing mails */
void
tracker_email_add_service_directories (DBConnection *db_con)
{
	TrackerMailWatchEmails func;

	if (!module)
		return;

	if (g_module_symbol (module, "tracker_email_watch_emails", (gpointer *) &func)) {
		(func) (db_con);
        }
}


void
tracker_email_end_email_watching (void)
{
	TrackerMailFinalize func;

	if (!module)
		return;

	if (g_module_symbol (module, "tracker_email_finalize", (gpointer *) &func)) {
		(func) ();
	}

	g_mime_shutdown ();
}


gboolean
tracker_email_index_file (DBConnection *db_con, FileInfo *info)
{
	TrackerMailIndexFile func;

	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (info, FALSE);

	if (!module)
		return FALSE;

	if (!g_module_symbol (module, "tracker_email_index_file", (gpointer *) &func))
		return FALSE;

	return (func) (db_con, info);
}

gboolean
tracker_email_init (void)
{
	TrackerMailInit func;
	const gchar *email_client;
	gchar *module_name, *module_path;
	gboolean result = FALSE;

	if (module)
		return result;

	email_client = tracker_config_get_email_client (tracker->config);

	if (!email_client)
		return result;

	if (!g_module_supported ()) {
		g_error ("Modules are not supported by this platform");
		return result;
	}

	module_name = g_strdup_printf ("libemail-%s.so", email_client);
	module_path = g_build_filename (MAIL_MODULES_DIR, module_name, NULL);

	module = g_module_open (module_path, G_MODULE_BIND_LOCAL);

	if (!module) {
		g_warning ("Could not load EMail module: %s , %s\n", module_name, g_module_error ());
		g_free (module_name);
		g_free (module_path);
		return result;
	}

	g_module_make_resident (module);

	if (g_module_symbol (module, "tracker_email_init", (gpointer *) &func)) {
		g_mime_init (0);

		result = (func) ();
	}

	g_free (module_name);
	g_free (module_path);

	return result;
}

const gchar *
tracker_email_get_name (void)
{
	TrackerMailGetName func;

	if (!module)
		return NULL;

	if (!g_module_symbol (module, "tracker_email_get_name", (gpointer *) &func))
		return NULL;

	return (func) ();
}
