/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
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
 *
 * Author: Carlos Garnacho  <carlos@lanedo.com>
 */

#include "config.h"

#include <libtracker-common/tracker-common.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-file-notifier.h"
#include "tracker-file-system.h"
#include "tracker-crawler.h"
#include "tracker-monitor.h"

static GQuark quark_property_iri = 0;
static GQuark quark_property_store_mtime = 0;
static GQuark quark_property_filesystem_mtime = 0;
static gboolean force_check_updated = FALSE;

#define MAX_DEPTH 1

enum {
	PROP_0,
	PROP_INDEXING_TREE,
	PROP_DATA_PROVIDER
};

enum {
	FILE_CREATED,
	FILE_UPDATED,
	FILE_DELETED,
	FILE_MOVED,
	DIRECTORY_STARTED,
	DIRECTORY_FINISHED,
	FINISHED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
	GFile *root;
	GFile *current_dir;
	GQueue *pending_dirs;
	GPtrArray *query_files;
	guint flags;
	guint directories_found;
	guint directories_ignored;
	guint files_found;
	guint files_ignored;
	guint current_dir_content_filtered : 1;
} RootData;

typedef struct {
	TrackerIndexingTree *indexing_tree;
	TrackerFileSystem *file_system;

	TrackerSparqlConnection *connection;
	GCancellable *cancellable;

	TrackerCrawler *crawler;
	TrackerMonitor *monitor;
	TrackerDataProvider *data_provider;

	GTimer *timer;

	/* List of pending directory
	 * trees to get data from
	 */
	GList *pending_index_roots;
	RootData *current_index_root;

	guint stopped : 1;
} TrackerFileNotifierPrivate;

typedef struct {
	TrackerFileNotifier *notifier;
	GNode *cur_parent_node;

	/* Canonical copy from priv->file_system */
	GFile *cur_parent;
} DirectoryCrawledData;

typedef struct {
	TrackerFileNotifier *notifier;
	gint max_depth;
} SparqlStartData;

static gboolean crawl_directories_start (TrackerFileNotifier *notifier);
static void     sparql_files_query_start (TrackerFileNotifier  *notifier,
                                          GFile               **files,
                                          guint                 n_files,
                                          gint                  max_depth);

G_DEFINE_TYPE (TrackerFileNotifier, tracker_file_notifier, G_TYPE_OBJECT)

