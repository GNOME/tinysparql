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

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-date-time.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-miner-common.h"
#include "tracker-file-notifier.h"
#include "tracker-file-system.h"
#include "tracker-crawler.h"
#include "tracker-monitor.h"
#include "tracker-marshal.h"

static GQuark quark_property_crawled = 0;
static GQuark quark_property_queried = 0;
static GQuark quark_property_iri = 0;
static GQuark quark_property_store_mtime = 0;
static GQuark quark_property_filesystem_mtime = 0;

enum {
	PROP_0,
	PROP_INDEXING_TREE
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
	TrackerIndexingTree *indexing_tree;
	TrackerFileSystem *file_system;

	TrackerSparqlConnection *connection;
	GCancellable *cancellable;

	TrackerCrawler *crawler;
	TrackerMonitor *monitor;

	GTimer *timer;

	/* List of pending directory
	 * trees to get data from
	 */
	GList *pending_index_roots;

	guint stopped : 1;
} TrackerFileNotifierPrivate;

typedef struct {
	TrackerFileNotifier *notifier;
	GNode *cur_parent_node;

	/* Canonical copy from priv->file_system */
	GFile *cur_parent;
} DirectoryCrawledData;

static gboolean crawl_directories_start (TrackerFileNotifier *notifier);


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
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
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

	canonical = tracker_file_system_peek_file (priv->file_system, directory);
	root = tracker_indexing_tree_get_root (priv->indexing_tree, directory, NULL);

	/* If it's a config root itself, other than the one
	 * currently processed, bypass it, it will be processed
	 * when the time arrives.
	 */
	if (canonical &&
	    root == canonical &&
	    root != priv->pending_index_roots->data) {
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

	notifier = user_data;
	priv = notifier->priv;

	store_mtime = tracker_file_system_get_property (priv->file_system, file,
	                                                quark_property_store_mtime);
	disk_mtime = tracker_file_system_get_property (priv->file_system, file,
	                                               quark_property_filesystem_mtime);

	if (store_mtime && !disk_mtime) {
		/* In store but not in disk, delete */
		g_signal_emit (notifier, signals[FILE_DELETED], 0, file);

		return TRUE;
	} else if (disk_mtime && !store_mtime) {
		/* In disk but not in store, create */
		g_signal_emit (notifier, signals[FILE_CREATED], 0, file);
	} else if (store_mtime && disk_mtime &&
	           abs (*disk_mtime - *store_mtime) > 2) {
		/* Mtime changed, update */
		g_signal_emit (notifier, signals[FILE_UPDATED], 0, file, FALSE);
	} else if (!store_mtime && !disk_mtime) {
		/* what are we doing with such file? should happen rarely,
		 * only with files that we've queried, but we decided not
		 * to crawl (i.e. embedded root directories, that would
		 * be processed when that root is being crawled).
		 */
		if (!tracker_indexing_tree_file_is_root (priv->indexing_tree, file)) {
			gchar *uri;

			uri = g_file_get_uri (file);
			g_critical ("File '%s' has no disk nor store mtime",
			            uri);
			g_free (uri);
		}
	}

	return FALSE;
}

