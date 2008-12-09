/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

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

/* The indexer works as a state machine, there are 3 different queues:
 *
 * * The files queue: the highest priority one, individual files are
 *   stored here, waiting for metadata extraction, etc... files are
 *   taken one by one in order to be processed, when this queue is
 *   empty, a single token from the next queue is processed.
 *
 * * The directories queue: directories are stored here, waiting for
 *   being inspected. When a directory is inspected, contained files
 *   and directories will be prepended in their respective queues.
 *   When this queue is empty, a single token from the next queue
 *   is processed.
 *
 * * The modules list: indexing modules are stored here, these modules
 *   can either prepend the files or directories to be inspected in
 *   their respective queues.
 *
 * Once all queues are empty, all elements have been inspected, and the
 * indexer will emit the ::finished signal, this behavior can be observed
 * in the process_func() function.
 *
 * NOTE: Normally all indexing petitions will be sent over DBus, being
 *	 everything just pushed in the files queue.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>

#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gmodule.h>

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-hal.h>
#include <libtracker-common/tracker-language.h>
#include <libtracker-common/tracker-parser.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-module-config.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-thumbnailer.h>

#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-index-manager.h>
#include <libtracker-db/tracker-db-interface-sqlite.h>

#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-data-update.h>

#include "tracker-indexer.h"
#include "tracker-indexer-module.h"
#include "tracker-marshal.h"
#include "tracker-module-metadata-private.h"

#define TRACKER_INDEXER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_INDEXER, TrackerIndexerPrivate))

/* Flush every 'x' seconds */
#define FLUSH_FREQUENCY		    60

#define LOW_DISK_CHECK_FREQUENCY    10
#define SIGNAL_STATUS_FREQUENCY     10

/* Transaction every 'x' items */
#define TRANSACTION_MAX		    2000

/* Throttle defaults */
#define THROTTLE_DEFAULT	    0
#define THROTTLE_DEFAULT_ON_BATTERY 5

#define TRACKER_INDEXER_ERROR	   "tracker-indexer-error-domain"
#define TRACKER_INDEXER_ERROR_CODE  0

/* Properties that change in move event */
#define METADATA_FILE_NAME_DELIMITED "File:NameDelimited"
#define METADATA_FILE_EXT	     "File:Ext"
#define METADATA_FILE_PATH	     "File:Path"
#define METADATA_FILE_NAME	     "File:Name"

typedef struct PathInfo PathInfo;
typedef struct MetadataForeachData MetadataForeachData;
typedef struct MetadataRequest MetadataRequest;
typedef struct UpdateWordsForeachData UpdateWordsForeachData;
typedef enum TrackerIndexerState TrackerIndexerState;

struct TrackerIndexerPrivate {
	GQueue *dir_queue;
	GQueue *file_queue;
	GQueue *modules_queue;

	GHashTable *mtime_cache;

	GList *module_names;
	GQuark current_module;
	GHashTable *indexer_modules;

	gchar *db_dir;

	TrackerDBIndex *file_index;
	TrackerDBIndex *email_index;

	TrackerDBInterface *file_metadata;
	TrackerDBInterface *file_contents;
	TrackerDBInterface *email_metadata;
	TrackerDBInterface *email_contents;
	TrackerDBInterface *common;
	TrackerDBInterface *cache;

	TrackerConfig *config;
	TrackerLanguage *language;

	TrackerHal *hal;

	GTimer *timer;

	guint idle_id;
	guint pause_for_duration_id;
	guint disk_space_check_id;
	guint signal_status_id;
	guint flush_id;

	guint files_processed;
	guint files_indexed;
	guint items_processed;

	gboolean in_transaction;
	gboolean in_process;

	guint state;
};

struct PathInfo {
	TrackerIndexerModule *module;
	GFile *file;
	GFile *other_file;
	TrackerModuleFile *module_file;
	TrackerModuleFile *other_module_file;
};

struct MetadataForeachData {
	TrackerLanguage *language;
	TrackerConfig *config;
	TrackerService *service;
	gboolean add;
	guint32 id;
};

struct UpdateWordsForeachData {
	guint32 service_id;
	guint32 service_type_id;
};

enum TrackerIndexerState {
	TRACKER_INDEXER_STATE_FLUSHING	= 1 << 0,
	TRACKER_INDEXER_STATE_PAUSED	= 1 << 1,
	TRACKER_INDEXER_STATE_DISK_FULL = 1 << 2,
	TRACKER_INDEXER_STATE_STOPPED	= 1 << 3,
	TRACKER_INDEXER_STATE_LOW_BATT  = 1 << 4
};

enum {
	PROP_0,
	PROP_RUNNING,
};

enum {
	STATUS,
	STARTED,
	FINISHED,
	MODULE_STARTED,
	MODULE_FINISHED,
	PAUSED,
	CONTINUED,
	LAST_SIGNAL
};

static gboolean process_func	       (gpointer	     data);
static void	check_disk_space_start (TrackerIndexer	    *indexer);
static void	state_set_flags        (TrackerIndexer	    *indexer,
					TrackerIndexerState  state);
static void	state_unset_flags      (TrackerIndexer	    *indexer,
					TrackerIndexerState  state);
static void	state_check	       (TrackerIndexer	    *indexer);

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (TrackerIndexer, tracker_indexer, G_TYPE_OBJECT)

static PathInfo *
path_info_new (TrackerIndexerModule *module,
	       GFile                *file,
	       GFile                *other_file)
{
	PathInfo *info;

	info = g_slice_new (PathInfo);
	info->module = module;

	info->file = g_object_ref (file);
	info->module_file = tracker_indexer_module_create_file (module, file);

	if (G_UNLIKELY (other_file)) {
		info->other_file = g_object_ref (other_file);
		info->other_module_file = tracker_indexer_module_create_file (module, other_file);
	} else {
		info->other_file = NULL;
		info->other_module_file = NULL;
	}

	return info;
}

static void
path_info_free (PathInfo *info)
{
	if (G_UNLIKELY (info->other_file)) {
		g_object_unref (info->other_file);
	}

	if (G_UNLIKELY (info->other_module_file)) {
		g_object_unref (info->other_module_file);
	}

	if (G_LIKELY (info->module_file)) {
		g_object_unref (info->module_file);
	}

	g_object_unref (info->file);
	g_slice_free (PathInfo, info);
}

static void
start_transaction (TrackerIndexer *indexer)
{
	g_debug ("Transaction start");

	indexer->private->in_transaction = TRUE;

	tracker_db_interface_start_transaction (indexer->private->cache);
	tracker_db_interface_start_transaction (indexer->private->file_contents);
	tracker_db_interface_start_transaction (indexer->private->email_contents);
	tracker_db_interface_start_transaction (indexer->private->file_metadata);
	tracker_db_interface_start_transaction (indexer->private->email_metadata);
	tracker_db_interface_start_transaction (indexer->private->common);
}

static void
stop_transaction (TrackerIndexer *indexer)
{
	if (!indexer->private->in_transaction) {
		return;
	}

	tracker_db_interface_end_transaction (indexer->private->common);
	tracker_db_interface_end_transaction (indexer->private->email_metadata);
	tracker_db_interface_end_transaction (indexer->private->file_metadata);
	tracker_db_interface_end_transaction (indexer->private->email_contents);
	tracker_db_interface_end_transaction (indexer->private->file_contents);
	tracker_db_interface_end_transaction (indexer->private->cache);

	indexer->private->files_indexed += indexer->private->files_processed;
	indexer->private->files_processed = 0;
	indexer->private->in_transaction = FALSE;

	g_debug ("Transaction commit");
}

static void
signal_status (TrackerIndexer *indexer,
	       const gchar    *why)
{
	gdouble seconds_elapsed;
	guint	files_remaining;

	files_remaining = g_queue_get_length (indexer->private->file_queue);
	seconds_elapsed = g_timer_elapsed (indexer->private->timer, NULL);

	if (indexer->private->files_indexed > 0 &&
	    files_remaining > 0) {
		gchar *str1;
		gchar *str2;

		str1 = tracker_seconds_estimate_to_string (seconds_elapsed,
							   TRUE,
							   indexer->private->files_indexed,
							   files_remaining);
		str2 = tracker_seconds_to_string (seconds_elapsed, TRUE);

		g_message ("Indexed %d/%d, module:'%s', %s left, %s elapsed (%s)",
			   indexer->private->files_indexed,
			   indexer->private->files_indexed + files_remaining,
			   g_quark_to_string (indexer->private->current_module),
			   str1,
			   str2,
			   why);

		g_free (str2);
		g_free (str1);
	}

	g_signal_emit (indexer, signals[STATUS], 0,
		       seconds_elapsed,
		       g_quark_to_string (indexer->private->current_module),
		       indexer->private->files_indexed,
		       files_remaining);
}

static gboolean
flush_data (TrackerIndexer *indexer)
{
	indexer->private->flush_id = 0;

	state_set_flags (indexer, TRACKER_INDEXER_STATE_FLUSHING);

	if (indexer->private->in_transaction) {
		stop_transaction (indexer);
	}

	tracker_db_index_flush (indexer->private->file_index);
	tracker_db_index_flush (indexer->private->email_index);
	signal_status (indexer, "flush");

	indexer->private->items_processed = 0;

	state_unset_flags (indexer, TRACKER_INDEXER_STATE_FLUSHING);

	return FALSE;
}

static void
stop_scheduled_flush (TrackerIndexer *indexer)
{
	if (indexer->private->flush_id) {
		g_source_remove (indexer->private->flush_id);
		indexer->private->flush_id = 0;
	}
}

