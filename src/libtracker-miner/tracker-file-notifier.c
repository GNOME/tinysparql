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
#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-file-notifier.h"
#include "tracker-file-system.h"
#include "tracker-crawler.h"
#include "tracker-monitor.h"

static GQuark quark_property_crawled = 0;
static GQuark quark_property_queried = 0;
static GQuark quark_property_iri = 0;
static GQuark quark_property_store_mtime = 0;
static GQuark quark_property_filesystem_mtime = 0;
static GQuark quark_property_file_info = 0;

enum {
	PROP_0,
	PROP_INDEXING_TREE
};

enum {
	FILE_CREATED,
	FILE_UPDATED,
	FILE_DELETED,
	FILE_MOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
	TrackerIndexingTree *indexing_tree;
	TrackerFileSystem *file_system;

	TrackerSparqlConnection *connection;

	TrackerCrawler *crawler;
	TrackerMonitor *monitor;

	GTimer *timer;

	/* List of pending directory
	 * trees to get data from
	 */
	GList *pending_index_roots;

	guint stopped : 1;
} TrackerFileNotifierPrivate;


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

	priv = TRACKER_FILE_NOTIFIER (user_data)->priv;

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
#if 0
	if (process) {
		TrackerDirectoryFlags parent_flags;
		gboolean add_monitor;

		tracker_indexing_tree_get_root (priv->indexing_tree,
						parent, &parent_flags);

		add_monitor = (parent_flags & TRACKER_DIRECTORY_FLAG_MONITOR) != 0;

		/* FIXME: add monitor */
	}
#endif

	return process;
}

static gboolean
file_notifier_traverse_tree_foreach (TrackerFile *file,
				     gpointer     user_data)
{
	TrackerFileNotifier *notifier;
	TrackerFileNotifierPrivate *priv;
	const gchar *store_mtime, *disk_mtime;
	GFile *f;

	notifier = user_data;
	priv = notifier->priv;

	store_mtime = tracker_file_system_get_property (priv->file_system, file,
							quark_property_store_mtime);
	disk_mtime = tracker_file_system_get_property (priv->file_system, file,
						       quark_property_filesystem_mtime);
	f = tracker_file_system_resolve_file (priv->file_system, file);

	if (store_mtime && !disk_mtime) {
		/* In store but not in disk, delete */
		g_signal_emit (notifier, signals[FILE_DELETED], 0, f);
	} else if (disk_mtime && !store_mtime) {
		/* In disk but not in store, create */
		g_signal_emit (notifier, signals[FILE_CREATED], 0, f);
	} else if (store_mtime && disk_mtime &&
		   strcmp (store_mtime, disk_mtime) != 0) {
		/* Mtime changed, update */
		g_signal_emit (notifier, signals[FILE_UPDATED], 0, f);
	} else if (!store_mtime && !disk_mtime) {
		g_assert_not_reached ();
	}

	return FALSE;
}

static void
file_notifier_traverse_tree (TrackerFileNotifier *notifier)
{
	TrackerFileNotifierPrivate *priv;

	priv = notifier->priv;
	tracker_file_system_traverse (priv->file_system,
				      priv->pending_index_roots->data,
				      G_PRE_ORDER,
				      file_notifier_traverse_tree_foreach,
				      notifier);

	tracker_info ("Finished notifying files after %2.2f seconds",
	              g_timer_elapsed (priv->timer, NULL));

	/* We've finished crawling/querying on the first element
	 * of the pending list, continue onto the next */
	priv->pending_index_roots = g_list_remove_link (priv->pending_index_roots,
							priv->pending_index_roots);

	crawl_directories_start (notifier);
}

typedef struct {
	TrackerFileNotifier *notifier;
	GNode *cur_parent_node;
	TrackerFile *cur_parent;
} DirectoryCrawledData;

