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

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-crawler.h"
#include "tracker-marshal.h"
#include "tracker-miner-fs.h"
#include "tracker-monitor.h"
#include "tracker-utils.h"
#include "tracker-thumbnailer.h"

/* If defined will print the tree from GNode while running */
#undef ENABLE_TREE_DEBUGGING

/**
 * SECTION:tracker-miner-fs
 * @short_description: Abstract base class for filesystem miners
 * @include: libtracker-miner/tracker-miner.h
 *
 * #TrackerMinerFS is an abstract base class for miners that collect data
 * from the filesystem, all the filesystem crawling and monitoring is
 * abstracted away, leaving to implementations the decisions of what
 * directories/files should it process, and the actual data extraction.
 **/

#define TRACKER_MINER_FS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_FS, TrackerMinerFSPrivate))

typedef struct {
	GFile *file;
	GFile *source_file;
} ItemMovedData;

typedef struct {
	GFile    *file;
	guint     recurse : 1;
	guint     ref_count : 7;
} DirectoryData;

typedef struct {
	GFile *file;
	gchar *urn;
	gchar *parent_urn;
	GCancellable *cancellable;
	TrackerSparqlBuilder *builder;
} ProcessData;

typedef struct {
	GMainLoop *main_loop;
	const gchar *uri;
	gchar *iri;
	gchar *mime;
	gboolean get_mime;
} SparqlQueryData;

typedef struct {
	GMainLoop *main_loop;
	GHashTable *values;
} CacheQueryData;

typedef struct {
	GMainLoop *main_loop;
	GString   *sparql;
	const gchar *source_uri;
	const gchar *uri;
} RecursiveMoveData;

typedef struct {
	GNode *tree;
	GQueue *nodes;
	guint n_items;
	guint n_items_processed;
} CrawledDirectoryData;

struct TrackerMinerFSPrivate {
	TrackerMonitor *monitor;
	TrackerCrawler *crawler;

	GQueue         *crawled_directories;

	/* File queues for indexer */
	GQueue         *items_created;
	GQueue         *items_updated;
	GQueue         *items_deleted;
	GQueue         *items_moved;
	GHashTable     *items_ignore_next_update;

	GQuark          quark_ignore_file;

	GList          *config_directories;

	GList          *directories;
	DirectoryData  *current_directory;

	GTimer         *timer;

	guint           crawl_directories_id;
	guint           item_queues_handler_id;

	gdouble         throttle;

	GList          *processing_pool;
	guint           pool_limit;

	/* Parent folder URN cache */
	GFile          *current_parent;
	gchar          *current_parent_urn;

	/* Folder contents' mtime cache */
	GHashTable     *mtime_cache;

	/* File -> iri cache */
	GHashTable     *iri_cache;

	/* Status */
	guint           been_started : 1;
	guint           been_crawled : 1;
	guint           shown_totals : 1;
	guint           is_paused : 1;
	guint           is_crawling : 1;

	/* Statistics */
	guint           total_directories_found;
	guint           total_directories_ignored;
	guint           total_files_found;
	guint           total_files_ignored;

	guint           directories_found;
	guint           directories_ignored;
	guint           files_found;
	guint           files_ignored;

	guint           total_files_processed;
	guint           total_files_notified;
	guint           total_files_notified_error;
};

typedef enum {
	QUEUE_NONE,
	QUEUE_CREATED,
	QUEUE_UPDATED,
	QUEUE_DELETED,
	QUEUE_MOVED,
	QUEUE_IGNORE_NEXT_UPDATE,
	QUEUE_WAIT
} QueueState;

enum {
	CHECK_FILE,
	CHECK_DIRECTORY,
	CHECK_DIRECTORY_CONTENTS,
	MONITOR_DIRECTORY,
	PROCESS_FILE,
	IGNORE_NEXT_UPDATE_FILE,
	FINISHED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_THROTTLE,
	PROP_POOL_LIMIT
};

static void           fs_finalize                         (GObject              *object);
static void           fs_set_property                     (GObject              *object,
                                                           guint                 prop_id,
                                                           const GValue         *value,
                                                           GParamSpec           *pspec);
static void           fs_get_property                     (GObject              *object,
                                                           guint                 prop_id,
                                                           GValue               *value,
                                                           GParamSpec           *pspec);
static gboolean       fs_defaults                         (TrackerMinerFS       *fs,
                                                           GFile                *file);
static gboolean       fs_contents_defaults                (TrackerMinerFS       *fs,
                                                           GFile                *parent,
                                                           GList                *children);
static void           miner_started                       (TrackerMiner         *miner);
static void           miner_stopped                       (TrackerMiner         *miner);
static void           miner_paused                        (TrackerMiner         *miner);
static void           miner_resumed                       (TrackerMiner         *miner);
static void           miner_ignore_next_update            (TrackerMiner         *miner,
                                                           const GStrv           subjects);
static DirectoryData *directory_data_new                  (GFile                *file,
                                                           gboolean              recurse);
static DirectoryData *directory_data_ref                  (DirectoryData        *dd);
static void           directory_data_unref                (DirectoryData        *dd);
static ItemMovedData *item_moved_data_new                 (GFile                *file,
                                                           GFile                *source_file);
static void           item_moved_data_free                (ItemMovedData        *data);
static void           monitor_item_created_cb             (TrackerMonitor       *monitor,
                                                           GFile                *file,
                                                           gboolean              is_directory,
                                                           gpointer              user_data);
static void           monitor_item_updated_cb             (TrackerMonitor       *monitor,
                                                           GFile                *file,
                                                           gboolean              is_directory,
                                                           gpointer              user_data);
static void           monitor_item_deleted_cb             (TrackerMonitor       *monitor,
                                                           GFile                *file,
                                                           gboolean              is_directory,
                                                           gpointer              user_data);
static void           monitor_item_moved_cb               (TrackerMonitor       *monitor,
                                                           GFile                *file,
                                                           GFile                *other_file,
                                                           gboolean              is_directory,
                                                           gboolean              is_source_monitored,
                                                           gpointer              user_data);
static gboolean       crawler_check_file_cb               (TrackerCrawler       *crawler,
                                                           GFile                *file,
                                                           gpointer              user_data);
static gboolean       crawler_check_directory_cb          (TrackerCrawler       *crawler,
                                                           GFile                *file,
                                                           gpointer              user_data);
static gboolean       crawler_check_directory_contents_cb (TrackerCrawler       *crawler,
                                                           GFile                *parent,
                                                           GList                *children,
                                                           gpointer              user_data);
static void           crawler_directory_crawled_cb        (TrackerCrawler       *crawler,
                                                           GFile                *directory,
                                                           GNode                *tree,
                                                           guint                 directories_found,
                                                           guint                 directories_ignored,
                                                           guint                 files_found,
                                                           guint                 files_ignored,
                                                           gpointer              user_data);
static void           crawler_finished_cb                 (TrackerCrawler       *crawler,
                                                           gboolean              was_interrupted,
                                                           gpointer              user_data);
static void           crawl_directories_start             (TrackerMinerFS       *fs);
static void           crawl_directories_stop              (TrackerMinerFS       *fs);
static void           item_queue_handlers_set_up          (TrackerMinerFS       *fs);
static void           item_update_children_uri            (TrackerMinerFS       *fs,
                                                           RecursiveMoveData    *data,
                                                           const gchar          *source_uri,
                                                           const gchar          *uri);
static void           crawled_directory_data_free         (CrawledDirectoryData *data);

static gboolean       should_recurse_for_directory            (TrackerMinerFS *fs,
							       GFile          *file);
static void           tracker_miner_fs_directory_add_internal (TrackerMinerFS *fs,
							       GFile          *file);


static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_ABSTRACT_TYPE (TrackerMinerFS, tracker_miner_fs, TRACKER_TYPE_MINER)

