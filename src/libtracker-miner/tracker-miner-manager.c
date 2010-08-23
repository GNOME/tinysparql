/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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
#include "tracker-miner-object.h"
#include "tracker-miner-manager.h"
#include "tracker-marshal.h"
#include "tracker-miner-client.h"
#include "tracker-miner-files-index-client.h"
#include "tracker-miner-dbus.h"

/**
 * SECTION:tracker-miner-manager
 * @short_description: External control and monitoring of miners
 * @include: libtracker-miner/tracker-miner.h
 *
 * #TrackerMinerManager keeps track of available miners, their current
 * progress/status, and also allows basic external control on them, such
 * as pausing or resuming data processing.
 **/

#define TRACKER_MINER_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_MANAGER, TrackerMinerManagerPrivate))

#define DESKTOP_ENTRY_GROUP "Desktop Entry"
#define DBUS_NAME_KEY "DBusName"
#define DBUS_PATH_KEY "DBusPath"
#define DISPLAY_NAME_KEY "Name"
#define DESCRIPTION_KEY "Comment"

typedef struct TrackerMinerManagerPrivate TrackerMinerManagerPrivate;
typedef struct MinerData MinerData;

struct MinerData {
	gchar *dbus_name;
	gchar *dbus_path;
	gchar *display_name;
	gchar *description;
};

struct TrackerMinerManagerPrivate {
	DBusGConnection *connection;
	DBusGProxy *proxy;

	GList *miners;
	GHashTable *miner_proxies;
};

static void miner_manager_finalize (GObject *object);
static void initialize_miners_data (TrackerMinerManager *manager);


G_DEFINE_TYPE (TrackerMinerManager, tracker_miner_manager, G_TYPE_OBJECT)

enum {
	MINER_PROGRESS,
	MINER_PAUSED,
	MINER_RESUMED,
	MINER_ACTIVATED,
	MINER_DEACTIVATED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

static void
tracker_miner_manager_class_init (TrackerMinerManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = miner_manager_finalize;

	/**
	 * TrackerMinerManager::miner-progress
	 * @manager: the #TrackerMinerManager
	 * @miner: miner reference
	 * @status: miner status
	 * @progress: miner progress, from 0 to 1
	 *
	 * The ::miner-progress signal is meant to report status/progress changes
	 * in any tracked miner.
	 **/
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
	/**
	 * TrackerMinerManager::miner-paused
	 * @manager: the #TrackerMinerManager
	 * @miner: miner reference
	 *
	 * The ::miner-paused signal will be emitted whenever a miner
	 * (referenced by @miner) is paused.
	 **/
	signals [MINER_PAUSED] =
		g_signal_new ("miner-paused",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerManagerClass, miner_paused),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__STRING,
		              G_TYPE_NONE, 1,
		              G_TYPE_STRING);
	/**
	 * TrackerMinerManager::miner-resumed
	 * @manager: the #TrackerMinerManager
	 * @miner: miner reference
	 *
	 * The ::miner-resumed signal will be emitted whenever a miner
	 * (referenced by @miner) is resumed.
	 **/
	signals [MINER_RESUMED] =
		g_signal_new ("miner-resumed",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerManagerClass, miner_resumed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__STRING,
		              G_TYPE_NONE, 1,
		              G_TYPE_STRING);
	/**
	 * TrackerMinerManager::miner-activated
	 * @manager: the #TrackerMinerManager
	 * @miner: miner reference
	 *
	 * The ::miner-activated signal will be emitted whenever a miner
	 * (referenced by @miner) is activated (technically, this means
	 * the miner has appeared in the session bus).
	 **/
	signals [MINER_ACTIVATED] =
		g_signal_new ("miner-activated",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerManagerClass, miner_activated),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__STRING,
		              G_TYPE_NONE, 1,
		              G_TYPE_STRING);
	/**
	 * TrackerMinerManager::miner-deactivated
	 * @manager: the #TrackerMinerManager
	 * @miner: miner reference
	 *
	 * The ::miner-deactivated signal will be emitted whenever a miner
	 * (referenced by @miner) is deactivated (technically, this means
	 * the miner has disappeared from the session bus).
	 **/
	signals [MINER_DEACTIVATED] =
		g_signal_new ("miner-deactivated",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerManagerClass, miner_deactivated),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__STRING,
		              G_TYPE_NONE, 1,
		              G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (TrackerMinerManagerPrivate));
}

