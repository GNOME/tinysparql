/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include <string.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-hal.h>
#include <libtracker-common/tracker-module-config.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-db/tracker-db-index.h>
#include <libtracker-db/tracker-db-index-manager.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-processor.h"
#include "tracker-crawler.h"
#include "tracker-daemon.h"
#include "tracker-dbus.h"
#include "tracker-indexer-client.h"
#include "tracker-main.h"
#include "tracker-monitor.h"
#include "tracker-status.h"

#define TRACKER_PROCESSOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_PROCESSOR, TrackerProcessorPrivate))

/* This means, if we have <= 50 items in the queues, we will use a
 * g_idle_add() call instead of g_timeout_add() with
 * ITEMS_QUEUE_PROCESS_INTERVAL to make sure items get handled as
 * quickly as possible. NOTE: This is only used AFTER an initial
 * check of all files and index status.
 */ 
#define ITEMS_QUEUE_PROCESS_QUICK_COUNT 50

/* This is the interval we wait before attempting to send items to
 * the indexer (again).
 */
#define ITEMS_QUEUE_PROCESS_INTERVAL    2

/* This is the maximum number of items we send at one time to the
 * indexer 
 */
#define ITEMS_QUEUE_PROCESS_MAX         1000

typedef enum {
	SENT_TYPE_NONE,
	SENT_TYPE_CREATED,
	SENT_TYPE_UPDATED,
	SENT_TYPE_DELETED,
	SENT_TYPE_MOVED
} SentType;

struct TrackerProcessorPrivate {
	TrackerConfig  *config;
	TrackerHal     *hal;
	TrackerMonitor *monitor;

	DBusGProxy     *indexer_proxy;

	/* Crawlers */
	GHashTable     *crawlers;

	/* File queues for indexer */
	guint		item_queues_handler_id;

	GHashTable     *items_created_queues;
	GHashTable     *items_updated_queues;
	GHashTable     *items_deleted_queues;
	GHashTable     *items_moved_queues;

	SentType	sent_type;
	GStrv		sent_items;
	const gchar    *sent_module_name;

	/* Status */
	GList	       *modules;
	GList	       *current_module;

	GList          *devices;
	GList          *current_device;

	GTimer	       *timer;

	gboolean        been_started;
	gboolean	interrupted;

	gboolean	finished_modules;
	gboolean	finished_devices;
	gboolean	finished_sending;
	gboolean	finished_indexer;

	/* Statistics */
	guint		total_directories_found;
	guint		total_directories_ignored;
	guint		total_files_found;
	guint		total_files_ignored;

	guint		directories_found;
	guint		directories_ignored;
	guint		files_found;
	guint		files_ignored;

	guint		indexer_items_done;
	guint		indexer_items_remaining;

	gdouble		indexer_seconds_elapsed;
};

enum {
	FINISHED,
	LAST_SIGNAL
};

static void tracker_processor_finalize        (GObject          *object);
static void crawler_destroy_notify            (gpointer          data);
static void item_queue_destroy_notify         (gpointer          data);
static void process_module_next               (TrackerProcessor *processor);
static void process_modules_stop              (TrackerProcessor *processor);
static void process_device_next               (TrackerProcessor *processor);
static void process_devices_stop              (TrackerProcessor *processor);
static void process_check_completely_finished (TrackerProcessor *processor);
static void process_next                      (TrackerProcessor *processor);
static void indexer_status_cb                 (DBusGProxy       *proxy,
					       gdouble           seconds_elapsed,
					       const gchar      *current_module_name,
					       guint             items_processed,
					       guint             items_indexed,
					       guint             items_remaining,
					       gpointer          user_data);
static void indexer_started_cb                (DBusGProxy       *proxy,
					       gpointer          user_data);
static void indexer_finished_cb               (DBusGProxy       *proxy,
					       gdouble           seconds_elapsed,
					       guint             items_processed,
					       guint             items_indexed,
					       gboolean          interrupted,
					       gpointer          user_data);
static void monitor_item_created_cb           (TrackerMonitor   *monitor,
					       const gchar      *module_name,
					       GFile            *file,
					       gboolean          is_directory,
					       gpointer          user_data);
static void monitor_item_updated_cb           (TrackerMonitor   *monitor,
					       const gchar      *module_name,
					       GFile            *file,
					       gboolean          is_directory,
					       gpointer          user_data);
static void monitor_item_deleted_cb           (TrackerMonitor   *monitor,
					       const gchar      *module_name,
					       GFile            *file,
					       gboolean          is_directory,
					       gpointer          user_data);
static void monitor_item_moved_cb             (TrackerMonitor   *monitor,
					       const gchar      *module_name,
					       GFile            *file,
					       GFile            *other_file,
					       gboolean          is_directory,
					       gboolean          is_source_monitored,
					       gpointer          user_data);
static void crawler_processing_file_cb        (TrackerCrawler   *crawler,
					       const gchar      *module_name,
					       GFile            *file,
					       gpointer          user_data);
static void crawler_processing_directory_cb   (TrackerCrawler   *crawler,
					       const gchar      *module_name,
					       GFile            *file,
					       gpointer          user_data);
static void crawler_finished_cb               (TrackerCrawler   *crawler,
					       const gchar      *module_name,
					       guint             directories_found,
					       guint             directories_ignored,
					       guint             files_found,
					       guint             files_ignored,
					       gpointer          user_data);

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (TrackerProcessor, tracker_processor, G_TYPE_OBJECT)

static void
tracker_processor_class_init (TrackerProcessorClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = tracker_processor_finalize;

	signals[FINISHED] =
		g_signal_new ("finished",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerProcessorClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (object_class, sizeof (TrackerProcessorPrivate));
}

static void
tracker_processor_init (TrackerProcessor *object)
{
	TrackerProcessorPrivate *priv;
	GList			*l;

	object->private = TRACKER_PROCESSOR_GET_PRIVATE (object);

	priv = object->private;

	priv->modules = tracker_module_config_get_modules ();

	/* For each module we create a TrackerCrawler and keep them in
	 * a hash table to look up.
	 */
	priv->crawlers =
		g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       g_free,
				       crawler_destroy_notify);


	/* For each module we create a hash table for queues for items
	 * to update/create/delete in the indexer. This is sent on
	 * when the queue is processed.
	 */
	priv->items_created_queues =
		g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       g_free,
				       item_queue_destroy_notify);
	priv->items_updated_queues =
		g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       g_free,
				       item_queue_destroy_notify);
	priv->items_deleted_queues =
		g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       g_free,
				       item_queue_destroy_notify);
	priv->items_moved_queues =
		g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       g_free,
				       item_queue_destroy_notify);

	for (l = priv->modules; l; l = l->next) {
		/* Create queues for this module */
		g_hash_table_insert (priv->items_created_queues,
				     g_strdup (l->data),
				     g_queue_new ());
		g_hash_table_insert (priv->items_updated_queues,
				     g_strdup (l->data),
				     g_queue_new ());
		g_hash_table_insert (priv->items_deleted_queues,
				     g_strdup (l->data),
				     g_queue_new ());
		g_hash_table_insert (priv->items_moved_queues,
				     g_strdup (l->data),
				     g_queue_new ());
	}
}

