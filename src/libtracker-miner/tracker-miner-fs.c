/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <libtracker-common/tracker-dbus.h>

#include "tracker-crawler.h"
#include "tracker-marshal.h"
#include "tracker-miner-fs.h"
#include "tracker-monitor.h"
#include "tracker-utils.h"

#define TRACKER_MINER_FS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_FS, TrackerMinerFSPrivate))

typedef struct {
	GFile *file;
	GFile *source_file;
} ItemMovedData;

typedef struct {
	GFile    *file;
	gboolean  recurse;
} DirectoryData;

struct TrackerMinerFSPrivate {
	TrackerMonitor *monitor;
	TrackerCrawler *crawler;

	/* File queues for indexer */
	GQueue         *items_created;
	GQueue         *items_updated;
	GQueue         *items_deleted;
	GQueue         *items_moved;

	GQuark          quark_ignore_file;

	GList          *directories;
	DirectoryData  *current_directory;

	GTimer	       *timer;

	guint           crawl_directories_id;
	guint		item_queues_handler_id;

	GFile          *current_file;
	GCancellable   *cancellable;

	/* Status */
	guint           been_started : 1;
	guint           been_crawled : 1;
	guint           shown_totals : 1;
	guint           is_paused : 1;

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
	MONITOR_DIRECTORY,
	FINISHED,
	LAST_SIGNAL
};

static void           fs_finalize                  (GObject        *object);
static gboolean       fs_defaults                  (TrackerMinerFS *fs,
						    GFile          *file);
static void           miner_started                (TrackerMiner   *miner);
static void           miner_stopped                (TrackerMiner   *miner);
static void           miner_paused                 (TrackerMiner   *miner);
static void           miner_resumed                (TrackerMiner   *miner);

static DirectoryData *directory_data_new           (GFile          *file,
						    gboolean        recurse);
static void           directory_data_free          (DirectoryData  *dd);
static ItemMovedData *item_moved_data_new          (GFile          *file,
						    GFile          *source_file);
static void           item_moved_data_free         (ItemMovedData  *data);
static void           monitor_item_created_cb      (TrackerMonitor *monitor,
						    GFile          *file,
						    gboolean        is_directory,
						    gpointer        user_data);
static void           monitor_item_updated_cb      (TrackerMonitor *monitor,
						    GFile          *file,
						    gboolean        is_directory,
						    gpointer        user_data);
static void           monitor_item_deleted_cb      (TrackerMonitor *monitor,
						    GFile          *file,
						    gboolean        is_directory,
						    gpointer        user_data);
static void           monitor_item_moved_cb        (TrackerMonitor *monitor,
						    GFile          *file,
						    GFile          *other_file,
						    gboolean        is_directory,
						    gboolean        is_source_monitored,
						    gpointer        user_data);
static gboolean       crawler_check_file_cb        (TrackerCrawler *crawler,
						    GFile          *file,
						    gpointer        user_data);
static gboolean       crawler_check_directory_cb   (TrackerCrawler *crawler,
						    GFile          *file,
						    gpointer        user_data);
static void           crawler_finished_cb          (TrackerCrawler *crawler,
						    GQueue         *found,
						    gboolean        was_interrupted,
						    guint           directories_found,
						    guint           directories_ignored,
						    guint           files_found,
						    guint           files_ignored,
						    gpointer        user_data);
static void           crawl_directories_start      (TrackerMinerFS *fs);
static void           crawl_directories_stop       (TrackerMinerFS *fs);

static void           item_queue_handlers_set_up   (TrackerMinerFS *fs);


static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_ABSTRACT_TYPE (TrackerMinerFS, tracker_miner_fs, TRACKER_TYPE_MINER)