static void
tracker_file_notifier_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	TrackerFileNotifierPrivate *priv;

	priv = TRACKER_FILE_NOTIFIER (object)->priv;

	switch (prop_id) {
	case PROP_INDEXING_TREE:
		priv->indexing_tree = g_value_dup_object (value);
		tracker_monitor_set_indexing_tree (priv->monitor,
		                                   priv->indexing_tree);
		break;
	case PROP_DATA_PROVIDER:
		priv->data_provider = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_file_notifier_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	TrackerFileNotifierPrivate *priv;

	priv = TRACKER_FILE_NOTIFIER (object)->priv;

	switch (prop_id) {
	case PROP_INDEXING_TREE:
		g_value_set_object (value, priv->indexing_tree);
		break;
	case PROP_DATA_PROVIDER:
		g_value_set_object (value, priv->data_provider);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static RootData *
root_data_new (TrackerFileNotifier *notifier,
               GFile               *file,
               guint                flags)
{
	RootData *data;

	data = g_new0 (RootData, 1);
	data->root = g_object_ref (file);
	data->pending_dirs = g_queue_new ();
	data->query_files = g_ptr_array_new_with_free_func (g_object_unref);
	data->flags = flags;

	g_queue_push_tail (data->pending_dirs, g_object_ref (file));

	return data;
}

static void
root_data_free (RootData *data)
{
	g_queue_free_full (data->pending_dirs, (GDestroyNotify) g_object_unref);
	g_ptr_array_unref (data->query_files);
	if (data->current_dir) {
		g_object_unref (data->current_dir);
	}
	g_object_unref (data->root);
	g_free (data);
}

/* Crawler signal handlers */
static gboolean
crawler_check_file_cb (TrackerCrawler *crawler,
                       GFile          *file,
                       gpointer        user_data)
{
	TrackerFileNotifierPrivate *priv;

	priv = TRACKER_FILE_NOTIFIER (user_data)->priv;

	return tracker_indexing_tree_file_is_indexable (priv->indexing_tree,
	                                                file,
	                                                G_FILE_TYPE_REGULAR);
}

static gboolean
crawler_check_directory_cb (TrackerCrawler *crawler,
                            GFile          *directory,
                            gpointer        user_data)
{
	TrackerFileNotifierPrivate *priv;
	GFile *root, *canonical;

	priv = TRACKER_FILE_NOTIFIER (user_data)->priv;
	g_assert (priv->current_index_root != NULL);

	canonical = tracker_file_system_peek_file (priv->file_system, directory);
	root = tracker_indexing_tree_get_root (priv->indexing_tree, directory, NULL);

	/* If it's a config root itself, other than the one
	 * currently processed, bypass it, it will be processed
	 * when the time arrives.
	 */
	if (canonical && root == canonical &&
	    root != priv->current_index_root->root) {
		return FALSE;
	}

	return tracker_indexing_tree_file_is_indexable (priv->indexing_tree,
	                                                directory,
	                                                G_FILE_TYPE_DIRECTORY);
}

static gboolean
crawler_check_directory_contents_cb (TrackerCrawler *crawler,
                                     GFile          *parent,
                                     GList          *children,
                                     gpointer        user_data)
{
	TrackerFileNotifierPrivate *priv;
	gboolean process;

	priv = TRACKER_FILE_NOTIFIER (user_data)->priv;
	process = tracker_indexing_tree_parent_is_indexable (priv->indexing_tree,
	                                                     parent, children);
	if (process) {
		TrackerDirectoryFlags parent_flags;
		GFile *canonical;
		gboolean add_monitor;

		canonical = tracker_file_system_get_file (priv->file_system,
		                                          parent,
		                                          G_FILE_TYPE_DIRECTORY,
		                                          NULL);
		tracker_indexing_tree_get_root (priv->indexing_tree,
		                                canonical, &parent_flags);

		add_monitor = (parent_flags & TRACKER_DIRECTORY_FLAG_MONITOR) != 0;

		if (add_monitor) {
			tracker_monitor_add (priv->monitor, canonical);
		} else {
			tracker_monitor_remove (priv->monitor, canonical);
		}
	} else {
		priv->current_index_root->current_dir_content_filtered = TRUE;
	}

	return process;
}

static gboolean
file_notifier_traverse_tree_foreach (GFile    *file,
                                     gpointer  user_data)
{
	TrackerFileNotifier *notifier;
	TrackerFileNotifierPrivate *priv;
	guint64 *store_mtime, *disk_mtime;
	GFile *current_root;

	notifier = user_data;
	priv = notifier->priv;
	current_root = priv->current_index_root->current_dir;

	/* If we're crawling over a subdirectory of a root index, it's been
	 * already notified in the crawling op that made it processed, so avoid
	 * it here again.
	 */
	if (current_root == file &&
	    current_root != priv->current_index_root->root) {
		tracker_file_system_unset_property (priv->file_system, file,
		                                    quark_property_filesystem_mtime);
		tracker_file_system_unset_property (priv->file_system, file,
		                                    quark_property_store_mtime);
		return FALSE;
	}

	store_mtime = tracker_file_system_steal_property (priv->file_system, file,
	                                                  quark_property_store_mtime);
	disk_mtime = tracker_file_system_steal_property (priv->file_system, file,
	                                                 quark_property_filesystem_mtime);

	if (store_mtime && !disk_mtime) {
		/* In store but not in disk, delete */
		g_signal_emit (notifier, signals[FILE_DELETED], 0, file);

		g_free (store_mtime);
		g_free (disk_mtime);
		return TRUE;
	} else if (disk_mtime && !store_mtime) {
		/* In disk but not in store, create */
		g_signal_emit (notifier, signals[FILE_CREATED], 0, file);
	} else if (store_mtime && disk_mtime && *disk_mtime != *store_mtime) {
		/* Mtime changed, update */
		g_signal_emit (notifier, signals[FILE_UPDATED], 0, file, FALSE);
	} else if (!store_mtime && !disk_mtime) {
		/* what are we doing with such file? should happen rarely,
		 * only with files that we've queried, but we decided not
		 * to crawl (i.e. embedded root directories, that would
		 * be processed when that root is being crawled).
		 */
		if (file != priv->current_index_root->root &&
		    !tracker_indexing_tree_file_is_root (priv->indexing_tree, file)) {
			gchar *uri;

			uri = g_file_get_uri (file);
			g_debug ("File '%s' has no disk nor store mtime",
			         uri);
			g_free (uri);
		}
	}

	g_free (store_mtime);
	g_free (disk_mtime);

	return FALSE;
}

static gboolean
notifier_check_next_root (TrackerFileNotifier *notifier)
{
	TrackerFileNotifierPrivate *priv;

	priv = notifier->priv;
	g_assert (priv->current_index_root == NULL);

	if (priv->pending_index_roots) {
		return crawl_directories_start (notifier);
	} else {
		g_signal_emit (notifier, signals[FINISHED], 0);
		return FALSE;
	}
}

static void
file_notifier_traverse_tree (TrackerFileNotifier *notifier, gint max_depth)
{
	TrackerFileNotifierPrivate *priv;
	GFile *config_root, *directory;
	TrackerDirectoryFlags flags;

	priv = notifier->priv;
	g_assert (priv->current_index_root != NULL);

	directory = priv->current_index_root->current_dir;
	config_root = tracker_indexing_tree_get_root (priv->indexing_tree,
						      directory, &flags);

	/* The max_depth parameter is usually '1', which would cause only the
	 * directory itself to be processed. We want the directory and its contents
	 * to be processed so we need to go to (max_depth + 1) here.
	 */

	if (config_root != directory ||
	    flags & TRACKER_DIRECTORY_FLAG_CHECK_MTIME) {
		tracker_file_system_traverse (priv->file_system,
		                              directory,
		                              G_LEVEL_ORDER,
		                              file_notifier_traverse_tree_foreach,
		                              max_depth + 1,
		                              notifier);
	}
}

static gboolean
file_notifier_is_directory_modified (TrackerFileNotifier *notifier,
                                     GFile               *file)
{
	TrackerFileNotifierPrivate *priv;
	guint64 *store_mtime, *disk_mtime;

	if (G_UNLIKELY (force_check_updated))
		return TRUE;

	priv = notifier->priv;
	store_mtime = tracker_file_system_get_property (priv->file_system, file,
	                                                quark_property_store_mtime);
	disk_mtime = tracker_file_system_get_property (priv->file_system, file,
	                                               quark_property_filesystem_mtime);

	return (store_mtime && disk_mtime && *disk_mtime != *store_mtime);
}

static gboolean
file_notifier_add_node_foreach (GNode    *node,
                                gpointer  user_data)
{
	DirectoryCrawledData *data = user_data;
	TrackerFileNotifierPrivate *priv;
	GFileInfo *file_info;
	GFile *canonical, *file;

	priv = data->notifier->priv;
	file = node->data;

	if (node->parent &&
	    node->parent != data->cur_parent_node) {
		data->cur_parent_node = node->parent;
		data->cur_parent = tracker_file_system_peek_file (priv->file_system,
		                                                  node->parent->data);
	} else {
		data->cur_parent_node = NULL;
		data->cur_parent = NULL;
	}

	file_info = tracker_crawler_get_file_info (priv->crawler, file);

	if (file_info) {
		GFileType file_type;
		guint64 time, *time_ptr;
		gint depth;

		file_type = g_file_info_get_file_type (file_info);
		depth = g_node_depth (node);

		/* Intern file in filesystem */
		canonical = tracker_file_system_get_file (priv->file_system,
							  file, file_type,
							  data->cur_parent);

		time = g_file_info_get_attribute_uint64 (file_info,
		                                         G_FILE_ATTRIBUTE_TIME_MODIFIED);

		time_ptr = g_new (guint64, 1);
		*time_ptr = time;

		tracker_file_system_set_property (priv->file_system, canonical,
		                                  quark_property_filesystem_mtime,
		                                  time_ptr);
		g_object_unref (file_info);

		if (file_type == G_FILE_TYPE_DIRECTORY && depth == MAX_DEPTH + 1) {
			/* If the max crawling depth is reached,
			 * queue dirs for later processing
			 */
			g_assert (node->children == NULL);
			g_queue_push_tail (priv->current_index_root->pending_dirs,
			                   g_object_ref (canonical));
		}

		if (file == priv->current_index_root->root ||
		    (depth != 0 &&
		     !tracker_indexing_tree_file_is_root (priv->indexing_tree, file))) {
			g_ptr_array_add (priv->current_index_root->query_files,
			                 g_object_ref (canonical));
		}
	}

	return FALSE;
}

static void
crawler_directory_crawled_cb (TrackerCrawler *crawler,
                              GFile          *directory,
                              GNode          *tree,
                              guint           directories_found,
                              guint           directories_ignored,
                              guint           files_found,
                              guint           files_ignored,
                              gpointer        user_data)
{
	TrackerFileNotifier *notifier;
	TrackerFileNotifierPrivate *priv;
	DirectoryCrawledData data = { 0 };

	notifier = data.notifier = user_data;
	priv = notifier->priv;

	g_node_traverse (tree,
	                 G_PRE_ORDER,
	                 G_TRAVERSE_ALL,
	                 -1,
	                 file_notifier_add_node_foreach,
	                 &data);

	priv->current_index_root->directories_found += directories_found;
	priv->current_index_root->directories_ignored += directories_ignored;
	priv->current_index_root->files_found += files_found;
	priv->current_index_root->files_ignored += files_ignored;
}

static GFile *
_insert_store_info (TrackerFileNotifier *notifier,
                    GFile               *file,
                    GFileType            file_type,
                    const gchar         *iri,
                    guint64              _time)
{
	TrackerFileNotifierPrivate *priv;
	GFile *canonical;

	priv = notifier->priv;
	canonical = tracker_file_system_get_file (priv->file_system,
	                                          file, file_type,
	                                          NULL);
	tracker_file_system_set_property (priv->file_system, canonical,
	                                  quark_property_iri,
	                                  g_strdup (iri));
	tracker_file_system_set_property (priv->file_system, canonical,
	                                  quark_property_store_mtime,
	                                  g_memdup (&_time, sizeof (guint64)));
	return canonical;
}

static void
sparql_files_query_populate (TrackerFileNotifier *notifier,
			     TrackerSparqlCursor *cursor,
			     gboolean             check_root)
{
	TrackerFileNotifierPrivate *priv;

	priv = notifier->priv;

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		GFile *file, *canonical, *root;
		const gchar *time_str, *iri;
		GError *error = NULL;
		guint64 _time;

		file = g_file_new_for_uri (tracker_sparql_cursor_get_string (cursor, 0, NULL));

		if (check_root) {
			/* If it's a config root itself, other than the one
			 * currently processed, bypass it, it will be processed
			 * when the time arrives.
			 */
			canonical = tracker_file_system_peek_file (priv->file_system, file);
			root = tracker_indexing_tree_get_root (priv->indexing_tree, file, NULL);

			if (canonical && root == file && priv->current_index_root &&
			    root != priv->current_index_root->root) {
				g_object_unref (file);
				continue;
			}
		}

		iri = tracker_sparql_cursor_get_string (cursor, 1, NULL);
		time_str = tracker_sparql_cursor_get_string (cursor, 2, NULL);
		_time = tracker_string_to_date (time_str, NULL, &error);

		if (error) {
			/* This should never happen. Assume that file was modified. */
			g_critical ("Getting store mtime: %s", error->message);
			g_clear_error (&error);
			_time = 0;
		}

		_insert_store_info (notifier, file,
		                    G_FILE_TYPE_UNKNOWN,
		                    iri, _time);
		g_object_unref (file);
	}
}

static void
sparql_contents_check_deleted (TrackerFileNotifier *notifier,
                               TrackerSparqlCursor *cursor)
{
	TrackerFileNotifierPrivate *priv;
	GFile *file, *canonical;
	const gchar *iri;

	priv = notifier->priv;

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		const gchar *uri;
		gboolean is_folder;
		GFileType file_type;

		/* Sometimes URI can be NULL when nie:url and
		 * nfo:belongsToContainer does not have a strictly 1:1
		 * relationship, e.g. data containers where there is
		 * only one nie:url but many nfo:belongsToContainer
		 * cases.
		 */
		uri = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		if (!uri) {
			continue;
		}

		file = g_file_new_for_uri (uri);
		iri = tracker_sparql_cursor_get_string (cursor, 1, NULL);
		is_folder = tracker_sparql_cursor_get_boolean (cursor, 3);
		file_type = is_folder ? G_FILE_TYPE_DIRECTORY : G_FILE_TYPE_UNKNOWN;
		canonical = tracker_file_system_peek_file (priv->file_system, file);

		if (!canonical) {
			/* The file exists on the store, but not on the
			 * crawled content, insert temporarily to handle
			 * the delete event.
			 */
			canonical = _insert_store_info (notifier, file,
							file_type,
			                                iri, 0);
			g_signal_emit (notifier, signals[FILE_DELETED], 0, canonical);
		} else if (priv->current_index_root->current_dir_content_filtered ||
		           !tracker_indexing_tree_file_is_indexable (priv->indexing_tree,
		                                                     canonical,
		                                                     file_type)) {
			/* File is there, but is not indexable anymore, remove too */
			g_signal_emit (notifier, signals[FILE_DELETED], 0, canonical);
		}

		g_object_unref (file);
	}
}