static void
schedule_flush (TrackerIndexer *indexer,
		gboolean	immediately)
{
	if (indexer->private->state != 0) {
		return;
	}

	if (immediately) {
		/* No need to wait for flush timeout */
		stop_scheduled_flush (indexer);
		flush_data (indexer);
		return;
	}

	/* Don't schedule more than one at the same time */
	if (indexer->private->flush_id != 0) {
		return;
	}

	indexer->private->flush_id = g_timeout_add_seconds (FLUSH_FREQUENCY,
							    (GSourceFunc) flush_data,
							    indexer);
}

#ifdef HAVE_HAL

static void
set_up_throttle (TrackerIndexer *indexer)
{
	gint throttle;

	/* If on a laptop battery and the throttling is default (i.e.
	 * 0), then set the throttle to be higher so we don't kill
	 * the laptop battery.
	 */
	throttle = tracker_config_get_throttle (indexer->private->config);

	if (tracker_hal_get_battery_in_use (indexer->private->hal)) {
		g_message ("We are running on battery");

		if (throttle == THROTTLE_DEFAULT) {
			tracker_config_set_throttle (indexer->private->config,
						     THROTTLE_DEFAULT_ON_BATTERY);
			g_message ("Setting throttle from %d to %d",
				   throttle,
				   THROTTLE_DEFAULT_ON_BATTERY);
		} else {
			g_message ("Not setting throttle, it is currently set to %d",
				   throttle);
		}
	} else {
		g_message ("We are not running on battery");

		if (throttle == THROTTLE_DEFAULT_ON_BATTERY) {
			tracker_config_set_throttle (indexer->private->config,
						     THROTTLE_DEFAULT);
			g_message ("Setting throttle from %d to %d",
				   throttle,
				   THROTTLE_DEFAULT);
		} else {
			g_message ("Not setting throttle, it is currently set to %d",
				   throttle);
		}

		state_unset_flags (indexer, TRACKER_INDEXER_STATE_LOW_BATT);
	}
}

static void
notify_battery_in_use_cb (GObject *gobject,
			  GParamSpec *arg1,
			  gpointer user_data)
{
	set_up_throttle (TRACKER_INDEXER (user_data));
}

static void
notify_battery_percentage_cb (GObject    *object,
			      GParamSpec *pspec,
			      gpointer    user_data)
{
	TrackerIndexer *indexer;
	gdouble percentage;
	gboolean battery_in_use;

	indexer = user_data;

	percentage = tracker_hal_get_battery_percentage (TRACKER_HAL (object));
	battery_in_use = tracker_hal_get_battery_in_use (TRACKER_HAL (object));

	/* FIXME: This could be a configuration option */
	if (battery_in_use && percentage <= 0.05) {
		/* Running on low batteries, stop indexing for now */
		state_set_flags (indexer, TRACKER_INDEXER_STATE_LOW_BATT);
	} else {
		state_unset_flags (indexer, TRACKER_INDEXER_STATE_LOW_BATT);
	}

	set_up_throttle (TRACKER_INDEXER (user_data));
}

#endif /* HAVE_HAL */

static void
tracker_indexer_finalize (GObject *object)
{
	TrackerIndexerPrivate *priv;

	priv = TRACKER_INDEXER_GET_PRIVATE (object);

	/* Important! Make sure we flush if we are scheduled to do so,
	 * and do that first.
	 */
	stop_scheduled_flush (TRACKER_INDEXER (object));

	if (priv->pause_for_duration_id) {
		g_source_remove (priv->pause_for_duration_id);
	}

	if (priv->idle_id) {
		g_source_remove (priv->idle_id);
	}

	if (priv->disk_space_check_id) {
		g_source_remove (priv->disk_space_check_id);
	}

	if (priv->signal_status_id) {
		g_source_remove (priv->signal_status_id);
	}

	if (priv->timer) {
		g_timer_destroy (priv->timer);
	}

#ifdef HAVE_HAL
	g_signal_handlers_disconnect_by_func (priv->hal,
					      notify_battery_in_use_cb,
					      TRACKER_INDEXER (object));
	g_signal_handlers_disconnect_by_func (priv->hal,
					      notify_battery_percentage_cb,
					      TRACKER_INDEXER (object));

	g_object_unref (priv->hal);
#endif /* HAVE_HAL */

	g_object_unref (priv->language);
	g_object_unref (priv->config);

	g_object_unref (priv->file_index);
	g_object_unref (priv->email_index);

	g_free (priv->db_dir);

	g_hash_table_unref (priv->indexer_modules);

	g_list_foreach (priv->module_names, (GFunc) g_free, NULL);
	g_list_free (priv->module_names);

	g_queue_foreach (priv->modules_queue, (GFunc) g_free, NULL);
	g_queue_free (priv->modules_queue);

	g_hash_table_unref (priv->mtime_cache);

	g_queue_foreach (priv->dir_queue, (GFunc) path_info_free, NULL);
	g_queue_free (priv->dir_queue);

	g_queue_foreach (priv->file_queue, (GFunc) path_info_free, NULL);
	g_queue_free (priv->file_queue);

	G_OBJECT_CLASS (tracker_indexer_parent_class)->finalize (object);
}

