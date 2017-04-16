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

#include <libtracker-common/tracker-common.h>

#include "tracker-crawler.h"
#include "tracker-miner-fs.h"
#include "tracker-media-art.h"
#include "tracker-monitor.h"
#include "tracker-utils.h"
#include "tracker-thumbnailer.h"
#include "tracker-priority-queue.h"
#include "tracker-task-pool.h"
#include "tracker-sparql-buffer.h"
#include "tracker-file-notifier.h"

/* If defined will print the tree from GNode while running */
#ifdef CRAWLED_TREE_ENABLE_TRACE
#warning Tree debugging traces enabled
#endif /* CRAWLED_TREE_ENABLE_TRACE */

/* If defined will print push/pop actions on queues */
#ifdef EVENT_QUEUE_ENABLE_TRACE
#warning Event Queue traces enabled
#define EVENT_QUEUE_LOG_PREFIX "[Event Queues] "
#define EVENT_QUEUE_STATUS_TIMEOUT_SECS 30
#define trace_eq(message, ...) g_debug (EVENT_QUEUE_LOG_PREFIX message, ##__VA_ARGS__)
#define trace_eq_action(pushed, queue_name, position, gfile1, gfile2, reason) \
	do { \
		gchar *uri1 = g_file_get_uri (gfile1); \
		gchar *uri2 = gfile2 ? g_file_get_uri (gfile2) : NULL; \
		g_debug ("%s%s '%s%s%s' %s %s of queue '%s'%s%s", \
		         EVENT_QUEUE_LOG_PREFIX, \
		         pushed ? "Pushed" : "Popped", \
		         uri1, \
		         uri2 ? "->" : "", \
		         uri2 ? uri2 : "", \
		         pushed ? "to" : "from", \
		         position, \
		         queue_name, \
		         reason ? ": " : "", \
		         reason ? reason : ""); \
		g_free (uri1); \
		g_free (uri2); \
	} while (0)
#define trace_eq_push_tail(queue_name, gfile, reason)	  \
	trace_eq_action (TRUE, queue_name, "tail", gfile, NULL, reason)
#define trace_eq_push_head(queue_name, gfile, reason)	  \
	trace_eq_action (TRUE, queue_name, "head", gfile, NULL, reason)
#define trace_eq_push_tail_2(queue_name, gfile1, gfile2, reason)	  \
	trace_eq_action (TRUE, queue_name, "tail", gfile1, gfile2, reason)
#define trace_eq_push_head_2(queue_name, gfile1, gfile2, reason)	  \
	trace_eq_action (TRUE, queue_name, "head", gfile1, gfile2, reason)
#define trace_eq_pop_head(queue_name, gfile)	  \
	trace_eq_action (FALSE, queue_name, "head", gfile, NULL, NULL)
#define trace_eq_pop_head_2(queue_name, gfile1, gfile2)	  \
	trace_eq_action (FALSE, queue_name, "head", gfile1, gfile2, NULL)
static gboolean miner_fs_queues_status_trace_timeout_cb (gpointer data);
#else
#define trace_eq(...)
#define trace_eq_push_tail(...)
#define trace_eq_push_head(...)
#define trace_eq_push_tail_2(...)
#define trace_eq_push_head_2(...)
#define trace_eq_pop_head(...)
#define trace_eq_pop_head_2(...)
#endif /* EVENT_QUEUE_ENABLE_TRACE */

/* Number of times a GFile can be re-queued before it's dropped for
 * whatever reason to avoid infinite loops.
*/
#define REENTRY_MAX 2

/* Default processing pool limits to be set */
#define DEFAULT_WAIT_POOL_LIMIT 1
#define DEFAULT_READY_POOL_LIMIT 1

/* Put tasks processing at a lower priority so other events
 * (timeouts, monitor events, etc...) are guaranteed to be
 * dispatched promptly.
 */
#define TRACKER_TASK_PRIORITY G_PRIORITY_DEFAULT_IDLE + 10

/**
 * SECTION:tracker-miner-fs
 * @short_description: Abstract base class for filesystem miners
 * @include: libtracker-miner/tracker-miner.h
 *
 * #TrackerMinerFS is an abstract base class for miners that collect data
 * from a filesystem where parent/child relationships need to be
 * inserted into the database correctly with queue management.
 *
 * All the filesystem crawling and monitoring is abstracted away,
 * leaving to implementations the decisions of what directories/files
 * should it process, and the actual data extraction.
 *
 * Example creating a TrackerMinerFS with our own file system root and
 * data provider.
 *
 * First create our class and base it on TrackerMinerFS:
 * |[
 * G_DEFINE_TYPE_WITH_CODE (MyMinerFiles, my_miner_files, TRACKER_TYPE_MINER_FS,
 *                          G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
 *                                                 my_miner_files_initable_iface_init))
 * ]|
 *
 * Later in our class creation function, we are supplying the
 * arguments we want. In this case, the 'root' is a #GFile pointing to
 * a root URI location (for example 'file:///') and 'data_provider' is a
 * #TrackerDataProvider used to enumerate 'root' and return children it
 * finds. If 'data_provider' is %NULL (the default), then a
 * #TrackerFileDataProvider is created automatically.
 * |[
 * // Note that only 'name' is mandatory
 * miner = g_initable_new (MY_TYPE_MINER_FILES,
 *                         NULL,
 *                         error,
 *                         "name", "MyMinerFiles",
 *                         "root", root,
 *                         "data-provider", data_provider,
 *                         "processing-pool-wait-limit", 10,
 *                         "processing-pool-ready-limit", 100,
 *                         NULL);
 * ]|
 **/

#define TRACKER_MINER_FS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_FS, TrackerMinerFSPrivate))

typedef struct {
	GFile *file;
	GFile *source_file;
} ItemMovedData;

typedef struct {
	GFile     *file;
	GPtrArray *results;
	GStrv      rdf_types;
	GCancellable *cancellable;
	guint notified : 1;
} ItemWritebackData;

typedef struct {
	GFile *file;
	gchar *urn;
	gchar *parent_urn;
	gint priority;
	GCancellable *cancellable;
	TrackerSparqlBuilder *builder;
	TrackerMiner *miner;
} UpdateProcessingTaskContext;

typedef struct {
	GMainLoop *main_loop;
	TrackerMiner *miner;
} ThumbnailMoveData;

struct _TrackerMinerFSPrivate {
	/* File queues for indexer */
	TrackerPriorityQueue *items_created;
	TrackerPriorityQueue *items_updated;
	TrackerPriorityQueue *items_deleted;
	TrackerPriorityQueue *items_moved;
	TrackerPriorityQueue *items_writeback;

	guint item_queues_handler_id;
	GFile *item_queue_blocker;
	GHashTable *items_ignore_next_update;

#ifdef EVENT_QUEUE_ENABLE_TRACE
	guint queue_status_timeout_id;
#endif /* EVENT_QUEUE_ENABLE_TRACE */

	/* Root / tree / index */
	GFile *root;
	TrackerIndexingTree *indexing_tree;
	TrackerFileNotifier *file_notifier;
	TrackerDataProvider *data_provider;

	/* Sparql insertion tasks */
	TrackerTaskPool *task_pool;
	TrackerSparqlBuffer *sparql_buffer;
	guint sparql_buffer_limit;

	/* File properties */
	GQuark quark_ignore_file;
	GQuark quark_recursive_removal;
	GQuark quark_attribute_updated;
	GQuark quark_directory_found_crawling;
	GQuark quark_reentry_counter;

	/* Properties */
	gdouble throttle;
	guint mtime_checking : 1;   /* TRUE if mtime checks should be done
	                             * during initial crawling. */
	guint initial_crawling : 1; /* TRUE if initial crawling should be
	                             * done */

	/* Writeback tasks */
	TrackerTaskPool *writeback_pool;

	TrackerThumbnailer *thumbnailer;

	/* Status */
	GTimer *timer;
	GTimer *extraction_timer;

	guint been_started : 1;     /* TRUE if miner has been started */
	guint been_crawled : 1;     /* TRUE if initial crawling has been
	                             * done */
	guint shown_totals : 1;     /* TRUE if totals have been shown */
	guint is_paused : 1;        /* TRUE if miner is paused */

	guint timer_stopped : 1;    /* TRUE if main timer is stopped */
	guint extraction_timer_stopped : 1; /* TRUE if the extraction
	                                     * timer is stopped */

	GHashTable *roots_to_notify;        /* Used to signal indexing
	                                     * trees finished */

	/*
	 * Statistics
	 */

	/* How many we found during crawling and how many were black
	 * listed (ignored). Reset to 0 when processing stops. */
	guint total_directories_found;
	guint total_directories_ignored;
	guint total_files_found;
	guint total_files_ignored;

	/* How many we indexed and how many had errors indexing. */
	guint total_files_processed;
	guint total_files_notified;
	guint total_files_notified_error;
};

typedef enum {
	QUEUE_NONE,
	QUEUE_CREATED,
	QUEUE_UPDATED,
	QUEUE_DELETED,
	QUEUE_MOVED,
	QUEUE_IGNORE_NEXT_UPDATE,
	QUEUE_WAIT,
	QUEUE_WRITEBACK
} QueueState;

enum {
	PROCESS_FILE,
	PROCESS_FILE_ATTRIBUTES,
	IGNORE_NEXT_UPDATE_FILE,
	FINISHED,
	WRITEBACK_FILE,
	FINISHED_ROOT,
	REMOVE_FILE,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_THROTTLE,
	PROP_ROOT,
	PROP_WAIT_POOL_LIMIT,
	PROP_READY_POOL_LIMIT,
	PROP_DATA_PROVIDER,
	PROP_MTIME_CHECKING,
	PROP_INITIAL_CRAWLING
};

static void           miner_fs_initable_iface_init        (GInitableIface       *iface);

static gboolean       miner_fs_remove_file                (TrackerMinerFS       *fs,
                                                           GFile                *file,
                                                           gboolean              children_only,
                                                           TrackerSparqlBuilder *builder);
static void           fs_finalize                         (GObject              *object);
static void           fs_constructed                      (GObject              *object);
static void           fs_set_property                     (GObject              *object,
                                                           guint                 prop_id,
                                                           const GValue         *value,
                                                           GParamSpec           *pspec);
static void           fs_get_property                     (GObject              *object,
                                                           guint                 prop_id,
                                                           GValue               *value,
                                                           GParamSpec           *pspec);

static void           miner_started                       (TrackerMiner         *miner);
static void           miner_stopped                       (TrackerMiner         *miner);
static void           miner_paused                        (TrackerMiner         *miner);
static void           miner_resumed                       (TrackerMiner         *miner);
static void           miner_ignore_next_update            (TrackerMiner         *miner,
                                                           const GStrv           subjects);
static ItemMovedData *item_moved_data_new                 (GFile                *file,
                                                           GFile                *source_file);
static void           item_moved_data_free                (ItemMovedData        *data);
static void           item_writeback_data_free            (ItemWritebackData    *data);

static void           indexing_tree_directory_removed     (TrackerIndexingTree  *indexing_tree,
                                                           GFile                *directory,
                                                           gpointer              user_data);
static void           file_notifier_file_created          (TrackerFileNotifier  *notifier,
                                                           GFile                *file,
                                                           gpointer              user_data);
static void           file_notifier_file_deleted          (TrackerFileNotifier  *notifier,
                                                           GFile                *file,
                                                           gpointer              user_data);
static void           file_notifier_file_updated          (TrackerFileNotifier  *notifier,
                                                           GFile                *file,
                                                           gboolean              attributes_only,
                                                           gpointer              user_data);
static void           file_notifier_file_moved            (TrackerFileNotifier  *notifier,
                                                           GFile                *source,
                                                           GFile                *dest,
                                                           gpointer              user_data);
static void           file_notifier_directory_started     (TrackerFileNotifier *notifier,
                                                           GFile               *directory,
                                                           gpointer             user_data);
static void           file_notifier_directory_finished    (TrackerFileNotifier *notifier,
                                                           GFile               *directory,
                                                           guint                directories_found,
                                                           guint                directories_ignored,
                                                           guint                files_found,
                                                           guint                files_ignored,
                                                           gpointer             user_data);
static void           file_notifier_finished              (TrackerFileNotifier *notifier,
                                                           gpointer             user_data);

static void           item_queue_handlers_set_up          (TrackerMinerFS       *fs);

static void           task_pool_cancel_foreach                (gpointer        data,
                                                               gpointer        user_data);
static void           task_pool_limit_reached_notify_cb       (GObject        *object,
                                                               GParamSpec     *pspec,
                                                               gpointer        user_data);
static void           writeback_pool_cancel_foreach           (gpointer        data,
                                                               gpointer        user_data);

static GQuark quark_file_iri = 0;
static GInitableIface* miner_fs_initable_parent_iface;
static guint signals[LAST_SIGNAL] = { 0, };

/**
 * tracker_miner_fs_error_quark:
 *
 * Gives the caller the #GQuark used to identify #TrackerMinerFS errors
 * in #GError structures. The #GQuark is used as the domain for the error.
 *
 * Returns: the #GQuark used for the domain of a #GError.
 *
 * Since: 1.2.
 **/
G_DEFINE_QUARK (TrackerMinerFSError, tracker_miner_fs_error)

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (TrackerMinerFS, tracker_miner_fs, TRACKER_TYPE_MINER,
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         miner_fs_initable_iface_init));