static void
tracker_processor_finalize (GObject *object)
{
	TrackerProcessorPrivate *priv;

	priv = TRACKER_PROCESSOR_GET_PRIVATE (object);

	if (priv->timer) {
		g_timer_destroy (priv->timer);
	}

	if (priv->item_queues_handler_id) {
		g_source_remove (priv->item_queues_handler_id);
		priv->item_queues_handler_id = 0;
	}

	if (priv->items_moved_queues) {
		g_hash_table_unref (priv->items_moved_queues);
	}

	if (priv->items_deleted_queues) {
		g_hash_table_unref (priv->items_deleted_queues);
	}

	if (priv->items_updated_queues) {
		g_hash_table_unref (priv->items_updated_queues);
	}

	if (priv->items_created_queues) {
		g_hash_table_unref (priv->items_created_queues);
	}

	if (priv->crawlers) {
		g_hash_table_unref (priv->crawlers);
	}

	g_list_free (priv->modules);

	dbus_g_proxy_disconnect_signal (priv->indexer_proxy, "Started",
					G_CALLBACK (indexer_started_cb),
					NULL);
	dbus_g_proxy_disconnect_signal (priv->indexer_proxy, "Finished",
					G_CALLBACK (indexer_finished_cb),
					NULL);
	dbus_g_proxy_disconnect_signal (priv->indexer_proxy, "Status",
					G_CALLBACK (indexer_status_cb),
					NULL);
	g_object_unref (priv->indexer_proxy);

	g_signal_handlers_disconnect_by_func (priv->monitor,
					      G_CALLBACK (monitor_item_deleted_cb),
					      object);
	g_signal_handlers_disconnect_by_func (priv->monitor,
					      G_CALLBACK (monitor_item_updated_cb),
					      object);
	g_signal_handlers_disconnect_by_func (priv->monitor,
					      G_CALLBACK (monitor_item_created_cb),
					      object);
	g_signal_handlers_disconnect_by_func (priv->monitor,
					      G_CALLBACK (monitor_item_moved_cb),
					      object);
	g_object_unref (priv->monitor);

#ifdef HAVE_HAL
	if (priv->devices) {
		g_list_foreach (priv->devices, (GFunc) g_free, NULL);
		g_list_free (priv->devices);
	}

	if (priv->hal) {
		g_object_unref (priv->hal);
	}
#endif /* HAVE_HAL */

	g_object_unref (priv->config);

	G_OBJECT_CLASS (tracker_processor_parent_class)->finalize (object);
}


static void
get_remote_roots (TrackerProcessor  *processor,
		  GList	           **mounted_directory_roots,
		  GList	           **removable_device_roots)
{
	GList *l1;
	GList *l2;

#ifdef HAVE_HAL
	l1 = tracker_hal_get_mounted_directory_roots (processor->private->hal);
	l2 = tracker_hal_get_removable_device_roots (processor->private->hal);
#else  /* HAVE_HAL */
	l1 = NULL;
	l2 = NULL;
#endif /* HAVE_HAL */

	/* The options to index removable media and the index mounted
	 * directories are both mutually exclusive even though
	 * removable media is mounted on a directory.
	 *
	 * Since we get ALL mounted directories from HAL, we need to
	 * remove those which are removable device roots.
	 */
	if (l2) {
		GList *l;
		GList *list = NULL;

		for (l = l1; l; l = l->next) {
			if (g_list_find_custom (l2, l->data, (GCompareFunc) g_strcmp0)) {
				continue;
			}

			list = g_list_prepend (list, l->data);
		}

		*mounted_directory_roots = g_list_reverse (list);
	} else {
		*mounted_directory_roots = NULL;
	}

	*removable_device_roots = g_list_copy (l2);
}

static gboolean
path_should_be_ignored_for_media (TrackerProcessor *processor,
				  const gchar	   *path)
{
	GList	 *roots = NULL;
	GList	 *mounted_directory_roots = NULL;
	GList	 *removable_device_roots = NULL;
	GList	 *l;
	gboolean  ignore_mounted_directories;
	gboolean  ignore_removable_devices;
	gboolean  ignore = FALSE;

	ignore_mounted_directories =
		!tracker_config_get_index_mounted_directories (processor->private->config);
	ignore_removable_devices =
		!tracker_config_get_index_removable_devices (processor->private->config);

	if (ignore_mounted_directories || ignore_removable_devices) {
		get_remote_roots (processor,
				  &mounted_directory_roots,
				  &removable_device_roots);
	}

	if (ignore_mounted_directories) {
		roots = g_list_concat (roots, mounted_directory_roots);
	}

	if (ignore_removable_devices) {
		roots = g_list_concat (roots, removable_device_roots);
	}

	for (l = roots; l && !ignore; l = l->next) {
		/* If path matches a mounted or removable device by
		 * prefix then we should ignore it since we don't
		 * crawl those by choice in the config.
		 */
		if (strcmp (path, l->data) == 0) {
			ignore = TRUE;
		}

		/* FIXME: Should we add a DIR_SEPARATOR on the end of
		 * these before comparing them?
		 */
		if (g_str_has_prefix (path, l->data)) {
			ignore = TRUE;
		}
	}

	g_list_free (roots);

	return ignore;
}

static GQueue *
get_next_queue_with_data (GList       *modules,
			  GHashTable  *hash_table,
			  gchar      **module_name)
{
	GQueue *found_queue;
	GQueue *queue;
	GList  *l;

	if (module_name) {
		*module_name = NULL;
	}

	for (l = modules, found_queue = NULL; l && !found_queue; l = l->next) {
		queue = g_hash_table_lookup (hash_table, l->data);

		if (g_queue_get_length (queue) > 0) {
			if (module_name) {
				*module_name = l->data;
				found_queue = queue;
			}
		}
	}

	return found_queue;
}

static void
crawler_destroy_notify (gpointer data)
{
	TrackerCrawler *crawler;

	crawler = TRACKER_CRAWLER (data);

	if (crawler) {
		guint lsignals;

		lsignals = g_signal_handlers_disconnect_matched (crawler,
								 G_SIGNAL_MATCH_FUNC,
								 0,
								 0,
								 NULL,
								 G_CALLBACK (crawler_processing_file_cb),
								 NULL);
		lsignals = g_signal_handlers_disconnect_matched (crawler,
								 G_SIGNAL_MATCH_FUNC,
								 0,
								 0,
								 NULL,
								 G_CALLBACK (crawler_processing_directory_cb),
								 NULL);
		lsignals = g_signal_handlers_disconnect_matched (crawler,
								 G_SIGNAL_MATCH_FUNC,
								 0,
								 0,
								 NULL,
								 G_CALLBACK (crawler_finished_cb),
								 NULL);

		g_object_unref (crawler);
	}
}

static void
item_queue_destroy_notify (gpointer data)
{
	GQueue *queue;

	queue = (GQueue *) data;

	g_queue_foreach (queue, (GFunc) g_object_unref, NULL);
	g_queue_free (queue);
}

static void
item_queue_readd_items (GQueue *queue,
			GStrv	strv)
{
	if (queue) {
		GStrv p;
		gint  i;

		for (p = strv, i = 0; *p; p++, i++) {
			g_queue_push_nth (queue, g_file_new_for_path (*p), i);
		}
	}
}

static guint 
item_queue_count_all (TrackerProcessor *processor)
{
	GList *l;
	guint  items = 0;

	for (l = processor->private->modules; l; l = l->next) {
		GQueue *q;

		q = g_hash_table_lookup (processor->private->items_created_queues, l->data);
		items += g_queue_get_length (q);

		q = g_hash_table_lookup (processor->private->items_updated_queues, l->data);
		items += g_queue_get_length (q);

		q = g_hash_table_lookup (processor->private->items_deleted_queues, l->data);
		items += g_queue_get_length (q);

		/* This queue has 2 items per transaction, from and
		 * to files so we half the length for the number of
		 * items to do in it.
		 */
		q = g_hash_table_lookup (processor->private->items_moved_queues, l->data);
		items += (g_queue_get_length (q) / 2);
	}

	return items;
}

