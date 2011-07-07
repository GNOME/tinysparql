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

#define MAX_EXTRACT_TIME 10

#define UNKNOWN_METHOD_MESSAGE "Method \"%s\" with signature \"%s\" on " \
                               "interface \"%s\" doesn't exist, expected \"%s\""

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.freedesktop.Tracker1.Extract'>"
  "    <method name='GetPid'>"
  "      <arg type='i' name='value' direction='out' />"
  "    </method>"
  "    <method name='GetMetadata'>"
  "      <arg type='s' name='uri' direction='in' />"
  "      <arg type='s' name='mime' direction='in' />"
  "      <arg type='s' name='preupdate' direction='out' />"
  "      <arg type='s' name='embedded' direction='out' />"
  "      <arg type='s' name='where' direction='out' />"
  "    </method>"
  "    <method name='GetMetadataFast'>"
  "      <arg type='s' name='uri' direction='in' />"
  "      <arg type='s' name='mime' direction='in' />"
  "      <arg type='h' name='fd' direction='in' />"
  "    </method>"
  "  </interface>"
  "</node>";

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
	GMutex *task_mutex;

	/* Thread pool for multi-threaded extractors */
	GThreadPool *thread_pool;

	/* module -> async queue hashtable
	 * for single-threaded extractors
	 */
	GHashTable *single_thread_extractors;

	gboolean disable_shutdown;
	gboolean force_internal_extractors;
	gboolean disable_summary_on_finalize;

	gint unhandled_count;
} TrackerExtractPrivate;

typedef struct {
	TrackerExtract *extract;
	GCancellable *cancellable;
	GAsyncResult *res;
	gchar *file;
	gchar *mimetype;

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

	priv->task_mutex = g_mutex_new ();
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

	G_OBJECT_CLASS (tracker_extract_parent_class)->finalize (object);
}

static void
report_statistics (GObject *object)
{
	TrackerExtractPrivate *priv;
	GHashTableIter iter;
	gpointer key, value;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

	g_mutex_lock (priv->task_mutex);

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

	g_mutex_unlock (priv->task_mutex);
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
	g_mutex_lock (priv->task_mutex);

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

	g_mutex_unlock (priv->task_mutex);
}

