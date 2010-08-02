/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include "tracker-crawler.h"
#include "tracker-marshal.h"
#include "tracker-utils.h"

#define TRACKER_CRAWLER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_CRAWLER, TrackerCrawlerPrivate))

#define FILE_ATTRIBUTES	  \
	G_FILE_ATTRIBUTE_STANDARD_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_TYPE

#define FILES_QUEUE_PROCESS_INTERVAL 2000
#define FILES_QUEUE_PROCESS_MAX      5000

/* This is the number of files to be called back with from GIO at a
 * time so we don't get called back for every file.
 */
#define FILES_GROUP_SIZE             100

typedef struct DirectoryChildData DirectoryChildData;
typedef struct DirectoryProcessingData DirectoryProcessingData;
typedef struct DirectoryRootInfo DirectoryRootInfo;

struct DirectoryChildData {
	GFile          *child;
	gboolean        is_dir;
};

struct DirectoryProcessingData {
	GNode *node;
	GSList *children;
	guint was_inspected : 1;
	guint ignored_by_content : 1;
};

struct DirectoryRootInfo {
	GFile *directory;
	GNode *tree;
	guint recurse : 1;

	GQueue *directory_processing_queue;

	/* Directory stats */
	guint directories_found;
	guint directories_ignored;
	guint files_found;
	guint files_ignored;
};

struct TrackerCrawlerPrivate {
	/* Directories to crawl */
	GQueue         *directories;

	GCancellable   *cancellable;

	/* Idle handler for processing found data */
	guint           idle_id;

	gdouble         throttle;

	gboolean        recurse;

	/* Statistics */
	GTimer         *timer;

	/* Status */
	gboolean        is_running;
	gboolean        is_finished;
	gboolean        is_paused;
	gboolean        was_started;
};

enum {
	CHECK_DIRECTORY,
	CHECK_FILE,
	CHECK_DIRECTORY_CONTENTS,
	DIRECTORY_CRAWLED,
	FINISHED,
	LAST_SIGNAL
};

typedef struct {
	TrackerCrawler *crawler;
	DirectoryRootInfo  *root_info;
	DirectoryProcessingData *dir_info;
	GFile *dir_file;
} EnumeratorData;

static void     crawler_finalize        (GObject         *object);
static gboolean check_defaults          (TrackerCrawler  *crawler,
                                         GFile           *file);
static gboolean check_contents_defaults (TrackerCrawler  *crawler,
                                         GFile           *file,
                                         GList           *contents);
static void     file_enumerate_next     (GFileEnumerator *enumerator,
                                         EnumeratorData  *ed);
static void     file_enumerate_children  (TrackerCrawler          *crawler,
					  DirectoryRootInfo       *info,
					  DirectoryProcessingData *dir_data);

static void     directory_root_info_free (DirectoryRootInfo *info);


static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (TrackerCrawler, tracker_crawler, G_TYPE_OBJECT)

static void
tracker_crawler_class_init (TrackerCrawlerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerCrawlerClass *crawler_class = TRACKER_CRAWLER_CLASS (klass);

	object_class->finalize = crawler_finalize;

	crawler_class->check_directory = check_defaults;
	crawler_class->check_file      = check_defaults;
	crawler_class->check_directory_contents = check_contents_defaults;

	signals[CHECK_DIRECTORY] =
		g_signal_new ("check-directory",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerCrawlerClass, check_directory),
		              tracker_accumulator_check_file,
		              NULL,
		              tracker_marshal_BOOLEAN__OBJECT,
		              G_TYPE_BOOLEAN,
		              1,
		              G_TYPE_FILE);
	signals[CHECK_FILE] =
		g_signal_new ("check-file",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerCrawlerClass, check_file),
		              tracker_accumulator_check_file,
		              NULL,
		              tracker_marshal_BOOLEAN__OBJECT,
		              G_TYPE_BOOLEAN,
		              1,
		              G_TYPE_FILE);
	signals[CHECK_DIRECTORY_CONTENTS] =
		g_signal_new ("check-directory-contents",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerCrawlerClass, check_directory_contents),
		              tracker_accumulator_check_file,
		              NULL,
		              tracker_marshal_BOOLEAN__OBJECT_POINTER,
		              G_TYPE_BOOLEAN,
		              2, G_TYPE_FILE, G_TYPE_POINTER);
	signals[DIRECTORY_CRAWLED] =
		g_signal_new ("directory-crawled",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerCrawlerClass, directory_crawled),
		              NULL, NULL,
		              tracker_marshal_VOID__OBJECT_POINTER_UINT_UINT_UINT_UINT,
		              G_TYPE_NONE,
		              6,
			      G_TYPE_FILE,
		              G_TYPE_POINTER,
		              G_TYPE_UINT,
		              G_TYPE_UINT,
		              G_TYPE_UINT,
		              G_TYPE_UINT);
	signals[FINISHED] =
		g_signal_new ("finished",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerCrawlerClass, finished),
		              NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
		              G_TYPE_NONE,
		              1, G_TYPE_BOOLEAN);

	g_type_class_add_private (object_class, sizeof (TrackerCrawlerPrivate));
}

