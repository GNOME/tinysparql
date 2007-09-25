/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <config.h>

#include <locale.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include "../libtracker/tracker.h" 


gint
main (gint argc, gchar *argv[])
{
	GError *error = NULL;
	TrackerClient *client = NULL;

	setlocale (LC_ALL, "");

        bindtextdomain (GETTEXT_PACKAGE, TRACKER_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

	client =  tracker_connect (FALSE);

        if (!client) {
                g_printerr (_("%s: no connection to tracker daemon"), argv[0]);
                g_printerr ("\n");
                g_printerr (_("Ensure \"trackerd\" is running before launch this command."));
                g_printerr ("\n");
                return 1;
        }

        gchar* status = tracker_get_status (client, &error);

        g_print ("Tracker daemon's status is %s.\n", status);

	tracker_disconnect (client);

	return 0;
}