static void
tracker_indexer_get_property (GObject	 *object,
			      guint	  prop_id,
			      GValue	 *value,
			      GParamSpec *pspec)
{
	TrackerIndexerPrivate *priv;

	priv = TRACKER_INDEXER_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_RUNNING:
		g_value_set_boolean (value,
				     tracker_indexer_get_running (TRACKER_INDEXER (object)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_indexer_class_init (TrackerIndexerClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = tracker_indexer_finalize;
	object_class->get_property = tracker_indexer_get_property;

	signals[STATUS] =
		g_signal_new ("status",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerIndexerClass, status),
			      NULL, NULL,
			      tracker_marshal_VOID__DOUBLE_STRING_UINT_UINT,
			      G_TYPE_NONE,
			      4,
			      G_TYPE_DOUBLE,
			      G_TYPE_STRING,
			      G_TYPE_UINT,
			      G_TYPE_UINT);
	signals[STARTED] =
		g_signal_new ("started",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerIndexerClass, started),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[PAUSED] =
		g_signal_new ("paused",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerIndexerClass, paused),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
	signals[CONTINUED] =
		g_signal_new ("continued",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerIndexerClass, continued),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[FINISHED] =
		g_signal_new ("finished",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerIndexerClass, finished),
			      NULL, NULL,
			      tracker_marshal_VOID__DOUBLE_UINT_BOOL,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_DOUBLE,
			      G_TYPE_UINT,
			      G_TYPE_BOOLEAN);
	signals[MODULE_STARTED] =
		g_signal_new ("module-started",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerIndexerClass, module_started),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);
	signals[MODULE_FINISHED] =
		g_signal_new ("module-finished",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerIndexerClass, module_finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

	g_object_class_install_property (object_class,
					 PROP_RUNNING,
					 g_param_spec_boolean ("running",
							       "Running",
							       "Whether the indexer is running",
							       TRUE,
							       G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof (TrackerIndexerPrivate));
}

static void
check_started (TrackerIndexer *indexer)
{
	TrackerIndexerState state;

	state = indexer->private->state;

	if (!(state & TRACKER_INDEXER_STATE_STOPPED)) {
		return;
	}

	state_unset_flags (indexer, TRACKER_INDEXER_STATE_STOPPED);

	g_timer_destroy (indexer->private->timer);
	indexer->private->timer = g_timer_new ();

	/* Open indexes */
	tracker_db_index_open (indexer->private->file_index);
	tracker_db_index_open (indexer->private->email_index);

	g_signal_emit (indexer, signals[STARTED], 0);
}

static void
check_stopped (TrackerIndexer *indexer,
	       gboolean        interrupted)
{
	gchar	*str;
	gdouble  seconds_elapsed;

	/* No more modules to query, we're done */
	g_timer_stop (indexer->private->timer);
	seconds_elapsed = g_timer_elapsed (indexer->private->timer, NULL);

	/* Flush remaining items */
	schedule_flush (indexer, TRUE);

	/* Close indexes */
	tracker_db_index_close (indexer->private->file_index);
	tracker_db_index_close (indexer->private->email_index);

	state_set_flags (indexer, TRACKER_INDEXER_STATE_STOPPED);

	/* Clean up temporary data */
	g_hash_table_remove_all (indexer->private->mtime_cache);

	/* Print out how long it took us */
	str = tracker_seconds_to_string (seconds_elapsed, FALSE);

	g_message ("Indexer finished in %s, %d files indexed in total",
		   str,
		   indexer->private->files_indexed);
	g_free (str);

	/* Finally signal done */
	g_signal_emit (indexer, signals[FINISHED], 0,
		       seconds_elapsed,
		       indexer->private->files_indexed,
		       interrupted);
}

static gboolean
check_is_disk_space_low (TrackerIndexer *indexer)
{
	const gchar *path;
	struct statvfs st;
	gint limit;

	limit = tracker_config_get_low_disk_space_limit (indexer->private->config);
	path = indexer->private->db_dir;

	if (limit < 1) {
		return FALSE;
	}

	if (statvfs (path, &st) == -1) {
		g_warning ("Could not statvfs '%s'", path);
		return FALSE;
	}

	if (((long long) st.f_bavail * 100 / st.f_blocks) <= limit) {
		g_warning ("Disk space is low");
		return TRUE;
	}

	return FALSE;
}

static gboolean
check_disk_space_cb (TrackerIndexer *indexer)
{
	gboolean disk_space_low;

	disk_space_low = check_is_disk_space_low (indexer);

	if (disk_space_low) {
		state_set_flags (indexer, TRACKER_INDEXER_STATE_DISK_FULL);
	} else {
		state_unset_flags (indexer, TRACKER_INDEXER_STATE_DISK_FULL);
	}

	return TRUE;
}

static void
check_disk_space_start (TrackerIndexer *indexer)
{
	TrackerIndexerPrivate *priv;
	gint low_disk_space_limit;

	priv = indexer->private;

	if (priv->disk_space_check_id != 0) {
		return;
	}

	low_disk_space_limit = tracker_config_get_low_disk_space_limit (priv->config);

	if (low_disk_space_limit != -1) {
		priv->disk_space_check_id = g_timeout_add_seconds (LOW_DISK_CHECK_FREQUENCY,
								   (GSourceFunc) check_disk_space_cb,
								   indexer);
	}
}

static void
check_disk_space_stop (TrackerIndexer *indexer)
{
	TrackerIndexerPrivate *priv;

	priv = indexer->private;

	if (priv->disk_space_check_id != 0) {
		g_source_remove (priv->disk_space_check_id);
		priv->disk_space_check_id = 0;
	}
}

static gboolean
signal_status_cb (TrackerIndexer *indexer)
{
	signal_status (indexer, "status update");
	return TRUE;
}

static void
signal_status_timeout_start (TrackerIndexer *indexer)
{
	TrackerIndexerPrivate *priv;

	priv = indexer->private;

	if (priv->signal_status_id == 0) {
		priv->signal_status_id = g_timeout_add_seconds (SIGNAL_STATUS_FREQUENCY,
								(GSourceFunc) signal_status_cb,
								indexer);
	}
}

static void
signal_status_timeout_stop (TrackerIndexer *indexer)
{
	TrackerIndexerPrivate *priv;

	priv = indexer->private;

	if (priv->signal_status_id != 0) {
		g_source_remove (priv->signal_status_id);
		priv->signal_status_id = 0;
	}
}

static void
tracker_indexer_load_modules (TrackerIndexer *indexer)
{
	TrackerIndexerPrivate *priv;
	GSList *disabled_modules;
	GList *modules, *l;

	priv = indexer->private;
	priv->indexer_modules = g_hash_table_new (g_str_hash, g_str_equal);

	disabled_modules = tracker_config_get_disabled_modules (priv->config);
	modules = tracker_module_config_get_modules ();

	for (l = modules; l; l = l->next) {
		TrackerIndexerModule *module;

		if (!tracker_module_config_get_enabled (l->data)) {
			continue;
		}

		if (tracker_string_in_gslist (l->data, disabled_modules)) {
			continue;
		}

		module = tracker_indexer_module_get (l->data);

		if (module) {
			g_hash_table_insert (priv->indexer_modules,
					     l->data, module);

			priv->module_names = g_list_prepend (priv->module_names,
							     g_strdup (l->data));
			g_quark_from_string (l->data);
		}
	}

	g_list_free (modules);
}

static void
tracker_indexer_init (TrackerIndexer *indexer)
{
	TrackerIndexerPrivate *priv;
	TrackerDBIndex *index;
	GList *l;

	priv = indexer->private = TRACKER_INDEXER_GET_PRIVATE (indexer);

	/* NOTE: We set this to stopped because it is likely the
	 * daemon sends a request for something other than to check
	 * files initially and we don't want to signal finished and
	 * have the process func in use with nothing in any of the
	 * queues. When we get files, this flag is unset.
	 */
	priv->state = TRACKER_INDEXER_STATE_STOPPED;

	priv->items_processed = 0;
	priv->in_transaction = FALSE;

	priv->dir_queue = g_queue_new ();
	priv->file_queue = g_queue_new ();

	priv->mtime_cache = g_hash_table_new_full (g_str_hash,
						   g_str_equal,
						   g_free,
						   NULL);
	priv->modules_queue = g_queue_new ();
	priv->config = tracker_config_new ();

#ifdef HAVE_HAL
	priv->hal = tracker_hal_new ();

	g_signal_connect (priv->hal, "notify::battery-in-use",
			  G_CALLBACK (notify_battery_in_use_cb),
			  indexer);
	g_signal_connect (priv->hal, "notify::battery-percentage",
			  G_CALLBACK (notify_battery_percentage_cb),
			  indexer);

	set_up_throttle (indexer);
#endif /* HAVE_HAL */

	priv->language = tracker_language_new (priv->config);

	priv->db_dir = g_build_filename (g_get_user_cache_dir (),
					 "tracker",
					 NULL);

	tracker_indexer_load_modules (indexer);

	/* Set up indexer */
	index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_FILE);
	priv->file_index = g_object_ref (index);

	index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_EMAIL);
	priv->email_index = g_object_ref (index);

	/* Set up databases, these pointers are mostly used to
	 * start/stop transactions, since TrackerDBManager treats
	 * interfaces as singletons, it's safe to just ask it
	 * again for an interface.
	 */
	priv->cache = tracker_db_manager_get_db_interface (TRACKER_DB_CACHE);
	priv->common = tracker_db_manager_get_db_interface (TRACKER_DB_COMMON);
	priv->file_metadata = tracker_db_manager_get_db_interface (TRACKER_DB_FILE_METADATA);
	priv->file_contents = tracker_db_manager_get_db_interface (TRACKER_DB_FILE_CONTENTS);
	priv->email_metadata = tracker_db_manager_get_db_interface (TRACKER_DB_EMAIL_METADATA);
	priv->email_contents = tracker_db_manager_get_db_interface (TRACKER_DB_EMAIL_CONTENTS);

	/* Set up timer to know how long the process will take and took */
	priv->timer = g_timer_new ();

	/* Set up idle handler to process files/directories */
	state_check (indexer);
}

static void
add_file (TrackerIndexer *indexer,
	  PathInfo *info)
{
	g_queue_push_tail (indexer->private->file_queue, info);

	/* Make sure we are still running */
	check_started (indexer);
}

static void
add_directory (TrackerIndexer *indexer,
	       PathInfo *info)
{
	g_queue_push_tail (indexer->private->dir_queue, info);

	/* Make sure we are still running */
	check_started (indexer);
}

static void
index_metadata_item (TrackerField	 *field,
		     const gchar	 *value,
		     MetadataForeachData *data)
{
	TrackerDBIndex *index;
	gchar *parsed_value;
	gchar **arr;
	gint service_id;
	gint i;
	gint score;

	parsed_value = tracker_parser_text_to_string (value,
						      data->language,
						      tracker_config_get_max_word_length (data->config),
						      tracker_config_get_min_word_length (data->config),
						      tracker_field_get_filtered (field),
						      tracker_field_get_filtered (field),
						      tracker_field_get_delimited (field));

	if (!parsed_value) {
		return;
	}

	if (data->add) {
		score = tracker_field_get_weight (field);
	} else {
		score = -1 * tracker_field_get_weight (field);
	}

	arr = g_strsplit (parsed_value, " ", -1);
	service_id = tracker_service_get_id (data->service);
	index = tracker_db_index_manager_get_index_by_service_id (service_id);

	for (i = 0; arr[i]; i++) {
		tracker_db_index_add_word (index,
					   arr[i],
					   data->id,
					   tracker_service_get_id (data->service),
					   score);
	}

	if (data->add) {
		tracker_data_update_set_metadata (data->service, data->id, field, (gchar *) value, parsed_value);
	} else {
		tracker_data_update_delete_metadata (data->service, data->id, field, (gchar *)value);
	}

	g_strfreev (arr);
	g_free (parsed_value);
}

static void
index_metadata_foreach (TrackerField *field,
			gpointer      value,
			gpointer      user_data)
{
	MetadataForeachData *data;
	gint throttle;

	if (!value) {
		return;
	}

	data = (MetadataForeachData *) user_data;

	/* Throttle indexer, value 9 is from older code, why 9? */
	throttle = tracker_config_get_throttle (data->config);
	if (throttle > 9) {
		tracker_throttle (data->config, throttle * 100);
	}

	if (!tracker_field_get_multiple_values (field)) {
		index_metadata_item (field, value, data);
	} else {
		GList *list;

		list = value;

		while (list) {
			index_metadata_item (field, list->data, data);
			list = list->next;
		}
	}
}

static void
index_metadata (TrackerIndexer	      *indexer,
		guint32		       id,
		TrackerService	      *service,
		TrackerModuleMetadata *metadata)
{
	MetadataForeachData data;

	data.language = indexer->private->language;
	data.config = indexer->private->config;
	data.service = service;
	data.id = id;
	data.add = TRUE;

	tracker_module_metadata_foreach (metadata, index_metadata_foreach, &data);

	schedule_flush (indexer, FALSE);
}

static void
unindex_metadata (TrackerIndexer      *indexer,
		  guint32	       id,
		  TrackerService      *service,
		  TrackerDataMetadata *metadata)
{
	MetadataForeachData data;

	data.language = indexer->private->language;
	data.config = indexer->private->config;
	data.service = service;
	data.id = id;
	data.add = FALSE;

	tracker_data_metadata_foreach (metadata, index_metadata_foreach, &data);

	schedule_flush (indexer, FALSE);
}


static void
send_text_to_index (TrackerIndexer *indexer,
		    gint	    service_id,
		    gint	    service_type,
		    const gchar    *text,
		    gboolean	    full_parsing,
		    gint	    weight_factor)
{
	TrackerDBIndex *index;
	GHashTable     *parsed;
	GHashTableIter	iter;
	gpointer	key, value;

	if (!text) {
		return;
	}

	if (full_parsing) {
		parsed = tracker_parser_text (NULL,
					      text,
					      weight_factor,
					      indexer->private->language,
					      tracker_config_get_max_words_to_index (indexer->private->config),
					      tracker_config_get_max_word_length (indexer->private->config),
					      tracker_config_get_min_word_length (indexer->private->config),
					      tracker_config_get_enable_stemmer (indexer->private->config),
					      FALSE);
	} else {
		/* We dont know the exact property weight.
		   Big value works.
		 */
		parsed = tracker_parser_text_fast (NULL,
						   text,
						   weight_factor);
	}

	g_hash_table_iter_init (&iter, parsed);

	index = tracker_db_index_manager_get_index_by_service_id (service_type);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		tracker_db_index_add_word (index,
					   key,
					   service_id,
					   service_type,
					   GPOINTER_TO_INT (value));
	}

	g_hash_table_unref (parsed);
}