static void
tracker_miner_fs_class_init (TrackerMinerFSClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);

	klass->remove_file = miner_fs_remove_file;

	object_class->finalize = fs_finalize;
	object_class->constructed = fs_constructed;
	object_class->set_property = fs_set_property;
	object_class->get_property = fs_get_property;

	miner_class->started = miner_started;
	miner_class->stopped = miner_stopped;
	miner_class->paused  = miner_paused;
	miner_class->resumed = miner_resumed;
	miner_class->ignore_next_update = miner_ignore_next_update;

	g_object_class_install_property (object_class,
	                                 PROP_THROTTLE,
	                                 g_param_spec_double ("throttle",
	                                                      "Throttle",
	                                                      "Modifier for the indexing speed, 0 is max speed",
	                                                      0, 1, 0,
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_ROOT,
	                                 g_param_spec_object ("root",
	                                                      "Root",
	                                                      "Top level URI for our indexing tree and file notify clases",
	                                                      G_TYPE_FILE,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
	                                 PROP_WAIT_POOL_LIMIT,
	                                 g_param_spec_uint ("processing-pool-wait-limit",
	                                                    "Processing pool limit for WAIT tasks",
	                                                    "Maximum number of files that can be concurrently "
	                                                    "processed by the upper layer",
	                                                    1, G_MAXUINT, DEFAULT_WAIT_POOL_LIMIT,
	                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_READY_POOL_LIMIT,
	                                 g_param_spec_uint ("processing-pool-ready-limit",
	                                                    "Processing pool limit for READY tasks",
	                                                    "Maximum number of SPARQL updates that can be merged "
	                                                    "in a single connection to the store",
	                                                    1, G_MAXUINT, DEFAULT_READY_POOL_LIMIT,
	                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_DATA_PROVIDER,
	                                 g_param_spec_object ("data-provider",
	                                                      "Data provider",
	                                                      "Data provider populating data, e.g. like GFileEnumerator",
	                                                      TRACKER_TYPE_DATA_PROVIDER,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
	                                 PROP_MTIME_CHECKING,
	                                 g_param_spec_boolean ("mtime-checking",
	                                                       "Mtime checking",
	                                                       "Whether to perform mtime checks during initial crawling or not",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_INITIAL_CRAWLING,
	                                 g_param_spec_boolean ("initial-crawling",
	                                                       "Initial crawling",
	                                                       "Whether to perform initial crawling or not",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE));

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
	 *
	 * Since: 0.8
	 **/
	signals[PROCESS_FILE] =
		g_signal_new ("process-file",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, process_file),
		              NULL, NULL,
		              NULL,
		              G_TYPE_BOOLEAN,
		              3, G_TYPE_FILE, TRACKER_SPARQL_TYPE_BUILDER, G_TYPE_CANCELLABLE);

	/**
	 * TrackerMinerFS::process-file-attributes:
	 * @miner_fs: the #TrackerMinerFS
	 * @file: a #GFile
	 * @builder: a #TrackerSparqlBuilder
	 * @cancellable: a #GCancellable
	 *
	 * The ::process-file-attributes signal is emitted whenever a file should
	 * be processed, but only the attribute-related metadata extracted.
	 *
	 * @builder is the #TrackerSparqlBuilder where all sparql updates
	 * to be performed for @file will be appended. For the properties being
	 * updated, the DELETE statements should be included as well.
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
	 *
	 * Since: 0.10
	 **/
	signals[PROCESS_FILE_ATTRIBUTES] =
		g_signal_new ("process-file-attributes",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, process_file_attributes),
		              NULL, NULL,
		              NULL,
		              G_TYPE_BOOLEAN,
		              3, G_TYPE_FILE, TRACKER_SPARQL_TYPE_BUILDER, G_TYPE_CANCELLABLE);

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
	 *
	 * Since: 0.8
	 *
	 * Deprecated: 0.12
	 **/
	signals[IGNORE_NEXT_UPDATE_FILE] =
		g_signal_new ("ignore-next-update-file",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, ignore_next_update_file),
		              NULL, NULL,
		              NULL,
		              G_TYPE_BOOLEAN,
		              3, G_TYPE_FILE, TRACKER_SPARQL_TYPE_BUILDER, G_TYPE_CANCELLABLE);

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
	 *
	 * Since: 0.8
	 **/
	signals[FINISHED] =
		g_signal_new ("finished",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, finished),
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              5,
		              G_TYPE_DOUBLE,
		              G_TYPE_UINT,
		              G_TYPE_UINT,
		              G_TYPE_UINT,
		              G_TYPE_UINT);

	/**
	 * TrackerMinerFS::writeback-file:
	 * @miner_fs: the #TrackerMinerFS
	 * @file: a #GFile
	 * @rdf_types: the set of RDF types
	 * @results: (element-type GStrv): a set of results prepared by the preparation query
	 * @cancellable: a #GCancellable
	 *
	 * The ::writeback-file signal is emitted whenever a file must be written
	 * back
	 *
	 * Returns: %TRUE on success, %FALSE otherwise
	 *
	 * Since: 0.10.20
	 **/
	signals[WRITEBACK_FILE] =
		g_signal_new ("writeback-file",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, writeback_file),
		              NULL,
		              NULL,
		              NULL,
		              G_TYPE_BOOLEAN,
		              4,
		              G_TYPE_FILE,
		              G_TYPE_STRV,
		              G_TYPE_PTR_ARRAY,
		              G_TYPE_CANCELLABLE);

	/**
	 * TrackerMinerFS::finished-root:
	 * @miner_fs: the #TrackerMinerFS
	 * @file: a #GFile
	 *
	 * The ::finished-crawl signal is emitted when @miner_fs has
	 * finished finding all resources that need to be indexed
	 * with the root location of @file. At this point, it's likely
	 * many are still in the queue to be added to the database,
	 * but this gives some indication that a location is
	 * processed.
	 *
	 * Since: 1.2
	 **/
	signals[FINISHED_ROOT] =
		g_signal_new ("finished-root",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, finished_root),
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              1,
		              G_TYPE_FILE);

	/**
	 * TrackerMinerFS::remove-file:
	 * @miner_fs: the #TrackerMinerFS
	 * @file: a #GFile
	 * @children_only: #TRUE if only the children of @file are to be deleted
	 * @builder: a #TrackerSparqlBuilder
	 *
	 * The ::remove-file signal will be emitted on files that need removal
	 * according to the miner configuration (either the files themselves are
	 * deleted, or the directory/contents no longer need inspection according
	 * to miner configuration and their location.
	 *
	 * This operation is always assumed to be recursive, the @children_only
	 * argument will be %TRUE if for any reason the topmost directory needs
	 * to stay (e.g. moved from a recursively indexed directory tree to a
	 * non-recursively indexed location).
	 *
	 * The @builder argument can be used to provide additional SPARQL
	 * deletes and updates necessary around the deletion of those items. If
	 * the return value of this signal is %TRUE, @builder is expected to
	 * contain all relevant deletes for this operation.
	 *
	 * If the return value of this signal is %FALSE, the miner will apply
	 * its default behavior, which is deleting all triples that correspond
	 * to the affected URIs.
	 *
	 * Returns: %TRUE if @builder contains all the necessary operations to
	 *          delete the affected resources, %FALSE to let the miner
	 *          implicitly handle the deletion.
	 *
	 * Since: 1.8
	 **/
	signals[REMOVE_FILE] =
		g_signal_new ("remove-file",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerFSClass, remove_file),
		              NULL, NULL, NULL,
		              G_TYPE_BOOLEAN,
		              3,
		              G_TYPE_FILE, G_TYPE_BOOLEAN,
		              TRACKER_SPARQL_TYPE_BUILDER);

	g_type_class_add_private (object_class, sizeof (TrackerMinerFSPrivate));

	quark_file_iri = g_quark_from_static_string ("tracker-miner-file-iri");
}

static void
tracker_miner_fs_init (TrackerMinerFS *object)
{
	TrackerMinerFSPrivate *priv;

	object->priv = TRACKER_MINER_FS_GET_PRIVATE (object);

	priv = object->priv;

	priv->timer = g_timer_new ();
	priv->extraction_timer = g_timer_new ();

	g_timer_stop (priv->timer);
	g_timer_stop (priv->extraction_timer);

	priv->timer_stopped = TRUE;
	priv->extraction_timer_stopped = TRUE;

	priv->items_created = tracker_priority_queue_new ();
	priv->items_updated = tracker_priority_queue_new ();
	priv->items_deleted = tracker_priority_queue_new ();
	priv->items_moved = tracker_priority_queue_new ();
	priv->items_writeback = tracker_priority_queue_new ();

#ifdef EVENT_QUEUE_ENABLE_TRACE
	priv->queue_status_timeout_id = g_timeout_add_seconds (EVENT_QUEUE_STATUS_TIMEOUT_SECS,
	                                                       miner_fs_queues_status_trace_timeout_cb,
	                                                       object);
#endif /* PROCESSING_POOL_ENABLE_TRACE */

	priv->items_ignore_next_update = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                                        (GDestroyNotify) g_free,
	                                                        (GDestroyNotify) NULL);

	/* Create processing pools */
	priv->task_pool = tracker_task_pool_new (DEFAULT_WAIT_POOL_LIMIT);
	g_signal_connect (priv->task_pool, "notify::limit-reached",
	                  G_CALLBACK (task_pool_limit_reached_notify_cb), object);

	priv->writeback_pool = tracker_task_pool_new (DEFAULT_WAIT_POOL_LIMIT);
	g_signal_connect (priv->writeback_pool, "notify::limit-reached",
	                  G_CALLBACK (task_pool_limit_reached_notify_cb), object);

	priv->quark_ignore_file = g_quark_from_static_string ("tracker-ignore-file");
	priv->quark_recursive_removal = g_quark_from_static_string ("tracker-recursive-removal");
	priv->quark_directory_found_crawling = g_quark_from_static_string ("tracker-directory-found-crawling");
	priv->quark_attribute_updated = g_quark_from_static_string ("tracker-attribute-updated");
	priv->quark_reentry_counter = g_quark_from_static_string ("tracker-reentry-counter");

	priv->mtime_checking = TRUE;
	priv->initial_crawling = TRUE;

	priv->roots_to_notify = g_hash_table_new_full (g_file_hash,
	                                               (GEqualFunc) g_file_equal,
	                                               g_object_unref,
	                                               NULL);
}

static gboolean
miner_fs_initable_init (GInitable     *initable,
                        GCancellable  *cancellable,
                        GError       **error)
{
	TrackerMinerFSPrivate *priv;
	guint limit;

	if (!miner_fs_initable_parent_iface->init (initable, cancellable, error)) {
		return FALSE;
	}

	priv = TRACKER_MINER_FS_GET_PRIVATE (initable);

	g_object_get (initable, "processing-pool-ready-limit", &limit, NULL);
	priv->sparql_buffer = tracker_sparql_buffer_new (tracker_miner_get_connection (TRACKER_MINER (initable)),
	                                                 limit);

	if (!priv->sparql_buffer) {
		g_set_error (error,
		             tracker_miner_fs_error_quark (),
		             TRACKER_MINER_FS_ERROR_INIT,
		             "Could not create TrackerSparqlBuffer needed to process resources");
		return FALSE;
	}

	g_signal_connect (priv->sparql_buffer, "notify::limit-reached",
	                  G_CALLBACK (task_pool_limit_reached_notify_cb),
	                  initable);

	if (!priv->indexing_tree) {
		g_set_error (error,
		             tracker_miner_fs_error_quark (),
		             TRACKER_MINER_FS_ERROR_INIT,
		             "Could not create TrackerIndexingTree needed to manage content indexed");
		return FALSE;
	}

	g_signal_connect (priv->indexing_tree, "directory-removed",
	                  G_CALLBACK (indexing_tree_directory_removed),
	                  initable);

	/* Create the file notifier */
	priv->file_notifier = tracker_file_notifier_new (priv->indexing_tree,
	                                                 priv->data_provider);

	if (!priv->file_notifier) {
		g_set_error (error,
		             tracker_miner_fs_error_quark (),
		             TRACKER_MINER_FS_ERROR_INIT,
		             "Could not create TrackerFileNotifier needed to signal new resources to be indexed");
		return FALSE;
	}

	g_signal_connect (priv->file_notifier, "file-created",
	                  G_CALLBACK (file_notifier_file_created),
	                  initable);
	g_signal_connect (priv->file_notifier, "file-updated",
	                  G_CALLBACK (file_notifier_file_updated),
	                  initable);
	g_signal_connect (priv->file_notifier, "file-deleted",
	                  G_CALLBACK (file_notifier_file_deleted),
	                  initable);
	g_signal_connect (priv->file_notifier, "file-moved",
	                  G_CALLBACK (file_notifier_file_moved),
	                  initable);
	g_signal_connect (priv->file_notifier, "directory-started",
	                  G_CALLBACK (file_notifier_directory_started),
	                  initable);
	g_signal_connect (priv->file_notifier, "directory-finished",
	                  G_CALLBACK (file_notifier_directory_finished),
	                  initable);
	g_signal_connect (priv->file_notifier, "finished",
	                  G_CALLBACK (file_notifier_finished),
	                  initable);

	priv->thumbnailer = tracker_thumbnailer_new ();

	return TRUE;
}

static void
miner_fs_initable_iface_init (GInitableIface *iface)
{
	miner_fs_initable_parent_iface = g_type_interface_peek_parent (iface);
	iface->init = miner_fs_initable_init;
}

static gboolean
miner_fs_remove_file (TrackerMinerFS *fs,
                      GFile                *file,
                      gboolean              children_only,
                      TrackerSparqlBuilder *builder)
{
	return FALSE;
}

static void
fs_finalize (GObject *object)
{
	TrackerMinerFSPrivate *priv;

	priv = TRACKER_MINER_FS_GET_PRIVATE (object);

	g_timer_destroy (priv->timer);
	g_timer_destroy (priv->extraction_timer);

	if (priv->item_queues_handler_id) {
		g_source_remove (priv->item_queues_handler_id);
		priv->item_queues_handler_id = 0;
	}

	if (priv->item_queue_blocker) {
		g_object_unref (priv->item_queue_blocker);
	}

	if (priv->file_notifier) {
		tracker_file_notifier_stop (priv->file_notifier);
	}

	/* Cancel every pending task */
	tracker_task_pool_foreach (priv->task_pool,
	                           task_pool_cancel_foreach,
	                           NULL);
	g_object_unref (priv->task_pool);

	g_object_unref (priv->writeback_pool);

	if (priv->sparql_buffer) {
		g_object_unref (priv->sparql_buffer);
	}

	tracker_priority_queue_foreach (priv->items_moved,
	                                (GFunc) item_moved_data_free,
	                                NULL);
	tracker_priority_queue_unref (priv->items_moved);

	tracker_priority_queue_foreach (priv->items_deleted,
	                                (GFunc) g_object_unref,
	                                NULL);
	tracker_priority_queue_unref (priv->items_deleted);

	tracker_priority_queue_foreach (priv->items_updated,
	                                (GFunc) g_object_unref,
	                                NULL);
	tracker_priority_queue_unref (priv->items_updated);

	tracker_priority_queue_foreach (priv->items_created,
	                                (GFunc) g_object_unref,
	                                NULL);
	tracker_priority_queue_unref (priv->items_created);

	tracker_priority_queue_foreach (priv->items_writeback,
	                                (GFunc) item_writeback_data_free,
	                                NULL);
	tracker_priority_queue_unref (priv->items_writeback);

	g_hash_table_unref (priv->items_ignore_next_update);

	if (priv->indexing_tree) {
		g_object_unref (priv->indexing_tree);
	}

	if (priv->file_notifier) {
		g_object_unref (priv->file_notifier);
	}

	if (priv->thumbnailer) {
		g_object_unref (priv->thumbnailer);
	}

	if (priv->roots_to_notify) {
		g_hash_table_unref (priv->roots_to_notify);

		/* Just in case we end up using this AFTER finalize, not expected */
		priv->roots_to_notify = NULL;
	}

#ifdef EVENT_QUEUE_ENABLE_TRACE
	if (priv->queue_status_timeout_id)
		g_source_remove (priv->queue_status_timeout_id);
#endif /* PROCESSING_POOL_ENABLE_TRACE */

	G_OBJECT_CLASS (tracker_miner_fs_parent_class)->finalize (object);
}

