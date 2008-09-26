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

#include "tracker-processor.h"
#include "tracker-crawler.h"
#include "tracker-daemon.h"
#include "tracker-dbus.h"
#include "tracker-indexer-client.h"
#include "tracker-main.h"
#include "tracker-monitor.h"
#include "tracker-status.h"

#define TRACKER_PROCESSOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_PROCESSOR, TrackerProcessorPrivate))

#define ITEMS_QUEUE_PROCESS_INTERVAL 2000
#define ITEMS_QUEUE_PROCESS_MAX      1000

#define ITEMS_SIGNAL_TO_DAEMON_RATIO 500

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
	gboolean	iterated_modules;
	gboolean	iterated_removable_media;

	GTimer	       *timer;

	gboolean	interrupted;
	gboolean	finished;

	/* Statistics */
	guint		directories_found;
	guint		directories_ignored;
	guint		files_found;
	guint		files_ignored;

	guint		items_done;
	guint		items_remaining;

	gdouble		seconds_elapsed;
};

enum {
	FINISHED,
	LAST_SIGNAL
};

static void tracker_processor_finalize	    (GObject	      *object);
static void crawler_destroy_notify	    (gpointer	       data);
static void item_queue_destroy_notify	    (gpointer	       data);
static void process_module_next		    (TrackerProcessor *processor);
static void indexer_status_cb		    (DBusGProxy       *proxy,
					     gdouble	       seconds_elapsed,
					     const gchar      *current_module_name,
					     guint	       items_done,
					     guint	       items_remaining,
					     gpointer	       user_data);
static void indexer_finished_cb		    (DBusGProxy       *proxy,
					     gdouble	       seconds_elapsed,
					     guint	       items_done,
					     gboolean	       interrupted,
					     gpointer	       user_data);
static void monitor_item_created_cb	    (TrackerMonitor   *monitor,
					     const gchar      *module_name,
					     GFile	      *file,
					     gboolean	       is_directory,
					     gpointer	       user_data);
static void monitor_item_updated_cb	    (TrackerMonitor   *monitor,
					     const gchar      *module_name,
					     GFile	      *file,
					     gboolean	       is_directory,
					     gpointer	       user_data);
static void monitor_item_deleted_cb	    (TrackerMonitor   *monitor,
					     const gchar      *module_name,
					     GFile	      *file,
					     gboolean	       is_directory,
					     gpointer	       user_data);
static void monitor_item_moved_cb	    (TrackerMonitor   *monitor,
					     const gchar      *module_name,
					     GFile	      *file,
					     GFile	      *other_file,
					     gboolean	       is_directory,
					     gpointer	       user_data);
static void crawler_processing_file_cb	    (TrackerCrawler   *crawler,
					     const gchar      *module_name,
					     GFile	      *file,
					     gpointer	       user_data);
static void crawler_processing_directory_cb (TrackerCrawler   *crawler,
					     const gchar      *module_name,
					     GFile	      *file,
					     gpointer	       user_data);
static void crawler_finished_cb		    (TrackerCrawler   *crawler,
					     const gchar      *module_name,
					     guint	       directories_found,
					     guint	       directories_ignored,
					     guint	       files_found,
					     guint	       files_ignored,
					     gpointer	       user_data);

#ifdef HAVE_HAL
static void mount_point_added_cb	    (TrackerHal       *hal,
					     const gchar      *mount_point,
					     gpointer	       user_data);
static void mount_point_removed_cb	    (TrackerHal       *hal,
					     const gchar      *mount_point,
					     gpointer	       user_data);
#endif /* HAVE_HAL */

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
	if (priv->hal) {
		g_signal_handlers_disconnect_by_func (priv->hal,
						      mount_point_added_cb,
						      object);
		g_signal_handlers_disconnect_by_func (priv->hal,
						      mount_point_removed_cb,
						      object);

		g_object_unref (priv->hal);
	}
