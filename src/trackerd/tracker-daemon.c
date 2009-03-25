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

typedef struct {
	TrackerConfig	 *config;
	TrackerProcessor *processor;
	DBusGProxy	 *indexer_proxy;

	GTimeVal          last_stats_time;
	GHashTable       *last_stats;
} TrackerDaemonPrivate;

enum {
	INDEX_STATE_CHANGE,
	INDEX_FINISHED,
	INDEX_PROGRESS,
	SERVICE_STATISTICS_UPDATED,
	LAST_SIGNAL
};

static void tracker_daemon_finalize (GObject *object);

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
indexer_finished_cb (DBusGProxy *proxy,
		     gdouble	 seconds_elapsed,
		     guint	 items_done,
		     gboolean	 interrupted,
		     gpointer	 user_data)
{
	tracker_daemon_signal_statistics ();
}

static void
tracker_daemon_init (TrackerDaemon *object)
{
	TrackerDaemonPrivate *priv;
	TrackerDBInterface   *iface;
	DBusGProxy           *proxy;

	priv = TRACKER_DAEMON_GET_PRIVATE (object);

	proxy = tracker_dbus_indexer_get_proxy ();
	priv->indexer_proxy = g_object_ref (proxy);

	dbus_g_proxy_connect_signal (proxy, "Finished",
				     G_CALLBACK (indexer_finished_cb),
				     object,
				     NULL);

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	priv->last_stats = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free, 
						  NULL);
}