static void
fs_constructed (GObject *object)
{
	TrackerMinerFSPrivate *priv;

	/* NOTE: We have to do this in this order because initables
	 * are called _AFTER_ constructed and for subclasses that are
	 * not initables we don't have any other way than to chain
	 * constructed and root/indexing tree must exist at that
	 * point.
	 *
	 * If priv->indexing_tree is NULL after this function, the
	 * initiable functions will fail and this class will not be
	 * created anyway.
	 */
	G_OBJECT_CLASS (tracker_miner_fs_parent_class)->constructed (object);

	priv = TRACKER_MINER_FS_GET_PRIVATE (object);

	/* Create root if one didn't exist */
	if (priv->root == NULL) {
		/* We default to file:/// */
		priv->root = g_file_new_for_uri ("file:///");
	}

	/* Create indexing tree */
	priv->indexing_tree = tracker_indexing_tree_new_with_root (priv->root);
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
	case PROP_ROOT:
		/* We expect this to only occur once, on object construct */
		fs->priv->root = g_value_dup_object (value);
		break;
	case PROP_WAIT_POOL_LIMIT:
		tracker_task_pool_set_limit (fs->priv->task_pool,
		                             g_value_get_uint (value));
		break;
	case PROP_READY_POOL_LIMIT:
		fs->priv->sparql_buffer_limit = g_value_get_uint (value);

		if (fs->priv->sparql_buffer) {
			tracker_task_pool_set_limit (TRACKER_TASK_POOL (fs->priv->sparql_buffer),
			                             fs->priv->sparql_buffer_limit);
		}
		break;
	case PROP_DATA_PROVIDER:
		fs->priv->data_provider = g_value_dup_object (value);
		break;
	case PROP_MTIME_CHECKING:
		fs->priv->mtime_checking = g_value_get_boolean (value);
		break;
	case PROP_INITIAL_CRAWLING:
		fs->priv->initial_crawling = g_value_get_boolean (value);
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
		g_value_set_double (value, fs->priv->throttle);
		break;
	case PROP_ROOT:
		g_value_set_object (value, fs->priv->root);
		break;
	case PROP_WAIT_POOL_LIMIT:
		g_value_set_uint (value, tracker_task_pool_get_limit (fs->priv->task_pool));
		break;
	case PROP_READY_POOL_LIMIT:
		g_value_set_uint (value, fs->priv->sparql_buffer_limit);
		break;
	case PROP_MTIME_CHECKING:
		g_value_set_boolean (value, fs->priv->mtime_checking);
		break;
	case PROP_DATA_PROVIDER:
		g_value_set_object (value, fs->priv->data_provider);
		break;
	case PROP_INITIAL_CRAWLING:
		g_value_set_boolean (value, fs->priv->initial_crawling);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
task_pool_limit_reached_notify_cb (GObject    *object,
				   GParamSpec *pspec,
				   gpointer    user_data)
{
	if (!tracker_task_pool_limit_reached (TRACKER_TASK_POOL (object))) {
		item_queue_handlers_set_up (TRACKER_MINER_FS (user_data));
	}
}

static void
miner_started (TrackerMiner *miner)
{
	TrackerMinerFS *fs;

	fs = TRACKER_MINER_FS (miner);

	fs->priv->been_started = TRUE;

	g_info ("Initializing");

	g_object_set (miner,
	              "progress", 0.0,
	              "status", "Initializing",
	              "remaining-time", 0,
	              NULL);

	tracker_file_notifier_start (fs->priv->file_notifier);
}

static void
miner_stopped (TrackerMiner *miner)
{
	g_info ("Idle");

	g_object_set (miner,
	              "progress", 1.0,
	              "status", "Idle",
	              "remaining-time", -1,
	              NULL);
}

static void
miner_paused (TrackerMiner *miner)
{
	TrackerMinerFS *fs;

	fs = TRACKER_MINER_FS (miner);

	fs->priv->is_paused = TRUE;

	tracker_file_notifier_stop (fs->priv->file_notifier);

	if (fs->priv->item_queues_handler_id) {
		g_source_remove (fs->priv->item_queues_handler_id);
		fs->priv->item_queues_handler_id = 0;
	}
}

static void
miner_resumed (TrackerMiner *miner)
{
	TrackerMinerFS *fs;

	fs = TRACKER_MINER_FS (miner);

	fs->priv->is_paused = FALSE;

	tracker_file_notifier_start (fs->priv->file_notifier);

	/* Only set up queue handler if we have items waiting to be
	 * processed.
	 */
	if (tracker_miner_fs_has_items_to_process (fs)) {
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
		g_hash_table_insert (fs->priv->items_ignore_next_update,
		                     g_strdup (urls[n]),
		                     GINT_TO_POINTER (TRUE));
	}

	item_queue_handlers_set_up (fs);
}

static void
notify_roots_finished (TrackerMinerFS *fs,
                       gboolean        check_queues)
{
	GHashTableIter iter;
	gpointer key, value;

	if (check_queues &&
	    fs->priv->roots_to_notify &&
	    g_hash_table_size (fs->priv->roots_to_notify) < 2) {
		/* Technically, if there is only one root, it's
		 * pointless to do anything before the FINISHED (not
		 * FINISHED_ROOT) signal is emitted. In that
		 * situation we calls function first anyway with
		 * check_queues=FALSE so we still notify roots. This
		 * is really just for efficiency.
		 */
		return;
	} else if (fs->priv->roots_to_notify == NULL ||
	           g_hash_table_size (fs->priv->roots_to_notify) < 1) {
		/* Nothing to do */
		return;
	}

	g_hash_table_iter_init (&iter, fs->priv->roots_to_notify);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GFile *root = key;

		/* Check if any content for root is still in the queue
		 * to be processed. This is only called each time a
		 * container/folder has been added to Tracker (so not
		 * too frequently)
		 */
		if (check_queues &&
		    (tracker_priority_queue_find (fs->priv->items_created, NULL, (GEqualFunc) g_file_has_prefix, root) ||
		     tracker_priority_queue_find (fs->priv->items_updated, NULL, (GEqualFunc) g_file_has_prefix, root) ||
		     tracker_priority_queue_find (fs->priv->items_deleted, NULL, (GEqualFunc) g_file_has_prefix, root) ||
		     tracker_priority_queue_find (fs->priv->items_moved, NULL, (GEqualFunc) g_file_has_prefix, root) ||
		     tracker_priority_queue_find (fs->priv->items_writeback, NULL, (GEqualFunc) g_file_has_prefix, root))) {
			continue;
		}

		/* Signal root is finished */
		g_signal_emit (fs, signals[FINISHED_ROOT], 0, root);

		/* Remove from hash table */
		g_hash_table_iter_remove (&iter);
	}
}

static void
process_print_stats (TrackerMinerFS *fs)
{
	/* Only do this the first time, otherwise the results are
	 * likely to be inaccurate. Devices can be added or removed so
	 * we can't assume stats are correct.
	 */
	if (!fs->priv->shown_totals) {
		fs->priv->shown_totals = TRUE;

		g_info ("--------------------------------------------------");
		g_info ("Total directories : %d (%d ignored)",
		        fs->priv->total_directories_found,
		        fs->priv->total_directories_ignored);
		g_info ("Total files       : %d (%d ignored)",
		        fs->priv->total_files_found,
		        fs->priv->total_files_ignored);
#if 0
		g_info ("Total monitors    : %d",
		        tracker_monitor_get_count (fs->priv->monitor));
#endif
		g_info ("Total processed   : %d (%d notified, %d with error)",
		        fs->priv->total_files_processed,
		        fs->priv->total_files_notified,
		        fs->priv->total_files_notified_error);
		g_info ("--------------------------------------------------\n");
	}
}

static void
process_stop (TrackerMinerFS *fs)
{
	/* Now we have finished crawling, print stats and enable monitor events */
	process_print_stats (fs);

	g_timer_stop (fs->priv->timer);
	g_timer_stop (fs->priv->extraction_timer);

	fs->priv->timer_stopped = TRUE;
	fs->priv->extraction_timer_stopped = TRUE;

	g_info ("Idle");

	g_object_set (fs,
	              "progress", 1.0,
	              "status", "Idle",
	              "remaining-time", 0,
	              NULL);

	/* Make sure we signal _ALL_ roots as finished before the
	 * main FINISHED signal
	 */
	notify_roots_finished (fs, FALSE);

	g_signal_emit (fs, signals[FINISHED], 0,
	               g_timer_elapsed (fs->priv->timer, NULL),
	               fs->priv->total_directories_found,
	               fs->priv->total_directories_ignored,
	               fs->priv->total_files_found,
	               fs->priv->total_files_ignored);

	g_timer_stop (fs->priv->timer);
	g_timer_stop (fs->priv->extraction_timer);

	fs->priv->total_directories_found = 0;
	fs->priv->total_directories_ignored = 0;
	fs->priv->total_files_found = 0;
	fs->priv->total_files_ignored = 0;

	fs->priv->been_crawled = TRUE;
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

static ItemWritebackData *
item_writeback_data_new (GFile     *file,
                         GStrv      rdf_types,
                         GPtrArray *results)
{
	ItemWritebackData *data;

	data = g_slice_new (ItemWritebackData);

	data->file = g_object_ref (file);
	data->results = g_ptr_array_ref (results);
	data->rdf_types = g_strdupv (rdf_types);
	data->cancellable = g_cancellable_new ();
	data->notified = FALSE;

	return data;
}

static void
item_writeback_data_free (ItemWritebackData *data)
{
	g_object_unref (data->file);
	g_ptr_array_unref (data->results);
	g_strfreev (data->rdf_types);
	g_object_unref (data->cancellable);
	g_slice_free (ItemWritebackData, data);
}

static gboolean
item_queue_is_blocked_by_file (TrackerMinerFS *fs,
                               GFile *file)
{
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	if (fs->priv->item_queue_blocker != NULL &&
	    (fs->priv->item_queue_blocker == file ||
	     g_file_equal (fs->priv->item_queue_blocker, file))) {
		return TRUE;
	}

	return FALSE;
}

static void
sparql_buffer_task_finished_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
	TrackerMinerFS *fs;
	TrackerMinerFSPrivate *priv;
	TrackerTask *task;
	GFile *task_file;
	gboolean recursive;
	GError *error = NULL;

	fs = user_data;
	priv = fs->priv;

	task = tracker_sparql_buffer_push_finish (TRACKER_SPARQL_BUFFER (object),
	                                          result, &error);

	if (error) {
		g_critical ("Could not execute sparql: %s", error->message);
		priv->total_files_notified_error++;
		g_error_free (error);
	}

	task_file = tracker_task_get_file (task);

	recursive = GPOINTER_TO_INT (g_object_steal_qdata (G_OBJECT (task_file),
	                                                     priv->quark_recursive_removal));
	tracker_file_notifier_invalidate_file_iri (priv->file_notifier, task_file, recursive);

	if (item_queue_is_blocked_by_file (fs, task_file)) {
		g_object_unref (priv->item_queue_blocker);
		priv->item_queue_blocker = NULL;
	}

	if (priv->item_queue_blocker != NULL) {
		if (tracker_task_pool_get_size (TRACKER_TASK_POOL (object)) > 0) {
			tracker_sparql_buffer_flush (TRACKER_SPARQL_BUFFER (object),
			                             "Item queue still blocked after flush");

			/* Check if we've finished inserting for given prefixes ... */
			notify_roots_finished (fs, TRUE);
		}
	} else {
		item_queue_handlers_set_up (fs);
	}

	tracker_task_unref (task);
}

static UpdateProcessingTaskContext *
update_processing_task_context_new (TrackerMiner         *miner,
                                    gint                  priority,
                                    const gchar          *urn,
                                    const gchar          *parent_urn,
                                    GCancellable         *cancellable,
                                    TrackerSparqlBuilder *builder)
{
	UpdateProcessingTaskContext *ctxt;

	ctxt = g_slice_new0 (UpdateProcessingTaskContext);
	ctxt->miner = miner;
	ctxt->urn = g_strdup (urn);
	ctxt->parent_urn = g_strdup (parent_urn);
	ctxt->priority = priority;

	if (cancellable) {
		ctxt->cancellable = g_object_ref (cancellable);
	}

	if (builder) {
		ctxt->builder = g_object_ref (builder);
	}

	return ctxt;
}

static void
update_processing_task_context_free (UpdateProcessingTaskContext *ctxt)
{
	g_free (ctxt->urn);
	g_free (ctxt->parent_urn);

	if (ctxt->cancellable) {
		g_object_unref (ctxt->cancellable);
	}

	if (ctxt->builder) {
		g_object_unref (ctxt->builder);
	}

	g_slice_free (UpdateProcessingTaskContext, ctxt);
}

static void
item_add_or_update_continue (TrackerMinerFS *fs,
                             TrackerTask    *task,
                             const GError   *error)
{
	UpdateProcessingTaskContext *ctxt;
	TrackerTask *sparql_task = NULL;
	GFile *file;
	gchar *uri;

	ctxt = tracker_task_get_data (task);
	file = tracker_task_get_file (task);
	uri = g_file_get_uri (file);

	if (error) {
		g_message ("Could not process '%s': %s", uri, error->message);

		fs->priv->total_files_notified_error++;
	} else {
		if (ctxt->urn) {
			gboolean attribute_update_only;

			/* The SPARQL builder will already contain the necessary
			 * DELETE statements for the properties being updated */
			attribute_update_only = GPOINTER_TO_INT (g_object_steal_qdata (G_OBJECT (file), fs->priv->quark_attribute_updated));
			g_debug ("Updating item '%s' with urn '%s'%s",
			         uri,
			         ctxt->urn,
			         attribute_update_only ? " (attributes only)" : "");

		} else {
			g_debug ("Creating new item '%s'", uri);
		}

		sparql_task = tracker_sparql_task_new_with_sparql (file, ctxt->builder);
	}

	if (sparql_task) {
		tracker_sparql_buffer_push (fs->priv->sparql_buffer,
		                            sparql_task,
		                            ctxt->priority,
		                            sparql_buffer_task_finished_cb,
		                            fs);

		if (item_queue_is_blocked_by_file (fs, file)) {
			tracker_sparql_buffer_flush (fs->priv->sparql_buffer, "Current file is blocking item queue");

			/* Check if we've finished inserting for given prefixes ... */
			notify_roots_finished (fs, TRUE);
		}

		/* We can let go of our reference here because the
		 * sparql buffer takes its own reference when adding
		 * it to the task pool.
		 */
		tracker_task_unref (sparql_task);
	} else {
		if (item_queue_is_blocked_by_file (fs, file)) {
			/* Make sure that we don't stall the item queue, although we could
			 * expect the file to be reenqueued until the loop detector makes
			 * us drop it since we were specifically waiting for it to complete.
			 */
			g_object_unref (fs->priv->item_queue_blocker);
			fs->priv->item_queue_blocker = NULL;
			item_queue_handlers_set_up (fs);
		}
	}

	/* Last reference is kept by the pool, removing the task from
	 * the pool cleans up the task too!
	 *
	 * NOTE that calling this any earlier actually causes invalid
	 * reads because the task frees up the
	 * UpdateProcessingTaskContext and GFile.
	 */
	tracker_task_pool_remove (fs->priv->task_pool, task);

	if (tracker_miner_fs_has_items_to_process (fs) == FALSE &&
	    tracker_task_pool_get_size (TRACKER_TASK_POOL (fs->priv->task_pool)) == 0) {
		/* We need to run this one more time to trigger process_stop() */
		item_queue_handlers_set_up (fs);
	}

	g_free (uri);
}

static const gchar *
lookup_file_urn (TrackerMinerFS *fs,
                 GFile          *file,
                 gboolean        force)
{
	const gchar *urn;

	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), NULL);
	g_return_val_if_fail (G_IS_FILE (file), NULL);

	urn = g_object_get_qdata (G_OBJECT (file), quark_file_iri);

	if (!urn)
		urn = tracker_file_notifier_get_file_iri (fs->priv->file_notifier,
		                                          file, force);
	return urn;
}

static gboolean
item_add_or_update (TrackerMinerFS *fs,
                    GFile          *file,
                    gint            priority)
{
	TrackerMinerFSPrivate *priv;
	TrackerSparqlBuilder *sparql;
	UpdateProcessingTaskContext *ctxt;
	GCancellable *cancellable;
	gboolean processing;
	gboolean keep_processing;
	gboolean attribute_update_only;
	TrackerTask *task;
	const gchar *parent_urn, *urn;
	gchar *uri;
	GFile *parent;

	priv = fs->priv;

	cancellable = g_cancellable_new ();
	sparql = tracker_sparql_builder_new_update ();
	g_object_ref (file);

	/* Always query. No matter we are notified the file was just
	 * created, its meta data might already be in the store
	 * (possibly inserted by other application) - in such a case
	 * we have to UPDATE, not INSERT. */
	urn = lookup_file_urn (fs, file, FALSE);

	if (!tracker_indexing_tree_file_is_root (fs->priv->indexing_tree, file)) {
		parent = g_file_get_parent (file);
		parent_urn = lookup_file_urn (fs, parent, TRUE);
		g_object_unref (parent);
	} else {
		parent_urn = NULL;
	}

	/* Create task and add it to the pool as a WAIT task (we need to extract
	 * the file metadata and such) */
	ctxt = update_processing_task_context_new (TRACKER_MINER (fs),
	                                           priority,
	                                           urn,
	                                           parent_urn,
	                                           cancellable,
	                                           sparql);
	task = tracker_task_new (file, ctxt,
	                         (GDestroyNotify) update_processing_task_context_free);

	tracker_task_pool_add (priv->task_pool, task);
	tracker_task_unref (task);

	/* Call ::process-file to see if we handle this resource or not */
	uri = g_file_get_uri (file);

	attribute_update_only = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (file), priv->quark_attribute_updated));

	if (!attribute_update_only) {
		g_debug ("Processing file '%s'...", uri);
		g_signal_emit (fs, signals[PROCESS_FILE], 0,
		               file,
		               ctxt->builder,
		               ctxt->cancellable,
		               &processing);
	} else {
		g_debug ("Processing attributes in file '%s'...", uri);
		g_signal_emit (fs, signals[PROCESS_FILE_ATTRIBUTES], 0,
		               file,
		               ctxt->builder,
		               ctxt->cancellable,
		               &processing);
	}

	keep_processing = TRUE;

	if (!processing) {
		/* Re-fetch data, since it might have been
		 * removed in broken implementations
		 */
		task = tracker_task_pool_find (priv->task_pool, file);

		g_message ("%s refused to process '%s'", G_OBJECT_TYPE_NAME (fs), uri);

		if (!task) {
			g_critical ("%s has returned FALSE in ::process-file for '%s', "
			            "but it seems that this file has been processed through "
			            "tracker_miner_fs_file_notify(), this is an "
			            "implementation error", G_OBJECT_TYPE_NAME (fs), uri);
		} else {
			tracker_task_pool_remove (priv->task_pool, task);
		}
	} else {
		fs->priv->total_files_processed++;

		if (tracker_task_pool_limit_reached (priv->task_pool)) {
			keep_processing = FALSE;
		}
	}

	g_free (uri);
	g_object_unref (file);
	g_object_unref (cancellable);
	g_object_unref (sparql);

	return keep_processing;
}