static gboolean
crawl_directory_in_current_root (TrackerFileNotifier *notifier)
{
	TrackerFileNotifierPrivate *priv = notifier->priv;
	gint depth;
	GFile *directory;

	if (!priv->current_index_root)
		return FALSE;

	directory = g_queue_pop_head (priv->current_index_root->pending_dirs);

	if (!directory)
		return FALSE;

	priv->current_index_root->current_dir = directory;

	if (priv->cancellable)
		g_object_unref (priv->cancellable);
	priv->cancellable = g_cancellable_new ();

	if ((priv->current_index_root->flags & TRACKER_DIRECTORY_FLAG_RECURSE) == 0) {
		/* Don't recurse */
		depth = 1;
	} else {
		/* Recurse */
		depth = MAX_DEPTH;
	}

	if (!tracker_crawler_start (priv->crawler,
	                            directory,
	                            priv->current_index_root->flags,
	                            depth)) {
		sparql_files_query_start (notifier, &directory, 1, depth);
	}

	return TRUE;
}

static void
finish_current_directory (TrackerFileNotifier *notifier,
                          gboolean             interrupted)
{
	TrackerFileNotifierPrivate *priv;
	GFile *directory;

	priv = notifier->priv;
	directory = priv->current_index_root->current_dir;
	priv->current_index_root->current_dir = NULL;
	priv->current_index_root->current_dir_content_filtered = FALSE;

	/* If crawling was interrupted, we take all collected info as invalid.
	 * Otherwise we dispose regular files here, only directories are
	 * cached once crawling has completed.
	 */
	tracker_file_system_forget_files (priv->file_system,
	                                  directory,
					  interrupted ?
					  G_FILE_TYPE_UNKNOWN :
	                                  G_FILE_TYPE_REGULAR);

	if (interrupted || !crawl_directory_in_current_root (notifier)) {
		/* No more directories left to be crawled in the current
		 * root, jump to the next one.
		 */
		g_signal_emit (notifier, signals[DIRECTORY_FINISHED], 0,
		               priv->current_index_root->root,
		               priv->current_index_root->directories_found,
		               priv->current_index_root->directories_ignored,
		               priv->current_index_root->files_found,
		               priv->current_index_root->files_ignored);

		g_info ("  Notified files after %2.2f seconds",
		        g_timer_elapsed (priv->timer, NULL));
		g_info ("  Found %d directories, ignored %d directories",
		        priv->current_index_root->directories_found,
		        priv->current_index_root->directories_ignored);
		g_info ("  Found %d files, ignored %d files",
		        priv->current_index_root->files_found,
		        priv->current_index_root->files_ignored);

		if (!interrupted) {
			root_data_free (priv->current_index_root);
			priv->current_index_root = NULL;

			notifier_check_next_root (notifier);
		}
	}

	g_object_unref (directory);
}

static gboolean
root_data_remove_directory (RootData *data,
			    GFile    *directory)
{
	GList *l = data->pending_dirs->head, *next;
	GFile *file;

	while (l) {
		file = l->data;
		next = l->next;

		if (g_file_equal (file, directory) ||
		    g_file_has_prefix (file, directory)) {
			g_queue_remove (data->pending_dirs, file);
			g_object_unref (file);
		}

		l = next;
	}

	return (g_file_equal (data->current_dir, directory) ||
		g_file_has_prefix (data->current_dir, directory));
}

static void
file_notifier_current_root_check_remove_directory (TrackerFileNotifier *notifier,
						   GFile               *file)
{
	TrackerFileNotifierPrivate *priv;

	priv = notifier->priv;

	if (priv->current_index_root &&
	    root_data_remove_directory (priv->current_index_root, file)) {
		g_cancellable_cancel (priv->cancellable);
		tracker_crawler_stop (priv->crawler);

		if (!crawl_directory_in_current_root (notifier)) {
			if (priv->current_index_root) {
				root_data_free (priv->current_index_root);
				priv->current_index_root = NULL;
			}

			notifier_check_next_root (notifier);
		}
	}
}

/* Query for directory contents, used to look for deleted contents in those */
static void
sparql_contents_query_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
	TrackerFileNotifier *notifier = TRACKER_FILE_NOTIFIER (user_data);
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                 result, &error);
	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_warning ("Could not query directory contents: %s\n", error->message);
			finish_current_directory (notifier, TRUE);
		}
		goto out;
	}

	if (cursor) {
		sparql_contents_check_deleted (notifier, cursor);
		g_object_unref (cursor);
	}

	finish_current_directory (notifier, FALSE);

out:
	if (error) {
	        g_error_free (error);
	}
}

static gchar *
sparql_contents_compose_query (GFile **directories,
                               guint   n_dirs)
{
	GString *str;
	gchar *uri;
	gint i;
	gboolean first = TRUE;

	str = g_string_new ("SELECT nie:url(?u) ?u nfo:fileLastModified(?u) "
	                    "       IF (nie:mimeType(?u) = \"inode/directory\", true, false) {"
			    " ?u nfo:belongsToContainer ?f . ?f nie:url ?url ."
			    " FILTER (?url IN (");
	for (i = 0; i < n_dirs; i++) {
		if (!first) {
			g_string_append_c (str, ',');
		}

		first = FALSE;
		uri = g_file_get_uri (directories[i]);
		g_string_append_printf (str, "\"%s\"", uri);
		g_free (uri);
	}

	g_string_append (str, "))}");

	return g_string_free (str, FALSE);
}