static DBusGProxy *
find_miner_proxy (TrackerMinerManager *manager,
                  const gchar         *name,
                  gboolean             try_suffix)
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

		if (try_suffix) {
			if (g_str_has_suffix (value, name)) {
				return key;
			}
		}
	}

	return NULL;
}

static void
name_owner_changed_cb (DBusGProxy  *proxy,
                       const gchar *name,
                       const gchar *old_owner,
                       const gchar *new_owner,
                       gpointer     user_data)
{
	TrackerMinerManager *manager = user_data;

	if (find_miner_proxy (manager, name, FALSE) != NULL) {
		if (new_owner && (!old_owner || !*old_owner)) {
			g_signal_emit (manager, signals[MINER_ACTIVATED], 0, name);
		} else if (old_owner && (!new_owner || !*new_owner)) {
			g_signal_emit (manager, signals[MINER_DEACTIVATED], 0, name);
		}
	}
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
	GList *m;

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!priv->connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            error ? error->message : "no error given.");
		g_clear_error (&error);
	}

	priv->proxy = dbus_g_proxy_new_for_name (priv->connection,
	                                         DBUS_SERVICE_DBUS,
	                                         DBUS_PATH_DBUS,
	                                         DBUS_INTERFACE_DBUS);

	if (!priv->proxy) {
		g_critical ("Could not get proxy for D-Bus service");
	}

	priv->miner_proxies = g_hash_table_new_full (NULL, NULL,
	                                             (GDestroyNotify) g_object_unref,
	                                             (GDestroyNotify) g_free);

	dbus_g_object_register_marshaller (tracker_marshal_VOID__STRING_DOUBLE,
	                                   G_TYPE_NONE,
	                                   G_TYPE_STRING,
	                                   G_TYPE_DOUBLE,
	                                   G_TYPE_INVALID);

	dbus_g_proxy_add_signal (priv->proxy,
	                         "NameOwnerChanged",
	                         G_TYPE_STRING,
	                         G_TYPE_STRING,
	                         G_TYPE_STRING,
	                         G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (priv->proxy,
	                             "NameOwnerChanged",
	                             G_CALLBACK (name_owner_changed_cb),
	                             manager, NULL);

	initialize_miners_data (manager);

	for (m = priv->miners; m; m = m->next) {
		DBusGProxy *proxy;
		MinerData *data;

		data = m->data;

		proxy = dbus_g_proxy_new_for_name (priv->connection,
		                                   data->dbus_name,
		                                   data->dbus_path,
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

		g_hash_table_insert (priv->miner_proxies, proxy, g_strdup (data->dbus_name));
	}
}

static void
miner_data_free (MinerData *data)
{
	g_free (data->dbus_path);
	g_free (data->display_name);
	g_slice_free (MinerData, data);
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

	g_list_foreach (priv->miners, (GFunc) miner_data_free, NULL);
	g_list_free (priv->miners);

	G_OBJECT_CLASS (tracker_miner_manager_parent_class)->finalize (object);
}

/**
 * tracker_miner_manager_new:
 *
 * Creates a new #TrackerMinerManager instance.
 *
 * Returns: a #TrackerMinerManager.
 **/
TrackerMinerManager *
tracker_miner_manager_new (void)
{
	return g_object_new (TRACKER_TYPE_MINER_MANAGER, NULL);
}

/**
 * tracker_miner_manager_get_running:
 * @manager: a #trackerMinerManager
 *
 * Returns a list of references for all active miners.
 *
 * Returns: a #GSList of miner references. This list must be freed
 *          through g_slist_free(), and all contained data with g_free().
 **/
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
			if (!g_str_has_prefix (*p, TRACKER_MINER_DBUS_NAME_PREFIX)) {
				continue;
			}

			/* Special case miner-fs which has
			 * additional D-Bus interface.
			 */
			if (strcmp (*p, "org.freedesktop.Tracker1.Miner.Files.Reindex") == 0) {
				continue;
			}

			list = g_slist_prepend (list, g_strdup (*p));
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
	TrackerMinerManager *manager;
	TrackerMinerManagerPrivate *priv;
	GKeyFile *key_file;
	gchar *path, *dbus_path, *dbus_name, *display_name, *description;
	GError *error = NULL;
	MinerData *data;

	manager = user_data;
	path = g_file_get_path (file);
	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	if (!g_str_has_suffix (path, ".desktop")) {
		return FALSE;
	}

	key_file = g_key_file_new ();
	g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, &error);

	if (error) {
		g_warning ("Error parsing miner .desktop file: %s", error->message);
		g_error_free (error);
		g_key_file_free (key_file);

		return FALSE;
	}

	dbus_path = g_key_file_get_string (key_file, DESKTOP_ENTRY_GROUP, DBUS_PATH_KEY, NULL);
	dbus_name = g_key_file_get_string (key_file, DESKTOP_ENTRY_GROUP, DBUS_NAME_KEY, NULL);
	display_name = g_key_file_get_locale_string (key_file, DESKTOP_ENTRY_GROUP, DISPLAY_NAME_KEY, NULL, NULL);

	if (!dbus_path || !dbus_name || !display_name) {
		g_warning ("Essential data (DBusPath, DBusName or Name) are missing in miner .desktop file");
		g_key_file_free (key_file);
		g_free (dbus_path);
		g_free (display_name);
		g_free (dbus_name);

		return FALSE;
	}

	description = g_key_file_get_locale_string (key_file, DESKTOP_ENTRY_GROUP, DESCRIPTION_KEY, NULL, NULL);

	data = g_slice_new0 (MinerData);
	data->dbus_path = dbus_path;
	data->dbus_name = dbus_name;
	data->display_name = display_name;
	data->description = description;

	priv->miners = g_list_prepend (priv->miners, data);

	g_free (path);

	return TRUE;
}