static gboolean
item_remove (TrackerMinerFS *fs,
             GFile          *file,
             gboolean        only_children)
{
	TrackerSparqlBuilder *builder;
	gboolean delete_handled = FALSE;
	gchar *uri;
	TrackerTask *task;
	guint flags = 0;

	uri = g_file_get_uri (file);

	g_debug ("Removing item: '%s' (Deleted from filesystem or no longer monitored)",
	         uri);

	if (!only_children) {
		if (fs->priv->thumbnailer)
			tracker_thumbnailer_remove_add (fs->priv->thumbnailer, uri, NULL);

#ifdef HAVE_LIBMEDIAART
		tracker_media_art_queue_remove (uri, NULL);
#endif
	}

	g_object_set_qdata (G_OBJECT (file),
	                    fs->priv->quark_recursive_removal,
	                    GINT_TO_POINTER (TRUE));

	builder = tracker_sparql_builder_new_update ();
	g_signal_emit (fs, signals[REMOVE_FILE], 0,
	               file, only_children, builder, &delete_handled);

	if (tracker_sparql_builder_get_length (builder) > 0) {
		task = tracker_sparql_task_new_with_sparql (file, builder);
		tracker_sparql_buffer_push (fs->priv->sparql_buffer,
		                            task,
		                            G_PRIORITY_DEFAULT,
		                            sparql_buffer_task_finished_cb,
		                            fs);
		tracker_task_unref (task);
	}

	g_object_unref (builder);

	if (!delete_handled) {
		if (!only_children)
			flags = TRACKER_BULK_MATCH_EQUALS;

		if (tracker_file_notifier_get_file_type (fs->priv->file_notifier, file) == G_FILE_TYPE_DIRECTORY)
			flags |= TRACKER_BULK_MATCH_CHILDREN;

		/* FIRST:
		 * Remove tracker:available for the resources we're going to remove.
		 * This is done so that unavailability of the resources is marked as soon
		 * as possible, as the actual delete may take reaaaally a long time
		 * (removing resources for 30GB of files takes even 30minutes in a 1-CPU
		 * device). */

		/* Add new task to processing pool */
		task = tracker_sparql_task_new_bulk (file,
		                                     "DELETE { "
		                                     "  ?f tracker:available true "
		                                     "}",
		                                     flags);

		tracker_sparql_buffer_push (fs->priv->sparql_buffer,
		                            task,
		                            G_PRIORITY_DEFAULT,
		                            sparql_buffer_task_finished_cb,
		                            fs);
		tracker_task_unref (task);

		/* SECOND:
		 * Actually remove all resources. This operation is the one which may take
		 * a long time.
		 */

		/* Add new task to processing pool */
		task = tracker_sparql_task_new_bulk (file,
		                                     "DELETE { "
		                                     "  ?f a rdfs:Resource . "
		                                     "  ?ie a rdfs:Resource "
		                                     "}",
		                                     flags |
		                                     TRACKER_BULK_MATCH_LOGICAL_RESOURCES);

		tracker_sparql_buffer_push (fs->priv->sparql_buffer,
		                            task,
		                            G_PRIORITY_DEFAULT,
		                            sparql_buffer_task_finished_cb,
		                            fs);
		tracker_task_unref (task);
	}

	if (!tracker_task_pool_limit_reached (TRACKER_TASK_POOL (fs->priv->sparql_buffer))) {
		item_queue_handlers_set_up (fs);
	}

	g_free (uri);

	return TRUE;
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

		query = g_strdup_printf ("DELETE { GRAPH <%s> { "
		                         "  ?u nfo:fileSize ?unknown1 ; "
		                         "     nfo:fileLastModified ?unknown2 ; "
		                         "     nfo:fileLastAccessed ?unknown3 ; "
		                         "     nie:mimeType ?unknown4 } "
		                         "} WHERE { GRAPH <%s> { "
		                         "  ?u nfo:fileSize ?unknown1 ; "
		                         "     nfo:fileLastModified ?unknown2 ; "
		                         "     nfo:fileLastAccessed ?unknown3 ; "
		                         "     nie:mimeType ?unknown4 ; "
		                         "     nie:url \"%s\" } "
		                         "} %s", TRACKER_OWN_GRAPH_URN,
		                         TRACKER_OWN_GRAPH_URN, uri,
		                         tracker_sparql_builder_get_result (sparql));

		tracker_sparql_connection_update_async (tracker_miner_get_connection (TRACKER_MINER (fs)),
		                                        query,
		                                        G_PRIORITY_DEFAULT,
		                                        NULL,
		                                        NULL,
		                                        NULL);

		g_free (query);
	}

	g_hash_table_remove (fs->priv->items_ignore_next_update, uri);

	g_object_unref (sparql);
	g_object_unref (working_file);
	g_object_unref (cancellable);

	g_free (uri);

	return FALSE;
}

static void
move_thumbnails_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
	ThumbnailMoveData *data = user_data;
	TrackerMinerFS *fs = TRACKER_MINER_FS (data->miner);
	GError *error = NULL;

	TrackerSparqlCursor *cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (object), result, &error);

	if (error) {
		g_critical ("Could move thumbnails: %s", error->message);
		g_error_free (error);
	} else {
		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			const gchar *src, *dst, *mimetype;

			src = tracker_sparql_cursor_get_string (cursor, 0, NULL);
			dst = tracker_sparql_cursor_get_string (cursor, 1, NULL);
			mimetype = tracker_sparql_cursor_get_string (cursor, 2, NULL);

			if (fs->priv->thumbnailer) {
				tracker_thumbnailer_move_add (fs->priv->thumbnailer,
				                              src, mimetype, dst);
			}
		}
	}

	g_object_unref (cursor);
	g_main_loop_quit (data->main_loop);
}

static gboolean
item_move (TrackerMinerFS *fs,
           GFile          *file,
           GFile          *source_file)
{
	gchar     *uri, *source_uri;
	GFileInfo *file_info;
	GString   *sparql;
	TrackerTask *task;
	const gchar *source_iri;
	gchar *display_name;
	gboolean source_exists;
	GFile *new_parent;
	const gchar *new_parent_iri;
	TrackerDirectoryFlags source_flags, flags;

	uri = g_file_get_uri (file);
	source_uri = g_file_get_uri (source_file);

	/* FIXME: Should check the _NO_STAT on TrackerDirectoryFlags first! */
	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
	                               G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
	                               G_FILE_ATTRIBUTE_STANDARD_TYPE,
	                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                               NULL, NULL);

	/* Get 'source' ID */
	source_iri = lookup_file_urn (fs, source_file, TRUE);
	source_exists = (source_iri != NULL);

	if (!file_info) {
		gboolean retval;

		if (source_exists) {
			/* Destination file has gone away, ignore dest file and remove source if any */
			retval = item_remove (fs, source_file, FALSE);
		} else {
			/* Destination file went away, and source wasn't indexed either */
			retval = TRUE;
		}

		g_free (source_uri);
		g_free (uri);

		return retval;
	} else if (!source_exists) {
		gboolean retval;

		/* The source file might not be indexed yet (eg. temporary save
		 * files that are immediately renamed to the definitive path).
		 * Deal with those as newly added items.
		 */
		g_debug ("Source file '%s' not yet in store, indexing '%s' "
		         "from scratch", source_uri, uri);

		retval = item_add_or_update (fs, file, G_PRIORITY_DEFAULT);

		g_free (source_uri);
		g_free (uri);
		g_object_unref (file_info);

		return retval;
	}

	g_debug ("Moving item from '%s' to '%s'",
	         source_uri,
	         uri);

	if (fs->priv->thumbnailer)
		tracker_thumbnailer_move_add (fs->priv->thumbnailer, source_uri,
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
	                        "DELETE { "
	                        "  <%s> nfo:fileName ?f ; "
	                        "       nie:url ?u ; "
	                        "       nie:isStoredAs ?s ; "
	                        "       nfo:belongsToContainer ?b"
	                        "} WHERE { "
	                        "  <%s> nfo:fileName ?f ; "
	                        "       nie:url ?u ; "
	                        "       nie:isStoredAs ?s . "
	                        "       OPTIONAL { <%s> nfo:belongsToContainer ?b }"
	                        "} ",
	                        source_iri, source_iri, source_iri);

	display_name = tracker_sparql_escape_string (g_file_info_get_display_name (file_info));

	/* Get new parent information */
	new_parent = g_file_get_parent (file);
	new_parent_iri = lookup_file_urn (fs, new_parent, TRUE);

	if (new_parent && new_parent_iri) {
		g_string_append_printf (sparql,
		                        "INSERT INTO <%s> {"
		                        "  <%s> nfo:fileName \"%s\" ; "
		                        "       nie:url \"%s\" ; "
		                        "       nie:isStoredAs <%s> ; "
		                        "       nfo:belongsToContainer \"%s\""
		                        "}"   ,
		                        TRACKER_OWN_GRAPH_URN, source_iri,
		                        display_name, uri,
		                        source_iri,
		                        new_parent_iri);
	} else {
		g_warning ("Adding moved item '%s' without nfo:belongsToContainer (new_parent: %p)",
		           uri, new_parent);
		g_string_append_printf (sparql,
		                        "INSERT INTO <%s> {"
		                        "  <%s> nfo:fileName \"%s\" ; "
		                        "       nie:url \"%s\" ; "
		                        "       nie:isStoredAs <%s>"
		                        "} ",
		                        TRACKER_OWN_GRAPH_URN, source_iri,
		                        display_name, uri,
		                        source_iri);
	}

	if (new_parent)
		g_object_unref (new_parent);
	g_free (display_name);

	tracker_indexing_tree_get_root (fs->priv->indexing_tree, source_file, &source_flags);

	if ((source_flags & TRACKER_DIRECTORY_FLAG_RECURSE) != 0 &&
	    g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY) {
		tracker_indexing_tree_get_root (fs->priv->indexing_tree,
		                                file, &flags);

		if ((flags & TRACKER_DIRECTORY_FLAG_RECURSE) != 0) {
			if (fs->priv->thumbnailer) {
				ThumbnailMoveData move_data;
				gchar *query;

				g_debug ("Moving thumbnails within '%s'", uri);

				/* Push all moved files to thumbnailer */
				move_data.main_loop = g_main_loop_new (NULL, FALSE);
				move_data.miner = TRACKER_MINER (fs);

				query = g_strdup_printf ("SELECT ?url ?new_url nie:mimeType(?u) {"
				                         "  ?u a rdfs:Resource ;"
				                         "     nie:url ?url ."
				                         "  BIND (CONCAT (\"%s/\", SUBSTR (?url, STRLEN (\"%s/\") + 1)) AS ?new_url) ."
				                         "  FILTER (STRSTARTS (?url, \"%s/\"))"
				                         "}",
				                         uri, source_uri, source_uri);

				tracker_sparql_connection_query_async (tracker_miner_get_connection (TRACKER_MINER (fs)),
				                                       query,
				                                       NULL,
				                                       move_thumbnails_cb,
				                                       &move_data);

				g_main_loop_run (move_data.main_loop);
				g_main_loop_unref (move_data.main_loop);
				g_free (query);
			}

			g_string_append_printf (sparql,
			                        " DELETE {"
			                        "  ?u nie:url ?url "
			                        "} INSERT { "
			                        "  GRAPH <" TRACKER_OWN_GRAPH_URN "> {"
			                        "    ?u nie:url ?new_url "
			                        "  }"
			                        "} WHERE {"
			                        "  ?u a rdfs:Resource;"
			                        "     nie:url ?url ."
			                        "  BIND (CONCAT (\"%s/\", SUBSTR (?url, STRLEN (\"%s/\") + 1)) AS ?new_url) ."
			                        "  FILTER (STRSTARTS (?url, \"%s/\"))"
			                        "} ",
			                        uri, source_uri, source_uri);
		} else {
			/* A directory is being moved from a recursive location to
			 * a non-recursive one, mark all children as deleted.
			 */
			item_remove (fs, source_file, TRUE);
		}
	}

	/* Add new task to processing pool */
	task = tracker_sparql_task_new_take_sparql_str (file,
	                                                g_string_free (sparql,
	                                                               FALSE));
	tracker_sparql_buffer_push (fs->priv->sparql_buffer,
	                            task,
	                            G_PRIORITY_DEFAULT,
	                            sparql_buffer_task_finished_cb,
	                            fs);
	tracker_task_unref (task);

	if (!tracker_task_pool_limit_reached (TRACKER_TASK_POOL (fs->priv->sparql_buffer))) {
		item_queue_handlers_set_up (fs);
	}

	g_free (uri);
	g_free (source_uri);
	g_object_unref (file_info);

	return TRUE;
}

static gboolean
check_ignore_next_update (TrackerMinerFS *fs, GFile *queue_file)
{
	gchar *uri = g_file_get_uri (queue_file);
	if (g_hash_table_lookup (fs->priv->items_ignore_next_update, uri)) {
		g_free (uri);
		return TRUE;
	}
	g_free (uri);
	return FALSE;
}

static gboolean
should_wait (TrackerMinerFS *fs,
             GFile          *file)
{
	GFile *parent;

	/* Is the item already being processed? */
	if (tracker_task_pool_find (fs->priv->task_pool, file) ||
	    tracker_task_pool_find (fs->priv->writeback_pool, file) ||
	    tracker_task_pool_find (TRACKER_TASK_POOL (fs->priv->sparql_buffer), file)) {
		/* Yes, a previous event on same item currently
		 * being processed */
		fs->priv->item_queue_blocker = g_object_ref (file);
		return TRUE;
	}

	/* Is the item's parent being processed right now? */
	parent = g_file_get_parent (file);
	if (parent) {
		if (tracker_task_pool_find (fs->priv->task_pool, parent) ||
		    tracker_task_pool_find (TRACKER_TASK_POOL (fs->priv->sparql_buffer), parent)) {
			/* Yes, a previous event on the parent of this item
			 * currently being processed */
			fs->priv->item_queue_blocker = parent;
			return TRUE;
		}

		g_object_unref (parent);
	}
	return FALSE;
}

