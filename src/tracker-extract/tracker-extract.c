/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <string.h>
#include <unistd.h>

#include <gmodule.h>
#include <gio/gio.h>

#include <gio/gunixoutputstream.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixfdlist.h>

#include <libtracker-common/tracker-common.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-extract.h"
#include "tracker-main.h"
#include "tracker-marshal.h"

#ifdef HAVE_LIBSTREAMANALYZER
#include "tracker-topanalyzer.h"
#endif /* HAVE_STREAMANALYZER */

#ifdef THREAD_ENABLE_TRACE
#warning Main thread traces enabled
#endif /* THREAD_ENABLE_TRACE */

#define TRACKER_EXTRACT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_EXTRACT, TrackerExtractPrivate))

extern gboolean debug;

typedef struct {
	gint extracted_count;
	gint failed_count;
} StatisticsData;

typedef struct {
	GHashTable *statistics_data;
	GList *running_tasks;

	/* used to maintain the running tasks
	 * and stats from different threads
	 */
#if GLIB_CHECK_VERSION (2,31,0)
	GMutex task_mutex;
#else
	GMutex *task_mutex;
#endif

	/* Thread pool for multi-threaded extractors */
	GThreadPool *thread_pool;

	/* module -> async queue hashtable
	 * for single-threaded extractors
	 */
	GHashTable *single_thread_extractors;

	gboolean disable_shutdown;
	gboolean force_internal_extractors;
	gboolean disable_summary_on_finalize;

	gchar *force_module;

	gint unhandled_count;
} TrackerExtractPrivate;

typedef struct {
	TrackerExtract *extract;
	GCancellable *cancellable;
	GAsyncResult *res;
	gchar *file;
	gchar *mimetype;
	gchar *graph;

	TrackerMimetypeInfo *mimetype_handlers;

	/* to be fed from mimetype_handlers */
	TrackerExtractMetadataFunc cur_func;
	GModule *cur_module;

	guint signal_id;
	guint success : 1;
} TrackerExtractTask;

static void tracker_extract_finalize (GObject *object);
static void report_statistics        (GObject *object);
static gboolean get_metadata         (TrackerExtractTask *task);
static gboolean dispatch_task_cb     (TrackerExtractTask *task);


G_DEFINE_TYPE(TrackerExtract, tracker_extract, G_TYPE_OBJECT)

static void
tracker_extract_class_init (TrackerExtractClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_extract_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerExtractPrivate));
}

static void
statistics_data_free (StatisticsData *data)
{
	g_slice_free (StatisticsData, data);
}

static void
tracker_extract_init (TrackerExtract *object)
{
	TrackerExtractPrivate *priv;

#ifdef HAVE_LIBSTREAMANALYZER
	tracker_topanalyzer_init ();
#endif /* HAVE_STREAMANALYZER */

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);
	priv->statistics_data = g_hash_table_new_full (NULL, NULL, NULL,
	                                               (GDestroyNotify) statistics_data_free);
	priv->single_thread_extractors = g_hash_table_new (NULL, NULL);
	priv->thread_pool = g_thread_pool_new ((GFunc) get_metadata,
	                                       NULL, 10, TRUE, NULL);

#if GLIB_CHECK_VERSION (2,31,0)
	g_mutex_init (&priv->task_mutex);
#else
	priv->task_mutex = g_mutex_new ();
#endif
}

static void
tracker_extract_finalize (GObject *object)
{
	TrackerExtractPrivate *priv;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

	/* FIXME: Shutdown modules? */

	g_hash_table_destroy (priv->single_thread_extractors);
	g_thread_pool_free (priv->thread_pool, TRUE, FALSE);

	if (!priv->disable_summary_on_finalize) {
		report_statistics (object);
	}

#ifdef HAVE_LIBSTREAMANALYZER
	tracker_topanalyzer_shutdown ();
#endif /* HAVE_STREAMANALYZER */

	g_hash_table_destroy (priv->statistics_data);

#if GLIB_CHECK_VERSION (2,31,0)
	g_mutex_clear (&priv->task_mutex);
#else
	g_mutex_free (priv->task_mutex);
#endif

	G_OBJECT_CLASS (tracker_extract_parent_class)->finalize (object);
}