static gboolean
file_notifier_add_node_foreach (GNode    *node,
				gpointer  user_data)
{
	DirectoryCrawledData *data = user_data;
	TrackerFileNotifierPrivate *priv;
	GFileInfo *file_info;
	TrackerFile *f;
	GFile *file;

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

	f = tracker_file_system_get_file (priv->file_system,
					  file,
					  G_FILE_TYPE_UNKNOWN,
					  data->cur_parent);
	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_TIME_MODIFIED,
				       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				       NULL,
				       NULL);
	if (file_info) {
		guint64 time;
		time_t mtime;
		struct tm t;
		gchar *time_str;

		time = g_file_info_get_attribute_uint64 (file_info,
							 G_FILE_ATTRIBUTE_TIME_MODIFIED);
		mtime = (time_t) time;
		gmtime_r (&mtime, &t);

		time_str = g_strdup_printf ("%04d-%02d-%02dT%02d:%02d:%02dZ",
					    t.tm_year + 1900,
					    t.tm_mon + 1,
					    t.tm_mday,
					    t.tm_hour,
					    t.tm_min,
					    t.tm_sec);

		tracker_file_system_set_property (priv->file_system, f,
						  quark_property_filesystem_mtime,
						  time_str);
		g_object_unref (file_info);
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
	DirectoryCrawledData data = { 0 };

	data.notifier = user_data;
	g_node_traverse (tree,
			 G_PRE_ORDER,
			 G_TRAVERSE_ALL,
			 -1,
			 file_notifier_add_node_foreach,
			 &data);

	g_message ("  Found %d directories, ignored %d directories",
	           directories_found,
	           directories_ignored);
	g_message ("  Found %d files, ignored %d files",
	           files_found,
	           files_ignored);
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

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		TrackerFile *file;
		const gchar *mtime, *iri;
		GFile *f;

		f = g_file_new_for_uri (tracker_sparql_cursor_get_string (cursor, 0, NULL));
		file = tracker_file_system_get_file (priv->file_system, f,
						     G_FILE_TYPE_UNKNOWN,
						     NULL);

		iri = tracker_sparql_cursor_get_string (cursor, 1, NULL);
		tracker_file_system_set_property (priv->file_system, file,
						  quark_property_iri,
						  g_strdup (iri));

		mtime = tracker_sparql_cursor_get_string (cursor, 2, NULL);
		tracker_file_system_set_property (priv->file_system, file,
						  quark_property_store_mtime,
						  g_strdup (mtime));

		g_object_unref (f);
	}

	/* Mark the directory root as queried */
	tracker_file_system_set_property (priv->file_system,
					  priv->pending_index_roots->data,
					  quark_property_queried,
					  GUINT_TO_POINTER (TRUE));

	tracker_info ("finished querying files after %2.2f seconds",
	              g_timer_elapsed (priv->timer, NULL));

	/* If it's also been crawled, finish operation */
	if (tracker_file_system_get_property (priv->file_system,
					      priv->pending_index_roots->data,
					      quark_property_crawled)) {
		file_notifier_traverse_tree (notifier);
	}

	g_object_unref (cursor);
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
		directory = tracker_file_system_resolve_file (priv->file_system,
							      priv->pending_index_roots->data);

		tracker_indexing_tree_get_root (priv->indexing_tree,
						directory,
						&flags);

		if (tracker_crawler_start (priv->crawler,
					   directory,
					   (flags & TRACKER_DIRECTORY_FLAG_RECURSE) != 0)) {
			gchar *sparql, *uri;

			g_timer_reset (priv->timer);

			uri = g_file_get_uri (directory);

			if (flags & TRACKER_DIRECTORY_FLAG_RECURSE) {
				sparql = g_strdup_printf ("select ?url ?u nfo:fileLastModified(?u) "
							  "where {"
							  "  ?u a nfo:FileDataObject ; "
							  "     nie:url ?url . "
							  "  FILTER (?url = \"%s\" || "
							  "          fn:starts-with (?url, \"%s/\")) "
							  "}", uri, uri);
			} else {
				sparql = g_strdup_printf ("select ?url ?u nfo:fileLastModified(?u) "
							  "where { "
							  "  ?u a nfo:FileDataObject ; "
							  "     nie:url ?url . "
							  "  OPTIONAL { ?u nfo:belongsToContainer ?p } . "
							  "  FILTER (?url = \"%s\" || "
							  "          nie:url(?p) = \"%s\") "
							  "}", uri, uri);
			}

			tracker_sparql_connection_query_async (priv->connection,
							       sparql,
							       NULL,
							       sparql_query_cb,
							       notifier);

			g_free (sparql);
			g_free (uri);

			return TRUE;
		}

		/* Remove index root and try the next one */
		priv->pending_index_roots = g_list_remove_link (priv->pending_index_roots,
								priv->pending_index_roots);
	}

	return FALSE;
}