static void
item_queue_processed_cb (DBusGProxy *proxy,
			 GError     *error,
			 gpointer    user_data)
{
	TrackerProcessor *processor;

	processor = user_data;

	if (error) {
		GQueue *queue;

		g_message ("Items could not be processed by the indexer, %s",
			   error->message);
		g_error_free (error);

		/* Put files back into queue */
		switch (processor->private->sent_type) {
		case SENT_TYPE_CREATED:
			queue = g_hash_table_lookup (processor->private->items_created_queues,
						     processor->private->sent_module_name);
			break;
		case SENT_TYPE_UPDATED:
			queue = g_hash_table_lookup (processor->private->items_updated_queues,
						     processor->private->sent_module_name);
			break;
		case SENT_TYPE_DELETED:
			queue = g_hash_table_lookup (processor->private->items_deleted_queues,
						     processor->private->sent_module_name);
			break;
		case SENT_TYPE_MOVED:
			queue = g_hash_table_lookup (processor->private->items_moved_queues,
						     processor->private->sent_module_name);
			break;
		case SENT_TYPE_NONE:
		default:
			queue = NULL;
			break;
		}

		item_queue_readd_items (queue, processor->private->sent_items);
	}

	g_strfreev (processor->private->sent_items);

	/* Reset for next batch to be sent */
	processor->private->sent_items = NULL;
	processor->private->sent_module_name = NULL;
	processor->private->sent_type = SENT_TYPE_NONE;
}

static gboolean
item_queue_handlers_cb (gpointer user_data)
{
	TrackerProcessor *processor;
	TrackerStatus     status;
	GQueue		 *queue;
	GStrv		  files;
	gchar		 *module_name;
	gboolean          should_repeat = FALSE;
	GTimeVal          time_now;
	static GTimeVal   time_last = { 0, 0 };

	processor = user_data;

	status = tracker_status_get ();

	/* Don't spam */
	g_get_current_time (&time_now);

	should_repeat = (time_now.tv_sec - time_last.tv_sec) >= 10;
	if (should_repeat) {
		time_last = time_now;
	}

	switch (status) {
	case TRACKER_STATUS_PAUSED:
		/* This way we don't send anything to the indexer from
		 * monitor events but we still queue them ready to
		 * send when we are unpaused.  
		 */
		if (should_repeat) {
			g_message ("We are paused, sending nothing to the "
				   "indexer until we are unpaused");
		}

	case TRACKER_STATUS_PENDING:
	case TRACKER_STATUS_WATCHING:
		/* Wait until we have finished crawling before
		 * sending anything.
		 */
		return TRUE;

	default:
		break;
	}

	/* This is here so we don't try to send something if we are
	 * still waiting for a response from the last send.
	 */
	if (processor->private->sent_type != SENT_TYPE_NONE) {
		if (should_repeat) {
			g_message ("Still waiting for response from indexer, "
				   "not sending more files yet");
		}

		return TRUE;
	}

	/* Process the deleted items first */
	queue = get_next_queue_with_data (processor->private->modules,
					  processor->private->items_deleted_queues,
					  &module_name);

	if (queue) {
		files = tracker_dbus_queue_gfile_to_strv (queue, ITEMS_QUEUE_PROCESS_MAX);

		g_message ("Queue for module:'%s' deleted items processed, sending first %d to the indexer",
			   module_name,
			   g_strv_length (files));

		processor->private->finished_indexer = FALSE;

		processor->private->sent_type = SENT_TYPE_DELETED;
		processor->private->sent_module_name = module_name;
		processor->private->sent_items = files;

		org_freedesktop_Tracker_Indexer_files_delete_async (processor->private->indexer_proxy,
								    module_name,
								    (const gchar **) files,
								    item_queue_processed_cb,
								    processor);

		return TRUE;
	}

	/* Process the created items next */
	queue = get_next_queue_with_data (processor->private->modules,
					  processor->private->items_created_queues,
					  &module_name);

	if (queue) {
		/* Now we try to send items to the indexer */
		tracker_status_set_and_signal (TRACKER_STATUS_INDEXING);

		files = tracker_dbus_queue_gfile_to_strv (queue, ITEMS_QUEUE_PROCESS_MAX);

		g_message ("Queue for module:'%s' created items processed, sending first %d to the indexer",
			   module_name,
			   g_strv_length (files));

		processor->private->finished_indexer = FALSE;

		processor->private->sent_type = SENT_TYPE_CREATED;
		processor->private->sent_module_name = module_name;
		processor->private->sent_items = files;

		org_freedesktop_Tracker_Indexer_files_check_async (processor->private->indexer_proxy,
								   module_name,
								   (const gchar **) files,
								   item_queue_processed_cb,
								   processor);

		return TRUE;
	}

	/* Process the updated items next */
	queue = get_next_queue_with_data (processor->private->modules,
					  processor->private->items_updated_queues,
					  &module_name);

	if (queue) {
		/* Now we try to send items to the indexer */
		tracker_status_set_and_signal (TRACKER_STATUS_INDEXING);

		files = tracker_dbus_queue_gfile_to_strv (queue, ITEMS_QUEUE_PROCESS_MAX);

		g_message ("Queue for module:'%s' updated items processed, sending first %d to the indexer",
			   module_name,
			   g_strv_length (files));

		processor->private->finished_indexer = FALSE;

		processor->private->sent_type = SENT_TYPE_UPDATED;
		processor->private->sent_module_name = module_name;
		processor->private->sent_items = files;

		org_freedesktop_Tracker_Indexer_files_update_async (processor->private->indexer_proxy,
								    module_name,
								    (const gchar **) files,
								    item_queue_processed_cb,
								    processor);

		return TRUE;
	}

	/* Process the moved items last */
	queue = get_next_queue_with_data (processor->private->modules,
					  processor->private->items_moved_queues,
					  &module_name);

	if (queue) {
		guint length;

		/* Now we try to send items to the indexer */
		tracker_status_set_and_signal (TRACKER_STATUS_INDEXING);

		/* Get queue length, if length is indicates more than
		 * one file has changed, then we call the
		 * indexer_files_move() API instead which can handle
		 * multiple files.
		 */
		length = g_queue_get_length (queue);

		if (length > 0) {
			gint max_items;
			gint multiples;

			multiples = length / 2;
			max_items = MIN (length, ITEMS_QUEUE_PROCESS_MAX * 2);

			if (multiples > 1) {
				files = tracker_dbus_queue_gfile_to_strv (queue, max_items);
			} else {
				files = tracker_dbus_queue_gfile_to_strv (queue, 2);
			}

			if (multiples > 1) {
				GStrv strv_from, strv_to;
				gint i, j;

				strv_from = g_new0 (gchar*, length + 1);
				strv_to = g_new0 (gchar*, length + 1);

				/* Split items into two separate GStrvs */
				for (i = 0, j = 0; i < max_items;) {
					gchar *path_from, *path_to;

					path_from = files[i++];
					path_to   = files[i++];
					
					if (!g_utf8_validate (path_from, -1, NULL)) {
						g_message ("Could not add string:'%s' to GStrv, invalid UTF-8", path_from);
						g_free (path_from);
						continue;
					}

					if (!g_utf8_validate (path_to, -1, NULL)) {
						g_message ("Could not add string:'%s' to GStrv, invalid UTF-8", path_to);
						g_free (path_from);
						g_free (path_to);
						continue;
					}
					
					strv_from[j] = path_from;
					strv_to[j] = path_to;
					j++;
				}
				
				strv_from[j] = NULL;
				strv_to[j] = NULL;

				g_message ("Queue for module:'%s' moved items processed, sending first %d to the indexer",
					   module_name,
					   j);
				
				processor->private->finished_indexer = FALSE;
				
				processor->private->sent_type = SENT_TYPE_MOVED;
				processor->private->sent_module_name = module_name;
				processor->private->sent_items = files;

				org_freedesktop_Tracker_Indexer_files_move_async (processor->private->indexer_proxy,
										  module_name,
										  (const gchar**) strv_from,
										  (const gchar**) strv_to,
										  item_queue_processed_cb,
										  processor);
			} else {
				const gchar *source;
				const gchar *target;

				if (files) {
					source = files[0];
					target = files[1];
				} else {
					source = NULL;
					target = NULL;
				}

				g_message ("Queue for module:'%s' moved items processed, sending first %d to the indexer",
					   module_name,
					   g_strv_length (files));

				processor->private->finished_indexer = FALSE;
				
				processor->private->sent_type = SENT_TYPE_MOVED;
				processor->private->sent_module_name = module_name;
				processor->private->sent_items = files;

				org_freedesktop_Tracker_Indexer_file_move_async (processor->private->indexer_proxy,
										 module_name,
										 source,
										 target,
										 item_queue_processed_cb,
										 processor);

			}
		}

		return TRUE;
	}

	g_message ("No items in any queues to process, doing nothing");
	processor->private->item_queues_handler_id = 0;

	processor->private->finished_sending = TRUE;
	process_check_completely_finished (processor);

	return FALSE;
}