static gboolean
item_enqueue_again (TrackerMinerFS       *fs,
                    TrackerPriorityQueue *item_queue,
                    GFile                *queue_file,
                    gint                  priority)
{
	gint reentry_counter;
	gchar *uri;
	gboolean should_wait;

	reentry_counter = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (queue_file),
	                                                       fs->priv->quark_reentry_counter));

	if (reentry_counter <= REENTRY_MAX) {
		g_object_set_qdata (G_OBJECT (queue_file),
		                    fs->priv->quark_reentry_counter,
		                    GINT_TO_POINTER (reentry_counter + 1));
		tracker_priority_queue_add (item_queue, g_object_ref (queue_file), priority);

		should_wait = TRUE;
	} else {
		uri = g_file_get_uri (queue_file);
		g_warning ("File '%s' has been reenqueued more than %d times. It will not be indexed.", uri, REENTRY_MAX);
		g_free (uri);

		/* We must be careful not to return QUEUE_WAIT when there's actually
		 * nothing left to wait for, or the crawling might never complete.
		 */
		if (tracker_miner_fs_has_items_to_process (fs)) {
			should_wait = TRUE;
		} else {
			should_wait = FALSE;
		}
	}

	return should_wait;
}

static QueueState
item_queue_get_next_file (TrackerMinerFS  *fs,
                          GFile          **file,
                          GFile          **source_file,
                          gint            *priority_out)
{
	ItemMovedData *data;
	ItemWritebackData *wdata;
	GFile *queue_file;
	gint priority;

	/* Writeback items first */
	wdata = tracker_priority_queue_pop (fs->priv->items_writeback,
	                                    &priority);
	if (wdata) {
		gboolean processing;

		*file = g_object_ref (wdata->file);
		*source_file = NULL;
		*priority_out = priority;

		trace_eq_pop_head ("WRITEBACK", wdata->file);

		g_signal_emit (fs, signals[WRITEBACK_FILE], 0,
		               wdata->file,
		               wdata->rdf_types,
		               wdata->results,
		               wdata->cancellable,
		               &processing);

		if (processing) {
			TrackerTask *task;

			task = tracker_task_new (wdata->file, wdata,
			                         (GDestroyNotify) item_writeback_data_free);
			tracker_task_pool_add (fs->priv->writeback_pool, task);

			return QUEUE_WRITEBACK;
		} else {
			item_writeback_data_free (wdata);
		}
	}

	/* Deleted items second */
	queue_file = tracker_priority_queue_peek (fs->priv->items_deleted,
	                                          &priority);
	if (queue_file) {
		*source_file = NULL;

		trace_eq_pop_head ("DELETED", queue_file);

		/* Do not ignore DELETED event even if file is marked as
		   IgnoreNextUpdate. We should never see DELETED on update
		   (atomic rename or in-place update) but we may see DELETED
		   due to actual file deletion right after update. */

		/* If the same item OR its first parent is currently being processed,
		 * we need to wait for this event */
		if (should_wait (fs, queue_file)) {
			*file = NULL;

			trace_eq_push_head ("DELETED", queue_file, "Should wait");
			return QUEUE_WAIT;
		}

		tracker_priority_queue_pop (fs->priv->items_deleted, NULL);
		*file = queue_file;
		*priority_out = priority;
		return QUEUE_DELETED;
	}

	/* Created items next */
	queue_file = tracker_priority_queue_peek (fs->priv->items_created,
	                                          &priority);
	if (queue_file) {
		*source_file = NULL;

		trace_eq_pop_head ("CREATED", queue_file);

		/* Note:
		 * We won't be considering an IgnoreNextUpdate request if
		 * the event being processed is a CREATED event and the
		 * file was still unknown to tracker.
		 */
		if (check_ignore_next_update (fs, queue_file)) {
			gchar *uri;

			uri = g_file_get_uri (queue_file);

			if (lookup_file_urn (fs, queue_file, FALSE) != NULL) {
				g_debug ("CREATED event ignored on file '%s' as it already existed, "
				         " processing as IgnoreNextUpdate...",
				         uri);
				g_free (uri);

				return QUEUE_IGNORE_NEXT_UPDATE;
			} else {
				/* Just remove the IgnoreNextUpdate request */
				g_debug ("Skipping the IgnoreNextUpdate request on CREATED event for '%s', file is actually new",
				         uri);
				g_hash_table_remove (fs->priv->items_ignore_next_update, uri);
				g_free (uri);
			}
		}

		/* If the same item OR its first parent is currently being processed,
		 * we need to wait for this event */
		if (should_wait (fs, queue_file)) {
			*file = NULL;

			trace_eq_push_head ("CREATED", queue_file, "Should wait");
			return QUEUE_WAIT;
		}

		tracker_priority_queue_pop (fs->priv->items_created, NULL);
		*file = queue_file;
		*priority_out = priority;
		return QUEUE_CREATED;
	}

	/* Updated items next */
	queue_file = tracker_priority_queue_peek (fs->priv->items_updated,
	                                          &priority);
	if (queue_file) {
		*file = queue_file;
		*source_file = NULL;

		trace_eq_pop_head ("UPDATED", queue_file);

		if (check_ignore_next_update (fs, queue_file)) {
			gchar *uri;

			uri = g_file_get_uri (queue_file);
			g_debug ("UPDATED event ignored on file '%s', "
			         " processing as IgnoreNextUpdate...",
			         uri);
			g_free (uri);

			return QUEUE_IGNORE_NEXT_UPDATE;
		}

		/* If the same item OR its first parent is currently being processed,
		 * we need to wait for this event */
		if (should_wait (fs, queue_file)) {
			*file = NULL;

			trace_eq_push_head ("UPDATED", queue_file, "Should wait");
			return QUEUE_WAIT;
		}

		tracker_priority_queue_pop (fs->priv->items_updated, NULL);
		*priority_out = priority;

		return QUEUE_UPDATED;
	}

	/* Moved items next */
	data = tracker_priority_queue_peek (fs->priv->items_moved,
	                                    &priority);
	if (data) {
		trace_eq_pop_head_2 ("MOVED", data->file, data->source_file);

		if (check_ignore_next_update (fs, data->file)) {
			gchar *uri;
			gchar *source_uri;

			uri = g_file_get_uri (queue_file);
			source_uri = g_file_get_uri (data->source_file);
			g_debug ("MOVED event ignored on files '%s->%s', "
			         " processing as IgnoreNextUpdate on '%s'",
			         source_uri, uri, uri);
			g_free (uri);
			g_free (source_uri);

			*file = g_object_ref (data->file);
			*source_file = g_object_ref (data->source_file);
			item_moved_data_free (data);
			return QUEUE_IGNORE_NEXT_UPDATE;
		}

		/* If the same item OR its first parent is currently being processed,
		 * we need to wait for this event */
		if (should_wait (fs, data->file) ||
		    should_wait (fs, data->source_file)) {
			*file = NULL;
			*source_file = NULL;

			trace_eq_push_head_2 ("MOVED", data->source_file, data->file, "Should wait");
			return QUEUE_WAIT;
		}

		tracker_priority_queue_pop (fs->priv->items_moved, NULL);
		*file = g_object_ref (data->file);
		*source_file = g_object_ref (data->source_file);
		*priority_out = priority;
		item_moved_data_free (data);
		return QUEUE_MOVED;
	}

	*file = NULL;
	*source_file = NULL;

	if (tracker_file_notifier_is_active (fs->priv->file_notifier) ||
	    tracker_task_pool_limit_reached (fs->priv->task_pool) ||
	    tracker_task_pool_limit_reached (TRACKER_TASK_POOL (fs->priv->sparql_buffer))) {
		if (tracker_task_pool_get_size (fs->priv->task_pool) == 0) {
			fs->priv->extraction_timer_stopped = TRUE;
			g_timer_stop (fs->priv->extraction_timer);
		}

		/* There are still pending items to crawl,
		 * or extract pool limit is reached
		 */
		return QUEUE_WAIT;
	}

	return QUEUE_NONE;
}

static gdouble
item_queue_get_progress (TrackerMinerFS *fs,
                         guint          *n_items_processed,
                         guint          *n_items_remaining)
{
	guint items_to_process = 0;
	guint items_total = 0;

	items_to_process += tracker_priority_queue_get_length (fs->priv->items_deleted);
	items_to_process += tracker_priority_queue_get_length (fs->priv->items_created);
	items_to_process += tracker_priority_queue_get_length (fs->priv->items_updated);
	items_to_process += tracker_priority_queue_get_length (fs->priv->items_moved);
	items_to_process += tracker_priority_queue_get_length (fs->priv->items_writeback);

	items_total += fs->priv->total_directories_found;
	items_total += fs->priv->total_files_found;

	if (n_items_processed) {
		*n_items_processed = ((items_total >= items_to_process) ?
		                      (items_total - items_to_process) : 0);
	}

	if (n_items_remaining) {
		*n_items_remaining = items_to_process;
	}

	if (items_total == 0 ||
	    items_to_process == 0 ||
	    items_to_process > items_total) {
		return 1.0;
	}

	return (gdouble) (items_total - items_to_process) / items_total;
}

