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

#include <gio/gio.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-type-utils.h>

#include "tracker-crawler.h"
#include "tracker-miner-object.h"
#include "tracker-miner-manager.h"
#include "tracker-marshal.h"
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

	GDBusConnection *connection;
	guint progress_signal;
	guint paused_signal;
	guint resumed_signal;
	guint watch_name_id;
	GObject *manager; /* weak */
};

struct TrackerMinerManagerPrivate {
	GDBusConnection *connection;
	GList *miners;
	GHashTable *miner_proxies;

	/* Property values */
	gboolean auto_start;
};

static void miner_manager_initable_iface_init (GInitableIface         *iface);
static void miner_manager_set_property        (GObject             *object,
                                               guint                param_id,
                                               const GValue        *value,
                                               GParamSpec          *pspec);
static void miner_manager_get_property        (GObject             *object,
                                               guint                param_id,
                                               GValue              *value,
                                               GParamSpec          *pspec);
static void miner_manager_finalize            (GObject             *object);
static void initialize_miners_data            (TrackerMinerManager *manager);

G_DEFINE_TYPE_WITH_CODE (TrackerMinerManager, tracker_miner_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                miner_manager_initable_iface_init));

enum {
	PROP_0,
	PROP_AUTO_START
};

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

	object_class->set_property = miner_manager_set_property;
	object_class->get_property = miner_manager_get_property;
	object_class->finalize = miner_manager_finalize;

	g_object_class_install_property (object_class,
	                                 PROP_AUTO_START,
	                                 g_param_spec_boolean ("auto-start",
	                                                      "Auto Start",
	                                                      "If set, auto starts miners when querying their status",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * TrackerMinerManager::miner-progress
	 * @manager: the #TrackerMinerManager
	 * @miner: miner reference
	 * @status: miner status
	 * @progress: miner progress, from 0 to 1
	 *
	 * The ::miner-progress signal is meant to report status/progress changes
	 * in any tracked miner.
	 *
	 * Since: 0.8
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
	 *
	 * Since: 0.8
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
	 *
	 * Since: 0.8
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
	 *
	 * Since: 0.8
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
	 *
	 * Since: 0.8
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

static void
miner_manager_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
	TrackerMinerManager *manager;
	TrackerMinerManagerPrivate *priv;

	manager = TRACKER_MINER_MANAGER (object);
	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	switch (prop_id) {
	case PROP_AUTO_START:
		priv->auto_start = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
miner_manager_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
	TrackerMinerManager *manager;
	TrackerMinerManagerPrivate *priv;

	manager = TRACKER_MINER_MANAGER (object);
	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	switch (prop_id) {
	case PROP_AUTO_START:
		g_value_set_boolean (value, priv->auto_start);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static GDBusProxy *
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
miner_appears (GDBusConnection *connection,
               const gchar     *name,
               const gchar     *name_owner,
               gpointer         user_data)
{
	MinerData *data = user_data;
	if (data->manager) {
		g_signal_emit (data->manager, signals[MINER_ACTIVATED], 0, data->dbus_name);
	}
}

static void
miner_disappears (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
	MinerData *data = user_data;
	if (data->manager) {
		g_signal_emit (data->manager, signals[MINER_DEACTIVATED], 0, data->dbus_name);
	}
}

static void
miner_progress_changed (GDBusConnection *connection,
                        const gchar     *sender_name,
                        const gchar     *object_path,
                        const gchar     *interface_name,
                        const gchar     *signal_name,
                        GVariant        *parameters,
                        gpointer         user_data)
{
	MinerData *data = user_data;
	const gchar *status = NULL;
	gdouble progress = 0;

	g_variant_get (parameters, "(&sd)", &status, &progress);
	if (data->manager) {
		g_signal_emit (data->manager, signals[MINER_PROGRESS], 0, data->dbus_name, status, progress);
	}
}

static void
miner_paused (GDBusConnection *connection,
              const gchar     *sender_name,
              const gchar     *object_path,
              const gchar     *interface_name,
              const gchar     *signal_name,
              GVariant        *parameters,
              gpointer         user_data)
{
	MinerData *data = user_data;
	if (data->manager) {
		g_signal_emit (data->manager, signals[MINER_PAUSED], 0, data->dbus_name);
	}
}

static void
miner_resumed (GDBusConnection *connection,
               const gchar     *sender_name,
               const gchar     *object_path,
               const gchar     *interface_name,
               const gchar     *signal_name,
               GVariant        *parameters,
               gpointer         user_data)
{
	MinerData *data = user_data;
	if (data->manager) {
		g_signal_emit (data->manager, signals[MINER_RESUMED], 0, data->dbus_name);
	}
}

static void
data_manager_weak_notify (gpointer user_data, GObject *old_object)
{
	MinerData *data = user_data;
	data->manager = NULL;
}

static void
tracker_miner_manager_init (TrackerMinerManager *manager)
{
	TrackerMinerManagerPrivate *priv;

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	priv->miner_proxies = g_hash_table_new_full (NULL, NULL,
	                                             (GDestroyNotify) g_object_unref,
	                                             (GDestroyNotify) g_free);
}

static gboolean
miner_manager_initable_init (GInitable     *initable,
                             GCancellable  *cancellable,
                             GError       **error)
{
	TrackerMinerManager *manager;
	GError *inner_error = NULL;
	TrackerMinerManagerPrivate *priv;
	GList *m;

	manager = TRACKER_MINER_MANAGER (initable);
	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &inner_error);
	if (!priv->connection) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	initialize_miners_data (manager);

	for (m = priv->miners; m; m = m->next) {
		GDBusProxy *proxy;
		MinerData *data;

		data = m->data;
		data->connection = g_object_ref (priv->connection);
		data->manager = G_OBJECT (manager);
		g_object_weak_ref (data->manager, data_manager_weak_notify, data);

		proxy = g_dbus_proxy_new_sync (priv->connection,
		                               (priv->auto_start ?
		                                G_DBUS_PROXY_FLAGS_NONE :
		                                G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START),
		                               NULL,
		                               data->dbus_name,
		                               data->dbus_path,
		                               TRACKER_MINER_DBUS_INTERFACE,
		                               NULL,
		                               &inner_error);
		/* This error shouldn't be considered fatal */
		if (inner_error) {
			g_critical ("Could not create proxy on the D-Bus session bus, %s",
			            inner_error ? inner_error->message : "no error given.");
			g_clear_error (&inner_error);
			continue;
		}

		data->progress_signal = g_dbus_connection_signal_subscribe (priv->connection,
		                                                            data->dbus_name,
		                                                            TRACKER_MINER_DBUS_INTERFACE,
		                                                            "Progress",
		                                                            data->dbus_path,
		                                                            NULL,
		                                                            G_DBUS_SIGNAL_FLAGS_NONE,
		                                                            miner_progress_changed,
		                                                            data,
		                                                            NULL);

		data->paused_signal = g_dbus_connection_signal_subscribe (priv->connection,
		                                                          data->dbus_name,
		                                                          TRACKER_MINER_DBUS_INTERFACE,
		                                                          "Paused",
		                                                          data->dbus_path,
		                                                          NULL,
		                                                          G_DBUS_SIGNAL_FLAGS_NONE,
		                                                          miner_paused,
		                                                          data,
		                                                          NULL);


		data->resumed_signal = g_dbus_connection_signal_subscribe (priv->connection,
		                                                           data->dbus_name,
		                                                           TRACKER_MINER_DBUS_INTERFACE,
		                                                           "Resumed",
		                                                           data->dbus_path,
		                                                           NULL,
		                                                           G_DBUS_SIGNAL_FLAGS_NONE,
		                                                           miner_resumed,
		                                                           data,
		                                                           NULL);

		g_hash_table_insert (priv->miner_proxies, proxy, g_strdup (data->dbus_name));

		data->watch_name_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
		                                        data->dbus_name,
		                                        G_BUS_NAME_WATCHER_FLAGS_NONE,
		                                        miner_appears,
		                                        miner_disappears,
		                                        data,
		                                        NULL);

	}

	return TRUE;
}

static void
miner_manager_initable_iface_init (GInitableIface *iface)
{
	iface->init = miner_manager_initable_init;
}


static void
miner_data_free (MinerData *data)
{
	if (data->watch_name_id != 0) {
		g_bus_unwatch_name (data->watch_name_id);
	}

	if (data->progress_signal) {
		g_dbus_connection_signal_unsubscribe (data->connection,
		                                      data->progress_signal);
	}

	if (data->paused_signal) {
		g_dbus_connection_signal_unsubscribe (data->connection,
		                                      data->paused_signal);
	}

	if (data->resumed_signal) {
		g_dbus_connection_signal_unsubscribe (data->connection,
		                                      data->resumed_signal);
	}

	if (data->connection) {
		g_object_unref (data->connection);
	}

	if (data->manager) {
		g_object_weak_unref (data->manager, data_manager_weak_notify, data);
	}

	g_free (data->dbus_path);
	g_free (data->display_name);
	g_slice_free (MinerData, data);
}

static void
miner_manager_finalize (GObject *object)
{
	TrackerMinerManagerPrivate *priv;

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (object);

	if (priv->connection) {
		g_object_unref (priv->connection);
	}

	g_list_foreach (priv->miners, (GFunc) miner_data_free, NULL);
	g_list_free (priv->miners);
	g_hash_table_unref (priv->miner_proxies);

	G_OBJECT_CLASS (tracker_miner_manager_parent_class)->finalize (object);
}

/**
 * tracker_miner_manager_new:
 *
 * Creates a new #TrackerMinerManager instance.
 *
 * Note: Auto-starting miners when querying status will be enabled.
 *
 * Returns: a #TrackerMinerManager or #NULL if an error happened.
 *
 * Since: 0.8
 **/
TrackerMinerManager *
tracker_miner_manager_new (void)
{
	GError *inner_error = NULL;
	TrackerMinerManager *manager;

	manager = g_initable_new (TRACKER_TYPE_MINER_MANAGER,
	                          NULL,
	                          &inner_error,
	                          NULL);
	if (!manager) {
		g_critical ("Couldn't create new TrackerMinerManager: '%s'",
		            inner_error ? inner_error->message : "unknown error");
		g_clear_error (&inner_error);
	}

	return manager;
}

/**
 * tracker_miner_manager_new_full:
 * @auto_start: Flag to disable auto-starting the miners when querying status
 * @error: a #GError to report errors.
 *
 * Creates a new #TrackerMinerManager.
 *
 * Returns: a #TrackerMinerManager. On error, #NULL is returned and @error is set
 * accordingly.
 *
 * Since: 0.10.5
 **/
TrackerMinerManager *
tracker_miner_manager_new_full (gboolean   auto_start,
                                GError   **error)
{
	GError *inner_error = NULL;
	TrackerMinerManager *manager;

	manager = g_initable_new (TRACKER_TYPE_MINER_MANAGER,
	                          NULL,
	                          &inner_error,
	                          "auto-start", auto_start,
	                          NULL);
	if (!manager) {
		g_critical ("Couldn't create new TrackerMinerManager: '%s'",
		            inner_error ? inner_error->message : "unknown error");
		g_clear_error (&inner_error);
	}

	return manager;
}

/**
 * tracker_miner_manager_get_running:
 * @manager: a #trackerMinerManager
 *
 * Returns a list of references for all active miners. Active miners
 * are miners which are running within a process.
 *
 * Returns: (transfer full): a #GSList which must be freed with g_slist_free() and all
 * contained data with g_free(). Otherwise %NULL is returned if there
 * are no miners.
 *
 * Since: 0.8
 **/
GSList *
tracker_miner_manager_get_running (TrackerMinerManager *manager)
{
	TrackerMinerManagerPrivate *priv;
	GSList *list = NULL;
	GError *error = NULL;
	GVariant *v;
	GVariantIter *iter;
	const gchar *str = NULL;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), NULL);

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	if (!priv->connection) {
		return NULL;
	}

	v = g_dbus_connection_call_sync (priv->connection,
	                                 "org.freedesktop.DBus",
	                                 "/org/freedesktop/DBus",
	                                 "org.freedesktop.DBus",
	                                 "ListNames",
	                                 NULL,
	                                 G_VARIANT_TYPE ("(as)"),
	                                 G_DBUS_CALL_FLAGS_NONE,
	                                 -1,
	                                 NULL,
	                                 &error);

	if (error) {
		g_critical ("Could not get a list of names registered on the session bus, %s",
		            error ? error->message : "no error given");
		g_clear_error (&error);
		return NULL;
	}

	g_variant_get (v, "(as)", &iter);
	while (g_variant_iter_loop (iter, "&s", &str)) {
		if (!g_str_has_prefix (str, TRACKER_MINER_DBUS_NAME_PREFIX)) {
			continue;
		}

		/* Special case miner-fs which has
		 * additional D-Bus interface.
		 */
		if (strcmp (str, "org.freedesktop.Tracker1.Miner.Files.Index") == 0) {
			continue;
		}

		list = g_slist_prepend (list, g_strdup (str));
	}

	g_variant_iter_free (iter);
	g_variant_unref (v);

	list = g_slist_reverse (list);

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
	const gchar *miners_dir;

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
		miners_dir = TRACKER_MINERS_DIR;
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
 * Returns a list of references for all available miners. Available
 * miners are miners which may or may not be running in a process at
 * the current time.
 *
 * Returns: (transfer full): a #GSList which must be freed with g_slist_free() and all
 * contained data with g_free(). Otherwise %NULL is returned if there
 * are no miners.
 *
 * Since: 0.8
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
 * Returns: %TRUE if the miner was paused successfully, otherwise
 * %FALSE.
 *
 * Since: 0.8
 **/
gboolean
tracker_miner_manager_pause (TrackerMinerManager *manager,
                             const gchar         *miner,
                             const gchar         *reason,
                             guint32             *cookie)
{
	GDBusProxy *proxy;
	const gchar *app_name;
	GError *error = NULL;
	GVariant *v;

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

	v = g_dbus_proxy_call_sync (proxy,
	                            "Pause",
	                            g_variant_new ("(ss)", app_name, reason),
	                            G_DBUS_CALL_FLAGS_NONE,
	                            -1,
	                            NULL,
	                            &error);

	if (error) {
		g_critical ("Could not pause miner '%s': %s", miner, error->message);
		g_error_free (error);
		return FALSE;
	}

	if (cookie) {
		g_variant_get (v, "(i)", cookie);
	}

	g_variant_unref (v);

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
 * Returns: %TRUE if the miner was successfully resumed, otherwise
 * %FALSE.
 *
 * Since: 0.8
 **/
gboolean
tracker_miner_manager_resume (TrackerMinerManager *manager,
                              const gchar         *miner,
                              guint32              cookie)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *v;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (miner != NULL, FALSE);
	proxy = find_miner_proxy (manager, miner, TRUE);

	if (!proxy) {
		g_critical ("No D-Bus proxy found for miner '%s'", miner);
		return FALSE;
	}

	v = g_dbus_proxy_call_sync (proxy,
	                            "Resume",
	                            g_variant_new ("(i)", (gint) cookie),
	                            G_DBUS_CALL_FLAGS_NONE,
	                            -1,
	                            NULL,
	                            &error);

	if (error) {
		g_critical ("Could not resume miner '%s': %s", miner, error->message);
		g_error_free (error);
		return FALSE;
	}

	g_variant_unref (v);

	return TRUE;
}

/**
 * tracker_miner_manager_is_active:
 * @manager: a #TrackerMinerManager
 * @miner: miner reference
 *
 * Returns the miner's current activity.
 *
 * Returns: %TRUE if the @miner is active, otherwise %FALSE.
 *
 * Since: 0.8
 **/
gboolean
tracker_miner_manager_is_active (TrackerMinerManager *manager,
                                 const gchar         *miner)
{
	TrackerMinerManagerPrivate *priv;
	GError *error = NULL;
	gboolean active = FALSE;
	GVariant *v;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (miner != NULL, FALSE);

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	v = g_dbus_connection_call_sync (priv->connection,
	                                 "org.freedesktop.DBus",
	                                 "/org/freedesktop/DBus",
	                                 "org.freedesktop.DBus",
	                                 "NameHasOwner",
	                                 g_variant_new ("(s)", miner),
	                                 (GVariantType *) "(b)",
	                                 G_DBUS_CALL_FLAGS_NONE,
	                                 -1,
	                                 NULL,
	                                 &error);

	if (error) {
		g_critical ("Could not check whether miner '%s' is currently active: %s",
		            miner, error ? error->message : "no error given");
		g_error_free (error);
		return FALSE;
	}

	g_variant_get (v, "(b)", &active);
	g_variant_unref (v);

	return active;
}

/**
 * tracker_miner_manager_get_status:
 * @manager: a #TrackerMinerManager
 * @miner: miner reference
 * @status: (out) (allow-none): return location for status
 * @progress: (out) (allow-none): return location for progress
 *
 * Returns the current status and progress for @miner.
 *
 * Returns: %TRUE if the status could be retrieved successfully,
 * otherwise %FALSE
 *
 * Since: 0.8
 **/
gboolean
tracker_miner_manager_get_status (TrackerMinerManager  *manager,
                                  const gchar          *miner,
                                  gchar               **status,
                                  gdouble              *progress)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	gdouble p;
	GVariant *v;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (miner != NULL, FALSE);

	proxy = find_miner_proxy (manager, miner, TRUE);

	if (!proxy) {
		g_critical ("No D-Bus proxy found for miner '%s'", miner);
		return FALSE;
	}

	v = g_dbus_proxy_call_sync (proxy,
	                            "GetProgress",
	                            NULL,
	                            G_DBUS_CALL_FLAGS_NONE,
	                            -1,
	                            NULL,
	                            &error);

	if (error) {
		/* We handle this error as a special case, some
		 * plugins don't have .service files.
		 */
		if (error->code != G_DBUS_ERROR_SERVICE_UNKNOWN) {
			g_critical ("Could not get miner progress for '%s': %s", miner,
			            error->message);
		}

		g_error_free (error);

		return FALSE;
	}

	g_variant_get (v, "(d)", &p);
	g_variant_unref (v);

	v = g_dbus_proxy_call_sync (proxy,
	                            "GetStatus",
	                            NULL,
	                            G_DBUS_CALL_FLAGS_NONE,
	                            -1,
	                            NULL,
	                            &error);

	if (error) {
		g_critical ("Could not get miner status for '%s': %s", miner,
		            error->message);
		g_error_free (error);
		return FALSE;
	}

	if (status) {
		g_variant_get (v, "(s)", status);
	}

	g_variant_unref (v);

	if (progress) {
		*progress = p;
	}

	return TRUE;
}