static void
item_queue_handlers_set_up (TrackerProcessor *processor)
{
	processor->private->finished_sending = FALSE;

	if (processor->private->item_queues_handler_id != 0) {
		return;
	}

	processor->private->item_queues_handler_id =
		g_timeout_add_seconds (ITEMS_QUEUE_PROCESS_INTERVAL,
				       item_queue_handlers_cb,
				       processor);

	if (!tracker_status_get_is_initial_check ()) {
		guint count;

		/* Get items left to handle */
		count = item_queue_count_all (processor);
		
		if (count <= ITEMS_QUEUE_PROCESS_QUICK_COUNT) {
			g_message ("Only %d items queued currently, setting up quick handler", 
				   count);
			item_queue_handlers_cb (processor);
		}
	}
}

static gboolean
is_path_on_ignore_list (GSList	    *ignore_list,
			const gchar *path)
{
	GSList *l;

	if (!ignore_list || !path) {
		return FALSE;
	}

	for (l = ignore_list; l; l = l->next) {
		if (strcmp (path, l->data) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
process_module_files_add_legacy_options (TrackerProcessor *processor)
{
	TrackerCrawler *crawler;
	GSList	       *no_watch_roots;
	GSList	       *watch_roots;
	GSList	       *crawl_roots;
	GSList	       *l;
	guint		watch_root_count;
	guint		crawl_root_count;
	const gchar    *module_name = "files";

	crawler = g_hash_table_lookup (processor->private->crawlers, module_name);

	tracker_crawler_use_module_paths (crawler, TRUE);
	tracker_crawler_special_paths_clear (crawler);

	no_watch_roots = tracker_config_get_no_watch_directory_roots (processor->private->config);
	watch_roots = tracker_config_get_watch_directory_roots (processor->private->config);
	crawl_roots = tracker_config_get_crawl_directory_roots (processor->private->config);

	watch_root_count = 0;
	crawl_root_count = 0;

	/* This module get special treatment to make sure legacy
	 * options are supported.
	 */

	/* Print ignored locations */
	g_message ("  User ignored crawls & monitors:");
	for (l = no_watch_roots; l; l = l->next) {
		g_message ("    %s", (gchar*) l->data);
	}

	if (g_slist_length (no_watch_roots) == 0) {
		g_message ("    NONE");
	}

	/* Add monitors, from WatchDirectoryRoots config key */
	g_message ("  User monitors being added:");
	for (l = watch_roots; l; l = l->next) {
		GFile *file;

		if (is_path_on_ignore_list (no_watch_roots, l->data)) {
			continue;
		}

		g_message ("    %s", (gchar*) l->data);

		file = g_file_new_for_path (l->data);
		tracker_monitor_add (processor->private->monitor, module_name, file);
		g_object_unref (file);

		watch_root_count++;
	}

	if (g_slist_length (watch_roots) == 0) {
		g_message ("    NONE");
	}

	/* Add crawls, from WatchDirectoryRoots and
	 * CrawlDirectoryRoots config keys.
	 */
	g_message ("  User crawls being added:");

	for (l = watch_roots; l; l = l->next) {
		if (is_path_on_ignore_list (no_watch_roots, l->data)) {
			continue;
		}

		g_message ("    %s", (gchar*) l->data);
		tracker_crawler_special_paths_add (crawler, l->data);
	}

	for (l = crawl_roots; l; l = l->next) {
		if (is_path_on_ignore_list (no_watch_roots, l->data)) {
			continue;
		}

		g_message ("    %s", (gchar*) l->data);
		tracker_crawler_special_paths_add (crawler, l->data);

		crawl_root_count++;
	}

	if (g_slist_length (watch_roots) == 0 &&
	    g_slist_length (crawl_roots) == 0) {
		g_message ("    NONE");
	}
}

static gboolean
process_module_is_disabled (TrackerProcessor *processor,
			    const gchar      *module_name)
{
	GSList *disabled_modules;
	
	if (!tracker_module_config_get_enabled (module_name)) {
		g_message ("  Module disabled by module config");
		return TRUE;
	} 

	disabled_modules = tracker_config_get_disabled_modules (processor->private->config);
	
	if (g_slist_find_custom (disabled_modules, module_name, (GCompareFunc) g_strcmp0)) {
		g_message ("  Module disabled by user");
		return TRUE;
	} 

	return FALSE;
}

static void
process_module (TrackerProcessor *processor,
		const gchar	 *module_name)
{
	TrackerCrawler *crawler;

	g_message ("Processing module:'%s'", module_name);

	if (process_module_is_disabled (processor, module_name)) {
		process_module_next (processor);
		return;
	}

	/* Here we set up legacy .cfg options like watch roots */
	tracker_status_set_and_signal (TRACKER_STATUS_WATCHING);

	if (strcmp (module_name, "files") == 0) {
		process_module_files_add_legacy_options (processor);
	}

	/* Gets all files and directories */
	tracker_status_set_and_signal (TRACKER_STATUS_PENDING);

	crawler = g_hash_table_lookup (processor->private->crawlers, module_name);

	if (!tracker_crawler_start (crawler)) {
		process_module_next (processor);
	}
}

static void
process_module_next (TrackerProcessor *processor)
{
	if (tracker_status_get_is_readonly ()) {
		/* Block any request to process
		 * modules if indexing is not enabled
		 */
		return;
	}

	/* Don't recursively iterate the modules */
	if (!processor->private->current_module) {
		if (!processor->private->finished_modules) {
			processor->private->current_module = processor->private->modules;
		}
	} else {
		processor->private->current_module = processor->private->current_module->next;
	}

	/* If we have no further modules to iterate */
	if (!processor->private->current_module) {
		process_modules_stop (processor);
		process_next (processor);
		return;
	}

	process_module (processor, processor->private->current_module->data);
}

static void
process_device (TrackerProcessor *processor,
		const gchar	 *device_root)
{
	TrackerCrawler *crawler;
	GFile          *file;
	const gchar    *module_name = "files";

	g_message ("Processing device with root:'%s'", device_root);

	if (process_module_is_disabled (processor, module_name)) {
		process_device_next (processor);
		return;
	}

	/* Here we set up legacy .cfg options like watch roots */
	tracker_status_set_and_signal (TRACKER_STATUS_WATCHING);

	/* Gets all files and directories */
	tracker_status_set_and_signal (TRACKER_STATUS_PENDING);

	crawler = g_hash_table_lookup (processor->private->crawlers, module_name);

	tracker_crawler_use_module_paths (crawler, FALSE);
	tracker_crawler_special_paths_clear (crawler);

	if (path_should_be_ignored_for_media (processor, device_root)) {
		g_message ("  Ignored due to config");
		process_device_next (processor);
		return;
	}

	file = g_file_new_for_path (device_root);
	tracker_monitor_add (processor->private->monitor, module_name, file);
	g_object_unref (file);
	
	tracker_crawler_special_paths_add (crawler, device_root);

	if (!tracker_crawler_start (crawler)) {
		process_device_next (processor);
	}
}

static void
process_device_next (TrackerProcessor *processor)
{
	if (tracker_status_get_is_readonly ()) {
		/* Block any request to process
		 * modules if indexing is not enabled
		 */
		return;
	}

	/* Don't recursively iterate the devices */
	if (!processor->private->current_device) {
		if (!processor->private->finished_devices) {
			processor->private->current_device = processor->private->devices;
		}
	} else {
		GList *l;

		l = processor->private->current_device;
		
		/* Now free that device so we don't recrawl it */
		if (l) {
			g_free (l->data);
			
			processor->private->current_device = 
			processor->private->devices = 
				g_list_delete_link (processor->private->devices, l);
		}
	}

	/* If we have no further devices to iterate */
	if (!processor->private->current_device) {
		process_devices_stop (processor);
		process_next (processor);
		return;
	}

	process_device (processor, processor->private->current_device->data);
}

static void
process_modules_start (TrackerProcessor *processor)
{
	g_message ("Processor has started iterating %d modules", 
		   g_list_length (processor->private->modules));

	if (processor->private->timer) {
		g_timer_destroy (processor->private->timer);
	}

	processor->private->timer = g_timer_new ();

	processor->private->finished_modules = FALSE;

	processor->private->directories_found = 0;
	processor->private->directories_ignored = 0;
	processor->private->files_found = 0;
	processor->private->files_ignored = 0;

	process_module_next (processor);
}

static void
process_modules_stop (TrackerProcessor *processor)
{
	if (processor->private->finished_modules) {
		return;
	}

	g_message ("--------------------------------------------------");
	g_message ("Processor has %s iterating modules",
		   processor->private->interrupted ? "been stopped while" : "finished");

	processor->private->finished_modules = TRUE;

	if (processor->private->interrupted) {
		TrackerCrawler *crawler;

		crawler = g_hash_table_lookup (processor->private->crawlers,
					       processor->private->current_module->data);
		if (crawler) {
			tracker_crawler_stop (crawler);
		}

		if (processor->private->timer) {
			g_timer_destroy (processor->private->timer);
			processor->private->timer = NULL;
		}
	} else {
		gdouble elapsed;
	
		if (processor->private->timer) {
			g_timer_stop (processor->private->timer);
			elapsed = g_timer_elapsed (processor->private->timer, NULL);
		} else {
			elapsed = 0;
		}
		
		g_message ("Module time taken : %4.4f seconds",
			   elapsed);
		g_message ("Module directories: %d (%d ignored)",
			   processor->private->directories_found,
			   processor->private->directories_ignored);
		g_message ("Module files      : %d (%d ignored)",
			   processor->private->files_found,
			   processor->private->files_ignored);
	}

	g_message ("--------------------------------------------------\n");
}

static void
process_devices_start (TrackerProcessor *processor)
{
	g_message ("Processor has started iterating %d devices", 
		   g_list_length (processor->private->devices));

	if (processor->private->timer) {
		g_timer_destroy (processor->private->timer);
	}

	processor->private->timer = g_timer_new ();

	processor->private->finished_devices = FALSE;

	processor->private->directories_found = 0;
	processor->private->directories_ignored = 0;
	processor->private->files_found = 0;
	processor->private->files_ignored = 0;

	process_device_next (processor);
}

static void
process_devices_stop (TrackerProcessor *processor)
{
	if (processor->private->finished_devices) {
		return;
	}

	g_message ("--------------------------------------------------");
	g_message ("Processor has %s iterating devices",
		   processor->private->interrupted ? "been stopped while" : "finished");

	processor->private->finished_devices = TRUE;

	if (processor->private->interrupted) {
		TrackerCrawler *crawler;

		crawler = g_hash_table_lookup (processor->private->crawlers, "files");
		if (crawler) {
			tracker_crawler_stop (crawler);
		}

		if (processor->private->timer) {
			g_timer_destroy (processor->private->timer);
			processor->private->timer = NULL;
		}
	} else {
		gdouble elapsed;
	
		if (processor->private->timer) {
			g_timer_stop (processor->private->timer);
			elapsed = g_timer_elapsed (processor->private->timer, NULL);
		} else {
			elapsed = 0;
		}
		
		g_message ("Device time taken : %4.4f seconds",
			   elapsed);
		g_message ("Device directories: %d (%d ignored)",
			   processor->private->directories_found,
			   processor->private->directories_ignored);
		g_message ("Device files      : %d (%d ignored)",
			   processor->private->files_found,
			   processor->private->files_ignored);
	}
	
	g_message ("--------------------------------------------------\n");
}

static void
process_continue (TrackerProcessor *processor)
{
	if (!processor->private->finished_modules) {
		process_module_next (processor);
		return;
	}

	if (!processor->private->finished_devices) {
		process_device_next (processor);
		return;
	}

	/* Nothing to do */
}

static void
process_next (TrackerProcessor *processor)
{
	if (!processor->private->finished_modules) {
		process_modules_start (processor);
		return;
	}

	if (!processor->private->finished_devices) {
		process_devices_start (processor);
		return;
	}

	/* Only do this the first time, otherwise the results are
	 * likely to be inaccurate. Devices can be added or removed so
	 * we can't assume stats are correct.
	 */
	if (tracker_status_get_is_initial_check ()) {
		g_message ("--------------------------------------------------");
		g_message ("Total directories : %d (%d ignored)",
			   processor->private->total_directories_found,
			   processor->private->total_directories_ignored);
		g_message ("Total files       : %d (%d ignored)",
			   processor->private->total_files_found,
			   processor->private->total_files_ignored);
		g_message ("Total monitors    : %d",
			   tracker_monitor_get_count (processor->private->monitor, NULL));
		g_message ("--------------------------------------------------\n");
	}

	/* Now we have finished crawling, we enable monitor events */
	g_message ("Enabling monitor events");
	tracker_monitor_set_enabled (processor->private->monitor, TRUE);

	/* Now we set the state to IDLE, the reason we do this, is it
	 * allows us to either return to an idle state if there was
	 * nothing to do, OR it allows us to start handling the
	 * queues of files we just crawled. The queue handler won't
	 * send files to the indexer while we are PENDING or WATCHING.
	 */
	tracker_status_set_and_signal (TRACKER_STATUS_IDLE);	
}

static void
process_finish (TrackerProcessor *processor)
{
	/* Optimize DBs */
	tracker_status_set_and_signal (TRACKER_STATUS_OPTIMIZING);
	tracker_db_manager_optimize ();
	
	/* All done */
	tracker_status_set_and_signal (TRACKER_STATUS_IDLE);
	
	/* Signal to the applet we are finished */
	g_signal_emit_by_name (tracker_dbus_get_object (TRACKER_TYPE_DAEMON),
			       "index-finished",
			       processor->private->indexer_seconds_elapsed);
	
	/* Set our internal state */
	tracker_status_set_is_initial_check (FALSE);
	
	g_signal_emit (processor, signals[FINISHED], 0);
}

static void
process_check_completely_finished (TrackerProcessor *processor)
{
	if (!processor->private->finished_sending ||
	    !processor->private->finished_indexer) {
		return;
	}
	
	process_finish (processor);
}

static void
indexer_status_cb (DBusGProxy  *proxy,
		   gdouble	seconds_elapsed,
		   const gchar *current_module_name,
		   guint        items_processed,
		   guint	items_indexed,
		   guint	items_remaining,
		   gpointer	user_data)
{
	TrackerProcessor *processor;
	TrackerDBIndex	 *index;
	GQueue		 *queue;
	GFile		 *file;
	gchar		 *path = NULL;
	gchar		 *str1;
	gchar		 *str2;

	processor = user_data;

	/* Update our local copy */
	processor->private->indexer_items_done = items_processed;
	processor->private->indexer_items_remaining = items_remaining;
	processor->private->indexer_seconds_elapsed = seconds_elapsed;

	if (items_remaining < 1 ||
	    current_module_name == NULL ||
	    current_module_name[0] == '\0') {
		return;
	}

	/* Signal to any applications */
	queue = g_hash_table_lookup (processor->private->items_created_queues, current_module_name);
	file = g_queue_peek_tail (queue);
	if (file) {
		path = g_file_get_path (file);
	}

	g_signal_emit_by_name (tracker_dbus_get_object (TRACKER_TYPE_DAEMON),
			       "index-progress",
			       tracker_module_config_get_index_service (current_module_name),
			       path ? path : "",
			       items_processed,
			       items_remaining,
			       items_processed + items_remaining,
			       seconds_elapsed);
	g_free (path);

	/* Tell the index that it can reload, really we should do
	 * module_name->index type so we don't do this for both
	 * every time:
	 */
	index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_FILE);
	tracker_db_index_set_reload (index, TRUE);

	index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_EMAIL);
	tracker_db_index_set_reload (index, TRUE);

	/* Message to the console about state */
	str1 = tracker_seconds_estimate_to_string (seconds_elapsed,
						   TRUE,
						   items_processed,
						   items_remaining);
	str2 = tracker_seconds_to_string (seconds_elapsed, TRUE);

	g_message ("Processed %d/%d, indexed %d, module:'%s', %s left, %s elapsed",
		   items_processed,
		   items_processed + items_remaining,
		   items_indexed,
		   current_module_name,
		   str1,
		   str2);

	g_free (str2);
	g_free (str1);
}

static void
indexer_started_cb (DBusGProxy  *proxy,
		    gpointer	 user_data)
{
	TrackerProcessor *processor;

	processor = user_data;

	processor->private->finished_indexer = FALSE;
}

static void
indexer_finished_cb (DBusGProxy  *proxy,
		    gdouble	 seconds_elapsed,
		    guint        items_processed,
		    guint	 items_indexed,
		    gboolean	 interrupted,
		    gpointer	 user_data)
{
	TrackerProcessor *processor;
	TrackerDBIndex	 *index;
	gchar		 *str;

	processor = user_data;

	processor->private->indexer_items_done = items_processed;
	processor->private->indexer_items_remaining = 0;
	processor->private->indexer_seconds_elapsed = seconds_elapsed;

	/* Signal to any applications */
	g_signal_emit_by_name (tracker_dbus_get_object (TRACKER_TYPE_DAEMON),
			       "index-progress",
			       "", /* Service */
			       "", /* Path */
			       items_processed,
			       0,
			       items_processed,
			       seconds_elapsed);

	/* Tell the index that it can reload, really we should do
	 * module_name->index type so we don't do this for both
	 * every time:
	 */
	index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_FILE);
	tracker_db_index_set_reload (index, TRUE);

	index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_EMAIL);
	tracker_db_index_set_reload (index, TRUE);

	/* Message to the console about state */
	str = tracker_seconds_to_string (seconds_elapsed, FALSE);

	g_message ("Indexer finished last batch in %s, %d items processed in total (%d indexed)",
		   str,
		   items_processed,
		   items_indexed);
	g_free (str);

	/* Save indexer's state */
	processor->private->finished_indexer = TRUE;
	process_check_completely_finished (processor);
}