static void
file_notifier_traverse_tree (TrackerFileNotifier *notifier)
{
	TrackerFileNotifierPrivate *priv;
	GFile *current_root, *config_root;
	TrackerDirectoryFlags flags;

	priv = notifier->priv;
	current_root = priv->pending_index_roots->data;
	config_root = tracker_indexing_tree_get_root (priv->indexing_tree,
						      current_root, &flags);

	/* Check mtime for 1) directories with the check_mtime flag
	 * and 2) directories gotten from monitor events.
	 */
	if (config_root != current_root ||
	    flags & TRACKER_DIRECTORY_FLAG_CHECK_MTIME) {
		tracker_file_system_traverse (priv->file_system,
		                              current_root,
		                              G_LEVEL_ORDER,
		                              file_notifier_traverse_tree_foreach,
		                              notifier);
	}

	/* We dispose regular files here, only directories are cached once crawling
	 * has completed.
	 */
	tracker_file_system_forget_files (priv->file_system,
	                                  current_root,
	                                  G_FILE_TYPE_REGULAR);

	tracker_info ("  Notified files after %2.2f seconds",
	              g_timer_elapsed (priv->timer, NULL));

	/* We've finished crawling/querying on the first element
	 * of the pending list, continue onto the next */
	priv->pending_index_roots = g_list_delete_link (priv->pending_index_roots,
	                                                priv->pending_index_roots);

	if (priv->pending_index_roots) {
		crawl_directories_start (notifier);
	} else {
		g_signal_emit (notifier, signals[FINISHED], 0);
	}
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

		file_type = g_file_info_get_file_type (file_info);

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
	DirectoryCrawledData data = { 0 };

	notifier = data.notifier = user_data;
	g_node_traverse (tree,
	                 G_PRE_ORDER,
	                 G_TRAVERSE_ALL,
	                 -1,
	                 file_notifier_add_node_foreach,
	                 &data);

	g_signal_emit (notifier, signals[DIRECTORY_FINISHED], 0,
	               directory,
	               directories_found, directories_ignored,
	               files_found, files_ignored);

	tracker_info ("  Found %d directories, ignored %d directories",
	              directories_found,
	              directories_ignored);
	tracker_info ("  Found %d files, ignored %d files",
	              files_found,
	              files_ignored);
}

static void
sparql_file_query_populate (TrackerFileNotifier *notifier,
                            TrackerSparqlCursor *cursor,
                            gboolean             check_root)
{
	TrackerFileNotifierPrivate *priv;

	priv = notifier->priv;

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		GFile *file, *canonical, *root;
		const gchar *mtime, *iri;
		guint64 *time_ptr;
		GError *error = NULL;

		file = g_file_new_for_uri (tracker_sparql_cursor_get_string (cursor, 0, NULL));

		if (check_root) {
			/* If it's a config root itself, other than the one
			 * currently processed, bypass it, it will be processed
			 * when the time arrives.
			 */
			canonical = tracker_file_system_peek_file (priv->file_system, file);
			root = tracker_indexing_tree_get_root (priv->indexing_tree, file, NULL);

			if (canonical &&
			    root == file &&
			    root != priv->pending_index_roots->data) {
				g_object_unref (file);
				continue;
			}
		}

		canonical = tracker_file_system_get_file (priv->file_system,
		                                          file,
		                                          G_FILE_TYPE_UNKNOWN,
		                                          NULL);

		iri = tracker_sparql_cursor_get_string (cursor, 1, NULL);
		tracker_file_system_set_property (priv->file_system, canonical,
		                                  quark_property_iri,
		                                  g_strdup (iri));

		mtime = tracker_sparql_cursor_get_string (cursor, 2, NULL);
		time_ptr = g_new (guint64, 1);
		*time_ptr = (guint64) tracker_string_to_date (mtime, NULL, &error);

		if (error) {
			/* This should never happen. Assume that file was modified. */
			g_critical ("Getting store mtime: %s", error->message);
			g_clear_error (&error);
			*time_ptr = 0;
		}

		tracker_file_system_set_property (priv->file_system, canonical,
		                                  quark_property_store_mtime,
		                                  time_ptr);
		g_object_unref (file);
	}
}

static void
sparql_query_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	TrackerFileNotifierPrivate *priv;
	TrackerFileNotifier *notifier;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	notifier = user_data;
	priv = notifier->priv;
	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                 result, &error);

	if (!cursor || error) {
		g_warning ("Could not query directory elements: %s\n", error->message);
		g_error_free (error);
		return;
	}

	sparql_file_query_populate (notifier, cursor, TRUE);

	/* Mark the directory root as queried */
	tracker_file_system_set_property (priv->file_system,
	                                  priv->pending_index_roots->data,
	                                  quark_property_queried,
	                                  GUINT_TO_POINTER (TRUE));

	tracker_info ("  Queried files after %2.2f seconds",
	              g_timer_elapsed (priv->timer, NULL));

	/* If it's also been crawled, finish operation */
	if (tracker_file_system_get_property (priv->file_system,
	                                      priv->pending_index_roots->data,
	                                      quark_property_crawled)) {
		file_notifier_traverse_tree (notifier);
	}

	g_object_unref (cursor);
}