static void
tracker_crawler_init (TrackerCrawler *object)
{
	TrackerCrawlerPrivate *priv;

	object->private = TRACKER_CRAWLER_GET_PRIVATE (object);

	priv = object->private;

	priv->directories = g_queue_new ();
	priv->cancellable = g_cancellable_new ();
}

static void
crawler_finalize (GObject *object)
{
	TrackerCrawlerPrivate *priv;

	priv = TRACKER_CRAWLER_GET_PRIVATE (object);

	if (priv->timer) {
		g_timer_destroy (priv->timer);
	}

	if (priv->idle_id) {
		g_source_remove (priv->idle_id);
	}

	g_object_unref (priv->cancellable);

	g_queue_foreach (priv->directories, (GFunc) directory_root_info_free, NULL);
	g_queue_free (priv->directories);

	G_OBJECT_CLASS (tracker_crawler_parent_class)->finalize (object);
}

static gboolean
check_defaults (TrackerCrawler *crawler,
                GFile          *file)
{
	return TRUE;
}

static gboolean
check_contents_defaults (TrackerCrawler  *crawler,
                         GFile           *file,
                         GList           *contents)
{
	return TRUE;
}

TrackerCrawler *
tracker_crawler_new (void)
{
	TrackerCrawler *crawler;

	crawler = g_object_new (TRACKER_TYPE_CRAWLER, NULL);

	return crawler;
}

static gboolean
check_file (TrackerCrawler    *crawler,
	    DirectoryRootInfo *info,
            GFile             *file)
{
	gboolean use = FALSE;
	TrackerCrawlerPrivate *priv;

	priv = TRACKER_CRAWLER_GET_PRIVATE (crawler);

	g_signal_emit (crawler, signals[CHECK_FILE], 0, file, &use);

	/* Crawler may have been stopped while waiting for the 'use' value,
	 * and the DirectoryRootInfo already disposed... */
	if (!priv->is_running) {
		return FALSE;
	}

	info->files_found++;

	if (!use) {
		info->files_ignored++;
	}

	return use;
}

static gboolean
check_directory (TrackerCrawler    *crawler,
		 DirectoryRootInfo *info,
		 GFile             *file)
{
	gboolean use = FALSE;
	TrackerCrawlerPrivate *priv;

	priv = TRACKER_CRAWLER_GET_PRIVATE (crawler);

	g_signal_emit (crawler, signals[CHECK_DIRECTORY], 0, file, &use);

	/* Crawler may have been stopped while waiting for the 'use' value,
	 * and the DirectoryRootInfo already disposed... */
	if (!priv->is_running) {
		return FALSE;
	}

	info->directories_found++;

	if (!use) {
		info->directories_ignored++;
	}

	return use;
}

static DirectoryChildData *
directory_child_data_new (GFile    *child,
			  gboolean  is_dir)
{
	DirectoryChildData *child_data;

	child_data = g_slice_new (DirectoryChildData);
	child_data->child = g_object_ref (child);
	child_data->is_dir = is_dir;

	return child_data;
}

