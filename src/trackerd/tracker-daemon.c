/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-dbus.h>

#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-index.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-dbus.h"
#include "tracker-daemon.h"
#include <libtracker-data/tracker-data-manager.h>
#include "tracker-indexer-client.h"
#include "tracker-main.h"
#include "tracker-status.h"
#include "tracker-marshal.h"

#define TRACKER_DAEMON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_DAEMON, TrackerDaemonPrivate))

#define TRACKER_TYPE_G_STRV_ARRAY  (dbus_g_type_get_collection ("GPtrArray", G_TYPE_STRV))

/* In seconds, if this is < 1, it is disabled */
#define STATS_CACHE_LIFETIME 120

typedef struct {
	TrackerConfig	 *config;
	TrackerProcessor *processor;
	DBusGProxy	 *indexer_proxy;

	GHashTable       *stats_cache;
	guint             stats_cache_timeout_id;
} TrackerDaemonPrivate;

enum {
	INDEX_STATE_CHANGE,
	INDEX_FINISHED,
	INDEX_PROGRESS,
	INDEXING_ERROR,
	SERVICE_STATISTICS_UPDATED,
	LAST_SIGNAL
};

static void        tracker_daemon_finalize (GObject       *object);
static gboolean    stats_cache_timeout     (gpointer       user_data);
static GHashTable *stats_cache_get_latest  (void);

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE(TrackerDaemon, tracker_daemon, G_TYPE_OBJECT)