static void
crawler_finished_cb (TrackerCrawler *crawler,
                     gboolean        was_interrupted,
                     gpointer        user_data)
{
	g_main_loop_quit (user_data);
}

static void
initialize_miners_data (TrackerMinerManager *manager)
{
	GMainLoop *main_loop;
	GFile *file;
	TrackerCrawler *crawler;
        const gchar    *miners_dir;
	crawler = tracker_crawler_new ();
	main_loop = g_main_loop_new (NULL, FALSE);

	g_signal_connect (crawler, "check-file",
	                  G_CALLBACK (crawler_check_file_cb),
	                  manager);
	g_signal_connect (crawler, "finished",
	                  G_CALLBACK (crawler_finished_cb),
	                  main_loop);

	/* Go through service files */
        miners_dir = g_getenv ("TRACKER_MINERS_DIR");
        if (G_LIKELY (miners_dir == NULL)) {
                miners_dir = TRACKER_MINERS_DIR ;
        } else {
                g_message ("Crawling miners in '%s' (set in env)", miners_dir);
        }

	file = g_file_new_for_path (miners_dir);
	tracker_crawler_start (crawler, file, TRUE);
	g_object_unref (file);

	g_main_loop_run (main_loop);

	g_object_unref (crawler);
}

/**
 * tracker_miner_manager_get_available:
 * @manager: a #TrackerMinerManager
 *
 * Returns a list of references for all available miners.
 *
 * Returns: a #GSList of miner references. This list must be freed
 *          through g_slist_free(), and all contained data with g_free().
 **/
GSList *
tracker_miner_manager_get_available (TrackerMinerManager *manager)
{
	TrackerMinerManagerPrivate *priv;
	GSList *list = NULL;
	GList *m;

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	for (m = priv->miners; m; m = m->next) {
		MinerData *data = m->data;

		list = g_slist_prepend (list, g_strdup (data->dbus_name));
	}

	return g_slist_reverse (list);
}