static void
directory_child_data_free (DirectoryChildData *child_data)
{
	g_object_unref (child_data->child);
	g_slice_free (DirectoryChildData, child_data);
}

static DirectoryProcessingData *
directory_processing_data_new (GNode *node)
{
	DirectoryProcessingData *data;

	data = g_slice_new0 (DirectoryProcessingData);
	data->node = node;

	return data;
}

static void
directory_processing_data_free (DirectoryProcessingData *data)
{
	g_slist_foreach (data->children, (GFunc) directory_child_data_free, NULL);
	g_slist_free (data->children);

	g_slice_free (DirectoryProcessingData, data);
}

static void
directory_processing_data_add_child (DirectoryProcessingData *data,
				     GFile                   *child,
				     gboolean                 is_dir)
{
	DirectoryChildData *child_data;

	child_data = directory_child_data_new (child, is_dir);
	data->children = g_slist_prepend (data->children, child_data);
}

static DirectoryRootInfo *
directory_root_info_new (GFile    *file,
			 gboolean  recurse)
{
	DirectoryRootInfo *info;
	DirectoryProcessingData *dir_info;

	info = g_slice_new0 (DirectoryRootInfo);

	info->directory = g_object_ref (file);
	info->recurse = recurse;
	info->directory_processing_queue = g_queue_new ();

	info->tree = g_node_new (g_object_ref (file));

	/* Fill in the processing info for the root node */
	dir_info = directory_processing_data_new (info->tree);
	g_queue_push_tail (info->directory_processing_queue, dir_info);

	return info;
}

static gboolean
directory_tree_free_foreach (GNode    *node,
			     gpointer  user_data)
{
	g_object_unref (node->data);
	return FALSE;
}

static void
directory_root_info_free (DirectoryRootInfo *info)
{
	g_object_unref (info->directory);

	g_node_traverse (info->tree,
			 G_PRE_ORDER,
			 G_TRAVERSE_ALL,
			 -1,
			 directory_tree_free_foreach,
			 NULL);
	g_node_destroy (info->tree);

	g_queue_foreach (info->directory_processing_queue,
			 (GFunc) directory_processing_data_free,
			 NULL);
	g_queue_free (info->directory_processing_queue);

	g_slice_free (DirectoryRootInfo, info);
}

