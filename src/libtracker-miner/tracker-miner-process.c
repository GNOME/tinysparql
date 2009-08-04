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

#include <libtracker-common/tracker-module-config.h>

#include "tracker-crawler.h"
#include "tracker-marshal.h"
#include "tracker-miner-process.h"
#include "tracker-monitor.h"

#define TRACKER_MINER_PROCESS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_PROCESS, TrackerMinerProcessPrivate))

struct TrackerMinerProcessPrivate {
	TrackerMonitor *monitor;
	TrackerCrawler *crawler;

	/* File queues for indexer */
	guint		item_queues_handler_id;

	GQueue         *items_created;
	GQueue         *items_updated;
	GQueue         *items_deleted;
	GQueue         *items_moved;

	GList          *directories;
	GList          *current_directory;

	GList          *devices;
	GList          *current_device;

	GTimer	       *timer;

	/* Status */
	gboolean        been_started;
	gboolean	interrupted;

	gboolean	finished_directories;
	gboolean	finished_devices;

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
} DirectoryData;

enum {
	CHECK_FILE,
	CHECK_DIRECTORY,
	PROCESS_FILE,
	MONITOR_DIRECTORY,
	FINISHED,
	LAST_SIGNAL
};

static void           process_finalize             (GObject             *object);
static gboolean       process_defaults             (TrackerMinerProcess *process,
						    GFile               *file);
static void           miner_started                (TrackerMiner        *miner);
static DirectoryData *directory_data_new           (const gchar         *path,
						    gboolean             recurse);
static void           directory_data_free          (DirectoryData       *dd);
static void           monitor_item_created_cb      (TrackerMonitor      *monitor,
						    GFile               *file,
						    gboolean             is_directory,
						    gpointer             user_data);
static void           monitor_item_updated_cb      (TrackerMonitor      *monitor,
						    GFile               *file,
						    gboolean             is_directory,
						    gpointer             user_data);
static void           monitor_item_deleted_cb      (TrackerMonitor      *monitor,
						    GFile               *file,
						    gboolean             is_directory,
						    gpointer             user_data);
static void           monitor_item_moved_cb        (TrackerMonitor      *monitor,
						    GFile               *file,
						    GFile               *other_file,
						    gboolean             is_directory,
						    gboolean             is_source_monitored,
						    gpointer             user_data);
static gboolean       crawler_process_file_cb      (TrackerCrawler      *crawler,
						    GFile               *file,
						    gpointer             user_data);
static gboolean       crawler_process_directory_cb (TrackerCrawler      *crawler,
						    GFile               *file,
						    gpointer             user_data);
static void           crawler_finished_cb          (TrackerCrawler      *crawler,
						    guint                directories_found,
						    guint                directories_ignored,
						    guint                files_found,
						    guint                files_ignored,
						    gpointer             user_data);
static void           process_continue             (TrackerMinerProcess *process);
static void           process_next                 (TrackerMinerProcess *process);
static void           process_directories_next     (TrackerMinerProcess *process);
static void           process_directories_start    (TrackerMinerProcess *process);
static void           process_directories_stop     (TrackerMinerProcess *process);

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_ABSTRACT_TYPE (TrackerMinerProcess, tracker_miner_process, TRACKER_TYPE_MINER)