static void
index_text_with_parsing (TrackerIndexer *indexer,
			 gint		 service_id,
			 gint		 service_type_id,
			 const gchar	*content,
			 gint		 weight_factor)
{
	send_text_to_index (indexer,
			    service_id,
			    service_type_id,
			    content,
			    TRUE,
			    weight_factor);
}

static void
unindex_text_with_parsing (TrackerIndexer *indexer,
			   gint		   service_id,
			   gint		   service_type_id,
			   const gchar	  *content,
			   gint		   weight_factor)
{
	send_text_to_index (indexer,
			    service_id,
			    service_type_id,
			    content,
			    TRUE,
			    weight_factor * -1);
}

static void
index_text_no_parsing (TrackerIndexer *indexer,
		       gint	       service_id,
		       gint	       service_type_id,
		       const gchar    *content,
		       gchar	       weight_factor)
{
	send_text_to_index (indexer,
			    service_id,
			    service_type_id,
			    content,
			    FALSE,
			    weight_factor);
}

static void
unindex_text_no_parsing (TrackerIndexer *indexer,
			 gint		 service_id,
			 gint		 service_type_id,
			 const gchar	*content,
			 gint		 weight_factor)
{
	send_text_to_index (indexer,
			    service_id,
			    service_type_id,
			    content,
			    FALSE,
			    weight_factor * -1);
}

static void
update_word_foreach (gpointer key,
		     gpointer value,
		     gpointer user_data)
{
	TrackerDBIndex	       *index;
	UpdateWordsForeachData *data;
	gchar		       *word;
	gint			score;

	word = key;
	score = GPOINTER_TO_INT (value);

	data = user_data;

	index = tracker_db_index_manager_get_index_by_service_id (data->service_type_id);

	tracker_db_index_add_word (index,
				   word,
				   data->service_id,
				   data->service_type_id,
				   score);
}

static void
update_words_no_parsing (TrackerIndexer *indexer,
			 gint		 service_id,
			 gint		 service_type_id,
			 GHashTable	*words)
{
	UpdateWordsForeachData user_data;

	user_data.service_id = service_id;
	user_data.service_type_id = service_type_id;

	g_hash_table_foreach (words, update_word_foreach, &user_data);
}

static void
merge_word_table (gpointer key,
		  gpointer value,
		  gpointer user_data)
{
	GHashTable *new_table;
	gpointer    k;
	gpointer    v;
	gchar	   *word;
	gint	    new_score;

	word = key;
	new_score = GPOINTER_TO_INT (value);
	new_table = user_data;

	if (g_hash_table_lookup_extended (new_table, word, &k, &v)) {
		gint old_score;
		gint calculated_score;

		old_score = GPOINTER_TO_INT (v);
		calculated_score = old_score - new_score;

		if (calculated_score != 0) {
			g_hash_table_insert (new_table,
					     g_strdup (word),
					     GINT_TO_POINTER (calculated_score));
		} else {
			/* The word is the same in old and new text */
			g_hash_table_remove (new_table, word);
		}
	} else {
		g_hash_table_insert (new_table,
				     g_strdup (word),
				     GINT_TO_POINTER (0 - new_score));
	}
}

static void
item_update_content (TrackerIndexer *indexer,
		     TrackerService *service,
		     guint32	     id,
		     const gchar    *old_text,
		     const gchar    *new_text)
{
	GHashTable *old_words;
	GHashTable *new_words;

	if (!old_text && !new_text) {
		return;
	}

	/* Service has/had full text */
	old_words = tracker_parser_text (NULL,
					 old_text,
					 1,
					 indexer->private->language,
					 tracker_config_get_max_words_to_index (indexer->private->config),
					 tracker_config_get_max_word_length (indexer->private->config),
					 tracker_config_get_min_word_length (indexer->private->config),
					 tracker_config_get_enable_stemmer (indexer->private->config),
					 FALSE);

	new_words = tracker_parser_text (NULL,
					 new_text,
					 1,
					 indexer->private->language,
					 tracker_config_get_max_words_to_index (indexer->private->config),
					 tracker_config_get_max_word_length (indexer->private->config),
					 tracker_config_get_min_word_length (indexer->private->config),
					 tracker_config_get_enable_stemmer (indexer->private->config),
					 FALSE);

	/* Merge the score of the words from one and
	 * other file new_table contains the words
	 * with the updated scores
	 */
	g_hash_table_foreach (old_words, merge_word_table, new_words);

	update_words_no_parsing (indexer,
				 id,
				 tracker_service_get_id (service),
				 new_words);

	/* Remove old text and set new one in the db */
	if (old_text) {
		tracker_data_update_delete_content (service, id);
	}

	if (new_text) {
		tracker_data_update_set_content (service, id, new_text);
	}

	g_hash_table_unref (old_words);
	g_hash_table_unref (new_words);
}

static TrackerService *
get_service_for_file (TrackerModuleFile    *file,
		      TrackerIndexerModule *module)
{
	TrackerService *service;
	const gchar *service_type;

	service_type = tracker_module_file_get_service_type (file);

	if (!service_type) {
		service_type = tracker_module_config_get_index_service (module->name);
	}

	if (!service_type) {
		return NULL;
	}

	return tracker_ontology_get_service_by_name (service_type);
}

static gboolean
remove_existing_non_emb_metadata (TrackerField *field,
				  gpointer      value,
				  gpointer      user_data)
{
	TrackerDataMetadata *old_metadata = (TrackerDataMetadata *) user_data;
	const gchar *name;

	if (tracker_field_get_embedded (field)) {
		return FALSE;
	}

	name = tracker_field_get_name (field);

	if (tracker_field_get_multiple_values (field)) {
		return (tracker_data_metadata_lookup_values (old_metadata, name) != NULL);
	} else {
		return (tracker_data_metadata_lookup (old_metadata, name) != NULL);
	}
}

static void
item_add_or_update (TrackerIndexer        *indexer,
		    PathInfo              *info,
		    const gchar           *dirname,
		    const gchar           *basename,
		    TrackerModuleMetadata *metadata)
{
	TrackerService *service;
	gchar *text;
	guint32 id;

	service = get_service_for_file (info->module_file, info->module);

	if (!service) {
		return;
	}

	if (tracker_data_query_service_exists (service, dirname, basename, &id, NULL)) {
		TrackerDataMetadata *old_metadata_emb, *old_metadata_non_emb;
		gchar *old_text;
		gchar *new_text;

		/* Update case */
		g_debug ("Updating item '%s/%s'", 
			 dirname, 
			 basename);

		/*
		 * "metadata" (new metadata) contains embedded props and can contain
		 * non-embedded properties with default values! Dont overwrite those 
		 * in the DB if they already has a value.
		 * 
		 * 1) Remove all old embedded metadata from index and DB
		 * 2) Remove from new metadata all non embedded properties that already have value.
		 * 3) Save the remain new metadata.
		 */
		old_metadata_emb = tracker_data_query_metadata (service, id, TRUE);
		old_metadata_non_emb = tracker_data_query_metadata (service, id, FALSE);

		unindex_metadata (indexer, id, service, old_metadata_emb);

		tracker_module_metadata_foreach_remove (metadata,
							remove_existing_non_emb_metadata,
							old_metadata_non_emb);

		index_metadata (indexer, id, service, metadata);

		/* Take the old text -> the new one, calculate
		 * difference and add the words.
		 */
		old_text = tracker_data_query_content (service, id);
		new_text = tracker_module_file_get_text (info->module_file);

		item_update_content (indexer, service, id, old_text, new_text);

		g_free (old_text);
		g_free (new_text);
		tracker_data_metadata_free (old_metadata_emb);
		tracker_data_metadata_free (old_metadata_non_emb);
	} else {
		GHashTable *data;

		g_debug ("Adding item '%s/%s'",
			 dirname,
			 basename);

		/* Service wasn't previously indexed */
		id = tracker_data_update_get_new_service_id (indexer->private->common);
		data = tracker_module_metadata_get_hash_table (metadata);

		tracker_data_update_create_service (service,
						    id,
						    dirname,
						    basename,
						    data);

		tracker_data_update_create_event (indexer->private->cache, id, "Create");
		tracker_data_update_increment_stats (indexer->private->common, service);

		index_metadata (indexer, id, service, metadata);

		text = tracker_module_file_get_text (info->module_file);

		if (text) {
			/* Save in the index */
			index_text_with_parsing (indexer,
						 id,
						 tracker_service_get_id (service),
						 text,
						 1);

			/* Save in the DB */
			tracker_data_update_set_content (service, id, text);
			g_free (text);
		}

		g_hash_table_destroy (data);
	}
}

static gboolean 
filter_invalid_after_move_properties (TrackerField *field,
				      gpointer value,
				      gpointer user_data) 
{
	return g_strcmp0 (tracker_field_get_name (field), METADATA_FILE_NAME_DELIMITED) 
		&& g_strcmp0 (tracker_field_get_name (field), METADATA_FILE_NAME)
		&& g_strcmp0 (tracker_field_get_name (field), METADATA_FILE_PATH)
		&& g_strcmp0 (tracker_field_get_name (field), METADATA_FILE_EXT);
}

