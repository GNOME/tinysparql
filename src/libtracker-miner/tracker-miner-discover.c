/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia
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

#include "config.h"

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-type-utils.h>

#include "tracker-crawler.h"
#include "tracker-miner.h"
#include "tracker-miner-discover.h"

GSList *
tracker_miner_discover_get_running (void)
{
	DBusGConnection *connection;
	DBusGProxy *gproxy;
	GSList *list;
	GError *error = NULL;
	gchar **p, **result;
	
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection) {
		g_critical ("Could not connect to the DBus session bus, %s",
			    error ? error->message : "no error given.");
		g_clear_error (&error);
		return NULL;
	}

	/* The definitions below (DBUS_SERVICE_DBUS, etc) are
	 * predefined for us to just use (dbus_g_proxy_...)
	 */
	gproxy = dbus_g_proxy_new_for_name (connection,
					    DBUS_SERVICE_DBUS,
					    DBUS_PATH_DBUS,
					    DBUS_INTERFACE_DBUS);

	if (!gproxy) {
		g_critical ("Could not get proxy for DBus service");
		return NULL;
	}

	if (!dbus_g_proxy_call (gproxy, "ListNames", &error, 
				G_TYPE_INVALID, 
				G_TYPE_STRV, &result, 
				G_TYPE_INVALID)) {
		g_critical ("Could not get a list of names registered on the session bus, %s",
			    error ? error->message : "no error given");
		g_clear_error (&error);
		g_object_unref (gproxy);
		return NULL;
	}

	g_object_unref (gproxy);

	list = NULL;

	if (result) {
		for (p = result; *p; p++) {
			if (g_str_has_prefix (*p, TRACKER_MINER_DBUS_NAME_PREFIX)) {
				list = g_slist_prepend (list, g_strdup (*p));
			}
		}

		list = g_slist_reverse (list);

		g_strfreev (result);
	}

	return list;
}

static gboolean
crawler_check_file_cb (TrackerCrawler *crawler,
		       GFile          *file,
		       gpointer        user_data)
{
	gchar *basename;

	basename = g_file_get_basename (file);

	if (g_str_has_prefix (basename, TRACKER_MINER_DBUS_NAME_PREFIX)) {
		gchar *p;

		p = strstr (basename, ".service");
		
		if (p) {
			GSList **list = user_data;

			*p = '\0';
			*list = g_slist_prepend (*list, basename);
		} else {
			g_free (basename);
		}
		
		return TRUE;
	} 

	g_free (basename);

	return FALSE;
}

static void
crawler_finished_cb (TrackerCrawler *crawler,
		     GQueue         *found,
		     gboolean        was_interrupted,
		     guint           directories_found, 
		     guint           directories_ignored, 
		     guint           files_found, 
		     guint           files_ignored,
		     gpointer        user_data)
{
	g_main_loop_quit (user_data);
}

GSList *
tracker_miner_discover_get_available (void)
{
	GSList *list = NULL;
	GMainLoop *main_loop;
	TrackerCrawler *crawler;

	crawler = tracker_crawler_new ();
	if (!crawler) {
		g_critical ("Couldn't create TrackerCrawler object");
		return NULL;
	}

	main_loop = g_main_loop_new (NULL, FALSE);

	g_signal_connect (crawler, "check-file", 
			  G_CALLBACK (crawler_check_file_cb),
			  &list);
	g_signal_connect (crawler, "finished", 
			  G_CALLBACK (crawler_finished_cb),
			  main_loop);

	/* Go through service files */
	tracker_crawler_start (crawler, DBUS_SERVICES_DIR, TRUE);

	g_main_loop_run (main_loop);

	g_object_unref (crawler);

	return g_slist_reverse (list);
}