static void
crawler_finished_cb (TrackerCrawler *crawler,
                     gboolean        was_interrupted,
                     gpointer        user_data)
{
	TrackerFileNotifier *notifier = user_data;
	TrackerFileNotifierPrivate *priv = notifier->priv;

	tracker_info ("%s crawling files after %2.2f seconds",
	              was_interrupted ? "Stopped" : "Finished",
	              g_timer_elapsed (priv->timer, NULL));

	if (!was_interrupted) {
		TrackerFile *directory;

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
	TrackerFile *file;

	tracker_indexing_tree_get_root (indexing_tree, directory, &flags);

	file = tracker_file_system_get_file (priv->file_system, directory,
	                                     G_FILE_TYPE_DIRECTORY, NULL);

	if (!priv->stopped &&
	    !priv->pending_index_roots) {
		start_crawler = TRUE;
	}

	priv->pending_index_roots = g_list_append (priv->pending_index_roots,
	                                           file);
	if (start_crawler) {
		crawl_directories_start (notifier);
	}
}

static void
indexing_tree_directory_removed (TrackerIndexingTree *indexing_tree,
                                 GFile               *directory,
                                 gpointer             user_data)
{
	/* Signal as removed? depends on delete vs unmount */
}

static void
tracker_file_notifier_finalize (GObject *object)
{
	TrackerFileNotifierPrivate *priv;

	priv = TRACKER_FILE_NOTIFIER (object)->priv;

	if (priv->indexing_tree) {
		g_object_unref (priv->indexing_tree);
	}

	g_object_unref (priv->file_system);
	g_object_unref (priv->crawler);

	g_list_free (priv->pending_index_roots);

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
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE,
		              1, G_TYPE_FILE);
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
	quark_property_queried = g_quark_from_static_string ("tracker-property-queried");
	quark_property_iri = g_quark_from_static_string ("tracker-property-iri");
	quark_property_store_mtime = g_quark_from_static_string ("tracker-property-store-mtime");
	quark_property_filesystem_mtime = g_quark_from_static_string ("tracker-property-filesystem-mtime");
	quark_property_file_info = g_quark_from_static_string ("tracker-property-file-info");
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
		g_critical ("Could not get SPARQL connection: %s\n",
			    error->message);
		g_error_free (error);

		g_assert_not_reached ();
	}

	/* Initialize filesystem and register properties */
	priv->file_system = tracker_file_system_new ();

	/* booleans */
	tracker_file_system_register_property (priv->file_system,
					       quark_property_crawled, NULL);
	tracker_file_system_register_property (priv->file_system,
					       quark_property_queried, NULL);

	/* strings */
	tracker_file_system_register_property (priv->file_system,
					       quark_property_iri, g_free);
	tracker_file_system_register_property (priv->file_system,
					       quark_property_store_mtime,
					       g_free);
	tracker_file_system_register_property (priv->file_system,
					       quark_property_filesystem_mtime,
					       g_free);

	/* GFileInfo */
	tracker_file_system_register_property (priv->file_system,
					       quark_property_file_info,
					       g_object_unref);

	priv->timer = g_timer_new ();
	priv->stopped = TRUE;

	/* Set up crawler */
	priv->crawler = tracker_crawler_new ();
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
		priv->stopped = TRUE;
	}
}