static void
sparql_file_query_start (TrackerFileNotifier *notifier,
                         GFile               *file,
                         GFileType            file_type,
                         gboolean             recursive,
                         gboolean             sync)
{
	TrackerFileNotifierPrivate *priv;
	gchar *uri, *sparql;

	priv = notifier->priv;
	uri = g_file_get_uri (file);

	if (file_type == G_FILE_TYPE_DIRECTORY) {
		if (recursive) {
			sparql = g_strdup_printf ("select ?url ?u nfo:fileLastModified(?u) "
			                          "where {"
			                          "  ?u a nie:DataObject ; "
			                          "     nie:url ?url . "
			                          "  FILTER (?url = \"%s\" || "
			                          "          fn:starts-with (?url, \"%s/\")) "
			                          "}", uri, uri);
		} else {
			sparql = g_strdup_printf ("select ?url ?u nfo:fileLastModified(?u) "
			                          "where { "
			                          "  ?u a nie:DataObject ; "
			                          "     nie:url ?url . "
			                          "  OPTIONAL { ?u nfo:belongsToContainer ?p } . "
			                          "  FILTER (?url = \"%s\" || "
			                          "          nie:url(?p) = \"%s\") "
			                          "}", uri, uri);
		}
	} else {
		/* If it's a regular file, only query this item */
		sparql = g_strdup_printf ("select ?url ?u nfo:fileLastModified(?u) "
		                          "where { "
		                          "  ?u a nie:DataObject ; "
		                          "     nie:url ?url ; "
		                          "     nie:url \"%s\" . "
		                          "}", uri);
	}

	if (sync) {
		TrackerSparqlCursor *cursor;

		cursor = tracker_sparql_connection_query (priv->connection,
		                                          sparql, NULL, NULL);
		if (cursor) {
			sparql_file_query_populate (notifier, cursor, FALSE);
			g_object_unref (cursor);
		}
	} else {
		tracker_sparql_connection_query_async (priv->connection,
		                                       sparql,
		                                       priv->cancellable,
		                                       sparql_query_cb,
		                                       notifier);
	}

	g_free (sparql);
	g_free (uri);
}