static void
processor_files_check (TrackerProcessor *processor,
		       const gchar	*module_name,
		       GFile		*file,
		       gboolean		 is_directory)
{
	TrackerCrawler *crawler;
	GQueue	       *queue;
	gboolean	ignored;
	gchar	       *path;

	path = g_file_get_path (file);
	crawler = g_hash_table_lookup (processor->private->crawlers, module_name);
	ignored = tracker_crawler_is_path_ignored (crawler, path, is_directory);

	g_debug ("%s:'%s' (%s) (create monitor event or user request)",
		 ignored ? "Ignored" : "Found ",
		 path,
		 is_directory ? "DIR" : "FILE");

	if (!ignored) {
		if (is_directory) {
			tracker_crawler_add_unexpected_path (crawler, path);
		}

		queue = g_hash_table_lookup (processor->private->items_created_queues, module_name);
		g_queue_push_tail (queue, g_object_ref (file));
			
		item_queue_handlers_set_up (processor);
	}

	g_free (path);
}

static void
processor_files_update (TrackerProcessor *processor,
			const gchar	 *module_name,
			GFile		 *file,
			gboolean	  is_directory)
{
	TrackerCrawler *crawler;
	GQueue	       *queue;
	gchar	       *path;
	gboolean	ignored;

	path = g_file_get_path (file);
	crawler = g_hash_table_lookup (processor->private->crawlers, module_name);
	ignored = tracker_crawler_is_path_ignored (crawler, path, is_directory);

	g_debug ("%s:'%s' (%s) (update monitor event or user request)",
		 ignored ? "Ignored" : "Found ",
		 path,
		 is_directory ? "DIR" : "FILE");

	if (!ignored) {
		queue = g_hash_table_lookup (processor->private->items_updated_queues, module_name);
		g_queue_push_tail (queue, g_object_ref (file));
		
		item_queue_handlers_set_up (processor);
	}

	g_free (path);
}

