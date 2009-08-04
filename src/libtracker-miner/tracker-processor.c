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
#include <libtracker-common/tracker-storage.h>
#include <libtracker-common/tracker-module-config.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-db/tracker-db-manager.h>

#include "tracker-crawler.h"
#include "tracker-dbus.h"
#include "tracker-monitor.h"
#include "tracker-marshal.h"
#include "tracker-processor.h"

#define TRACKER_PROCESSOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_PROCESSOR, TrackerProcessorPrivate))

struct TrackerProcessorPrivate {
	TrackerStorage *hal;
	TrackerMonitor *monitor;
	TrackerCrawler *crawler;

	/* File queues for indexer */
	guint		item_queues_handler_id;

	GQueue         *items_created;
	GQueue         *items_updated;
	GQueue         *items_deleted;
	GQueue         *items_moved;

	GArray         *dirs;
	GList          *devices;
	GList          *current_device;

	GTimer	       *timer;

	/* Status */
	gboolean        been_started;
	gboolean	interrupted;

	gboolean	finished_files;
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
};

typedef struct {
	gchar    *path;
	gboolean  recurse;
} DirData;

enum {
	CHECK_FILE,
	CHECK_DIR,
	PROCESS_FILE,
	MONITOR_DIR,
	FINISHED,
	LAST_SIGNAL
};

static void     processor_finalize           (GObject          *object);
static gboolean processor_defaults           (TrackerProcessor *processor,
					      GFile            *file);
static void     processor_started            (TrackerMiner     *miner);
static void     monitor_item_created_cb      (TrackerMonitor   *monitor,
					      GFile            *file,
					      gboolean          is_directory,
					      gpointer          user_data);
static void     monitor_item_updated_cb      (TrackerMonitor   *monitor,
					      GFile            *file,
					      gboolean          is_directory,
					      gpointer          user_data);
static void     monitor_item_deleted_cb      (TrackerMonitor   *monitor,
					      GFile            *file,
					      gboolean          is_directory,
					      gpointer          user_data);
static void     monitor_item_moved_cb        (TrackerMonitor   *monitor,
					      GFile            *file,
					      GFile            *other_file,
					      gboolean          is_directory,
					      gboolean          is_source_monitored,
					      gpointer          user_data);
static gboolean crawler_process_file_cb      (TrackerCrawler   *crawler,
					      GFile            *file,
					      gpointer          user_data);
static gboolean crawler_process_directory_cb (TrackerCrawler   *crawler,
					      GFile            *file,
					      gpointer          user_data);
static void     crawler_finished_cb          (TrackerCrawler   *crawler,
					      guint             directories_found,
					      guint             directories_ignored,
					      guint             files_found,
					      guint             files_ignored,
					      gpointer          user_data);

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_ABSTRACT_TYPE (TrackerProcessor, tracker_processor, TRACKER_TYPE_MINER)

static void
tracker_processor_class_init (TrackerProcessorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
        TrackerProcessorClass *processor_class = TRACKER_PROCESSOR_CLASS (klass);
        TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);

	object_class->finalize = processor_finalize;

	if (0) {
	processor_class->check_file   = processor_defaults;
	processor_class->check_dir    = processor_defaults;
	processor_class->monitor_dir  = processor_defaults;
	}

        miner_class->started = processor_started;
	/*
        miner_class->stopped = miner_crawler_stopped;
        miner_class->paused  = miner_crawler_paused;
        miner_class->resumed = miner_crawler_resumed;
	*/
	

	signals[CHECK_FILE] =
		g_signal_new ("check-file",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerProcessorClass, check_file),
			      NULL, NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN, 1, G_TYPE_FILE);
	signals[CHECK_DIR] =
		g_signal_new ("check-dir",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerProcessorClass, check_dir),
			      NULL, NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN, 1, G_TYPE_FILE);
	signals[PROCESS_FILE] =
		g_signal_new ("process-file",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerProcessorClass, process_file),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_FILE);
	signals[MONITOR_DIR] =
		g_signal_new ("monitor-dir",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerProcessorClass, monitor_dir),
			      NULL, NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN, 1, G_TYPE_FILE);
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

	object->private = TRACKER_PROCESSOR_GET_PRIVATE (object);

	priv = object->private;

	/* For each module we create a TrackerCrawler and keep them in
	 * a hash table to look up.
	 */

	priv->items_created = g_queue_new ();
	priv->items_updated = g_queue_new ();
	priv->items_deleted = g_queue_new ();
	priv->items_moved = g_queue_new ();

	priv->dirs = g_array_new (FALSE, TRUE, sizeof (DirData));

	/* Set up the crawlers now we have config and hal */
	priv->crawler = tracker_crawler_new ();

	g_signal_connect (priv->crawler, "process-file",
			  G_CALLBACK (crawler_process_file_cb),
			  object);
	g_signal_connect (priv->crawler, "process-directory",
			  G_CALLBACK (crawler_process_directory_cb),
			  object);
	g_signal_connect (priv->crawler, "finished",
			  G_CALLBACK (crawler_finished_cb),
			  object);

	/* Set up the monitor */
	priv->monitor = tracker_monitor_new ();

	g_message ("Disabling monitor events until we have crawled the file system");
	tracker_monitor_set_enabled (priv->monitor, FALSE);

	g_signal_connect (priv->monitor, "item-created",
			  G_CALLBACK (monitor_item_created_cb),
			  object);
	g_signal_connect (priv->monitor, "item-updated",
			  G_CALLBACK (monitor_item_updated_cb),
			  object);
	g_signal_connect (priv->monitor, "item-deleted",
			  G_CALLBACK (monitor_item_deleted_cb),
			  object);
	g_signal_connect (priv->monitor, "item-moved",
			  G_CALLBACK (monitor_item_moved_cb),
			  object);
}