static void
sparql_contents_query_start (TrackerFileNotifier  *notifier,
                             GFile               **directories,
                             guint                 n_dirs)
{
	TrackerFileNotifierPrivate *priv;
	gchar *sparql;

	priv = notifier->priv;

	if (G_UNLIKELY (priv->connection == NULL)) {
		return;
	}

	sparql = sparql_contents_compose_query (directories, n_dirs);
	tracker_sparql_connection_query_async (priv->connection,
	                                       sparql,
	                                       priv->cancellable,
	                                       sparql_contents_query_cb,
	                                       notifier);
	g_free (sparql);
}

/* Query for file information, used on all elements found during crawling */
static void
sparql_files_query_cb (GObject      *object,
		       GAsyncResult *result,
		       gpointer      user_data)
{
	SparqlStartData *data = user_data;
	TrackerFileNotifierPrivate *priv;
	TrackerFileNotifier *notifier;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GFile *directory;
	guint flags;

	notifier = data->notifier;
	priv = notifier->priv;

	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                 result, &error);
	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_warning ("Could not query indexed files: %s\n", error->message);
			finish_current_directory (notifier, TRUE);
		}
		goto out;
	}

	if (cursor) {
		sparql_files_query_populate (notifier, cursor, TRUE);
		g_object_unref (cursor);
	}

	file_notifier_traverse_tree (notifier, data->max_depth);
	directory = priv->current_index_root->current_dir;
	flags = priv->current_index_root->flags;

	if ((flags & TRACKER_DIRECTORY_FLAG_CHECK_DELETED) != 0 ||
	    priv->current_index_root->current_dir_content_filtered ||
	    file_notifier_is_directory_modified (notifier, directory)) {
		/* The directory has updated its mtime, this means something
		 * was either added or removed in the mean time. Crawling
		 * will always find all newly added files. But still, we
		 * must check the contents in the store to handle contents
		 * having been deleted in the directory.
		 */
		sparql_contents_query_start (notifier, &directory, 1);
	} else {
		finish_current_directory (notifier, FALSE);
	}

out:
	if (error) {
	        g_error_free (error);
	}
	g_free (data);
}

static gchar *
sparql_files_compose_query (GFile **files,
			    guint   n_files)
{
	GString *str;
	gchar *uri;
	gint i = 0;

	str = g_string_new ("SELECT ?url ?u nfo:fileLastModified(?u) {"
			    "  ?u a rdfs:Resource ; nie:url ?url . "
			    "FILTER (?url IN (");
	for (i = 0; i < n_files; i++) {
		if (i != 0)
			g_string_append_c (str, ',');

		uri = g_file_get_uri (files[i]);
		g_string_append_printf (str, "\"%s\"", uri);
		g_free (uri);
	}

	g_string_append (str, "))}");

	return g_string_free (str, FALSE);
}

static void
sparql_files_query_start (TrackerFileNotifier  *notifier,
                          GFile               **files,
                          guint                 n_files,
                          gint                  max_depth)
{
	TrackerFileNotifierPrivate *priv;
	gchar *sparql;
	SparqlStartData *data = g_new (SparqlStartData, 1);

	data->notifier = notifier;
	data->max_depth = max_depth;

	priv = notifier->priv;

	if (G_UNLIKELY (priv->connection == NULL)) {
		return;
	}

	sparql = sparql_files_compose_query (files, n_files);
	tracker_sparql_connection_query_async (priv->connection,
	                                       sparql,
	                                       priv->cancellable,
	                                       sparql_files_query_cb,
	                                       data);
	g_free (sparql);
}

static gboolean
crawl_directories_start (TrackerFileNotifier *notifier)
{
	TrackerFileNotifierPrivate *priv = notifier->priv;
	TrackerDirectoryFlags flags;
	GFile *directory;

	if (priv->current_index_root) {
		return FALSE;
	}

	if (!priv->pending_index_roots) {
		return FALSE;
	}

	if (priv->stopped) {
		return FALSE;
	}

	while (priv->pending_index_roots) {
		priv->current_index_root = priv->pending_index_roots->data;
		priv->pending_index_roots = g_list_delete_link (priv->pending_index_roots,
		                                                priv->pending_index_roots);
		directory = priv->current_index_root->root;
		flags = priv->current_index_root->flags;

		if ((flags & TRACKER_DIRECTORY_FLAG_IGNORE) == 0 &&
		    crawl_directory_in_current_root (notifier)) {
			gchar *uri;

			uri = g_file_get_uri (directory);
			g_info ("Processing location: '%s'", uri);
			g_free (uri);

			g_timer_reset (priv->timer);
			g_signal_emit (notifier, signals[DIRECTORY_STARTED], 0, directory);

			return TRUE;
		} else {
			/* Emit both signals for consistency */
			g_signal_emit (notifier, signals[DIRECTORY_STARTED], 0, directory);

			if ((flags & TRACKER_DIRECTORY_FLAG_PRESERVE) == 0) {
				g_signal_emit (notifier, signals[FILE_DELETED], 0, directory);
			}

			g_signal_emit (notifier, signals[DIRECTORY_FINISHED], 0,
			               directory, 0, 0, 0, 0);
		}

		root_data_free (priv->current_index_root);
		priv->current_index_root = NULL;
	}

	g_signal_emit (notifier, signals[FINISHED], 0);

	return FALSE;
}

static void
crawler_finished_cb (TrackerCrawler *crawler,
                     gboolean        was_interrupted,
                     gpointer        user_data)
{
	TrackerFileNotifier *notifier = user_data;
	TrackerFileNotifierPrivate *priv = notifier->priv;
	GFile *directory;
	gint max_depth = -1;

	g_assert (priv->current_index_root != NULL);

	if (was_interrupted) {
		finish_current_directory (notifier, TRUE);
		return;
	}

	max_depth = tracker_crawler_get_max_depth (crawler);

	directory = priv->current_index_root->current_dir;

	if (priv->current_index_root->query_files->len > 0 &&
	    (directory == priv->current_index_root->root ||
	     tracker_file_system_get_property (priv->file_system,
	                                       directory, quark_property_iri))) {
		sparql_files_query_start (notifier,
                                  (GFile**) priv->current_index_root->query_files->pdata,
		                          priv->current_index_root->query_files->len, max_depth);
		g_ptr_array_set_size (priv->current_index_root->query_files, 0);
	} else {
		g_ptr_array_set_size (priv->current_index_root->query_files, 0);
		file_notifier_traverse_tree (notifier, max_depth);
		finish_current_directory (notifier, FALSE);
	}
}

static gint
find_directory_root (RootData *data,
                     GFile    *file)
{
	if (data->root == file)
		return 0;
	return -1;
}

static void
notifier_queue_file (TrackerFileNotifier   *notifier,
                     GFile                 *file,
                     TrackerDirectoryFlags  flags)
{
	TrackerFileNotifierPrivate *priv = notifier->priv;
	RootData *data = root_data_new (notifier, file, flags);

	if (priv->current_index_root &&
	    priv->current_index_root->root == file)
		return;

	if (g_list_find_custom (priv->pending_index_roots, file,
	                        (GCompareFunc) find_directory_root))
		return;

	if (flags & TRACKER_DIRECTORY_FLAG_PRIORITY) {
		priv->pending_index_roots = g_list_prepend (priv->pending_index_roots, data);
	} else {
		priv->pending_index_roots = g_list_append (priv->pending_index_roots, data);
	}
}