static void
tracker_daemon_class_init (TrackerDaemonClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_daemon_finalize;

	signals[INDEX_STATE_CHANGE] =
		g_signal_new ("index-state-change",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN,
			      G_TYPE_NONE,
			      7,
			      G_TYPE_STRING,
			      G_TYPE_BOOLEAN,
			      G_TYPE_BOOLEAN,
			      G_TYPE_BOOLEAN,
			      G_TYPE_BOOLEAN,
			      G_TYPE_BOOLEAN,
			      G_TYPE_BOOLEAN);
	signals[INDEX_FINISHED] =
		g_signal_new ("index-finished",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__DOUBLE,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_DOUBLE);
	signals[INDEX_PROGRESS] =
		g_signal_new ("index-progress",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_STRING_INT_INT_INT_DOUBLE,
			      G_TYPE_NONE,
			      6,
			      G_TYPE_STRING,
			      G_TYPE_STRING,
			      G_TYPE_INT,
			      G_TYPE_INT,
			      G_TYPE_INT,
			      G_TYPE_DOUBLE);
	signals[INDEXING_ERROR] =
		g_signal_new ("indexing-error",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL,
			      tracker_marshal_VOID__STRING_BOOLEAN,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_BOOLEAN);
	signals[SERVICE_STATISTICS_UPDATED] =
		g_signal_new ("service-statistics-updated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      TRACKER_TYPE_G_STRV_ARRAY);

	g_type_class_add_private (object_class, sizeof (TrackerDaemonPrivate));
}

static void
indexer_started_cb (DBusGProxy *proxy,
		    gpointer    user_data)
{
	TrackerDaemonPrivate *priv;

	priv = TRACKER_DAEMON_GET_PRIVATE (user_data);

	/* Make sure we have the cache timeout set up */
	if (priv->stats_cache_timeout_id != 0) {
		return;
	}

	if (STATS_CACHE_LIFETIME > 0) {
		g_debug ("Starting statistics cache timeout");
		priv->stats_cache_timeout_id = 
			g_timeout_add_seconds (STATS_CACHE_LIFETIME,
					       stats_cache_timeout,
					       user_data);
	}
}

static void
indexer_finished_cb (DBusGProxy *proxy,
		     gdouble	 seconds_elapsed,
		     guint	 items_processed,
		     guint	 items_done,
		     gboolean	 interrupted,
		     gpointer	 user_data)
{
	TrackerDaemonPrivate *priv;

	tracker_daemon_signal_statistics ();

	priv = TRACKER_DAEMON_GET_PRIVATE (user_data);

	if (priv->stats_cache_timeout_id == 0) {
		return;
	}

	g_debug ("Stopping statistics cache timeout");
	g_source_remove (priv->stats_cache_timeout_id);
	priv->stats_cache_timeout_id = 0;
}

static void
indexing_error_cb (DBusGProxy    *proxy,
		   const gchar   *reason,
		   gboolean       requires_reindex,
		   TrackerDaemon *daemon)
{
	g_signal_emit (daemon, signals[INDEXING_ERROR], 0,
		       reason, requires_reindex);
}

static void
tracker_daemon_init (TrackerDaemon *object)
{
	TrackerDaemonPrivate *priv;
	DBusGProxy           *proxy;

	priv = TRACKER_DAEMON_GET_PRIVATE (object);

	proxy = tracker_dbus_indexer_get_proxy ();
	priv->indexer_proxy = g_object_ref (proxy);

	dbus_g_proxy_connect_signal (proxy, "Started",
				     G_CALLBACK (indexer_started_cb),
				     object,
				     NULL);
	dbus_g_proxy_connect_signal (proxy, "Finished",
				     G_CALLBACK (indexer_finished_cb),
				     object,
				     NULL);
	dbus_g_proxy_connect_signal (proxy, "IndexingError",
				     G_CALLBACK (indexing_error_cb),
				     object,
				     NULL);

	priv->stats_cache = g_hash_table_new_full (g_str_hash,
						   g_str_equal,
						   g_free,
						   NULL);

	if (STATS_CACHE_LIFETIME > 0) {
		priv->stats_cache_timeout_id = 
			g_timeout_add_seconds (STATS_CACHE_LIFETIME,
					       stats_cache_timeout,
					       object);
	}
}

static void
tracker_daemon_finalize (GObject *object)
{
	TrackerDaemon	     *daemon;
	TrackerDaemonPrivate *priv;

	daemon = TRACKER_DAEMON (object);
	priv = TRACKER_DAEMON_GET_PRIVATE (daemon);

	if (priv->stats_cache_timeout_id != 0) {
		g_source_remove (priv->stats_cache_timeout_id);
	}

	dbus_g_proxy_disconnect_signal (priv->indexer_proxy, "Started",
					G_CALLBACK (indexer_started_cb),
					daemon);
	dbus_g_proxy_disconnect_signal (priv->indexer_proxy, "Finished",
					G_CALLBACK (indexer_finished_cb),
					daemon);
	dbus_g_proxy_disconnect_signal (priv->indexer_proxy, "IndexingError",
					G_CALLBACK (indexing_error_cb),
					daemon);

	if (priv->stats_cache) {
		g_hash_table_unref (priv->stats_cache);
	}

	g_object_unref (priv->indexer_proxy);

	g_object_unref (priv->processor);
	g_object_unref (priv->config);

	G_OBJECT_CLASS (tracker_daemon_parent_class)->finalize (object);
}

TrackerDaemon *
tracker_daemon_new (TrackerConfig    *config,
		    TrackerProcessor *processor)
{
	TrackerDaemon	     *object;
	TrackerDaemonPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), NULL);

	object = g_object_new (TRACKER_TYPE_DAEMON, NULL);

	priv = TRACKER_DAEMON_GET_PRIVATE (object);

	priv->config = g_object_ref (config);
	priv->processor = g_object_ref (processor);

	return object;
}

/*
 * Functions
 */
void
tracker_daemon_get_version (TrackerDaemon	   *object,
			    DBusGMethodInvocation  *context,
			    GError		  **error)
{
	guint  request_id;
	gint   major = 0;
	gint   minor = 0;
	gint   revision = 0;
	gint   version;
	gchar *str;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
				  "DBus request to get daemon version");


	sscanf (PACKAGE_VERSION, "%d.%d.%d", &major, &minor, &revision);
	str = g_strdup_printf ("%d%d%d", major, minor, revision);
	version = atoi (str);
	g_free (str);

	dbus_g_method_return (context, version);

	tracker_dbus_request_success (request_id);
}