static gboolean
item_queue_handlers_cb (gpointer user_data)
{
	TrackerMinerFS *fs = user_data;
	GFile *file = NULL;
	GFile *source_file = NULL;
	GFile *parent;
	QueueState queue;
	GTimeVal time_now;
	static GTimeVal time_last = { 0 };
	gboolean keep_processing = TRUE;
	gint priority = 0;

	if (fs->priv->timer_stopped) {
		g_timer_start (fs->priv->timer);
		fs->priv->timer_stopped = FALSE;
	}

	if (tracker_task_pool_limit_reached (TRACKER_TASK_POOL (fs->priv->sparql_buffer))) {
		/* Task pool is full, give it a break */
		fs->priv->item_queues_handler_id = 0;
		return FALSE;
	}

	queue = item_queue_get_next_file (fs, &file, &source_file, &priority);

	if (queue == QUEUE_WAIT) {
		/* Items are still being processed, so wait until
		 * the processing pool is cleared before starting with
		 * the next directories batch.
		 */
		fs->priv->item_queues_handler_id = 0;

		/* We should flush the processing pool buffer here, because
		 * if there was a previous task on the same file we want to
		 * process now, we want it to get finished before we can go
		 * on with the queues... */
		tracker_sparql_buffer_flush (fs->priv->sparql_buffer,
		                             "Queue handlers WAIT");

		/* Check if we've finished inserting for given prefixes ... */
		notify_roots_finished (fs, TRUE);

		return FALSE;
	}

	if (queue == QUEUE_NONE) {
		g_timer_stop (fs->priv->extraction_timer);
		fs->priv->extraction_timer_stopped = TRUE;
	} else if (fs->priv->extraction_timer_stopped) {
		g_timer_continue (fs->priv->extraction_timer);
		fs->priv->extraction_timer_stopped = FALSE;
	}

	/* Update progress, but don't spam it. */
	g_get_current_time (&time_now);

	if ((time_now.tv_sec - time_last.tv_sec) >= 1) {
		guint items_processed, items_remaining;
		gdouble progress_now;
		static gdouble progress_last = 0.0;
		static gint info_last = 0;
		gdouble seconds_elapsed, extraction_elapsed;

		time_last = time_now;

		/* Update progress? */
		progress_now = item_queue_get_progress (fs,
		                                        &items_processed,
		                                        &items_remaining);
		seconds_elapsed = g_timer_elapsed (fs->priv->timer, NULL);
		extraction_elapsed = g_timer_elapsed (fs->priv->extraction_timer, NULL);

		if (!tracker_file_notifier_is_active (fs->priv->file_notifier)) {
			gchar *status;
			gint remaining_time;

			g_object_get (fs, "status", &status, NULL);

			/* Compute remaining time */
			remaining_time = (gint)tracker_seconds_estimate (extraction_elapsed,
			                                                 items_processed,
			                                                 items_remaining);

			/* CLAMP progress so it doesn't go back below
			 * 2% (which we use for crawling)
			 */
			if (g_strcmp0 (status, "Processing…") != 0) {
				/* Don't spam this */
				g_info ("Processing…");
				g_object_set (fs,
				              "status", "Processing…",
				              "progress", CLAMP (progress_now, 0.02, 1.00),
				              "remaining-time", remaining_time,
				              NULL);
			} else {
				g_object_set (fs,
				              "progress", CLAMP (progress_now, 0.02, 1.00),
				              "remaining-time", remaining_time,
				              NULL);
			}

			g_free (status);
		}

		if (++info_last >= 5 &&
		    (gint) (progress_last * 100) != (gint) (progress_now * 100)) {
			gchar *str1, *str2;

			info_last = 0;
			progress_last = progress_now;

			/* Log estimated remaining time */
			str1 = tracker_seconds_estimate_to_string (extraction_elapsed,
			                                           TRUE,
			                                           items_processed,
			                                           items_remaining);
			str2 = tracker_seconds_to_string (seconds_elapsed, TRUE);

			g_info ("Processed %u/%u, estimated %s left, %s elapsed",
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
		if (!tracker_file_notifier_is_active (fs->priv->file_notifier) &&
		    tracker_task_pool_get_size (fs->priv->task_pool) == 0) {
			if (tracker_task_pool_get_size (TRACKER_TASK_POOL (fs->priv->sparql_buffer)) == 0) {
				/* Print stats and signal finished */
				process_stop (fs);

				if (fs->priv->thumbnailer)
					tracker_thumbnailer_send (fs->priv->thumbnailer);
#ifdef HAVE_LIBMEDIAART
				tracker_media_art_queue_empty (tracker_miner_get_connection (TRACKER_MINER (fs)));
#endif
			} else {
				/* Flush any possible pending update here */
				tracker_sparql_buffer_flush (fs->priv->sparql_buffer,
				                             "Queue handlers NONE");

				/* Check if we've finished inserting for given prefixes ... */
				notify_roots_finished (fs, TRUE);
			}
		}

		/* No more files left to process */
		keep_processing = FALSE;
		break;
	case QUEUE_MOVED:
		keep_processing = item_move (fs, file, source_file);
		break;
	case QUEUE_DELETED:
		keep_processing = item_remove (fs, file, FALSE);
		break;
	case QUEUE_CREATED:
	case QUEUE_UPDATED:
		parent = g_file_get_parent (file);

		if (!parent ||
		    tracker_indexing_tree_file_is_root (fs->priv->indexing_tree, file) ||
		    !tracker_indexing_tree_get_root (fs->priv->indexing_tree, file, NULL) ||
		    lookup_file_urn (fs, parent, TRUE)) {
			keep_processing = item_add_or_update (fs, file, priority);
		} else {
			TrackerPriorityQueue *item_queue;
			gchar *uri;

			uri = g_file_get_uri (parent);
			g_message ("Parent '%s' not indexed yet", uri);
			g_free (uri);

			if (queue == QUEUE_CREATED) {
				item_queue = fs->priv->items_created;
			} else {
				item_queue = fs->priv->items_updated;
			}

			/* Parent isn't indexed yet, reinsert the task into the queue,
			 * but forcily prepended by its parent so its indexing is
			 * ensured, tasks are inserted at a higher priority so they
			 * are processed promptly anyway.
			 */
			item_enqueue_again (fs, item_queue, parent, priority - 1);
			item_enqueue_again (fs, item_queue, file, priority);

			keep_processing = TRUE;
		}

		if (parent) {
			g_object_unref (parent);
		}

		break;
	case QUEUE_IGNORE_NEXT_UPDATE:
		keep_processing = item_ignore_next_update (fs, file, source_file);
		break;
	case QUEUE_WRITEBACK:
		/* Nothing to do here */
		keep_processing = TRUE;
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
		fs->priv->item_queues_handler_id = 0;
		return FALSE;
	} else {
		return TRUE;
	}
}

static guint
_tracker_idle_add (TrackerMinerFS *fs,
                   GSourceFunc     func,
                   gpointer        user_data)
{
	guint interval;

	interval = TRACKER_CRAWLER_MAX_TIMEOUT_INTERVAL * fs->priv->throttle;

	if (interval == 0) {
		return g_idle_add_full (TRACKER_TASK_PRIORITY, func, user_data, NULL);
	} else {
		return g_timeout_add_full (TRACKER_TASK_PRIORITY, interval, func, user_data, NULL);
	}
}

static void
item_queue_handlers_set_up (TrackerMinerFS *fs)
{
	trace_eq ("Setting up queue handlers...");
	if (fs->priv->item_queues_handler_id != 0) {
		trace_eq ("   cancelled: already one active");
		return;
	}

	if (fs->priv->is_paused) {
		trace_eq ("   cancelled: paused");
		return;
	}

	if (fs->priv->item_queue_blocker) {
		trace_eq ("   cancelled: item queue blocked waiting for file '%s'",
		          g_file_get_uri (fs->priv->item_queue_blocker));
		return;
	}

	/* Already sent max number of tasks to tracker-extract/writeback? */
	if (tracker_task_pool_limit_reached (fs->priv->task_pool) ||
	    tracker_task_pool_limit_reached (fs->priv->writeback_pool)) {
		trace_eq ("   cancelled: pool limit reached (tasks: %u (max %u) , writeback: %u (max %u))",
		          tracker_task_pool_get_size (fs->priv->task_pool),
		          tracker_task_pool_get_limit (fs->priv->task_pool),
		          tracker_task_pool_get_size (fs->priv->writeback_pool),
		          tracker_task_pool_get_limit (fs->priv->writeback_pool));
		return;
	}

	if (tracker_task_pool_limit_reached (TRACKER_TASK_POOL (fs->priv->sparql_buffer))) {
		trace_eq ("   cancelled: pool limit reached (sparql buffer: %u)",
		          tracker_task_pool_get_limit (TRACKER_TASK_POOL (fs->priv->sparql_buffer)));
		return;
	}

	if (!tracker_file_notifier_is_active (fs->priv->file_notifier)) {
		gchar *status;
		gdouble progress;

		g_object_get (fs,
		              "progress", &progress,
		              "status", &status,
		              NULL);

		/* Don't spam this */
		if (progress > 0.01 && g_strcmp0 (status, "Processing…") != 0) {
			g_info ("Processing…");
			g_object_set (fs, "status", "Processing…", NULL);
		}

		g_free (status);
	}

	trace_eq ("   scheduled in idle");
	fs->priv->item_queues_handler_id =
		_tracker_idle_add (fs,
		                   item_queue_handlers_cb,
		                   fs);
}

static gboolean
should_check_file (TrackerMinerFS *fs,
                   GFile          *file,
                   gboolean        is_dir)
{
	GFileType file_type;

	file_type = (is_dir) ? G_FILE_TYPE_DIRECTORY : G_FILE_TYPE_REGULAR;
	return tracker_indexing_tree_file_is_indexable (fs->priv->indexing_tree,
	                                                file, file_type);
}

static gboolean
moved_files_equal (gconstpointer a,
                   gconstpointer b)
{
	const ItemMovedData *data = a;
	GFile *file = G_FILE (b);

	/* Compare with dest file */
	return g_file_equal (data->file, file);
}

static gboolean
writeback_files_equal (gconstpointer a,
                       gconstpointer b)
{
	const ItemWritebackData *data = a;
	GFile *file = G_FILE (b);

	/* Compare with dest file */
	return g_file_equal (data->file, file);
}

static gboolean
remove_writeback_task (TrackerMinerFS *fs,
                       GFile          *file)
{
	TrackerTask *task;
	ItemWritebackData *data;

	task = tracker_task_pool_find (fs->priv->writeback_pool, file);

	if (!task) {
		return FALSE;
	}

	data = tracker_task_get_data (task);

	if (data->notified) {
		tracker_task_pool_remove (fs->priv->writeback_pool, task);
		tracker_task_unref (task);
		return TRUE;
	}

	return FALSE;
}

static void
cancel_writeback_task (TrackerMinerFS *fs,
                       GFile          *file)
{
	TrackerTask *task;

	task = tracker_task_pool_find (fs->priv->writeback_pool, file);

	if (task) {
		ItemWritebackData *data;

		data = tracker_task_get_data (task);
		g_cancellable_cancel (data->cancellable);
		tracker_task_pool_remove (fs->priv->writeback_pool, task);
		tracker_task_unref (task);
	}
}

static gint
miner_fs_get_queue_priority (TrackerMinerFS *fs,
                             GFile          *file)
{
	TrackerDirectoryFlags flags;

	tracker_indexing_tree_get_root (fs->priv->indexing_tree,
	                                file, &flags);

	return (flags & TRACKER_DIRECTORY_FLAG_PRIORITY) ?
	        G_PRIORITY_HIGH : G_PRIORITY_DEFAULT;
}

static void
miner_fs_cache_file_urn (TrackerMinerFS *fs,
                         GFile          *file,
                         gboolean        query_urn)
{
	const gchar *urn;

	/* Store urn as qdata */
	urn = tracker_file_notifier_get_file_iri (fs->priv->file_notifier, file, query_urn);
	g_object_set_qdata_full (G_OBJECT (file), quark_file_iri,
	                         g_strdup (urn), (GDestroyNotify) g_free);
}

static void
miner_fs_queue_file (TrackerMinerFS       *fs,
                     TrackerPriorityQueue *item_queue,
                     GFile                *file,
                     gboolean              query_urn)
{
	gint priority;

	miner_fs_cache_file_urn (fs, file, query_urn);
	priority = miner_fs_get_queue_priority (fs, file);
	tracker_priority_queue_add (item_queue, g_object_ref (file), priority);
}

/* Checks previous created/updated/deleted/moved/writeback queues for
 * monitor events. Returns TRUE if the item should still
 * be added to the queue.
 */
static gboolean
check_item_queues (TrackerMinerFS *fs,
                   QueueState      queue,
                   GFile          *file,
                   GFile          *other_file)
{
	ItemMovedData *move_data;

	if (!fs->priv->been_crawled) {
		/* Only do this after initial crawling, so
		 * we are mostly sure that we won't be doing
		 * checks on huge lists.
		 */
		return TRUE;
	}

	if (queue == QUEUE_UPDATED) {
		TrackerTask *task;

		if (other_file) {
			task = tracker_task_pool_find (fs->priv->writeback_pool, other_file);
		} else {
			task = tracker_task_pool_find (fs->priv->writeback_pool, file);
		}

		if (task) {
			/* There is a writeback task for
			 * this file, so avoid any updates
			 */
			return FALSE;
		}
	}

	switch (queue) {
	case QUEUE_CREATED:
		/* Created items aren't likely to have
		 * anything in other queues for the same
		 * file.
		 */
		return TRUE;
	case QUEUE_UPDATED:
		/* No further updates after a previous created/updated event */
		if (tracker_priority_queue_find (fs->priv->items_created, NULL,
		                                 (GEqualFunc) g_file_equal, file) ||
		    tracker_priority_queue_find (fs->priv->items_updated, NULL,
		                                 (GEqualFunc) g_file_equal, file)) {
			g_debug ("  Found previous unhandled CREATED/UPDATED event");
			return FALSE;
		}
		return TRUE;
	case QUEUE_WRITEBACK:
		/* No consecutive writebacks for the same file */
		if (tracker_priority_queue_find (fs->priv->items_writeback, NULL,
		                                 writeback_files_equal, file)) {
			g_debug ("  Found previous unhandled WRITEBACK event");
			return FALSE;
		}

		return TRUE;
	case QUEUE_DELETED:
		if (tracker_task_pool_find (fs->priv->writeback_pool, file)) {
			/* Cancel writeback operations on a deleted file */
			cancel_writeback_task (fs, file);
		}

		if (tracker_file_notifier_get_file_type (fs->priv->file_notifier,
		                                         file) == G_FILE_TYPE_DIRECTORY) {
			if (tracker_priority_queue_foreach_remove (fs->priv->items_updated,
			                                           (GEqualFunc) g_file_has_prefix,
			                                           file,
			                                           (GDestroyNotify) g_object_unref)) {
				g_debug ("  Deleting previous unhandled UPDATED events on children");
			}

			if (tracker_priority_queue_foreach_remove (fs->priv->items_created,
			                                           (GEqualFunc) g_file_has_prefix,
			                                           file,
			                                           (GDestroyNotify) g_object_unref)) {
				g_debug ("  Deleting previous unhandled CREATED events on children");
			}

			if (tracker_priority_queue_foreach_remove (fs->priv->items_deleted,
			                                           (GEqualFunc) g_file_has_prefix,
			                                           file,
			                                           (GDestroyNotify) g_object_unref)) {
				g_debug ("  Deleting previous unhandled DELETED events on children");
			}
		}

		/* Remove all previous updates */
		if (tracker_priority_queue_foreach_remove (fs->priv->items_updated,
		                                           (GEqualFunc) g_file_equal,
		                                           file,
		                                           (GDestroyNotify) g_object_unref)) {
			g_debug ("  Deleting previous unhandled UPDATED event");
		}

		if (tracker_priority_queue_foreach_remove (fs->priv->items_created,
		                                           (GEqualFunc) g_file_equal,
		                                           file,
		                                           (GDestroyNotify) g_object_unref)) {
			/* Created event was still in the queue,
			 * remove it and ignore the current event
			 */
			g_debug ("  Found matching unhandled CREATED event, removing file altogether");
			return FALSE;
		}

		return TRUE;
	case QUEUE_MOVED:
		if (tracker_task_pool_find (fs->priv->writeback_pool, file)) {
			/* If the origin file is also being written back,
			 * cancel it as this is an external operation.
			 */
			cancel_writeback_task (fs, file);
		}

		/* Kill any events on other_file (The dest one), since it will be rewritten anyway */
		if (tracker_priority_queue_foreach_remove (fs->priv->items_created,
		                                           (GEqualFunc) g_file_equal,
		                                           other_file,
		                                           (GDestroyNotify) g_object_unref)) {
			g_debug ("  Removing previous unhandled CREATED event for dest file, will be rewritten anyway");
		}

		if (tracker_priority_queue_foreach_remove (fs->priv->items_updated,
		                                           (GEqualFunc) g_file_equal,
		                                           other_file,
		                                           (GDestroyNotify) g_object_unref)) {
			g_debug ("  Removing previous unhandled UPDATED event for dest file, will be rewritten anyway");
		}

		/* Now check file (Origin one) */
		if (tracker_priority_queue_foreach_remove (fs->priv->items_created,
		                                           (GEqualFunc) g_file_equal,
		                                           file,
		                                           (GDestroyNotify) g_object_unref)) {
			/* If source file was created, replace it with
			 * a create event for the destination file, and
			 * discard this event.
			 *
			 * We assume all posterior updates
			 * have been merged together previously by this
			 * same function.
			 */
			g_debug ("  Found matching unhandled CREATED event "
			         "for source file, merging both events together");
			miner_fs_queue_file (fs, fs->priv->items_created, other_file, FALSE);

			return FALSE;
		}

		move_data = tracker_priority_queue_find (fs->priv->items_moved, NULL,
		                                         (GEqualFunc) moved_files_equal, file);
		if (move_data) {
			/* Origin file was the dest of a previous
			 * move operation, merge these together.
			 */
			g_debug ("  Source file is the destination of a previous "
			         "unhandled MOVED event, merging both events together");
			g_object_unref (move_data->file);
			move_data->file = g_object_ref (other_file);
			return FALSE;
		}

		return TRUE;
		break;
	default:
		g_assert_not_reached ();
	}

	return TRUE;
}

static void
file_notifier_file_created (TrackerFileNotifier  *notifier,
                            GFile                *file,
                            gpointer              user_data)
{
	TrackerMinerFS *fs = user_data;

	if (check_item_queues (fs, QUEUE_CREATED, file, NULL)) {
		miner_fs_queue_file (fs, fs->priv->items_created, file, FALSE);
		item_queue_handlers_set_up (fs);
	}
}

static void
file_notifier_file_deleted (TrackerFileNotifier  *notifier,
                            GFile                *file,
                            gpointer              user_data)
{
	TrackerMinerFS *fs = user_data;

	if (tracker_file_notifier_get_file_type (notifier, file) == G_FILE_TYPE_DIRECTORY) {
		/* Cancel all pending tasks on files inside the path given by file */
		tracker_task_pool_foreach (fs->priv->task_pool,
					   task_pool_cancel_foreach,
					   file);
		tracker_task_pool_foreach (fs->priv->writeback_pool,
					   writeback_pool_cancel_foreach,
					   file);
	}

	if (check_item_queues (fs, QUEUE_DELETED, file, NULL)) {
		miner_fs_queue_file (fs, fs->priv->items_deleted, file, FALSE);
		item_queue_handlers_set_up (fs);
	}
}

static void
file_notifier_file_updated (TrackerFileNotifier  *notifier,
                            GFile                *file,
                            gboolean              attributes_only,
                            gpointer              user_data)
{
	TrackerMinerFS *fs = user_data;

	/* Writeback tasks would receive an updated after move,
	 * consequence of the data being written back in the
	 * copy, and its monitor events being propagated to
	 * the destination file.
	 */
	if (!attributes_only &&
	    remove_writeback_task (fs, file)) {
		item_queue_handlers_set_up (fs);
		return;
	}

	if (check_item_queues (fs, QUEUE_UPDATED, file, NULL)) {
		if (attributes_only) {
			g_object_set_qdata (G_OBJECT (file),
			                    fs->priv->quark_attribute_updated,
			                    GINT_TO_POINTER (TRUE));
		}

		miner_fs_queue_file (fs, fs->priv->items_updated, file, TRUE);
		item_queue_handlers_set_up (fs);
	}
}

static void
file_notifier_file_moved (TrackerFileNotifier *notifier,
                          GFile               *source,
                          GFile               *dest,
                          gpointer             user_data)
{
	TrackerMinerFS *fs = user_data;

	if (check_item_queues (fs, QUEUE_MOVED, source, dest)) {
		gint priority;

		priority = miner_fs_get_queue_priority (fs, dest);
		tracker_priority_queue_add (fs->priv->items_moved,
		                            item_moved_data_new (dest, source),
					    priority);
		item_queue_handlers_set_up (fs);
	}
}

static void
file_notifier_directory_started (TrackerFileNotifier *notifier,
                                 GFile               *directory,
                                 gpointer             user_data)
{
	TrackerMinerFS *fs = user_data;
	TrackerDirectoryFlags flags;
	gchar *str, *uri;

	uri = g_file_get_uri (directory);
	tracker_indexing_tree_get_root (fs->priv->indexing_tree,
					directory, &flags);

	if ((flags & TRACKER_DIRECTORY_FLAG_RECURSE) != 0) {
                str = g_strdup_printf ("Crawling recursively directory '%s'", uri);
        } else {
                str = g_strdup_printf ("Crawling single directory '%s'", uri);
        }

	if (fs->priv->timer_stopped) {
		g_timer_start (fs->priv->timer);
		fs->priv->timer_stopped = FALSE;
	}

	if (fs->priv->extraction_timer_stopped) {
		g_timer_start (fs->priv->timer);
		fs->priv->extraction_timer_stopped = FALSE;
	}

	/* Always set the progress here to at least 1%, and the remaining time
         * to -1 as we cannot guess during crawling (we don't know how many directories
         * we will find) */
        g_object_set (fs,
                      "progress", 0.01,
                      "status", str,
                      "remaining-time", -1,
                      NULL);
	g_free (str);
	g_free (uri);
}

static void
file_notifier_directory_finished (TrackerFileNotifier *notifier,
                                  GFile               *directory,
                                  guint                directories_found,
                                  guint                directories_ignored,
                                  guint                files_found,
                                  guint                files_ignored,
                                  gpointer             user_data)
{
	TrackerMinerFS *fs = user_data;
	gchar *str, *uri;

	/* Update stats */
	fs->priv->total_directories_found += directories_found;
	fs->priv->total_directories_ignored += directories_ignored;
	fs->priv->total_files_found += files_found;
	fs->priv->total_files_ignored += files_ignored;

	uri = g_file_get_uri (directory);
	str = g_strdup_printf ("Crawl finished for directory '%s'", uri);

        g_object_set (fs,
                      "progress", 0.01,
                      "status", str,
                      "remaining-time", -1,
                      NULL);

	g_free (str);
	g_free (uri);

	if (directories_found == 0 &&
	    files_found == 0) {
		/* Signal now because we have nothing to index */
		g_signal_emit (fs, signals[FINISHED_ROOT], 0, directory);
	} else {
		/* Add root to list we want to be notified about when
		 * finished indexing! */
		g_hash_table_replace (fs->priv->roots_to_notify,
		                      g_object_ref (directory),
		                      GUINT_TO_POINTER(time(NULL)));
	}
}

static void
file_notifier_finished (TrackerFileNotifier *notifier,
                        gpointer             user_data)
{
	TrackerMinerFS *fs = user_data;

	if (!tracker_miner_fs_has_items_to_process (fs)) {
		g_info ("Finished all tasks");
		process_stop (fs);
	}
}


#ifdef CRAWLED_TREE_ENABLE_TRACE

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

#endif /* CRAWLED_TREE_ENABLE_TRACE */

/* Returns TRUE if file equals to
 * other_file, or is a child of it
 */
static gboolean
file_equal_or_descendant (GFile *file,
                          GFile *prefix)
{
	if (g_file_equal (file, prefix) ||
	    g_file_has_prefix (file, prefix)) {
		return TRUE;
	}

	return FALSE;
}

/**
 * tracker_miner_fs_directory_add:
 * @fs: a #TrackerMinerFS
 * @file: #GFile for the directory to inspect
 * @recurse: whether the directory should be inspected recursively
 *
 * Tells the filesystem miner to inspect a directory.
 *
 * Since: 0.8
 **/
void
tracker_miner_fs_directory_add (TrackerMinerFS *fs,
                                GFile          *file,
                                gboolean        recurse)
{
	TrackerDirectoryFlags flags = TRACKER_DIRECTORY_FLAG_NONE;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));
	g_return_if_fail (G_IS_FILE (file));

	if (recurse) {
		flags |= TRACKER_DIRECTORY_FLAG_RECURSE;
	}

	if (!fs->priv->data_provider) {
		flags |= TRACKER_DIRECTORY_FLAG_MONITOR;
	}

	if (fs->priv->mtime_checking) {
		flags |= TRACKER_DIRECTORY_FLAG_CHECK_MTIME;
	}

	tracker_indexing_tree_add (fs->priv->indexing_tree,
	                           file,
	                           flags);
}