/* Monitor signal handlers */
static void
monitor_item_created_cb (TrackerMonitor *monitor,
                         GFile          *file,
                         gboolean        is_directory,
                         gpointer        user_data)
{
	TrackerFileNotifier *notifier = user_data;
	TrackerFileNotifierPrivate *priv = notifier->priv;
	GFileType file_type;
	GFile *canonical;

	file_type = (is_directory) ? G_FILE_TYPE_DIRECTORY : G_FILE_TYPE_REGULAR;

	if (!tracker_indexing_tree_file_is_indexable (priv->indexing_tree,
	                                              file, file_type)) {
		/* File should not be indexed */
		return ;
	}

	if (!is_directory) {
		gboolean indexable;
		GList *children;
		GFile *parent;

		parent = g_file_get_parent (file);

		if (parent) {
			children = g_list_prepend (NULL, file);
			indexable = tracker_indexing_tree_parent_is_indexable (priv->indexing_tree,
			                                                       parent,
			                                                       children);
			g_list_free (children);

			if (!indexable) {
				/* New file triggered a directory content
				 * filter, remove parent directory altogether
				 */
				g_signal_emit (notifier, signals[FILE_DELETED], 0, parent);
				file_notifier_current_root_check_remove_directory (notifier, parent);
				g_object_unref (parent);
				return;
			}

			g_object_unref (parent);
		}
	} else {
		TrackerDirectoryFlags flags;

		/* If config for the directory is recursive,
		 * Crawl new entire directory and add monitors
		 */
		tracker_indexing_tree_get_root (priv->indexing_tree,
		                                file, &flags);

		if (flags & TRACKER_DIRECTORY_FLAG_RECURSE) {
			canonical = tracker_file_system_get_file (priv->file_system,
			                                          file,
			                                          file_type,
			                                          NULL);
			notifier_queue_file (notifier, canonical, flags);
			crawl_directories_start (notifier);
			return;
		}
	}

	/* Fetch the interned copy */
	canonical = tracker_file_system_get_file (priv->file_system,
	                                          file, file_type, NULL);

	g_signal_emit (notifier, signals[FILE_CREATED], 0, canonical);

	if (!is_directory) {
		tracker_file_system_forget_files (priv->file_system, canonical,
		                                  G_FILE_TYPE_REGULAR);
	}
}

static void
monitor_item_updated_cb (TrackerMonitor *monitor,
                         GFile          *file,
                         gboolean        is_directory,
                         gpointer        user_data)
{
	TrackerFileNotifier *notifier = user_data;
	TrackerFileNotifierPrivate *priv = notifier->priv;
	GFileType file_type;
	GFile *canonical;

	file_type = (is_directory) ? G_FILE_TYPE_DIRECTORY : G_FILE_TYPE_REGULAR;

	if (!tracker_indexing_tree_file_is_indexable (priv->indexing_tree,
	                                              file, file_type)) {
		/* File should not be indexed */
		return;
	}

	/* Fetch the interned copy */
	canonical = tracker_file_system_get_file (priv->file_system,
	                                          file, file_type, NULL);
	g_signal_emit (notifier, signals[FILE_UPDATED], 0, canonical, FALSE);

	if (!is_directory) {
		tracker_file_system_forget_files (priv->file_system, canonical,
		                                  G_FILE_TYPE_REGULAR);
	}
}

static void
monitor_item_attribute_updated_cb (TrackerMonitor *monitor,
                                   GFile          *file,
                                   gboolean        is_directory,
                                   gpointer        user_data)
{
	TrackerFileNotifier *notifier = user_data;
	TrackerFileNotifierPrivate *priv = notifier->priv;
	GFile *canonical;
	GFileType file_type;

	file_type = (is_directory) ? G_FILE_TYPE_DIRECTORY : G_FILE_TYPE_REGULAR;

	if (!tracker_indexing_tree_file_is_indexable (priv->indexing_tree,
	                                              file, file_type)) {
		/* File should not be indexed */
		return;
	}

	/* Fetch the interned copy */
	canonical = tracker_file_system_get_file (priv->file_system,
	                                          file, file_type, NULL);
	g_signal_emit (notifier, signals[FILE_UPDATED], 0, canonical, TRUE);

	if (!is_directory) {
		tracker_file_system_forget_files (priv->file_system, canonical,
		                                  G_FILE_TYPE_REGULAR);
	}
}

static void
monitor_item_deleted_cb (TrackerMonitor *monitor,
                         GFile          *file,
                         gboolean        is_directory,
                         gpointer        user_data)
{
	TrackerFileNotifier *notifier = user_data;
	TrackerFileNotifierPrivate *priv = notifier->priv;
	GFile *canonical;
	GFileType file_type;

	file_type = (is_directory) ? G_FILE_TYPE_DIRECTORY : G_FILE_TYPE_REGULAR;

	/* Remove monitors if any */
	if (is_directory &&
	    tracker_indexing_tree_file_is_root (priv->indexing_tree, file)) {
		tracker_monitor_remove_children_recursively (priv->monitor,
		                                             file);
	} else if (is_directory) {
		tracker_monitor_remove_recursively (priv->monitor, file);
	}

	if (!tracker_indexing_tree_file_is_indexable (priv->indexing_tree,
	                                              file, file_type)) {
		/* File was not indexed */
		return ;
	}

	if (!is_directory) {
		TrackerDirectoryFlags flags;
		gboolean indexable;
		GList *children;
		GFile *parent;

		children = g_list_prepend (NULL, file);
		parent = g_file_get_parent (file);

		indexable = tracker_indexing_tree_parent_is_indexable (priv->indexing_tree,
		                                                       parent, children);
		g_object_unref (parent);
		g_list_free (children);

		/* note: This supposedly works, but in practice
		 * won't ever happen as we don't get monitor events
		 * from directories triggering a filter of type
		 * TRACKER_FILTER_PARENT_DIRECTORY.
		 */
		if (!indexable) {
			/* New file was triggering a directory content
			 * filter, reindex parent directory altogether
			 */
			file = tracker_file_system_get_file (priv->file_system,
			                                     file,
			                                     G_FILE_TYPE_DIRECTORY,
			                                     NULL);
			tracker_indexing_tree_get_root (priv->indexing_tree,
							file, &flags);
			notifier_queue_file (notifier, file, flags);
			crawl_directories_start (notifier);
			return;
		}
	}

	/* Fetch the interned copy */
	canonical = tracker_file_system_get_file (priv->file_system,
	                                          file, file_type, NULL);

	/* tracker_file_system_forget_files() might already have been
	 * called on this file. In this case, the object might become
	 * invalid when returning from g_signal_emit(). Take a
	 * reference in order to prevent that.
	 */
	g_object_ref (canonical);
	g_signal_emit (notifier, signals[FILE_DELETED], 0, canonical);

	file_notifier_current_root_check_remove_directory (notifier, canonical);

	/* Remove the file from the cache (works recursively for directories) */
	tracker_file_system_forget_files (priv->file_system,
	                                  canonical,
	                                  G_FILE_TYPE_UNKNOWN);
	g_object_unref (canonical);
}