void
tracker_daemon_get_status (TrackerDaemon	  *object,
			   DBusGMethodInvocation  *context,
			   GError		 **error)
{
	guint  request_id;
	gchar *status;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_block_hooks ();
	tracker_dbus_request_new (request_id,
				  "DBus request to get daemon status");

	status = g_strdup (tracker_status_get_as_string ());

	dbus_g_method_return (context, status);
	g_free (status);

	tracker_dbus_request_success (request_id);
	tracker_dbus_request_unblock_hooks ();
}

void
tracker_daemon_get_services (TrackerDaemon	    *object,
			     gboolean		     main_services_only,
			     DBusGMethodInvocation  *context,
			     GError		   **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	guint		    request_id;
	GHashTable	   *values = NULL;

	/* FIXME: Note, the main_services_only variable is redundant */

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
				  "DBus request to get daemon services");

	iface = tracker_db_manager_get_db_interface (TRACKER_DB_COMMON);

	result_set = tracker_data_manager_exec_proc (iface, "GetServices", 0);
	values = tracker_dbus_query_result_to_hash_table (result_set);

	if (result_set) {
		g_object_unref (result_set);
	}

	dbus_g_method_return (context, values);

	g_hash_table_destroy (values);

	tracker_dbus_request_success (request_id);
}

static void
stats_cache_filter_dups_func (gpointer data,
			      gpointer user_data)
{
	GHashTable *values;
	GStrv       strv;
	gpointer    p;
	gint        count;

	/* FIXME: There is a really shit bug here that needs
	 * fixing. If a file has "Files" as its main category
	 * and not its *parent* category, then we end up with
	 * 2 "Files" listings. This sounds like a bug with
	 * the indexer's insert mechanisms. So far this seems
	 * to only happen for removable media files
	 *
	 * For now, we will concatenate values to sort out the
	 * duplicates: 
	 */
	strv = data;
	values = user_data;

	count = atoi (strv[1]);

	p = g_hash_table_lookup (values, strv[0]);

	if (G_UNLIKELY (p)) {
		count += GPOINTER_TO_INT (p);
	}

	g_hash_table_replace (values, 
			      g_strdup (strv[0]), 
			      GINT_TO_POINTER (count));
}

static GHashTable *
stats_cache_get_latest (void)
{
	TrackerDBResultSet *result_set;
	TrackerDBInterface *iface;
	GHashTable         *services;
	GHashTable         *values;
	GHashTableIter      iter;
	GSList             *parent_services, *l;
	gpointer            key, value;
	guint               i;
	const gchar        *services_to_fetch[3] = { 
		TRACKER_DB_FOR_FILE_SERVICE, 
		TRACKER_DB_FOR_EMAIL_SERVICE, 
		NULL 
	};

	/* Set up empty list of services because SQL queries won't give us 0 items. */
	iface = tracker_db_manager_get_db_interface (TRACKER_DB_COMMON);
	result_set = tracker_data_manager_exec_proc (iface, "GetServices", 0);
	services = tracker_dbus_query_result_to_hash_table (result_set);

	if (result_set) {
		g_object_unref (result_set);
	}

	values = g_hash_table_new_full (g_str_hash,
					g_str_equal,
					g_free,
					NULL);

	/* Put services with 0 counts into new hash table */
	g_hash_table_iter_init (&iter, services);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		g_hash_table_replace (values, g_strdup (key), GINT_TO_POINTER (0));
	}

	g_hash_table_unref (services);

	/* Populate with real stats */
	for (i = 0; services_to_fetch[i]; i++) {		
		TrackerDBInterface *iface;
		GPtrArray          *stats; 

		iface = tracker_db_manager_get_db_interface_by_service (services_to_fetch[i]);

		/* GetStats has asc in its query. Therefore we don't have to
		 * lookup the in a to compare in b, just compare index based.
		 * Maybe we want to change this nonetheless later?
		 */
		result_set = tracker_data_manager_exec_proc (iface, "GetStats", 0);
		stats = tracker_dbus_query_result_to_ptr_array (result_set);

		if (result_set) {
			g_object_unref (result_set);
		}

		g_ptr_array_foreach (stats, stats_cache_filter_dups_func, values);
		tracker_dbus_results_ptr_array_free (&stats);
	}

	/*
	 * For each of the top services, add the items of their subservices 
	 * (calculated in the previous GetStats)
	 */
	parent_services = tracker_ontology_get_parent_services ();

	for (l = parent_services; l; l = l->next) {
		GArray      *subcategories;
		const gchar *name;
		gint         children = 0;

		name = tracker_service_get_name (l->data);
		
		if (!name) {
			continue;
		}

		subcategories = tracker_ontology_get_subcategory_ids (name);

		if (!subcategories) {
			continue;
		}

		for (i = 0; i < subcategories->len; i++) {
			const gchar *subclass;
			gpointer     p;
			gint         id;

			id = g_array_index (subcategories, gint, i);
			subclass = tracker_ontology_get_service_by_id (id);
			p = g_hash_table_lookup (values, subclass);
			children += GPOINTER_TO_INT (p);
		}

		g_hash_table_replace (values, 
				      g_strdup (name), 
				      GINT_TO_POINTER (children));
	}

	return values;
}