static void
tracker_miner_process_class_init (TrackerMinerProcessClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
        TrackerMinerProcessClass *process_class = TRACKER_MINER_PROCESS_CLASS (klass);
        TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);

	object_class->finalize = process_finalize;

	if (0) {
		process_class->check_file         = process_defaults;
		process_class->check_directory    = process_defaults;
		process_class->monitor_directory  = process_defaults;
	}

        miner_class->started = miner_started;

	/*
	  miner_class->stopped = miner_crawler_stopped;
	  miner_class->paused  = miner_crawler_paused;
	  miner_class->resumed = miner_crawler_resumed;
	*/

	signals[CHECK_FILE] =
		g_signal_new ("check-file",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerProcessClass, check_file),
			      NULL, NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN, 1, G_TYPE_FILE);
	signals[CHECK_DIRECTORY] =
		g_signal_new ("check-directory",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerProcessClass, check_directory),
			      NULL, NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN, 1, G_TYPE_FILE);
	signals[PROCESS_FILE] =
		g_signal_new ("process-file",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerProcessClass, process_file),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_FILE);
	signals[MONITOR_DIRECTORY] =
		g_signal_new ("monitor-directory",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerProcessClass, monitor_directory),
			      NULL, NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN, 1, G_TYPE_FILE);
	signals[FINISHED] =
		g_signal_new ("finished",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerProcessClass, finished),
			      NULL, NULL,
			      tracker_marshal_VOID__UINT_UINT_UINT_UINT,
			      G_TYPE_NONE,
			      4,
			      G_TYPE_UINT,
			      G_TYPE_UINT,
			      G_TYPE_UINT,
			      G_TYPE_UINT);

	g_type_class_add_private (object_class, sizeof (TrackerMinerProcessPrivate));
}

