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
#include <libtracker-data/tracker-data-search.h>
#include <libtracker-data/tracker-turtle.h>
#include <libtracker-data/tracker-data-backup.h>

#include "tracker-indexer.h"
#include "tracker-indexer-module.h"
#include "tracker-marshal.h"
#include "tracker-module-metadata-private.h"
#include "tracker-removable-device.h"

#define TRACKER_INDEXER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_INDEXER, TrackerIndexerPrivate))

#define FILES_REMAINING_THRESHOLD   10000
#define MAX_FLUSH_FREQUENCY         60
#define MIN_FLUSH_FREQUENCY         1

#define SIGNAL_STATUS_FREQUENCY     10

/* Throttle defaults */
#define THROTTLE_DEFAULT	    0
#define THROTTLE_DEFAULT_ON_BATTERY 5

#define TRACKER_INDEXER_ERROR	    "tracker-indexer-error-domain"
#define TRACKER_INDEXER_ERROR_CODE  0

/* Properties that change in move event */
#define METADATA_FILE_NAME_DELIMITED "File:NameDelimited"
#define METADATA_FILE_EXT	     "File:Ext"
#define METADATA_FILE_PATH	     "File:Path"
#define METADATA_FILE_NAME	     "File:Name"
#define METADATA_FILE_MIMETYPE       "File:Mime"

typedef struct PathInfo PathInfo;
typedef struct MetadataForeachData MetadataForeachData;
typedef struct MetadataRequest MetadataRequest;
typedef struct UpdateWordsForeachData UpdateWordsForeachData;
typedef enum TrackerIndexerState TrackerIndexerState;

struct TrackerIndexerPrivate {
	GQueue *dir_queue;
	GQueue *file_queue;
	GQueue *modules_queue;

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
	guint signal_status_id;
	guint flush_id;
	guint cleanup_task_id;

	guint items_indexed;
	guint items_processed;
	guint items_to_index;
	guint subelements_processed;

	guint in_transaction : 1;
	guint in_process : 1;
	guint interrupted : 1;

	guint state;
};