static void
processor_finalize (GObject *object)
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

	if (priv->crawler) {
		guint lsignals;

		lsignals = g_signal_handlers_disconnect_matched (priv->crawler,
								 G_SIGNAL_MATCH_FUNC,
								 0,
								 0,
								 NULL,
								 G_CALLBACK (crawler_process_file_cb),
								 NULL);
		lsignals = g_signal_handlers_disconnect_matched (priv->crawler,
								 G_SIGNAL_MATCH_FUNC,
								 0,
								 0,
								 NULL,
								 G_CALLBACK (crawler_process_directory_cb),
								 NULL);
		lsignals = g_signal_handlers_disconnect_matched (priv->crawler,
								 G_SIGNAL_MATCH_FUNC,
								 0,
								 0,
								 NULL,
								 G_CALLBACK (crawler_finished_cb),
								 NULL);

		g_object_unref (priv->crawler);
	}

	if (priv->monitor) {
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
	}

	if (priv->dirs) {
		gint i;

		for (i = 0; i < priv->dirs->len; i++) {
			DirData dd;

			dd = g_array_index (priv->dirs, DirData, i);
			g_free (dd.path);
		}

		g_array_free (priv->dirs, TRUE);
	}

	g_queue_foreach (priv->items_moved, (GFunc) g_object_unref, NULL);
	g_queue_free (priv->items_moved);

	g_queue_foreach (priv->items_deleted, (GFunc) g_object_unref, NULL);
	g_queue_free (priv->items_deleted);

	g_queue_foreach (priv->items_updated, (GFunc) g_object_unref, NULL);
	g_queue_free (priv->items_updated);

	g_queue_foreach (priv->items_created, (GFunc) g_object_unref, NULL);
	g_queue_free (priv->items_created);

#ifdef HAVE_HAL
	if (priv->devices) {
		g_list_foreach (priv->devices, (GFunc) g_free, NULL);
		g_list_free (priv->devices);
	}
#endif /* HAVE_HAL */

	G_OBJECT_CLASS (tracker_processor_parent_class)->finalize (object);
}

static gboolean 
processor_defaults (TrackerProcessor *processor,
		    GFile            *file)
{
	return TRUE;
}

static void
processor_started (TrackerMiner *miner)
{
	TrackerProcessor *processor;
	gint i;

	processor = TRACKER_PROCESSOR (miner);

	processor->private->been_started = TRUE;

	processor->private->interrupted = FALSE;

	processor->private->finished_files = FALSE;
	processor->private->finished_devices = FALSE;
	processor->private->finished_sending = FALSE;
	processor->private->finished_indexer = FALSE;

	/* Go through dirs and crawl */
	if (!processor->private->dirs) {
		g_message ("No directories set up for processor to handle, doing nothing");
		return;
	}

	for (i = 0; i < processor->private->dirs->len; i++) {
		DirData dd;
		
		dd = g_array_index (processor->private->dirs, DirData, i);

		tracker_crawler_start (processor->private->crawler, 
				       dd.path, 
				       dd.recurse);
	}


#if 0
	process_next (processor);
#endif
}