static void
tracker_miner_fs_class_init (TrackerMinerFSClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
        TrackerMinerFSClass *fs_class = TRACKER_MINER_FS_CLASS (klass);
        TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);

	object_class->finalize = fs_finalize;

        miner_class->started = miner_started;
        miner_class->stopped = miner_stopped;
	miner_class->paused  = miner_paused;
	miner_class->resumed = miner_resumed;

	fs_class->check_file        = fs_defaults;
	fs_class->check_directory   = fs_defaults;
	fs_class->monitor_directory = fs_defaults;

	signals[CHECK_FILE] =
		g_signal_new ("check-file",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerFSClass, check_file),
			      tracker_accumulator_check_file,
			      NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN, 1, G_TYPE_FILE);
	signals[CHECK_DIRECTORY] =
		g_signal_new ("check-directory",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerFSClass, check_directory),
			      tracker_accumulator_check_file,
			      NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN, 1, G_TYPE_FILE);
	signals[MONITOR_DIRECTORY] =
		g_signal_new ("monitor-directory",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerFSClass, monitor_directory),
			      tracker_accumulator_check_file,
			      NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN, 1, G_TYPE_FILE);
	signals[FINISHED] =
		g_signal_new ("finished",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerFSClass, finished),
			      NULL, NULL,
			      tracker_marshal_VOID__DOUBLE_UINT_UINT_UINT_UINT,
			      G_TYPE_NONE,
			      5,
			      G_TYPE_DOUBLE,
			      G_TYPE_UINT,
			      G_TYPE_UINT,
			      G_TYPE_UINT,
			      G_TYPE_UINT);

	g_type_class_add_private (object_class, sizeof (TrackerMinerFSPrivate));
}

static void
tracker_miner_fs_init (TrackerMinerFS *object)
{
	TrackerMinerFSPrivate *priv;

	object->private = TRACKER_MINER_FS_GET_PRIVATE (object);

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

	g_signal_connect (priv->crawler, "check-file",
			  G_CALLBACK (crawler_check_file_cb),
			  object);
	g_signal_connect (priv->crawler, "check-directory",
			  G_CALLBACK (crawler_check_directory_cb),
			  object);
	g_signal_connect (priv->crawler, "finished",
			  G_CALLBACK (crawler_finished_cb),
			  object);

	/* Set up the monitor */
	priv->monitor = tracker_monitor_new ();

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

	priv->quark_ignore_file = g_quark_from_static_string ("tracker-ignore-file");
}

