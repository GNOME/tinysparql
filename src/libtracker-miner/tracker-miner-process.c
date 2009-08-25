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

#include <libtracker-common/tracker-dbus.h>

#include "tracker-crawler.h"
#include "tracker-marshal.h"
#include "tracker-miner-process.h"
#include "tracker-monitor.h"

#define TRACKER_MINER_PROCESS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_PROCESS, TrackerMinerProcessPrivate))

typedef struct ItemMovedData ItemMovedData;

struct ItemMovedData {
	GFile *file;
	GFile *source_file;
};

typedef struct {
	gchar    *path;
	gboolean  recurse;
} DirectoryData;

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
	DirectoryData  *current_directory;

	GList          *devices;
	GList          *current_device;

	GTimer	       *timer;

	guint           process_dirs_id;

	/* Status */
	guint           been_started : 1;
	guint           shown_totals : 1;

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

enum {
	QUEUE_NONE,
	QUEUE_CREATED,
	QUEUE_UPDATED,
	QUEUE_DELETED,
	QUEUE_MOVED
};

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
static ItemMovedData *item_moved_data_new          (GFile               *file,
						    GFile               *source_file);
static void           item_moved_data_free         (ItemMovedData       *data);
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
						    gboolean             was_interrupted,
						    guint                directories_found,
						    guint                directories_ignored,
						    guint                files_found,
						    guint                files_ignored,
						    gpointer             user_data);
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

        miner_class->started = miner_started;

	process_class->check_file        = process_defaults;
	process_class->check_directory   = process_defaults;
	process_class->monitor_directory = process_defaults;

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
			      tracker_marshal_BOOLEAN__OBJECT_OBJECT,
			      G_TYPE_BOOLEAN, 2, G_TYPE_FILE, TRACKER_TYPE_SPARQL_BUILDER);
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
			      tracker_marshal_VOID__DOUBLE_UINT_UINT_UINT_UINT,
			      G_TYPE_NONE,
			      5,
			      G_TYPE_DOUBLE,
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

	process_directories_stop (TRACKER_MINER_PROCESS (object));

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

	g_queue_foreach (priv->items_moved, (GFunc) item_moved_data_free, NULL);
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
		  GFile               *file)
{
	return TRUE;
}