static void
tracker_miner_fs_class_init (TrackerMinerFSClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerMinerFSClass *fs_class = TRACKER_MINER_FS_CLASS (klass);
	TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);

	object_class->finalize = fs_finalize;
	object_class->set_property = fs_set_property;
	object_class->get_property = fs_get_property;

	miner_class->started = miner_started;
	miner_class->stopped = miner_stopped;
	miner_class->paused  = miner_paused;
	miner_class->resumed = miner_resumed;
	miner_class->ignore_next_update = miner_ignore_next_update;

	fs_class->check_file        = fs_defaults;
	fs_class->check_directory   = fs_defaults;
	fs_class->monitor_directory = fs_defaults;
	fs_class->check_directory_contents = fs_contents_defaults;

	g_object_class_install_property (object_class,
	                                 PROP_THROTTLE,
	                                 g_param_spec_double ("throttle",
	                                                      "Throttle",
	                                                      "Modifier for the indexing speed, 0 is max speed",
	                                                      0, 1, 0,
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_POOL_LIMIT,
	                                 g_param_spec_uint ("process-pool-limit",
	                                                    "Processing pool limit",
	                                                    "Number of files that can be concurrently processed",
	                                                    1, G_MAXUINT, 1,
	                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * TrackerMinerFS::check-file:
	 * @miner_fs: the #TrackerMinerFS
	 * @file: a #GFile
	 *
	 * The ::check-file signal is emitted either on the filesystem crawling
	 * phase or whenever a new file appears in a monitored directory
	 * in order to check whether @file must be inspected my @miner_fs.
	 *
	 * Returns: %TRUE if @file must be inspected.
	 **/
	signals[CHECK_FILE] =
		g_signal_new ("check-file",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, check_file),
		              tracker_accumulator_check_file,
		              NULL,
		              tracker_marshal_BOOLEAN__OBJECT,
		              G_TYPE_BOOLEAN, 1, G_TYPE_FILE);
	/**
	 * TrackerMinerFS::check-directory:
	 * @miner_fs: the #TrackerMinerFS
	 * @directory: a #GFile
	 *
	 * The ::check-directory signal is emitted either on the filesystem crawling
	 * phase or whenever a new directory appears in a monitored directory
	 * in order to check whether @directory must be inspected my @miner_fs.
	 *
	 * Returns: %TRUE if @directory must be inspected.
	 **/
	signals[CHECK_DIRECTORY] =
		g_signal_new ("check-directory",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, check_directory),
		              tracker_accumulator_check_file,
		              NULL,
		              tracker_marshal_BOOLEAN__OBJECT,
		              G_TYPE_BOOLEAN, 1, G_TYPE_FILE);
	/**
	 * TrackerMinerFS::check-directory-contents:
	 * @miner_fs: the #TrackerMinerFS
	 * @directory: a #GFile
	 * @children: #GList of #GFile<!-- -->s
	 *
	 * The ::check-directory-contents signal is emitted either on the filesystem
	 * crawling phase or whenever a new directory appears in a monitored directory
	 * in order to check whether @directory must be inspected my @miner_fs based on
	 * the directory contents, for some implementations this signal may be useful
	 * to discard backup directories for example.
	 *
	 * Returns: %TRUE if @directory must be inspected.
	 **/
	signals[CHECK_DIRECTORY_CONTENTS] =
		g_signal_new ("check-directory-contents",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, check_directory_contents),
		              tracker_accumulator_check_file,
		              NULL,
		              tracker_marshal_BOOLEAN__OBJECT_POINTER,
		              G_TYPE_BOOLEAN, 2, G_TYPE_FILE, G_TYPE_POINTER);
	/**
	 * TrackerMinerFS::monitor-directory:
	 * @miner_fs: the #TrackerMinerFS
	 * @directory: a #GFile
	 *
	 * The ::monitor-directory is emitted either on the filesystem crawling phase
	 * or whenever a new directory appears in a monitored directory in order to
	 * check whether @directory must be monitored for filesystem changes or not.
	 *
	 * Returns: %TRUE if the directory must be monitored for changes.
	 **/
	signals[MONITOR_DIRECTORY] =
		g_signal_new ("monitor-directory",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, monitor_directory),
		              tracker_accumulator_check_file,
		              NULL,
		              tracker_marshal_BOOLEAN__OBJECT,
		              G_TYPE_BOOLEAN, 1, G_TYPE_FILE);
	/**
	 * TrackerMinerFS::process-file:
	 * @miner_fs: the #TrackerMinerFS
	 * @file: a #GFile
	 * @builder: a #TrackerSparqlBuilder
	 * @cancellable: a #GCancellable
	 *
	 * The ::process-file signal is emitted whenever a file should
	 * be processed, and it's metadata extracted.
	 *
	 * @builder is the #TrackerSparqlBuilder where all sparql updates
	 * to be performed for @file will be appended.
	 *
	 * This signal allows both synchronous and asynchronous extraction,
	 * in the synchronous case @cancellable can be safely ignored. In
	 * either case, on successful metadata extraction, implementations
	 * must call tracker_miner_fs_file_notify() to indicate that
	 * processing has finished on @file, so the miner can execute
	 * the SPARQL updates and continue processing other files.
	 *
	 * Returns: %TRUE if the file is accepted for processing,
	 *          %FALSE if the file should be ignored.
	 **/
	signals[PROCESS_FILE] =
		g_signal_new ("process-file",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, process_file),
		              NULL, NULL,
		              tracker_marshal_BOOLEAN__OBJECT_OBJECT_OBJECT,
		              G_TYPE_BOOLEAN,
		              3, G_TYPE_FILE, TRACKER_TYPE_SPARQL_BUILDER, G_TYPE_CANCELLABLE),

		/**
		 * TrackerMinerFS::ignore-next-update-file:
		 * @miner_fs: the #TrackerMinerFS
		 * @file: a #GFile
		 * @builder: a #TrackerSparqlBuilder
		 * @cancellable: a #GCancellable
		 *
		 * The ::ignore-next-update-file signal is emitted whenever a file should
		 * be marked as to ignore on next update, and it's metadata prepared for that.
		 *
		 * @builder is the #TrackerSparqlBuilder where all sparql updates
		 * to be performed for @file will be appended.
		 *
		 * Returns: %TRUE on success
		 *          %FALSE on failure
		 **/
		signals[IGNORE_NEXT_UPDATE_FILE] =
		g_signal_new ("ignore-next-update-file",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, ignore_next_update_file),
		              NULL, NULL,
		              tracker_marshal_BOOLEAN__OBJECT_OBJECT_OBJECT,
		              G_TYPE_BOOLEAN,
		              3, G_TYPE_FILE, TRACKER_TYPE_SPARQL_BUILDER, G_TYPE_CANCELLABLE),

		/**
		 * TrackerMinerFS::finished:
		 * @miner_fs: the #TrackerMinerFS
		 * @elapsed: elapsed time since mining was started
		 * @directories_found: number of directories found
		 * @directories_ignored: number of ignored directories
		 * @files_found: number of files found
		 * @files_ignored: number of ignored files
		 *
		 * The ::finished signal is emitted when @miner_fs has finished
		 * all pending processing.
		 **/
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

	priv->crawled_directories = g_queue_new ();
	priv->items_created = g_queue_new ();
	priv->items_updated = g_queue_new ();
	priv->items_deleted = g_queue_new ();
	priv->items_moved = g_queue_new ();
	priv->items_ignore_next_update = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                                        (GDestroyNotify) g_free,
	                                                        (GDestroyNotify) NULL);

	/* Set up the crawlers now we have config and hal */
	priv->crawler = tracker_crawler_new ();

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

	priv->iri_cache = g_hash_table_new_full (g_file_hash,
	                                         (GEqualFunc) g_file_equal,
	                                         (GDestroyNotify) g_object_unref,
	                                         (GDestroyNotify) g_free);
}

static ProcessData *
process_data_new (GFile                *file,
		  const gchar          *urn,
		  const gchar          *parent_urn,
                  GCancellable         *cancellable,
                  TrackerSparqlBuilder *builder)
{
	ProcessData *data;

	data = g_slice_new0 (ProcessData);
	data->file = g_object_ref (file);
	data->urn = g_strdup (urn);
	data->parent_urn = g_strdup (parent_urn);

	if (cancellable) {
		data->cancellable = g_object_ref (cancellable);
	}

	if (builder) {
		data->builder = g_object_ref (builder);
	}

	return data;
}

static void
process_data_free (ProcessData *data)
{
	g_object_unref (data->file);
	g_free (data->urn);
	g_free (data->parent_urn);

	if (data->cancellable) {
		g_object_unref (data->cancellable);
	}

	if (data->builder) {
		g_object_unref (data->builder);
	}

	g_slice_free (ProcessData, data);
}

static ProcessData *
process_data_find (TrackerMinerFS *fs,
                   GFile          *file)
{
	GList *l;

	for (l = fs->private->processing_pool; l; l = l->next) {
		ProcessData *data = l->data;

		/* Different operations for the same file URI could be
		 * piled up here, each being a different GFile object.
		 * Miner implementations should really notify on the
		 * same GFile object that's being passed, so we check for
		 * pointer equality here, rather than doing path comparisons
		 */
		if (data->file == file) {
			return data;
		}
	}

	return NULL;
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

	if (priv->current_parent)
		g_object_unref (priv->current_parent);

	g_free (priv->current_parent_urn);

	if (priv->directories) {
		g_list_foreach (priv->directories, (GFunc) directory_data_unref, NULL);
		g_list_free (priv->directories);
	}

	if (priv->config_directories) {
		g_list_foreach (priv->config_directories, (GFunc) directory_data_unref, NULL);
		g_list_free (priv->config_directories);
	}

	g_queue_foreach (priv->crawled_directories, (GFunc) crawled_directory_data_free, NULL);
	g_queue_free (priv->crawled_directories);

	g_list_foreach (priv->processing_pool, (GFunc) process_data_free, NULL);
	g_list_free (priv->processing_pool);

	g_queue_foreach (priv->items_moved, (GFunc) item_moved_data_free, NULL);
	g_queue_free (priv->items_moved);

	g_queue_foreach (priv->items_deleted, (GFunc) g_object_unref, NULL);
	g_queue_free (priv->items_deleted);

	g_queue_foreach (priv->items_updated, (GFunc) g_object_unref, NULL);
	g_queue_free (priv->items_updated);

	g_queue_foreach (priv->items_created, (GFunc) g_object_unref, NULL);
	g_queue_free (priv->items_created);

	g_hash_table_unref (priv->items_ignore_next_update);

	if (priv->mtime_cache) {
		g_hash_table_unref (priv->mtime_cache);
	}

	if (priv->iri_cache) {
		g_hash_table_unref (priv->iri_cache);
	}

	G_OBJECT_CLASS (tracker_miner_fs_parent_class)->finalize (object);
}