static gboolean
process_func (gpointer data)
{
	TrackerCrawler          *crawler;
	TrackerCrawlerPrivate   *priv;
	DirectoryRootInfo       *info;
	DirectoryProcessingData *dir_data = NULL;
	gboolean                 stop_idle = FALSE;

	crawler = TRACKER_CRAWLER (data);
	priv = crawler->private;

	if (priv->is_paused) {
		/* Stop the idle func for now until we are unpaused */
		priv->idle_id = 0;

		return FALSE;
	}

	info = g_queue_peek_head (priv->directories);

	if (info) {
		dir_data = g_queue_peek_head (info->directory_processing_queue);
	}

	if (dir_data) {
		/* One directory inside the tree hierarchy is being inspected */
		if (!dir_data->was_inspected) {
			gboolean iterate;

			if (G_NODE_IS_ROOT (dir_data->node)) {
				iterate = check_directory (crawler, info, dir_data->node->data);
			} else {
				/* Directory has been already checked in the block below, so
				 * so obey the settings for the current directory root.
				 */
				iterate = info->recurse;
			}

			dir_data->was_inspected = TRUE;

			/* Crawler may have been already stopped while we were waiting for the
			 *  check_directory return value, and thus we should check if it's
			 *  running before going on with the iteration */
			if (priv->is_running && iterate) {
				/* Directory contents haven't been inspected yet,
				 * stop this idle function while it's being iterated
				 */
				file_enumerate_children (crawler, info, dir_data);
				stop_idle = TRUE;
			}
		} else if (dir_data->was_inspected &&
			   !dir_data->ignored_by_content &&
			   dir_data->children != NULL) {
			DirectoryChildData *child_data;
			GNode *child_node = NULL;

			/* Directory has been already inspected, take children
			 * one by one and check whether they should be incorporated
			 * to the tree.
			 */
			child_data = dir_data->children->data;
			dir_data->children = g_slist_remove (dir_data->children, child_data);

			if (((child_data->is_dir &&
			      check_directory (crawler, info, child_data->child)) ||
			     (!child_data->is_dir &&
			      check_file (crawler, info, child_data->child))) &&
			    /* Crawler may have been already stopped while we were waiting for the
			     *	check_directory or check_file return value, and thus we should
			     *	 check if it's running before going on */
			    priv->is_running) {
				child_node = g_node_prepend_data (dir_data->node,
								  g_object_ref (child_data->child));
			}

			if (priv->is_running && child_node && child_data->is_dir) {
				DirectoryProcessingData *child_dir_data;

				child_dir_data = directory_processing_data_new (child_node);
				g_queue_push_tail (info->directory_processing_queue, child_dir_data);
			}

			directory_child_data_free (child_data);
		} else {
			/* No (more) children, or directory ignored. stop processing. */
			g_queue_pop_head (info->directory_processing_queue);
			directory_processing_data_free (dir_data);
		}
	} else if (!dir_data && info) {
		/* Current directory being crawled doesn't have anything else
		 * to process, emit ::directory-crawled and free data.
		 */
		g_signal_emit (crawler, signals[DIRECTORY_CRAWLED], 0,
			       info->directory,
			       info->tree,
			       info->directories_found,
			       info->directories_ignored,
			       info->files_found,
			       info->files_ignored);

		g_queue_pop_head (priv->directories);
		directory_root_info_free (info);
	}

	if (!g_queue_peek_head (priv->directories)) {
		/* There's nothing else to process */
		priv->is_finished = TRUE;
		tracker_crawler_stop (crawler);
		stop_idle = TRUE;
	}

	if (stop_idle) {
		priv->idle_id = 0;
		return FALSE;
	}

	return TRUE;
}

static gboolean
process_func_start (TrackerCrawler *crawler)
{
	if (crawler->private->is_paused) {
		return FALSE;
	}

	if (crawler->private->is_finished) {
		return FALSE;
	}

	if (crawler->private->idle_id == 0) {
		crawler->private->idle_id = g_idle_add (process_func, crawler);
	}

	return TRUE;
}

static void
process_func_stop (TrackerCrawler *crawler)
{
	if (crawler->private->idle_id != 0) {
		g_source_remove (crawler->private->idle_id);
		crawler->private->idle_id = 0;
	}
}

static EnumeratorData *
enumerator_data_new (TrackerCrawler          *crawler,
		     DirectoryRootInfo       *root_info,
		     DirectoryProcessingData *dir_info)
{
	EnumeratorData *ed;

	ed = g_slice_new (EnumeratorData);

	ed->crawler = g_object_ref (crawler);
	ed->root_info = root_info;
	ed->dir_info = dir_info;
	/* Make sure there's always a ref of the GFile while we're
	 * iterating it */
	ed->dir_file = g_object_ref (G_FILE (dir_info->node->data));

	return ed;
}

static void
enumerator_data_process (EnumeratorData *ed)
{
	TrackerCrawler *crawler;
	GSList *l;
	GList *children = NULL;
	gboolean use;

	crawler = ed->crawler;

	for (l = ed->dir_info->children; l; l = l->next) {
		DirectoryChildData *child_data;

		child_data = l->data;
		children = g_list_prepend (children, child_data->child);
	}

	g_signal_emit (crawler, signals[CHECK_DIRECTORY_CONTENTS], 0, ed->dir_info->node->data, children, &use);
	g_list_free (children);

	if (!use) {
		ed->dir_info->ignored_by_content = TRUE;
		/* FIXME: Update stats */
		return;
	}
}

static void
enumerator_data_free (EnumeratorData *ed)
{
	g_object_unref (ed->dir_file);
	g_object_unref (ed->crawler);
	g_slice_free (EnumeratorData, ed);
}