static void
miner_started (TrackerMiner *miner)
{
	TrackerMinerProcess *process;

	process = TRACKER_MINER_PROCESS (miner);

	process->private->been_started = TRUE;
	process_directories_start (process);
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

static ItemMovedData *
item_moved_data_new (GFile *file,
		     GFile *source_file)
{
	ItemMovedData *data;

	data = g_slice_new (ItemMovedData);
	data->file = g_object_ref (file);
	data->source_file = g_object_ref (source_file);

	return data;
}

static void
item_moved_data_free (ItemMovedData *data)
{
	g_object_unref (data->file);
	g_object_unref (data->source_file);
	g_slice_free (ItemMovedData, data);
}

static void
item_add_or_update (TrackerMinerProcess  *miner,
		    GFile                *file)
{
	TrackerSparqlBuilder *sparql;
	gchar *full_sparql, *uri;
	gboolean processed;

	sparql = tracker_sparql_builder_new_update ();
	g_signal_emit (miner, signals[PROCESS_FILE], 0, file, sparql, &processed);

	if (!processed) {
		g_object_unref (sparql);
		return;
	}

	uri = g_file_get_uri (file);

	g_debug ("Adding item '%s'", uri);

	tracker_sparql_builder_insert_close (sparql);

	full_sparql = g_strdup_printf ("DROP GRAPH <%s> %s",
		uri, tracker_sparql_builder_get_result (sparql));

	tracker_miner_execute_sparql (TRACKER_MINER (miner), full_sparql, NULL);
	g_free (full_sparql);
	g_object_unref (sparql);
}

static gboolean
query_resource_exists (TrackerMinerProcess *miner,
		       GFile               *file)
{
	TrackerClient *client;
	gboolean   result;
	gchar     *sparql, *uri;
	GPtrArray *sparql_result;

	uri = g_file_get_uri (file);
	sparql = g_strdup_printf ("SELECT ?s WHERE { ?s a rdfs:Resource . FILTER (?s = <%s>) }",
	                          uri);

	client = tracker_miner_get_client (TRACKER_MINER (miner));
	sparql_result = tracker_resources_sparql_query (client, sparql, NULL);

	result = (sparql_result && sparql_result->len == 1);

	tracker_dbus_results_ptr_array_free (&sparql_result);
	g_free (sparql);
	g_free (uri);

	return result;
}

static void
item_remove (TrackerMinerProcess *miner,
	     GFile               *file)
{
	gchar *sparql, *uri;

	uri = g_file_get_uri (file);

	g_debug ("Removing item: '%s' (Deleted from filesystem)",
		 uri);

	if (!query_resource_exists (miner, file)) {
		g_debug ("  File does not exist anyway (uri:'%s')", uri);
		return;
	}

	/* Delete resource */
	sparql = g_strdup_printf ("DELETE { <%s> a rdfs:Resource }", uri);
	tracker_miner_execute_sparql (TRACKER_MINER (miner), sparql, NULL);
	g_free (sparql);

	/* FIXME: Should delete recursively? */
}

static void
update_file_uri_recursively (TrackerMinerProcess *miner,
			     GString             *sparql_update,
			     const gchar         *source_uri,
			     const gchar         *uri)
{
	TrackerClient *client;
	gchar *sparql;
	GPtrArray *result_set;

	g_debug ("Moving item from '%s' to '%s'",
		 source_uri,
		 uri);

	/* FIXME: tracker:uri doesn't seem to exist */
	/* g_string_append_printf (sparql_update, " <%s> tracker:uri <%s> .", source_uri, uri); */

	client = tracker_miner_get_client (TRACKER_MINER (miner));
	sparql = g_strdup_printf ("SELECT ?child WHERE { ?child nfo:belongsToContainer <%s> }", source_uri);
	result_set = tracker_resources_sparql_query (client, sparql, NULL);
	g_free (sparql);

	if (result_set) {
		gint i;

		for (i = 0; i < result_set->len; i++) {
			gchar **child_source_uri, *child_uri;

			child_source_uri = g_ptr_array_index (result_set, i);

			if (!g_str_has_prefix (*child_source_uri, source_uri)) {
				g_warning ("Child URI '%s' does not start with parent URI '%s'",
				           *child_source_uri,
				           source_uri);
				continue;
			}

			child_uri = g_strdup_printf ("%s%s", uri, *child_source_uri + strlen (source_uri));

			update_file_uri_recursively (miner, sparql_update, *child_source_uri, child_uri);

			g_free (child_source_uri);
			g_free (child_uri);
		}

		g_ptr_array_free (result_set, TRUE);
	}
}

static void
item_move (TrackerMinerProcess *miner,
	   GFile               *file,
	   GFile               *source_file)
{
	gchar     *uri, *source_uri, *escaped_filename;
	GFileInfo *file_info;
	GString   *sparql;

	uri = g_file_get_uri (file);
	source_uri = g_file_get_uri (source_file);

	/* Get 'source' ID */
	if (!query_resource_exists (miner, source_file)) {
		g_message ("Source file '%s' not found in store to move, indexing '%s' from scratch", source_uri, uri);

		item_add_or_update (miner, file);

		g_free (source_uri);
		g_free (uri);

		return;
	}

	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
				       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				       NULL, NULL);

	if (!file_info) {
		/* Destination file has gone away, ignore dest file and remove source if any */
		item_remove (miner, source_file);

		g_free (source_uri);
		g_free (uri);

		return;
	}

	sparql = g_string_new ("");

	g_string_append_printf (sparql,
		"DELETE { <%s> nfo:fileName ?o } WHERE { <%s> nfo:fileName ?o }",
		source_uri, source_uri);

	g_string_append (sparql, " INSERT {");

	escaped_filename = g_strescape (g_file_info_get_display_name (file_info), NULL);

	g_string_append_printf (sparql, " <%s> nfo:fileName \"%s\" .", source_uri, escaped_filename);
	g_string_append_printf (sparql, " <%s> nie:isStoredAs <%s> .", source_uri, uri);

	/* FIXME: This function just seemed to update the thumbnail */
	/* update_file_uri_recursively (miner, sparql, source_uri, uri); */

	g_string_append (sparql, " }");

	tracker_miner_execute_sparql (TRACKER_MINER (miner), sparql->str, NULL);

	g_free (uri);
	g_free (source_uri);
	g_object_unref (file_info);
	g_string_free (sparql, TRUE);
}