static void
processor_files_delete (TrackerProcessor *processor,
			const gchar	 *module_name,
			GFile		 *file,
			gboolean	  is_directory)
{
	TrackerCrawler *crawler;
	GQueue	       *queue;
	gchar	       *path;
	gboolean	ignored;

	path = g_file_get_path (file);
	crawler = g_hash_table_lookup (processor->private->crawlers, module_name);
	ignored = tracker_crawler_is_path_ignored (crawler, path, is_directory);

	g_debug ("%s:'%s' (%s) (delete monitor event or user request)",
		 ignored ? "Ignored" : "Found ",
		 path,
		 is_directory ? "DIR" : "FILE");

	if (!ignored) {
		queue = g_hash_table_lookup (processor->private->items_deleted_queues, module_name);
		g_queue_push_tail (queue, g_object_ref (file));
		
		item_queue_handlers_set_up (processor);
	}

	g_free (path);
}

static void
processor_files_move (TrackerProcessor *processor,
		      const gchar      *module_name,
		      GFile	       *file,
		      GFile	       *other_file,
		      gboolean		is_directory)
{
	TrackerCrawler *crawler;
	GQueue	       *queue;
	gchar	       *path;
	gchar	       *other_path;
	gboolean	path_ignored;
	gboolean	other_path_ignored;

	path = g_file_get_path (file);
	other_path = g_file_get_path (other_file);
	crawler = g_hash_table_lookup (processor->private->crawlers, module_name);

	path_ignored = tracker_crawler_is_path_ignored (crawler, path, is_directory);
	other_path_ignored = tracker_crawler_is_path_ignored (crawler, other_path, is_directory);

	g_debug ("%s:'%s'->'%s':%s (%s) (move monitor event or user request)",
		 path_ignored ? "Ignored" : "Found ",
		 path,
		 other_path,
		 other_path_ignored ? "Ignored" : " Found",
		 is_directory ? "DIR" : "FILE");

	if (path_ignored && other_path_ignored) {
		/* Do nothing */
	} else if (path_ignored) {
		/* Check new file */
		if (!is_directory) {
			queue = g_hash_table_lookup (processor->private->items_created_queues, module_name);
			g_queue_push_tail (queue, g_object_ref (other_file));

			item_queue_handlers_set_up (processor);
		} else {
			/* If this is a directory we need to crawl it */
			tracker_crawler_add_unexpected_path (crawler, other_path);
		}
	} else if (other_path_ignored) {
		/* Delete old file */
		queue = g_hash_table_lookup (processor->private->items_deleted_queues, module_name);
		g_queue_push_tail (queue, g_object_ref (file));

		item_queue_handlers_set_up (processor);
	} else {
		/* Move old file to new file */
		queue = g_hash_table_lookup (processor->private->items_moved_queues, module_name);
		g_queue_push_tail (queue, g_object_ref (file));
		g_queue_push_tail (queue, g_object_ref (other_file));

		item_queue_handlers_set_up (processor);
	}

	g_free (other_path);
	g_free (path);
}

