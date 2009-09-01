/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
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

#define TRACKER_MINER_DISCOVER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_DISCOVER, TrackerMinerDiscoverPrivate))

typedef struct TrackerMinerDiscoverPrivate TrackerMinerDiscoverPrivate;

struct TrackerMinerDiscoverPrivate {
	DBusGConnection *connection;
	DBusGProxy *proxy;
	TrackerCrawler *crawler;
};

static void miner_discover_finalize (GObject *object);


G_DEFINE_TYPE (TrackerMinerDiscover, tracker_miner_discover, G_TYPE_OBJECT)

static void
tracker_miner_discover_class_init (TrackerMinerDiscoverClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = miner_discover_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerMinerDiscoverPrivate));
}

static void
tracker_miner_discover_init (TrackerMinerDiscover *discover)
{
	TrackerMinerDiscoverPrivate *priv;
	GError *error = NULL;

	priv = TRACKER_MINER_DISCOVER_GET_PRIVATE (discover);

	priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!priv->connection) {
		g_critical ("Could not connect to the DBus session bus, %s",
			    error ? error->message : "no error given.");
		g_clear_error (&error);
	}

	priv->proxy = dbus_g_proxy_new_for_name (priv->connection,
						 DBUS_SERVICE_DBUS,
						 DBUS_PATH_DBUS,
						 DBUS_INTERFACE_DBUS);

	if (!priv->proxy) {
		g_critical ("Could not get proxy for DBus service");
	}

	priv->crawler = tracker_crawler_new ();
}

static void
miner_discover_finalize (GObject *object)
{
	TrackerMinerDiscoverPrivate *priv;

	priv = TRACKER_MINER_DISCOVER_GET_PRIVATE (object);

	if (priv->proxy) {
		g_object_unref (priv->proxy);
	}

	if (priv->connection) {
		dbus_g_connection_unref (priv->connection);
	}

	G_OBJECT_CLASS (tracker_miner_discover_parent_class)->finalize (object);
}

TrackerMinerDiscover *
tracker_miner_discover_new (void)
{
	return g_object_new (TRACKER_TYPE_MINER_DISCOVER, NULL);
}

GSList *
tracker_miner_discover_get_running (TrackerMinerDiscover *discover)
{
	TrackerMinerDiscoverPrivate *priv;
	GSList *list = NULL;
	GError *error = NULL;
	gchar **p, **result;

	g_return_val_if_fail (TRACKER_IS_MINER_DISCOVER (discover), NULL);

	priv = TRACKER_MINER_DISCOVER_GET_PRIVATE (discover);

	if (!priv->connection || !priv->proxy) {
		return NULL;
	}

	if (!dbus_g_proxy_call (priv->proxy, "ListNames", &error,
				G_TYPE_INVALID,
				G_TYPE_STRV, &result,
				G_TYPE_INVALID)) {
		g_critical ("Could not get a list of names registered on the session bus, %s",
			    error ? error->message : "no error given");
		g_clear_error (&error);
		return NULL;
	}

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
tracker_miner_discover_get_available (TrackerMinerDiscover *discover)
{
	GSList *list = NULL;
	GMainLoop *main_loop;
	GFile *file;
	TrackerCrawler *crawler;

	crawler = tracker_crawler_new ();
	main_loop = g_main_loop_new (NULL, FALSE);

	g_signal_connect (crawler, "check-file",
			  G_CALLBACK (crawler_check_file_cb),
			  &list);
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb),
			  main_loop);

	/* Go through service files */
	file = g_file_new_for_path (DBUS_SERVICES_DIR);
	tracker_crawler_start (crawler, file, TRUE);
	g_object_unref (file);

	g_main_loop_run (main_loop);

	g_object_unref (crawler);

	return g_slist_reverse (list);
}