static void
fs_finalize (GObject *object)
{
	TrackerMinerFSPrivate *priv;

	priv = TRACKER_MINER_FS_GET_PRIVATE (object);

	if (priv->timer) {
		g_timer_destroy (priv->timer);
	}

	if (priv->item_queues_handler_id) {
		g_source_remove (priv->item_queues_handler_id);
		priv->item_queues_handler_id = 0;
	}

	crawl_directories_stop (TRACKER_MINER_FS (object));

	g_object_unref (priv->crawler);
	g_object_unref (priv->monitor);

	if (priv->cancellable) {
		g_object_unref (priv->cancellable);
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

	G_OBJECT_CLASS (tracker_miner_fs_parent_class)->finalize (object);
}

static gboolean 
fs_defaults (TrackerMinerFS *fs,
	     GFile          *file)
{
	return TRUE;
}

static void
miner_started (TrackerMiner *miner)
{
	TrackerMinerFS *fs;

	fs = TRACKER_MINER_FS (miner);

	fs->private->been_started = TRUE;

	g_object_set (miner, 
		      "progress", 0.0, 
		      "status", _("Initializing"),
		      NULL);

	crawl_directories_start (fs);
}

static void
miner_stopped (TrackerMiner *miner)
{
	g_object_set (miner, 
		      "progress", 1.0, 
		      "status", _("Idle"),
		      NULL);
}

static void
miner_paused (TrackerMiner *miner)
{
	TrackerMinerFS *fs;

	fs = TRACKER_MINER_FS (miner);

	fs->private->is_paused = TRUE;

	tracker_crawler_pause (fs->private->crawler);

	if (fs->private->item_queues_handler_id) {
		g_source_remove (fs->private->item_queues_handler_id);
		fs->private->item_queues_handler_id = 0;
	}
}

static void
miner_resumed (TrackerMiner *miner)
{
	TrackerMinerFS *fs;

	fs = TRACKER_MINER_FS (miner);

	fs->private->is_paused = FALSE;

	tracker_crawler_resume (fs->private->crawler);

	/* Only set up queue handler if we have items waiting to be
	 * processed.
	 */
	if (g_queue_get_length (fs->private->items_deleted) > 0 ||
	    g_queue_get_length (fs->private->items_created) > 0 ||
	    g_queue_get_length (fs->private->items_updated) > 0 ||
	    g_queue_get_length (fs->private->items_moved) > 0) {
		item_queue_handlers_set_up (fs);
	}
}

static DirectoryData *
directory_data_new (GFile    *file,
		    gboolean  recurse)
{
	DirectoryData *dd;

	dd = g_slice_new (DirectoryData);

	dd->file = g_object_ref (file);
	dd->recurse = recurse;

	return dd;
}

static void
directory_data_free (DirectoryData *dd)
{
	if (!dd) {
		return;
	}

	g_object_unref (dd->file);
	g_slice_free (DirectoryData, dd);
}

static void
process_print_stats (TrackerMinerFS *fs)
{
	/* Only do this the first time, otherwise the results are
	 * likely to be inaccurate. Devices can be added or removed so
	 * we can't assume stats are correct.
	 */
	if (!fs->private->shown_totals) {
		fs->private->shown_totals = TRUE;

		g_message ("--------------------------------------------------");
		g_message ("Total directories : %d (%d ignored)",
			   fs->private->total_directories_found,
			   fs->private->total_directories_ignored);
		g_message ("Total files       : %d (%d ignored)",
			   fs->private->total_files_found,
			   fs->private->total_files_ignored);
		g_message ("Total monitors    : %d",
			   tracker_monitor_get_count (fs->private->monitor));
		g_message ("--------------------------------------------------\n");
	}
}

static void
process_stop (TrackerMinerFS *fs) 
{
	/* Now we have finished crawling, print stats and enable monitor events */
	process_print_stats (fs);

	tracker_miner_commit (TRACKER_MINER (fs));

	g_message ("Idle");

	g_object_set (fs, 
		      "progress", 1.0, 
		      "status", _("Idle"),
		      NULL);

	g_signal_emit (fs, signals[FINISHED], 0,
		       g_timer_elapsed (fs->private->timer, NULL),
		       fs->private->total_directories_found,
		       fs->private->total_directories_ignored,
		       fs->private->total_files_found,
		       fs->private->total_files_ignored);

	if (fs->private->timer) {
		g_timer_destroy (fs->private->timer);
		fs->private->timer = NULL;
	}

	fs->private->total_directories_found = 0;
	fs->private->total_directories_ignored = 0;
	fs->private->total_files_found = 0;
	fs->private->total_files_ignored = 0;

	fs->private->been_crawled = TRUE;
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
item_add_or_update_cb (TrackerMinerFS       *fs,
		       GFile                *file,
		       TrackerSparqlBuilder *sparql,
		       const GError         *error,
		       gpointer              user_data)
{
	gchar *uri;

	uri = g_file_get_uri (file);

	if (error) {
		g_warning ("Could not process '%s': %s", uri, error->message);
	} else {
		gchar *full_sparql;

		g_debug ("Adding item '%s'", uri);

		full_sparql = g_strdup_printf ("DROP GRAPH <%s> %s",
					       uri, tracker_sparql_builder_get_result (sparql));

		tracker_miner_execute_sparql (TRACKER_MINER (fs), full_sparql, NULL);
		g_free (full_sparql);

		if (fs->private->been_crawled) {
			/* Only commit immediately for
			 * changes after initial crawling.
			 */
			tracker_miner_commit (TRACKER_MINER (fs));
		}
	}

	if (fs->private->cancellable) {
		g_object_unref (fs->private->cancellable);
		fs->private->cancellable = NULL;
	}

	if (fs->private->current_file) {
		g_object_unref (fs->private->current_file);
		fs->private->current_file = NULL;
	}

	g_object_unref (sparql);
	g_free (uri);

	/* Processing is now done, continue with other files */
	item_queue_handlers_set_up (fs);
}

static gboolean
item_add_or_update (TrackerMinerFS *fs,
		    GFile          *file)
{
	TrackerSparqlBuilder *sparql;
	gboolean processing;

	if (fs->private->cancellable) {
		g_debug ("Cancellable for older operation still around, destroying");
		g_object_unref (fs->private->cancellable);
	}

	fs->private->cancellable = g_cancellable_new ();
	sparql = tracker_sparql_builder_new_update ();

	processing = TRACKER_MINER_FS_GET_CLASS (fs)->process_file (fs, file, sparql,
								    fs->private->cancellable,
								    item_add_or_update_cb,
								    NULL);

	if (!processing) {
		g_object_unref (sparql);
		g_object_unref (fs->private->cancellable);
		fs->private->cancellable = NULL;

		return TRUE;
	} else {
		fs->private->current_file = g_object_ref (file);
	}

	return FALSE;
}

static gboolean
item_query_exists (TrackerMinerFS *miner,
		   GFile          *file)
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
item_remove (TrackerMinerFS *fs,
	     GFile          *file)
{
	gchar *sparql, *uri;

	uri = g_file_get_uri (file);

	g_debug ("Removing item: '%s' (Deleted from filesystem)",
		 uri);

	if (!item_query_exists (fs, file)) {
		g_debug ("  File does not exist anyway (uri:'%s')", uri);
		return;
	}

	/* Delete resource */
	sparql = g_strdup_printf ("DELETE { <%s> a rdfs:Resource }", uri);
	tracker_miner_execute_sparql (TRACKER_MINER (fs), sparql, NULL);
	g_free (sparql);

	/* FIXME: Should delete recursively? */
}

static void
update_file_uri_recursively (TrackerMinerFS *fs,
			     GString        *sparql_update,
			     const gchar    *source_uri,
			     const gchar    *uri)
{
	TrackerClient *client;
	gchar *sparql;
	GPtrArray *result_set;

	g_debug ("Moving item from '%s' to '%s'",
		 source_uri,
		 uri);

	/* FIXME: tracker:uri doesn't seem to exist */
	/* g_string_append_printf (sparql_update, " <%s> tracker:uri <%s> .", source_uri, uri); */

	client = tracker_miner_get_client (TRACKER_MINER (fs));
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

			update_file_uri_recursively (fs, sparql_update, *child_source_uri, child_uri);

			g_free (child_source_uri);
			g_free (child_uri);
		}

		g_ptr_array_free (result_set, TRUE);
	}
}