#endif /* HAVE_HAL */

	g_object_unref (priv->config);

	G_OBJECT_CLASS (tracker_processor_parent_class)->finalize (object);
}


static void
get_remote_roots (TrackerProcessor  *processor,
		  GSList	   **mounted_directory_roots,
		  GSList	   **removable_device_roots)
{
	GSList *l1;
	GSList *l2;

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
		GSList *l;
		GSList *list = NULL;

		for (l = l1; l; l = l->next) {
			if (g_slist_find_custom (l2, l->data, (GCompareFunc) strcmp)) {
				continue;
			}

			list = g_slist_prepend (list, l->data);
		}

		*mounted_directory_roots = g_slist_reverse (list);
	} else {
		*mounted_directory_roots = NULL;
	}

	*removable_device_roots = g_slist_copy (l2);
}

static gboolean
path_should_be_ignored_for_media (TrackerProcessor *processor,
				  const gchar	   *path)
{
	GSList	 *roots = NULL;
	GSList	 *mounted_directory_roots = NULL;
	GSList	 *removable_device_roots = NULL;
	GSList	 *l;
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
		roots = g_slist_concat (roots, mounted_directory_roots);
	}

	if (ignore_removable_devices) {
		roots = g_slist_concat (roots, removable_device_roots);
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

	g_slist_free (roots);

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
		guint signals;

		signals = g_signal_handlers_disconnect_matched (crawler,
								G_SIGNAL_MATCH_FUNC,
								0,
								0,
								NULL,
								G_CALLBACK (crawler_processing_file_cb),
								NULL);
		signals = g_signal_handlers_disconnect_matched (crawler,
								G_SIGNAL_MATCH_FUNC,
								0,
								0,
								NULL,
								G_CALLBACK (crawler_processing_directory_cb),
								NULL);
		signals = g_signal_handlers_disconnect_matched (crawler,
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
	} else {
		g_debug ("Sent!");
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
	GQueue		 *queue;
	GStrv		  files;
	gchar		 *module_name;

	processor = user_data;

	/* This way we don't send anything to the indexer from monitor
	 * events but we still queue them ready to send when we are
	 * unpaused.
	 */
	if (tracker_status_get_is_paused_manually () ||
	    tracker_status_get_is_paused_for_io ()) {
		g_message ("We are paused, sending nothing to the index until we are unpaused");
		return TRUE;
	}

	/* Now we try to send items to the indexer */
	tracker_status_set_and_signal (TRACKER_STATUS_INDEXING);

	/* This is here so we don't try to send something if we are
	 * still waiting for a response from the last send.
	 */
	if (processor->private->sent_type != SENT_TYPE_NONE) {
		g_message ("Still waiting for response from indexer, "
			   "not sending more files yet");
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
		files = tracker_dbus_queue_gfile_to_strv (queue, ITEMS_QUEUE_PROCESS_MAX);

		g_message ("Queue for module:'%s' created items processed, sending first %d to the indexer",
			   module_name,
			   g_strv_length (files));

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
		files = tracker_dbus_queue_gfile_to_strv (queue, ITEMS_QUEUE_PROCESS_MAX);

		g_message ("Queue for module:'%s' updated items processed, sending first %d to the indexer",
			   module_name,
			   g_strv_length (files));

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
		const gchar *source;
		const gchar *target;

		files = tracker_dbus_queue_gfile_to_strv (queue, 2);

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

		processor->private->sent_type = SENT_TYPE_MOVED;
		processor->private->sent_module_name = module_name;
		processor->private->sent_items = files;

		org_freedesktop_Tracker_Indexer_file_move_async (processor->private->indexer_proxy,
								 module_name,
								 source,
								 target,
								 item_queue_processed_cb,
								 processor);

		return TRUE;
	}

	g_message ("No items in any queues to process, doing nothing");
	processor->private->item_queues_handler_id = 0;

	return FALSE;
}

static void
item_queue_handlers_set_up (TrackerProcessor *processor)
{
	if (processor->private->item_queues_handler_id != 0) {
		return;
	}

	processor->private->item_queues_handler_id =
		g_timeout_add (ITEMS_QUEUE_PROCESS_INTERVAL,
			       item_queue_handlers_cb,
			       processor);
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
process_module_files_add_removable_media (TrackerProcessor *processor)
{
	TrackerCrawler *crawler;
	GSList	       *roots;
	GSList	       *l;
	const gchar    *module_name = "files";

	crawler = g_hash_table_lookup (processor->private->crawlers, module_name);

#ifdef HAVE_HAL
	roots = tracker_hal_get_removable_device_roots (processor->private->hal);
#else  /* HAVE_HAL */
	roots = NULL;
#endif /* HAVE_HAL */

	g_message ("  Removable media monitors being added:");

	for (l = roots; l; l = l->next) {
		GFile *file;

		/* This is dreadfully inefficient */
		if (path_should_be_ignored_for_media (processor, l->data)) {
			g_message ("	%s (ignored due to config)", (gchar*) l->data);
			continue;
		}

		g_message ("	%s", (gchar*) l->data);

		file = g_file_new_for_path (l->data);
		tracker_monitor_add (processor->private->monitor, module_name, file);
		g_object_unref (file);
	}

	if (g_slist_length (roots) == 0) {
		g_message ("	NONE");
	}

	g_message ("  Removable media crawls being added:");

	for (l = roots; l; l = l->next) {
		/* This is dreadfully inefficient */
		if (path_should_be_ignored_for_media (processor, l->data)) {
			g_message ("	%s (ignored due to config)", (gchar*) l->data);
			continue;
		}

		g_message ("	%s", (gchar*) l->data);
		tracker_crawler_add_path (crawler, l->data);
	}

	if (g_slist_length (roots) == 0) {
		g_message ("	NONE");
	}

	tracker_crawler_set_use_module_paths (crawler, FALSE);
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
		g_message ("	%s", (gchar*) l->data);
	}

	if (g_slist_length (no_watch_roots) == 0) {
		g_message ("	NONE");
	}

	/* Add monitors, from WatchDirectoryRoots config key */
	g_message ("  User monitors being added:");
	for (l = watch_roots; l; l = l->next) {
		GFile *file;

		if (is_path_on_ignore_list (no_watch_roots, l->data)) {
			continue;
		}

		g_message ("	%s", (gchar*) l->data);

		file = g_file_new_for_path (l->data);
		tracker_monitor_add (processor->private->monitor, module_name, file);
		g_object_unref (file);

		watch_root_count++;
	}

	if (g_slist_length (watch_roots) == 0) {
		g_message ("	NONE");
	}

	/* Add crawls, from WatchDirectoryRoots and
	 * CrawlDirectoryRoots config keys.
	 */
	g_message ("  User crawls being added:");

	for (l = watch_roots; l; l = l->next) {
		if (is_path_on_ignore_list (no_watch_roots, l->data)) {
			continue;
		}

		g_message ("	%s", (gchar*) l->data);
		tracker_crawler_add_path (crawler, l->data);
	}

	for (l = crawl_roots; l; l = l->next) {
		if (is_path_on_ignore_list (no_watch_roots, l->data)) {
			continue;
		}

		g_message ("	%s", (gchar*) l->data);
		tracker_crawler_add_path (crawler, l->data);

		crawl_root_count++;
	}

	if (g_slist_length (watch_roots) == 0 &&
	    g_slist_length (crawl_roots) == 0) {
		g_message ("	NONE");
	}
}

static void
process_module (TrackerProcessor *processor,
		const gchar	 *module_name,
		gboolean	  is_removable_media)
{
	TrackerCrawler *crawler;
	GSList	       *disabled_modules;

	g_message ("Processing module:'%s' %s",
		   module_name,
		   is_removable_media ? "(for removable media)" : "");

	/* Check it is enabled */
	if (!tracker_module_config_get_enabled (module_name)) {
		g_message ("  Module disabled");
		process_module_next (processor);
		return;
	}

	/* Check it is is not disabled by the user locally */
	disabled_modules = tracker_config_get_disabled_modules (processor->private->config);
	if (g_slist_find_custom (disabled_modules, module_name, (GCompareFunc) strcmp)) {
		g_message ("  Module disabled by user");
		process_module_next (processor);
		return;
	}

	/* Here we set up legacy .cfg options like watch roots */
	tracker_status_set_and_signal (TRACKER_STATUS_WATCHING);

	if (strcmp (module_name, "files") == 0) {
		if (is_removable_media) {
			process_module_files_add_removable_media (processor);
		} else {
			process_module_files_add_legacy_options (processor);
		}
	}

	/* Gets all files and directories */
	tracker_status_set_and_signal (TRACKER_STATUS_PENDING);

	crawler = g_hash_table_lookup (processor->private->crawlers, module_name);

	if (!tracker_crawler_start (crawler)) {
		/* If there is nothing to crawl, we are done, process
		 * the next module.
		 */
		process_module_next (processor);
	}
}

static void
process_module_next (TrackerProcessor *processor)
{
	const gchar *module_name;

	/* Don't recursively iterate the modules if this function is
	 * called, check first.
	 */
	if (!processor->private->current_module) {
		if (!processor->private->iterated_modules) {
			processor->private->current_module = processor->private->modules;
		}
	} else {
		processor->private->current_module = processor->private->current_module->next;
	}

	/* If we have no further modules to iterate */
	if (!processor->private->current_module) {
		processor->private->iterated_modules = TRUE;

		/* If we have no more modules but some removable media
		 * was added during the initial crawl, we then make
		 * sure we crawl the new removable media too.
		 */
		if (processor->private->iterated_removable_media) {
			processor->private->interrupted = FALSE;
			tracker_processor_stop (processor);
			return;
		}

		/* We use this for removable media */
		module_name = "files";
		processor->private->iterated_removable_media = TRUE;
	} else {
		module_name = processor->private->current_module->data;
	}

	/* Set up new crawler for new module */
	process_module (processor, module_name, processor->private->iterated_removable_media);
}

static void
indexer_status_cb (DBusGProxy  *proxy,
		   gdouble	seconds_elapsed,
		   const gchar *current_module_name,
		   guint	items_done,
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
	processor->private->items_done = items_done;
	processor->private->items_remaining = items_remaining;
	processor->private->seconds_elapsed = seconds_elapsed;

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
			       items_done,
			       items_remaining,
			       items_done + items_remaining,
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
						   items_done,
						   items_remaining);
	str2 = tracker_seconds_to_string (seconds_elapsed, TRUE);

	g_message ("Indexed %d/%d, module:'%s', %s left, %s elapsed",
		   items_done,
		   items_done + items_remaining,
		   current_module_name,
		   str1,
		   str2);

	g_free (str2);
	g_free (str1);
}