static void
file_enumerator_close_cb (GObject      *enumerator,
                          GAsyncResult *result,
                          gpointer      user_data)
{
	TrackerCrawler *crawler;
	GError *error = NULL;

	crawler = TRACKER_CRAWLER (user_data);

	if (!g_file_enumerator_close_finish (G_FILE_ENUMERATOR (enumerator),
	                                     result,
	                                     &error)) {
		g_warning ("Couldn't close GFileEnumerator (%p): %s", enumerator,
		           (error) ? error->message : "No reason");

		g_clear_error (&error);
	}

	/* Processing of directory is now finished,
	 * continue with queued files/directories.
	 */
	process_func_start (crawler);
}

static void
file_enumerate_next_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
	TrackerCrawler *crawler;
	EnumeratorData *ed;
	GFileEnumerator *enumerator;
	GFile *parent, *child;
	GFileInfo *info;
	GList *files, *l;
	GError *error = NULL;
	gboolean cancelled;

	enumerator = G_FILE_ENUMERATOR (object);

	ed = user_data;
	crawler = ed->crawler;
	cancelled = g_cancellable_is_cancelled (crawler->private->cancellable);

	files = g_file_enumerator_next_files_finish (enumerator,
	                                             result,
	                                             &error);

	if (error || !files || !crawler->private->is_running) {
		if (error && !cancelled) {
			g_critical ("Could not crawl through directory: %s", error->message);
			g_error_free (error);
		}

		/* No more files or we are stopping anyway, so clean
		 * up and close all file enumerators.
		 */
		if (files) {
			g_list_foreach (files, (GFunc) g_object_unref, NULL);
			g_list_free (files);
		}

		if (!cancelled) {
			enumerator_data_process (ed);
		}

		enumerator_data_free (ed);
		g_file_enumerator_close_async (enumerator,
		                               G_PRIORITY_DEFAULT,
		                               NULL,
		                               file_enumerator_close_cb,
		                               crawler);
		g_object_unref (enumerator);

		return;
	}

	parent = ed->dir_info->node->data;

	for (l = files; l; l = l->next) {
		const gchar *child_name;
		gboolean is_dir;

		info = l->data;

		child_name = g_file_info_get_name (info);
		child = g_file_get_child (parent, child_name);
		is_dir = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;

		directory_processing_data_add_child (ed->dir_info, child, is_dir);

		g_object_unref (child);
		g_object_unref (info);
	}

	g_list_free (files);

	/* Get next files */
	file_enumerate_next (enumerator, ed);
}

static void
file_enumerate_next (GFileEnumerator *enumerator,
                     EnumeratorData  *ed)
{
	g_file_enumerator_next_files_async (enumerator,
	                                    FILES_GROUP_SIZE,
	                                    G_PRIORITY_DEFAULT,
	                                    ed->crawler->private->cancellable,
	                                    file_enumerate_next_cb,
	                                    ed);
}

static void
file_enumerate_children_cb (GObject      *file,
                            GAsyncResult *result,
                            gpointer      user_data)
{
	TrackerCrawler *crawler;
	EnumeratorData *ed;
	GFileEnumerator *enumerator;
	GFile *parent;
	GError *error = NULL;
	gboolean cancelled;

	parent = G_FILE (file);
	ed = (EnumeratorData*) user_data;
	crawler = ed->crawler;
	cancelled = g_cancellable_is_cancelled (crawler->private->cancellable);
	enumerator = g_file_enumerate_children_finish (parent, result, &error);

	if (!enumerator) {
		if (error && !cancelled) {
			gchar *path;

			path = g_file_get_path (parent);

			g_warning ("Could not open directory '%s': %s",
			           path, error->message);

			g_error_free (error);
			g_free (path);
		}

		enumerator_data_free (ed);
		process_func_start (crawler);
		return;
	}

	/* Start traversing the directory's files */
	file_enumerate_next (enumerator, ed);
}