static gboolean
item_queue_handlers_cb (gpointer user_data)
{
	TrackerProcessor *processor;
	GFile *file;

	processor = user_data;

	/* Deleted items first */
	file = g_queue_pop_head (processor->private->items_deleted);
	if (file) {
		g_signal_emit (processor, signals[PROCESS_FILE], 0, file);
		g_object_unref (file);
	
		return TRUE;
	}

	/* Created items next */
	file = g_queue_pop_head (processor->private->items_created);
	if (file) {
		g_signal_emit (processor, signals[PROCESS_FILE], 0, file);
		g_object_unref (file);
	
		return TRUE;
	}

	/* Updated items next */
	file = g_queue_pop_head (processor->private->items_updated);
	if (file) {
		g_signal_emit (processor, signals[PROCESS_FILE], 0, file);
		g_object_unref (file);
	
		return TRUE;
	}

	/* Moved items next */
	file = g_queue_pop_head (processor->private->items_moved);
	if (file) {
		g_signal_emit (processor, signals[PROCESS_FILE], 0, file);
		g_object_unref (file);
	
		return TRUE;
	}

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
		g_idle_add (item_queue_handlers_cb,
			    processor);
}

static void
monitor_item_created_cb (TrackerMonitor *monitor,
			 GFile		*file,
			 gboolean	 is_directory,
			 gpointer	 user_data)
{
	TrackerProcessor *processor;
	gboolean should_process = TRUE;
	gchar *path;

	processor = user_data;

	g_signal_emit (processor, signals[CHECK_FILE], 0, file, &should_process);

	path = g_file_get_path (file);

	g_debug ("%s:'%s' (%s) (create monitor event or user request)",
		 should_process ? "Found " : "Ignored",
		 path,
		 is_directory ? "DIR" : "FILE");

	if (should_process) {
		if (is_directory) {
#ifdef FIX
			tracker_crawler_add_unexpected_path (processor->private->crawler, path);
#endif
		}

		g_queue_push_tail (processor->private->items_created, 
				   g_object_ref (file));
		
		item_queue_handlers_set_up (processor);
	}

	g_free (path);
}

static void
monitor_item_updated_cb (TrackerMonitor *monitor,
			 GFile		*file,
			 gboolean	 is_directory,
			 gpointer	 user_data)
{
	TrackerProcessor *processor;
	gchar *path;
	gboolean should_process = TRUE;

	g_signal_emit (processor, signals[CHECK_FILE], 0, file, &should_process);

	path = g_file_get_path (file);

 	g_debug ("%s:'%s' (%s) (update monitor event or user request)",
		 should_process ? "Found " : "Ignored",
		 path,
		 is_directory ? "DIR" : "FILE");

	if (should_process) {
		g_queue_push_tail (processor->private->items_updated, 
				   g_object_ref (file));
		
		item_queue_handlers_set_up (processor);
	}

	g_free (path);
}

static void
monitor_item_deleted_cb (TrackerMonitor *monitor,
			 GFile		*file,
			 gboolean	 is_directory,
			 gpointer	 user_data)
{
	TrackerProcessor *processor;
	gchar *path;
	gboolean should_process = TRUE;

	processor = user_data;

	g_signal_emit (processor, signals[CHECK_FILE], 0, file, &should_process);

	path = g_file_get_path (file);

	g_debug ("%s:'%s' (%s) (delete monitor event or user request)",
		 should_process ? "Found " : "Ignored",
		 path,
		 is_directory ? "DIR" : "FILE");

	if (should_process) {
		g_queue_push_tail (processor->private->items_deleted, 
				   g_object_ref (file));
		
		item_queue_handlers_set_up (processor);
	}

	g_free (path);
}