static void
indexer_finished_cb (DBusGProxy  *proxy,
		     gdouble	  seconds_elapsed,
		     guint	  items_done,
		     gboolean	  interrupted,
		     gpointer	  user_data)
{
	TrackerProcessor *processor;
	TrackerDBIndex	 *index;
	GObject		 *object;
	gchar		 *str;

	processor = user_data;
	object = tracker_dbus_get_object (TRACKER_TYPE_DAEMON);

	processor->private->items_done = items_done;
	processor->private->items_remaining = 0;
	processor->private->seconds_elapsed = seconds_elapsed;

	/* Signal to any applications */
	g_signal_emit_by_name (object,
			       "index-progress",
			       "", /* Service */
			       "", /* Path */
			       items_done,
			       0,
			       items_done,
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

	g_message ("Indexer finished in %s, %d items indexed in total",
		   str,
		   items_done);
	g_free (str);

	/* Do we even need this step Optimizing ? */
	tracker_status_set_and_signal (TRACKER_STATUS_OPTIMIZING);

	/* Now the indexer is done, we can signal our status as IDLE */
	tracker_status_set_and_signal (TRACKER_STATUS_IDLE);

	/* Signal to the applet we are finished */
	g_signal_emit_by_name (object,
			       "index-finished",
			       seconds_elapsed);

	/* Signal the processor is now finished */
	processor->private->finished = TRUE;
	g_signal_emit (processor, signals[FINISHED], 0);
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

	g_debug ("%s:'%s' (create monitor event or user request)",
		 ignored ? "Ignored" : "Found ",
		 path);

	g_free (path);

	if (ignored) {
		return;
	}

	queue = g_hash_table_lookup (processor->private->items_created_queues, module_name);
	g_queue_push_tail (queue, g_object_ref (file));

	item_queue_handlers_set_up (processor);
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

	g_debug ("%s:'%s' (update monitor event or user request)",
		 ignored ? "Ignored" : "Found ",
		 path);

	g_free (path);

	if (ignored) {
		return;
	}

	queue = g_hash_table_lookup (processor->private->items_updated_queues, module_name);
	g_queue_push_tail (queue, g_object_ref (file));

	item_queue_handlers_set_up (processor);
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

	g_debug ("%s:'%s' (delete monitor event or user request)",
		 ignored ? "Ignored" : "Found ",
		 path);

	g_free (path);

	if (ignored) {
		return;
	}

	queue = g_hash_table_lookup (processor->private->items_deleted_queues, module_name);
	g_queue_push_tail (queue, g_object_ref (file));

	item_queue_handlers_set_up (processor);
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

	g_debug ("%s:'%s'->'%s':%s (move monitor event or user request)",
		 path_ignored ? "Ignored" : "Found ",
		 path,
		 other_path,
		 other_path_ignored ? "Ignored" : " Found");

	g_free (other_path);
	g_free (path);

	if (path_ignored && other_path_ignored) {
		/* Do nothing */
		return;
	} else if (path_ignored) {
		/* Check new file */
		queue = g_hash_table_lookup (processor->private->items_created_queues, module_name);
		g_queue_push_tail (queue, g_object_ref (other_file));
	} else if (other_path_ignored) {
		/* Delete old file */
		queue = g_hash_table_lookup (processor->private->items_deleted_queues, module_name);
		g_queue_push_tail (queue, g_object_ref (file));
	} else {
		/* Move old file to new file */
		queue = g_hash_table_lookup (processor->private->items_moved_queues, module_name);
		g_queue_push_tail (queue, g_object_ref (file));
		g_queue_push_tail (queue, g_object_ref (other_file));
	}

	item_queue_handlers_set_up (processor);
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
		       gpointer        user_data)
{
	processor_files_move (user_data, module_name, file, other_file, is_directory);
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

	processor->private->directories_found += directories_found;
	processor->private->directories_ignored += directories_ignored;
	processor->private->files_found += files_found;
	processor->private->files_ignored += files_ignored;

	/* Proceed to next module */
	process_module_next (processor);
}

#ifdef HAVE_HAL

static void
mount_point_added_cb (TrackerHal  *hal,
		      const gchar *mount_point,
		      gpointer	   user_data)
{
	TrackerProcessor *processor;
	TrackerStatus	  status;

	processor = user_data;

	g_message ("** TRAWLING THROUGH NEW MOUNT POINT:'%s'", mount_point);

	status = tracker_status_get ();

	processor->private->iterated_removable_media = FALSE;

	if (status == TRACKER_STATUS_INDEXING ||
	    status == TRACKER_STATUS_OPTIMIZING ||
	    status == TRACKER_STATUS_IDLE) {
		/* If we are indexing then we must have already
		 * crawled all locations so we need to start up the
		 * processor again for the removable media once more.
		 */
		process_module_next (processor);
	}
}

static void
mount_point_removed_cb (TrackerHal  *hal,
			const gchar *mount_point,
			gpointer     user_data)
{
	TrackerProcessor *processor;
	GFile		 *file;

	processor = user_data;

	g_message ("** CLEANING UP OLD MOUNT POINT:'%s'", mount_point);

	/* Remove the monitor? */
	file = g_file_new_for_path (mount_point);
	tracker_monitor_remove (processor->private->monitor, "files", file);
	g_object_unref (file);
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

	g_signal_connect (priv->hal, "mount-point-added",
			  G_CALLBACK (mount_point_added_cb),
			  processor);
	g_signal_connect (priv->hal, "mount-point-removed",
			  G_CALLBACK (mount_point_removed_cb),
			  processor);
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

	g_message ("Starting to process %d modules...",
		   g_list_length (processor->private->modules));

	if (processor->private->timer) {
		g_timer_destroy (processor->private->timer);
	}

	processor->private->timer = g_timer_new ();

	processor->private->interrupted = TRUE;

	process_module_next (processor);
}

void
tracker_processor_stop (TrackerProcessor *processor)
{
	gdouble elapsed;

	g_return_if_fail (TRACKER_IS_PROCESSOR (processor));

	if (processor->private->interrupted) {
		TrackerCrawler *crawler;

		crawler = g_hash_table_lookup (processor->private->crawlers,
					       processor->private->current_module->data);
		tracker_crawler_stop (crawler);

	}

	/* Now we have finished crawling, we enable monitor events */
	g_message ("Enabling monitor events");
	tracker_monitor_set_enabled (processor->private->monitor, TRUE);

	g_message ("Process %s\n",
		   processor->private->finished ? "has finished" : "been stopped");

	if (processor->private->timer) {
		g_timer_stop (processor->private->timer);
		elapsed = g_timer_elapsed (processor->private->timer, NULL);
	} else {
		elapsed = 0;
	}

	g_message ("Total time taken : %4.4f seconds",
		   elapsed);
	g_message ("Total directories: %d (%d ignored)",
		   processor->private->directories_found,
		   processor->private->directories_ignored);
	g_message ("Total files      : %d (%d ignored)",
		   processor->private->files_found,
		   processor->private->files_ignored);
	g_message ("Total monitors   : %d\n",
		   tracker_monitor_get_count (processor->private->monitor, NULL));

	/* Here we set to IDLE when we were stopped, otherwise, we
	 * we are currently in the process of sending files to the
	 * indexer and we set the state to INDEXING
	 */
	if (processor->private->interrupted) {
		/* Do we even need this step optimizing ? */
		tracker_status_set_and_signal (TRACKER_STATUS_OPTIMIZING);

		/* All done */
		tracker_status_set_and_signal (TRACKER_STATUS_IDLE);

		processor->private->finished = TRUE;
		g_signal_emit (processor, signals[FINISHED], 0);
	} else {
		item_queue_handlers_set_up (processor);
	}
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

	return processor->private->directories_found;
}

guint
tracker_processor_get_directories_ignored (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->directories_ignored;
}

guint
tracker_processor_get_directories_total (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->directories_found + processor->private->directories_ignored;
}

guint
tracker_processor_get_files_found (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->files_found;
}

guint
tracker_processor_get_files_ignored (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->files_ignored;
}

guint
tracker_processor_get_files_total (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->files_found + processor->private->files_ignored;
}

gdouble
tracker_processor_get_seconds_elapsed (TrackerProcessor *processor)
{
	g_return_val_if_fail (TRACKER_IS_PROCESSOR (processor), 0);

	return processor->private->seconds_elapsed;
}