static void
monitor_item_created_cb (TrackerMonitor *monitor,
			 const gchar	*module_name,
			 GFile		*file,
			 gboolean	 is_directory,
			 gpointer	 user_data)
{
	tracker_processor_files_check (user_data, module_name, file, is_directory);
}

static void
monitor_item_updated_cb (TrackerMonitor *monitor,
			 const gchar	*module_name,
			 GFile		*file,
			 gboolean	 is_directory,
			 gpointer	 user_data)
{
	processor_files_update (user_data, module_name, file, is_directory);
}

static void
monitor_item_deleted_cb (TrackerMonitor *monitor,
			 const gchar	*module_name,
			 GFile		*file,
			 gboolean	 is_directory,
			 gpointer	 user_data)
{
	processor_files_delete (user_data, module_name, file, is_directory);
}

static void
monitor_item_moved_cb (TrackerMonitor *monitor,
		       const gchar    *module_name,
		       GFile	      *file,
		       GFile	      *other_file,
		       gboolean        is_directory,
		       gboolean        is_source_monitored,
		       gpointer        user_data)
{
	if (!is_source_monitored) {
		if (is_directory) {
			TrackerProcessor *processor;
			TrackerCrawler   *crawler;
			gchar            *path;

			processor = user_data;

			/* If the source is not monitored, we need to crawl it. */
			path = g_file_get_path (other_file);
			crawler = g_hash_table_lookup (processor->private->crawlers, module_name);
			tracker_crawler_add_unexpected_path (crawler, path);
			g_free (path);
		}
	} else {
		processor_files_move (user_data, module_name, file, other_file, is_directory);
	}
}

static void
crawler_processing_file_cb (TrackerCrawler *crawler,
			    const gchar    *module_name,
			    GFile	   *file,
			    gpointer	    user_data)
{
	TrackerProcessor *processor;
	GQueue		 *queue;

	processor = user_data;

	/* Add files in queue to our queues to send to the indexer */
	queue = g_hash_table_lookup (processor->private->items_created_queues, module_name);
	g_queue_push_tail (queue, g_object_ref (file));

	item_queue_handlers_set_up (processor);
}

static void
crawler_processing_directory_cb (TrackerCrawler *crawler,
				 const gchar	*module_name,
				 GFile		*file,
				 gpointer	 user_data)
{
	TrackerProcessor *processor;
	GQueue		 *queue;
	gboolean	  add_monitor;

	processor = user_data;

	/* FIXME: Get ignored directories from .cfg? We know that
	 * normally these would have monitors because these
	 * directories are those crawled based on the module config.
	 */
	add_monitor = TRUE;

	/* Should we add? */
	if (add_monitor) {
		tracker_monitor_add (processor->private->monitor, module_name, file);
	}

	/* Add files in queue to our queues to send to the indexer */
	queue = g_hash_table_lookup (processor->private->items_created_queues, module_name);
	g_queue_push_tail (queue, g_object_ref (file));

	item_queue_handlers_set_up (processor);
}

static void
crawler_finished_cb (TrackerCrawler *crawler,
		     const gchar    *module_name,
		     guint	     directories_found,
		     guint	     directories_ignored,
		     guint	     files_found,
		     guint	     files_ignored,
		     gpointer	     user_data)
{
	TrackerProcessor *processor;

	processor = user_data;

	/* Update stats */
	processor->private->directories_found += directories_found;
	processor->private->directories_ignored += directories_ignored;
	processor->private->files_found += files_found;
	processor->private->files_ignored += files_ignored;

	processor->private->total_directories_found += directories_found;
	processor->private->total_directories_ignored += directories_ignored;
	processor->private->total_files_found += files_found;
	processor->private->total_files_ignored += files_ignored;

	/* Proceed to next thing to process */
	process_continue (processor);
}

#ifdef HAVE_HAL

static gchar *
normalize_mount_point (const gchar *mount_point)
{
	if (g_str_has_suffix (mount_point, G_DIR_SEPARATOR_S)) {
		return g_strdup (mount_point);
	} else {
		return g_strconcat (mount_point, G_DIR_SEPARATOR_S, NULL);
	}
}

void
tracker_processor_mount_point_added (TrackerProcessor *processor,
				     const gchar      *udi,
				     const gchar      *mount_point)
{
	TrackerProcessorPrivate *priv;
	TrackerStatus	         status;
	gchar                   *mp;

	g_return_if_fail (TRACKER_IS_PROCESSOR (processor));

	priv = processor->private;

	status = tracker_status_get ();
	mp = normalize_mount_point (mount_point);

	/* Add removable device to list of known devices to iterate */
	if (!g_list_find_custom (priv->devices, mp, (GCompareFunc) g_strcmp0)) {
		priv->devices = g_list_append (priv->devices, mp);
	}

	/* Reset finished devices flag */
	processor->private->finished_devices = FALSE;

	/* If we are idle/not doing anything, start up the processor
	 * again so we handle the new location.
	 */
	if (status == TRACKER_STATUS_INDEXING ||
	    status == TRACKER_STATUS_OPTIMIZING ||
	    status == TRACKER_STATUS_IDLE) {
		/* If we are indexing then we must have already
		 * crawled all locations so we need to start up the
		 * processor again for the removable media once more.
		 */
		process_next (processor);
	}
}

