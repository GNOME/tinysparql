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
 * * The files queue: highest priority, individual files are
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

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-power.h>
#include <libtracker-common/tracker-storage.h>
#include <libtracker-common/tracker-parser.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-thumbnailer.h>

#include <libtracker-db/tracker-db-dbus.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-turtle.h>
#include <libtracker-data/tracker-data-backup.h>

#include <libtracker/tracker.h>

#include "tracker-config.h"
#include "tracker-indexer.h"
#include "tracker-indexer-module.h"
#include "tracker-marshal.h"
#include "tracker-removable-device.h"
#include "tracker-status.h"
#include "tracker-utils.h"

#define TRACKER_INDEXER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_INDEXER, TrackerIndexerPrivate))

#define FILES_REMAINING_THRESHOLD   1000
#define MAX_FLUSH_FREQUENCY         60
#define MIN_FLUSH_FREQUENCY         1

#define SIGNAL_STATUS_FREQUENCY     10

/* Throttle defaults */
#define THROTTLE_DEFAULT	    0
#define THROTTLE_DEFAULT_ON_BATTERY 5

#define TRACKER_INDEXER_ERROR	    "tracker-indexer-error-domain"
#define TRACKER_INDEXER_ERROR_CODE  0

#define NIE_PREFIX TRACKER_NIE_PREFIX
#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"
#define TRACKER_PREFIX TRACKER_TRACKER_PREFIX

#define NIE_PLAIN_TEXT_CONTENT NIE_PREFIX "plainTextContent"
#define NIE_MIME_TYPE NIE_PREFIX "mimeType"

#define TRACKER_DATASOURCE TRACKER_PREFIX "Volume"
#define NIE_DATASOURCE_P NIE_PREFIX "dataSource"

#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NFO_FILE_NAME NFO_PREFIX "fileName"
#define NIE_PLAIN_TEXT_CONTENT NIE_PREFIX "plainTextContent"
#define NIE_MIME_TYPE NIE_PREFIX "mimeType"

typedef struct PathInfo PathInfo;
typedef struct MetadataForeachData MetadataForeachData;
typedef struct MetadataRequest MetadataRequest;
typedef enum TrackerIndexerState TrackerIndexerState;

struct TrackerIndexerPrivate {
	GQueue *dir_queue;
	GQueue *file_queue;
	GQueue *modules_queue;

	GList *module_names;
	GQuark current_module;
	GHashTable *indexer_modules;

	TrackerConfig *config;

	TrackerPower   *power;
	TrackerStorage *storage;

	TrackerClient *client;

	GTimer *timer;

	GVolumeMonitor *volume_monitor;

	guint idle_id;
	guint pause_for_duration_id;
	guint signal_status_id;
	guint flush_id;

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
	TrackerConfig *config;
	TrackerClass *service;
	gboolean add;
	const gchar *uri;
	guint32 id;
};

enum TrackerIndexerState {
	TRACKER_INDEXER_STATE_INDEX_OVERLOADED = 1 << 0,
	TRACKER_INDEXER_STATE_PAUSED	= 1 << 1,
	TRACKER_INDEXER_STATE_STOPPED	= 1 << 2,
};

enum {
	PROP_0,
	PROP_CONFIG,
	PROP_STORAGE,
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
	INDEXING_ERROR,
	LAST_SIGNAL
};

static void     indexer_set_property (GObject             *object,
				      guint                param_id,
				      const GValue        *value,
				      GParamSpec          *pspec);
static void     indexer_get_property (GObject             *object,
				      guint                prop_id,
				      GValue              *value,
				      GParamSpec          *pspec);
static void     indexer_constructed  (GObject             *object);
static gboolean process_func         (gpointer             data);
static void     state_set_flags      (TrackerIndexer      *indexer,
				      TrackerIndexerState  state);
static void     state_unset_flags    (TrackerIndexer      *indexer,
				      TrackerIndexerState  state);
static void     state_check          (TrackerIndexer      *indexer);
static void     item_remove          (TrackerIndexer      *indexer,
				      PathInfo            *info,
				      const gchar         *uri,
				      const gchar         *mime_type);