static gboolean
get_file_metadata (TrackerExtractTask     *task,
                   TrackerSparqlBuilder  **preupdate_out,
                   TrackerSparqlBuilder  **statements_out,
                   gchar                 **where_out)
{
	TrackerSparqlBuilder *statements, *preupdate;
	GString *where;
	gchar *mime_used = NULL;
#ifdef HAVE_LIBSTREAMANALYZER
	gchar *content_type = NULL;
#endif
	gint items = 0;

	g_debug ("Extracting...");

	*preupdate_out = NULL;
	*statements_out = NULL;
	*where_out = NULL;

	/* Create sparql builders to send back */
	preupdate = tracker_sparql_builder_new_update ();
	statements = tracker_sparql_builder_new_embedded_insert ();
	where = g_string_new ("");

#ifdef HAVE_LIBSTREAMANALYZER
	if (!priv->force_internal_extractors) {
		g_debug ("  Using libstreamanalyzer...");

		tracker_topanalyzer_extract (uri, statements, &content_type);

		if (tracker_sparql_builder_get_length (statements) > 0) {
			g_free (content_type);
			tracker_sparql_builder_insert_close (statements);

			*preupdate_out = preupdate;
			*statements_out = statements;
			*where_out = g_string_free (where, FALSE);
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
		return FALSE;
	}

	/* Now we have sanity checked everything, actually get the
	 * data we need from the extractors.
	 */
	if (mime_used) {
		if (task->cur_func) {
			g_debug ("  Using %s...", g_module_name (task->cur_module));

			(task->cur_func) (task->file, mime_used, preupdate, statements, where);

			items = tracker_sparql_builder_get_length (statements);

			if (items > 0) {
				tracker_sparql_builder_insert_close (statements);

				g_debug ("Done (%d items)", items);

				task->success = TRUE;
			}
		}

		g_free (mime_used);
	}

	*preupdate_out = preupdate;
	*statements_out = statements;
	*where_out = g_string_free (where, FALSE);

	if (items == 0) {
		g_debug ("No extractor or failed");
	}

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

	g_mutex_lock (priv->task_mutex);

	if (g_list_find (priv->running_tasks, task)) {
		g_message ("Cancelled task for '%s' was currently being "
		           "processed, _exit()ing immediately",
		           task->file);
		_exit (0);
	}

	g_mutex_unlock (priv->task_mutex);
}

static TrackerExtractTask *
extract_task_new (TrackerExtract *extract,
                  const gchar    *uri,
                  const gchar    *mimetype,
                  GCancellable   *cancellable,
                  GAsyncResult   *res)
{
	TrackerExtractTask *task;

	if (!mimetype) {
		GFile *file;
		GFileInfo *info;

		file = g_file_new_for_uri (uri);
		info = g_file_query_info (file,
		                          G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
		                          G_FILE_QUERY_INFO_NONE,
		                          NULL, NULL);

		if (info) {
			mimetype = g_strdup (g_file_info_get_content_type (info));
			g_object_unref (info);
		} else {
			g_warning ("Could not get mimetype for '%s'", uri);
			return NULL;
		}

		g_object_unref (file);
	}

	task = g_slice_new0 (TrackerExtractTask);
	task->cancellable = (cancellable) ? g_object_ref (cancellable) : NULL;
	task->res = (res) ? g_object_ref (res) : NULL;
	task->file = g_strdup (uri);
	task->mimetype = g_strdup (mimetype);
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
	if (task->cancellable &&
	    task->signal_id != 0) {
		g_cancellable_disconnect (task->cancellable, task->signal_id);
	}

	notify_task_finish (task, task->success);

	if (task->res) {
		g_object_unref (task->res);
	}

	if (task->cancellable) {
		g_object_unref (task->cancellable);
	}

	g_free (task->file);
	g_free (task->mimetype);
	g_slice_free (TrackerExtractTask, task);
}

static gboolean
get_metadata (TrackerExtractTask *task)
{
	TrackerExtractInfo *info;
	TrackerSparqlBuilder *preupdate, *statements;
	gchar *where = NULL;

	preupdate = statements = NULL;

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

		extract_task_free (task);
		return FALSE;
	}

	if (get_file_metadata (task, &preupdate, &statements, &where)) {
		info = tracker_extract_info_new ((preupdate) ? tracker_sparql_builder_get_result (preupdate) : NULL,
		                                 (statements) ? tracker_sparql_builder_get_result (statements) : NULL,
		                                 where);

		g_simple_async_result_set_op_res_gpointer ((GSimpleAsyncResult *) task->res,
		                                           info,
		                                           (GDestroyNotify) tracker_extract_info_free);

		g_simple_async_result_complete_in_idle ((GSimpleAsyncResult *) task->res);
		extract_task_free (task);
	} else {
		if (preupdate) {
			g_object_unref (preupdate);
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

	g_mutex_lock (priv->task_mutex);
	priv->running_tasks = g_list_prepend (priv->running_tasks, task);
	g_mutex_unlock (priv->task_mutex);

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
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  cb,
                      gpointer             user_data)
{
	GSimpleAsyncResult *res;
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

	task = extract_task_new (extract, file, mimetype, cancellable, G_ASYNC_RESULT (res));

	if (task) {
		g_idle_add ((GSourceFunc) dispatch_task_cb, task);

		/* task takes a ref */
		g_object_unref (res);
	}
}

void
tracker_extract_get_metadata_by_cmdline (TrackerExtract *object,
                                         const gchar    *uri,
                                         const gchar    *mime)
{
	TrackerSparqlBuilder *statements, *preupdate;
	gchar *where;
	TrackerExtractPrivate *priv;
	TrackerExtractTask *task;
	TrackerExtractInitFunc init_func;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);
	priv->disable_summary_on_finalize = TRUE;

	g_return_if_fail (uri != NULL);

	g_message ("Extracting...");

	task = extract_task_new (object, uri, mime, NULL, NULL);

	if (!task) {
		return;
	}

	task->cur_module = tracker_extract_module_manager_get_for_mimetype (task->mimetype, &init_func, NULL, &task->cur_func);

	if (init_func) {
		TrackerModuleThreadAwareness ignore;

		/* Initialize module for this single run */
		(init_func) (&ignore, NULL);
	}

	if (get_file_metadata (task, &preupdate, &statements, &where)) {
		const gchar *preupdate_str, *statements_str;

		preupdate_str = statements_str = NULL;

		if (tracker_sparql_builder_get_length (statements) > 0) {
			statements_str = tracker_sparql_builder_get_result (statements);
		}

		if (tracker_sparql_builder_get_length (preupdate) > 0) {
			preupdate_str = tracker_sparql_builder_get_result (preupdate);
		}

		g_print ("SPARQL pre-update:\n%s\n",
		         preupdate_str ? preupdate_str : "");
		g_print ("SPARQL item:\n%s\n",
		         statements_str ? statements_str : "");
		g_print ("SPARQL where clause:\n%s\n",
		         where ? where : "");

		g_object_unref (statements);
		g_object_unref (preupdate);
		g_free (where);
	}

	extract_task_free (task);
}