static void
fs_set_property (GObject      *object,
                 guint         prop_id,
                 const GValue *value,
                 GParamSpec   *pspec)
{
	TrackerMinerFS *fs = TRACKER_MINER_FS (object);

	switch (prop_id) {
	case PROP_THROTTLE:
		tracker_miner_fs_set_throttle (TRACKER_MINER_FS (object),
		                               g_value_get_double (value));
		break;
	case PROP_POOL_LIMIT:
		fs->private->pool_limit = g_value_get_uint (value);
		g_message ("Miner process pool limit is set to %d", fs->private->pool_limit);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
fs_get_property (GObject    *object,
                 guint       prop_id,
                 GValue     *value,
                 GParamSpec *pspec)
{
	TrackerMinerFS *fs;

	fs = TRACKER_MINER_FS (object);

	switch (prop_id) {
	case PROP_THROTTLE:
		g_value_set_double (value, fs->private->throttle);
		break;
	case PROP_POOL_LIMIT:
		g_value_set_uint (value, fs->private->pool_limit);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gboolean
fs_defaults (TrackerMinerFS *fs,
             GFile          *file)
{
	return TRUE;
}

static gboolean
fs_contents_defaults (TrackerMinerFS *fs,
                      GFile          *parent,
                      GList          *children)
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
	              "status", "Initializing",
	              NULL);

	crawl_directories_start (fs);
}

static void
miner_stopped (TrackerMiner *miner)
{
	g_object_set (miner,
	              "progress", 1.0,
	              "status", "Idle",
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


static void
miner_ignore_next_update (TrackerMiner *miner, const GStrv urls)
{
	TrackerMinerFS *fs;
	guint n;

	fs = TRACKER_MINER_FS (miner);

	for (n = 0; urls[n] != NULL; n++) {
		g_hash_table_insert (fs->private->items_ignore_next_update,
		                     g_strdup (urls[n]),
		                     GINT_TO_POINTER (TRUE));
	}

	item_queue_handlers_set_up (fs);
}


static DirectoryData *
directory_data_new (GFile    *file,
                    gboolean  recurse)
{
	DirectoryData *dd;

	dd = g_slice_new (DirectoryData);

	dd->file = g_object_ref (file);
	dd->recurse = recurse;
	dd->ref_count = 1;

	return dd;
}

static DirectoryData *
directory_data_ref (DirectoryData *dd)
{
	dd->ref_count++;
	return dd;
}

static void
directory_data_unref (DirectoryData *dd)
{
	if (!dd) {
		return;
	}

	dd->ref_count--;

	if (dd->ref_count == 0) {
		g_object_unref (dd->file);
		g_slice_free (DirectoryData, dd);
	}
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
		g_message ("Total processed   : %d (%d notified, %d with error)",
			   fs->private->total_files_processed,
			   fs->private->total_files_notified,
			   fs->private->total_files_notified_error);
		g_message ("--------------------------------------------------\n");
	}
}

static void
commit_cb (GObject      *object,
           GAsyncResult *result,
           gpointer      user_data)
{
	TrackerMiner *miner = TRACKER_MINER (object);
	GError *error = NULL;

	tracker_miner_commit_finish (miner, result, &error);

	if (error) {
		g_critical ("Could not commit: %s", error->message);
		g_error_free (error);
	}
}

static void
process_stop (TrackerMinerFS *fs)
{
	/* Now we have finished crawling, print stats and enable monitor events */
	process_print_stats (fs);

	tracker_miner_commit (TRACKER_MINER (fs), NULL, commit_cb, NULL);

	g_message ("Idle");

	g_object_set (fs,
	              "progress", 1.0,
	              "status", "Idle",
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
sparql_update_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
	TrackerMinerFS *fs;
	TrackerMinerFSPrivate *priv;
	ProcessData *data;
	GError *error = NULL;

	tracker_miner_execute_update_finish (TRACKER_MINER (object), result, &error);

	fs = TRACKER_MINER_FS (object);
	priv = fs->private;
	data = user_data;

	if (error) {
		g_critical ("Could not execute sparql: %s", error->message);
		priv->total_files_notified_error++;
		g_error_free (error);
	} else {
		if (fs->private->been_crawled) {
			/* Only commit immediately for
			 * changes after initial crawling.
			 */
			tracker_miner_commit (TRACKER_MINER (fs), NULL, commit_cb, NULL);
		}

		if (fs->private->current_parent) {
			GFile *parent;

			parent = g_file_get_parent (data->file);

			if (g_file_equal (parent, fs->private->current_parent) &&
			    g_hash_table_lookup (fs->private->iri_cache, data->file) == NULL) {
				/* Item is processed, add an empty element for the processed GFile,
				 * in case it is again processed before the cache expires
				 */
				g_hash_table_insert (fs->private->iri_cache, g_object_ref (data->file), NULL);
			}

			g_object_unref (parent);
		}
	}

	priv->processing_pool = g_list_remove (priv->processing_pool, data);
	process_data_free (data);

	item_queue_handlers_set_up (fs);
}

static void
sparql_query_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	SparqlQueryData *data = user_data;
	const GPtrArray *query_results;
	TrackerMiner *miner;
	GError *error = NULL;

	miner = TRACKER_MINER (object);
	query_results = tracker_miner_execute_sparql_finish (miner, result, &error);

	g_main_loop_quit (data->main_loop);

	if (error) {
		g_critical ("Could not execute sparql query: %s", error->message);
		g_error_free (error);
		return;
	}

	if (!query_results ||
	    query_results->len == 0)
		return;

	if (query_results->len == 1) {
		gchar **val;

		val = g_ptr_array_index (query_results, 0);
		data->iri = g_strdup (val[0]);
		if (data->get_mime)
			data->mime = g_strdup (val[1]);
	} else {
		g_critical ("More than one URNs (%d) have been found for uri \"%s\"", query_results->len, data->uri);
	}
}

static gboolean
item_query_exists (TrackerMinerFS  *miner,
                   GFile           *file,
                   gchar          **iri,
                   gchar          **mime)
{
	gboolean   result;
	gchar     *sparql, *uri;
	SparqlQueryData data = { 0 };

	data.get_mime = (mime != NULL);

	uri = g_file_get_uri (file);

	if (data.get_mime) {
		sparql = g_strdup_printf ("SELECT ?s nie:mimeType(?s) WHERE { ?s nie:url \"%s\" }", uri);
	} else {
		sparql = g_strdup_printf ("SELECT ?s WHERE { ?s nie:url \"%s\" }", uri);
	}

	data.main_loop = g_main_loop_new (NULL, FALSE);
	data.uri = uri;

	tracker_miner_execute_sparql (TRACKER_MINER (miner),
	                              sparql,
	                              NULL,
	                              sparql_query_cb,
	                              &data);

	g_main_loop_run (data.main_loop);
	result = (data.iri != NULL);

	g_main_loop_unref (data.main_loop);

	if (iri) {
		*iri = data.iri;
	} else {
		g_free (data.iri);
	}

	if (mime) {
		*mime = data.mime;
	} else {
		g_free (data.mime);
	}

	g_free (sparql);
	g_free (uri);

	return result;
}

static void
cache_query_cb (GObject	     *object,
		GAsyncResult *result,
		gpointer      user_data)
{
	const GPtrArray *query_results;
	TrackerMiner *miner;
	CacheQueryData *data;
	GError *error = NULL;
	guint i;

	data = user_data;
	miner = TRACKER_MINER (object);
	query_results = tracker_miner_execute_sparql_finish (miner, result, &error);

	g_main_loop_quit (data->main_loop);

	if (G_UNLIKELY (error)) {
		g_critical ("Could not execute cache query: %s", error->message);
		g_error_free (error);
		return;
	}

	for (i = 0; i < query_results->len; i++) {
		GFile *file;
		GStrv strv;

		strv = g_ptr_array_index (query_results, i);
		file = g_file_new_for_uri (strv[0]);

		g_hash_table_insert (data->values, file, g_strdup (strv[1]));
	}
}

static void
ensure_iri_cache (TrackerMinerFS *fs,
                  GFile          *parent)
{
	gchar *query, *uri;
	CacheQueryData data;

	g_hash_table_remove_all (fs->private->iri_cache);

	uri = g_file_get_uri (parent);

	g_debug ("Generating IRI cache for folder: %s", uri);

	query = g_strdup_printf ("SELECT ?url ?u { "
	                         "  ?u nfo:belongsToContainer ?p ; "
	                         "     nie:url ?url . "
	                         "  ?p nie:url \"%s\" "
	                         "}",
	                         uri);
	g_free (uri);

	data.main_loop = g_main_loop_new (NULL, FALSE);
	data.values = g_hash_table_ref (fs->private->iri_cache);

	tracker_miner_execute_sparql (TRACKER_MINER (fs),
	                              query,
	                              NULL,
	                              cache_query_cb,
	                              &data);

	g_main_loop_run (data.main_loop);

	g_main_loop_unref (data.main_loop);
	g_hash_table_unref (data.values);
	g_free (query);
}

static const gchar *
iri_cache_lookup (TrackerMinerFS *fs,
                  GFile          *file)
{
	gpointer value;
	const gchar *iri;

	if (!g_hash_table_lookup_extended (fs->private->iri_cache, file, NULL, &value)) {
		/* Item doesn't exist in cache */
		return NULL;
	}

	iri = value;

	if (!iri) {
		gchar *query_iri;

		/* Cache miss, this item was added after the last
		 * iri cache update, so query it independently
		 */
		if (item_query_exists (fs, file, &query_iri, NULL)) {
			g_hash_table_insert (fs->private->iri_cache,
			                     g_object_ref (file), query_iri);
		} else {
			g_hash_table_remove (fs->private->iri_cache, file);
			iri = NULL;
		}
	}

	return iri;
}

static void
iri_cache_invalidate (TrackerMinerFS *fs,
                      GFile          *file)
{
	g_hash_table_remove (fs->private->iri_cache, file);
}

static gboolean
do_process_file (TrackerMinerFS *fs,
		 ProcessData    *data)
{
	TrackerMinerFSPrivate *priv;
	gboolean processing;

	priv = fs->private;

	g_signal_emit (fs, signals[PROCESS_FILE], 0,
	               data->file, data->builder, data->cancellable,
	               &processing);

	if (!processing) {
		gchar *uri;

		uri = g_file_get_uri (data->file);

		/* Re-fetch data, since it might have been
		 * removed in broken implementations
		 */
		data = process_data_find (fs, data->file);

		g_message ("%s refused to process '%s'", G_OBJECT_TYPE_NAME (fs), uri);

		if (!data) {
			g_critical ("%s has returned FALSE in ::process-file for '%s', "
			            "but it seems that this file has been processed through "
			            "tracker_miner_fs_file_notify(), this is an "
			            "implementation error", G_OBJECT_TYPE_NAME (fs), uri);
		} else {
			priv->processing_pool = g_list_remove (priv->processing_pool, data);
			process_data_free (data);
		}

		g_free (uri);
	}

	return processing;
}

static void
item_add_or_update_cb (TrackerMinerFS *fs,
                       ProcessData    *data,
                       const GError   *error)
{
	gchar *uri;

	uri = g_file_get_uri (data->file);

	if (error) {
		ProcessData *first_item_data;
		GList *last;

		last = g_list_last (fs->private->processing_pool);
		first_item_data = last->data;

		/* Perhaps this is too specific to TrackerMinerFiles, if the extractor
		 * is choking on some file, the miner will get a timeout for all files
		 * being currently processed, but the one that is actually causing it
		 * is the first one that was added to the processing pool, so we retry
		 * the others.
		 */
		if (data != first_item_data &&
		    (error->code == DBUS_GERROR_NO_REPLY ||
		     error->code == DBUS_GERROR_TIMEOUT ||
		     error->code == DBUS_GERROR_TIMED_OUT)) {
			g_debug ("  Got DBus timeout error on '%s', but it could not be caused by it. Retrying file.", uri);

			/* Reset the TrackerSparqlBuilder */
			g_object_unref (data->builder);
			data->builder = tracker_sparql_builder_new_update ();

			do_process_file (fs, data);
		} else {
                        g_message ("Could not process '%s': %s", uri, error->message);

			fs->private->total_files_notified_error++;
			fs->private->processing_pool =
				g_list_remove (fs->private->processing_pool, data);
			process_data_free (data);

			item_queue_handlers_set_up (fs);
		}
	} else {
		gchar *full_sparql;

		g_debug ("Adding item '%s'", uri);

		full_sparql = g_strdup_printf ("DROP GRAPH <%s> %s",
		                               uri, tracker_sparql_builder_get_result (data->builder));

		tracker_miner_execute_batch_update (TRACKER_MINER (fs),
		                                    full_sparql,
		                                    NULL,
		                                    sparql_update_cb,
		                                    data);
		g_free (full_sparql);
	}

	g_free (uri);
}

static gboolean
item_add_or_update (TrackerMinerFS *fs,
                    GFile          *file)
{
	TrackerMinerFSPrivate *priv;
	TrackerSparqlBuilder *sparql;
	GCancellable *cancellable;
	gboolean retval;
	ProcessData *data;
	GFile *parent;
	const gchar *urn;
	const gchar *parent_urn = NULL;

	priv = fs->private;
	retval = TRUE;

	cancellable = g_cancellable_new ();
	sparql = tracker_sparql_builder_new_update ();
	g_object_ref (file);

	parent = g_file_get_parent (file);

	if (parent) {
		if (!fs->private->current_parent ||
		    !g_file_equal (parent, fs->private->current_parent)) {
			/* Cache the URN for the new current parent, processing
			 * order guarantees that all contents for a folder are
			 * inspected together, and that the parent folder info
			 * is already in tracker-store. So this should only
			 * happen on folder switch.
			 */
			if (fs->private->current_parent)
				g_object_unref (fs->private->current_parent);

			g_free (fs->private->current_parent_urn);

			fs->private->current_parent = g_object_ref (parent);

			if (!item_query_exists (fs, parent, &fs->private->current_parent_urn, NULL)) {
				fs->private->current_parent_urn = NULL;
			}

			ensure_iri_cache (fs, parent);
		}

		parent_urn = fs->private->current_parent_urn;
		g_object_unref (parent);
	}

	urn = iri_cache_lookup (fs, file);

	data = process_data_new (file, urn, parent_urn, cancellable, sparql);
	priv->processing_pool = g_list_prepend (priv->processing_pool, data);

	if (do_process_file (fs, data)) {
		guint length;

		length = g_list_length (priv->processing_pool);

		fs->private->total_files_processed++;

		if (length >= priv->pool_limit) {
			retval = FALSE;
		}
	}

	g_object_unref (file);
	g_object_unref (cancellable);
	g_object_unref (sparql);

	return retval;
}

static gboolean
item_remove (TrackerMinerFS *fs,
             GFile          *file)
{
	GString *sparql;
	gchar *uri;
	gchar *mime = NULL;
	ProcessData *data;

	iri_cache_invalidate (fs, file);
	uri = g_file_get_uri (file);

	g_debug ("Removing item: '%s' (Deleted from filesystem)",
	         uri);

	if (!item_query_exists (fs, file, NULL, &mime)) {
		g_debug ("  File does not exist anyway (uri:'%s')", uri);
		g_free (uri);
		g_free (mime);
		return TRUE;
	}

	tracker_thumbnailer_remove_add (uri, mime);

	g_free (mime);

	sparql = g_string_new ("");

	/* Delete all children */
	g_string_append_printf (sparql,
	                        "DELETE { "
	                        "  ?child a rdfs:Resource "
	                        "} WHERE {"
	                        "  ?child nie:url ?u . "
	                        "  FILTER (tracker:uri-is-descendant (\"%s\", ?u)) "
	                        "}",
	                        uri);

	/* Delete resource itself */
	g_string_append_printf (sparql,
	                        "DELETE { "
	                        "  ?u a rdfs:Resource "
	                        "} WHERE { "
	                        "  ?u nie:url \"%s\" "
	                        "}",
	                        uri);

	data = process_data_new (file, NULL, NULL, NULL, NULL);
	fs->private->processing_pool = g_list_prepend (fs->private->processing_pool, data);

	tracker_miner_execute_batch_update (TRACKER_MINER (fs),
	                                    sparql->str,
	                                    NULL,
	                                    sparql_update_cb,
	                                    data);

	g_string_free (sparql, TRUE);
	g_free (uri);

	return FALSE;
}

static gboolean
item_ignore_next_update (TrackerMinerFS *fs,
                         GFile          *file,
                         GFile          *source_file)
{
	TrackerSparqlBuilder *sparql;
	gchar *uri;
	gboolean success = FALSE;
	GCancellable *cancellable;
	GFile *working_file;

	/* While we are in ignore-on-next-update:
	 * o. We always ignore deletes because it's never the final operation
	 *    of a write. We have a delete when both are null.
	 * o. A create means the write used rename(). This is the final
	 *    operation of a write and thus we make the update query.
	 *    We have a create when file == null and source_file != null
	 * o. A move means the write used rename(). This is the final
	 *    operation of a write and thus we make the update query.
	 *    We have a move when both file and source_file aren't null.
	 * o. A update means the write didn't use rename(). This is the
	 *    final operation of a write and thus we make the update query.
	 *    An update means that file != null and source_file == null. */

	/* Happens on delete while in write */
	if (!file && !source_file) {
		return TRUE;
	}

	/* Create or update, we are the final one so we make the update query */

	if (!file && source_file) {
		/* Happens on create while in write */
		working_file = source_file;
	} else {
		/* Happens on update while in write */
		working_file = file;
	}

	uri = g_file_get_uri (working_file);

	g_debug ("Updating item: '%s' (IgnoreNextUpdate event)", uri);

	cancellable = g_cancellable_new ();
	sparql = tracker_sparql_builder_new_update ();
	g_object_ref (working_file);

	/* IgnoreNextUpdate */
	g_signal_emit (fs, signals[IGNORE_NEXT_UPDATE_FILE], 0,
	               working_file, sparql, cancellable, &success);

	if (success) {
		gchar *query;

		/* Perhaps we should move the DELETE to tracker-miner-files.c?
		 * Or we add support for DELETE to TrackerSparqlBuilder ofcrs */

		query = g_strdup_printf ("DELETE { "
		                         "  ?u nfo:fileSize ?unknown1 ; "
		                         "     nfo:fileLastModified ?unknown2 ; "
		                         "     nfo:fileLastAccessed ?unknown3 ; "
		                         "     nie:mimeType ?unknown4 "
		                         "} WHERE { "
		                         "  ?u nfo:fileSize ?unknown1 ; "
		                         "     nfo:fileLastModified ?unknown2 ; "
		                         "     nfo:fileLastAccessed ?unknown3 ; "
		                         "     nie:mimeType ?unknown4 ; "
		                         "     nie:url \"%s\" "
		                         "} %s",
		                         uri, tracker_sparql_builder_get_result (sparql));

		tracker_miner_execute_batch_update (TRACKER_MINER (fs),
		                                    query,
		                                    NULL, NULL, NULL);

		g_free (query);
	}

	g_hash_table_remove (fs->private->items_ignore_next_update, uri);

	g_object_unref (sparql);
	g_object_unref (working_file);
	g_object_unref (cancellable);

	g_free (uri);

	return FALSE;
}

static void
item_update_children_uri_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
	RecursiveMoveData *data = user_data;
	GError *error = NULL;

	const GPtrArray *query_results = tracker_miner_execute_sparql_finish (TRACKER_MINER (object), result, &error);

	if (error) {
		g_critical ("Could not query children: %s", error->message);
		g_error_free (error);
	} else if (query_results) {
		gint i;

		for (i = 0; i < query_results->len; i++) {
			const gchar *child_source_uri, *child_mime, *child_urn;
			gchar *child_uri;
			GStrv row;

			row = g_ptr_array_index (query_results, i);
			child_urn = row[0];
			child_source_uri = row[1];
			child_mime = row[2];

			if (!g_str_has_prefix (child_source_uri, data->source_uri)) {
				g_warning ("Child URI '%s' does not start with parent URI '%s'",
					   child_source_uri,
					   data->source_uri);
				continue;
			}

			child_uri = g_strdup_printf ("%s%s", data->uri, child_source_uri + strlen (data->source_uri));

			g_string_append_printf (data->sparql,
			                        "DELETE FROM <%s> { "
			                        "  <%s> nie:url ?u "
			                        "} WHERE { "
			                        "  <%s> nie:url ?u "
			                        "} ",
			                        child_urn, child_urn, child_urn);

			g_string_append_printf (data->sparql,
			                        "INSERT INTO <%s> {"
			                        "  <%s> nie:url \"%s\" "
			                        "} ",
			                        child_urn, child_urn, child_uri);

			tracker_thumbnailer_move_add (child_source_uri, child_mime, child_uri);

			g_free (child_uri);
		}
	}

	g_main_loop_quit (data->main_loop);
}

static void
item_update_children_uri (TrackerMinerFS    *fs,
                          RecursiveMoveData *move_data,
                          const gchar       *source_uri,
                          const gchar       *uri)
{
	gchar *sparql;

	sparql = g_strdup_printf ("SELECT ?child ?url nie:mimeType(?child) WHERE { "
				  "  ?child nie:url ?url . "
                                  "  FILTER (tracker:uri-is-descendant (\"%s\", ?url)) "
				  "}",
				  source_uri);

	tracker_miner_execute_sparql (TRACKER_MINER (fs),
	                              sparql,
	                              NULL,
	                              item_update_children_uri_cb,
	                              move_data);

	g_free (sparql);
}

static gboolean
item_move (TrackerMinerFS *fs,
           GFile          *file,
           GFile          *source_file)
{
	gchar     *uri, *source_uri, *escaped_filename;
	GFileInfo *file_info;
	GString   *sparql;
	RecursiveMoveData move_data;
	ProcessData *data;
	gchar *source_iri;
	gboolean source_exists;

	iri_cache_invalidate (fs, file);
	iri_cache_invalidate (fs, source_file);

	uri = g_file_get_uri (file);
	source_uri = g_file_get_uri (source_file);

	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
	                               G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				       G_FILE_ATTRIBUTE_STANDARD_TYPE,
	                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                               NULL, NULL);

	/* Get 'source' ID */
	source_exists = item_query_exists (fs, source_file, &source_iri, NULL);

	if (!file_info) {
		gboolean retval;

		if (source_exists) {
			/* Destination file has gone away, ignore dest file and remove source if any */
			retval = item_remove (fs, source_file);
		} else {
			/* Destination file went away, and source wasn't indexed either */
			retval = TRUE;
		}

		g_free (source_iri);
		g_free (source_uri);
		g_free (uri);

		return retval;
	} else if (file_info && !source_exists) {
		gboolean retval;
		GFileType file_type;

		g_message ("Source file '%s' not found in store to move, indexing '%s' from scratch", source_uri, uri);

		file_type = g_file_info_get_file_type (file_info);

		if (file_type == G_FILE_TYPE_DIRECTORY &&
		    should_recurse_for_directory (fs, file)) {
			/* We're dealing with a recursive directory */
			tracker_miner_fs_directory_add_internal (fs, file);
			retval = TRUE;
		} else {
			retval = item_add_or_update (fs, file);
		}

		g_free (source_uri);
		g_free (uri);

		return retval;
	}

	g_debug ("Moving item from '%s' to '%s'",
	         source_uri,
	         uri);

	tracker_thumbnailer_move_add (source_uri, 
	                              g_file_info_get_content_type (file_info),
	                              uri);

	sparql = g_string_new ("");

	/* Delete destination item from store if any */
	g_string_append_printf (sparql,
				"DELETE { "
	                        "  ?urn a rdfs:Resource "
	                        "} WHERE {"
	                        "  ?urn nie:url \"%s\" "
				"}",
				uri);

	g_string_append_printf (sparql,
	                        "DELETE FROM <%s> { "
	                        "  <%s> nfo:fileName ?f ; "
	                        "       nie:url ?u ; "
	                        "       nie:isStoredAs ?s "
	                        "} WHERE { "
	                        "  <%s> nfo:fileName ?f ; "
	                        "       nie:url ?u ; "
	                        "       nie:isStoredAs ?s "
	                        "} ",
	                        source_iri, source_iri, source_iri);

	escaped_filename = g_strescape (g_file_info_get_display_name (file_info), NULL);

	g_string_append_printf (sparql,
	                        "INSERT INTO <%s> {"
	                        "  <%s> nfo:fileName \"%s\" ; "
	                        "       nie:url \"%s\" ; "
	                        "       nie:isStoredAs <%s> "
	                        "} ",
	                        source_iri, source_iri,
	                        escaped_filename, uri,
	                        source_iri);

	g_free (escaped_filename);

	move_data.main_loop = g_main_loop_new (NULL, FALSE);
	move_data.sparql = sparql;
	move_data.source_uri = source_uri;
	move_data.uri = uri;

	item_update_children_uri (fs, &move_data, source_uri, uri);

	g_main_loop_run (move_data.main_loop);

	g_main_loop_unref (move_data.main_loop);

	data = process_data_new (file, NULL, NULL, NULL, NULL);
	fs->private->processing_pool = g_list_prepend (fs->private->processing_pool, data);

	tracker_miner_execute_batch_update (TRACKER_MINER (fs),
	                                    sparql->str,
	                                    NULL,
	                                    sparql_update_cb,
	                                    data);

	g_free (uri);
	g_free (source_uri);
	g_object_unref (file_info);
	g_string_free (sparql, TRUE);
	g_free (source_iri);

	return TRUE;
}

static gboolean
check_ignore_next_update (TrackerMinerFS *fs, GFile *queue_file)
{
	gchar *uri = g_file_get_uri (queue_file);
	if (g_hash_table_lookup (fs->private->items_ignore_next_update, uri)) {
		g_free (uri);
		return TRUE;
	}
	g_free (uri);
	return FALSE;
}

static void
fill_in_queue (TrackerMinerFS       *fs,
	       GQueue               *queue)
{
	CrawledDirectoryData *dir_data;
	GList *l, *post_nodes = NULL;
	GFile *file;
	GNode *node;

	dir_data = g_queue_peek_head (fs->private->crawled_directories);

	if (g_queue_is_empty (dir_data->nodes)) {
		/* Special case, append the root directory for the tree */
		node = dir_data->tree;
		file = node->data;
		dir_data->n_items_processed++;

		g_queue_push_tail (dir_data->nodes, node);

		if (!g_object_get_qdata (G_OBJECT (file), fs->private->quark_ignore_file)) {
			g_queue_push_tail (queue, g_object_ref (file));
			return;
		}
	}

	node = g_queue_pop_head (dir_data->nodes);

	/* There are nodes in the middle of processing. Append
	 * items to the queue, an add directories to post_nodes,
	 * so they can be processed later on.
	 */
	while (node) {
		GNode *children;
		gchar *uri;

		children = node->children;

		uri = g_file_get_uri (node->data);
		g_message ("Adding files from directory '%s' into the processing queue", uri);
		g_free (uri);

		while (children) {
			file = children->data;
			dir_data->n_items_processed++;

			if (!g_object_get_qdata (G_OBJECT (file), fs->private->quark_ignore_file)) {
				g_queue_push_tail (queue, g_object_ref (file));
			}

			if (children->children) {
				post_nodes = g_list_prepend (post_nodes, children);
			}

			children = children->next;
		}

		node = g_queue_pop_head (dir_data->nodes);
	}

	/* Children collected in post_nodes will be
	 * the ones processed on the next iteration
	 */
	for (l = post_nodes; l; l = l->next) {
		g_queue_push_tail (dir_data->nodes, l->data);
	}

	g_list_free (post_nodes);

	if (g_queue_is_empty (dir_data->nodes)) {
		/* There's no more data to process, move on to the next one */
		g_queue_pop_head (fs->private->crawled_directories);
		crawled_directory_data_free (dir_data);
	}
}

static QueueState
item_queue_get_next_file (TrackerMinerFS  *fs,
                          GFile          **file,
                          GFile          **source_file)
{
	ItemMovedData *data;
	GFile *queue_file;

	/* Deleted items first */
	queue_file = g_queue_pop_head (fs->private->items_deleted);
	if (queue_file) {
		*source_file = NULL;
		if (check_ignore_next_update (fs, queue_file)) {
			*file = NULL;
			return QUEUE_IGNORE_NEXT_UPDATE;
		}
		*file = queue_file;
		return QUEUE_DELETED;
	}

	if (g_queue_is_empty (fs->private->items_created) &&
	    !g_queue_is_empty (fs->private->crawled_directories)) {
		/* The items_created queue is empty, but there are pending
		 * items from the crawler to be processed. We feed the queue
		 * in this manner so it's ensured that the parent directory
		 * info is inserted to the store before the children are
		 * inspected.
		 */
		if (fs->private->processing_pool) {
			/* Items still being processed */
			*file = NULL;
			*source_file = NULL;
			return QUEUE_WAIT;
		} else {
			/* Iterate through all directory hierarchies until
			 * one of these return something for the miner to do,
			 * or no data is left to process.
			 */
			while (g_queue_is_empty (fs->private->items_created) &&
			       !g_queue_is_empty (fs->private->crawled_directories)) {
				fill_in_queue (fs, fs->private->items_created);
			}
		}
	}

	/* Created items next */
	queue_file = g_queue_pop_head (fs->private->items_created);
	if (queue_file) {
		if (check_ignore_next_update (fs, queue_file)) {
			*file = NULL;
			*source_file = queue_file;
			return QUEUE_IGNORE_NEXT_UPDATE;
		}
		*file = queue_file;
		*source_file = NULL;
		return QUEUE_CREATED;
	}

	/* Updated items next */
	queue_file = g_queue_pop_head (fs->private->items_updated);
	if (queue_file) {
		*file = queue_file;
		*source_file = NULL;
		if (check_ignore_next_update (fs, queue_file))
			return QUEUE_IGNORE_NEXT_UPDATE;
		return QUEUE_UPDATED;
	}

	/* Moved items next */
	data = g_queue_pop_head (fs->private->items_moved);
	if (data) {
		*file = g_object_ref (data->file);
		*source_file = g_object_ref (data->source_file);
		item_moved_data_free (data);
		if (check_ignore_next_update (fs, *file))
			return QUEUE_IGNORE_NEXT_UPDATE;
		return QUEUE_MOVED;
	}

	*file = NULL;
	*source_file = NULL;

	return QUEUE_NONE;
}

static void
get_tree_progress_foreach (CrawledDirectoryData *data,
			   gint                 *items_to_process)
{
	*items_to_process += data->n_items - data->n_items_processed;
}

static gdouble
item_queue_get_progress (TrackerMinerFS *fs,
                         guint          *n_items_processed,
                         guint          *n_items_remaining)
{
	guint items_to_process = 0;
	guint items_total = 0;

	items_to_process += g_queue_get_length (fs->private->items_deleted);
	items_to_process += g_queue_get_length (fs->private->items_created);
	items_to_process += g_queue_get_length (fs->private->items_updated);
	items_to_process += g_queue_get_length (fs->private->items_moved);

	g_queue_foreach (fs->private->crawled_directories,
			 (GFunc) get_tree_progress_foreach,
			 &items_to_process);

	items_total += fs->private->total_directories_found;
	items_total += fs->private->total_files_found;

	if (n_items_processed) {
		*n_items_processed = items_total - items_to_process;
	}

	if (n_items_remaining) {
		*n_items_remaining = items_to_process;
	}

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
	QueueState queue;
	GTimeVal time_now;
	static GTimeVal time_last = { 0 };
	gboolean keep_processing = TRUE;

	fs = user_data;
	queue = item_queue_get_next_file (fs, &file, &source_file);

	if (queue == QUEUE_WAIT) {
		/* Items are still being processed, and there is pending
		 * data in priv->crawled_directories, so wait until
		 * the processing pool is cleared before starting with
		 * the next directories batch.
		 */
		fs->private->item_queues_handler_id = 0;
		return FALSE;
	}

	if (file && queue != QUEUE_DELETED &&
	    tracker_file_is_locked (file)) {
		/* File is locked, ignore any updates on it */
		g_object_unref (file);

		if (source_file) {
			g_object_unref (source_file);
		}

		return TRUE;
	}

	if (!fs->private->timer) {
		fs->private->timer = g_timer_new ();
	}

	/* Update progress, but don't spam it. */
	g_get_current_time (&time_now);

	if ((time_now.tv_sec - time_last.tv_sec) >= 1) {
		guint items_processed, items_remaining;
		gdouble progress_now;
		static gdouble progress_last = 0.0;
                static gint info_last = 0;

		time_last = time_now;

		/* Update progress */
		progress_now = item_queue_get_progress (fs,
		                                        &items_processed,
		                                        &items_remaining);
		g_object_set (fs, "progress", progress_now, NULL);

                if (++info_last >= 5 &&
                    (gint) (progress_last * 100) != (gint) (progress_now * 100)) {
			gchar *str1, *str2;
			gdouble seconds_elapsed;

                        info_last = 0;
			progress_last = progress_now;

			/* Log estimated remaining time */
			seconds_elapsed = g_timer_elapsed (fs->private->timer, NULL);
			str1 = tracker_seconds_estimate_to_string (seconds_elapsed,
			                                           TRUE,
			                                           items_processed,
			                                           items_remaining);
			str2 = tracker_seconds_to_string (seconds_elapsed, TRUE);

			tracker_info ("Processed %d/%d, estimated %s left, %s elapsed",
			              items_processed,
			              items_processed + items_remaining,
			              str1,
			              str2);

			g_free (str2);
			g_free (str1);
		}
	}

	/* Handle queues */
	switch (queue) {
	case QUEUE_NONE:
		/* Print stats and signal finished */
		if (!fs->private->is_crawling &&
		    !fs->private->processing_pool) {
			process_stop (fs);
		}

		tracker_thumbnailer_send ();
		/* No more files left to process */
		keep_processing = FALSE;
		break;
	case QUEUE_MOVED:
		keep_processing = item_move (fs, file, source_file);
		break;
	case QUEUE_DELETED:
		keep_processing = item_remove (fs, file);
		break;
	case QUEUE_CREATED:
	case QUEUE_UPDATED:
		keep_processing = item_add_or_update (fs, file);
		break;
	case QUEUE_IGNORE_NEXT_UPDATE:
		keep_processing = item_ignore_next_update (fs, file, source_file);
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
			tracker_miner_commit (TRACKER_MINER (fs), NULL, commit_cb, NULL);
		}

		return TRUE;
	}
}