static void
tracker_daemon_finalize (GObject *object)
{
	TrackerDaemon	     *daemon;
	TrackerDaemonPrivate *priv;

	daemon = TRACKER_DAEMON (object);
	priv = TRACKER_DAEMON_GET_PRIVATE (daemon);

	dbus_g_proxy_disconnect_signal (priv->indexer_proxy, "Finished",
					G_CALLBACK (indexer_finished_cb),
					NULL);

	if (priv->last_stats) {
		g_hash_table_unref (priv->last_stats);
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
	TrackerDaemonPrivate *priv;
	gchar		     *status;

	priv = TRACKER_DAEMON_GET_PRIVATE (object);

	g_debug ("DBus request to get daemon status");

	status = g_strdup (tracker_status_get_as_string ());

	dbus_g_method_return (context, status);

	g_free (status);
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

	/* Here it doesn't matter which one we ask, as long as it has common.db
	 * attached. The service ones are cached connections, so we can use
	 * those instead of asking for an individual-file connection (like what
	 * the original code had)
	 */

	/* iface = tracker_db_manager_get_db_interfaceX (TRACKER_DB_COMMON); */

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	result_set = tracker_data_manager_exec_proc (iface, "GetServices", 0);
	values = tracker_dbus_query_result_to_hash_table (result_set);

	if (result_set) {
		g_object_unref (result_set);
	}

	dbus_g_method_return (context, values);

	g_hash_table_destroy (values);

	tracker_dbus_request_success (request_id);
}

void
tracker_daemon_get_stats (TrackerDaemon		 *object,
			  DBusGMethodInvocation  *context,
			  GError		**error)
{
	TrackerDaemonPrivate *priv;
	TrackerDBInterface   *iface;
	TrackerDBResultSet   *result_set;
	guint		      request_id;
	GPtrArray            *values;
	GTimeVal	      now;
	gboolean              use_cache;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
				  "DBus request to get daemon service stats");

	priv = TRACKER_DAEMON_GET_PRIVATE (object);

	values = g_ptr_array_new ();

	/* Use the cache if we have requests stats recently. */
	g_get_current_time (&now);

	if (priv->last_stats_time.tv_sec == 0) {
		use_cache = FALSE;
	} else {
		use_cache = now.tv_sec - priv->last_stats_time.tv_sec < 60;
	}

	if (use_cache) {
		GHashTableIter iter;
		gpointer key, value;

		g_message ("Using cache for stats");
		
		g_hash_table_iter_init (&iter, priv->last_stats);
		while (g_hash_table_iter_next (&iter, &key, &value)) {
			GStrv         strv;
			const gchar  *service_type;
			gint          count;
			
			service_type = key;
			count = GPOINTER_TO_INT (value);

			strv = g_new (gchar*, 3);
			strv[0] = g_strdup (service_type);
			strv[1] = g_strdup_printf ("%d", count);
			strv[2] = NULL;

			g_ptr_array_add (values, strv);
		}
	} else {
		GPtrArray *stats, *parent_stats;
		gint i;

		g_message ("Using database for stats (cache is %ld seconds old)",
			   now.tv_sec - priv->last_stats_time.tv_sec);

		/* Here it doesn't matter which one we ask, as long as
		 * it has common.db attached. The service ones are
		 * cached connections, so we can use those instead of
		 * asking for an individual-file connection (like what
		 * the original code had).
		 */
		iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);
		
		result_set = tracker_data_manager_exec_proc (iface, "GetStats", 0);
		stats = tracker_dbus_query_result_to_ptr_array (result_set);
		
		if (result_set) {
			g_object_unref (result_set);
		}
		
		result_set = tracker_data_manager_exec_proc (iface, "GetStatsForParents", 0);
		parent_stats = tracker_dbus_query_result_to_ptr_array (result_set);
		
		if (result_set) {
			g_object_unref (result_set);
		}

		/* Concatenate stats */
		for (i = 0; i < stats->len; i++) {
			g_ptr_array_add (values, g_ptr_array_index (stats, i));
		}
		
		for (i = 0; i < parent_stats->len; i++) {
			g_ptr_array_add (values, g_ptr_array_index (parent_stats, i));
		}

		/* Update local cache */
		for (i = 0; i < values->len; i++) {
			gchar       **p;
			const gchar  *service_type = NULL;
			gint          new_count;

			p = g_ptr_array_index (values, i);
			service_type = p[0];
			new_count = atoi (p[1]);

			g_hash_table_replace (priv->last_stats, 
					      g_strdup (service_type), 
					      GINT_TO_POINTER (new_count));
		}

		g_ptr_array_free (parent_stats, TRUE);
		g_ptr_array_free (stats, TRUE);

		priv->last_stats_time = now;

		g_message ("Updated stats cache time to now");
	}

	dbus_g_method_return (context, values);

	g_ptr_array_foreach (values, (GFunc) g_strfreev, NULL);
	g_ptr_array_free (values, TRUE);

	tracker_dbus_request_success (request_id);
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
	TrackerDaemonPrivate *priv;
	guint		      request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
				  "DBus request to shutdown daemon, "
				  "reindex:%s",
				  reindex ? "yes" : "no");

	priv = TRACKER_DAEMON_GET_PRIVATE (object);

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
	TrackerDBInterface   *iface;
	TrackerDBResultSet   *result_set;
	GPtrArray	     *stats, *parent_stats;
	GPtrArray            *values;
	GTimeVal              now;
	gint                  i;

	daemon = tracker_dbus_get_object (TRACKER_TYPE_DAEMON);
	priv = TRACKER_DAEMON_GET_PRIVATE (daemon);

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	/* GetStats has asc in its query. Therefore we don't have to
	 * lookup the in a to compare in b, just compare index based.
	 * Maybe we want to change this nonetheless later?
	 */
	g_message ("Using database for stats for accurate signal");

	result_set = tracker_data_manager_exec_proc (iface, "GetStats", 0);
	stats = tracker_dbus_query_result_to_ptr_array (result_set);

	if (result_set) {
		g_object_unref (result_set);
	}

	result_set = tracker_data_manager_exec_proc (iface, "GetStatsForParents", 0);
	parent_stats = tracker_dbus_query_result_to_ptr_array (result_set);

	if (result_set) {
		g_object_unref (result_set);
	}
	
	/* Concatenate stats */
	values = g_ptr_array_new ();
	
	for (i = 0; i < stats->len; i++) {
		g_ptr_array_add (values, g_ptr_array_index (stats, i));
	}

	for (i = 0; i < parent_stats->len; i++) {
		g_ptr_array_add (values, g_ptr_array_index (parent_stats, i));
	}

	g_ptr_array_free (parent_stats, TRUE);
	g_ptr_array_free (stats, TRUE);

	/* There are 3 situations here:
	 *  - 1. No new stats
	 *       Action: Do nothing
	 *  - 2. No previous stats
	 *       Action: Emit all new stats
	 *  - 3. New stats and old stats
	 *       Action: Check what has changed and emit new stats
	 */

	g_message ("Checking for statistics changes and signalling clients...");

	/* Situation #1 */
	if (!values || values->len < 1) {
		g_message ("  No new statistics, doing nothing");
		return;
	}

	if (g_hash_table_size (priv->last_stats) < 1) {
		gint i;

		/* Situation #2 */
		g_message ("  No previous statistics");

		for (i = 0; i < values->len; i++) {
			const gchar **p;
			const gchar  *service_type = NULL;
			gint          new_count;

			p = g_ptr_array_index (values, i);

			service_type = p[0];
			new_count = atoi (p[1]);
			
			if (!service_type) {
				continue;
			}

			g_message ("  Adding '%s' with count:%d", 
				   service_type,
				   new_count);
			g_hash_table_insert (priv->last_stats, 
					     g_strdup (service_type), 
					     GINT_TO_POINTER (new_count));
		}

		/* Emit signal */
		g_signal_emit (daemon, signals[SERVICE_STATISTICS_UPDATED], 0, values);
	} else {
		gint i;

		/* Situation #3 */
		for (i = 0; i < values->len; i++) {
			gchar       **p;
			const gchar  *service_type = NULL;
			gpointer      data;
			gint          old_count, new_count;

			p = g_ptr_array_index (values, i);
			service_type = p[0];
			new_count = atoi (p[1]);

			if (!service_type) {
				continue;
			}

			data = g_hash_table_lookup (priv->last_stats, service_type);
			old_count = GPOINTER_TO_INT (data);

			if (old_count != new_count) {
				g_message ("  Updating '%s' with new count:%d, old count:%d, diff:%d", 
					   service_type,
					   new_count,
					   old_count,
					   new_count - old_count);

				g_hash_table_replace (priv->last_stats, 
						      g_strdup (service_type), 
						      GINT_TO_POINTER (new_count));
			} else {
				/* Remove from values since the value is the same */
				g_strfreev (p);
				g_ptr_array_remove (values, p);

				/* Decrement i since we are about to
				 * increment it and we just removed
				 * an item. Otherwise we miss items.
				 */
				i--;
			}
		}

		if (values->len > 0) {
			g_signal_emit (daemon, signals[SERVICE_STATISTICS_UPDATED], 0, values);
		} else {
			g_message ("  No changes in the statistics");
		}
	}

	/* Update time for last stats */
	g_get_current_time (&now);
	priv->last_stats_time = now;

	g_message ("Updated stats cache time to now");

	g_ptr_array_foreach (values, (GFunc) g_strfreev, NULL);
	g_ptr_array_free (values, TRUE);
}