static void
monitor_item_moved_cb (TrackerMonitor *monitor,
                       GFile          *file,
                       GFile          *other_file,
                       gboolean        is_directory,
                       gboolean        is_source_monitored,
                       gpointer        user_data)
{
	TrackerFileNotifier *notifier;
	TrackerFileNotifierPrivate *priv;
	TrackerDirectoryFlags flags;

	notifier = user_data;
	priv = notifier->priv;
	tracker_indexing_tree_get_root (priv->indexing_tree, other_file, &flags);

	if (!is_source_monitored) {
		if (is_directory) {
			/* Remove monitors if any */
			tracker_monitor_remove_recursively (priv->monitor, file);

			/* If should recurse, crawl other_file, as content is "new" */
			file = tracker_file_system_get_file (priv->file_system,
			                                     other_file,
			                                     G_FILE_TYPE_DIRECTORY,
			                                     NULL);
			notifier_queue_file (notifier, file, flags);
			crawl_directories_start (notifier);
		}
		/* else, file, do nothing */
	} else {
		gboolean source_stored, should_process_other;
		GFileType file_type;
		GFile *check_file;

		if (is_directory) {
			check_file = g_object_ref (file);
		} else {
			check_file = g_file_get_parent (file);
		}

		file_type = (is_directory) ? G_FILE_TYPE_DIRECTORY : G_FILE_TYPE_REGULAR;

		/* If the (parent) directory is in
		 * the filesystem, file is stored
		 */
		source_stored = (tracker_file_system_peek_file (priv->file_system,
		                                                check_file) != NULL);
		should_process_other = tracker_indexing_tree_file_is_indexable (priv->indexing_tree,
		                                                                other_file,
		                                                                file_type);
		g_object_unref (check_file);

		if (!source_stored) {
			/* Destination location should be indexed as if new */
			/* Remove monitors if any */
			if (is_directory) {
				tracker_monitor_remove_recursively (priv->monitor,
				                                    file);
			}

			if (should_process_other) {
				gboolean dest_is_recursive;
				TrackerDirectoryFlags flags;

				tracker_indexing_tree_get_root (priv->indexing_tree, other_file, &flags);
				dest_is_recursive = (flags & TRACKER_DIRECTORY_FLAG_RECURSE) != 0;

				/* Source file was not stored, check dest file as new */
				if (!is_directory || !dest_is_recursive) {
					g_signal_emit (notifier, signals[FILE_CREATED], 0, other_file);
				} else if (is_directory) {
					/* Crawl dest directory */
					other_file = tracker_file_system_get_file (priv->file_system,
					                                           other_file,
					                                           G_FILE_TYPE_DIRECTORY,
					                                           NULL);
					notifier_queue_file (notifier, other_file, flags);
					crawl_directories_start (notifier);
				}
			}
			/* Else, do nothing else */
		} else if (!should_process_other) {
			/* Delete original location as it moves to be non indexable */
			if (is_directory) {
				tracker_monitor_remove_recursively (priv->monitor,
				                                    file);
			}

			g_signal_emit (notifier, signals[FILE_DELETED], 0, file);
			file_notifier_current_root_check_remove_directory (notifier, file);
		} else {
			/* Handle move */
			if (is_directory) {
				gboolean dest_is_recursive, source_is_recursive;
				TrackerDirectoryFlags source_flags;

				tracker_monitor_move (priv->monitor,
				                      file, other_file);

				tracker_indexing_tree_get_root (priv->indexing_tree,
				                                file, &source_flags);
				source_is_recursive = (source_flags & TRACKER_DIRECTORY_FLAG_RECURSE) != 0;
				dest_is_recursive = (flags & TRACKER_DIRECTORY_FLAG_RECURSE) != 0;

				if (source_is_recursive && !dest_is_recursive) {
					/* A directory is being moved from a
					 * recursive location to a non-recursive
					 * one, don't do anything here, and let
					 * TrackerMinerFS handle it, see item_move().
					 */
				} else if (!source_is_recursive && dest_is_recursive) {
					/* crawl the folder */
					file = tracker_file_system_get_file (priv->file_system,
					                                     other_file,
					                                     G_FILE_TYPE_DIRECTORY,
					                                     NULL);
					notifier_queue_file (notifier, file, flags);
					crawl_directories_start (notifier);
				}
			}

			g_signal_emit (notifier, signals[FILE_MOVED], 0, file, other_file);
		}
	}
}

/* Indexing tree signal handlers */
static void
indexing_tree_directory_added (TrackerIndexingTree *indexing_tree,
                               GFile               *directory,
                               gpointer             user_data)
{
	TrackerFileNotifier *notifier = user_data;
	TrackerFileNotifierPrivate *priv = notifier->priv;
	TrackerDirectoryFlags flags;

	tracker_indexing_tree_get_root (indexing_tree, directory, &flags);

	directory = tracker_file_system_get_file (priv->file_system, directory,
	                                          G_FILE_TYPE_DIRECTORY, NULL);
	notifier_queue_file (notifier, directory, flags);
	crawl_directories_start (notifier);
}

static void
indexing_tree_directory_updated (TrackerIndexingTree *indexing_tree,
                                 GFile               *directory,
                                 gpointer             user_data)
{
	TrackerFileNotifier *notifier = user_data;
	TrackerFileNotifierPrivate *priv = notifier->priv;
	TrackerDirectoryFlags flags;

	tracker_indexing_tree_get_root (indexing_tree, directory, &flags);
	flags |= TRACKER_DIRECTORY_FLAG_CHECK_DELETED;

	directory = tracker_file_system_get_file (priv->file_system, directory,
	                                          G_FILE_TYPE_DIRECTORY, NULL);
	notifier_queue_file (notifier, directory, flags);
	crawl_directories_start (notifier);
}

static void
indexing_tree_directory_removed (TrackerIndexingTree *indexing_tree,
                                 GFile               *directory,
                                 gpointer             user_data)
{
	TrackerFileNotifier *notifier = user_data;
	TrackerFileNotifierPrivate *priv = notifier->priv;
	TrackerDirectoryFlags flags;
	GList *elem;

	/* Flags are still valid at the moment of deletion */
	tracker_indexing_tree_get_root (indexing_tree, directory, &flags);
	directory = tracker_file_system_peek_file (priv->file_system, directory);

	if (!directory) {
		/* If the dir has no canonical copy,
		 * it wasn't even told to be indexed.
		 */
		return;
	}

	/* If the folder was being ignored, index/crawl it from scratch */
	if (flags & TRACKER_DIRECTORY_FLAG_IGNORE) {
		GFile *parent;

		parent = g_file_get_parent (directory);

		if (parent) {
			TrackerDirectoryFlags parent_flags;

			tracker_indexing_tree_get_root (indexing_tree,
			                                parent,
			                                &parent_flags);

			if (parent_flags & TRACKER_DIRECTORY_FLAG_RECURSE) {
				notifier_queue_file (notifier, directory, parent_flags);
				crawl_directories_start (notifier);
			} else if (tracker_indexing_tree_file_is_root (indexing_tree,
			                                               parent)) {
				g_signal_emit (notifier, signals[FILE_CREATED],
					       0, directory);
			}

			g_object_unref (parent);
		}
		return;
	}

	if ((flags & TRACKER_DIRECTORY_FLAG_PRESERVE) == 0) {
		/* Directory needs to be deleted from the store too */
		g_signal_emit (notifier, signals[FILE_DELETED], 0, directory);
	}

	elem = g_list_find_custom (priv->pending_index_roots, directory,
	                           (GCompareFunc) find_directory_root);

	if (elem) {
		root_data_free (elem->data);
		priv->pending_index_roots =
			g_list_delete_link (priv->pending_index_roots, elem);
	}

	if (priv->current_index_root &&
	    directory == priv->current_index_root->root) {
		/* Directory being currently processed */
		tracker_crawler_stop (priv->crawler);
		g_cancellable_cancel (priv->cancellable);

		/* If the crawler was already stopped (eg. we're at the querying
		 * phase), the current index root won't be cleared.
		 */
		if (priv->current_index_root) {
			root_data_free (priv->current_index_root);
			priv->current_index_root = NULL;
		}

		notifier_check_next_root (notifier);
	}

	/* Remove monitors if any */
	/* FIXME: How do we handle this with 3rd party data_providers? */
	tracker_monitor_remove_recursively (priv->monitor, directory);

	/* Remove all files from cache */
	tracker_file_system_forget_files (priv->file_system, directory,
	                                  G_FILE_TYPE_UNKNOWN);
}