void
tracker_processor_mount_point_removed (TrackerProcessor *processor,
				       const gchar      *udi,
				       const gchar      *mount_point)
{
	TrackerProcessorPrivate *priv;
	GFile		        *file;
	GList                   *l;
	gchar                   *mp;

	g_return_if_fail (TRACKER_IS_PROCESSOR (processor));

	priv = processor->private;
	mp = normalize_mount_point (mount_point);

	/* Remove directory from list of iterated_removable_media, so
	 * we don't traverse it.
	 */
	l = g_list_find_custom (priv->devices, mp, (GCompareFunc) g_strcmp0);


	/* Make sure we don't remove the current device we are
	 * processing, this is because we do this same clean up later
	 * in process_device_next() 
	 */
	if (l && l != priv->current_device) {
		g_free (l->data);
		priv->devices = g_list_delete_link (priv->devices, l);
	}

	/* Remove the monitor, the volumes are updated somewhere else
	 * in main. 
	 */
	file = g_file_new_for_path (mount_point);
	tracker_monitor_remove_recursively (priv->monitor, file);
	g_object_unref (file);

	g_free (mp);
}

#endif /* HAVE_HAL */

TrackerProcessor *
tracker_processor_new (TrackerConfig *config,
		       TrackerHal    *hal)
{
	TrackerProcessor	*processor;
	TrackerProcessorPrivate *priv;
	TrackerCrawler		*crawler;
	DBusGProxy		*proxy;
	GList			*l;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

#ifdef HAVE_HAL
	g_return_val_if_fail (TRACKER_IS_HAL (hal), NULL);
#endif /* HAVE_HAL */

	tracker_status_set_and_signal (TRACKER_STATUS_INITIALIZING);

	processor = g_object_new (TRACKER_TYPE_PROCESSOR, NULL);
	priv = processor->private;

	/* Set up config */
	priv->config = g_object_ref (config);

#ifdef HAVE_HAL
	/* Set up hal */
	priv->hal = g_object_ref (hal);

	priv->devices = tracker_hal_get_removable_device_roots (priv->hal);
#endif /* HAVE_HAL */

	/* Set up the crawlers now we have config and hal */
	for (l = priv->modules; l; l = l->next) {
		crawler = tracker_crawler_new (priv->config, l->data);

		g_signal_connect (crawler, "processing-file",
				  G_CALLBACK (crawler_processing_file_cb),
				  processor);
		g_signal_connect (crawler, "processing-directory",
				  G_CALLBACK (crawler_processing_directory_cb),
				  processor);
		g_signal_connect (crawler, "finished",
				  G_CALLBACK (crawler_finished_cb),
				  processor);

		g_hash_table_insert (priv->crawlers,
				     g_strdup (l->data),
				     crawler);
	}

	/* Set up the monitor */
	priv->monitor = tracker_monitor_new (config);

	g_message ("Disabling monitor events until we have crawled the file system");
	tracker_monitor_set_enabled (priv->monitor, FALSE);

	g_signal_connect (priv->monitor, "item-created",
			  G_CALLBACK (monitor_item_created_cb),
			  processor);
	g_signal_connect (priv->monitor, "item-updated",
			  G_CALLBACK (monitor_item_updated_cb),
			  processor);
	g_signal_connect (priv->monitor, "item-deleted",
			  G_CALLBACK (monitor_item_deleted_cb),
			  processor);
	g_signal_connect (priv->monitor, "item-moved",
			  G_CALLBACK (monitor_item_moved_cb),
			  processor);

	/* Set up the indexer proxy and signalling to know when we are
	 * finished.
	 */
	proxy = tracker_dbus_indexer_get_proxy ();
	priv->indexer_proxy = g_object_ref (proxy);

	dbus_g_proxy_connect_signal (proxy, "Status",
				     G_CALLBACK (indexer_status_cb),
				     processor,
				     NULL);
	dbus_g_proxy_connect_signal (proxy, "Started",
				     G_CALLBACK (indexer_started_cb),
				     processor,
				     NULL);
	dbus_g_proxy_connect_signal (proxy, "Finished",
				     G_CALLBACK (indexer_finished_cb),
				     processor,
				     NULL);

	return processor;
}

void
tracker_processor_start (TrackerProcessor *processor)
{
	g_return_if_fail (TRACKER_IS_PROCESSOR (processor));

	processor->private->been_started = TRUE;

	processor->private->interrupted = FALSE;

	processor->private->finished_modules = FALSE;
	processor->private->finished_devices = FALSE;
	processor->private->finished_sending = FALSE;
	processor->private->finished_indexer = FALSE;

	process_next (processor);
}

void 
tracker_processor_stop (TrackerProcessor *processor)
{
	g_return_if_fail (TRACKER_IS_PROCESSOR (processor));

	if (!processor->private->been_started) {
		return;
	}

	processor->private->interrupted = TRUE;

	process_modules_stop (processor);
	process_devices_stop (processor);
	
	/* Queues? */

	process_finish (processor);
}

void
tracker_processor_files_check (TrackerProcessor *processor,
			       const gchar	*module_name,
			       GFile		*file,
			       gboolean		 is_directory)
{
	g_return_if_fail (TRACKER_IS_PROCESSOR (processor));
	g_return_if_fail (module_name != NULL);
	g_return_if_fail (G_IS_FILE (file));
	
	processor_files_check (processor, module_name, file, is_directory);
}

void
tracker_processor_files_update (TrackerProcessor *processor,
				const gchar	 *module_name,
				GFile		 *file,
				gboolean	  is_directory)
{
	g_return_if_fail (TRACKER_IS_PROCESSOR (processor));
	g_return_if_fail (module_name != NULL);
	g_return_if_fail (G_IS_FILE (file));

	processor_files_update (processor, module_name, file, is_directory);
}

void
tracker_processor_files_delete (TrackerProcessor *processor,
				const gchar	 *module_name,
				GFile		 *file,
				gboolean	  is_directory)
{
	g_return_if_fail (TRACKER_IS_PROCESSOR (processor));
	g_return_if_fail (module_name != NULL);
	g_return_if_fail (G_IS_FILE (file));

	processor_files_delete (processor, module_name, file, is_directory);
}

void
tracker_processor_files_move (TrackerProcessor *processor,
			      const gchar      *module_name,
			      GFile	       *file,
			      GFile	       *other_file,
			      gboolean		is_directory)
{
	g_return_if_fail (TRACKER_IS_PROCESSOR (processor));
	g_return_if_fail (module_name != NULL);
	g_return_if_fail (G_IS_FILE (file));
	g_return_if_fail (G_IS_FILE (other_file));

	processor_files_move (processor, module_name, file, other_file, is_directory);
}

guint
tracker_processor_get_directories_found (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->total_directories_found;
}

guint
tracker_processor_get_directories_ignored (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->total_directories_ignored;
}

guint
tracker_processor_get_directories_total (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->total_directories_found + processor->private->total_directories_ignored;
}

guint
tracker_processor_get_files_found (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->total_files_found;
}

guint
tracker_processor_get_files_ignored (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->total_files_ignored;
}

guint
tracker_processor_get_files_total (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->total_files_found + processor->private->total_files_ignored;
}

gdouble
tracker_processor_get_seconds_elapsed (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->indexer_seconds_elapsed;
}