static void
file_enumerate_children (TrackerCrawler          *crawler,
			 DirectoryRootInfo       *info,
			 DirectoryProcessingData *dir_data)
{
	EnumeratorData *ed;

	ed = enumerator_data_new (crawler, info, dir_data);

	g_file_enumerate_children_async (ed->dir_file,
	                                 FILE_ATTRIBUTES,
	                                 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                                 G_PRIORITY_LOW,
	                                 crawler->private->cancellable,
	                                 file_enumerate_children_cb,
	                                 ed);
}

gboolean
tracker_crawler_start (TrackerCrawler *crawler,
                       GFile          *file,
                       gboolean        recurse)
{
	TrackerCrawlerPrivate *priv;
	DirectoryRootInfo *info;

	g_return_val_if_fail (TRACKER_IS_CRAWLER (crawler), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	priv = crawler->private;

	if (!g_file_query_exists (file, NULL)) {
		return FALSE;
	}

	if (g_cancellable_is_cancelled (priv->cancellable)) {
		g_cancellable_reset (priv->cancellable);
	}

	priv->was_started = TRUE;
	priv->recurse = recurse;

	/* Time the event */
	if (priv->timer) {
		g_timer_destroy (priv->timer);
	}

	priv->timer = g_timer_new ();

	if (priv->is_paused) {
		g_timer_stop (priv->timer);
	}

	/* Set as running now */
	priv->is_running = TRUE;
	priv->is_finished = FALSE;

	info = directory_root_info_new (file, recurse);
	g_queue_push_tail (priv->directories, info);

	process_func_start (crawler);

	return TRUE;
}

void
tracker_crawler_stop (TrackerCrawler *crawler)
{
	TrackerCrawlerPrivate *priv;

	g_return_if_fail (TRACKER_IS_CRAWLER (crawler));

	priv = crawler->private;

	/* If already not running, just ignore */
	if (!priv->is_running) {
		return;
	}

	priv->is_running = FALSE;
	g_cancellable_cancel (priv->cancellable);

	process_func_stop (crawler);

	if (priv->timer) {
		g_timer_destroy (priv->timer);
		priv->timer = NULL;
	}

	g_signal_emit (crawler, signals[FINISHED], 0,
	               !priv->is_finished);

	/* Clean up queue */
	g_queue_foreach (priv->directories, (GFunc) directory_root_info_free, NULL);
	g_queue_clear (priv->directories);

	/* We don't free the queue in case the crawler is reused, it
	 * is only freed in finalize.
	 */
}

void
tracker_crawler_pause (TrackerCrawler *crawler)
{
	g_return_if_fail (TRACKER_IS_CRAWLER (crawler));

	crawler->private->is_paused = TRUE;

	if (crawler->private->is_running) {
		g_timer_stop (crawler->private->timer);
		process_func_stop (crawler);
	}

	g_message ("Crawler is paused, %s",
	           crawler->private->is_running ? "currently running" : "not running");
}

void
tracker_crawler_resume (TrackerCrawler *crawler)
{
	g_return_if_fail (TRACKER_IS_CRAWLER (crawler));

	crawler->private->is_paused = FALSE;

	if (crawler->private->is_running) {
		g_timer_continue (crawler->private->timer);
		process_func_start (crawler);
	}

	g_message ("Crawler is resuming, %s",
	           crawler->private->is_running ? "currently running" : "not running");
}

void
tracker_crawler_set_throttle (TrackerCrawler *crawler,
                              gdouble         throttle)
{
	g_return_if_fail (TRACKER_IS_CRAWLER (crawler));

	throttle = CLAMP (throttle, 0, 1);
	crawler->private->throttle = throttle;

	/* Update timeouts */
	if (crawler->private->idle_id != 0) {
		guint interval, idle_id;

		interval = MAX_TIMEOUT_INTERVAL * crawler->private->throttle;

		g_source_remove (crawler->private->idle_id);

		if (interval == 0) {
			idle_id = g_idle_add (process_func, crawler);
		} else {
			idle_id = g_timeout_add (interval, process_func, crawler);
		}

		crawler->private->idle_id = idle_id;
	}
}