static void
tracker_miner_process_init (TrackerMinerProcess *object)
{
	TrackerMinerProcessPrivate *priv;

	object->private = TRACKER_MINER_PROCESS_GET_PRIVATE (object);

	priv = object->private;

	tracker_module_config_init ();

	/* For each module we create a TrackerCrawler and keep them in
	 * a hash table to look up.
	 */
	priv->items_created = g_queue_new ();
	priv->items_updated = g_queue_new ();
	priv->items_deleted = g_queue_new ();
	priv->items_moved = g_queue_new ();

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
process_finalize (GObject *object)
{
	TrackerMinerProcessPrivate *priv;

	priv = TRACKER_MINER_PROCESS_GET_PRIVATE (object);

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

	if (priv->directories) {
		g_list_foreach (priv->directories, (GFunc) directory_data_free, NULL);
		g_list_free (priv->directories);
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

	G_OBJECT_CLASS (tracker_miner_process_parent_class)->finalize (object);
}

static gboolean 
process_defaults (TrackerMinerProcess *process,
		  GFile            *file)
{
	return TRUE;
}

static void
miner_started (TrackerMiner *miner)
{
	TrackerMinerProcess *process;

	process = TRACKER_MINER_PROCESS (miner);

	process->private->been_started = TRUE;

	process->private->interrupted = FALSE;

	process->private->finished_directories = FALSE;

	/* Disabled for now */
	process->private->finished_devices = TRUE;

	process_next (process);
}

static DirectoryData *
directory_data_new (const gchar *path,
		    gboolean     recurse)
{
	DirectoryData *dd;

	dd = g_slice_new (DirectoryData);

	dd->path = g_strdup (path);
	dd->recurse = recurse;

	return dd;
}

static void
directory_data_free (DirectoryData *dd)
{
	if (!dd) {
		return;
	}

	g_free (dd->path);
	g_slice_free (DirectoryData, dd);
}

static gboolean
item_queue_handlers_cb (gpointer user_data)
{
	TrackerMinerProcess *process;
	GFile *file;

	process = user_data;

	/* Deleted items first */
	file = g_queue_pop_head (process->private->items_deleted);
	if (file) {
		g_signal_emit (process, signals[PROCESS_FILE], 0, file);
		g_object_unref (file);
	
		return TRUE;
	}

	/* Created items next */
	file = g_queue_pop_head (process->private->items_created);
	if (file) {
		g_signal_emit (process, signals[PROCESS_FILE], 0, file);
		g_object_unref (file);
	
		return TRUE;
	}

	/* Updated items next */
	file = g_queue_pop_head (process->private->items_updated);
	if (file) {
		g_signal_emit (process, signals[PROCESS_FILE], 0, file);
		g_object_unref (file);
	
		return TRUE;
	}

	/* Moved items next */
	file = g_queue_pop_head (process->private->items_moved);
	if (file) {
		g_signal_emit (process, signals[PROCESS_FILE], 0, file);
		g_object_unref (file);
	
		return TRUE;
	}

	process->private->item_queues_handler_id = 0;

	return FALSE;
}

static void
item_queue_handlers_set_up (TrackerMinerProcess *process)
{
	if (process->private->item_queues_handler_id != 0) {
		return;
	}

	process->private->item_queues_handler_id =
		g_idle_add (item_queue_handlers_cb,
			    process);
}

static void
monitor_item_created_cb (TrackerMonitor *monitor,
			 GFile		*file,
			 gboolean	 is_directory,
			 gpointer	 user_data)
{
	TrackerMinerProcess *process;
	gboolean should_process = TRUE;
	gchar *path;

	process = user_data;

	g_signal_emit (process, signals[CHECK_FILE], 0, file, &should_process);

	path = g_file_get_path (file);

	g_debug ("%s:'%s' (%s) (create monitor event or user request)",
		 should_process ? "Found " : "Ignored",
		 path,
		 is_directory ? "DIR" : "FILE");

	if (should_process) {
		if (is_directory) {
			gboolean add_monitor = TRUE;

			g_signal_emit (process, signals[MONITOR_DIRECTORY], 0, file, &add_monitor);
			
			if (add_monitor) {
				tracker_monitor_add (process->private->monitor, file);	     
			}

			/* Add to the list */
			process->private->directories = 
				g_list_append (process->private->directories, 
					       directory_data_new (path, TRUE));

			/* Make sure we are handling that list */
			process->private->finished_directories = FALSE;

			if (process->private->finished_devices) {
				process_next (process);
			}
		}

		g_queue_push_tail (process->private->items_created, 
				   g_object_ref (file));
		
		item_queue_handlers_set_up (process);
	}

	g_free (path);
}

static void
monitor_item_updated_cb (TrackerMonitor *monitor,
			 GFile		*file,
			 gboolean	 is_directory,
			 gpointer	 user_data)
{
	TrackerMinerProcess *process;
	gchar *path;
	gboolean should_process = TRUE;

	process = user_data;

	g_signal_emit (process, signals[CHECK_FILE], 0, file, &should_process);

	path = g_file_get_path (file);

 	g_debug ("%s:'%s' (%s) (update monitor event or user request)",
		 should_process ? "Found " : "Ignored",
		 path,
		 is_directory ? "DIR" : "FILE");

	if (should_process) {
		g_queue_push_tail (process->private->items_updated, 
				   g_object_ref (file));
		
		item_queue_handlers_set_up (process);
	}

	g_free (path);
}

static void
monitor_item_deleted_cb (TrackerMonitor *monitor,
			 GFile		*file,
			 gboolean	 is_directory,
			 gpointer	 user_data)
{
	TrackerMinerProcess *process;
	gchar *path;
	gboolean should_process = TRUE;

	process = user_data;

	g_signal_emit (process, signals[CHECK_FILE], 0, file, &should_process);

	path = g_file_get_path (file);

	g_debug ("%s:'%s' (%s) (delete monitor event or user request)",
		 should_process ? "Found " : "Ignored",
		 path,
		 is_directory ? "DIR" : "FILE");

	if (should_process) {
		g_queue_push_tail (process->private->items_deleted, 
				   g_object_ref (file));
		
		item_queue_handlers_set_up (process);
	}

#if 0
	/* FIXME: Should we do this for MOVE events too? */

	/* Remove directory from list of directories we are going to
	 * iterate if it is in there.
	 */
	l = g_list_find_custom (process->private->directories, 
				path, 
				(GCompareFunc) g_strcmp0);

	/* Make sure we don't remove the current device we are
	 * processing, this is because we do this same clean up later
	 * in process_device_next() 
	 */
	if (l && l != process->private->current_directory) {
		directory_data_free (l->data);
		process->private->directories = 
			g_list_delete_link (process->private->directories, l);
	}
#endif

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
	TrackerMinerProcess *process;

	process = user_data;

	if (!is_source_monitored) {
		gchar *path;

		path = g_file_get_path (other_file);

#ifdef FIX
		/* If the source is not monitored, we need to crawl it. */
		tracker_crawler_add_unexpected_path (process->private->crawler, path);
#endif
		g_free (path);
	} else {
		gchar *path;
		gchar *other_path;
		gboolean should_process;
		gboolean should_process_other;
		
		path = g_file_get_path (file);
		other_path = g_file_get_path (other_file);

		g_signal_emit (process, signals[CHECK_FILE], 0, file, &should_process);
		g_signal_emit (process, signals[CHECK_FILE], 0, other_file, &should_process_other);
		
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
				g_queue_push_tail (process->private->items_created, 
						   g_object_ref (other_file));
				
				item_queue_handlers_set_up (process);
			} else {
				gboolean add_monitor = TRUE;
				
				g_signal_emit (process, signals[MONITOR_DIRECTORY], 0, file, &add_monitor);
				
				if (add_monitor) {
					tracker_monitor_add (process->private->monitor, file);	     
				}

#ifdef FIX
				/* If this is a directory we need to crawl it */
				tracker_crawler_add_unexpected_path (process->private->crawler, other_path);
#endif
			}
		} else if (!should_process_other) {
			/* Delete old file */
			g_queue_push_tail (process->private->items_deleted, g_object_ref (file));
			
			item_queue_handlers_set_up (process);
		} else {
			/* Move old file to new file */
			g_queue_push_tail (process->private->items_moved, g_object_ref (file));
			g_queue_push_tail (process->private->items_moved, g_object_ref (other_file));
			
			item_queue_handlers_set_up (process);
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
	TrackerMinerProcess *process;
	gboolean should_process = TRUE;

	process = user_data;

	g_signal_emit (process, signals[CHECK_FILE], 0, file, &should_process);

	if (should_process) {
		/* Add files in queue to our queues to send to the indexer */
		g_queue_push_tail (process->private->items_created, 
				   g_object_ref (file));
		item_queue_handlers_set_up (process);
	}

	return should_process;
}

static gboolean
crawler_process_directory_cb (TrackerCrawler *crawler,
			      GFile	     *file,
			      gpointer	      user_data)
{
	TrackerMinerProcess *process;
	gboolean should_process = TRUE;
	gboolean add_monitor = TRUE;

	process = user_data;

	g_signal_emit (process, signals[CHECK_DIRECTORY], 0, file, &should_process);
	
	if (should_process) {
		/* FIXME: Do we add directories to the queue? */
		g_queue_push_tail (process->private->items_created, 
				   g_object_ref (file));
		
		item_queue_handlers_set_up (process);
	}

	g_signal_emit (process, signals[MONITOR_DIRECTORY], 0, file, &add_monitor);

	/* Should we add? */
	if (add_monitor) {
		tracker_monitor_add (process->private->monitor, file);
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
	TrackerMinerProcess *process;

	process = user_data;

	/* Update stats */
	process->private->directories_found += directories_found;
	process->private->directories_ignored += directories_ignored;
	process->private->files_found += files_found;
	process->private->files_ignored += files_ignored;

	process->private->total_directories_found += directories_found;
	process->private->total_directories_ignored += directories_ignored;
	process->private->total_files_found += files_found;
	process->private->total_files_ignored += files_ignored;

	/* Proceed to next thing to process */
	process_continue (process);
}

static void
process_continue (TrackerMinerProcess *process)
{
	if (!process->private->finished_directories) {
		process_directories_next (process);
		return;
	}

#if 0
	if (!process->private->finished_devices) {
		process_device_next (process);
		return;
	}
#endif

	/* Nothing to do */
}

static void
process_next (TrackerMinerProcess *process)
{
	static gboolean shown_totals = FALSE;

	if (!process->private->finished_directories) {
		process_directories_start (process);
		return;
	}

#if 0
	if (!process->private->finished_devices) {
		process_devices_start (process);
		return;
	}
#endif

	/* Only do this the first time, otherwise the results are
	 * likely to be inaccurate. Devices can be added or removed so
	 * we can't assume stats are correct.
	 */
	if (!shown_totals) {
		shown_totals = TRUE;

		g_message ("--------------------------------------------------");
		g_message ("Total directories : %d (%d ignored)",
			   process->private->total_directories_found,
			   process->private->total_directories_ignored);
		g_message ("Total files       : %d (%d ignored)",
			   process->private->total_files_found,
			   process->private->total_files_ignored);
		g_message ("Total monitors    : %d",
			   tracker_monitor_get_count (process->private->monitor));
		g_message ("--------------------------------------------------\n");
	}

	/* Now we have finished crawling, we enable monitor events */
	g_message ("Enabling monitor events");
	tracker_monitor_set_enabled (process->private->monitor, TRUE);
}

static void
process_directories_next (TrackerMinerProcess *process)
{
	DirectoryData *dd;

	/* Don't recursively iterate the modules */
	if (!process->private->current_directory) {
		if (!process->private->finished_directories) {
			process->private->current_directory = process->private->directories;
		}
	} else {
		GList *l;

		l = process->private->current_directory;
		
		/* Now free that device so we don't recrawl it */
		if (l) {
			directory_data_free (l->data);
			
			process->private->current_directory = 
				process->private->directories = 
				g_list_delete_link (process->private->directories, l);
		}
	}

	/* If we have no further modules to iterate */
	if (!process->private->current_directory) {
		process_directories_stop (process);
		process_next (process);
		return;
	}

	dd = process->private->current_directory->data;

	tracker_crawler_start (process->private->crawler, 
			       dd->path, 
			       dd->recurse);
}

static void
process_directories_start (TrackerMinerProcess *process)
{
	g_message ("Process is starting to iterating directories");

	/* Go through dirs and crawl */
	if (!process->private->directories) {
		g_message ("No directories set up for process to handle, doing nothing");
		return;
	}

	if (process->private->timer) {
		g_timer_destroy (process->private->timer);
	}

	process->private->timer = g_timer_new ();

	process->private->finished_directories = FALSE;

	process->private->directories_found = 0;
	process->private->directories_ignored = 0;
	process->private->files_found = 0;
	process->private->files_ignored = 0;

	process_directories_next (process);
}

static void
process_directories_stop (TrackerMinerProcess *process)
{
	if (process->private->finished_directories) {
		return;
	}

	g_message ("--------------------------------------------------");
	g_message ("Process has %s iterating files",
		   process->private->interrupted ? "been stopped while" : "finished");

	process->private->finished_directories = TRUE;

	if (process->private->interrupted) {
		if (process->private->crawler) {
			tracker_crawler_stop (process->private->crawler);
		}

		if (process->private->timer) {
			g_timer_destroy (process->private->timer);
			process->private->timer = NULL;
		}
	} else {
		gdouble elapsed;
	
		if (process->private->timer) {
			g_timer_stop (process->private->timer);
			elapsed = g_timer_elapsed (process->private->timer, NULL);
		} else {
			elapsed = 0;
		}
		
		g_message ("FS time taken : %4.4f seconds",
			   elapsed);
		g_message ("FS directories: %d (%d ignored)",
			   process->private->directories_found,
			   process->private->directories_ignored);
		g_message ("FS files      : %d (%d ignored)",
			   process->private->files_found,
			   process->private->files_ignored);
	}

	g_message ("--------------------------------------------------\n");

	g_signal_emit (process, signals[FINISHED], 0,
		       process->private->total_directories_found,
		       process->private->total_directories_ignored,
		       process->private->total_files_found,
		       process->private->total_files_ignored);
}

void
tracker_miner_process_add_directory (TrackerMinerProcess *process,
				     const gchar         *path,
				     gboolean             recurse)
{
	g_return_if_fail (TRACKER_IS_PROCESS (process));
	g_return_if_fail (path != NULL);

	/* WHAT HAPPENS IF WE ADD DURING OPERATION ? */

	process->private->directories = 
		g_list_append (process->private->directories, 
			       directory_data_new (path, recurse));
}