static void
report_statistics (GObject *object)
{
	TrackerExtractPrivate *priv;
	GHashTableIter iter;
	gpointer key, value;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

#if GLIB_CHECK_VERSION (2,31,0)
	g_mutex_lock (&priv->task_mutex);
#else
	g_mutex_lock (priv->task_mutex);
#endif

	g_message ("--------------------------------------------------");
	g_message ("Statistics:");

	g_hash_table_iter_init (&iter, priv->statistics_data);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GModule *module = key;
		StatisticsData *data = value;

		if (data->extracted_count > 0 || data->failed_count > 0) {
			const gchar *name, *name_without_path;

			name = g_module_name (module);
			name_without_path = strrchr (name, G_DIR_SEPARATOR) + 1;

			g_message ("    Module:'%s', extracted:%d, failures:%d",
			           name_without_path,
			           data->extracted_count,
			           data->failed_count);
		}
	}

	g_message ("Unhandled files: %d", priv->unhandled_count);

	if (priv->unhandled_count == 0 &&
	    g_hash_table_size (priv->statistics_data) < 1) {
		g_message ("    No files handled");
	}

	g_message ("--------------------------------------------------");

#if GLIB_CHECK_VERSION (2,31,0)
	g_mutex_unlock (&priv->task_mutex);
#else
	g_mutex_unlock (priv->task_mutex);
#endif
}

TrackerExtract *
tracker_extract_new (gboolean     disable_shutdown,
                     gboolean     force_internal_extractors,
                     const gchar *force_module)
{
	TrackerExtract *object;
	TrackerExtractPrivate *priv;

	if (!tracker_extract_module_manager_init ()) {
		return NULL;
	}

	/* Set extractors */
	object = g_object_new (TRACKER_TYPE_EXTRACT, NULL);

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

	priv->disable_shutdown = disable_shutdown;
	priv->force_internal_extractors = force_internal_extractors;
	priv->force_module = g_strdup (force_module);

	return object;
}

static void
notify_task_finish (TrackerExtractTask *task,
                    gboolean            success)
{
	TrackerExtract *extract;
	TrackerExtractPrivate *priv;
	StatisticsData *stats_data;

	extract = task->extract;
	priv = TRACKER_EXTRACT_GET_PRIVATE (extract);

	/* Reports and ongoing tasks may be
	 * accessed from other threads.
	 */
#if GLIB_CHECK_VERSION (2,31,0)
	g_mutex_lock (&priv->task_mutex);
#else
	g_mutex_lock (priv->task_mutex);
#endif

	stats_data = g_hash_table_lookup (priv->statistics_data,
	                                  task->cur_module);

	if (!stats_data) {
		stats_data = g_slice_new0 (StatisticsData);
		g_hash_table_insert (priv->statistics_data,
		                     task->cur_module,
		                     stats_data);
	}

	stats_data->extracted_count++;

	if (!success) {
		stats_data->failed_count++;
	}

	priv->running_tasks = g_list_remove (priv->running_tasks, task);

#if GLIB_CHECK_VERSION (2,31,0)
	g_mutex_unlock (&priv->task_mutex);
#else
	g_mutex_unlock (priv->task_mutex);
#endif
}

static gboolean
get_file_metadata (TrackerExtractTask  *task,
                   TrackerExtractInfo **info_out)
{
	TrackerExtractInfo *info;
	GFile *file;
	gchar *mime_used = NULL;
#ifdef HAVE_LIBSTREAMANALYZER
	gchar *content_type = NULL;
#endif
	gint items = 0;

	g_debug ("Extracting...");

	*info_out = NULL;

	file = g_file_new_for_uri (task->file);
	info = tracker_extract_info_new (file, task->mimetype, task->graph);
	g_object_unref (file);

#ifdef HAVE_LIBSTREAMANALYZER
	/* FIXME: This entire section is completely broken,
	 * it doesn't even build these days. It should be removed or fixed.
	 * -mr (05/09/11)
	 */
	if (!priv->force_internal_extractors) {
		g_debug ("  Using libstreamanalyzer...");

		tracker_topanalyzer_extract (task->file, statements, &content_type);

		if (tracker_sparql_builder_get_length (statements) > 0) {
			g_free (content_type);
			tracker_sparql_builder_insert_close (statements);

			*info_out = info;

			return TRUE;
		}
	} else {
		g_debug ("  Using internal extractors ONLY...");
	}
#endif /* HAVE_LIBSTREAMANALYZER */

	if (task->mimetype && *task->mimetype) {
		/* We know the mime */
		mime_used = g_strdup (task->mimetype);
	}
#ifdef HAVE_LIBSTREAMANALYZER
	else if (content_type && *content_type) {
		/* We know the mime from LSA */
		mime_used = content_type;
		g_strstrip (mime_used);
	}
#endif /* HAVE_LIBSTREAMANALYZER */
	else {
		tracker_extract_info_unref (info);
		return FALSE;
	}

	/* Now we have sanity checked everything, actually get the
	 * data we need from the extractors.
	 */
	if (mime_used) {
		if (task->cur_func) {
			TrackerSparqlBuilder *statements;

			g_debug ("  Using %s...", g_module_name (task->cur_module));

			(task->cur_func) (info);

			statements = tracker_extract_info_get_metadata_builder (info);
			items = tracker_sparql_builder_get_length (statements);

			if (items > 0) {
				tracker_sparql_builder_insert_close (statements);

				g_debug ("Done (%d items)", items);

				task->success = TRUE;
			}
		}

		g_free (mime_used);
	}

	if (items == 0) {
		g_debug ("No extractor or failed");
		tracker_extract_info_unref (info);
		info = NULL;
	}

	*info_out = info;

	return (items > 0);
}