static gboolean 
stats_cache_timeout (gpointer user_data)
{
	g_message ("Statistics cache has expired, updating...");

	tracker_dbus_indexer_check_is_paused ();
	tracker_daemon_signal_statistics ();

	return TRUE;
}

static gint
stats_cache_sort_func (gconstpointer a,
		       gconstpointer b)
{
	
	const GStrv *strv_a = (GStrv *) a;
	const GStrv *strv_b = (GStrv *) b;

	g_return_val_if_fail (strv_a != NULL, 0);
	g_return_val_if_fail (strv_b != NULL, 0);

	return g_strcmp0 (*strv_a[0], *strv_b[0]);
}

void
tracker_daemon_get_stats (TrackerDaemon		 *object,
			  DBusGMethodInvocation  *context,
			  GError		**error)
{
	TrackerDaemonPrivate *priv;
	guint		      request_id;
	GPtrArray            *values;
	GHashTableIter        iter;
	gpointer              key, value;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_block_hooks ();
	tracker_dbus_request_new (request_id,
				  "DBus request to get daemon service stats");

	priv = TRACKER_DAEMON_GET_PRIVATE (object);

	values = g_ptr_array_new ();

	g_hash_table_iter_init (&iter, priv->stats_cache);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GStrv        strv;
		const gchar *service_type;
		gint         count;

		service_type = key;
		count = GPOINTER_TO_INT (value);

		strv = g_new (gchar*, 3);
		strv[0] = g_strdup (service_type);
		strv[1] = g_strdup_printf ("%d", count);
		strv[2] = NULL;

		g_ptr_array_add (values, strv);
	}

	/* Sort result so it is alphabetical */
	g_ptr_array_sort (values, stats_cache_sort_func);

	dbus_g_method_return (context, values);

	g_ptr_array_foreach (values, (GFunc) g_strfreev, NULL);
	g_ptr_array_free (values, TRUE);

	tracker_dbus_request_success (request_id);
	tracker_dbus_request_unblock_hooks ();
}