struct PathInfo {
	TrackerIndexerModule *module;
	GFile *file;
	GFile *source_file;
	TrackerModuleFile *module_file;
	TrackerModuleFile *source_module_file;
	gboolean recurse;
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
	TRACKER_INDEXER_STATE_INDEX_OVERLOADED = 1 << 0,
	TRACKER_INDEXER_STATE_PAUSED	= 1 << 1,
	TRACKER_INDEXER_STATE_STOPPED	= 1 << 2,
	TRACKER_INDEXER_STATE_CLEANUP   = 1 << 3
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
static gboolean cleanup_task_func      (gpointer             user_data);

static void	state_set_flags        (TrackerIndexer	    *indexer,
					TrackerIndexerState  state);
static void	state_unset_flags      (TrackerIndexer	    *indexer,
					TrackerIndexerState  state);
static void	state_check	       (TrackerIndexer	    *indexer);

static void     check_finished         (TrackerIndexer      *indexer,
					gboolean             interrupted);


static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (TrackerIndexer, tracker_indexer, G_TYPE_OBJECT)

static PathInfo *
path_info_new (TrackerIndexerModule *module,
	       GFile                *file,
	       GFile                *source_file,
	       gboolean              recurse)
{
	PathInfo *info;

	info = g_slice_new (PathInfo);
	info->module = module;

	info->file = g_object_ref (file);
	info->module_file = tracker_indexer_module_create_file (module, file);

	if (G_UNLIKELY (source_file)) {
		info->source_file = g_object_ref (source_file);
		info->source_module_file = tracker_indexer_module_create_file (module, source_file);
	} else {
		info->source_file = NULL;
		info->source_module_file = NULL;
	}

	info->recurse = recurse;

	return info;
}

static void
path_info_free (PathInfo *info)
{
	if (G_UNLIKELY (info->source_file)) {
		g_object_unref (info->source_file);
	}

	if (G_UNLIKELY (info->source_module_file)) {
		g_object_unref (info->source_module_file);
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

	indexer->private->in_transaction = FALSE;

	g_debug ("Transaction commit");
}

static void
signal_status (TrackerIndexer *indexer,
	       const gchar    *why)
{
	PathInfo *path;
	gdouble   seconds_elapsed;
	guint	  items_remaining;

	items_remaining = g_queue_get_length (indexer->private->file_queue);
	seconds_elapsed = g_timer_elapsed (indexer->private->timer, NULL);

	if ((path = g_queue_peek_head (indexer->private->file_queue)) != NULL) {
		if (TRACKER_IS_MODULE_ITERATABLE (path->module_file)) {
			guint count;

			count = tracker_module_iteratable_get_count (TRACKER_MODULE_ITERATABLE (path->module_file));
			items_remaining += count - indexer->private->subelements_processed;
		}
	}

	if (indexer->private->items_processed > 0 &&
	    items_remaining > 0) {
		gchar *str1;
		gchar *str2;

		str1 = tracker_seconds_estimate_to_string (seconds_elapsed,
							   TRUE,
							   indexer->private->items_indexed,
							   items_remaining);
		str2 = tracker_seconds_to_string (seconds_elapsed, TRUE);

		g_message ("Processed %d/%d, indexed %d, module:'%s', %s left, %s elapsed (%s)",
			   indexer->private->items_processed,
			   indexer->private->items_processed + items_remaining,
			   indexer->private->items_indexed,
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
		       indexer->private->items_processed,
		       indexer->private->items_indexed,
		       items_remaining);
}

static gboolean
flush_data (TrackerIndexer *indexer)
{
	indexer->private->flush_id = 0;

	if (indexer->private->in_transaction) {
		stop_transaction (indexer);
	}

	tracker_db_index_flush (indexer->private->file_index);
	tracker_db_index_flush (indexer->private->email_index);

	if ((indexer->private->state & TRACKER_INDEXER_STATE_STOPPED) != 0) {
		signal_status (indexer, "flush");
	}

	indexer->private->items_indexed += indexer->private->items_to_index;
	indexer->private->items_to_index = 0;

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

static guint
get_flush_time (TrackerIndexer *indexer)
{
	guint seconds, remaining_files;

	remaining_files = g_queue_get_length (indexer->private->file_queue);

	seconds = (remaining_files * MAX_FLUSH_FREQUENCY) / FILES_REMAINING_THRESHOLD;

	return CLAMP (seconds, MIN_FLUSH_FREQUENCY, MAX_FLUSH_FREQUENCY);
}

static void
schedule_flush (TrackerIndexer *indexer,
		gboolean	immediately)
{
#if 0
	if (indexer->private->state != 0) {
		return;
	}
#endif

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

	indexer->private->flush_id = g_timeout_add_seconds (get_flush_time (indexer),
							    (GSourceFunc) flush_data,
							    indexer);
}


void 
tracker_indexer_transaction_commit (TrackerIndexer *indexer)
{
	stop_transaction (indexer);
	tracker_indexer_set_running (indexer, TRUE);

}

void
tracker_indexer_transaction_open (TrackerIndexer *indexer)
{
	tracker_indexer_set_running (indexer, FALSE);
	start_transaction (indexer);
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
	}
}

static void
notify_battery_in_use_cb (GObject *gobject,
			  GParamSpec *arg1,
			  gpointer user_data)
{
	set_up_throttle (TRACKER_INDEXER (user_data));
}

#endif /* HAVE_HAL */

static void
index_flushing_notify_cb (GObject        *object,
			  GParamSpec     *pspec,
			  TrackerIndexer *indexer)
{
	TrackerIndexerState state;

	state = indexer->private->state;

	if ((state & TRACKER_INDEXER_STATE_STOPPED) != 0 &&
	    !tracker_db_index_get_flushing (indexer->private->file_index) &&
	    !tracker_db_index_get_flushing (indexer->private->email_index)) {
		/* The indexer has been already stopped and all indices are flushed */
		check_finished (indexer, indexer->private->interrupted);
	}
}

static void
index_overloaded_notify_cb (GObject        *object,
			    GParamSpec     *pspec,
			    TrackerIndexer *indexer)
{
	if (tracker_db_index_get_overloaded (indexer->private->file_index) ||
	    tracker_db_index_get_overloaded (indexer->private->email_index)) {
		g_debug ("Index overloaded, stopping indexer to let it process items");
		state_set_flags (indexer, TRACKER_INDEXER_STATE_INDEX_OVERLOADED);
	} else {
		g_debug ("Index no longer overloaded, resuming data harvesting");
		state_unset_flags (indexer, TRACKER_INDEXER_STATE_INDEX_OVERLOADED);
	}
}

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

	if (priv->signal_status_id) {
		g_source_remove (priv->signal_status_id);
	}

	if (priv->cleanup_task_id) {
		g_source_remove (priv->cleanup_task_id);
	}

	if (priv->timer) {
		g_timer_destroy (priv->timer);
	}

#ifdef HAVE_HAL
	g_signal_handlers_disconnect_by_func (priv->hal,
					      notify_battery_in_use_cb,
					      TRACKER_INDEXER (object));

	g_object_unref (priv->hal);
#endif /* HAVE_HAL */

	g_object_unref (priv->language);
	g_object_unref (priv->config);

	g_signal_handlers_disconnect_by_func (priv->file_index,
					      index_flushing_notify_cb,
					      object);
	g_signal_handlers_disconnect_by_func (priv->file_index,
					      index_overloaded_notify_cb,
					      object);
	g_object_unref (priv->file_index);

	g_signal_handlers_disconnect_by_func (priv->email_index,
					      index_flushing_notify_cb,
					      object);
	g_signal_handlers_disconnect_by_func (priv->email_index,
					      index_overloaded_notify_cb,
					      object);
	g_object_unref (priv->email_index);

	g_free (priv->db_dir);

	g_hash_table_unref (priv->indexer_modules);

	g_list_foreach (priv->module_names, (GFunc) g_free, NULL);
	g_list_free (priv->module_names);

	g_queue_foreach (priv->modules_queue, (GFunc) g_free, NULL);
	g_queue_free (priv->modules_queue);

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
			      tracker_marshal_VOID__DOUBLE_STRING_UINT_UINT_UINT,
			      G_TYPE_NONE,
			      5,
			      G_TYPE_DOUBLE,
			      G_TYPE_STRING,
			      G_TYPE_UINT,
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
			      0);
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
			      tracker_marshal_VOID__DOUBLE_UINT_UINT_BOOL,
			      G_TYPE_NONE,
			      4,
			      G_TYPE_DOUBLE,
			      G_TYPE_UINT,
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

	indexer->private->interrupted = FALSE;
	state_unset_flags (indexer,
			   TRACKER_INDEXER_STATE_STOPPED |
			   TRACKER_INDEXER_STATE_CLEANUP);

	if (indexer->private->timer) {
		g_timer_destroy (indexer->private->timer);
	}

	indexer->private->timer = g_timer_new ();

	/* Open indexes */
	tracker_db_index_open (indexer->private->file_index);
	tracker_db_index_open (indexer->private->email_index);

	g_signal_emit (indexer, signals[STARTED], 0);
}

static void
check_finished (TrackerIndexer *indexer,
		gboolean        interrupted)
{
	TrackerIndexerState state;
	gdouble seconds_elapsed = 0;
	gchar *str;

	state = indexer->private->state;

	if (indexer->private->timer) {
		g_timer_stop (indexer->private->timer);
		seconds_elapsed = g_timer_elapsed (indexer->private->timer, NULL);

		g_timer_destroy (indexer->private->timer);
		indexer->private->timer = NULL;
	}

	/* Close indexes */
	tracker_db_index_close (indexer->private->file_index);
	tracker_db_index_close (indexer->private->email_index);

	/* Print out how long it took us */
	str = tracker_seconds_to_string (seconds_elapsed, FALSE);

	g_message ("Indexer finished in %s, %d items processed in total (%d indexed)",
		   str,
		   indexer->private->items_processed,
		   indexer->private->items_indexed);
	g_free (str);

	/* Finally signal done */
	g_signal_emit (indexer, signals[FINISHED], 0,
		       seconds_elapsed,
		       indexer->private->items_processed,
		       indexer->private->items_indexed,
		       interrupted);

	/* Reset stats */
	indexer->private->items_processed = 0;
	indexer->private->items_indexed = 0;
	indexer->private->items_to_index = 0;
	indexer->private->subelements_processed = 0;

	/* Setup clean up task */
	state_set_flags (indexer, TRACKER_INDEXER_STATE_CLEANUP);
}

static void
check_stopped (TrackerIndexer *indexer,
	       gboolean        interrupted)
{
	if ((indexer->private->state & TRACKER_INDEXER_STATE_STOPPED) == 0) {
		schedule_flush (indexer, TRUE);
		state_set_flags (indexer, TRACKER_INDEXER_STATE_STOPPED);
		indexer->private->interrupted = (interrupted != FALSE);
	} else {
		/* If the indexer is stopped and the indices aren't
		 * being flushed, then it's ready for finishing right away
		 */
		if (!tracker_db_index_get_flushing (indexer->private->file_index) &&
		    !tracker_db_index_get_flushing (indexer->private->email_index)) {
			check_finished (indexer, interrupted);
		}
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
cleanup_task_start (TrackerIndexer *indexer)
{
	TrackerIndexerPrivate *priv;

	priv = indexer->private;

	if (priv->cleanup_task_id == 0) {
		priv->cleanup_task_id = g_timeout_add (500, cleanup_task_func, indexer);

		/* Open indexes */
		tracker_db_index_open (indexer->private->file_index);
		tracker_db_index_open (indexer->private->email_index);
	}
}

static void
cleanup_task_stop (TrackerIndexer *indexer)
{
	TrackerIndexerPrivate *priv;

	priv = indexer->private;

	if (priv->cleanup_task_id != 0) {
		g_source_remove (priv->cleanup_task_id);
		priv->cleanup_task_id = 0;

		/* close indexes */
		tracker_db_index_close (indexer->private->file_index);
		tracker_db_index_close (indexer->private->email_index);
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
	TrackerDBIndex *lindex;

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

	priv->modules_queue = g_queue_new ();
	priv->config = tracker_config_new ();

#ifdef HAVE_HAL
	priv->hal = tracker_hal_new ();

	g_signal_connect (priv->hal, "notify::battery-in-use",
			  G_CALLBACK (notify_battery_in_use_cb),
			  indexer);

	set_up_throttle (indexer);
#endif /* HAVE_HAL */

	priv->language = tracker_language_new (priv->config);

	priv->db_dir = g_build_filename (g_get_user_cache_dir (),
					 "tracker",
					 NULL);

	tracker_indexer_load_modules (indexer);

	/* Set up indexer */
	lindex = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_FILE);
	priv->file_index = g_object_ref (lindex);

	g_signal_connect (priv->file_index, "notify::flushing",
			  G_CALLBACK (index_flushing_notify_cb), indexer);
	g_signal_connect (priv->file_index, "notify::overloaded",
			  G_CALLBACK (index_overloaded_notify_cb), indexer);

	lindex = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_EMAIL);
	priv->email_index = g_object_ref (lindex);

	g_signal_connect (priv->email_index, "notify::flushing",
			  G_CALLBACK (index_flushing_notify_cb), indexer);
	g_signal_connect (priv->email_index, "notify::overloaded",
			  G_CALLBACK (index_overloaded_notify_cb), indexer);

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
	TrackerDBIndex *lindex;
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
	lindex = tracker_db_index_manager_get_index_by_service_id (service_id);

	for (i = 0; arr[i]; i++) {
		tracker_db_index_add_word (lindex,
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
	TrackerDBIndex *lindex;
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

	lindex = tracker_db_index_manager_get_index_by_service_id (service_type);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		tracker_db_index_add_word (lindex,
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
	TrackerDBIndex	       *lindex;
	UpdateWordsForeachData *data;
	gchar		       *word;
	gint			score;

	word = key;
	score = GPOINTER_TO_INT (value);

	data = user_data;

	lindex = tracker_db_index_manager_get_index_by_service_id (data->service_type_id);

	tracker_db_index_add_word (lindex,
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
remove_stale_children (TrackerIndexer *indexer,
		       TrackerService *service,
		       PathInfo       *parent_info,
		       const gchar    *path)
{
	TrackerDBInterface *iface;
	gchar **children;
	PathInfo *info;
	gint i;

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	children = tracker_data_search_files_get (iface, path);

	for (i = 0; children[i]; i++) {
		GFile *file;

		file = g_file_new_for_path (children[i]);

		if (!g_file_query_exists (file, NULL)) {
			/* File doesn't exist, check for deletion */
			info = path_info_new (parent_info->module, file, NULL, TRUE);
			add_file (indexer, info);
		}

		g_object_unref (file);
	}

	g_strfreev (children);
}

static void
generate_item_thumbnail (TrackerIndexer        *indexer,
			 const gchar           *dirname,
			 const gchar           *basename,
			 TrackerModuleMetadata *metadata)
{
	const gchar *path, *mime_type;

	path = tracker_module_metadata_lookup (metadata, METADATA_FILE_NAME_DELIMITED, FALSE);
	mime_type = tracker_module_metadata_lookup (metadata, METADATA_FILE_MIMETYPE, FALSE);

	if (path && path[0] == G_DIR_SEPARATOR && mime_type &&
	    tracker_config_get_enable_thumbnails (indexer->private->config)) {
		GFile *file;
		gchar *uri;

		file = g_file_new_for_path (path);
		uri = g_file_get_uri (file);

		tracker_thumbnailer_get_file_thumbnail (uri, mime_type);

		g_object_unref (file);
		g_free (uri);
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
	gchar *mount_point = NULL;
	gchar *service_path;

	service = get_service_for_file (info->module_file, info->module);

	if (!service) {
		return;
	}

	if (tracker_data_query_service_exists (service, dirname, basename, &id, NULL)) {
		TrackerDataMetadata *old_metadata_emb, *old_metadata_non_emb;
		gchar *old_text;
		gchar *new_text;

		if (tracker_module_file_get_flags (info->module_file) & TRACKER_FILE_CONTENTS_STATIC) {
			/* According to the module, the metadata can't change for this item */
			g_debug ("Not updating static item '%s/%s'",
				 dirname,
				 basename);
			return;
		}

		/* Update case */
		g_debug ("Updating item '%s/%s'", 
			 dirname, 
			 basename);

		/* "metadata" (new metadata) contains embedded props and can contain
		 * non-embedded properties with default values! Dont overwrite those 
		 * in the DB if they already has a value.
		 * 
		 * 1) Remove all old embedded metadata from index and DB
		 * 2) Remove from new metadata all non embedded
		 *    properties that already have value.
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

		if (strcmp (tracker_service_get_name (service), "Folders") == 0) {
			gchar *path;

			/* Remove no longer existing children, this is necessary in case
			 * there were files added/removed in a directory between tracker
			 * executions
			 */

			path = g_build_path (G_DIR_SEPARATOR_S, dirname, basename, NULL);
			remove_stale_children (indexer, service, info, path);
			g_free (path);
		}

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

	generate_item_thumbnail (indexer, dirname, basename, metadata);

	/* TODO: URI branch path -> uri */

	service_path = g_build_path (G_DIR_SEPARATOR_S, 
				     dirname, 
				     basename, 
				     NULL);

	if (tracker_hal_path_is_on_removable_device (indexer->private->hal,
						     service_path, 
						     &mount_point,
						     NULL)) {

		tracker_removable_device_add_metadata (indexer, 
						       mount_point, 
						       service_path, 
						       tracker_service_get_name (service),
						       metadata);
	}

	g_free (mount_point);
	g_free (service_path);
}


static gboolean 
filter_invalid_after_move_properties (TrackerField *field,
				      gpointer value,
				      gpointer user_data) 
{
	const gchar *name;

	name = tracker_field_get_name (field);

	if (g_strcmp0 (name, METADATA_FILE_NAME_DELIMITED) == 0 ||
	    g_strcmp0 (name, METADATA_FILE_NAME) == 0 ||
	    g_strcmp0 (name, METADATA_FILE_PATH) == 0 ||
	    g_strcmp0 (name, METADATA_FILE_EXT) == 0) {
		return FALSE;
	}

	return TRUE;
}

static void
update_moved_item_thumbnail (TrackerIndexer      *indexer,
			     TrackerDataMetadata *old_metadata,
			     GFile               *file,
			     GFile               *source_file)
{
	gchar *uri, *source_uri;
	const gchar *mime_type;

	if (!old_metadata) {
		gchar *path;

		path = g_file_get_path (file);
		g_message ("Could not get mime type to remove thumbnail for:'%s'", path);
		g_free (path);

		return;
	}

	/* TODO URI branch: this is a URI conversion */
	uri = g_file_get_uri (file);
	source_uri = g_file_get_uri (source_file);

	mime_type = tracker_data_metadata_lookup (old_metadata, "File:Mime");
	tracker_thumbnailer_move (source_uri, mime_type, uri);

	g_free (source_uri);
	g_free (uri);
}

static void
update_moved_item_removable_device (TrackerIndexer *indexer,
				    TrackerService *service,
				    GFile          *file,
				    GFile          *source_file)
{
	const gchar *service_name;
	gchar *path, *source_path;
	gchar *mount_point = NULL;

	service_name = tracker_service_get_name (service);
	path = g_file_get_path (file);
	source_path = g_file_get_path (source_file);

	if (tracker_hal_path_is_on_removable_device (indexer->private->hal,
						     source_path,
						     &mount_point,
						     NULL) ) {

		if (tracker_hal_path_is_on_removable_device (indexer->private->hal,
						     path,
						     NULL,
						     NULL) ) {

			tracker_removable_device_add_move (indexer,
							   mount_point,
							   source_path,
							   path,
							   service_name);

		} else {
			tracker_removable_device_add_removal (indexer,
							      mount_point,
							      source_path,
							      service_name);
		}
	}

	g_free (mount_point);
	g_free (source_path);
	g_free (path);
}

static void
update_moved_item_index (TrackerIndexer      *indexer,
			 TrackerService      *service,
			 TrackerDataMetadata *old_metadata,
			 guint32              service_id,
			 GFile               *file,
			 GFile               *source_file)
{
	TrackerModuleMetadata *new_metadata;
	gchar *path, *new_path, *new_name;
	const gchar *ext;

	path = g_file_get_path (file);

	/*
	 *  Update what changes in move event (Path related properties)
	 */
	tracker_data_metadata_foreach_remove (old_metadata,
					      filter_invalid_after_move_properties,
					      NULL);

	unindex_metadata (indexer, service_id, service, old_metadata);

	new_metadata = tracker_module_metadata_new ();

	tracker_file_get_path_and_name (path, &new_path, &new_name);

	tracker_module_metadata_add_string (new_metadata, METADATA_FILE_PATH, new_path);
	tracker_module_metadata_add_string (new_metadata, METADATA_FILE_NAME, new_name);
	tracker_module_metadata_add_string (new_metadata, METADATA_FILE_NAME_DELIMITED, path);

	ext = strrchr (path, '.');
	if (ext) {
		ext++;
		tracker_module_metadata_add_string (new_metadata, METADATA_FILE_EXT, ext);
	}

	index_metadata (indexer, service_id, service, new_metadata);

	g_object_unref (new_metadata);
	g_free (new_path);
	g_free (new_name);
	g_free (path);
}

static void
item_erase (TrackerIndexer *indexer,
	    TrackerService *service,
	    guint32         service_id)
{
	gchar *content, *metadata;
	guint32 service_type_id;
	TrackerDataMetadata *data_metadata;

	service_type_id = tracker_service_get_id (service);

	/* Get mime type and remove thumbnail from thumbnailerd */
	data_metadata = tracker_data_query_metadata (service, service_id, TRUE);

	if (data_metadata) {
		const gchar *path, *mime_type;
		GFile *file;
		gchar *uri;

		/* TODO URI branch: this is a URI conversion */
		path = tracker_data_metadata_lookup (data_metadata, "File:NameDelimited");
		file = g_file_new_for_path (path);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		mime_type = tracker_data_metadata_lookup (data_metadata, "File:Mime");
		tracker_thumbnailer_remove (uri, mime_type);

		tracker_data_metadata_free (data_metadata);
		g_free (uri);
	}

	/* Get content, unindex the words and delete the contents */
	content = tracker_data_query_content (service, service_id);

	if (content) {
		unindex_text_with_parsing (indexer,
					   service_id,
					   service_type_id,
					   content,
					   1000);
		g_free (content);
		tracker_data_update_delete_content (service, service_id);
	}

	/* Get metadata from DB to remove it from the index */
	metadata = tracker_data_query_parsed_metadata (service, service_id);
	unindex_text_no_parsing (indexer,
				 service_id,
				 service_type_id,
				 metadata,
				 1000);
	g_free (metadata);

	/* The weight depends on metadata, but a number high enough
	 * force deletion.
	 */
	metadata = tracker_data_query_unparsed_metadata (service, service_id);
	unindex_text_with_parsing (indexer,
				   service_id,
				   service_type_id,
				   metadata,
				   1000);
	g_free (metadata);

	/* Delete service */
	tracker_data_update_delete_all_metadata (service, service_id);
	tracker_data_update_delete_service (service, service_id);

	schedule_flush (indexer, FALSE);
}

static void
item_move (TrackerIndexer  *indexer,
	   PathInfo	   *info,
	   const gchar	   *dirname,
	   const gchar	   *basename)
{
	TrackerService *service;
	TrackerDataMetadata *old_metadata;
	gchar *path, *source_path;
	gchar *dest_dirname, *dest_basename;
	guint32 service_id, dest_service_id;
	GHashTable *children = NULL;

	service = get_service_for_file (info->module_file, info->module);

	if (!service) {
		return;
	}

	path = g_file_get_path (info->file);
	source_path = g_file_get_path (info->source_file);

	g_debug ("Moving item from '%s' to '%s'", source_path, path);

	/* Get 'source' ID */
	if (!tracker_data_query_service_exists (service,
						dirname,
						basename,
						&service_id,
						NULL)) {
		TrackerModuleMetadata *metadata;
		gchar *dest_dirname, *dest_basename;

		g_message ("Source file '%s' not found in database to move, indexing '%s' from scratch", source_path, path);

		metadata = tracker_module_file_get_metadata (info->module_file);
		tracker_file_get_path_and_name (path, &dest_dirname, &dest_basename);

		if (metadata) {
			item_add_or_update (indexer, info, dest_dirname, dest_basename, metadata);
			g_object_unref (metadata);
		}

		g_free (dest_dirname);
		g_free (dest_basename);
		g_free (path);
		g_free (source_path);

		return;
	}

	tracker_file_get_path_and_name (path, &dest_dirname, &dest_basename);

	/* Check whether destination path already existed */
	if (tracker_data_query_service_exists (service,
					       dest_dirname,
					       dest_basename,
					       &dest_service_id,
					       NULL)) {
		g_message ("Destination file '%s' already existed in database, removing", path);

		/* Item has to be deleted from the database immediately */
		item_erase (indexer, service, dest_service_id);
	}

	g_free (dest_dirname);
	g_free (dest_basename);

	if (info->recurse && strcmp (tracker_service_get_name (service), "Folders") == 0) {
		children = tracker_data_query_service_children (service, source_path);
	}

	/* Get mime type in order to move thumbnail from thumbnailerd */
	old_metadata = tracker_data_query_metadata (service, service_id, TRUE);

	if (!tracker_data_update_move_service (service, source_path, path)) {
		g_critical ("Moving item could not be done for unknown reasons");

		g_free (path);
		g_free (source_path);

		if (old_metadata) {
			tracker_data_metadata_free (old_metadata);
		}

		return;
	}

	/* Update item being moved */
	update_moved_item_thumbnail (indexer, old_metadata, info->file, info->source_file);
	update_moved_item_removable_device (indexer, service, info->file, info->source_file);
	update_moved_item_index (indexer, service, old_metadata, service_id, info->file, info->source_file);

	if (children) {
		GHashTableIter iter;
		gpointer key, value;

		g_hash_table_iter_init (&iter, children);

		/* Queue children to be moved */
		while (g_hash_table_iter_next (&iter, &key, &value)) {
			PathInfo *child_info;
			const gchar *child_name;
			GFile *child_file, *child_source_file;

			child_name = (const gchar *) value;

			child_file = g_file_get_child (info->file, child_name);
			child_source_file = g_file_get_child (info->source_file, child_name);

			child_info = path_info_new (info->module, child_file, child_source_file, TRUE);
			add_file (indexer, child_info);

			g_object_unref (child_file);
			g_object_unref (child_source_file);
		}

		g_hash_table_destroy (children);
	}

	if (old_metadata) {
		tracker_data_metadata_free (old_metadata);
	}

	g_free (source_path);
	g_free (path);
}


static void
item_mark_for_removal (TrackerIndexer *indexer,
		       PathInfo	      *info,
		       const gchar    *dirname,
		       const gchar    *basename)
{
	TrackerService *service;
	gchar *path;
	gchar *mount_point = NULL;
	const gchar *service_type;
	guint service_id, service_type_id;
	GHashTable *children = NULL;

	g_debug ("Removing item: '%s/%s' (no metadata was given by module)", 
		 dirname, 
		 basename);

	/* The file is not anymore in the filesystem. Obtain
	 * the service type from the DB.
	 */
	service_type_id = tracker_data_query_service_type_id (dirname, basename);

	if (service_type_id == 0) {
		/* File didn't exist, nothing to delete */
		return;
	}

	service_type = tracker_ontology_get_service_by_id (service_type_id);
	service = tracker_ontology_get_service_by_name (service_type);

	tracker_data_query_service_exists (service, dirname, basename, &service_id, NULL);

	if (service_id < 1) {
		g_debug ("  File does not exist anyway "
			 "(dirname:'%s', basename:'%s')",
			 dirname, basename);
		return;
	}

	/* This is needed in a few places. */
	path = g_build_path (G_DIR_SEPARATOR_S, dirname, basename, NULL);

	if (info->recurse && strcmp (tracker_service_get_name (service), "Folders") == 0) {
		children = tracker_data_query_service_children (service, path);
	}

	tracker_data_update_disable_service (service, service_id);

	if (children) {
		GHashTableIter iter;
		gpointer key, value;

		g_hash_table_iter_init (&iter, children);

		/* Queue children to be removed */
		while (g_hash_table_iter_next (&iter, &key, &value)) {
			PathInfo *child_info;
			const gchar *child_name;
			GFile *child_file;

			child_name = (const gchar *) value;
			child_file = g_file_get_child (info->file, child_name);

			child_info = path_info_new (info->module, child_file, NULL, TRUE);
			add_file (indexer, child_info);

			g_object_unref (child_file);
		}

		g_hash_table_destroy (children);
	}

	if (tracker_hal_path_is_on_removable_device (indexer->private->hal,
						     path, 
						     &mount_point,
						     NULL)) {

		tracker_removable_device_add_removal (indexer, mount_point, 
						      path,
						      tracker_service_get_name (service));
	}

	g_free (mount_point);
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
should_change_index_for_file (TrackerIndexer *indexer,
			      PathInfo        *info,
			      const gchar	  *dirname,
			      const gchar	  *basename)
{
	TrackerService *service;
	gchar *path;
	guint64 current_mtime;
	time_t db_mtime;

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
						&db_mtime)) {
		return TRUE;
	}

	/* So, if we are here, then the file or directory DID exist
	 * in the database already. Now we need to check if the
	 * parent directory mtime matches the mtime we have for it in
	 * the database. If it does, then we can ignore any files
	 * immediately in this parent directory.
	 */
	path = g_file_get_path (info->file);
	current_mtime = tracker_file_get_mtime (path);

	/* NOTE: We return TRUE here because we want to update the DB
	 * about this file, not because we want to index it.
	 */
	if (current_mtime == 0) {
		g_free (path);
		return TRUE;
	}

	if (current_mtime <= db_mtime) {
		g_debug ("'%s' is already up to date in DB, not (re)indexing", path);
		g_free (path);

		return FALSE;
	}

	g_free (path);

	return TRUE;
}

static gboolean
process_file (TrackerIndexer *indexer,
	      PathInfo	     *info)
{
	TrackerModuleMetadata *metadata;
	gchar *uri, *dirname, *basename;

	/* Note: If info->source_file is set, the PathInfo is for a
	 * MOVE event not for normal file event.
	 */

	if (!info->module_file) {
		return TRUE;
	}

	/* Set the current module */
	indexer->private->current_module = g_quark_from_string (info->module->name);

	if (G_UNLIKELY (info->source_module_file)) {
		uri = tracker_module_file_get_uri (info->source_module_file);
	} else {
		uri = tracker_module_file_get_uri (info->module_file);
	}

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
	if (G_LIKELY (!info->source_file) && dirname[0] == G_DIR_SEPARATOR) {
		if (!should_change_index_for_file (indexer, info, dirname, basename)) {
			indexer->private->items_processed++;

			g_free (dirname);
			g_free (basename);

			return TRUE;
		}
	}

	/* Sleep to throttle back indexing */
	tracker_throttle (indexer->private->config, 100);

	/* For normal files create or delete the item with the
	 * metadata. For move PathInfo we use the db function to move
	 * a service and set the metadata.
	 */
	if (G_UNLIKELY (info->source_file)) {
		item_move (indexer, info, dirname, basename);
	} else {
		metadata = tracker_module_file_get_metadata (info->module_file);

		if (metadata) {
			item_add_or_update (indexer, info, dirname, basename, metadata);
			g_object_unref (metadata);
		} else {
			item_mark_for_removal (indexer, info, dirname, basename);
		}
	}

	indexer->private->subelements_processed++;
	indexer->private->items_processed++;
	indexer->private->items_to_index++;

	g_free (dirname);
	g_free (basename);

	if (TRACKER_IS_MODULE_ITERATABLE (info->module_file)) {
		return !tracker_module_iteratable_iter_contents (TRACKER_MODULE_ITERATABLE (info->module_file));
	}

	return TRUE;
}

static void
process_directory (TrackerIndexer *indexer,
		   PathInfo       *info)
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

		new_info = path_info_new (info->module, child, NULL, FALSE);
		add_file (indexer, new_info);

		if (info->recurse && g_file_test (child_path, G_FILE_TEST_IS_DIR)) {
			new_info = path_info_new (info->module, child, NULL, TRUE);
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

	if (!module) {
		/* No need to signal stopped here, we will get that
		 * signal the next time this function is called.
		 */
		g_message ("No module for:'%s'", module_name);
		return;
	}

	dirs = tracker_module_config_get_monitor_recurse_directories (module_name);

	if (!dirs) {
		return;
	}

	g_message ("Starting module:'%s'", module_name);

	/* Signal module start/stop */
	process_module_emit_signals (indexer, module_name);

	for (d = dirs; d; d = d->next) {
		PathInfo *info;
		GFile *file;

		file = g_file_new_for_path (d->data);
		info = path_info_new (module, file, NULL, TRUE);
		add_directory (indexer, info);

		g_object_unref (file);
	}

	g_list_free (dirs);
}

static gboolean
cleanup_task_func (gpointer user_data)
{
	TrackerIndexer *indexer;
	TrackerIndexerPrivate *priv;
	TrackerService *service;
	guint32 id;

	indexer = (TrackerIndexer *) user_data;
	priv = indexer->private;

	if (indexer->private->idle_id) {
		/* Sanity check, do not index and clean up at the same time */
		indexer->private->cleanup_task_id = 0;
		return FALSE;
	}

	if (tracker_data_query_first_removed_service (priv->file_metadata, &id)) {
		g_debug ("Cleanup: Deleting service '%d' from files", id);
		service = tracker_ontology_get_service_by_name ("Files");
		item_erase (indexer, service, id);

		return TRUE;
	} else if (tracker_data_query_first_removed_service (priv->email_metadata, &id)) {
		g_debug ("Cleanup: Deleting service '%d' from emails", id);
		service = tracker_ontology_get_service_by_name ("Emails");
		item_erase (indexer, service, id);

		return TRUE;
	}

	g_debug ("Cleanup: No elements left, exiting");

	state_unset_flags (indexer, TRACKER_INDEXER_STATE_CLEANUP);

	return FALSE;
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
			indexer->private->subelements_processed = 0;
			path = g_queue_pop_head (indexer->private->file_queue);
			path_info_free (path);
		}
	} else if ((path = g_queue_pop_head (indexer->private->dir_queue)) != NULL) {
		/* Process directory contents */
		process_directory (indexer, path);
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
			
			return FALSE;
		}

		process_module (indexer, module_name);
		g_free (module_name);
	}

	if (indexer->private->items_to_index > TRACKER_INDEXER_TRANSACTION_MAX) {
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

	if ((state & TRACKER_INDEXER_STATE_PAUSED) != 0) {
		return FALSE;
	}

	if ((state & TRACKER_INDEXER_STATE_CLEANUP) == 0 &&
	    (state & TRACKER_INDEXER_STATE_STOPPED) != 0) {
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_indexer_get_stoppable (TrackerIndexer *indexer)
{
	TrackerIndexerState state;

	g_return_val_if_fail (TRACKER_IS_INDEXER (indexer), FALSE);

	state = indexer->private->state;

	return ((state & TRACKER_INDEXER_STATE_STOPPED) != 0 ||
		(state & TRACKER_INDEXER_STATE_PAUSED) != 0);
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

		if ((state & TRACKER_INDEXER_STATE_CLEANUP) != 0) {
			/* Cleanup stage is only modified by paused */
			if ((state & TRACKER_INDEXER_STATE_PAUSED) != 0) {
				cleanup_task_stop (indexer);
			} else {
				cleanup_task_start (indexer);
			}
		}
	} else {
		signal_status_timeout_start (indexer);
		cleanup_task_stop (indexer);

		if (indexer->private->idle_id == 0) {
			indexer->private->idle_id = g_idle_add (process_func, indexer);
		}
	}
}

static gchar *
state_to_string (TrackerIndexerState state)
{
	GString *s;

	s = g_string_new ("");
	
	if (state & TRACKER_INDEXER_STATE_INDEX_OVERLOADED) {
		s = g_string_append (s, "INDEX_OVERLOADED | ");
	}
	if (state & TRACKER_INDEXER_STATE_PAUSED) {
		s = g_string_append (s, "PAUSED | ");
	}
	if (state & TRACKER_INDEXER_STATE_STOPPED) {
		s = g_string_append (s, "STOPPED | ");
	}
	if (state & TRACKER_INDEXER_STATE_CLEANUP) {
		s = g_string_append (s, "CLEANUP | ");
	}

	s->str[s->len - 3] = '\0';

	return g_string_free (s, FALSE);
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
	    (state & TRACKER_INDEXER_STATE_PAUSED)) {
		gchar *old_state_str;
		gchar *state_str;

		old_state_str = state_to_string (old_state);
		state_str = state_to_string (indexer->private->state);

		g_message ("State change from '%s' --> '%s'",
			   old_state_str,
			   state_str);

		g_free (state_str);
		g_free (old_state_str);

		g_signal_emit (indexer, signals[PAUSED], 0);
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

	if ((old_state & TRACKER_INDEXER_STATE_PAUSED) &&
	    (! (new_state & TRACKER_INDEXER_STATE_PAUSED))) {
		gchar *old_state_str;
		gchar *state_str;

		old_state_str = state_to_string (old_state);
		state_str = state_to_string (indexer->private->state);

		g_message ("State change from '%s' --> '%s'",
			   old_state_str,
			   state_str);

		g_free (state_str);
		g_free (old_state_str);

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
		info = path_info_new (module, file, NULL, TRUE);
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
	GFile *file_from, *file_to;

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

	file_from = g_file_new_for_path (from);
	file_to = g_file_new_for_path (to);

	/* Add files to the queue */
	info = path_info_new (module, file_to, file_from, TRUE);
	add_file (indexer, info);

	dbus_g_method_return (context);
	tracker_dbus_request_success (request_id);

	g_object_unref (file_from);
	g_object_unref (file_to);
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

	/* tracker_turtle_process_ttl will be spinning the mainloop, therefore
	   we can already return the DBus method */

	if (enabled) {
		tracker_removable_device_load (indexer, path);
	}
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

static void
restore_backup_cb (const gchar *subject,
		   const gchar *predicate,
		   const gchar *object,
		   gpointer     user_data)
{
	const gchar *values[2] = { object, NULL };
	TrackerIndexer *indexer = user_data;
	GError *error = NULL;

	handle_metadata_add (indexer,
			     "Files",
			     subject,
			     predicate,
			     (GStrv) values,
			     &error);

	if (error) {
		g_warning ("Restoring backup: %s", error->message);
		g_error_free (error);
	}

	g_main_context_iteration (NULL, FALSE);
}

void
tracker_indexer_restore_backup (TrackerIndexer         *indexer,
				const gchar            *backup_file,
				DBusGMethodInvocation  *context,
				GError                **error)
{
	guint request_id;
	GError *err = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (TRACKER_IS_INDEXER (indexer), context);

	tracker_dbus_request_new (request_id,
				  "DBus request to restore backup data from '%s'",
				  backup_file);

	tracker_data_backup_restore (backup_file,
				     restore_backup_cb,
				     indexer,
				     &err);

	if (err) {
		GError *actual_error = NULL;

		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     err->message);

		dbus_g_method_return_error (context, actual_error);

		g_error_free (actual_error);
		g_error_free (err);
	} else {
		dbus_g_method_return (context);
		tracker_dbus_request_success (request_id);
	}
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