static void
item_move (TrackerIndexer  *indexer,
	   PathInfo	   *info,
	   const gchar	   *dirname,
	   const gchar	   *basename)
{
	TrackerService *service;
	TrackerDataMetadata *old_metadata;
	TrackerModuleMetadata *new_metadata;
	gchar *service_type;
	gchar *new_path, *new_name, *ext;
	GFile *file, *other_file;
	gchar *path, *other_path;
	gchar *uri, *other_uri, *mime_type;
	guint32 service_id;

	service = get_service_for_file (info->other_module_file, info->module);

	if (!service) {
		return;
	}

	path = g_file_get_path (info->file);
	other_path = g_file_get_path (info->other_file);

	g_debug ("Moving item from '%s' to '%s'", path, other_path);

	/* Get 'source' ID */
	if (!tracker_data_query_service_exists (service,
						dirname,
						basename,
						&service_id,
						NULL)) {
		g_message ("Source file '%s' not found in database to move", path);

		g_free (path);
		g_free (other_path);

		return;
	}

	/* Get mime type and move thumbnail from thumbnailerd */
	old_metadata = tracker_data_query_metadata (service, service_id, TRUE);

	if (old_metadata) {
		GFile *file;
		const gchar *mime_type;
		gchar *uri;

		/* TODO URI branch: this is a URI conversion */
		uri = g_file_get_uri (info->file);
		other_uri = g_file_get_uri (info->other_file);

		mime_type = tracker_data_metadata_lookup (old_metadata, "File:Mime");
		tracker_thumbnailer_move (uri, mime_type, other_uri);
		
		g_free (other_uri);
		g_free (uri);
	} else {
		g_message ("Could not get mime type to remove thumbnail for:'%s'",
			   path);
	}

	tracker_data_update_move_service (service, path, other_path);

	/*
	 *  Updating what changes in move event (Path related properties)
	 */
	tracker_data_metadata_foreach_remove (old_metadata,
					      filter_invalid_after_move_properties,
					      NULL);

	unindex_metadata (indexer, service_id, service, old_metadata);

	new_metadata = tracker_module_metadata_new ();

	tracker_file_get_path_and_name (other_path, &new_path, &new_name);

	tracker_module_metadata_add_string (new_metadata, METADATA_FILE_PATH, new_path);
	tracker_module_metadata_add_string (new_metadata, METADATA_FILE_NAME, new_name);
	tracker_module_metadata_add_string (new_metadata, METADATA_FILE_NAME_DELIMITED, other_path);

	g_free (new_path);
	g_free (new_name);

	ext = strrchr (other_path, '.');
	if (ext) {
		ext++;
		tracker_module_metadata_add_string (new_metadata, METADATA_FILE_EXT, ext);
	}

	index_metadata (indexer, service_id, service, new_metadata);

	tracker_data_metadata_free (old_metadata);
	g_object_unref (new_metadata);

	g_free (path);
	g_free (other_path);
}

static void
item_remove (TrackerIndexer *indexer,
	     PathInfo	    *info,
	     const gchar    *dirname,
	     const gchar    *basename)
{
	TrackerService *service;
	TrackerDataMetadata *data_metadata;
	gchar *content;
	gchar *metadata;
	gchar *path;
	const gchar *service_type;
	guint service_id, service_type_id;

	g_debug ("Removing item: '%s/%s' (no metadata was given by module)", 
		 dirname, 
		 basename);

	service_type = tracker_module_config_get_index_service (info->module->name);

	if (!service_type || !service_type[0]) {
		const gchar *name;

		/* The file is not anymore in the filesystem. Obtain
		 * the service type from the DB.
		 */
		service_type_id = tracker_data_query_service_type_id (dirname, basename);

		if (service_type_id == 0) {
			/* File didn't exist, nothing to delete */
			return;
		}

		name = tracker_ontology_get_service_by_id (service_type_id);
		service = tracker_ontology_get_service_by_name (name);
	} else {
		service = tracker_ontology_get_service_by_name (service_type);
		service_type_id = tracker_service_get_id (service);
	}

	tracker_data_query_service_exists (service, dirname, basename, &service_id, NULL);

	if (service_id < 1) {
		g_debug ("  File does not exist anyway "
			 "(dirname:'%s', basename:'%s')",
			 dirname, basename);
		return;
	}

	/* This is needed in a few places. */
	path = g_build_path (G_DIR_SEPARATOR_S, dirname, basename, NULL);

	/* Get mime type and remove thumbnail from thumbnailerd */
	data_metadata = tracker_data_query_metadata (service, service_id, TRUE);

	if (data_metadata) {
		GFile *file;
		const gchar *mime_type;
		gchar *uri;

		/* TODO URI branch: this is a URI conversion */
		file = g_file_new_for_path (path);
		uri = g_file_get_uri (file);
		g_object_unref (file);
		
		mime_type = tracker_data_metadata_lookup (data_metadata, "File:Mime");
		tracker_thumbnailer_remove (uri, mime_type);

		tracker_data_metadata_free (data_metadata);
		g_free (uri);
	} else {
		g_message ("Could not get mime type to remove thumbnail for:'%s'",
			   path);
	}

	/* Get content, unindex the words and delete the contents */
	content = tracker_data_query_content (service, service_id);
	if (content) {
		unindex_text_with_parsing (indexer,
					   service_id,
					   service_type_id,
					   content,
					   1);
		g_free (content);
		tracker_data_update_delete_content (service, service_id);
	}

	/* Get metadata from DB to remove it from the index */
	metadata = tracker_data_query_parsed_metadata (service,
						       service_id);
	unindex_text_no_parsing (indexer,
				 service_id,
				 service_type_id,
				 metadata,
				 1000);
	g_free (metadata);

	/* The weight depends on metadata, but a number high enough
	 * force deletion.
	 */
	metadata = tracker_data_query_unparsed_metadata (service,
							 service_id);
	unindex_text_with_parsing (indexer,
				   service_id,
				   service_type_id,
				   metadata,
				   1000);
	g_free (metadata);

	/* Delete service */
	tracker_data_update_delete_service (service, service_id);
	tracker_data_update_delete_all_metadata (service, service_id);

	if (strcmp (service_type, "Folders") == 0) {
		tracker_data_update_delete_service_recursively (service, path);
	}

	tracker_data_update_decrement_stats (indexer->private->common, service);

	g_free (path);
}

/*
 * TODO: Check how are we using this functions. 
 *       I think 99% of the time "values" has only 1 element.
 */
static gboolean
handle_metadata_add (TrackerIndexer *indexer,
		     const gchar    *service_type,
		     const gchar    *uri,
		     const gchar    *property,
		     GStrv	     values,
		     GError	   **error)
{
	TrackerService *service;
	TrackerField   *field;
	guint           service_id, i, j = 0;
	gchar         **setted_values;
	gchar          *joined, *dirname = NULL, *basename = NULL;
	gchar         **old_contents;
	gint            len;

	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	check_started (indexer);

	service = tracker_ontology_get_service_by_name (service_type);
	if (!service) {
		g_set_error (error,
			     g_quark_from_string (TRACKER_INDEXER_ERROR),
			     TRACKER_INDEXER_ERROR_CODE,
			     "Unknown service type: '%s'",
			     service_type);
		return FALSE;
	}

	field = tracker_ontology_get_field_by_name (property);
	if (!field) {
		g_set_error (error,
			     g_quark_from_string (TRACKER_INDEXER_ERROR),
			     TRACKER_INDEXER_ERROR_CODE,
			     "Unknown field type: '%s'",
			     property);
		return FALSE;
	}

	if (tracker_field_get_embedded (field)) {
		g_set_error (error,
			     g_quark_from_string (TRACKER_INDEXER_ERROR),
			     TRACKER_INDEXER_ERROR_CODE,
			     "Field type: '%s' is embedded and not writable",
			     property);
		return FALSE;
	}

	len = g_strv_length (values);

	if (!tracker_field_get_multiple_values (field) && len > 1) {
		g_set_error (error,
			     g_quark_from_string (TRACKER_INDEXER_ERROR),
			     TRACKER_INDEXER_ERROR_CODE,
			     "Field type: '%s' doesnt support multiple values (trying to set %d)",
			     property,
			     len);
		return FALSE;
	}

	tracker_file_get_path_and_name (uri, &dirname, &basename);

	tracker_data_query_service_exists (service,
				  dirname,
				  basename,
				  &service_id,
				  NULL);
	g_free (dirname);
	g_free (basename);

	if (service_id < 1) {
		g_set_error (error,
			     g_quark_from_string (TRACKER_INDEXER_ERROR),
			     TRACKER_INDEXER_ERROR_CODE,
			     "File '%s' doesnt exist in the DB", uri);
		return FALSE;
	}

	old_contents = tracker_data_query_metadata_field_values (service,
						       service_id,
						       field);
	if (!tracker_field_get_multiple_values (field) && old_contents) {
		/* Remove old value from DB and index */
		len = g_strv_length (old_contents);

		if (old_contents && len > 1) {
			g_critical ("Seems to be multiple values in field:'%s' that doesn allow that",
				    tracker_field_get_name (field));
		} else if (old_contents && len == 1) {
			if (tracker_field_get_filtered (field)) {
				unindex_text_with_parsing (indexer,
							   service_id,
							   tracker_service_get_id (service),
							   old_contents[0],
							   tracker_field_get_weight (field));
			} else {
				unindex_text_no_parsing (indexer,
							 service_id,
							 tracker_service_get_id (service),
							 old_contents[0],
							 tracker_field_get_weight (field));
			}
			tracker_data_update_delete_metadata (service, service_id, field, old_contents[0]);
		}
	}

	setted_values = g_new0 (gchar *, g_strv_length (values));

	for (i = 0, j = 0; values[i] != NULL; i++) {
		g_debug ("Setting metadata: service_type '%s' id '%d' field '%s' value '%s'",
			 tracker_service_get_name (service),
			 service_id,
			 tracker_field_get_name (field),
			 values[i]);

		if (tracker_field_get_multiple_values (field) 
		    && (tracker_string_in_string_list (values[i], old_contents) > -1) ) {
			continue;
		}

		tracker_data_update_set_metadata (service, service_id, field, values[i], NULL);
		setted_values [j++] = values[i];
	}
	setted_values [j] = NULL;
	
	joined = g_strjoinv (" ", setted_values);
	if (tracker_field_get_filtered (field)) {
		index_text_no_parsing (indexer,
				       service_id,
				       tracker_service_get_id (service),
				       joined,
				       tracker_field_get_weight (field));
	} else {
		index_text_with_parsing (indexer,
					 service_id,
					 tracker_service_get_id (service),
					 joined,
					 tracker_field_get_weight (field));
	}

	if (old_contents) {
		g_strfreev (old_contents);
	}

	/* Not g_strfreev because. It contains the pointers of "values"! */
	g_free (setted_values);
	g_free (joined);

	return TRUE;
}