static guint
_tracker_idle_add (TrackerMinerFS *fs,
                   GSourceFunc     func,
                   gpointer        user_data)
{
	guint interval;

	interval = MAX_TIMEOUT_INTERVAL * fs->private->throttle;

	if (interval == 0) {
		return g_idle_add (func, user_data);
	} else {
		return g_timeout_add (interval, func, user_data);
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

	if (g_list_length (fs->private->processing_pool) >= fs->private->pool_limit) {
		/* There is no room in the pool for more files */
		return;
	}

	g_object_get (fs, "status", &status, NULL);

	if (g_strcmp0 (status, "Processing…") != 0) {
		/* Don't spam this */
		g_message ("Processing…");
		g_object_set (fs, "status", "Processing…", NULL);
	}

	g_free (status);

	fs->private->item_queues_handler_id =
		_tracker_idle_add (fs,
		                   item_queue_handlers_cb,
		                   fs);
}

static void
ensure_mtime_cache (TrackerMinerFS *fs,
                    GFile          *file)
{
	gchar *query, *uri;
	CacheQueryData data;
	GFile *parent;

	if (G_UNLIKELY (!fs->private->mtime_cache)) {
		fs->private->mtime_cache = g_hash_table_new_full (g_file_hash,
		                                                  (GEqualFunc) g_file_equal,
		                                                  (GDestroyNotify) g_object_unref,
		                                                  (GDestroyNotify) g_free);
	}

	parent = g_file_get_parent (file);

	if (fs->private->current_parent) {
		if (g_file_equal (parent, fs->private->current_parent)) {
			/* Cache is still valid */
			g_object_unref (parent);
			return;
		}

		g_object_unref (fs->private->current_parent);
	}

	fs->private->current_parent = parent;

	g_hash_table_remove_all (fs->private->mtime_cache);

	uri = g_file_get_uri (parent);

	g_debug ("Generating mtime cache for folder: %s", uri);

	query = g_strdup_printf ("SELECT ?url ?last { ?u nfo:belongsToContainer ?p ; "
	                                                "nie:url ?url ; "
	                                                "nfo:fileLastModified ?last . "
	                                             "?p nie:url \"%s\" }", uri);

	g_free (uri);

	data.main_loop = g_main_loop_new (NULL, FALSE);
	data.values = g_hash_table_ref (fs->private->mtime_cache);

	tracker_miner_execute_sparql (TRACKER_MINER (fs),
	                              query,
	                              NULL,
	                              cache_query_cb,
	                              &data);

	g_main_loop_run (data.main_loop);

	g_main_loop_unref (data.main_loop);
	g_hash_table_unref (data.values);
	g_free (query);
}

static gboolean
should_change_index_for_file (TrackerMinerFS *fs,
                              GFile          *file)
{
	GFileInfo          *file_info;
	guint64             time;
	time_t              mtime;
	struct tm           t;
	gchar              *time_str, *lookup_time;

	ensure_mtime_cache (fs, file);
	lookup_time = g_hash_table_lookup (fs->private->mtime_cache, file);

	if (!lookup_time) {
		return TRUE;
	}

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

	gmtime_r (&mtime, &t);

	time_str = g_strdup_printf ("%04d-%02d-%02dT%02d:%02d:%02dZ",
	                            t.tm_year + 1900,
	                            t.tm_mon + 1,
	                            t.tm_mday,
	                            t.tm_hour,
	                            t.tm_min,
	                            t.tm_sec);

	if (strcmp (time_str, lookup_time) == 0) {
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
                         GFile          *file,
                         gboolean        is_directory,
                         gpointer        user_data)
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
		if (is_directory &&
		    should_recurse_for_directory (fs, file)) {
			tracker_miner_fs_directory_add_internal (fs, file);
		} else {
			g_queue_push_tail (fs->private->items_created,
			                   g_object_ref (file));

			item_queue_handlers_set_up (fs);
		}
	}

	g_free (path);
}