static void
task_pool_cancel_foreach (gpointer data,
                          gpointer user_data)
{
	TrackerTask *task = data;
	GFile *file = user_data;
	GFile *task_file;
	UpdateProcessingTaskContext *ctxt;

	ctxt = tracker_task_get_data (task);
	task_file = tracker_task_get_file (task);

	if (ctxt &&
	    ctxt->cancellable &&
	    (!file ||
	     (g_file_equal (task_file, file) ||
	      g_file_has_prefix (task_file, file)))) {
		g_cancellable_cancel (ctxt->cancellable);
	}
}

static void
writeback_pool_cancel_foreach (gpointer data,
                               gpointer user_data)
{
	GFile *task_file, *file;
	TrackerTask *task;

	task = data;
	file = user_data;
	task_file = tracker_task_get_file (task);

	if (!file ||
	    g_file_equal (task_file, file) ||
	    g_file_has_prefix (task_file, file)) {
		ItemWritebackData *task_data;

		task_data = tracker_task_get_data (task);
		g_cancellable_cancel (task_data->cancellable);
	}
}

static void
indexing_tree_directory_removed (TrackerIndexingTree *indexing_tree,
                                 GFile               *directory,
                                 gpointer             user_data)
{
	TrackerMinerFS *fs = user_data;
	TrackerMinerFSPrivate *priv = fs->priv;
	GTimer *timer = g_timer_new ();

	/* Cancel all pending tasks on files inside the path given by file */
	tracker_task_pool_foreach (priv->task_pool,
	                           task_pool_cancel_foreach,
	                           directory);

	g_debug ("  Cancelled processing pool tasks at %f\n", g_timer_elapsed (timer, NULL));

	tracker_task_pool_foreach (priv->writeback_pool,
	                           writeback_pool_cancel_foreach,
	                           directory);

	g_debug ("  Cancelled writeback pool tasks at %f\n",
	         g_timer_elapsed (timer, NULL));

	/* Remove anything contained in the removed directory
	 * from all relevant processing queues.
	 */
	tracker_priority_queue_foreach_remove (priv->items_updated,
	                                       (GEqualFunc) file_equal_or_descendant,
	                                       directory,
	                                       (GDestroyNotify) g_object_unref);
	tracker_priority_queue_foreach_remove (priv->items_created,
	                                       (GEqualFunc) file_equal_or_descendant,
	                                       directory,
	                                       (GDestroyNotify) g_object_unref);

	g_debug ("  Removed files at %f\n", g_timer_elapsed (timer, NULL));

	g_message ("Finished remove directory operation in %f\n", g_timer_elapsed (timer, NULL));
	g_timer_destroy (timer);
}

/**
 * tracker_miner_fs_directory_remove:
 * @fs: a #TrackerMinerFS
 * @file: #GFile for the directory to be removed
 *
 * Removes a directory from being inspected by @fs. Note that only directory
 *  watches are removed.
 *
 * Returns: %TRUE if the directory was successfully removed.
 *
 * Since: 0.8
 **/
gboolean
tracker_miner_fs_directory_remove (TrackerMinerFS *fs,
                                   GFile          *file)
{
	TrackerMinerFSPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	priv = fs->priv;

	if (!tracker_indexing_tree_file_is_root (priv->indexing_tree, file)) {
		return FALSE;
	}

	g_debug ("Removing directory");
	tracker_indexing_tree_remove (priv->indexing_tree, file);

	return TRUE;
}


/**
 * tracker_miner_fs_directory_remove_full:
 * @fs: a #TrackerMinerFS
 * @file: #GFile for the directory to be removed
 *
 * Removes a directory from being inspected by @fs, and removes all
 * associated metadata of the directory (and its contents) from the
 * store.
 *
 * Returns: %TRUE if the directory was successfully removed.
 *
 * Since: 0.10
 **/
gboolean
tracker_miner_fs_directory_remove_full (TrackerMinerFS *fs,
                                        GFile          *file)
{
	TrackerDirectoryFlags flags;

	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	tracker_indexing_tree_get_root (fs->priv->indexing_tree, file, &flags);

	if (tracker_miner_fs_directory_remove (fs, file)) {
		if ((flags & TRACKER_DIRECTORY_FLAG_PRESERVE) != 0) {
			/* If the preserve flag is unset, TrackerFileNotifier
			 * will delete automatically files from this config
			 * directory, if it's set, we force the delete here
			 * to preserve remove_full() semantics.
			 */
			trace_eq_push_tail ("DELETED", file, "on remove full");
			miner_fs_queue_file (fs, fs->priv->items_deleted, file, FALSE);
			item_queue_handlers_set_up (fs);
		}

		return TRUE;
	}

	return FALSE;
}

static gboolean
check_file_parents (TrackerMinerFS *fs,
                    GFile          *file)
{
	GFile *parent, *root;
	GList *parents = NULL, *p;

	parent = g_file_get_parent (file);

	if (!parent) {
		return FALSE;
	}

	root = tracker_indexing_tree_get_root (fs->priv->indexing_tree,
	                                       parent, NULL);
	if (!root) {
		g_object_unref (parent);
		return FALSE;
	}

	/* Add parent directories until we're past the config dir */
	while (parent &&
	       !g_file_has_prefix (root, parent)) {
		parents = g_list_prepend (parents, parent);
		parent = g_file_get_parent (parent);
	}

	/* Last parent fetched is not added to the list */
	if (parent) {
		g_object_unref (parent);
	}

	for (p = parents; p; p = p->next) {
		trace_eq_push_tail ("UPDATED", p->data, "checking file parents");
		miner_fs_queue_file (fs, fs->priv->items_updated, p->data, TRUE);
		g_object_unref (p->data);
	}

	g_list_free (parents);

	return TRUE;
}

/**
 * tracker_miner_fs_check_file_with_priority:
 * @fs: a #TrackerMinerFS
 * @file: #GFile for the file to check
 * @priority: the priority of the check task
 * @check_parents: whether to check parents and eligibility or not
 *
 * Tells the filesystem miner to check and index a file at
 * a given priority, this file must be part of the usual
 * crawling directories of #TrackerMinerFS. See
 * tracker_miner_fs_directory_add().
 *
 * Since: 0.10
 **/
void
tracker_miner_fs_check_file_with_priority (TrackerMinerFS *fs,
                                           GFile          *file,
                                           gint            priority,
                                           gboolean        check_parents)
{
	gboolean should_process = TRUE;
	gchar *uri;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));
	g_return_if_fail (G_IS_FILE (file));

	if (check_parents) {
		should_process = should_check_file (fs, file, FALSE);
	}

	uri = g_file_get_uri (file);

	g_debug ("%s:'%s' (FILE) (requested by application)",
	         should_process ? "Found " : "Ignored",
	         uri);

	if (should_process) {
		if (check_parents && !check_file_parents (fs, file)) {
			return;
		}

		trace_eq_push_tail ("UPDATED", file, "Requested by application");
		miner_fs_cache_file_urn (fs, file, TRUE);
		tracker_priority_queue_add (fs->priv->items_updated,
		                            g_object_ref (file),
		                            priority);

		item_queue_handlers_set_up (fs);
	}

	g_free (uri);
}


/**
 * tracker_miner_fs_writeback_file:
 * @fs: a #TrackerMinerFS
 * @file: #GFile for the file to check
 * @rdf_types: A #GStrv with rdf types
 * @results: (element-type GStrv): A array of results from the preparation query
 *
 * Tells the filesystem miner to writeback a file.
 *
 * Since: 0.10.20
 **/
void
tracker_miner_fs_writeback_file (TrackerMinerFS *fs,
                                 GFile          *file,
                                 GStrv           rdf_types,
                                 GPtrArray      *results)
{
	gchar *uri;
	ItemWritebackData *data;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));
	g_return_if_fail (G_IS_FILE (file));

	uri = g_file_get_uri (file);

	g_debug ("Performing write-back:'%s' (requested by application)", uri);

	trace_eq_push_tail ("WRITEBACK", file, "Requested by application");

	data = item_writeback_data_new (file, rdf_types, results);
	tracker_priority_queue_add (fs->priv->items_writeback, data,
	                            G_PRIORITY_DEFAULT);

	item_queue_handlers_set_up (fs);

	g_free (uri);
}

/**
 * tracker_miner_fs_writeback_notify:
 * @fs: a #TrackerMinerFS
 * @file: a #GFile
 * @error: a #GError with the error that happened during processing, or %NULL.
 *
 * Notifies @fs that all writing back on @file has been finished, if any error
 * happened during file data processing, it should be passed in @error, else
 * that parameter will contain %NULL to reflect success.
 *
 * Since: 0.10.20
 **/
void
tracker_miner_fs_writeback_notify (TrackerMinerFS *fs,
                                   GFile          *file,
                                   const GError   *error)
{
	TrackerTask *task;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));
	g_return_if_fail (G_IS_FILE (file));

	fs->priv->total_files_notified++;

	task = tracker_task_pool_find (fs->priv->writeback_pool, file);

	if (!task) {
		gchar *uri;

		uri = g_file_get_uri (file);
		g_critical ("%s has notified that file '%s' has been written back, "
		            "but that file was not in the task pool. "
		            "This is an implementation error, please ensure that "
		            "tracker_miner_fs_writeback_notify() is called on the same "
		            "GFile that is passed in ::writeback-file, and that this"
		            "signal didn't return FALSE for it",
		            G_OBJECT_TYPE_NAME (fs), uri);
		g_free (uri);
	} else if (error) {

		if (!(error->domain == TRACKER_DBUS_ERROR &&
		      error->code == TRACKER_DBUS_ERROR_UNSUPPORTED)) {
			g_warning ("Writeback operation failed: %s", error->message);
		}

		/* We don't expect any further monitor
		 * events on the original file.
		 */
		tracker_task_pool_remove (fs->priv->writeback_pool, task);
		tracker_task_unref (task);

		item_queue_handlers_set_up (fs);
	} else {
		ItemWritebackData *data;

		data = tracker_task_get_data (task);
		data->notified = TRUE;
	}

	/* Check monitor_item_updated_cb() for the remainder of this notify,
	 * as the last event happening on the written back file would be an
	 * UPDATED event caused by the changes on the cloned file, followed
	 * by a MOVE onto the original file, so the delayed update happens
	 * on the destination file.
	 */
}

/**
 * tracker_miner_fs_check_file:
 * @fs: a #TrackerMinerFS
 * @file: #GFile for the file to check
 * @check_parents: whether to check parents and eligibility or not
 *
 * Tells the filesystem miner to check and index a file,
 * this file must be part of the usual crawling directories
 * of #TrackerMinerFS. See tracker_miner_fs_directory_add().
 *
 * Since: 0.10
 **/
void
tracker_miner_fs_check_file (TrackerMinerFS *fs,
                             GFile          *file,
                             gboolean        check_parents)
{
	tracker_miner_fs_check_file_with_priority (fs, file,
	                                           G_PRIORITY_HIGH,
	                                           check_parents);
}

/**
 * tracker_miner_fs_check_directory_with_priority:
 * @fs: a #TrackerMinerFS
 * @file: #GFile for the directory to check
 * @priority: the priority of the check task
 * @check_parents: whether to check parents and eligibility or not
 *
 * Tells the filesystem miner to check and index a directory at
 * a given priority, this file must be part of the usual crawling
 * directories of #TrackerMinerFS. See tracker_miner_fs_directory_add().
 *
 * Since: 0.10
 **/
void
tracker_miner_fs_check_directory_with_priority (TrackerMinerFS *fs,
                                                GFile          *file,
                                                gint            priority,
                                                gboolean        check_parents)
{
	gboolean should_process = TRUE;
	gchar *uri;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));
	g_return_if_fail (G_IS_FILE (file));

	if (check_parents) {
		should_process = should_check_file (fs, file, TRUE);
	}

	uri = g_file_get_uri (file);

	g_debug ("%s:'%s' (DIR) (requested by application)",
	         should_process ? "Found " : "Ignored",
	         uri);

	if (should_process) {
		TrackerDirectoryFlags flags;

		if (check_parents && !check_file_parents (fs, file)) {
			return;
		}

		flags = TRACKER_DIRECTORY_FLAG_RECURSE |
			TRACKER_DIRECTORY_FLAG_CHECK_MTIME;

		if (!fs->priv->data_provider) {
			flags |= TRACKER_DIRECTORY_FLAG_MONITOR;
		}

		/* Priorities run from positive to negative */
		if (priority < G_PRIORITY_DEFAULT)
			flags |= TRACKER_DIRECTORY_FLAG_PRIORITY;

		tracker_indexing_tree_add (fs->priv->indexing_tree,
		                           file, flags);
	}

	g_free (uri);
}

/**
 * tracker_miner_fs_check_directory:
 * @fs: a #TrackerMinerFS
 * @file: #GFile for the directory to check
 * @check_parents: whether to check parents and eligibility or not
 *
 * Tells the filesystem miner to check and index a directory,
 * this file must be part of the usual crawling directories
 * of #TrackerMinerFS. See tracker_miner_fs_directory_add().
 *
 * Since: 0.10
 **/
void
tracker_miner_fs_check_directory (TrackerMinerFS *fs,
                                  GFile          *file,
                                  gboolean        check_parents)
{
	tracker_miner_fs_check_directory_with_priority (fs, file,
	                                                G_PRIORITY_HIGH,
	                                                check_parents);
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
 *
 * Since: 0.8
 **/
void
tracker_miner_fs_file_notify (TrackerMinerFS *fs,
                              GFile          *file,
                              const GError   *error)
{
	TrackerTask *task;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));
	g_return_if_fail (G_IS_FILE (file));

	fs->priv->total_files_notified++;

	task = tracker_task_pool_find (fs->priv->task_pool, file);

	if (!task) {
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

		if (item_queue_is_blocked_by_file (fs, file)) {
			/* Ensure we don't stall, although this is a very ugly situation */
			g_object_unref (fs->priv->item_queue_blocker);
			fs->priv->item_queue_blocker = NULL;
			item_queue_handlers_set_up (fs);
		}

		return;
	}

	item_add_or_update_continue (fs, task, error);
}