static gboolean
handle_metadata_remove (TrackerIndexer *indexer,
			const gchar    *service_type,
			const gchar    *uri,
			const gchar    *property,
			GStrv		values,
			GError	      **error)
{
	TrackerService *service;
	TrackerField *field;
	guint service_id, i;
	gchar *joined = NULL, *dirname = NULL, *basename = NULL;

	check_started (indexer);

	service = tracker_ontology_get_service_by_name (service_type);
	if (!service) {
		g_set_error (error,
			     g_quark_from_string (TRACKER_INDEXER_ERROR),
			     TRACKER_INDEXER_ERROR_CODE,
			     "Unknown service type: '%s'",
			     service_type);
		return FALSE;
	}

	field = tracker_ontology_get_field_by_name (property);
	if (!field) {
		g_set_error (error,
			     g_quark_from_string (TRACKER_INDEXER_ERROR),
			     TRACKER_INDEXER_ERROR_CODE,
			     "Unknown field type: '%s'",
			     property);
		return FALSE;
	}

	if (tracker_field_get_embedded (field)) {
		g_set_error (error,
			     g_quark_from_string (TRACKER_INDEXER_ERROR),
			     TRACKER_INDEXER_ERROR_CODE,
			     "Field type: '%s' is embedded and cannot be deleted",
			     property);
		return FALSE;
	}

	tracker_file_get_path_and_name (uri, &dirname, &basename);

	tracker_data_query_service_exists (service, dirname, basename, &service_id, NULL);

	g_free (dirname);
	g_free (basename);

	if (service_id < 1) {
		g_set_error (error,
			     g_quark_from_string (TRACKER_INDEXER_ERROR),
			     TRACKER_INDEXER_ERROR_CODE,
			     "File '%s' doesnt exist in the DB", uri);
		return FALSE;
	}

	/* If we receive concrete values, we delete those rows in the
	 * db. Otherwise, retrieve the old values of the property and
	 * remove all their instances for the file
	 */
	if (g_strv_length (values) > 0) {
		for (i = 0; values[i] != NULL; i++) {
			tracker_data_update_delete_metadata (service,
						    service_id,
						    field,
						    values[i]);
		}
		joined = g_strjoinv (" ", values);
	} else {
		gchar **old_contents;

		old_contents = tracker_data_query_metadata_field_values (service,
							       service_id,
							       field);
		if (old_contents) {
			tracker_data_update_delete_metadata (service,
						    service_id,
						    field,
						    NULL);

			joined = g_strjoinv (" ", old_contents);
			g_strfreev (old_contents);
		}
	}

	/* Now joined contains the words to unindex */
	if (tracker_field_get_filtered (field)) {
		unindex_text_with_parsing (indexer,
					   service_id,
					   tracker_service_get_id (service),
					   joined,
					   tracker_field_get_weight (field));
	} else {
		unindex_text_no_parsing (indexer,
					 service_id,
					 tracker_service_get_id (service),
					 joined,
					 tracker_field_get_weight (field));
	}

	g_free (joined);

	return TRUE;
}

static gboolean
should_index_file (TrackerIndexer *indexer,
		   PathInfo	  *info,
		   const gchar	  *dirname,
		   const gchar	  *basename)
{
	TrackerService *service;
	gchar *path;
	const gchar *str;
	gboolean is_dir;
	gboolean should_be_cached;
	struct stat st;
	time_t mtime;

	service = get_service_for_file (info->module_file, info->module);

	if (!service) {
		return TRUE;
	}

	/* Check the file/directory exists. If it doesn't we
	 * definitely want to index it.
	 */
	if (!tracker_data_query_service_exists (service,
				       dirname,
				       basename,
				       NULL,
				       &mtime)) {
		return TRUE;
	}

	/* So, if we are here, then the file or directory DID exist
	 * in the database already. Now we need to check if the
	 * parent directory mtime matches the mtime we have for it in
	 * the database. If it does, then we can ignore any files
	 * immediately in this parent directory.
	 */
	path = g_file_get_path (info->file);

	if (g_lstat (path, &st) == -1) {
		g_free (path);
		return TRUE;
	}

	/* It is most efficient to keep a hash table of mtime values
	 * which are out of sync with the database. Since there are
	 * more likely to be less of those than actual directories.
	 * So we keep a list of them and if the dirname is in the
	 * hash table we then say it needs reindexing. If not, we
	 * assume that it must be up to date.
	 *
	 * This entry in the hash table is removed after each index is
	 * complete.
	 *
	 * Note: info->file->path = '/tmp/foo/bar'
	 *	 dirname	  = '/tmp/foo'
	 *	 basename	  = 'bar'
	 *
	 * Example A. PathInfo is file.
	 *   1) Lookup 'dirname', if exists then
	 *     --> return TRUE
	 *   2) Check 'dirname' mtime is newer, if not then
	 *     --> return FALSE
	 *   3) Add to hash table
	 *     --> return TRUE
	 *
	 * Example B. PathInfo is directory.
	 *   1) Lookup 'info->file->path', if exists then
	 *     --> return TRUE
	 *   2) Check 'info->file->path' mtime is newer, if not then
	 *     --> return FALSE
	 *   3) Add to hash table
	 *     --> return TRUE
	 */
	is_dir = S_ISDIR (st.st_mode);
	should_be_cached = TRUE;

	/* Choose the path we evaluate based on if we have a directory
	 * or not. All operations are done using the same string.
	 */
	if (is_dir) {
		str = path;
	} else {
		str = dirname;
	}

	/* Step 1. */
	if (g_hash_table_lookup (indexer->private->mtime_cache, str)) {
		gboolean should_index;

		if (!is_dir) {
			/* Only index files in this directory which
			 * have an old mtime.
			 */
			should_index = st.st_mtime > mtime;
		} else {
			/* We always index directories */
			should_index = TRUE;
		}

		g_debug ("%s:'%s' exists in cache, %s",
			 is_dir ? "Path" : "Parent path",
			 str,
			 should_index ? "should index" : "should not index");

		g_free (path);

		return should_index;
	}

	/* Step 2. */
	if (!is_dir) {
		gchar *parent_dirname;
		gchar *parent_basename;
		gboolean exists;

		/* FIXME: What if there is no parent? */
		parent_dirname = g_path_get_dirname (dirname);
		parent_basename = g_path_get_basename (dirname);

		/* We don't have the mtime for the dirname yet, we do
		 * if this is a info->file->path of course.
		 */
		exists = tracker_data_query_service_exists (service,
						   parent_dirname,
						   parent_basename,
						   NULL,
						   &mtime);
		if (!exists) {
			g_message ("Expected path '%s/%s' to exist, not in database?",
				   parent_dirname,
				   parent_basename);

			g_free (parent_basename);
			g_free (parent_dirname);
			g_free (path);

			return TRUE;
		}

		if (g_lstat (dirname, &st) == -1) {
			g_message ("Expected path '%s' to exist, could not stat()",
				   parent_dirname);

			g_free (parent_basename);
			g_free (parent_dirname);
			g_free (path);

			return TRUE;
		}

		g_free (parent_basename);
		g_free (parent_dirname);
	}

	if (st.st_mtime <= mtime) {
		g_debug ("%s:'%s' has indifferent mtime and should not be indexed",
			 is_dir ? "Path" : "Parent path",
			 str);
		g_free (path);

		return FALSE;
	}

	/* Step 3. */
	g_debug ("%s:'%s' being added to cache and should be indexed",
		 is_dir ? "Path" : "Parent path",
		 str);

	g_hash_table_replace (indexer->private->mtime_cache,
			      g_strdup (str),
			      GINT_TO_POINTER (1));

	g_free (path);

	return TRUE;
}