/* This function is called on the thread calling g_cancellable_cancel() */
static void
task_cancellable_cancelled_cb (GCancellable       *cancellable,
                               TrackerExtractTask *task)
{
	TrackerExtractPrivate *priv;
	TrackerExtract *extract;

	extract = task->extract;
	priv = TRACKER_EXTRACT_GET_PRIVATE (extract);

#if GLIB_CHECK_VERSION (2,31,0)
	g_mutex_lock (&priv->task_mutex);
#else
	g_mutex_lock (priv->task_mutex);
#endif

	if (g_list_find (priv->running_tasks, task)) {
		g_message ("Cancelled task for '%s' was currently being "
		           "processed, _exit()ing immediately",
		           task->file);
		_exit (0);
	}

#if GLIB_CHECK_VERSION (2,31,0)
	g_mutex_unlock (&priv->task_mutex);
#else
	g_mutex_unlock (priv->task_mutex);
#endif
}

static TrackerExtractTask *
extract_task_new (TrackerExtract *extract,
                  const gchar    *uri,
                  const gchar    *mimetype,
                  const gchar    *graph,
                  GCancellable   *cancellable,
                  GAsyncResult   *res,
                  GError        **error)
{
	TrackerExtractTask *task;
	gchar *mimetype_used;

	if (!mimetype || !*mimetype) {
		GFile *file;
		GFileInfo *info;
		GError *internal_error = NULL;

		file = g_file_new_for_uri (uri);
		info = g_file_query_info (file,
		                          G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
		                          G_FILE_QUERY_INFO_NONE,
		                          NULL,
		                          &internal_error);

		g_object_unref (file);

		if (internal_error) {
			g_propagate_error (error, internal_error);
			return NULL;
		}

		mimetype_used = g_strdup (g_file_info_get_content_type (info));
		g_debug ("Guessing mime type as '%s'", mimetype);
		g_object_unref (info);
	} else {
		mimetype_used = g_strdup (mimetype);
	}

	task = g_slice_new0 (TrackerExtractTask);
	task->cancellable = (cancellable) ? g_object_ref (cancellable) : NULL;
	task->res = (res) ? g_object_ref (res) : NULL;
	task->file = g_strdup (uri);
	task->mimetype = mimetype_used;
	task->graph = g_strdup (graph);
	task->extract = extract;

	if (task->cancellable) {
		task->signal_id = g_cancellable_connect (cancellable,
		                                         G_CALLBACK (task_cancellable_cancelled_cb),
		                                         task, NULL);
	}

	return task;
}

static void
extract_task_free (TrackerExtractTask *task)
{
	if (task->cancellable && task->signal_id != 0) {
		g_cancellable_disconnect (task->cancellable, task->signal_id);
	}

	notify_task_finish (task, task->success);

	if (task->res) {
		g_object_unref (task->res);
	}

	if (task->cancellable) {
		g_object_unref (task->cancellable);
	}

	if (task->mimetype_handlers) {
		tracker_mimetype_info_free (task->mimetype_handlers);
	}

	g_free (task->graph);
	g_free (task->mimetype);
	g_free (task->file);

	g_slice_free (TrackerExtractTask, task);
}