/**
 * tracker_miner_fs_set_throttle:
 * @fs: a #TrackerMinerFS
 * @throttle: a double between 0.0 and 1.0
 *
 * Tells the filesystem miner to throttle its operations. A value of
 * 0.0 means no throttling at all, so the miner will perform
 * operations at full speed, 1.0 is the slowest value. With a value of
 * 1.0, the @fs is typically waiting one full second before handling
 * the next batch of queued items to be processed.
 *
 * Since: 0.8
 **/
void
tracker_miner_fs_set_throttle (TrackerMinerFS *fs,
                               gdouble         throttle)
{
	g_return_if_fail (TRACKER_IS_MINER_FS (fs));

	throttle = CLAMP (throttle, 0, 1);

	if (fs->priv->throttle == throttle) {
		return;
	}

	fs->priv->throttle = throttle;

	/* Update timeouts */
	if (fs->priv->item_queues_handler_id != 0) {
		g_source_remove (fs->priv->item_queues_handler_id);

		fs->priv->item_queues_handler_id =
			_tracker_idle_add (fs,
			                   item_queue_handlers_cb,
			                   fs);
	}
}

/**
 * tracker_miner_fs_get_throttle:
 * @fs: a #TrackerMinerFS
 *
 * Gets the current throttle value, see
 * tracker_miner_fs_set_throttle() for more details.
 *
 * Returns: a double representing a value between 0.0 and 1.0.
 *
 * Since: 0.8
 **/
gdouble
tracker_miner_fs_get_throttle (TrackerMinerFS *fs)
{
	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), 0);

	return fs->priv->throttle;
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
 * Returns: (transfer none) (nullable): The URN containing the data associated to @file,
 *          or %NULL.
 *
 * Since: 0.8
 **/
const gchar *
tracker_miner_fs_get_urn (TrackerMinerFS *fs,
                          GFile          *file)
{
	TrackerTask *task;

	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), NULL);
	g_return_val_if_fail (G_IS_FILE (file), NULL);

	/* Check if found in currently processed data */
	task = tracker_task_pool_find (fs->priv->task_pool, file);

	if (!task) {
		gchar *uri;

		uri = g_file_get_uri (file);

		g_critical ("File '%s' is not being currently processed, "
		            "so the URN cannot be retrieved.", uri);
		g_free (uri);

		return NULL;
	} else {
		UpdateProcessingTaskContext *ctxt;

		/* We are only storing the URN in the created/updated tasks */
		ctxt = tracker_task_get_data (task);

		if (!ctxt) {
			gchar *uri;

			uri = g_file_get_uri (file);
			g_critical ("File '%s' is being processed, but not as a "
			            "CREATED/UPDATED task, so cannot get URN",
			            uri);
			g_free (uri);
			return NULL;
		}

		return ctxt->urn;
	}
}

/**
 * tracker_miner_fs_query_urn:
 * @fs: a #TrackerMinerFS
 * @file: a #GFile
 *
 * If the item exists in the store, this function retrieves
 * the URN of the given #GFile

 * If @file doesn't exist in the store yet, %NULL will be returned.
 *
 * Returns: (transfer full): A newly allocated string with the URN containing the data associated
 *          to @file, or %NULL.
 *
 * Since: 0.10
 **/
gchar *
tracker_miner_fs_query_urn (TrackerMinerFS *fs,
                            GFile          *file)
{
	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), NULL);
	g_return_val_if_fail (G_IS_FILE (file), NULL);

	return g_strdup (lookup_file_urn (fs, file, TRUE));
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
 * Returns: (transfer none) (nullable): The parent folder URN, or %NULL.
 *
 * Since: 0.8
 **/
const gchar *
tracker_miner_fs_get_parent_urn (TrackerMinerFS *fs,
                                 GFile          *file)
{
	TrackerTask *task;

	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), NULL);
	g_return_val_if_fail (G_IS_FILE (file), NULL);

	/* Check if found in currently processed data */
	task = tracker_task_pool_find (fs->priv->task_pool, file);

	if (!task) {
		gchar *uri;

		uri = g_file_get_uri (file);

		g_critical ("File '%s' is not being currently processed, "
		            "so the parent URN cannot be retrieved.", uri);
		g_free (uri);

		return NULL;
	} else {
		UpdateProcessingTaskContext *ctxt;

		/* We are only storing the URN in the created/updated tasks */
		ctxt = tracker_task_get_data (task);

		if (!ctxt) {
			gchar *uri;

			uri = g_file_get_uri (file);
			g_critical ("File '%s' is being processed, but not as a "
			            "CREATED/UPDATED task, so cannot get parent "
			            "URN",
			            uri);
			g_free (uri);
			return NULL;
		}

		return ctxt->parent_urn;
	}
}

void
tracker_miner_fs_force_recheck (TrackerMinerFS *fs)
{
#if 0
	GList *directories;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));

	g_message ("Forcing re-check on all index directories");

	directories = fs->priv->config_directories;

	while (directories) {
		tracker_priority_queue_add (fs->priv->directories,
		                            directory_data_ref (directories->data),
		                            G_PRIORITY_LOW);
		directories = directories->next;
	}

	crawl_directories_start (fs);
#endif
}

/**
 * tracker_miner_fs_set_mtime_checking:
 * @fs: a #TrackerMinerFS
 * @mtime_checking: a #gboolean
 *
 * Tells the miner-fs that during the crawling phase, directory mtime
 * checks should or shouldn't be performed against the database to
 * make sure we have the most up to date version of the file being
 * checked at the time. Setting this to #FALSE can dramatically
 * improve the start up the crawling of the @fs.
 *
 * The down side is that using this consistently means that some files
 * on the disk may be out of date with files in the database.
 *
 * The main purpose of this function is for systems where a @fs is
 * running the entire time and where it is very unlikely that a file
 * could be changed outside between startup and shutdown of the
 * process using this API.
 *
 * The default if not set directly is that @mtime_checking is %TRUE.
 *
 * Since: 0.10
 **/
void
tracker_miner_fs_set_mtime_checking (TrackerMinerFS *fs,
                                     gboolean        mtime_checking)
{
	g_return_if_fail (TRACKER_IS_MINER_FS (fs));

	fs->priv->mtime_checking = mtime_checking;
}

/**
 * tracker_miner_fs_get_mtime_checking:
 * @fs: a #TrackerMinerFS
 *
 * Returns a boolean used to identify if file modification time checks
 * are performed when processing content. This may be set to %FALSE if
 * working prodominently with cloud data where you can't perform these
 * checks. By default and for local file systems, this is enabled.
 *
 * Returns: %TRUE if mtime checks for directories against the database
 * are done when @fs crawls the file system, otherwise %FALSE.
 *
 * Since: 0.10
 **/
gboolean
tracker_miner_fs_get_mtime_checking (TrackerMinerFS *fs)
{
	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), FALSE);

	return fs->priv->mtime_checking;
}

/**
 * tracker_miner_fs_force_mtime_checking:
 * @fs: a #TrackerMinerFS
 * @directory: a #GFile representing the directory
 *
 * Tells @fs to force mtime checking (regardless of the global mtime check
 * configuration) on the given @directory.
 *
 * Since: 0.12
 **/
void
tracker_miner_fs_force_mtime_checking (TrackerMinerFS *fs,
                                       GFile          *directory)
{
	TrackerDirectoryFlags flags;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));
	g_return_if_fail (G_IS_FILE (directory));

	flags = TRACKER_DIRECTORY_FLAG_RECURSE |
		TRACKER_DIRECTORY_FLAG_CHECK_MTIME;

	if (!fs->priv->data_provider) {
		flags |= TRACKER_DIRECTORY_FLAG_MONITOR;
	}

	tracker_indexing_tree_add (fs->priv->indexing_tree,
	                           directory,
	                           flags);
}

/**
 * tracker_miner_fs_set_initial_crawling:
 * @fs: a #TrackerMinerFS
 * @do_initial_crawling: a #gboolean
 *
 * Tells the @fs that crawling the #TrackerIndexingTree should happen
 * initially. This is actually required to set up file system monitor
 * using technologies like inotify, etc.
 *
 * Setting this to #FALSE can dramatically improve the start up the
 * crawling of the @fs.
 *
 * The down side is that using this consistently means that some files
 * on the disk may be out of date with files in the database.
 *
 * The main purpose of this function is for systems where a @fs is
 * running the entire time and where it is very unlikely that a file
 * could be changed outside between startup and shutdown of the
 * process using this API.
 *
 * The default if not set directly is that @do_initial_crawling is %TRUE.
 *
 * Since: 0.10
 **/
void
tracker_miner_fs_set_initial_crawling (TrackerMinerFS *fs,
                                       gboolean        do_initial_crawling)
{
	g_return_if_fail (TRACKER_IS_MINER_FS (fs));

	fs->priv->initial_crawling = do_initial_crawling;
}

/**
 * tracker_miner_fs_get_initial_crawling:
 * @fs: a #TrackerMinerFS
 *
 * Returns a boolean which indicates if the indexing tree is crawled
 * upon start up or not. This may be set to %FALSE if working
 * prodominently with cloud data where you can't perform these checks.
 * By default and for local file systems, this is enabled.
 *
 * Returns: %TRUE if a file system structure is crawled for new
 * updates on start up, otherwise %FALSE.
 *
 * Since: 0.10
 **/
gboolean
tracker_miner_fs_get_initial_crawling (TrackerMinerFS *fs)
{
	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), FALSE);

	return fs->priv->initial_crawling;
}

/**
 * tracker_miner_fs_has_items_to_process:
 * @fs: a #TrackerMinerFS
 *
 * The @fs keeps many priority queus for content it is processing.
 * This function returns %TRUE if the sum of all (or any) priority
 * queues is more than 0. This includes items deleted, created,
 * updated, moved or being written back.
 *
 * Returns: %TRUE if there are items to process in the internal
 * queues, otherwise %FALSE.
 *
 * Since: 0.10
 **/
gboolean
tracker_miner_fs_has_items_to_process (TrackerMinerFS *fs)
{
	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), FALSE);

	if (tracker_file_notifier_is_active (fs->priv->file_notifier) ||
	    !tracker_priority_queue_is_empty (fs->priv->items_deleted) ||
	    !tracker_priority_queue_is_empty (fs->priv->items_created) ||
	    !tracker_priority_queue_is_empty (fs->priv->items_updated) ||
	    !tracker_priority_queue_is_empty (fs->priv->items_moved) ||
	    !tracker_priority_queue_is_empty (fs->priv->items_writeback)) {
		return TRUE;
	}

	return FALSE;
}

#ifndef TRACKER_DISABLE_DEPRECATED
/**
 * tracker_miner_fs_add_directory_without_parent:
 * @fs: a #TrackerMinerFS
 * @file: a #GFile
 *
 * Tells the miner-fs that the given #GFile corresponds to a
 * directory which was created in the store without a specific
 * parent object. In this case, when regenerating internal
 * caches, an extra query will be done so that these elements
 * are taken into account.
 *
 * Since: 0.10
 **/
void
tracker_miner_fs_add_directory_without_parent (TrackerMinerFS *fs,
                                               GFile          *file)
{
	TrackerDirectoryFlags flags;

	g_return_if_fail (TRACKER_IS_MINER_FS (fs));
	g_return_if_fail (G_IS_FILE (file));

	flags = TRACKER_DIRECTORY_FLAG_RECURSE |
		TRACKER_DIRECTORY_FLAG_PRESERVE |
		TRACKER_DIRECTORY_FLAG_CHECK_MTIME;

	if (!fs->priv->data_provider) {
		flags |= TRACKER_DIRECTORY_FLAG_MONITOR;
	}

	tracker_indexing_tree_add (fs->priv->indexing_tree,
	                           file,
	                           flags);
}
#endif

/**
 * tracker_miner_fs_get_indexing_tree:
 * @fs: a #TrackerMinerFS
 *
 * Returns the #TrackerIndexingTree which determines
 * what files/directories are indexed by @fs
 *
 * Returns: (transfer none): The #TrackerIndexingTree
 *          holding the indexing configuration
 **/
TrackerIndexingTree *
tracker_miner_fs_get_indexing_tree (TrackerMinerFS *fs)
{
	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), NULL);

	return fs->priv->indexing_tree;
}

/**
 * tracker_miner_fs_get_data_provider:
 * @fs: a #TrackerMinerFS
 *
 * Returns the #TrackerDataProvider implementation, which is being used
 * to supply #GFile and #GFileInfo content to Tracker.
 *
 * Returns: (transfer none): The #TrackerDataProvider supplying content
 *
 * Since: 1.2
 **/
TrackerDataProvider *
tracker_miner_fs_get_data_provider (TrackerMinerFS *fs)
{
	g_return_val_if_fail (TRACKER_IS_MINER_FS (fs), NULL);

	return fs->priv->data_provider;
}

#ifdef EVENT_QUEUE_ENABLE_TRACE

static void
trace_files_foreach (gpointer file,
                     gpointer fs)
{
	gchar *uri;

	uri = g_file_get_uri (G_FILE (file));
	trace_eq ("(%s)     '%s'",
	          G_OBJECT_TYPE_NAME (G_OBJECT (fs)),
	          uri);
	g_free (uri);
}

static void
trace_moved_foreach (gpointer moved_data,
                     gpointer fs)
{
	ItemMovedData *data = moved_data;
	gchar *source_uri;
	gchar *dest_uri;

	source_uri = g_file_get_uri (data->source_file);
	dest_uri = g_file_get_uri (data->file);
	trace_eq ("(%s)     '%s->%s'",
	          G_OBJECT_TYPE_NAME (G_OBJECT (fs)),
	          source_uri,
	          dest_uri);
	g_free (source_uri);
	g_free (dest_uri);
}

static void
trace_writeback_foreach (gpointer writeback_data,
                         gpointer fs)
{
	ItemWritebackData *data = writeback_data;
	gchar *uri;

	uri = g_file_get_uri (G_FILE (data->file));
	trace_eq ("(%s)     '%s'",
	          G_OBJECT_TYPE_NAME (G_OBJECT (fs)),
	          uri);
	g_free (uri);
}

static void
miner_fs_trace_queue (TrackerMinerFS       *fs,
                      const gchar          *queue_name,
                      TrackerPriorityQueue *queue,
                      GFunc                 foreach_cb)
{
	trace_eq ("(%s) Queue '%s' has %u elements:",
	          G_OBJECT_TYPE_NAME (fs),
	          queue_name,
	          tracker_priority_queue_get_length (queue));
	tracker_priority_queue_foreach (queue,
	                                foreach_cb,
	                                fs);
}

static gboolean
miner_fs_queues_status_trace_timeout_cb (gpointer data)
{
	TrackerMinerFS *fs = data;

	trace_eq ("(%s) ------------", G_OBJECT_TYPE_NAME (fs));
	miner_fs_trace_queue (fs, "CREATED",   fs->priv->items_created,   trace_files_foreach);
	miner_fs_trace_queue (fs, "UPDATED",   fs->priv->items_updated,   trace_files_foreach);
	miner_fs_trace_queue (fs, "DELETED",   fs->priv->items_deleted,   trace_files_foreach);
	miner_fs_trace_queue (fs, "MOVED",     fs->priv->items_moved,     trace_moved_foreach);
	miner_fs_trace_queue (fs, "WRITEBACK", fs->priv->items_writeback, trace_writeback_foreach);

	return TRUE;
}

#endif /* EVENT_QUEUE_ENABLE_TRACE */