static gboolean
process_file (TrackerIndexer *indexer,
	      PathInfo	     *info)
{
	TrackerModuleMetadata *metadata;
	gchar *uri, *dirname, *basename;

	/* Note: If info->other_file is set, the PathInfo is for a
	 * MOVE event not for normal file event.
	 */

	if (!info->module_file) {
		return TRUE;
	}

	/* Set the current module */
	indexer->private->current_module = g_quark_from_string (info->module->name);
	uri = tracker_module_file_get_uri (info->module_file);

	if (!uri) {
		if (TRACKER_IS_MODULE_ITERATABLE (info->module_file)) {
			return !tracker_module_iteratable_iter_contents (TRACKER_MODULE_ITERATABLE (info->module_file));
		}

		return TRUE;
	}

	tracker_file_get_path_and_name (uri, &dirname, &basename);
	g_free (uri);

	/*
	 * FIRST:
	 * ======
	 * We don't check if we should index files for MOVE events.
	 *
	 * SECOND:
	 * =======
	 * Here we do a quick check to see if this is an email URI or
	 * not. For emails we don't check if we should index them, we
	 * always do since it is ONLY summary files we index and we
	 * need to look at them to know if anything changed anyway.
	 * The only improvement we could have here is an mtime check.
	 * This is yet to do.
	 *
	 * The info->file->path is the REAL location. The dirname and
	 * basename which are returned by the module are combined to
	 * look like:
	 *
	 *   email://1192717939.16218.20@petunia/INBOX;uid=1
	 *
	 * We simply check the dirname[0] to make sure it isn't an
	 * email based dirname.
	 */
	if (G_LIKELY (!info->other_file) && dirname[0] == G_DIR_SEPARATOR) {
		if (!should_index_file (indexer, info, dirname, basename)) {
			gchar *path;

			path = g_file_get_path (info->file);

			g_debug ("File is already up to date: '%s'", path);

			g_free (dirname);
			g_free (basename);
			g_free (path);

			return TRUE;
		}
	}

	/* Sleep to throttle back indexing */
	tracker_throttle (indexer->private->config, 100);

	/* For normal files create or delete the item with the
	 * metadata. For move PathInfo we use the db function to move
	 * a service and set the metadata.
	 */
	if (G_UNLIKELY (info->other_file)) {
		item_move (indexer, info, dirname, basename);
	} else {
		metadata = tracker_module_file_get_metadata (info->module_file);

		if (metadata) {
			item_add_or_update (indexer, info, dirname, basename, metadata);
			g_object_unref (metadata);
		} else {
			item_remove (indexer, info, dirname, basename);
		}
	}

	indexer->private->items_processed++;

	g_free (dirname);
	g_free (basename);

	if (TRACKER_IS_MODULE_ITERATABLE (info->module_file)) {
		return !tracker_module_iteratable_iter_contents (TRACKER_MODULE_ITERATABLE (info->module_file));
	}

	return TRUE;
}

static void
process_directory (TrackerIndexer *indexer,
		   PathInfo *info,
		   gboolean recurse)
{
	gchar *path;
	const gchar *name;
	GDir *dir;

	path = g_file_get_path (info->file);

	/* FIXME: Use gio to iterate the directory */
	g_debug ("Processing directory:'%s'", path);

	dir = g_dir_open (path, 0, NULL);

	if (!dir) {
		g_free (path);
		return;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		PathInfo *new_info;
		GFile *child;
		gchar *child_path;

		if (name[0] == '.') {
			continue;
		}

		child = g_file_get_child (info->file, name);
		child_path = g_file_get_path (child);

		new_info = path_info_new (info->module, child, NULL);
		add_file (indexer, new_info);

		if (recurse && g_file_test (child_path, G_FILE_TEST_IS_DIR)) {
			new_info = path_info_new (info->module, child, NULL);
			add_directory (indexer, new_info);
		}

		g_object_unref (child);
		g_free (child_path);
	}

	g_dir_close (dir);
	g_free (path);
}

static void
process_module_emit_signals (TrackerIndexer *indexer,
			     const gchar *next_module_name)
{
	/* Signal the last module as finished */
	g_signal_emit (indexer, signals[MODULE_FINISHED], 0,
		       g_quark_to_string (indexer->private->current_module));

	/* Set current module */
	indexer->private->current_module = g_quark_from_string (next_module_name);

	/* Signal the next module as started */
	if (next_module_name) {
		g_signal_emit (indexer, signals[MODULE_STARTED], 0,
			       next_module_name);
	}
}

static void
process_module (TrackerIndexer *indexer,
		const gchar *module_name)
{
	TrackerIndexerModule *module;
	GList *dirs, *d;

	module = g_hash_table_lookup (indexer->private->indexer_modules, module_name);

	/* Signal module start/stop */
	process_module_emit_signals (indexer, module_name);

	if (!module) {
		/* No need to signal stopped here, we will get that
		 * signal the next time this function is called.
		 */
		g_message ("No module for:'%s'", module_name);
		return;
	}

	g_message ("Starting module:'%s'", module_name);

	dirs = tracker_module_config_get_monitor_recurse_directories (module_name);
	g_return_if_fail (dirs != NULL);

	for (d = dirs; d; d = d->next) {
		PathInfo *info;
		GFile *file;

		file = g_file_new_for_path (d->data);
		info = path_info_new (module, file, NULL);
		add_directory (indexer, info);

		g_object_unref (file);
	}

	g_list_free (dirs);
}

static gboolean
process_func (gpointer data)
{
	TrackerIndexer *indexer;
	PathInfo *path;

	indexer = TRACKER_INDEXER (data);

	indexer->private->in_process = TRUE;

	if (G_UNLIKELY (!indexer->private->in_transaction)) {
		start_transaction (indexer);
	}

	if ((path = g_queue_peek_head (indexer->private->file_queue)) != NULL) {
		/* Process file */
		if (process_file (indexer, path)) {
			indexer->private->files_processed++;
			path = g_queue_pop_head (indexer->private->file_queue);
			path_info_free (path);
		}
	} else if ((path = g_queue_pop_head (indexer->private->dir_queue)) != NULL) {
		/* Process directory contents */
		process_directory (indexer, path, TRUE);
		path_info_free (path);
	} else {
		gchar *module_name;

		/* Dirs/files queues are empty, process the next module */
		module_name = g_queue_pop_head (indexer->private->modules_queue);

		if (!module_name) {
			/* Signal the last module as finished */
			process_module_emit_signals (indexer, NULL);

			/* We are no longer processing, this is
			 * needed so that when we call
			 * check_stopped() it cleans up the idle
			 * handler and id.
			 */
			indexer->private->in_process = FALSE;

			/* Signal stopped and clean up */
			check_stopped (indexer, FALSE);
			check_disk_space_stop (indexer);

			return FALSE;
		}

		process_module (indexer, module_name);
		g_free (module_name);
	}

	if (indexer->private->items_processed > TRANSACTION_MAX) {
		schedule_flush (indexer, TRUE);
	}

	indexer->private->in_process = FALSE;

	if (indexer->private->state != 0) {
		/* Some flag has been set, meaning the idle function should stop */
		indexer->private->idle_id = 0;
		return FALSE;
	}

	return TRUE;
}

TrackerIndexer *
tracker_indexer_new (void)
{
	return g_object_new (TRACKER_TYPE_INDEXER, NULL);
}

gboolean
tracker_indexer_get_running (TrackerIndexer *indexer)
{
	TrackerIndexerState state;

	g_return_val_if_fail (TRACKER_IS_INDEXER (indexer), FALSE);

	state = indexer->private->state;

	return ((state & TRACKER_INDEXER_STATE_PAUSED) == 0 &&
		(state & TRACKER_INDEXER_STATE_STOPPED) == 0);
}

static void
state_check (TrackerIndexer *indexer)
{
	TrackerIndexerState state;

	state = indexer->private->state;

	if (state != 0) {
		signal_status_timeout_stop (indexer);
		stop_scheduled_flush (indexer);
		stop_transaction (indexer);

		/* Actually, we don't want to remove/add back the idle
		 * function if we're in the middle of processing one item,
		 * as we could end up with a detached idle function running
		 * plus a newly created one, instead we don't remove the source
		 * here and make the idle function return FALSE if any flag is set.
		 */
		if (indexer->private->idle_id &&
		    !indexer->private->in_process) {
			g_source_remove (indexer->private->idle_id);
			indexer->private->idle_id = 0;
		}
	} else {
		check_disk_space_start (indexer);
		signal_status_timeout_start (indexer);

		if (indexer->private->idle_id == 0) {
			indexer->private->idle_id = g_idle_add (process_func, indexer);
		}
	}
}

static void
state_set_flags (TrackerIndexer      *indexer,
		 TrackerIndexerState  state)
{
	guint old_state;

	old_state = indexer->private->state;
	indexer->private->state |= state;
	state_check (indexer);

	/* Just emit ::paused for the states that
	 * could be relevant outside the indexer
	 */
	if ((! (old_state & TRACKER_INDEXER_STATE_PAUSED)) &&
	    (! (old_state & TRACKER_INDEXER_STATE_DISK_FULL)) &&
	    (! (old_state & TRACKER_INDEXER_STATE_LOW_BATT)) &&
	    (state & TRACKER_INDEXER_STATE_PAUSED ||
	     state & TRACKER_INDEXER_STATE_DISK_FULL ||
	     state & TRACKER_INDEXER_STATE_LOW_BATT)) {
		const gchar *reason = NULL;

		if (state & TRACKER_INDEXER_STATE_DISK_FULL) {
			reason = "Disk full";
		} else if (state & TRACKER_INDEXER_STATE_LOW_BATT) {
			reason = "Battery low";
		}

		g_signal_emit (indexer, signals[PAUSED], 0, reason);
	}
}

static void
state_unset_flags (TrackerIndexer      *indexer,
		   TrackerIndexerState	state)
{
	guint old_state, new_state;

	old_state = indexer->private->state;
	indexer->private->state &= ~(state);
	new_state = indexer->private->state;
	state_check (indexer);

	if ((old_state & TRACKER_INDEXER_STATE_PAUSED ||
	     old_state & TRACKER_INDEXER_STATE_DISK_FULL ||
	     old_state & TRACKER_INDEXER_STATE_LOW_BATT) &&
	    (! (new_state & TRACKER_INDEXER_STATE_PAUSED)) &&
	    (! (new_state & TRACKER_INDEXER_STATE_DISK_FULL)) &&
	    (! (new_state & TRACKER_INDEXER_STATE_LOW_BATT))) {
		g_signal_emit (indexer, signals[CONTINUED], 0);
	}
}

