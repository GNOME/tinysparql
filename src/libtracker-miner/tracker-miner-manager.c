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
#include "tracker-miner-manager.h"
#include "tracker-marshal.h"
#include "tracker-miner-client.h"

#define TRACKER_MINER_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_MANAGER, TrackerMinerManagerPrivate))

typedef struct TrackerMinerManagerPrivate TrackerMinerManagerPrivate;

struct TrackerMinerManagerPrivate {
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GHashTable *miner_proxies;
};

static void miner_manager_finalize (GObject *object);


G_DEFINE_TYPE (TrackerMinerManager, tracker_miner_manager, G_TYPE_OBJECT)

enum {
	MINER_PROGRESS,
	MINER_PAUSED,
	MINER_RESUMED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

static void
tracker_miner_manager_class_init (TrackerMinerManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = miner_manager_finalize;

	signals [MINER_PROGRESS] =
		g_signal_new ("miner-progress",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerManagerClass, miner_progress),
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_STRING_DOUBLE,
			      G_TYPE_NONE, 3,
			      G_TYPE_STRING,
			      G_TYPE_STRING,
			      G_TYPE_DOUBLE);
	signals [MINER_PAUSED] =
		g_signal_new ("miner-paused",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerManagerClass, miner_paused),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);
	signals [MINER_RESUMED] =
		g_signal_new ("miner-resumed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerManagerClass, miner_resumed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (TrackerMinerManagerPrivate));
}

static DBusGProxy *
find_miner_proxy (TrackerMinerManager *manager,
		  const gchar         *name)
{
	TrackerMinerManagerPrivate *priv;
	GHashTableIter iter;
	gpointer key, value;

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);
	g_hash_table_iter_init (&iter, priv->miner_proxies);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		if (g_strcmp0 (name, (gchar *) value) == 0) {
			return key;
		}
	}

	return NULL;
}

static void
miner_progress_changed (DBusGProxy  *proxy,
			const gchar *status,
			gdouble      progress,
			gpointer     user_data)
{
	TrackerMinerManager *manager = user_data;
	TrackerMinerManagerPrivate *priv;
	const gchar *name;

	manager = user_data;
	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);
	name = g_hash_table_lookup (priv->miner_proxies, proxy);

	g_signal_emit (manager, signals[MINER_PROGRESS], 0, name, status, progress);
}

static void
miner_paused (DBusGProxy *proxy,
	      gpointer    user_data)
{
	TrackerMinerManager *manager = user_data;
	TrackerMinerManagerPrivate *priv;
	const gchar *name;

	manager = user_data;
	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);
	name = g_hash_table_lookup (priv->miner_proxies, proxy);

	g_signal_emit (manager, signals[MINER_PAUSED], 0, name);
}

static void
miner_resumed (DBusGProxy *proxy,
	       gpointer    user_data)
{
	TrackerMinerManager *manager = user_data;
	TrackerMinerManagerPrivate *priv;
	const gchar *name;

	manager = user_data;
	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);
	name = g_hash_table_lookup (priv->miner_proxies, proxy);

	g_signal_emit (manager, signals[MINER_RESUMED], 0, name);
}

static void
tracker_miner_manager_init (TrackerMinerManager *manager)
{
	TrackerMinerManagerPrivate *priv;
	GError *error = NULL;
	GSList *miners, *m;

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

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

	priv->miner_proxies = g_hash_table_new_full (NULL, NULL,
						     (GDestroyNotify) g_object_unref,
						     (GDestroyNotify) g_free);

	dbus_g_object_register_marshaller (tracker_marshal_VOID__STRING_DOUBLE,
					   G_TYPE_NONE,
					   G_TYPE_STRING,
					   G_TYPE_DOUBLE,
					   G_TYPE_INVALID);

	miners = tracker_miner_manager_get_available (manager);

	for (m = miners; m; m = m->next) {
		DBusGProxy *proxy;
		gchar *name, *path;

		name = strrchr (m->data, '.');
		path = g_strdup_printf (TRACKER_MINER_DBUS_PATH_PREFIX "%s", ++name);

		proxy = dbus_g_proxy_new_for_name (priv->connection,
						   m->data, path,
						   TRACKER_MINER_DBUS_INTERFACE);

		dbus_g_proxy_add_signal (proxy,
					 "Progress",
					 G_TYPE_STRING,
					 G_TYPE_DOUBLE,
					 G_TYPE_INVALID);
		dbus_g_proxy_add_signal (proxy, "Paused", G_TYPE_INVALID);
		dbus_g_proxy_add_signal (proxy, "Resumed", G_TYPE_INVALID);

		dbus_g_proxy_connect_signal (proxy,
					     "Progress",
					     G_CALLBACK (miner_progress_changed),
					     manager, NULL);
		dbus_g_proxy_connect_signal (proxy,
					     "Paused",
					     G_CALLBACK (miner_paused),
					     manager, NULL);
		dbus_g_proxy_connect_signal (proxy,
					     "Resumed",
					     G_CALLBACK (miner_resumed),
					     manager, NULL);

		g_hash_table_insert (priv->miner_proxies, proxy, g_strdup (m->data));
	}
}

static void
miner_manager_finalize (GObject *object)
{
	TrackerMinerManagerPrivate *priv;

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (object);

	if (priv->proxy) {
		g_object_unref (priv->proxy);
	}

	if (priv->connection) {
		dbus_g_connection_unref (priv->connection);
	}

	G_OBJECT_CLASS (tracker_miner_manager_parent_class)->finalize (object);
}

TrackerMinerManager *
tracker_miner_manager_new (void)
{
	return g_object_new (TRACKER_TYPE_MINER_MANAGER, NULL);
}

GSList *
tracker_miner_manager_get_running (TrackerMinerManager *manager)
{
	TrackerMinerManagerPrivate *priv;
	GSList *list = NULL;
	GError *error = NULL;
	gchar **p, **result;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), NULL);

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

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
tracker_miner_manager_get_available (TrackerMinerManager *manager)
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

gboolean
tracker_miner_manager_pause (TrackerMinerManager *manager,
			     const gchar         *miner,
			     const gchar         *reason,
			     guint32             *cookie)
{
	DBusGProxy *proxy;
	const gchar *app_name;
	GError *error = NULL;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (miner != NULL, FALSE);
	g_return_val_if_fail (reason != NULL, FALSE);

	proxy = find_miner_proxy (manager, miner);

	if (!proxy) {
		g_critical ("No DBus proxy found for miner '%s'", miner);
		return FALSE;
	}

	/* Find a reasonable app name */
	app_name = g_get_application_name ();

	if (!app_name) {
		app_name = g_get_prgname ();
	}

	if (!app_name) {
		app_name = "TrackerMinerManager client";
	}

	org_freedesktop_Tracker1_Miner_pause (proxy, app_name, reason, cookie, &error);

	if (error) {
		g_critical ("Could not pause miner '%s': %s", miner, error->message);
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_miner_manager_resume (TrackerMinerManager *manager,
			      const gchar         *miner,
			      guint32              cookie)
{
	DBusGProxy *proxy;
	GError *error = NULL;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (miner != NULL, FALSE);
	proxy = find_miner_proxy (manager, miner);

	if (!proxy) {
		g_critical ("No DBus proxy found for miner '%s'", miner);
		return FALSE;
	}

	org_freedesktop_Tracker1_Miner_resume (proxy, cookie, &error);

	if (error) {
		g_critical ("Could not resume miner '%s': %s", miner, error->message);
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}