/**
 * tracker_miner_manager_pause:
 * @manager: a #TrackerMinerManager.
 * @miner: miner reference
 * @reason: reason to pause
 * @cookie: return location for the pause cookie ID
 *
 * Asks @miner to pause. a miner could be paused by
 * several reasons, and its activity won't be resumed
 * until all pause requests have been resumed.
 *
 * Returns: %TRUE if the miner was paused successfully.
 **/
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

	proxy = find_miner_proxy (manager, miner, TRUE);

	if (!proxy) {
		g_critical ("No D-Bus proxy found for miner '%s'", miner);
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

/**
 * tracker_miner_manager_resume:
 * @manager: a #TrackerMinerManager
 * @miner: miner reference
 * @cookie: pause cookie
 *
 * Tells @miner to resume activity. The miner won't actually resume
 * operations until all pause requests have been resumed.
 *
 * Returns: %TRUE if the miner was successfully resumed.
 **/
gboolean
tracker_miner_manager_resume (TrackerMinerManager *manager,
                              const gchar         *miner,
                              guint32              cookie)
{
	DBusGProxy *proxy;
	GError *error = NULL;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (miner != NULL, FALSE);
	proxy = find_miner_proxy (manager, miner, TRUE);

	if (!proxy) {
		g_critical ("No D-Bus proxy found for miner '%s'", miner);
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

/**
 * tracker_miner_manager_is_active:
 * @manager: a #TrackerMinerManager
 * @miner: miner reference
 *
 * Returns %TRUE if @miner is currently active.
 *
 * Returns: %TRUE if @miner is active.
 **/
gboolean
tracker_miner_manager_is_active (TrackerMinerManager *manager,
                                 const gchar         *miner)
{
	TrackerMinerManagerPrivate *priv;
	GError *error = NULL;
	gboolean active;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (miner != NULL, FALSE);

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	if (!dbus_g_proxy_call (priv->proxy, "NameHasOwner", &error,
	                        G_TYPE_STRING, miner,
	                        G_TYPE_INVALID,
	                        G_TYPE_BOOLEAN, &active,
	                        G_TYPE_INVALID)) {
		g_critical ("Could not check whether miner '%s' is currently active: %s",
		            miner, error ? error->message : "no error given");
		g_error_free (error);
		return FALSE;
	}

	return active;
}

/**
 * tracker_miner_manager_get_status:
 * @manager: a #TrackerMinerManager
 * @miner: miner reference
 * @status: return location for status
 * @progress: return location for progress
 *
 * Returns the current status and progress for @miner.
 *
 * Returns: %TRUE if the status could be retrieved successfully.
 **/
gboolean
tracker_miner_manager_get_status (TrackerMinerManager  *manager,
                                  const gchar          *miner,
                                  gchar               **status,
                                  gdouble              *progress)
{
	DBusGProxy *proxy;
	GError *error = NULL;
	gdouble p;
	gchar *st;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (miner != NULL, FALSE);

	proxy = find_miner_proxy (manager, miner, TRUE);

	if (!proxy) {
		g_critical ("No D-Bus proxy found for miner '%s'", miner);
		return FALSE;
	}

	org_freedesktop_Tracker1_Miner_get_progress (proxy, &p, &error);

	if (error) {
		/* We handle this error as a special case, some
		 * plugins don't have .service files.
		 */
		if (error->code != DBUS_GERROR_SERVICE_UNKNOWN) {
			g_critical ("Could not get miner progress for '%s': %s", miner,
			            error->message);
		}

		g_error_free (error);

		return FALSE;
	}

	org_freedesktop_Tracker1_Miner_get_status (proxy, &st, &error);

	if (error) {
		g_critical ("Could not get miner status for '%s': %s", miner,
		            error->message);
		g_error_free (error);

		return FALSE;
	}

	if (status) {
		*status = st;
	} else {
		g_free (st);
	}

	if (progress) {
		*progress = p;
	}

	return TRUE;
}

/**
 * tracker_miner_manager_is_paused:
 * @manager: a #TrackerMinerManager
 * @miner: miner reference
 * @applications: return location for application names.
 * @reasons: return location for pause reasons.
 *
 * This function either returns %FALSE if the miner is not paused,
 * or returns %TRUE and fills in @applications and @reasons with
 * the pause reasons and the applications that asked for it. Both
 * arrays will have the same lengh, and will be sorted so the
 * application/pause reason pairs have the same index.
 *
 * Returns: %TRUE if @miner is paused.
 **/
gboolean
tracker_miner_manager_is_paused (TrackerMinerManager *manager,
                                 const gchar         *miner,
                                 GStrv               *applications,
                                 GStrv               *reasons)
{
	DBusGProxy *proxy;
	GStrv apps, r;
	GError *error = NULL;
	gboolean paused;

	if (applications) {
		*applications = NULL;
	}

	if (reasons) {
		*reasons = NULL;
	}

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), TRUE);
	g_return_val_if_fail (miner != NULL, TRUE);

	proxy = find_miner_proxy (manager, miner, TRUE);

	if (!proxy) {
		g_critical ("No D-Bus proxy found for miner '%s'", miner);
		return FALSE;
	}

	org_freedesktop_Tracker1_Miner_get_pause_details (proxy, &apps, &r, &error);

	if (error) {
		g_critical ("Could not get pause details for miner '%s': %s", miner,
		            error->message);
		g_error_free (error);

		return TRUE;
	}

	paused = (g_strv_length (apps) > 0);

	if (applications) {
		*applications = apps;
	} else {
		g_strfreev (apps);
	}

	if (reasons) {
		*reasons = r;
	} else  {
		g_strfreev (r);
	}

	return paused;
}

/**
 * tracker_miner_manager_get_display_name:
 * @manager: a #TrackerMinerManager
 * @miner: miner reference
 *
 * Returns a translated display name for @miner.
 *
 * Returns: The miner display name.
 **/
const gchar *
tracker_miner_manager_get_display_name (TrackerMinerManager *manager,
                                        const gchar         *miner)
{
	TrackerMinerManagerPrivate *priv;
	GList *m;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), NULL);
	g_return_val_if_fail (miner != NULL, NULL);

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	for (m = priv->miners; m; m = m->next) {
		MinerData *data = m->data;

		if (strcmp (miner, data->dbus_name) == 0) {
			return data->display_name;
		}
	}

	return NULL;
}