static void
indexing_tree_child_updated (TrackerIndexingTree *indexing_tree,
                             GFile               *root,
                             GFile               *child,
                             gpointer             user_data)
{
	TrackerFileNotifier *notifier = user_data;
	TrackerFileNotifierPrivate *priv = notifier->priv;
	TrackerDirectoryFlags flags;
	GFileType child_type;
	GFile *canonical;

	child_type = g_file_query_file_type (child,
	                                     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                                     NULL);

	if (child_type == G_FILE_TYPE_UNKNOWN)
		return;

	canonical = tracker_file_system_get_file (priv->file_system,
	                                          child, child_type, NULL);
	tracker_indexing_tree_get_root (indexing_tree, child, &flags);

	if (child_type == G_FILE_TYPE_DIRECTORY &&
	    (flags & TRACKER_DIRECTORY_FLAG_RECURSE)) {
		flags |= TRACKER_DIRECTORY_FLAG_CHECK_DELETED;

		notifier_queue_file (notifier, canonical, flags);
		crawl_directories_start (notifier);
	} else if (tracker_indexing_tree_file_is_indexable (priv->indexing_tree,
	                                                    canonical, child_type)) {
		g_signal_emit (notifier, signals[FILE_UPDATED], 0, canonical, FALSE);
	}
}

static void
tracker_file_notifier_finalize (GObject *object)
{
	TrackerFileNotifierPrivate *priv;

	priv = TRACKER_FILE_NOTIFIER (object)->priv;

	if (priv->indexing_tree) {
		g_object_unref (priv->indexing_tree);
	}

	if (priv->data_provider) {
		g_object_unref (priv->data_provider);
	}

	if (priv->cancellable) {
		g_cancellable_cancel (priv->cancellable);
		g_object_unref (priv->cancellable);
	}

	g_object_unref (priv->crawler);
	g_object_unref (priv->monitor);
	g_object_unref (priv->file_system);
	g_clear_object (&priv->connection);

	if (priv->current_index_root)
		root_data_free (priv->current_index_root);

	g_list_foreach (priv->pending_index_roots, (GFunc) root_data_free, NULL);
	g_list_free (priv->pending_index_roots);
	g_timer_destroy (priv->timer);

	G_OBJECT_CLASS (tracker_file_notifier_parent_class)->finalize (object);
}

static void
tracker_file_notifier_constructed (GObject *object)
{
	TrackerFileNotifierPrivate *priv;
	GFile *root;

	G_OBJECT_CLASS (tracker_file_notifier_parent_class)->constructed (object);

	priv = TRACKER_FILE_NOTIFIER (object)->priv;
	g_assert (priv->indexing_tree);

	/* Initialize filesystem and register properties */
	root = tracker_indexing_tree_get_master_root (priv->indexing_tree);
	priv->file_system = tracker_file_system_new (root);

	g_signal_connect (priv->indexing_tree, "directory-added",
	                  G_CALLBACK (indexing_tree_directory_added), object);
	g_signal_connect (priv->indexing_tree, "directory-updated",
	                  G_CALLBACK (indexing_tree_directory_updated), object);
	g_signal_connect (priv->indexing_tree, "directory-removed",
	                  G_CALLBACK (indexing_tree_directory_removed), object);
	g_signal_connect (priv->indexing_tree, "child-updated",
	                  G_CALLBACK (indexing_tree_child_updated), object);

	/* Set up crawler */
	priv->crawler = tracker_crawler_new (priv->data_provider);
	tracker_crawler_set_file_attributes (priv->crawler,
	                                     G_FILE_ATTRIBUTE_TIME_MODIFIED ","
	                                     G_FILE_ATTRIBUTE_STANDARD_TYPE);

	g_signal_connect (priv->crawler, "check-file",
	                  G_CALLBACK (crawler_check_file_cb),
	                  object);
	g_signal_connect (priv->crawler, "check-directory",
	                  G_CALLBACK (crawler_check_directory_cb),
	                  object);
	g_signal_connect (priv->crawler, "check-directory-contents",
	                  G_CALLBACK (crawler_check_directory_contents_cb),
	                  object);
	g_signal_connect (priv->crawler, "directory-crawled",
	                  G_CALLBACK (crawler_directory_crawled_cb),
	                  object);
	g_signal_connect (priv->crawler, "finished",
	                  G_CALLBACK (crawler_finished_cb),
	                  object);
}

static void
tracker_file_notifier_class_init (TrackerFileNotifierClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_file_notifier_finalize;
	object_class->set_property = tracker_file_notifier_set_property;
	object_class->get_property = tracker_file_notifier_get_property;
	object_class->constructed = tracker_file_notifier_constructed;

	signals[FILE_CREATED] =
		g_signal_new ("file-created",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerFileNotifierClass,
		                               file_created),
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              1, G_TYPE_FILE);
	signals[FILE_UPDATED] =
		g_signal_new ("file-updated",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerFileNotifierClass,
		                               file_updated),
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              2, G_TYPE_FILE, G_TYPE_BOOLEAN);
	signals[FILE_DELETED] =
		g_signal_new ("file-deleted",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerFileNotifierClass,
		                               file_deleted),
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              1, G_TYPE_FILE);
	signals[FILE_MOVED] =
		g_signal_new ("file-moved",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerFileNotifierClass,
		                               file_moved),
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              2, G_TYPE_FILE, G_TYPE_FILE);
	signals[DIRECTORY_STARTED] =
		g_signal_new ("directory-started",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerFileNotifierClass,
		                               directory_started),
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              1, G_TYPE_FILE);
	signals[DIRECTORY_FINISHED] =
		g_signal_new ("directory-finished",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerFileNotifierClass,
		                               directory_finished),
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              5, G_TYPE_FILE, G_TYPE_UINT,
		              G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
	signals[FINISHED] =
		g_signal_new ("finished",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerFileNotifierClass,
		                               finished),
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE, 0, G_TYPE_NONE);

	g_object_class_install_property (object_class,
	                                 PROP_INDEXING_TREE,
	                                 g_param_spec_object ("indexing-tree",
	                                                      "Indexing tree",
	                                                      "Indexing tree",
	                                                      TRACKER_TYPE_INDEXING_TREE,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
	                                 PROP_DATA_PROVIDER,
	                                 g_param_spec_object ("data-provider",
	                                                      "Data provider",
	                                                      "Data provider to use to crawl structures populating data, e.g. like GFileEnumerator",
	                                                      TRACKER_TYPE_DATA_PROVIDER,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class,
	                          sizeof (TrackerFileNotifierClass));

	/* Initialize property quarks */
	quark_property_iri = g_quark_from_static_string ("tracker-property-iri");
	tracker_file_system_register_property (quark_property_iri, g_free);

	quark_property_store_mtime = g_quark_from_static_string ("tracker-property-store-mtime");
	tracker_file_system_register_property (quark_property_store_mtime,
	                                       g_free);

	quark_property_filesystem_mtime = g_quark_from_static_string ("tracker-property-filesystem-mtime");
	tracker_file_system_register_property (quark_property_filesystem_mtime,
	                                       g_free);

	force_check_updated = g_getenv ("TRACKER_MINER_FORCE_CHECK_UPDATED") != NULL;
}

static void
tracker_file_notifier_init (TrackerFileNotifier *notifier)
{
	TrackerFileNotifierPrivate *priv;
	GError *error = NULL;

	priv = notifier->priv =
		G_TYPE_INSTANCE_GET_PRIVATE (notifier,
		                             TRACKER_TYPE_FILE_NOTIFIER,
		                             TrackerFileNotifierPrivate);

	priv->connection = tracker_sparql_connection_get (NULL, &error);

	if (error) {
		g_warning ("Could not get SPARQL connection: %s\n",
		           error->message);
		g_error_free (error);
	}

	priv->timer = g_timer_new ();
	priv->stopped = TRUE;

	/* Set up monitor */
	priv->monitor = tracker_monitor_new ();

	g_signal_connect (priv->monitor, "item-created",
	                  G_CALLBACK (monitor_item_created_cb),
	                  notifier);
	g_signal_connect (priv->monitor, "item-updated",
	                  G_CALLBACK (monitor_item_updated_cb),
	                  notifier);
	g_signal_connect (priv->monitor, "item-attribute-updated",
	                  G_CALLBACK (monitor_item_attribute_updated_cb),
	                  notifier);
	g_signal_connect (priv->monitor, "item-deleted",
	                  G_CALLBACK (monitor_item_deleted_cb),
	                  notifier);
	g_signal_connect (priv->monitor, "item-moved",
	                  G_CALLBACK (monitor_item_moved_cb),
	                  notifier);
}

TrackerFileNotifier *
tracker_file_notifier_new (TrackerIndexingTree  *indexing_tree,
                           TrackerDataProvider  *data_provider)
{
	g_return_val_if_fail (TRACKER_IS_INDEXING_TREE (indexing_tree), NULL);

	return g_object_new (TRACKER_TYPE_FILE_NOTIFIER,
	                     "indexing-tree", indexing_tree,
	                     "data-provider", data_provider,
	                     NULL);
}

gboolean
tracker_file_notifier_start (TrackerFileNotifier *notifier)
{
	TrackerFileNotifierPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_FILE_NOTIFIER (notifier), FALSE);

	priv = notifier->priv;

	if (priv->stopped) {
		priv->stopped = FALSE;

		if (priv->pending_index_roots) {
			crawl_directories_start (notifier);
		} else {
			g_signal_emit (notifier, signals[FINISHED], 0);
		}
	}

	return TRUE;
}