static void
monitor_item_moved_cb (TrackerMonitor *monitor,
		       GFile	      *file,
		       GFile	      *other_file,
		       gboolean        is_directory,
		       gboolean        is_source_monitored,
		       gpointer        user_data)
{
	TrackerProcessor *processor;

	processor = user_data;

	if (!is_source_monitored) {
		gchar *path;

		/* If the source is not monitored, we need to crawl it. */
		path = g_file_get_path (other_file);
#ifdef FIX
		tracker_crawler_add_unexpected_path (processor->private->crawler, path);
#endif
		g_free (path);
	} else {
		gchar *path;
		gchar *other_path;
		gboolean should_process;
		gboolean should_process_other;
		
		path = g_file_get_path (file);
		other_path = g_file_get_path (other_file);

		g_signal_emit (processor, signals[CHECK_FILE], 0, file, &should_process);
		g_signal_emit (processor, signals[CHECK_FILE], 0, other_file, &should_process_other);
		
		g_debug ("%s:'%s'->'%s':%s (%s) (move monitor event or user request)",
			 should_process ? "Found " : "Ignored",
			 path,
			 other_path,
			 should_process_other ? "Found " : "Ignored",
			 is_directory ? "DIR" : "FILE");
		
		if (!should_process && !should_process_other) {
			/* Do nothing */
		} else if (!should_process) {
			/* Check new file */
			if (!is_directory) {
				g_queue_push_tail (processor->private->items_created, 
						   g_object_ref (other_file));
				
				item_queue_handlers_set_up (processor);
			}
			
			/* If this is a directory we need to crawl it */
#ifdef FIX
			tracker_crawler_add_unexpected_path (processor->private->crawler, other_path);
#endif
		} else if (!should_process_other) {
			/* Delete old file */
			g_queue_push_tail (processor->private->items_deleted, g_object_ref (file));
			
			item_queue_handlers_set_up (processor);
		} else {
			/* Move old file to new file */
			g_queue_push_tail (processor->private->items_moved, g_object_ref (file));
			g_queue_push_tail (processor->private->items_moved, g_object_ref (other_file));
			
			item_queue_handlers_set_up (processor);
		}
		
		g_free (other_path);
		g_free (path);
	}
}

static gboolean
crawler_process_file_cb (TrackerCrawler *crawler,
			 GFile	        *file,
			 gpointer	 user_data)
{
	TrackerProcessor *processor;
	gboolean should_process = TRUE;

	processor = user_data;

	g_signal_emit (processor, signals[CHECK_FILE], 0, file, &should_process);

	if (should_process) {
		/* Add files in queue to our queues to send to the indexer */
		g_queue_push_tail (processor->private->items_created, 
				   g_object_ref (file));
		item_queue_handlers_set_up (processor);
	}

	return should_process;
}

static gboolean
crawler_process_directory_cb (TrackerCrawler *crawler,
			      GFile	     *file,
			      gpointer	      user_data)
{
	TrackerProcessor *processor;
	gboolean should_process = TRUE;
	gboolean add_monitor = TRUE;

	processor = user_data;

	g_signal_emit (processor, signals[CHECK_DIR], 0, file, &should_process);
	
	if (should_process) {
		/* FIXME: Do we add directories to the queue? */
		g_queue_push_tail (processor->private->items_created, 
				   g_object_ref (file));
		
		item_queue_handlers_set_up (processor);
	}

	g_signal_emit (processor, signals[MONITOR_DIR], 0, file, &add_monitor);

	/* Should we add? */
	if (add_monitor) {
		tracker_monitor_add (processor->private->monitor, file);
	}

	return should_process;
}

static void
crawler_finished_cb (TrackerCrawler *crawler,
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
	/* process_continue (processor); */
}

TrackerProcessor *
tracker_processor_new (TrackerStorage *storage)
{
	TrackerProcessor	*processor;
	TrackerProcessorPrivate *priv;

#ifdef HAVE_HAL
	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), NULL);
#endif /* HAVE_HAL */

	/* tracker_status_init (NULL, tracker_power_new ()); */
	tracker_module_config_init ();

	/* tracker_status_set_and_signal (TRACKER_STATUS_INITIALIZING); */

	processor = g_object_new (TRACKER_TYPE_PROCESSOR, NULL);
	priv = processor->private;

#ifdef HAVE_HAL
	/* Set up hal */
	priv->hal = g_object_ref (storage);

	priv->devices = tracker_storage_get_removable_device_roots (priv->hal);

#endif /* HAVE_HAL */


#if 0
	/* Set up the indexer and signalling to know when we are
	 * finished.
	 */

	g_signal_connect (priv->indexer, "started",
			  G_CALLBACK (indexer_started_cb),
			  processor);
	g_signal_connect (priv->indexer, "finished",
			  G_CALLBACK (indexer_finished_cb),
			  processor);
#endif

	return processor;
}

void 
tracker_processor_stop (TrackerProcessor *processor)
{
#if 0
	g_return_if_fail (TRACKER_IS_PROCESSOR (processor));

	if (!processor->private->been_started) {
		return;
	}

	processor->private->interrupted = TRUE;

	process_files_stop (processor);
	process_devices_stop (processor);
	
	/* Queues? */

	process_finish (processor);
#endif
}

void
tracker_processor_add_directory (TrackerProcessor *processor,
				 const gchar      *path,
				 gboolean          recurse)
{
	DirData dd;

	g_return_if_fail (TRACKER_IS_PROCESSOR (processor));
	g_return_if_fail (path != NULL);

	dd.path = g_strdup (path);
	dd.recurse = recurse;

	g_array_append_val (processor->private->dirs, dd);
}