static void
monitor_item_updated_cb (TrackerMonitor *monitor,
                         GFile          *file,
                         gboolean        is_directory,
                         gpointer        user_data)
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
                         GFile          *file,
                         gboolean        is_directory,
                         gpointer        user_data)
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
		directory_data_unref (l->data);
		fs->private->directories =
			g_list_delete_link (fs->private->directories, l);
	}
#endif

	g_free (path);
}

static void
monitor_item_moved_cb (TrackerMonitor *monitor,
                       GFile          *file,
                       GFile          *other_file,
                       gboolean        is_directory,
                       gboolean        is_source_monitored,
                       gpointer        user_data)
{
	TrackerMinerFS *fs;

	fs = user_data;

	if (!is_source_monitored) {
		if (is_directory &&
		    should_recurse_for_directory (fs, other_file)) {
			gchar *path;

			path = g_file_get_path (other_file);

			g_debug ("Not in store:'?'->'%s' (DIR) (move monitor event, source unknown)",
			         path);

			/* If the source is not monitored, we need to crawl it. */
			tracker_miner_fs_directory_add_internal (fs, other_file);

			g_free (path);
		}
	} else {
		gchar *path;
		gchar *other_path;
		gboolean source_stored, should_process_other;

		path = g_file_get_path (file);
		other_path = g_file_get_path (other_file);

		source_stored = item_query_exists (fs, file, NULL, NULL);
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
			if (!is_directory ||
			    !should_recurse_for_directory (fs, other_file)) {
				g_queue_push_tail (fs->private->items_created,
				                   g_object_ref (other_file));

				item_queue_handlers_set_up (fs);
			} else {
				g_debug ("Not in store:'?'->'%s' (DIR) (move monitor event, source monitored)",
				         path);

				tracker_miner_fs_directory_add_internal (fs, other_file);
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
                       GFile          *file,
                       gpointer        user_data)
{
	TrackerMinerFS *fs = user_data;

	return should_process_file (fs, file, FALSE);
}

static gboolean
crawler_check_directory_cb (TrackerCrawler *crawler,
                            GFile          *file,
                            gpointer        user_data)
{
	TrackerMinerFS *fs = user_data;
	gboolean should_check;

	should_check = should_check_file (fs, file, TRUE);

	if (!should_check) {
		/* Remove monitors if any */
		tracker_monitor_remove (fs->private->monitor, file);
	} else {
                gboolean should_change_index;

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

static gboolean
crawler_check_directory_contents_cb (TrackerCrawler *crawler,
                                     GFile          *parent,
                                     GList          *children,
                                     gpointer        user_data)
{
	TrackerMinerFS *fs = user_data;
	gboolean add_monitor = FALSE;
	gboolean process;

	g_signal_emit (fs, signals[CHECK_DIRECTORY_CONTENTS], 0, parent, children, &process);

	if (process) {
		g_signal_emit (fs, signals[MONITOR_DIRECTORY], 0, parent, &add_monitor);
	}

	/* FIXME: Should we add here or when we process the queue in
	 * the finished sig?
	 */
	if (add_monitor) {
		tracker_monitor_add (fs->private->monitor, parent);
	} else {
		tracker_monitor_remove (fs->private->monitor, parent);
	}

	return process;
}

#ifdef ENABLE_TREE_DEBUGGING

static gboolean
print_file_tree (GNode    *node,
		 gpointer  user_data)
{
	gchar *name;
	gint i;

	name = g_file_get_basename (node->data);

	/* Indentation */
	for (i = g_node_depth (node) - 1; i > 0; i--) {
		g_print ("  ");
	}

	g_print ("%s\n", name);
	g_free (name);

	return FALSE;
}

#endif /* ENABLE_TREE_DEBUGGING */

static CrawledDirectoryData *
crawled_directory_data_new (GNode *tree)
{
	CrawledDirectoryData *data;

	data = g_slice_new (CrawledDirectoryData);
	data->tree = g_node_copy_deep (tree, (GCopyFunc) g_object_ref, NULL);
	data->nodes = g_queue_new ();

	data->n_items = g_node_n_nodes (data->tree, G_TRAVERSE_ALL);
	data->n_items_processed = 0;

	return data;
}

static gboolean
crawled_directory_data_free_foreach (GNode    *node,
				     gpointer  user_data)
{
	g_object_unref (node->data);
	return FALSE;
}

static void
crawled_directory_data_free (CrawledDirectoryData *data)
{
	g_node_traverse (data->tree,
			 G_POST_ORDER,
			 G_TRAVERSE_ALL,
			 -1,
			 crawled_directory_data_free_foreach,
			 NULL);
	g_node_destroy (data->tree);

	g_queue_free (data->nodes);

	g_slice_free (CrawledDirectoryData, data);
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
	TrackerMinerFS *fs = user_data;
	CrawledDirectoryData *dir_data;

#ifdef ENABLE_TREE_DEBUGGING
	/* Debug printing of the directory tree */
	g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 print_file_tree, NULL);
#endif /* ENABLE_TREE_DEBUGGING */

	/* Add tree to the crawled directories queue, this queue
	 * will be used to fill priv->items_created in when no
	 * further data is left there.
	 */
	dir_data = crawled_directory_data_new (tree);
	g_queue_push_tail (fs->private->crawled_directories, dir_data);

	/* Update stats */
	fs->private->directories_found += directories_found;
	fs->private->directories_ignored += directories_ignored;
	fs->private->files_found += files_found;
	fs->private->files_ignored += files_ignored;

	fs->private->total_directories_found += directories_found;
	fs->private->total_directories_ignored += directories_ignored;
	fs->private->total_files_found += files_found;
	fs->private->total_files_ignored += files_ignored;

	g_message ("  Found %d directories, ignored %d directories",
	           directories_found,
	           directories_ignored);
	g_message ("  Found %d files, ignored %d files",
	           files_found,
	           files_ignored);
}

static void
crawler_finished_cb (TrackerCrawler *crawler,
                     gboolean        was_interrupted,
		     gpointer        user_data)
{
	TrackerMinerFS *fs = user_data;

	fs->private->is_crawling = FALSE;

	g_message ("%s crawling files after %2.2f seconds",
	           was_interrupted ? "Stopped" : "Finished",
	           g_timer_elapsed (fs->private->timer, NULL));

	directory_data_unref (fs->private->current_directory);
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

	if (!fs->private->timer) {
		fs->private->timer = g_timer_new ();
	}

	fs->private->current_directory = fs->private->directories->data;
	fs->private->directories = g_list_remove (fs->private->directories,
	                                          fs->private->current_directory);

	path = g_file_get_path (fs->private->current_directory->file);

	if (fs->private->current_directory->recurse) {
		str = g_strdup_printf ("Crawling recursively directory '%s'", path);
	} else {
		str = g_strdup_printf ("Crawling single directory '%s'", path);
	}

	g_message ("%s", str);

	/* Always set the progress here to at least 1% */
	g_object_set (fs,
	              "progress", 0.01,
	              "status", str,
	              NULL);
	g_free (str);
	g_free (path);

	if (tracker_crawler_start (fs->private->crawler,
	                           fs->private->current_directory->file,
	                           fs->private->current_directory->recurse)) {
		/* Crawler when restart the idle function when done */
		fs->private->is_crawling = TRUE;
		fs->private->crawl_directories_id = 0;
		return FALSE;
	}

	/* Directory couldn't be processed */
	directory_data_unref (fs->private->current_directory);
	fs->private->current_directory = NULL;

	return TRUE;
}

static void
crawl_directories_start (TrackerMinerFS *fs)
{
	if (fs->private->crawl_directories_id != 0 ||
	    fs->private->current_directory) {
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

	fs->private->crawl_directories_id = _tracker_idle_add (fs, crawl_directories_cb, fs);
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

static gboolean
should_recurse_for_directory (TrackerMinerFS *fs,
			      GFile          *file)
{
	gboolean recurse = FALSE;
	GList *dirs;

	for (dirs = fs->private->config_directories; dirs; dirs = dirs->next) {
		DirectoryData *data;

		data = dirs->data;

		if (data->recurse &&
		    (g_file_equal (file, data->file) ||
		     g_file_has_prefix (file, data->file))) {
			/* File is inside a recursive dir */
			recurse = TRUE;
			break;
		}
	}

	return recurse;
}

/* This function is for internal use, adds the file to the processing
 * queue with the same directory settings than the corresponding
 * config directory.
 */
static void
tracker_miner_fs_directory_add_internal (TrackerMinerFS *fs,
					 GFile          *file)
{
	DirectoryData *data;
	gboolean recurse;

	recurse = should_recurse_for_directory (fs, file);
	data = directory_data_new (file, recurse);

	fs->private->directories =
		g_list_append (fs->private->directories, data);

	crawl_directories_start (fs);
}

/**
 * tracker_miner_fs_directory_add:
 * @fs: a #TrackerMinerFS
 * @file: #GFile for the directory to inspect
 * @recurse: whether the directory should be inspected recursively
 *
 * Tells the filesystem miner to inspect a directory.
 **/
void
tracker_miner_fs_directory_add (TrackerMinerFS *fs,
                                GFile          *file,
                                gboolean        recurse)
{
	DirectoryData *dir_data;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));
	g_return_if_fail (G_IS_FILE (file));

	dir_data = directory_data_new (file, recurse);

	fs->private->config_directories =
		g_list_append (fs->private->config_directories, dir_data);

	fs->private->directories =
		g_list_append (fs->private->directories,
			       directory_data_ref (dir_data));

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

/**
 * tracker_miner_fs_directory_remove:
 * @fs: a #TrackerMinerFS
 * @file: #GFile for the directory to be removed
 *
 * Removes a directory from being inspected by @fs.
 *
 * Returns: %TRUE if the directory was successfully removed.
 **/
gboolean
tracker_miner_fs_directory_remove (TrackerMinerFS *fs,
                                   GFile          *file)
{
	TrackerMinerFSPrivate *priv;
	gboolean return_val = FALSE;
	GList *dirs, *pool;

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
			directory_data_unref (data);
			fs->private->directories = g_list_delete_link (fs->private->directories, link);
			return_val = TRUE;
		}
	}

	dirs = fs->private->config_directories;

	while (dirs) {
		DirectoryData *data = dirs->data;
		GList *link = dirs;

		dirs = dirs->next;

		if (g_file_equal (file, data->file)) {
			directory_data_unref (data);
			fs->private->config_directories = g_list_delete_link (fs->private->config_directories, link);
			return_val = TRUE;
		}
	}


	/* Remove anything contained in the removed directory
	 * from all relevant processing queues.
	 */
	check_files_removal (priv->items_updated, file);
	check_files_removal (priv->items_created, file);

	pool = fs->private->processing_pool;

	while (pool) {
		ProcessData *data = pool->data;

		if (g_file_equal (data->file, file) ||
		    g_file_has_prefix (data->file, file)) {
			g_cancellable_cancel (data->cancellable);
		}

		pool = pool->next;
	}

	return return_val;
}

/**
 * tracker_miner_fs_file_add:
 * @fs: a #TrackerMinerFS
 * @file: #GFile for the file to inspect
 *
 * Tells the filesystem miner to inspect a file.
 **/
void
tracker_miner_fs_file_add (TrackerMinerFS *fs,
                           GFile          *file)
{
	gboolean should_process;
	gchar *path;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));
	g_return_if_fail (G_IS_FILE (file));

	should_process = should_check_file (fs, file, FALSE);

	path = g_file_get_path (file);

	g_debug ("%s:'%s' (FILE) (requested by application)",
	         should_process ? "Found " : "Ignored",
	         path);

	if (should_process) {
		g_queue_push_tail (fs->private->items_updated,
		                   g_object_ref (file));

		item_queue_handlers_set_up (fs);
	}

	g_free (path);
}