/**
 * tracker_miner_manager_is_paused:
 * @manager: a #TrackerMinerManager
 * @miner: miner reference
 * @applications: (out callee-allocates) (allow-none) (transfer full): return location for application names.
 * @reasons: (out callee-allocates) (allow-none) (transfer full): return location for pause reasons.
 *
 * This function either returns %FALSE if the miner is not paused,
 * or returns %TRUE and fills in @applications and @reasons with
 * the pause reasons and the applications that asked for it. Both
 * arrays will have the same lengh, and will be sorted so the
 * application/pause reason pairs have the same index.
 *
 * Returns: %TRUE if @miner is paused, otherwise %FALSE.
 *
 * Since: 0.8
 **/
gboolean
tracker_miner_manager_is_paused (TrackerMinerManager *manager,
                                 const gchar         *miner,
                                 GStrv               *applications,
                                 GStrv               *reasons)
{
	GDBusProxy *proxy;
	GStrv apps, r;
	GError *error = NULL;
	gboolean paused;
	GVariant *v;

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

	v = g_dbus_proxy_call_sync (proxy,
	                            "GetPauseDetails",
	                            NULL,
	                            G_DBUS_CALL_FLAGS_NONE,
	                            -1,
	                            NULL,
	                            &error);

	if (error) {
		g_critical ("Could not get pause details for miner '%s': %s", miner,
		            error->message);
		g_error_free (error);
		return FALSE;
	}

	g_variant_get (v, "(^as^as)", &apps, &r);
	g_variant_unref (v);

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
 * Returns: (transfer none): A string which should not be freed or %NULL.
 *
 * Since: 0.8
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
 * Returns the description for the given @miner.
 *
 * Returns: (transfer none): A string which should not be freed or %NULL if none is specified.
 *
 * Since: 0.8
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
 * @manager: a #TrackerMinerManager
 * @miner: miner reference
 * @urls: (in): the subjects to ignore the next updates of
 *
 * Tells the @miner to ignore any events for the next @urls. This is
 * used for cases where a file is updated by Tracker by the
 * tracker-writeback service. This API is used to avoid signalling up
 * the stack the changes to @urls.
 *
 * Returns: %TRUE on success, otherwise %FALSE.
 *
 * Since: 0.8
 * Deprecated since 0.12
 **/
gboolean
tracker_miner_manager_ignore_next_update (TrackerMinerManager *manager,
                                          const gchar         *miner,
                                          const gchar        **urls)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *v;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (miner != NULL, FALSE);

	proxy = find_miner_proxy (manager, miner, TRUE);

	if (!proxy) {
		g_warning ("No D-Bus proxy found for miner '%s'", miner);
		return FALSE;
	}

	v = g_dbus_proxy_call_sync (proxy,
	                            "IgnoreNextUpdate",
	                            g_variant_new ("(^as)", urls),
	                            G_DBUS_CALL_FLAGS_NONE,
	                            -1,
	                            NULL,
	                            &error);

	if (error) {
		g_warning ("Could not ignore next update for miner '%s': %s", miner, error->message);
		g_error_free (error);
		return FALSE;
	}

	g_variant_unref (v);

	return TRUE;
}