void
tracker_indexer_set_running (TrackerIndexer *indexer,
			     gboolean	     running)
{
	TrackerIndexerState state;

	state = indexer->private->state;

	if (running && (state & TRACKER_INDEXER_STATE_PAUSED)) {
		state_unset_flags (indexer, TRACKER_INDEXER_STATE_PAUSED);

		tracker_db_index_set_paused (indexer->private->file_index, FALSE);
		tracker_db_index_set_paused (indexer->private->email_index, FALSE);
	} else if (!running && !(state & TRACKER_INDEXER_STATE_PAUSED)) {
		state_set_flags (indexer, TRACKER_INDEXER_STATE_PAUSED);

		tracker_db_index_set_paused (indexer->private->file_index, TRUE);
		tracker_db_index_set_paused (indexer->private->email_index, TRUE);
	}
}

void
tracker_indexer_stop (TrackerIndexer *indexer)
{
	g_return_if_fail (TRACKER_IS_INDEXER (indexer));

	check_stopped (indexer, TRUE);
}

void
tracker_indexer_pause (TrackerIndexer	      *indexer,
		       DBusGMethodInvocation  *context,
		       GError		     **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (TRACKER_IS_INDEXER (indexer), context);

	tracker_dbus_request_new (request_id,
				  "DBus request to pause the indexer");

	if (tracker_indexer_get_running (indexer)) {
		tracker_dbus_request_comment (request_id,
					      "Pausing indexing");

		tracker_indexer_set_running (indexer, FALSE);
	}

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

static gboolean
pause_for_duration_cb (gpointer user_data)
{
	TrackerIndexer *indexer;

	indexer = TRACKER_INDEXER (user_data);

	tracker_indexer_set_running (indexer, TRUE);
	indexer->private->pause_for_duration_id = 0;

	return FALSE;
}

void
tracker_indexer_pause_for_duration (TrackerIndexer	   *indexer,
				    guint		    seconds,
				    DBusGMethodInvocation  *context,
				    GError		  **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (TRACKER_IS_INDEXER (indexer), context);

	tracker_dbus_request_new (request_id,
				  "DBus request to pause the indexer for %d seconds",
				  seconds);

	if (tracker_indexer_get_running (indexer)) {
		if (indexer->private->in_transaction) {
			tracker_dbus_request_comment (request_id,
						      "Committing transactions");
		}

		tracker_dbus_request_comment (request_id,
					      "Pausing indexing");

		tracker_indexer_set_running (indexer, FALSE);

		indexer->private->pause_for_duration_id =
			g_timeout_add_seconds (seconds,
					       pause_for_duration_cb,
					       indexer);
	}

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

void
tracker_indexer_continue (TrackerIndexer	 *indexer,
			  DBusGMethodInvocation  *context,
			  GError		**error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (TRACKER_IS_INDEXER (indexer), context);

	tracker_dbus_request_new (request_id,
				  "DBus request to continue the indexer");

	if (tracker_indexer_get_running (indexer) == FALSE) {
		tracker_dbus_request_comment (request_id,
					      "Continuing indexing");

		tracker_indexer_set_running (indexer, TRUE);
	}

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

void
tracker_indexer_process_all (TrackerIndexer *indexer)
{
	GList *l;

	state_unset_flags (indexer, TRACKER_INDEXER_STATE_STOPPED);

	for (l = indexer->private->module_names; l; l = l->next) {
		g_queue_push_tail (indexer->private->modules_queue, g_strdup (l->data));
	}
}

void
tracker_indexer_process_modules (TrackerIndexer  *indexer,
				 gchar          **modules)
{
	GList *l;
	gint i;

	for (l = indexer->private->module_names; l; l = l->next) {
		for (i = 0; modules[i]; i++) {
			if (strcmp (l->data, modules[i]) == 0) {
				g_queue_push_tail (indexer->private->modules_queue, g_strdup (l->data));
			}
		}
	}
}

void
tracker_indexer_files_check (TrackerIndexer *indexer,
			     const gchar *module_name,
			     GStrv files,
			     DBusGMethodInvocation *context,
			     GError **error)
{
	TrackerIndexerModule *module;
	guint request_id;
	gint i;
	GError *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (TRACKER_IS_INDEXER (indexer), context);
	tracker_dbus_async_return_if_fail (files != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to check %d files",
				  g_strv_length (files));

	module = g_hash_table_lookup (indexer->private->indexer_modules, module_name);

	if (!module) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "The module '%s' is not loaded",
					     module_name);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	/* Add files to the queue */
	for (i = 0; files[i]; i++) {
		PathInfo *info;
		GFile *file;

		file = g_file_new_for_path (files[i]);
		info = path_info_new (module, file, NULL);
		add_file (indexer, info);

		g_object_unref (file);
	}

	dbus_g_method_return (context);
	tracker_dbus_request_success (request_id);
}

/* FIXME: Should get rid of this DBus method */
void
tracker_indexer_files_update (TrackerIndexer *indexer,
			      const gchar *module_name,
			      GStrv files,
			      DBusGMethodInvocation *context,
			      GError **error)
{
	tracker_indexer_files_check (indexer, module_name,
				     files, context, error);
}

/* FIXME: Should get rid of this DBus method */
void
tracker_indexer_files_delete (TrackerIndexer *indexer,
			      const gchar *module_name,
			      GStrv files,
			      DBusGMethodInvocation *context,
			      GError **error)
{
	tracker_indexer_files_check (indexer, module_name,
				     files, context, error);
}

void
tracker_indexer_file_move (TrackerIndexer	  *indexer,
			   const gchar		  *module_name,
			   gchar		  *from,
			   gchar		  *to,
			   DBusGMethodInvocation  *context,
			   GError		 **error)
{
	TrackerIndexerModule *module;
	guint request_id;
	GError *actual_error;
	PathInfo *info;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (TRACKER_IS_INDEXER (indexer), context);
	tracker_dbus_async_return_if_fail (from != NULL, context);
	tracker_dbus_async_return_if_fail (to != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to move '%s' to '%s'",
				  from, to);

	module = g_hash_table_lookup (indexer->private->indexer_modules, module_name);

	if (!module) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "The module '%s' is not loaded",
					     module_name);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	/* Add files to the queue */
	info = path_info_new (module,
			      g_file_new_for_path (from),
			      g_file_new_for_path (to));
	add_file (indexer, info);

	dbus_g_method_return (context);
	tracker_dbus_request_success (request_id);
}

void            
tracker_indexer_volume_disable_all (TrackerIndexer         *indexer,
				    DBusGMethodInvocation  *context,
				    GError                **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (TRACKER_IS_INDEXER (indexer), context);

	tracker_dbus_request_new (request_id,
				  "DBus request to disable all volumes");

	tracker_data_update_disable_all_volumes ();

	dbus_g_method_return (context);
	tracker_dbus_request_success (request_id);
}

void
tracker_indexer_volume_update_state (TrackerIndexer         *indexer,
				     const gchar            *volume_uuid,
				     const gchar            *path,
				     gboolean                enabled,
				     DBusGMethodInvocation  *context,
				     GError                **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (TRACKER_IS_INDEXER (indexer), context);
	tracker_dbus_async_return_if_fail (volume_uuid != NULL, context);
	tracker_dbus_async_return_if_fail (path != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to update volume "
				  "UUID:'%s', path:'%s', enabled:%s",
				  volume_uuid, 
				  path,
				  enabled ? "yes" : "no");

	if (enabled) {
		tracker_data_update_enable_volume (volume_uuid, path);
	} else {
		tracker_data_update_disable_volume (volume_uuid);
	}

	dbus_g_method_return (context);
	tracker_dbus_request_success (request_id);
}

void
tracker_indexer_property_set (TrackerIndexer	     *indexer,
			      const gchar	     *service_type,
			      const gchar	     *uri,
			      const gchar	     *property,
			      GStrv		      values,
			      DBusGMethodInvocation  *context,
			      GError		    **error)
{
	guint	request_id;
	GError *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (TRACKER_IS_INDEXER (indexer), context);
	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (uri != NULL, context);
	tracker_dbus_async_return_if_fail (property != NULL, context);
	tracker_dbus_async_return_if_fail (values != NULL, context);
	tracker_dbus_async_return_if_fail (g_strv_length (values) > 0, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to set %d values in property '%s' for file '%s' ",
				  g_strv_length (values),
				  property,
				  uri);

	if (!handle_metadata_add (indexer,
				  service_type,
				  uri,
				  property,
				  values,
				  &actual_error)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	schedule_flush (indexer, TRUE);

	dbus_g_method_return (context);
	tracker_dbus_request_success (request_id);
}

void
tracker_indexer_property_remove (TrackerIndexer		*indexer,
				 const gchar		*service_type,
				 const gchar		*uri,
				 const gchar		*property,
				 GStrv			 values,
				 DBusGMethodInvocation	*context,
				 GError		       **error)
{
	guint	request_id;
	GError *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (TRACKER_IS_INDEXER (indexer), context);
	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (uri != NULL, context);
	tracker_dbus_async_return_if_fail (property != NULL, context);
	tracker_dbus_async_return_if_fail (values != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to remove %d values in property '%s' for file '%s' ",
				  g_strv_length (values),
				  property,
				  uri);

	if (!handle_metadata_remove (indexer,
				     service_type,
				     uri,
				     property,
				     values,
				     &actual_error)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	dbus_g_method_return (context);
	tracker_dbus_request_success (request_id);
}

void
tracker_indexer_shutdown (TrackerIndexer	 *indexer,
			  DBusGMethodInvocation  *context,
			  GError		**error)
{
	guint request_id;

	tracker_dbus_async_return_if_fail (TRACKER_IS_INDEXER (indexer), context);

	request_id = tracker_dbus_get_next_request_id ();
	tracker_dbus_request_new (request_id,
				  "DBus request to shutdown the indexer");

	tracker_indexer_stop (indexer);

	dbus_g_method_return (context);
	tracker_dbus_request_success (request_id);
}