/**
 * tracker_miner_fs_file_notify:
 * @fs: a #TrackerMinerFS
 * @file: a #GFile
 * @error: a #GError with the error that happened during processing, or %NULL.
 *
 * Notifies @fs that all processing on @file has been finished, if any error
 * happened during file data processing, it should be passed in @error, else
 * that parameter will contain %NULL to reflect success.
 **/
void
tracker_miner_fs_file_notify (TrackerMinerFS *fs,
                              GFile          *file,
                              const GError   *error)
{
	ProcessData *data;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));
	g_return_if_fail (G_IS_FILE (file));

	fs->private->total_files_notified++;

	data = process_data_find (fs, file);

	if (!data) {
		gchar *uri;

		uri = g_file_get_uri (file);
		g_critical ("%s has notified that file '%s' has been processed, "
		            "but that file was not in the processing queue. "
		            "This is an implementation error, please ensure that "
		            "tracker_miner_fs_file_notify() is called on the same "
			    "GFile that is passed in ::process-file, and that this"
			    "signal didn't return FALSE for it",
			    G_OBJECT_TYPE_NAME (fs), uri);
		g_free (uri);

		return;
	}

	item_add_or_update_cb (fs, data, error);
}

/**
 * tracker_miner_fs_set_throttle:
 * @fs: a #TrackerMinerFS
 * @throttle: throttle value, between 0 and 1
 *
 * Tells the filesystem miner to throttle its operations.
 * a value of 0 means no throttling at all, so the miner
 * will perform operations at full speed, 1 is the slowest
 * value.
 **/