/**
 * tracker_miner_manager_error_quark (skip):
 *
 * Returns: the #GQuark used to identify miner manager errors in
 * GError structures.
 *
 * Since: 0.8
 **/
GQuark
tracker_miner_manager_error_quark (void)
{
	static GQuark error_quark = 0;

	if (G_UNLIKELY (error_quark == 0)) {
		error_quark = g_quark_from_static_string ("tracker-miner-manager-error-quark");
	}

	return error_quark;
}

/**
 * tracker_miner_manager_reindex_by_mimetype:
 * @manager: a #TrackerMinerManager
 * @mimetypes: (in): an array of mimetypes (E.G. "text/plain"). All items
 * with a mimetype in that list will be reindexed.
 * @error: (out callee-allocates) (transfer full) (allow-none): return location for errors
 *
 * Tells the filesystem miner to reindex any file with a mimetype in
 * the @mimetypes list.
 *
 * On failure @error will be set.
 *
 * Returns: %TRUE on success, otherwise %FALSE.
 *
 * Since: 0.10
 **/
gboolean
tracker_miner_manager_reindex_by_mimetype (TrackerMinerManager  *manager,
                                           const GStrv           mimetypes,
                                           GError              **error)
{
	TrackerMinerManagerPrivate *priv;
	GVariant *v;
	GError *new_error = NULL;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (mimetypes != NULL, FALSE);

	if (!tracker_miner_manager_is_active (manager,
	                                      "org.freedesktop.Tracker1.Miner.Files")) {
		g_set_error_literal (error,
		                     TRACKER_MINER_MANAGER_ERROR,
		                     TRACKER_MINER_MANAGER_ERROR_NOT_AVAILABLE,
		                     "Filesystem miner is not active");
		return FALSE;
	}

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	v = g_dbus_connection_call_sync (priv->connection,
	                                 "org.freedesktop.Tracker1.Miner.Files.Index",
	                                 "/org/freedesktop/Tracker1/Miner/Files/Index",
	                                 "org.freedesktop.Tracker1.Miner.Files.Index",
	                                 "ReindexMimeTypes",
	                                 g_variant_new ("(^as)", mimetypes),
	                                 NULL,
	                                 G_DBUS_CALL_FLAGS_NONE,
	                                 -1,
	                                 NULL,
	                                 &new_error);

	if (new_error) {
		g_propagate_error (error, new_error);
		return FALSE;
	}

	g_variant_unref (v);

	return FALSE;
}