void
tracker_daemon_set_bool_option (TrackerDaemon	       *object,
				const gchar	       *option,
				gboolean		value,
				DBusGMethodInvocation  *context,
				GError		      **error)
{
	TrackerDaemonPrivate *priv;
	guint		      request_id;
	GError		     *actual_error = NULL;

	/* FIXME: Shouldn't we just make the TrackerConfig module a
	 * DBus object instead so values can be tweaked in real time
	 * over the bus?
	 */

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (option != NULL, context);

	priv = TRACKER_DAEMON_GET_PRIVATE (object);

	tracker_dbus_request_new (request_id,
				  "DBus request to set daemon boolean option, "
				  "key:'%s', value:%s",
				  option,
				  value ? "true" : "false");

	if (strcasecmp (option, "Pause") == 0) {
		/* We do it here and not in the callback because we
		 * don't know if something else paused us or if it
		 * was the signal from our request.
		 */
		tracker_status_set_is_paused_manually (value);
	} else if (strcasecmp (option, "FastMerges") == 0) {
		tracker_config_set_fast_merges (priv->config, value);
		g_message ("Fast merges set to %d", value);
	} else if (strcasecmp (option, "EnableIndexing") == 0) {
		/* FIXME: Ideally we should be picking up the
		 * "nofify::enable-indexing" change on the
		 * priv->config in the tracker-main.c module to do
		 * the signal change and to set the daemon to
		 * readonly mode.
		 */
		tracker_config_set_enable_indexing (priv->config, value);
		tracker_status_set_is_readonly (value);
		g_message ("Enable indexing set to %d", value);
	} else if (strcasecmp (option, "EnableWatching") == 0) {
		tracker_config_set_enable_watches (priv->config, value);
		g_message ("Enable Watching set to %d", value);
	} else if (strcasecmp (option, "LowMemoryMode") == 0) {
		tracker_config_set_low_memory_mode (priv->config, value);
		g_message ("Extra memory usage set to %d", !value);
	} else if (strcasecmp (option, "IndexFileContents") == 0) {
		tracker_config_set_enable_content_indexing (priv->config, value);
		g_message ("Index file contents set to %d", value);
	} else if (strcasecmp (option, "GenerateThumbs") == 0) {
		tracker_config_set_enable_thumbnails (priv->config, value);
		g_message ("Generate thumbnails set to %d", value);
	} else if (strcasecmp (option, "IndexMountedDirectories") == 0) {
		tracker_config_set_index_mounted_directories (priv->config, value);
		g_message ("Indexing mounted directories set to %d", value);
	} else if (strcasecmp (option, "IndexRemovableDevices") == 0) {
		tracker_config_set_index_removable_devices (priv->config, value);
		g_message ("Indexing removable devices set to %d", value);
	} else if (strcasecmp (option, "BatteryIndex") == 0) {
		tracker_config_set_disable_indexing_on_battery (priv->config, !value);
		g_message ("Disable index on battery set to %d", !value);
	} else if (strcasecmp (option, "BatteryIndexInitial") == 0) {
		tracker_config_set_disable_indexing_on_battery_init (priv->config, !value);
		g_message ("Disable initial index sweep on battery set to %d", !value);
	} else {
		g_set_error (&actual_error,
			     TRACKER_DBUS_ERROR,
			     0,
			     "Option does not exist");
	}

	if (!actual_error) {
		dbus_g_method_return (context);
	} else {
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
	}

	tracker_dbus_request_success (request_id);
}

void
tracker_daemon_set_int_option (TrackerDaemon	      *object,
			       const gchar	      *option,
			       gint		       value,
			       DBusGMethodInvocation  *context,
			       GError		     **error)
{
	TrackerDaemonPrivate *priv;
	guint		      request_id;
	GError		     *actual_error = NULL;

	/* FIXME: Shouldn't we just make the TrackerConfig module a
	 * DBus object instead so values can be tweaked in real time
	 * over the bus?
	 */

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (option != NULL, context);

	priv = TRACKER_DAEMON_GET_PRIVATE (object);

	tracker_dbus_request_new (request_id,
				  "DBus request to set daemon integer option, "
				  "key:'%s', value:%d",
				  option,
				  value);

	if (strcasecmp (option, "Throttle") == 0) {
		tracker_config_set_throttle (priv->config, value);
		g_message ("throttle set to %d", value);
	} else if (strcasecmp (option, "MaxText") == 0) {
		tracker_config_set_max_text_to_index (priv->config, value);
		g_message ("Maxinum amount of text set to %d", value);
	} else if (strcasecmp (option, "MaxWords") == 0) {
		tracker_config_set_max_words_to_index (priv->config, value);
		g_message ("Maxinum number of unique words set to %d", value);
	} else {
		g_set_error (&actual_error,
			     TRACKER_DBUS_ERROR,
			     0,
			     "Option does not exist");
	}

	if (!actual_error) {
		dbus_g_method_return (context);
	} else {
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
	}

	tracker_dbus_request_success (request_id);
}