static gboolean
item_move (TrackerMinerFS *fs,
	   GFile          *file,
	   GFile          *source_file)
{
	gchar     *uri, *source_uri, *escaped_filename;
	GFileInfo *file_info;
	GString   *sparql;

	uri = g_file_get_uri (file);
	source_uri = g_file_get_uri (source_file);

	/* Get 'source' ID */
	if (!item_query_exists (fs, source_file)) {
		gboolean retval;

		g_message ("Source file '%s' not found in store to move, indexing '%s' from scratch", source_uri, uri);

		retval = item_add_or_update (fs, file);

		g_free (source_uri);
		g_free (uri);

		return retval;
	}

	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
				       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				       NULL, NULL);

	if (!file_info) {
		/* Destination file has gone away, ignore dest file and remove source if any */
		item_remove (fs, source_file);

		g_free (source_uri);
		g_free (uri);

		return TRUE;
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
	/* update_file_uri_recursively (fs, sparql, source_uri, uri); */

	g_string_append (sparql, " }");

	tracker_miner_execute_sparql (TRACKER_MINER (fs), sparql->str, NULL);

	g_free (uri);
	g_free (source_uri);
	g_object_unref (file_info);
	g_string_free (sparql, TRUE);

	return TRUE;
}

static gint
item_queue_get_next_file (TrackerMinerFS  *fs,
			  GFile          **file,
			  GFile          **source_file)
{
	ItemMovedData *data;
	GFile *queue_file;

	/* Deleted items first */
	queue_file = g_queue_pop_head (fs->private->items_deleted);
	if (queue_file) {
		*file = queue_file;
		*source_file = NULL;
		return QUEUE_DELETED;
	}

	/* Created items next */
	queue_file = g_queue_pop_head (fs->private->items_created);
	if (queue_file) {
		*file = queue_file;
		*source_file = NULL;
		return QUEUE_CREATED;
	}

	/* Updated items next */
	queue_file = g_queue_pop_head (fs->private->items_updated);
	if (queue_file) {
		*file = queue_file;
		*source_file = NULL;
		return QUEUE_UPDATED;
	}

	/* Moved items next */
	data = g_queue_pop_head (fs->private->items_moved);
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

static gdouble
item_queue_get_progress (TrackerMinerFS *fs)
{
	guint items_to_process = 0;
	guint items_total = 0;
	
	items_to_process += g_queue_get_length (fs->private->items_deleted);
	items_to_process += g_queue_get_length (fs->private->items_created);
	items_to_process += g_queue_get_length (fs->private->items_updated);
	items_to_process += g_queue_get_length (fs->private->items_moved);

	items_total += fs->private->total_directories_found;
	items_total += fs->private->total_files_found;

	if (items_to_process == 0 && items_total > 0) {
		return 0.0;
	}

	if (items_total == 0 || items_to_process > items_total) {
		return 1.0;
	}

	return (gdouble) (items_total - items_to_process) / items_total;
}

static gboolean
item_queue_handlers_cb (gpointer user_data)
{
	TrackerMinerFS *fs;
	GFile *file, *source_file;
	gint queue;
	GTimeVal time_now;
	static GTimeVal time_last = { 0 };
	gboolean keep_processing = TRUE;

	fs = user_data;
	queue = item_queue_get_next_file (fs, &file, &source_file);

	/* Update progress, but don't spam it. */
	g_get_current_time (&time_now);

	if ((time_now.tv_sec - time_last.tv_sec) >= 1) {
		time_last = time_now;
		g_object_set (fs, "progress", item_queue_get_progress (fs), NULL);
	}

	/* Handle queues */
	switch (queue) {
	case QUEUE_NONE:
		/* Print stats and signal finished */
		process_stop (fs);

		/* No more files left to process */
		keep_processing = FALSE;
		break;
	case QUEUE_MOVED:
		keep_processing = item_move (fs, file, source_file);
		break;
	case QUEUE_DELETED:
		item_remove (fs, file);
		break;
	case QUEUE_CREATED:
	case QUEUE_UPDATED:
		keep_processing = item_add_or_update (fs, file);
		break;
	default:
		g_assert_not_reached ();
	}

	if (file) {
		g_object_unref (file);
	}

	if (source_file) {
		g_object_unref (source_file);
	}

	if (!keep_processing) {
		fs->private->item_queues_handler_id = 0;
		return FALSE;
	} else {
		if (fs->private->been_crawled) {
			/* Only commit immediately for
			 * changes after initial crawling.
			 */
			tracker_miner_commit (TRACKER_MINER (fs));
		}

		return TRUE;
	}
}

static void
item_queue_handlers_set_up (TrackerMinerFS *fs)
{
	gchar *status;

	if (fs->private->item_queues_handler_id != 0) {
		return;
	}

	if (fs->private->is_paused) {
		return;
	}

	if (!fs->private->timer) {
		fs->private->timer = g_timer_new ();
	}

	g_object_get (fs, "status", &status, NULL);

	if (g_strcmp0 (status, _("Processing files")) != 0) {
		/* Don't spam this */
		g_message ("Processing files...");
		g_object_set (fs, "status", _("Processing files"), NULL);
	}

	g_free (status);

	fs->private->item_queues_handler_id =
		g_idle_add (item_queue_handlers_cb,
			    fs);
}

static gboolean
should_change_index_for_file (TrackerMinerFS *fs,
			      GFile          *file)
{
	TrackerClient      *client;
	gboolean            uptodate;
	GPtrArray          *sparql_result;
	GFileInfo          *file_info;
	guint64             time;
	time_t              mtime;
	struct tm           t;
	gchar              *query, *uri;

	file_info = g_file_query_info (file, 
				       G_FILE_ATTRIBUTE_TIME_MODIFIED, 
				       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, 
				       NULL, 
				       NULL);
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
	client = tracker_miner_get_client (TRACKER_MINER (fs));

	gmtime_r (&mtime, &t);

	query = g_strdup_printf ("SELECT ?file { ?file nfo:fileLastModified \"%04d-%02d-%02dT%02d:%02d:%02d\" . FILTER (?file = <%s>) }",
	                         t.tm_year + 1900, 
				 t.tm_mon + 1, 
				 t.tm_mday, 
				 t.tm_hour, 
				 t.tm_min, 
				 t.tm_sec, 
				 uri);
	sparql_result = tracker_resources_sparql_query (client, query, NULL);

	uptodate = sparql_result && sparql_result->len == 1;

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
should_check_file (TrackerMinerFS *fs,
		   GFile          *file,
		   gboolean        is_dir)
{
	gboolean should_check;

	if (is_dir) {
		g_signal_emit (fs, signals[CHECK_DIRECTORY], 0, file, &should_check);
	} else {
		g_signal_emit (fs, signals[CHECK_FILE], 0, file, &should_check);
	}

	return should_check;
}

static gboolean
should_process_file (TrackerMinerFS *fs,
		     GFile          *file,
		     gboolean        is_dir)
{
	if (!should_check_file (fs, file, is_dir)) {
		return FALSE;
	}

	/* Check whether file is up-to-date in tracker-store */
	return should_change_index_for_file (fs, file);
}

static void
monitor_item_created_cb (TrackerMonitor *monitor,
			 GFile		*file,
			 gboolean	 is_directory,
			 gpointer	 user_data)
{
	TrackerMinerFS *fs;
	gboolean should_process = TRUE;
	gchar *path;

	fs = user_data;
	should_process = should_check_file (fs, file, is_directory);

	path = g_file_get_path (file);

	g_debug ("%s:'%s' (%s) (create monitor event or user request)",
		 should_process ? "Found " : "Ignored",
		 path,
		 is_directory ? "DIR" : "FILE");

	if (should_process) {
		if (is_directory) {
			gboolean add_monitor = TRUE;

			g_signal_emit (fs, signals[MONITOR_DIRECTORY], 0, file, &add_monitor);

			if (add_monitor) {
				tracker_monitor_add (fs->private->monitor, file);
			}

			/* Add to the list */
			tracker_miner_fs_add_directory (fs, file, TRUE);
		}

		g_queue_push_tail (fs->private->items_created,
				   g_object_ref (file));

		item_queue_handlers_set_up (fs);
	}

	g_free (path);
}

static void
monitor_item_updated_cb (TrackerMonitor *monitor,
			 GFile		*file,
			 gboolean	 is_directory,
			 gpointer	 user_data)
{
	TrackerMinerFS *fs;
	gboolean should_process;
	gchar *path;

	fs = user_data;
	should_process = should_check_file (fs, file, is_directory);

	path = g_file_get_path (file);

 	g_debug ("%s:'%s' (%s) (update monitor event or user request)",
		 should_process ? "Found " : "Ignored",
		 path,
		 is_directory ? "DIR" : "FILE");

	if (should_process) {
		g_queue_push_tail (fs->private->items_updated,
				   g_object_ref (file));

		item_queue_handlers_set_up (fs);
	}

	g_free (path);
}

static void
monitor_item_deleted_cb (TrackerMonitor *monitor,
			 GFile		*file,
			 gboolean	 is_directory,
			 gpointer	 user_data)
{
	TrackerMinerFS *fs;
	gboolean should_process;
	gchar *path;

	fs = user_data;
	should_process = should_check_file (fs, file, is_directory);
	path = g_file_get_path (file);

	g_debug ("%s:'%s' (%s) (delete monitor event or user request)",
		 should_process ? "Found " : "Ignored",
		 path,
		 is_directory ? "DIR" : "FILE");

	if (should_process) {
		g_queue_push_tail (fs->private->items_deleted,
				   g_object_ref (file));

		item_queue_handlers_set_up (fs);
	}

#if 0
	/* FIXME: Should we do this for MOVE events too? */

	/* Remove directory from list of directories we are going to
	 * iterate if it is in there.
	 */
	l = g_list_find_custom (fs->private->directories, 
				path, 
				(GCompareFunc) g_strcmp0);

	/* Make sure we don't remove the current device we are
	 * processing, this is because we do this same clean up later
	 * in process_device_next() 
	 */
	if (l && l != fs->private->current_directory) {
		directory_data_free (l->data);
		fs->private->directories = 
			g_list_delete_link (fs->private->directories, l);
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
	TrackerMinerFS *fs;

	fs = user_data;

	if (!is_source_monitored) {
		gchar *path;

		path = g_file_get_path (other_file);

#ifdef FIX
		/* If the source is not monitored, we need to crawl it. */
		tracker_crawler_add_unexpected_path (fs->private->crawler, path);
#endif
		g_free (path);
	} else {
		gchar *path;
		gchar *other_path;
		gboolean source_stored, should_process_other;

		path = g_file_get_path (file);
		other_path = g_file_get_path (other_file);

		source_stored = item_query_exists (fs, file);
		should_process_other = should_check_file (fs, other_file, is_directory);

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
				g_queue_push_tail (fs->private->items_created, 
						   g_object_ref (other_file));
				
				item_queue_handlers_set_up (fs);
			} else {
				gboolean add_monitor = TRUE;
				
				g_signal_emit (fs, signals[MONITOR_DIRECTORY], 0, file, &add_monitor);
				
				if (add_monitor) {
					tracker_monitor_add (fs->private->monitor, file);	     
				}

#ifdef FIX
				/* If this is a directory we need to crawl it */
				tracker_crawler_add_unexpected_path (fs->private->crawler, other_path);
#endif
			}
		} else if (!should_process_other) {
			/* Delete old file */
			g_queue_push_tail (fs->private->items_deleted, g_object_ref (file));
			
			item_queue_handlers_set_up (fs);
		} else {
			/* Move old file to new file */
			g_queue_push_tail (fs->private->items_moved,
					   item_moved_data_new (other_file, file));

			item_queue_handlers_set_up (fs);
		}
		
		g_free (other_path);
		g_free (path);
	}
}

static gboolean
crawler_check_file_cb (TrackerCrawler *crawler,
		       GFile	      *file,
		       gpointer        user_data)
{
	TrackerMinerFS *fs = user_data;

	return should_process_file (fs, file, FALSE);
}

static gboolean
crawler_check_directory_cb (TrackerCrawler *crawler,
			    GFile	   *file,
			    gpointer	    user_data)
{
	TrackerMinerFS *fs = user_data;
	gboolean should_check, should_change_index;
	gboolean add_monitor = TRUE;

	should_check = should_check_file (fs, file, TRUE);
	should_change_index = should_change_index_for_file (fs, file);

	if (!should_change_index) {
		/* Mark the file as ignored, we still want the crawler
		 * to iterate over its contents, but the directory hasn't
		 * actually changed, hence this flag.
		 */
		g_object_set_qdata (G_OBJECT (file),
				    fs->private->quark_ignore_file,
				    GINT_TO_POINTER (TRUE));
	}

	g_signal_emit (fs, signals[MONITOR_DIRECTORY], 0, file, &add_monitor);

	/* FIXME: Should we add here or when we process the queue in
	 * the finished sig? 
	 */
	if (add_monitor) {
		tracker_monitor_add (fs->private->monitor, file);
	}

	/* We _HAVE_ to check ALL directories because mtime updates
	 * are not guaranteed on parents on Windows AND we on Linux
	 * only the immediate parent directory mtime is updated, this
	 * is not done recursively.
	 *
	 * As such, we only use the "check" rules here, we don't do
	 * any database comparison with mtime. 
	 */
	return should_check;
}

static void
crawler_finished_cb (TrackerCrawler *crawler,
		     GQueue         *found,
		     gboolean        was_interrupted,
		     guint	     directories_found,
		     guint	     directories_ignored,
		     guint	     files_found,
		     guint	     files_ignored,
		     gpointer	     user_data)
{
	TrackerMinerFS *fs = user_data;
	GList *l;

	/* Add items in queue to current queues. */
	for (l = found->head; l; l = l->next) {
		GFile *file = l->data;

		if (!g_object_get_qdata (G_OBJECT (file), fs->private->quark_ignore_file)) {
			g_queue_push_tail (fs->private->items_created, g_object_ref (file));
		}
	}

	/* Update stats */
	fs->private->directories_found += directories_found;
	fs->private->directories_ignored += directories_ignored;
	fs->private->files_found += files_found;
	fs->private->files_ignored += files_ignored;

	fs->private->total_directories_found += directories_found;
	fs->private->total_directories_ignored += directories_ignored;
	fs->private->total_files_found += files_found;
	fs->private->total_files_ignored += files_ignored;

	g_message ("%s crawling files after %2.2f seconds",
		   was_interrupted ? "Stoped" : "Finished",
		   g_timer_elapsed (fs->private->timer, NULL));
	g_message ("  Found %d directories, ignored %d directories",
		   directories_found,
		   directories_ignored);
	g_message ("  Found %d files, ignored %d files",
		   files_found,
		   files_ignored);

	directory_data_free (fs->private->current_directory);
	fs->private->current_directory = NULL;

	/* Proceed to next thing to process */
	crawl_directories_start (fs);
}

static gboolean
crawl_directories_cb (gpointer user_data)
{
	TrackerMinerFS *fs = user_data;
	gchar *path;
	gchar *str;

	if (fs->private->current_directory) {
		g_critical ("One directory is already being processed, bailing out");
		fs->private->crawl_directories_id = 0;
		return FALSE;
	}

	if (!fs->private->directories) {
		/* Now we handle the queue */
		item_queue_handlers_set_up (fs);
		crawl_directories_stop (fs);

		fs->private->crawl_directories_id = 0;
		return FALSE;
	}


	fs->private->current_directory = fs->private->directories->data;
	fs->private->directories = g_list_remove (fs->private->directories,
						  fs->private->current_directory);

	path = g_file_get_path (fs->private->current_directory->file);

	if (fs->private->current_directory->recurse) {
		str = g_strdup_printf (_("Crawling recursively directory '%s'"), path);
	} else {
		str = g_strdup_printf (_("Crawling single directory '%s'"), path);
	}

	g_message ("%s", str);

	g_object_set (fs, "status", str, NULL);
	g_free (str);
	g_free (path);

	if (tracker_crawler_start (fs->private->crawler,
				   fs->private->current_directory->file,
				   fs->private->current_directory->recurse)) {
		/* Crawler when restart the idle function when done */
		fs->private->crawl_directories_id = 0;
		return FALSE;
	}

	/* Directory couldn't be processed */
	directory_data_free (fs->private->current_directory);
	fs->private->current_directory = NULL;

	g_free (path);

	return TRUE;
}

static void
crawl_directories_start (TrackerMinerFS *fs)
{
	if (fs->private->crawl_directories_id != 0) {
		/* Processing ALREADY going on */
		return;
	}

	if (!fs->private->been_started) {
		/* Miner has not been started yet */
		return;
	}

	if (!fs->private->timer) {
		fs->private->timer = g_timer_new ();
	}

	fs->private->directories_found = 0;
	fs->private->directories_ignored = 0;
	fs->private->files_found = 0;
	fs->private->files_ignored = 0;

	fs->private->crawl_directories_id = g_idle_add (crawl_directories_cb, fs);
}

static void
crawl_directories_stop (TrackerMinerFS *fs)
{
	if (fs->private->crawl_directories_id == 0) {
		/* No processing going on, nothing to stop */
		return;
	}

	if (fs->private->current_directory) {
		tracker_crawler_stop (fs->private->crawler);
	}

	/* Is this the right time to emit FINISHED? What about
	 * monitor events left to handle? Should they matter
	 * here?
	 */
	if (fs->private->crawl_directories_id != 0) {
		g_source_remove (fs->private->crawl_directories_id);
		fs->private->crawl_directories_id = 0;
	}
}

void
tracker_miner_fs_add_directory (TrackerMinerFS *fs,
				GFile          *file,
				gboolean        recurse)
{
	g_return_if_fail (TRACKER_IS_MINER_FS (fs));
	g_return_if_fail (G_IS_FILE (file));

	fs->private->directories =
		g_list_append (fs->private->directories,
			       directory_data_new (file, recurse));

	crawl_directories_start (fs);
}

static void
check_files_removal (GQueue *queue,
		     GFile  *parent)
{
	GList *l;

	l = queue->head;

	while (l) {
		GFile *file = l->data;
		GList *link = l;

		l = l->next;

		if (g_file_equal (file, parent) ||
		    g_file_has_prefix (file, parent)) {
			g_queue_delete_link (queue, link);
			g_object_unref (file);
		}
	}
}

gboolean
tracker_miner_fs_remove_directory (TrackerMinerFS *fs,
				   GFile          *file)
{
	TrackerMinerFSPrivate *priv;
	gboolean return_val = FALSE;
	GList *dirs;

	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	priv = fs->private;

	if (fs->private->current_directory) {
		GFile *current_file;

		current_file = fs->private->current_directory->file;

		if (g_file_equal (file, current_file) ||
		    g_file_has_prefix (file, current_file)) {
			/* Dir is being processed currently, cancel crawler */
			tracker_crawler_stop (fs->private->crawler);
			return_val = TRUE;
		}
	}

	dirs = fs->private->directories;

	while (dirs) {
		DirectoryData *data = dirs->data;
		GList *link = dirs;

		dirs = dirs->next;

		if (g_file_equal (file, data->file) ||
		    g_file_has_prefix (file, data->file)) {
			directory_data_free (data);
			fs->private->directories = g_list_delete_link (fs->private->directories, link);
			return_val = TRUE;
		}
	}

	/* Remove anything contained in the removed directory
	 * from all relevant processing queues.
	 */
	check_files_removal (priv->items_updated, file);
	check_files_removal (priv->items_created, file);

	if (priv->current_file &&
	    priv->cancellable &&
	    (g_file_equal (priv->current_file, file) ||
	     g_file_has_prefix (priv->current_file, file))) {
		/* Cancel processing if currently processed file is
		 * inside the removed directory.
		 */
		g_cancellable_cancel (priv->cancellable);
	}

	return return_val;
}