/**
 * tracker_miner_manager_index_file:
 * @manager: a #TrackerMinerManager
 * @file: a URL valid in GIO of a file to give to the miner for processing
 * @error: (out callee-allocates) (transfer full) (allow-none): return location for errors
 *
 * Tells the filesystem miner to index the @file.
 *
 * On failure @error will be set.
 *
 * Returns: %TRUE on success, otherwise %FALSE.
 *
 * Since: 0.10
 **/
gboolean
tracker_miner_manager_index_file (TrackerMinerManager  *manager,
                                  GFile                *file,
                                  GError              **error)
{
	TrackerMinerManagerPrivate *priv;
	gchar *uri;
	GVariant *v;
	GError *new_error = NULL;

	g_return_val_if_fail (TRACKER_IS_MINER_MANAGER (manager), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	if (!g_file_query_exists (file, NULL)) {
		g_set_error_literal (error,
		                     TRACKER_MINER_MANAGER_ERROR,
		                     TRACKER_MINER_MANAGER_ERROR_NOENT,
		                     "File or directory does not exist");
		return FALSE;
	}

	if (!tracker_miner_manager_is_active (manager,
	                                      "org.freedesktop.Tracker1.Miner.Files")) {
		g_set_error_literal (error,
		                     TRACKER_MINER_MANAGER_ERROR,
		                     TRACKER_MINER_MANAGER_ERROR_NOT_AVAILABLE,
		                     "Filesystem miner is not active");
		return FALSE;
	}

	priv = TRACKER_MINER_MANAGER_GET_PRIVATE (manager);

	uri = g_file_get_uri (file);

	v = g_dbus_connection_call_sync (priv->connection,
	                                 "org.freedesktop.Tracker1.Miner.Files.Index",
	                                 "/org/freedesktop/Tracker1/Miner/Files/Index",
	                                 "org.freedesktop.Tracker1.Miner.Files.Index",
	                                 "IndexFile",
	                                 g_variant_new ("(s)", uri),
	                                 NULL,
	                                 G_DBUS_CALL_FLAGS_NONE,
	                                 -1,
	                                 NULL,
	                                 &new_error);

	g_free (uri);

	if (new_error) {
		g_propagate_error (error, new_error);
		return FALSE;
	}

	g_variant_unref (v);

	return FALSE;
}