void
tracker_daemon_shutdown (TrackerDaemon		*object,
			 gboolean		 reindex,
			 DBusGMethodInvocation	*context,
			 GError		       **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
				  "DBus request to shutdown daemon, "
				  "reindex:%s",
				  reindex ? "yes" : "no");

	g_message ("Tracker daemon attempting to shutdown");

	tracker_set_reindex_on_shutdown (reindex);

	g_timeout_add (500, (GSourceFunc) tracker_shutdown, NULL);

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

void
tracker_daemon_prompt_index_signals (TrackerDaemon	    *object,
				     DBusGMethodInvocation  *context,
				     GError		   **error)
{
	TrackerDaemonPrivate *priv;
	guint		      request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
				  "DBus request to daemon to signal progress/state");

	priv = TRACKER_DAEMON_GET_PRIVATE (object);

	/* Signal state change */
	tracker_status_signal ();

	/* Signal progress */
	g_signal_emit_by_name (object,
			       "index-progress",
			       "Files",
			       "",
			       tracker_processor_get_files_total (priv->processor),
			       tracker_processor_get_directories_found (priv->processor),
			       tracker_processor_get_directories_total (priv->processor),
			       tracker_processor_get_seconds_elapsed (priv->processor));

#if 1
	/* FIXME: We need a way of knowing WHAT service we have a
	 * count for, i.e. emails, files, etc.
	 */
	g_signal_emit_by_name (object,
			       "index-progress",
			       "Emails",
			       "",
			       0,  /* priv->tracker->index_count, */
			       0,  /* priv->tracker->mbox_processed, */
			       0,  /* priv->tracker->mbox_count); */
			       0); /* seconds elapsed */
#endif

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

void
tracker_daemon_signal_statistics (void)
{
	GObject		     *daemon;
	TrackerDaemonPrivate *priv;
	GHashTable           *stats;
	GHashTableIter        iter;
	gpointer              key, value;
	GPtrArray            *values;

	daemon = tracker_dbus_get_object (TRACKER_TYPE_DAEMON);
	priv = TRACKER_DAEMON_GET_PRIVATE (daemon);

	/* Get latest */
	stats = stats_cache_get_latest ();

	/* There are 3 situations here:
	 *  - 1. No new stats
	 *       Action: Do nothing
	 *  - 2. New stats and old stats
	 *       Action: Check what has changed and emit new stats
	 */

	g_message ("Checking for statistics changes and signalling clients...");

	/* Situation #1 */
	if (g_hash_table_size (stats) < 1) {
		g_hash_table_unref (stats);
		g_message ("  No new statistics, doing nothing");
		return;
	}

	/* Situation #2 */
	values = g_ptr_array_new ();

	g_hash_table_iter_init (&iter, stats);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar  *service_type;
		gpointer      data;
		gint          old_count, new_count;
		
		service_type = key;
		new_count = GPOINTER_TO_INT (value);
			
		data = g_hash_table_lookup (priv->stats_cache, service_type);
		old_count = GPOINTER_TO_INT (data);
		
		if (old_count != new_count) {
			GStrv strv;

			g_message ("  Updating '%s' with new count:%d, old count:%d, diff:%d", 
				   service_type,
				   new_count,
				   old_count,
				   new_count - old_count);
			
			g_hash_table_replace (priv->stats_cache, 
					      g_strdup (service_type), 
					      GINT_TO_POINTER (new_count));

			strv = g_new (gchar*, 3);
			strv[0] = g_strdup (service_type);
			strv[1] = g_strdup_printf ("%d", new_count);
			strv[2] = NULL;
			
			g_ptr_array_add (values, strv);
		}
	}

	g_hash_table_unref (stats);

	if (values->len > 0) {
		/* Make sure we sort the results first */
		g_ptr_array_sort (values, stats_cache_sort_func);
		
		g_signal_emit (daemon, signals[SERVICE_STATISTICS_UPDATED], 0, values);
	} else {
		g_message ("  No changes in the statistics");
	}
	
	g_ptr_array_foreach (values, (GFunc) g_strfreev, NULL);
	g_ptr_array_free (values, TRUE);
}