static gboolean
filter_module (TrackerExtract *extract,
               GModule        *module)
{
	TrackerExtractPrivate *priv;
	gchar *module_basename, *filter_name;
	gboolean filter;

	priv = TRACKER_EXTRACT_GET_PRIVATE (extract);

	if (!priv->force_module) {
		return FALSE;
	}

	/* Module name is the full path to it */
	module_basename = g_path_get_basename (g_module_name (module));

	if (g_str_has_prefix (priv->force_module, "lib") &&
	    g_str_has_suffix (priv->force_module, "." G_MODULE_SUFFIX)) {
		filter_name = g_strdup (priv->force_module);
	} else {
		filter_name = g_strdup_printf ("libextract-%s.so",
		                               priv->force_module);
	}

	filter = strcmp (module_basename, filter_name) != 0;

	if (filter) {
		g_debug ("Module filtered out '%s' (due to --force-module='%s')",
		         module_basename,
		         filter_name);
	} else {
		g_debug ("Module used '%s' (due to --force-module='%s')",
		         module_basename,
		         filter_name);
	}

	g_free (module_basename);
	g_free (filter_name);

	return filter;
}

static gboolean
get_metadata (TrackerExtractTask *task)
{
	TrackerExtractInfo *info;
	TrackerSparqlBuilder *preupdate, *postupdate, *statements;
	gchar *where = NULL;

	preupdate = postupdate = statements = NULL;

#ifdef THREAD_ENABLE_TRACE
	g_debug ("Thread:%p --> File:'%s' - Extracted",
	         g_thread_self (),
	         task->file);
#endif /* THREAD_ENABLE_TRACE */

	if (task->cancellable &&
	    g_cancellable_is_cancelled (task->cancellable)) {
		g_simple_async_result_set_error ((GSimpleAsyncResult *) task->res,
		                                 TRACKER_DBUS_ERROR, 0,
		                                 "Extraction of '%s' was cancelled",
		                                 task->file);

		g_simple_async_result_complete_in_idle ((GSimpleAsyncResult *) task->res);
		extract_task_free (task);
		return FALSE;
	}

	if (!filter_module (task->extract, task->cur_module) &&
	    get_file_metadata (task, &info)) {
		g_simple_async_result_set_op_res_gpointer ((GSimpleAsyncResult *) task->res,
		                                           info,
		                                           (GDestroyNotify) tracker_extract_info_unref);

		g_simple_async_result_complete_in_idle ((GSimpleAsyncResult *) task->res);
		extract_task_free (task);
	} else {
		if (preupdate) {
			g_object_unref (preupdate);
		}

		if (postupdate) {
			g_object_unref (postupdate);
		}

		if (statements) {
			g_object_unref (statements);
		}

		g_free (where);

		/* Reinject the task into the main thread
		 * queue, so the next module kicks in.
		 */
		g_idle_add ((GSourceFunc) dispatch_task_cb, task);
	}

	return FALSE;
}

static void
single_thread_get_metadata (GAsyncQueue *queue)
{
	while (TRUE) {
		TrackerExtractTask *task;

		task = g_async_queue_pop (queue);
		g_message ("Dispatching '%s' in dedicated thread", task->file);
		get_metadata (task);
	}
}

/* This function is executed in the main thread, decides the
 * module that's going to be run for a given task, and dispatches
 * the task according to the threading strategy of that module.
 */