void
tracker_miner_fs_set_throttle (TrackerMinerFS *fs,
                               gdouble         throttle)
{
	g_return_if_fail (TRACKER_IS_MINER_FS (fs));

	throttle = CLAMP (throttle, 0, 1);

	if (fs->private->throttle == throttle) {
		return;
	}

	fs->private->throttle = throttle;

	/* Update timeouts */
	if (fs->private->item_queues_handler_id != 0) {
		g_source_remove (fs->private->item_queues_handler_id);

		fs->private->item_queues_handler_id =
			_tracker_idle_add (fs,
			                   item_queue_handlers_cb,
			                   fs);
	}

	if (fs->private->crawl_directories_id) {
		g_source_remove (fs->private->crawl_directories_id);

		fs->private->crawl_directories_id =
			_tracker_idle_add (fs, crawl_directories_cb, fs);
	}
}

/**
 * tracker_miner_fs_get_throttle:
 * @fs: a #TrackerMinerFS
 *
 * Gets the current throttle value. see tracker_miner_fs_set_throttle().
 *
 * Returns: current throttle value.
 **/
gdouble
tracker_miner_fs_get_throttle (TrackerMinerFS *fs)
{
	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), 0);

	return fs->private->throttle;
}

/**
 * tracker_miner_fs_get_urn:
 * @fs: a #TrackerMinerFS
 * @file: a #GFile obtained in #TrackerMinerFS::process-file
 *
 * If the item exists in the store, this function retrieves
 * the URN for a #GFile being currently processed.

 * If @file is not being currently processed by @fs, or doesn't
 * exist in the store yet, %NULL will be returned.
 *
 * Returns: The URN containing the data associated to @file,
 *          or %NULL.
 **/