static void     check_finished       (TrackerIndexer      *indexer,
				      gboolean             interrupted);
static gboolean item_process         (TrackerIndexer      *indexer,
				      PathInfo            *info,
				      const gchar         *uri);
static void     indexer_load_modules (TrackerIndexer      *indexer);
static gboolean start_cb             (gpointer             user_data);

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
	indexer->private->in_transaction = TRUE;
}

static void
stop_transaction (TrackerIndexer *indexer)
{
	if (!indexer->private->in_transaction) {
		return;
	}

	indexer->private->in_transaction = FALSE;
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

	tracker_resources_batch_commit (indexer->private->client, NULL);

	if ((indexer->private->state & TRACKER_INDEXER_STATE_STOPPED) == 0) {
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

#if defined(HAVE_HAL) || defined(HAVE_DEVKIT_POWER)

static void
set_up_throttle (TrackerIndexer *indexer)
{
	gint throttle;

	/* If on a laptop battery and the throttling is default (i.e.
	 * 0), then set the throttle to be higher so we don't kill
	 * the laptop battery.
	 */
	throttle = tracker_config_get_throttle (indexer->private->config);

	if (tracker_power_get_on_battery (indexer->private->power)) {
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
notify_on_battery_cb (GObject *gobject,
		      GParamSpec *arg1,
		      gpointer user_data)
{
	set_up_throttle (TRACKER_INDEXER (user_data));
}

#endif /* HAVE_HAL || HAVE_DEVKIT_POWER */

static void
check_mount_removal (GQueue   *queue,
		     GFile    *mount_root,
		     gboolean  remove_first)
{
	GList *list, *next;
	PathInfo *info;

	if (!queue->head) {
		/* No elements here */
		return;
	}

	list = (remove_first) ? queue->head : queue->head->next;

	while (list) {
		next = list->next;
		info = list->data;

		if (g_file_has_prefix (info->file, mount_root) ||
		    (info->source_file && g_file_has_prefix (info->source_file, mount_root))) {
			g_queue_delete_link (queue, list);
			path_info_free (info);
		}

		list = next;
	}
}

static void
mount_pre_unmount_cb (GVolumeMonitor *volume_monitor,
		      GMount         *mount,
		      TrackerIndexer *indexer)
{
	TrackerIndexerPrivate *priv;
	GFile *mount_root;
	PathInfo *current_info;
	gchar *uri;

	mount_root = g_mount_get_root (mount);
	priv = indexer->private;

	uri = g_file_get_uri (mount_root);
	g_debug ("Pre-unmount event for '%s', removing all child elements to be processed", uri);
	g_free (uri);

	/* Cancel any future elements in the mount */
	check_mount_removal (priv->dir_queue, mount_root, TRUE);
	check_mount_removal (priv->file_queue, mount_root, FALSE);

	/* Now cancel current element if it's also in the mount */
	current_info = g_queue_peek_head (indexer->private->file_queue);

	if (current_info &&
	    g_file_has_prefix (current_info->file, mount_root)) {
		tracker_module_file_cancel (current_info->module_file);
	}

	g_object_unref (mount_root);
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

	if (priv->timer) {
		g_timer_destroy (priv->timer);
	}

#if defined(HAVE_HAL) || defined(HAVE_DEVKIT_POWER)
	g_signal_handlers_disconnect_by_func (priv->power,
					      notify_on_battery_cb,
					      TRACKER_INDEXER (object));

	g_object_unref (priv->power);
#endif /* HAVE_HAL || HAVE_DEVKIT_POWER */

#ifdef HAVE_HAL
	g_object_unref (priv->storage);
#endif /* HAVE_HAL */

	if (priv->client) {
		tracker_disconnect (priv->client);
	}

	g_object_unref (priv->config);

	g_hash_table_unref (priv->indexer_modules);

	g_list_foreach (priv->module_names, (GFunc) g_free, NULL);
	g_list_free (priv->module_names);

	g_queue_foreach (priv->modules_queue, (GFunc) g_free, NULL);
	g_queue_free (priv->modules_queue);

	g_queue_foreach (priv->dir_queue, (GFunc) path_info_free, NULL);
	g_queue_free (priv->dir_queue);

	g_queue_foreach (priv->file_queue, (GFunc) path_info_free, NULL);
	g_queue_free (priv->file_queue);

	if (priv->volume_monitor) {
		g_signal_handlers_disconnect_by_func (priv->volume_monitor,
						      mount_pre_unmount_cb,
						      object);
		g_object_unref (priv->volume_monitor);
	}

	G_OBJECT_CLASS (tracker_indexer_parent_class)->finalize (object);
}

static void
tracker_indexer_class_init (TrackerIndexerClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = tracker_indexer_finalize;
	object_class->set_property = indexer_set_property;
	object_class->get_property = indexer_get_property;
	object_class->constructed = indexer_constructed;

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
	signals[INDEXING_ERROR] =
		g_signal_new ("indexing-error",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerIndexerClass, indexing_error),
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_BOOL,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_BOOLEAN);

	g_object_class_install_property (object_class,
					 PROP_STORAGE,
					 g_param_spec_pointer ("storage",
							       "Storage HAL object",
							       "The object used for storage knowledge",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_CONFIG,
					 g_param_spec_pointer ("config",
							       "Config object",
							       "The object used for config knowledge",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_RUNNING,
					 g_param_spec_boolean ("running",
							       "Running",
							       "Whether the indexer is running",
							       TRUE,
							       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (TrackerIndexerPrivate));
}

static void
indexer_set_property (GObject	   *object,
		      guint	    param_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	TrackerIndexer *indexer;

	indexer = TRACKER_INDEXER (object);

	switch (param_id) {
	case PROP_CONFIG: {
		gpointer p;

		p = g_value_get_pointer (value);

		if (indexer->private->config) {
			g_object_unref (indexer->private->config);
		}

		if (p) {
			indexer->private->config = g_object_ref (p);
		} else {
			indexer->private->config = NULL;
		}

		break;
	}

	case PROP_STORAGE: {
		gpointer p;

		p = g_value_get_pointer (value);

		if (indexer->private->storage) {
			g_object_unref (indexer->private->storage);
		}

		if (p) {
			indexer->private->storage = g_object_ref (p);
		} else {
			indexer->private->storage = NULL;
		}

		break;
	}

	case PROP_RUNNING:
		tracker_indexer_set_running (TRACKER_INDEXER (object),
					     g_value_get_boolean (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
indexer_get_property (GObject	 *object,
		      guint	  prop_id,
		      GValue	 *value,
		      GParamSpec *pspec)
{
	TrackerIndexer *indexer;

	indexer = TRACKER_INDEXER (object);

	switch (prop_id) {
	case PROP_CONFIG:
		g_value_set_pointer (value, indexer->private->config);
		break;

	case PROP_STORAGE:
		g_value_set_pointer (value, indexer->private->storage);
		break;

	case PROP_RUNNING:
		g_value_set_boolean (value,
				     tracker_indexer_get_running (TRACKER_INDEXER (object)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
indexer_constructed (GObject *object)
{
	TrackerIndexer *indexer;
	gint seconds;

	indexer = TRACKER_INDEXER (object);

	set_up_throttle (indexer);

	tracker_status_init (indexer->private->config, 
			     indexer->private->power);

	/* Set our status as running, if this is FALSE, threads stop
	 * doing what they do and shutdown.
	 */
	tracker_status_set_is_ready (TRUE);

	/* We set the state here because it is not set in the
	 * processor otherwise.
	 */
	tracker_status_set_and_signal (TRACKER_STATUS_IDLE);

	indexer_load_modules (indexer);

	/* Set up idle handler to process files/directories */
	state_check (indexer);

	/* Set up wait time */
	seconds = tracker_config_get_initial_sleep (indexer->private->config);

	if (seconds > 0) {
		g_message ("Waiting %d seconds before starting",
		           seconds);
		g_timeout_add_seconds (seconds, start_cb, indexer);
	} else {
		g_idle_add (start_cb, indexer);
	}
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
	state_unset_flags (indexer, TRACKER_INDEXER_STATE_STOPPED);

	if (indexer->private->timer) {
		g_timer_destroy (indexer->private->timer);
	}

	indexer->private->timer = g_timer_new ();

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

	/* Push all items in thumbnail queue to the thumbnailer */
	tracker_thumbnailer_queue_send ();
}

static void
check_stopped (TrackerIndexer *indexer,
	       gboolean        interrupted)
{
	schedule_flush (indexer, TRUE);
	state_set_flags (indexer, TRACKER_INDEXER_STATE_STOPPED);
	indexer->private->interrupted = (interrupted != FALSE);
	check_finished (indexer, interrupted);
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
indexer_load_modules (TrackerIndexer *indexer)
{
	TrackerIndexerPrivate *priv;
	GList *modules, *l;

	priv = indexer->private;
	priv->indexer_modules = g_hash_table_new (g_str_hash, g_str_equal);

	/* modules = tracker_module_config_get_modules (); */

	modules = g_list_prepend (NULL, g_strdup ("files"));

	for (l = modules; l; l = l->next) {
		TrackerIndexerModule *module;

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

static gboolean
start_cb (gpointer user_data)
{
	TrackerIndexer *indexer;

	if (!tracker_status_get_is_ready ()) {
		return FALSE;
	}

	indexer = TRACKER_INDEXER (user_data);

#ifdef FIX
	/* Start processor here */
#endif

	return FALSE;
}

static void
tracker_indexer_init (TrackerIndexer *indexer)
{
	TrackerIndexerPrivate *priv;

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

	priv->client = tracker_connect (TRUE, -1);

#if defined(HAVE_HAL) || defined(HAVE_DEVKIT_POWER)
	priv->power = tracker_power_new ();
	g_signal_connect (priv->power, "notify::on-battery",
			  G_CALLBACK (notify_on_battery_cb),
			  indexer);
#endif /* HAVE_HAL || HAVE_DEVKIT_POWER */

	/* Set up volume monitor */
	priv->volume_monitor = g_volume_monitor_get ();
	g_signal_connect (priv->volume_monitor, "mount-pre-unmount",
			  G_CALLBACK (mount_pre_unmount_cb), 
			  indexer);
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

static gboolean
query_resource_exists (TrackerIndexer *indexer,
		       const gchar    *uri)
{
	gboolean   result;
	gchar     *sparql;
	GPtrArray *sparql_result;

	sparql = g_strdup_printf ("SELECT ?s WHERE { ?s a rdfs:Resource . FILTER (?s = <%s>) }",
	                          uri);

	sparql_result = tracker_resources_sparql_query (indexer->private->client, sparql, NULL);

	result = (sparql_result && sparql_result->len == 1);

	tracker_dbus_results_ptr_array_free (&sparql_result);
	g_free (sparql);

	return result;
}

static gchar *
query_property_value (TrackerIndexer *indexer,
		      const gchar    *subject,
		      const gchar    *predicate)
{
	gchar     *sparql, *result;
	GPtrArray *sparql_result;

	sparql = g_strdup_printf ("SELECT ?o WHERE { <%s> <%s> ?o }",
	                          subject, predicate);

	sparql_result = tracker_resources_sparql_query (indexer->private->client, sparql, NULL);

	result = NULL;

	if (sparql_result) {
		if (sparql_result->len == 1) {
			gchar **row;

			row = sparql_result->pdata[0];
			if (row[0] != NULL && strlen (row[0]) > 0) {
				result = row[0];
			}
			g_free (row);
		}
		g_ptr_array_free (sparql_result, TRUE);
	}

	g_free (sparql);

	return result;
}

static void
item_add_to_datasource (TrackerIndexer *indexer,
			const gchar *uri,
			TrackerModuleFile *module_file,
			TrackerSparqlBuilder *sparql)
{
	GFile *file;
	const gchar *removable_device_udi;

	file = tracker_module_file_get_file (module_file);

#ifdef HAVE_HAL
	removable_device_udi = tracker_storage_get_volume_udi_for_file (indexer->private->storage,
									file);
#else
	removable_device_udi = NULL;
#endif

	if (removable_device_udi) {
		gchar *removable_device_urn;

		removable_device_urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s", 
						        removable_device_udi);

		tracker_sparql_builder_subject_iri (sparql, removable_device_urn);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "tracker:Volume");

		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, removable_device_urn);

		g_free (removable_device_urn);
	} else {
		tracker_sparql_builder_subject_iri (sparql, TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "tracker:Volume");

		tracker_sparql_builder_predicate (sparql, "nie:dataSource");
		tracker_sparql_builder_object_iri (sparql, TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN);
	}
}

static void
item_add_or_update (TrackerIndexer        *indexer,
		    PathInfo              *info,
		    const gchar           *uri,
		    TrackerSparqlBuilder  *sparql,
		    const gchar           *mime_type)
{
	gchar *full_sparql;
	gchar *mount_point = NULL;

	if (G_UNLIKELY (!indexer->private->in_transaction)) {
		start_transaction (indexer);
	}

	g_debug ("Adding item '%s'", 
		 uri);

	item_add_to_datasource (indexer, uri, info->module_file, sparql);

	tracker_sparql_builder_insert_close (sparql);

	full_sparql = g_strdup_printf ("DROP GRAPH <%s> %s",
		uri, tracker_sparql_builder_get_result (sparql));

	tracker_resources_batch_sparql_update (indexer->private->client, full_sparql, NULL);
	g_free (full_sparql);

	schedule_flush (indexer, FALSE);

	if (mime_type) {
		if (tracker_config_get_enable_thumbnails (indexer->private->config)) {
			tracker_thumbnailer_queue_add (uri, mime_type);
		}
	}

#ifdef HAVE_HAL
	if (tracker_storage_uri_is_on_removable_device (indexer->private->storage,
							uri, 
							&mount_point,
							NULL)) {

		tracker_removable_device_add_metadata (indexer, 
						       mount_point, 
						       uri, 
						       sparql);
	}
#endif
	g_free (mount_point);
}

static void
update_file_uri_recursively (TrackerIndexer *indexer,
			     GString        *sparql_update,
			     const gchar    *source_uri,
			     const gchar    *uri)
{
	gchar *mime_type, *sparql;
	GPtrArray *result_set;
	GError *error = NULL;

	g_debug ("Moving item from '%s' to '%s'",
		 source_uri,
		 uri);

	g_string_append_printf (sparql_update, " <%s> tracker:uri <%s> .", source_uri, uri);

	/* Get mime type in order to move thumbnail from thumbnailerd */
	mime_type = query_property_value (indexer, source_uri, NIE_MIME_TYPE);

	if (mime_type) {
		tracker_thumbnailer_move (source_uri, mime_type, uri);
		g_free (mime_type);
	} else {
		g_message ("Could not get mime type to remove thumbnail for:'%s'",
			   source_uri);
	}

	sparql = g_strdup_printf ("SELECT ?child WHERE { ?child nfo:belongsToContainer <%s> }", source_uri);
	result_set = tracker_resources_sparql_query (indexer->private->client, sparql, &error);
	g_free (sparql);
	if (result_set) {
		gint i;

		for (i = 0; i < result_set->len; i++) {
			gchar **child_source_uri, *child_uri;

			child_source_uri = result_set->pdata[i];
			if (!g_str_has_prefix (*child_source_uri, source_uri)) {
				g_warning ("Child URI '%s' does not start with parent URI '%s'",
				           *child_source_uri,
				           source_uri);
				continue;
			}
			child_uri = g_strdup_printf ("%s%s", uri, *child_source_uri + strlen (source_uri));

			update_file_uri_recursively (indexer, sparql_update, *child_source_uri, child_uri);

			g_free (child_source_uri);
			g_free (child_uri);
		}
		g_ptr_array_free (result_set, TRUE);
	}
}

static gboolean
item_move (TrackerIndexer  *indexer,
	   PathInfo	   *info,
	   const gchar	   *source_uri)
{
	gchar     *uri, *escaped_filename;
	GFileInfo *file_info;
	GString   *sparql;
#ifdef HAVE_HAL
	gchar *mount_point = NULL;
#endif

	uri = g_file_get_uri (info->file);

	/* Get 'source' ID */
	if (!query_resource_exists (indexer, source_uri)) {
		gboolean res;

		g_message ("Source file '%s' not found in database to move, indexing '%s' from scratch", source_uri, uri);

		res = item_process (indexer, info, uri);

		g_free (uri);

		return res;
	}

	sparql = g_string_new ("");

	g_string_append_printf (sparql,
		"DELETE { <%s> nfo:fileName ?o } WHERE { <%s> nfo:fileName ?o }",
		source_uri, source_uri);

	g_string_append (sparql, " INSERT {");

	file_info = g_file_query_info (info->file,
					G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
					G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					NULL, NULL);

	escaped_filename = g_strescape (g_file_info_get_display_name (file_info), NULL);

	g_string_append_printf (sparql, " <%s> nfo:fileName \"%s\" .", source_uri, escaped_filename);

	update_file_uri_recursively (indexer, sparql, source_uri, uri);

	g_string_append (sparql, " }");

	tracker_resources_batch_sparql_update (indexer->private->client, sparql->str, NULL);

#ifdef HAVE_HAL
	if (tracker_storage_uri_is_on_removable_device (indexer->private->storage,
							source_uri, 
							&mount_point,
							NULL) ) {
		if (tracker_storage_uri_is_on_removable_device (indexer->private->storage,
								uri, 
								NULL,
								NULL) ) {

			tracker_removable_device_add_move (indexer, 
							   mount_point, 
							   source_uri, 
							   uri);
		} else {
			tracker_removable_device_add_removal (indexer, 
							      mount_point, 
							      source_uri);
		}

		g_free (mount_point);
	}
#endif

	g_free (uri);
	g_object_unref (file_info);
	g_string_free (sparql, TRUE);

	return TRUE;
}


static void
item_remove (TrackerIndexer *indexer,
	     PathInfo	    *info,
	     const gchar    *uri,
	     const gchar    *mime_type)
{
	gchar *mount_point = NULL;
	gchar *sparql;

	g_debug ("Removing item: '%s' (no metadata was given by module)", 
		 uri);

	if (G_UNLIKELY (!indexer->private->in_transaction)) {
		start_transaction (indexer);
	}

	if (!query_resource_exists (indexer, uri)) {
		g_debug ("  File does not exist anyway "
			 "(uri:'%s')",
			 uri);
		return;
	}

	if (mime_type) {
		tracker_thumbnailer_remove (uri, mime_type);
	} else {
		g_message ("Could not get mime type to remove thumbnail for:'%s'",
			   uri);
	}

	/* Delete service */
	sparql = g_strdup_printf ("DELETE { <%s> a rdfs:Resource }", uri);
	tracker_resources_batch_sparql_update (indexer->private->client, sparql, NULL);
	g_free (sparql);

	/* TODO
	if (info->recurse && strcmp (service_type, "Folders") == 0) {
		tracker_data_update_delete_service_recursively (uri);
	}*/

#ifdef HAVE_HAL
	if (tracker_storage_uri_is_on_removable_device (indexer->private->storage,
							uri, 
							&mount_point,
							NULL)) {

		tracker_removable_device_add_removal (indexer, mount_point, uri);
	}
#endif

	g_free (mount_point);

}

static gboolean
item_process (TrackerIndexer *indexer,
	      PathInfo       *info,
	      const gchar    *uri)
{
	TrackerSparqlBuilder *sparql;
	gchar *text, *mime_type;

	sparql = tracker_module_file_get_metadata (info->module_file, &mime_type);

	if (tracker_module_file_is_cancelled (info->module_file)) {
		if (sparql) {
			g_object_unref (sparql);
		}

		return FALSE;
	}

	if (sparql) {
		text = tracker_module_file_get_text (info->module_file);

		if (tracker_module_file_is_cancelled (info->module_file)) {
			g_object_unref (sparql);
			g_free (text);

			return FALSE;
		}

		if (text) {
			tracker_sparql_builder_subject_iri (sparql, uri);
			tracker_sparql_builder_predicate (sparql, "nie:plainTextContent");
			tracker_sparql_builder_object_string (sparql, text);

			g_free (text);
		}

		item_add_or_update (indexer, info, uri, sparql, mime_type);

		g_object_unref (sparql);
	} else {
		item_remove (indexer, info, uri, mime_type);
	}

	g_free (mime_type);

	return TRUE;
}

static gboolean
should_change_index_for_file (TrackerIndexer *indexer,
			      PathInfo       *info,
			      const gchar    *uri)
{
	gboolean            uptodate;
	GPtrArray          *sparql_result;
	GFileInfo          *file_info;
	time_t              mtime;
	struct tm           t;
	gchar               *query;

	file_info = g_file_query_info (info->file, G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
	if (!file_info) {
		/* NOTE: We return TRUE here because we want to update the DB
		 * about this file, not because we want to index it.
		 */
		return TRUE;
	}

	mtime = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	g_object_unref (file_info);

	gmtime_r (&mtime, &t);

	query = g_strdup_printf ("SELECT ?file { ?file nfo:fileLastModified \"%04d-%02d-%02dT%02d:%02d:%02d\" . FILTER (?file = <%s>) }",
	                         t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, uri);
	sparql_result = tracker_resources_sparql_query (indexer->private->client, query, NULL);

	uptodate = (sparql_result && sparql_result->len == 1);

	tracker_dbus_results_ptr_array_free (&sparql_result);
	g_free (query);

	if (uptodate) {
		/* File already up-to-date in the database */
		return FALSE;
	}

	/* File either not yet in the database or mtime is different
	 * Update in database required
	 */
	return TRUE;
}

static gboolean
process_file (TrackerIndexer *indexer,
	      PathInfo	     *info)
{
	gchar *uri;
	gboolean inc_counters = FALSE;

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

	/* We don't check if we should index files for MOVE events */
	if (G_LIKELY (!info->source_file)) {
		if (!should_change_index_for_file (indexer, info, uri)) {
			indexer->private->items_processed++;

			g_free (uri);
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
		if (item_move (indexer, info, uri)) {
			inc_counters = TRUE;
		}
	} else {
		if (item_process (indexer, info, uri)) {
			inc_counters = TRUE;
		}
	}

	if (inc_counters) {
		indexer->private->subelements_processed++;
		indexer->private->items_processed++;
		indexer->private->items_to_index++;
	}

	g_free (uri);

	if (!tracker_module_file_is_cancelled (info->module_file) &&
	    TRACKER_IS_MODULE_ITERATABLE (info->module_file)) {
		return !tracker_module_iteratable_iter_contents (TRACKER_MODULE_ITERATABLE (info->module_file));
	}

	return TRUE;
}

static void
process_directory (TrackerIndexer *indexer,
		   PathInfo       *info)
{
	char *path;
	GFileEnumerator *enumerator;
	GFileInfo *file_info;

	path = g_file_get_path (info->file);
	g_debug ("Processing directory:'%s'", path);
	g_free (path);

	enumerator = g_file_enumerate_children (info->file, "standard::*", 0, NULL, NULL);

	if (!enumerator) {
		return;
	}

	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {
		PathInfo *new_info;
		GFile *child;

		if (g_file_info_get_is_hidden (file_info)) {
			g_object_unref (file_info);
			continue;
		}

		child = g_file_get_child (info->file, g_file_info_get_name (file_info));

		new_info = path_info_new (info->module, child, NULL, FALSE);
		add_file (indexer, new_info);

		if (info->recurse && g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY) {
			new_info = path_info_new (info->module, child, NULL, FALSE);
			add_directory (indexer, new_info);
		}

		g_object_unref (child);
		g_object_unref (file_info);
	}

	g_object_unref (enumerator);
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

#ifdef FIX
	dirs = tracker_module_config_get_monitor_recurse_directories (module_name);
#else 
	dirs = NULL;
#endif

	if (!dirs) {
		return;
	}

	g_message ("Starting module:'%s'", module_name);

	/* Signal module start/stop */
	process_module_emit_signals (indexer, module_name);

	for (d = dirs; d; d = d->next) {
		GFile *file;
		PathInfo *info;

		file = g_file_new_for_path (d->data);
		info = path_info_new (module, file, NULL, TRUE);
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
tracker_indexer_new (TrackerConfig  *config,
		     TrackerStorage *storage)
{
	return g_object_new (TRACKER_TYPE_INDEXER, 
			     "config", config,
			     "storage", storage,
			     NULL);
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

	if ((state & TRACKER_INDEXER_STATE_STOPPED) != 0) {
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
	} else {
		signal_status_timeout_start (indexer);

		if (indexer->private->idle_id == 0) {
			indexer->private->idle_id = g_idle_add (process_func, indexer);
		}
	}
}

static gchar *
state_to_string (TrackerIndexerState state)
{
	GString *s;
	gchar   *str, *p;

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

	str = g_string_free (s, FALSE);

	/* Remove last separator */
	p = g_utf8_strrchr (str, -1, '|');
	if (p) {
		/* Go back one to the space before '|' */
		p--;
		
		/* NULL terminate here */
		*p = '\0';
	}

	return str;
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

	} else if (!running && !(state & TRACKER_INDEXER_STATE_PAUSED)) {
		state_set_flags (indexer, TRACKER_INDEXER_STATE_PAUSED);
	}
}

void
tracker_indexer_stop (TrackerIndexer *indexer)
{
	g_return_if_fail (TRACKER_IS_INDEXER (indexer));

	check_stopped (indexer, TRUE);
}

void
tracker_indexer_pause (TrackerIndexer *indexer)
{
	if (tracker_indexer_get_running (indexer)) {
		tracker_indexer_set_running (indexer, FALSE);
	}
}

void
tracker_indexer_continue (TrackerIndexer *indexer)
{
	if (tracker_indexer_get_running (indexer) == FALSE) {
		tracker_indexer_set_running (indexer, TRUE);
	}
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
			     GStrv files)
{
	TrackerIndexerModule *module;
	gint i;

	g_return_if_fail (TRACKER_IS_INDEXER (indexer));
	g_return_if_fail (files != NULL);

	module = g_hash_table_lookup (indexer->private->indexer_modules, module_name);

	/* Add files to the queue */
	for (i = 0; files[i]; i++) {
		GFile *file;
		PathInfo *info;

		file = g_file_new_for_path (files[i]);
		info = path_info_new (module, file, NULL, TRUE);
		add_file (indexer, info);

		g_object_unref (file);
	}
}

void
tracker_indexer_file_move (TrackerIndexer	  *indexer,
			   const gchar		  *module_name,
			   const gchar		  *from,
			   const gchar		  *to)
{
	TrackerIndexerModule *module;
	PathInfo *info;
	GFile *file_from, *file_to;

	g_return_if_fail (TRACKER_IS_INDEXER (indexer));
	g_return_if_fail (from != NULL);
	g_return_if_fail (to != NULL);

	module = g_hash_table_lookup (indexer->private->indexer_modules, module_name);

	file_from = g_file_new_for_path (from);
	file_to = g_file_new_for_path (to);

	/* Add files to the queue */
	info = path_info_new (module, file_to, file_from, TRUE);
	add_file (indexer, info);

	g_object_unref (file_from);
	g_object_unref (file_to);
}

/* TODO: Fix volume handling in tracker-store / tracker-indexer */

#if 0
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
	guint  request_id;

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

	/* tracker_turtle_process_ttl will be spinning the mainloop, therefore 
	   we can already return the DBus method */

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);

	/* tracker_turtle_process_ttl will be spinning the mainloop, therefore
	   we can already return the DBus method */

	if (enabled) {
		tracker_removable_device_load (indexer, path);
	}

}
#endif


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