static gboolean
dispatch_task_cb (TrackerExtractTask *task)
{
	TrackerModuleThreadAwareness thread_awareness;
	TrackerExtractPrivate *priv;
	GError *error = NULL;
	GModule *module;

#ifdef THREAD_ENABLE_TRACE
	g_debug ("Thread:%p (Main) <-- File:'%s' - Dispatching\n",
	         g_thread_self (),
	         task->file);
#endif /* THREAD_ENABLE_TRACE */

	priv = TRACKER_EXTRACT_GET_PRIVATE (task->extract);

	if (!task->mimetype) {
		error = g_error_new (TRACKER_DBUS_ERROR, 0,
		                     "No mimetype for '%s'",
		                     task->file);
	} else {
		if (!task->mimetype_handlers) {
			/* First iteration for task, get the mimetype handlers */
			task->mimetype_handlers = tracker_extract_module_manager_get_mimetype_handlers (task->mimetype);

			if (!task->mimetype_handlers) {
				error = g_error_new (TRACKER_DBUS_ERROR, 0,
				                     "No mimetype extractor handlers for uri:'%s' and mime:'%s'",
				                     task->file, task->mimetype);
			}
		} else {
			/* Any further iteration, should happen rarely if
			 * most specific handlers know nothing about the file
			 */
			g_message ("Trying next extractor for '%s'", task->file);

			if (!tracker_mimetype_info_iter_next (task->mimetype_handlers)) {
				g_message ("  There's no next extractor");

				error = g_error_new (TRACKER_DBUS_ERROR, 0,
				                     "Could not get any metadata for uri:'%s' and mime:'%s'",
				                     task->file, task->mimetype);
			}
		}
	}

	if (error) {
		g_simple_async_result_set_from_error ((GSimpleAsyncResult *) task->res, error);
		g_simple_async_result_complete_in_idle ((GSimpleAsyncResult *) task->res);
		extract_task_free (task);
		g_error_free (error);

		return FALSE;
	}

	task->cur_module = module = tracker_mimetype_info_get_module (task->mimetype_handlers, &task->cur_func, &thread_awareness);

	if (!module || !task->cur_func) {
		g_warning ("Discarding task with no module '%s'", task->file);
		priv->unhandled_count++;
		return FALSE;
	}

#if GLIB_CHECK_VERSION (2,31,0)
	g_mutex_lock (&priv->task_mutex);
	priv->running_tasks = g_list_prepend (priv->running_tasks, task);
	g_mutex_unlock (&priv->task_mutex);
#else
	g_mutex_lock (priv->task_mutex);
	priv->running_tasks = g_list_prepend (priv->running_tasks, task);
	g_mutex_unlock (priv->task_mutex);
#endif

	switch (thread_awareness) {
	case TRACKER_MODULE_NONE:
		/* Error out */
		g_simple_async_result_set_error ((GSimpleAsyncResult *) task->res,
		                                 TRACKER_DBUS_ERROR, 0,
		                                 "Module '%s' initialization failed",
		                                 g_module_name (module));
		g_simple_async_result_complete_in_idle ((GSimpleAsyncResult *) task->res);
		extract_task_free (task);
		break;
	case TRACKER_MODULE_MAIN_THREAD:
		/* Dispatch the task right away in this thread */
		g_message ("Dispatching '%s' in main thread", task->file);
		get_metadata (task);
		break;
	case TRACKER_MODULE_SINGLE_THREAD:
	{
		GAsyncQueue *async_queue;

		async_queue = g_hash_table_lookup (priv->single_thread_extractors, module);

		if (!async_queue) {
			/* No thread created yet for this module, create it
			 * together with the async queue used to pass data to it
			 */
			async_queue = g_async_queue_new ();

#if GLIB_CHECK_VERSION (2,31,0)
			{
				GThread *thread;

				thread = g_thread_try_new ("single",
				                           (GThreadFunc) single_thread_get_metadata,
				                           g_async_queue_ref (async_queue),
				                           &error);
				if (!thread) {
					g_simple_async_result_take_error ((GSimpleAsyncResult *) task->res, error);
					g_simple_async_result_complete_in_idle ((GSimpleAsyncResult *) task->res);
					extract_task_free (task);
					return FALSE;
				}
				/* We won't join the thread, so just unref it here */
				g_object_unref (thread);
			}
#else
			g_thread_create ((GThreadFunc) single_thread_get_metadata,
			                 g_async_queue_ref (async_queue),
			                 FALSE, &error);

			if (error) {
				g_simple_async_result_set_from_error ((GSimpleAsyncResult *) task->res, error);
				g_simple_async_result_complete_in_idle ((GSimpleAsyncResult *) task->res);
				extract_task_free (task);
				g_error_free (error);

				return FALSE;
			}
#endif

			g_hash_table_insert (priv->single_thread_extractors, module, async_queue);
		}

		g_async_queue_push (async_queue, task);
	}
		break;
	case TRACKER_MODULE_MULTI_THREAD:
		/* Put task in thread pool */
		g_message ("Dispatching '%s' in thread pool", task->file);
		g_thread_pool_push (priv->thread_pool, task, &error);

		if (error) {
			g_simple_async_result_set_from_error ((GSimpleAsyncResult *) task->res, error);
			g_simple_async_result_complete_in_idle ((GSimpleAsyncResult *) task->res);
			extract_task_free (task);
			g_error_free (error);

			return FALSE;
		}

		break;
	}

	return FALSE;
}