static gboolean
crawl_directories_start (TrackerFileNotifier *notifier)
{
	TrackerFileNotifierPrivate *priv = notifier->priv;
	TrackerDirectoryFlags flags;
	GFile *directory;

	if (!priv->pending_index_roots) {
		return FALSE;
	}

	if (priv->stopped) {
		return FALSE;
	}

	while (priv->pending_index_roots) {
		directory = priv->pending_index_roots->data;

		tracker_indexing_tree_get_root (priv->indexing_tree,
		                                directory,
		                                &flags);

		/* Unset crawled/queried checks on the
		 * directory, we might have requested a
		 * reindex.
		 */
		tracker_file_system_unset_property (priv->file_system,
						    directory,
						    quark_property_crawled);
		tracker_file_system_unset_property (priv->file_system,
						    directory,
						    quark_property_queried);

		g_cancellable_reset (priv->cancellable);

		if ((flags & TRACKER_DIRECTORY_FLAG_IGNORE) == 0 &&
		    tracker_crawler_start (priv->crawler,
		                           directory,
		                           (flags & TRACKER_DIRECTORY_FLAG_RECURSE) != 0)) {
			gchar *uri;

			sparql_file_query_start (notifier, directory,
			                         G_FILE_TYPE_DIRECTORY,
			                         (flags & TRACKER_DIRECTORY_FLAG_RECURSE) != 0,
			                         FALSE);

			g_timer_reset (priv->timer);
			g_signal_emit (notifier, signals[DIRECTORY_STARTED], 0, directory);

			uri = g_file_get_uri (directory);
			tracker_info ("Started inspecting '%s'", uri);
			g_free (uri);

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

		/* Remove index root and try the next one */
		priv->pending_index_roots = g_list_delete_link (priv->pending_index_roots,
		                                                priv->pending_index_roots);
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

	tracker_info ("  %s crawling files after %2.2f seconds",
	              was_interrupted ? "Stopped" : "Finished",
	              g_timer_elapsed (priv->timer, NULL));

	if (!was_interrupted) {
		GFile *directory;

		directory = priv->pending_index_roots->data;

		/* Mark the directory root as crawled */
		tracker_file_system_set_property (priv->file_system, directory,
		                                  quark_property_crawled,
		                                  GUINT_TO_POINTER (TRUE));

		/* If it's also been queried, finish operation */
		if (tracker_file_system_get_property (priv->file_system,
		                                      directory,
		                                      quark_property_queried)) {
			file_notifier_traverse_tree (notifier);
		}
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
			priv->pending_index_roots =
				g_list_append (priv->pending_index_roots,
				               canonical);

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

	if (!tracker_indexing_tree_file_is_indexable (priv->indexing_tree,
	                                              file, file_type)) {
		/* File was not indexed */
		return ;
	}

	if (!is_directory) {
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
			priv->pending_index_roots =
				g_list_append (priv->pending_index_roots, file);

			crawl_directories_start (notifier);
			return;
		}
	}

	/* Fetch the interned copy */
	canonical = tracker_file_system_get_file (priv->file_system,
	                                          file, file_type, NULL);
	g_signal_emit (notifier, signals[FILE_DELETED], 0, canonical);

	/* Remove the file from the cache (works recursively for directories) */
	tracker_file_system_forget_files (priv->file_system,
	                                  file,
	                                  G_FILE_TYPE_UNKNOWN);
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

	notifier = user_data;
	priv = notifier->priv;

	if (!is_source_monitored) {
		if (is_directory) {
			/* Remove monitors if any */
			tracker_monitor_remove_recursively (priv->monitor, file);

			/* If should recurse, crawl other_file, as content is "new" */
			file = tracker_file_system_get_file (priv->file_system,
			                                     other_file,
			                                     G_FILE_TYPE_DIRECTORY,
			                                     NULL);
			priv->pending_index_roots =
				g_list_append (priv->pending_index_roots, file);

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
					priv->pending_index_roots =
						g_list_append (priv->pending_index_roots,
						               other_file);

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
		} else {
			/* Handle move */
			if (is_directory) {
				gboolean dest_is_recursive, source_is_recursive;
				TrackerDirectoryFlags flags;

				tracker_monitor_move (priv->monitor,
				                      file, other_file);

				tracker_indexing_tree_get_root (priv->indexing_tree,
				                                other_file,
				                                &flags);
				dest_is_recursive = (flags & TRACKER_DIRECTORY_FLAG_RECURSE) != 0;

				tracker_indexing_tree_get_root (priv->indexing_tree,
				                                file, &flags);
				source_is_recursive = (flags & TRACKER_DIRECTORY_FLAG_RECURSE) != 0;

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
					priv->pending_index_roots =
						g_list_append (priv->pending_index_roots,
						               file);

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
	gboolean start_crawler = FALSE;
	TrackerDirectoryFlags flags;

	tracker_indexing_tree_get_root (indexing_tree, directory, &flags);

	directory = tracker_file_system_get_file (priv->file_system, directory,
	                                          G_FILE_TYPE_DIRECTORY, NULL);
	if (!priv->stopped &&
	    !priv->pending_index_roots) {
		start_crawler = TRUE;
	}

	if (!g_list_find (priv->pending_index_roots, directory)) {
		priv->pending_index_roots = g_list_append (priv->pending_index_roots,
		                                           directory);
		if (start_crawler) {
			crawl_directories_start (notifier);
		}
	}
}

static void
indexing_tree_directory_removed (TrackerIndexingTree *indexing_tree,
                                 GFile               *directory,
                                 gpointer             user_data)
{
	TrackerFileNotifier *notifier = user_data;
	TrackerFileNotifierPrivate *priv = notifier->priv;
	TrackerDirectoryFlags flags;

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
				priv->pending_index_roots =
					g_list_append (priv->pending_index_roots,
						       g_object_ref (directory));

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

	if (priv->pending_index_roots) {
		gboolean start_crawler = FALSE;

		if (directory == priv->pending_index_roots->data) {
			/* Directory being currently processed */
			tracker_crawler_stop (priv->crawler);
			g_cancellable_cancel (priv->cancellable);
			start_crawler = TRUE;
		}

		priv->pending_index_roots = g_list_remove_all (priv->pending_index_roots,
		                                               directory);

		if (start_crawler && priv->pending_index_roots != NULL) {
			crawl_directories_start (notifier);
		}
	}

	/* Remove monitors if any */
	tracker_monitor_remove_recursively (priv->monitor, directory);

	/* Remove all files from cache */
	tracker_file_system_forget_files (priv->file_system, directory,
	                                  G_FILE_TYPE_UNKNOWN);
}

static void
tracker_file_notifier_finalize (GObject *object)
{
	TrackerFileNotifierPrivate *priv;

	priv = TRACKER_FILE_NOTIFIER (object)->priv;

	if (priv->indexing_tree) {
		g_object_unref (priv->indexing_tree);
	}

	g_object_unref (priv->crawler);
	g_object_unref (priv->monitor);
	g_object_unref (priv->file_system);
	g_object_unref (priv->cancellable);

	g_list_free (priv->pending_index_roots);
	g_timer_destroy (priv->timer);

	G_OBJECT_CLASS (tracker_file_notifier_parent_class)->finalize (object);
}

static void
tracker_file_notifier_constructed (GObject *object)
{
	TrackerFileNotifierPrivate *priv;

	priv = TRACKER_FILE_NOTIFIER (object)->priv;
	g_assert (priv->indexing_tree);

	g_signal_connect (priv->indexing_tree, "directory-added",
	                  G_CALLBACK (indexing_tree_directory_added), object);
	g_signal_connect (priv->indexing_tree, "directory-updated",
	                  G_CALLBACK (indexing_tree_directory_added), object);
	g_signal_connect (priv->indexing_tree, "directory-removed",
	                  G_CALLBACK (indexing_tree_directory_removed), object);
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
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE,
		              1, G_TYPE_FILE);
	signals[FILE_UPDATED] =
		g_signal_new ("file-updated",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerFileNotifierClass,
		                               file_updated),
		              NULL, NULL,
		              tracker_marshal_VOID__OBJECT_BOOLEAN,
		              G_TYPE_NONE,
		              2, G_TYPE_FILE, G_TYPE_BOOLEAN);
	signals[FILE_DELETED] =
		g_signal_new ("file-deleted",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerFileNotifierClass,
		                               file_deleted),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE,
		              1, G_TYPE_FILE);
	signals[FILE_MOVED] =
		g_signal_new ("file-moved",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerFileNotifierClass,
		                               file_moved),
		              NULL, NULL,
		              tracker_marshal_VOID__OBJECT_OBJECT,
		              G_TYPE_NONE,
		              2, G_TYPE_FILE, G_TYPE_FILE);
	signals[DIRECTORY_STARTED] =
		g_signal_new ("directory-started",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerFileNotifierClass,
		                               directory_started),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE,
		              1, G_TYPE_FILE);
	signals[DIRECTORY_FINISHED] =
		g_signal_new ("directory-finished",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerFileNotifierClass,
		                               directory_finished),
		              NULL, NULL,
		              tracker_marshal_VOID__OBJECT_UINT_UINT_UINT_UINT,
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
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0, G_TYPE_NONE);

	g_object_class_install_property (object_class,
	                                 PROP_INDEXING_TREE,
	                                 g_param_spec_object ("indexing-tree",
	                                                      "Indexing tree",
	                                                      "Indexing tree",
	                                                      TRACKER_TYPE_INDEXING_TREE,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY));
	g_type_class_add_private (object_class,
	                          sizeof (TrackerFileNotifierClass));

	/* Initialize property quarks */
	quark_property_crawled = g_quark_from_static_string ("tracker-property-crawled");
	tracker_file_system_register_property (quark_property_crawled, NULL);

	quark_property_queried = g_quark_from_static_string ("tracker-property-queried");
	tracker_file_system_register_property (quark_property_queried, NULL);

	quark_property_iri = g_quark_from_static_string ("tracker-property-iri");
	tracker_file_system_register_property (quark_property_iri, g_free);

	quark_property_store_mtime = g_quark_from_static_string ("tracker-property-store-mtime");
	tracker_file_system_register_property (quark_property_store_mtime,
	                                       g_free);

	quark_property_filesystem_mtime = g_quark_from_static_string ("tracker-property-filesystem-mtime");
	tracker_file_system_register_property (quark_property_filesystem_mtime,
	                                       g_free);
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
	priv->cancellable = g_cancellable_new ();

	if (error) {
		g_critical ("Could not get SPARQL connection: %s\n",
		            error->message);
		g_error_free (error);

		g_assert_not_reached ();
	}

	/* Initialize filesystem and register properties */
	priv->file_system = tracker_file_system_new ();

	priv->timer = g_timer_new ();
	priv->stopped = TRUE;

	/* Set up crawler */
	priv->crawler = tracker_crawler_new ();
	tracker_crawler_set_file_attributes (priv->crawler,
	                                     G_FILE_ATTRIBUTE_TIME_MODIFIED ","
	                                     G_FILE_ATTRIBUTE_STANDARD_TYPE);

	g_signal_connect (priv->crawler, "check-file",
	                  G_CALLBACK (crawler_check_file_cb),
	                  notifier);
	g_signal_connect (priv->crawler, "check-directory",
	                  G_CALLBACK (crawler_check_directory_cb),
	                  notifier);
	g_signal_connect (priv->crawler, "check-directory-contents",
	                  G_CALLBACK (crawler_check_directory_contents_cb),
	                  notifier);
	g_signal_connect (priv->crawler, "directory-crawled",
	                  G_CALLBACK (crawler_directory_crawled_cb),
	                  notifier);
	g_signal_connect (priv->crawler, "finished",
	                  G_CALLBACK (crawler_finished_cb),
	                  notifier);

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
tracker_file_notifier_new (TrackerIndexingTree *indexing_tree)
{
	g_return_val_if_fail (TRACKER_IS_INDEXING_TREE (indexing_tree), NULL);

	return g_object_new (TRACKER_TYPE_FILE_NOTIFIER,
	                     "indexing-tree", indexing_tree,
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
	return priv->pending_index_roots != NULL;
}

const gchar *
tracker_file_notifier_get_file_iri (TrackerFileNotifier *notifier,
                                    GFile               *file)
{
	TrackerFileNotifierPrivate *priv;
	GFile *canonical;
	gchar *iri = NULL;

	g_return_val_if_fail (TRACKER_IS_FILE_NOTIFIER (notifier), NULL);
	g_return_val_if_fail (G_IS_FILE (file), NULL);

	priv = notifier->priv;
	canonical = tracker_file_system_get_file (priv->file_system,
	                                          file,
	                                          G_FILE_TYPE_REGULAR,
	                                          NULL);
	if (!canonical) {
		return NULL;
	}

	iri = tracker_file_system_get_property (priv->file_system,
	                                        canonical,
	                                        quark_property_iri);

	if (!iri) {
		/* Fetch data for this file synchronously */
		sparql_file_query_start (notifier, canonical,
		                         G_FILE_TYPE_REGULAR,
		                         FALSE, TRUE);

		iri = tracker_file_system_get_property (priv->file_system,
		                                        canonical,
		                                        quark_property_iri);
	}

	return iri;
}