G_CONST_RETURN gchar *
tracker_miner_fs_get_urn (TrackerMinerFS *fs,
                          GFile          *file)
{
	ProcessData *data;

	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), NULL);
	g_return_val_if_fail (G_IS_FILE (file), NULL);

	data = process_data_find (fs, file);

	if (!data) {
		gchar *uri;

		uri = g_file_get_uri (file);

		g_critical ("File '%s' is not being currently processed, "
			    "so the URN cannot be retrieved.", uri);
		g_free (uri);

		return NULL;
	}

	return data->urn;
}

/**
 * tracker_miner_fs_get_parent_urn:
 * @fs: a #TrackerMinerFS
 * @file: a #GFile obtained in #TrackerMinerFS::process-file
 *
 * If @file is currently being processed by @fs, this function
 * will return the parent folder URN if any. This function is
 * useful to set the nie:belongsToContainer relationship. The
 * processing order of #TrackerMinerFS guarantees that a folder
 * has been already fully processed for indexing before any
 * children is processed, so most usually this function should
 * return non-%NULL.
 *
 * Returns: The parent folder URN, or %NULL.
 **/
G_CONST_RETURN gchar *
tracker_miner_fs_get_parent_urn (TrackerMinerFS *fs,
                                 GFile          *file)
{
	ProcessData *data;

	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), NULL);
	g_return_val_if_fail (G_IS_FILE (file), NULL);

	data = process_data_find (fs, file);

	if (!data) {
		gchar *uri;

		uri = g_file_get_uri (file);

		g_critical ("File '%s' is not being currently processed, "
			    "so the URN cannot be retrieved.", uri);
		g_free (uri);

		return NULL;
	}

	return data->parent_urn;
}

void
tracker_miner_fs_force_recheck (TrackerMinerFS *fs)
{
	GList *directories;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));

	g_message ("Forcing re-check on all index directories");

	directories = g_list_copy (fs->private->config_directories);
	g_list_foreach (directories, (GFunc) directory_data_ref, NULL);

	fs->private->directories = g_list_concat (fs->private->directories, directories);

	crawl_directories_start (fs);
}