void
tracker_file_notifier_stop (TrackerFileNotifier *notifier)
{
	TrackerFileNotifierPrivate *priv;

	g_return_if_fail (TRACKER_IS_FILE_NOTIFIER (notifier));

	priv = notifier->priv;

	if (!priv->stopped) {
		tracker_crawler_stop (priv->crawler);

		if (priv->current_index_root) {
			root_data_free (priv->current_index_root);
			priv->current_index_root = NULL;
		}

		g_cancellable_cancel (priv->cancellable);
		priv->stopped = TRUE;
	}
}

gboolean
tracker_file_notifier_is_active (TrackerFileNotifier *notifier)
{
	TrackerFileNotifierPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_FILE_NOTIFIER (notifier), FALSE);

	priv = notifier->priv;
	return priv->pending_index_roots || priv->current_index_root;
}

const gchar *
tracker_file_notifier_get_file_iri (TrackerFileNotifier *notifier,
                                    GFile               *file,
                                    gboolean             force)
{
	TrackerFileNotifierPrivate *priv;
	GFile *canonical;
	gchar *iri = NULL;
	gboolean found;

	g_return_val_if_fail (TRACKER_IS_FILE_NOTIFIER (notifier), NULL);
	g_return_val_if_fail (G_IS_FILE (file), NULL);

	priv = notifier->priv;

	if (G_UNLIKELY (priv->connection == NULL)) {
		return NULL;
	}

	canonical = tracker_file_system_get_file (priv->file_system,
	                                          file,
	                                          G_FILE_TYPE_REGULAR,
	                                          NULL);
	if (!canonical) {
		return NULL;
	}

	found = tracker_file_system_get_property_full (priv->file_system,
	                                               canonical,
	                                               quark_property_iri,
	                                               (gpointer *) &iri);

	if (found && !iri) {
		/* NULL here mean the file iri was "invalidated", the file
		 * was inserted by a previous event, so it has an unknown iri,
		 * and further updates are keeping the file object alive.
		 *
		 * When these updates are processed, they'll need fetching the
		 * file IRI again, so we force here extraction for these cases.
		 */
		force = TRUE;
	}

	if (!iri && force) {
		TrackerSparqlCursor *cursor;
		const gchar *str;
		gchar *sparql;

		/* Fetch data for this file synchronously */
		sparql = sparql_files_compose_query (&file, 1);
		cursor = tracker_sparql_connection_query (priv->connection,
		                                          sparql, NULL, NULL);
		g_free (sparql);

		if (!cursor)
			return NULL;

		if (!tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			g_object_unref (cursor);
			return NULL;
		}

		str = tracker_sparql_cursor_get_string (cursor, 1, NULL);
		iri = g_strdup (str);
		tracker_file_system_set_property (priv->file_system, canonical,
		                                  quark_property_iri, iri);
		g_object_unref (cursor);
	}

	return iri;
}

static gboolean
file_notifier_invalidate_file_iri_foreach (GFile    *file,
                                           gpointer  user_data)
{
	TrackerFileSystem *file_system = user_data;

	tracker_file_system_set_property (file_system,
	                                  file,
	                                  quark_property_iri,
	                                  NULL);

	return FALSE;
}

void
tracker_file_notifier_invalidate_file_iri (TrackerFileNotifier *notifier,
                                           GFile               *file,
                                           gboolean             recursive)
{
	TrackerFileNotifierPrivate *priv;
	GFile *canonical;

	g_return_if_fail (TRACKER_IS_FILE_NOTIFIER (notifier));
	g_return_if_fail (G_IS_FILE (file));

	priv = notifier->priv;
	canonical = tracker_file_system_get_file (priv->file_system,
	                                          file,
	                                          G_FILE_TYPE_REGULAR,
	                                          NULL);
	if (!canonical) {
		return;
	}

	if (!recursive) {
		/* Set a NULL iri, so we make sure to look it up afterwards */
		tracker_file_system_set_property (priv->file_system,
		                                  canonical,
		                                  quark_property_iri,
		                                  NULL);
		return;
	}

	tracker_file_system_traverse (priv->file_system,
	                              canonical,
	                              G_PRE_ORDER,
	                              file_notifier_invalidate_file_iri_foreach,
	                              -1,
	                              priv->file_system);
}

GFileType
tracker_file_notifier_get_file_type (TrackerFileNotifier *notifier,
                                     GFile               *file)
{
	TrackerFileNotifierPrivate *priv;
	GFile *canonical;

	g_return_val_if_fail (TRACKER_IS_FILE_NOTIFIER (notifier), G_FILE_TYPE_UNKNOWN);
	g_return_val_if_fail (G_IS_FILE (file), G_FILE_TYPE_UNKNOWN);

	priv = notifier->priv;
	canonical = tracker_file_system_get_file (priv->file_system,
	                                          file,
	                                          G_FILE_TYPE_REGULAR,
	                                          NULL);
	if (!canonical) {
		return G_FILE_TYPE_UNKNOWN;
	}

	return tracker_file_system_get_file_type (priv->file_system, canonical);
}