static gint
get_next_file (TrackerMinerProcess  *miner,
	       GFile               **file,
	       GFile               **source_file)
{
	ItemMovedData *data;
	GFile *queue_file;

	/* Deleted items first */
	queue_file = g_queue_pop_head (miner->private->items_deleted);
	if (queue_file) {
		*file = queue_file;
		*source_file = NULL;
		return QUEUE_DELETED;
	}

	/* Created items next */
	queue_file = g_queue_pop_head (miner->private->items_created);
	if (queue_file) {
		*file = queue_file;
		*source_file = NULL;
		return QUEUE_CREATED;
	}

	/* Updated items next */
	queue_file = g_queue_pop_head (miner->private->items_updated);
	if (queue_file) {
		*file = queue_file;
		*source_file = NULL;
		return QUEUE_UPDATED;
	}

	/* Moved items next */
	data = g_queue_pop_head (miner->private->items_moved);
	if (data) {
		*file = g_object_ref (data->file);
		*source_file = g_object_ref (data->source_file);
		item_moved_data_free (data);

		return QUEUE_MOVED;
	}

	*file = NULL;
	*source_file = NULL;

	return QUEUE_NONE;
}

static gboolean
item_queue_handlers_cb (gpointer user_data)
{
	TrackerMinerProcess *miner;
	GFile *file, *source_file;
	gint queue;

	miner = user_data;
	queue = get_next_file (miner, &file, &source_file);

	if (queue == QUEUE_NONE) {
		/* No more files left to process */
		miner->private->item_queues_handler_id = 0;
		return FALSE;
	}

	switch (queue) {
	case QUEUE_MOVED:
		item_move (miner, file, source_file);
		break;
	case QUEUE_DELETED:
		item_remove (miner, file);
		break;
	case QUEUE_CREATED:
	case QUEUE_UPDATED:
		item_add_or_update (miner, file);
		break;
	default:
		g_assert_not_reached ();
	}

	g_object_unref (file);

	if (source_file) {
		g_object_unref (source_file);
	}

	return TRUE;
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

static gboolean
should_change_index_for_file (TrackerMinerProcess *miner,
			      GFile               *file)
{
	TrackerClient      *client;
	gboolean            uptodate;
	GPtrArray          *sparql_result;
	GFileInfo          *file_info;
	guint64             time;
	time_t              mtime;
	struct tm           t;
	gchar              *query, *uri;;

	file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
	if (!file_info) {
		/* NOTE: We return TRUE here because we want to update the DB
		 * about this file, not because we want to index it.
		 */
		return TRUE;
	}

	time = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	mtime = (time_t) time;
	g_object_unref (file_info);

	uri = g_file_get_uri (file);
	client = tracker_miner_get_client (TRACKER_MINER (miner));

	gmtime_r (&mtime, &t);

	query = g_strdup_printf ("SELECT ?file { ?file nfo:fileLastModified \"%04d-%02d-%02dT%02d:%02d:%02d\" . FILTER (?file = <%s>) }",
	                         t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, uri);
	sparql_result = tracker_resources_sparql_query (client, query, NULL);

	uptodate = (sparql_result && sparql_result->len == 1);

	tracker_dbus_results_ptr_array_free (&sparql_result);

	g_free (query);
	g_free (uri);

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
should_process_file (TrackerMinerProcess *process,
		     GFile               *file,
		     gboolean             is_dir)
{
	gboolean should_process;

	if (is_dir) {
		g_signal_emit (process, signals[CHECK_DIRECTORY], 0, file, &should_process);
	} else {
		g_signal_emit (process, signals[CHECK_FILE], 0, file, &should_process);
	}

	if (!should_process) {
		return FALSE;
	}

	if (is_dir) {
		/* We _HAVE_ to check ALL directories because mtime
		 * updates are not guaranteed on parents on Windows
		 * AND we on Linux only the immediate parent directory
		 * mtime is updated, this is not done recursively.
		 */
		return TRUE;
	} else {
		/* Check whether file is up-to-date in tracker-store */
		return should_change_index_for_file (process, file);
	}
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
	should_process = should_process_file (process, file, is_directory);

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
			tracker_miner_process_add_directory (process, path, TRUE);
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
	gboolean should_process;
	gchar *path;

	process = user_data;
	should_process = should_process_file (process, file, is_directory);

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
	gboolean should_process;
	gchar *path;

	process = user_data;
	should_process = should_process_file (process, file, is_directory);
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
		gboolean source_stored, should_process_other;

		path = g_file_get_path (file);
		other_path = g_file_get_path (other_file);

		source_stored = query_resource_exists (process, file);
		should_process_other = should_process_file (process, other_file, is_directory);

		g_debug ("%s:'%s'->'%s':%s (%s) (move monitor event or user request)",
			 source_stored ? "In store" : "Not in store",
			 path,
			 other_path,
			 should_process_other ? "Found " : "Ignored",
			 is_directory ? "DIR" : "FILE");

		/* FIXME: Guessing this soon the queue the event should pertain
		 *        to could introduce race conditions if events from other
		 *        queues for the same files are processed before items_moved,
		 *        Most of these decisions should be taken when the event is
		 *        actually being processed.
		 */
		if (!source_stored && !should_process_other) {
			/* Do nothing */
		} else if (!source_stored) {
			/* Source file was not stored, check dest file as new */
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
			g_queue_push_tail (process->private->items_moved,
					   item_moved_data_new (other_file, file));

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
	gchar *path;
	gboolean should_process;

	process = user_data;

	path = g_file_get_path (file);

	should_process = should_process_file (process, file, FALSE);

	if (should_process) {
		g_debug ("Found  :'%s'", path);

		/* Add files in queue to our queues to send to the indexer */
		g_queue_push_tail (process->private->items_created,
				   g_object_ref (file));
		item_queue_handlers_set_up (process);
	} else {
		g_debug ("Ignored:'%s'", path);
	}

	g_free (path);

	return should_process;
}

static gboolean
crawler_process_directory_cb (TrackerCrawler *crawler,
			      GFile	     *file,
			      gpointer	      user_data)
{
	TrackerMinerProcess *process;
	gchar *path;
	gboolean should_process;
	gboolean add_monitor = TRUE;

	process = user_data;

	path = g_file_get_path (file);
	should_process = should_process_file (process, file, TRUE);

	if (should_process) {
		g_debug ("Found  :'%s'", path);

		/* FIXME: Do we add directories to the queue? */
		g_queue_push_tail (process->private->items_created,
				   g_object_ref (file));

		item_queue_handlers_set_up (process);
	} else {
		g_debug ("Ignored:'%s'", path);
	}

	g_signal_emit (process, signals[MONITOR_DIRECTORY], 0, file, &add_monitor);

	/* Should we add? */
	if (add_monitor) {
		tracker_monitor_add (process->private->monitor, file);
	}

	g_free (path);

	return should_process;
}

static void
crawler_finished_cb (TrackerCrawler *crawler,
		     gboolean        was_interrupted,
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

	g_message ("%s crawling files after %2.2f seconds",
		   was_interrupted ? "Stoped" : "Finished",
		   g_timer_elapsed (process->private->timer, NULL));
	g_message ("  Found %d directories, ignored %d directories",
		   directories_found,
		   directories_ignored);
	g_message ("  Found %d files, ignored %d files",
		   files_found,
		   files_ignored);

	directory_data_free (process->private->current_directory);
	process->private->current_directory = NULL;

	/* Proceed to next thing to process */
	process_directories_start (process);
}

static void
print_stats (TrackerMinerProcess *process)
{
	/* Only do this the first time, otherwise the results are
	 * likely to be inaccurate. Devices can be added or removed so
	 * we can't assume stats are correct.
	 */
	if (!process->private->shown_totals) {
		process->private->shown_totals = TRUE;
		g_timer_stop (process->private->timer);

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
}

static gboolean
process_directories_cb (gpointer user_data)
{
	TrackerMinerProcess *miner = user_data;

	if (miner->private->current_directory) {
		g_critical ("One directory is already being processed, bailing out");
		miner->private->process_dirs_id = 0;
		return FALSE;
	}

	if (!miner->private->directories) {
		process_directories_stop (miner);
		return FALSE;
	}

	miner->private->current_directory = miner->private->directories->data;
	miner->private->directories = g_list_remove (miner->private->directories,
						     miner->private->current_directory);

	g_debug ("Processing %s path '%s'\n",
		 miner->private->current_directory->recurse ? "recursive" : "single",
		 miner->private->current_directory->path);

	if (tracker_crawler_start (miner->private->crawler,
				   miner->private->current_directory->path,
				   miner->private->current_directory->recurse)) {
		/* Crawler when restart the idle function when done */
		miner->private->process_dirs_id = 0;
		return FALSE;
	}

	/* Directory couldn't be processed */
	directory_data_free (miner->private->current_directory);
	miner->private->current_directory = NULL;

	return TRUE;
}

static void
process_directories_start (TrackerMinerProcess *process)
{
	if (process->private->process_dirs_id != 0) {
		/* Processing ALREADY going on */
		return;
	}

	if (!process->private->been_started) {
		/* Miner has not been started yet */
		return;
	}

	process->private->timer = g_timer_new ();

	process->private->total_directories_found = 0;
	process->private->total_directories_ignored = 0;
	process->private->total_files_found = 0;
	process->private->total_files_ignored = 0;
	process->private->directories_found = 0;
	process->private->directories_ignored = 0;
	process->private->files_found = 0;
	process->private->files_ignored = 0;

	process->private->process_dirs_id = g_idle_add (process_directories_cb, process);
}

static void
process_directories_stop (TrackerMinerProcess *process)
{
	if (process->private->process_dirs_id == 0) {
		/* No processing going on, nothing to stop */
		return;
	}

	if (process->private->current_directory) {
		tracker_crawler_stop (process->private->crawler);
	}

	/* Now we have finished crawling, print stats and enable monitor events */
	print_stats (process);
	
	g_message ("Enabling monitor events");
	tracker_monitor_set_enabled (process->private->monitor, TRUE);
	
	/* Is this the right time to emit FINISHED? What about
	 * monitor events left to handle? Should they matter
	 * here?
	 */
	g_signal_emit (process, signals[FINISHED], 0, 
		       g_timer_elapsed (process->private->timer, NULL),
		       process->private->total_directories_found,
		       process->private->total_directories_ignored,
		       process->private->total_files_found,
		       process->private->total_files_ignored);

	if (process->private->timer) {
		g_timer_destroy (process->private->timer);
		process->private->timer = NULL;
	}

	if (process->private->process_dirs_id != 0) {
		g_source_remove (process->private->process_dirs_id);
		process->private->process_dirs_id = 0;
	}
}

void
tracker_miner_process_add_directory (TrackerMinerProcess *process,
				     const gchar         *path,
				     gboolean             recurse)
{
	g_return_if_fail (TRACKER_IS_PROCESS (process));
	g_return_if_fail (path != NULL);

	process->private->directories =
		g_list_append (process->private->directories,
			       directory_data_new (path, recurse));

	process_directories_start (process);
}

gboolean
tracker_miner_process_remove_directory (TrackerMinerProcess *process,
					const gchar         *path)
{
	gboolean return_val = FALSE;
	GList *l;

	g_return_val_if_fail (TRACKER_IS_PROCESS (process), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	if (process->private->current_directory &&
	    strcmp (process->private->current_directory->path, path) == 0) {
		/* Dir is being processed currently, cancel crawler */
		tracker_crawler_stop (process->private->crawler);
		return_val = TRUE;
	}

	l = g_list_find_custom (process->private->directories, path,
				(GCompareFunc) g_strcmp0);

	if (l) {
		directory_data_free (l->data);
		process->private->directories =
			g_list_delete_link (process->private->directories, l);
		return_val = TRUE;
	}

	return return_val;
}