/* This function can be called in any thread */
void
tracker_extract_file (TrackerExtract      *extract,
                      const gchar         *file,
                      const gchar         *mimetype,
                      const gchar         *graph,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  cb,
                      gpointer             user_data)
{
	GSimpleAsyncResult *res;
	GError *error = NULL;
	TrackerExtractTask *task;

	g_return_if_fail (TRACKER_IS_EXTRACT (extract));
	g_return_if_fail (file != NULL);
	g_return_if_fail (cb != NULL);

#ifdef THREAD_ENABLE_TRACE
	g_debug ("Thread:%p <-- File:'%s' - Extracting\n",
	         g_thread_self (),
	         file);
#endif /* THREAD_ENABLE_TRACE */

	res = g_simple_async_result_new (G_OBJECT (extract), cb, user_data, NULL);

	task = extract_task_new (extract, file, mimetype, graph,
	                         cancellable, G_ASYNC_RESULT (res), &error);

	if (error) {
		g_warning ("Could not get mimetype, %s", error->message);
		g_simple_async_result_set_from_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		g_error_free (error);
	} else {
		g_idle_add ((GSourceFunc) dispatch_task_cb, task);
	}

	/* Task takes a ref and if this fails, we want to unref anyway */
	g_object_unref (res);
}

void
tracker_extract_get_metadata_by_cmdline (TrackerExtract *object,
                                         const gchar    *uri,
                                         const gchar    *mime)
{
	GError *error = NULL;
	TrackerExtractPrivate *priv;
	TrackerExtractTask *task;
	TrackerExtractInfo *info;
	gboolean no_modules = TRUE;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);
	priv->disable_summary_on_finalize = TRUE;

	g_return_if_fail (uri != NULL);

	task = extract_task_new (object, uri, mime, NULL, NULL, NULL, &error);

	if (error) {
		g_printerr ("Extraction failed, %s\n", error->message);
		g_error_free (error);

		return;
	}

	task->mimetype_handlers = tracker_extract_module_manager_get_mimetype_handlers (task->mimetype);
	task->cur_module = tracker_mimetype_info_get_module (task->mimetype_handlers, &task->cur_func, NULL);

	while (task->cur_module && task->cur_func) {
		if (!filter_module (object, task->cur_module) &&
		    get_file_metadata (task, &info)) {
			const gchar *preupdate_str, *postupdate_str, *statements_str, *where;
			TrackerSparqlBuilder *builder;

			no_modules = FALSE;
			preupdate_str = statements_str = postupdate_str = NULL;

			builder = tracker_extract_info_get_metadata_builder (info);

			if (tracker_sparql_builder_get_length (builder) > 0) {
				statements_str = tracker_sparql_builder_get_result (builder);
			}

			builder = tracker_extract_info_get_preupdate_builder (info);

			if (tracker_sparql_builder_get_length (builder) > 0) {
				preupdate_str = tracker_sparql_builder_get_result (builder);
			}

			builder = tracker_extract_info_get_postupdate_builder (info);

			if (tracker_sparql_builder_get_length (builder) > 0) {
				postupdate_str = tracker_sparql_builder_get_result (builder);
			}

			where = tracker_extract_info_get_where_clause (info);

			g_print ("\n");

			g_print ("SPARQL pre-update:\n--\n%s--\n\n",
			         preupdate_str ? preupdate_str : "");
			g_print ("SPARQL item:\n--\n%s--\n\n",
			         statements_str ? statements_str : "");
			g_print ("SPARQL where clause:\n--\n%s--\n\n",
			         where ? where : "");
			g_print ("SPARQL post-update:\n--\n%s--\n\n",
			         postupdate_str ? postupdate_str : "");

			tracker_extract_info_unref (info);
			break;
		} else {
			if (!tracker_mimetype_info_iter_next (task->mimetype_handlers)) {
				break;
			}

			task->cur_module = tracker_mimetype_info_get_module (task->mimetype_handlers,
			                                                     &task->cur_func,
			                                                     NULL);
		}
	}

	if (no_modules) {
		g_print ("No modules found to handle metadata extraction\n\n");
	}

	extract_task_free (task);
}