/**
 * tracker_miner_manager_get_description:
 * @manager: a #TrackerMinerManager
 * @miner: miner reference
 *
 * Returns the description for @miner, or %NULL if none is specified.
 *
 * Returns: The miner description.
 **/
const gchar *
tracker_miner_manager_get_description (TrackerMinerManager *manager,
                                       const gchar         *miner)
{
	TrackerMinerManagerPrivate *priv;
	GList *m;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), NULL);
	g_return_val_if_fail (miner != NULL, NULL);

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	for (m = priv->miners; m; m = m->next) {
		MinerData *data = m->data;

		if (strcmp (miner, data->dbus_name) == 0) {
			return data->description;
		}
	}

	return NULL;
}


/**
 * tracker_miner_manager_ignore_next_update:
 * @manager: a #TrackerMinerManager.
 * @miner: miner reference
 * @urls: subjects to mark as writeback
 *
 * Asks @miner to mark @subjects as writeback
 *
 * Returns: %TRUE if the miner was asked to ignore on next update successfully.
 **/
gboolean
tracker_miner_manager_ignore_next_update (TrackerMinerManager *manager,
                                          const gchar         *miner,
                                          const gchar        **urls)
{
	DBusGProxy *proxy;
	const gchar *app_name;
	GError *error = NULL;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (miner != NULL, FALSE);

	proxy = find_miner_proxy (manager, miner, TRUE);

	if (!proxy) {
		g_warning ("No D-Bus proxy found for miner '%s'", miner);
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

	org_freedesktop_Tracker1_Miner_ignore_next_update (proxy, urls, &error);

	if (error) {
		g_warning ("Could not ignore next update for miner '%s': %s", miner, error->message);
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_miner_manager_index_file (TrackerMinerManager  *manager,
                                  GFile                *file,
                                  GError              **error)
{
	static DBusGProxy *proxy = NULL;
	TrackerMinerManagerPrivate *priv;
	GError *internal_error = NULL;
	gchar *uri;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	if (G_UNLIKELY (!proxy)) {
		proxy = dbus_g_proxy_new_for_name (priv->connection,
		                                   "org.freedesktop.Tracker1.Miner.Files.Index",
		                                   "/org/freedesktop/Tracker1/Miner/Files/Index",
		                                   "org.freedesktop.Tracker1.Miner.Files.Index");
	}

	uri = g_file_get_uri (file);
	org_freedesktop_Tracker1_Miner_Files_Index_index_file (proxy, uri, &internal_error);
	g_free (uri);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	return TRUE;
}
