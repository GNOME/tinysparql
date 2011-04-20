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
} TrackerExtractTask;

static void tracker_extract_finalize (GObject *object);
static void report_statistics        (GObject *object);

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
tracker_extract_init (TrackerExtract *object)
{
	TrackerExtractPrivate *priv;

#ifdef HAVE_LIBSTREAMANALYZER
	tracker_topanalyzer_init ();
#endif /* HAVE_STREAMANALYZER */

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);
	priv->statistics_data = g_hash_table_new (NULL, NULL);
}

static void
tracker_extract_finalize (GObject *object)
{
	TrackerExtractPrivate *priv;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

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

static gboolean
get_file_metadata (TrackerExtract         *extract,
                   const gchar            *uri,
                   const gchar            *mime,
                   TrackerSparqlBuilder  **preupdate_out,
                   TrackerSparqlBuilder  **statements_out,
                   gchar                 **where_out)
{
	TrackerExtractPrivate *priv;
	TrackerSparqlBuilder *statements, *preupdate;
	GString *where;
	gchar *mime_used = NULL;
#ifdef HAVE_LIBSTREAMANALYZER
	gchar *content_type = NULL;
#endif

	priv = TRACKER_EXTRACT_GET_PRIVATE (extract);

	*preupdate_out = NULL;
	*statements_out = NULL;
	*where_out = NULL;

	/* Create sparql builders to send back */
	preupdate = tracker_sparql_builder_new_update ();
	statements = tracker_sparql_builder_new_embedded_insert ();
	where = g_string_new ("");

#ifdef HAVE_LIBSTREAMANALYZER
	if (!priv->force_internal_extractors) {
		tracker_dbus_request_comment (request,
		                              "  Extracting with libstreamanalyzer...");

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
		tracker_dbus_request_comment (request,
		                              "  Extracting with internal extractors ONLY...");
	}
#endif /* HAVE_LIBSTREAMANALYZER */

	if (mime && *mime) {
		/* We know the mime */
		mime_used = g_strdup (mime);
		g_strstrip (mime_used);
	}
#ifdef HAVE_LIBSTREAMANALYZER
	else if (content_type && *content_type) {
		/* We know the mime from LSA */
		mime_used = content_type;
		g_strstrip (mime_used);
	}
#endif /* HAVE_LIBSTREAMANALYZER */
	else {
		GFile *file;
		GFileInfo *info;
		GError *error = NULL;

		file = g_file_new_for_uri (uri);
		if (!file) {
			g_warning ("Could not create GFile for uri:'%s'",
			           uri);
			g_object_unref (statements);
			g_object_unref (preupdate);
			g_string_free (where, TRUE);
			return FALSE;
		}

		info = g_file_query_info (file,
		                          G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
		                          G_FILE_QUERY_INFO_NONE,
		                          NULL,
		                          &error);

		if (error || !info) {
			/* FIXME: Propagate error */
			g_error_free (error);

			if (info) {
				g_object_unref (info);
			}

			g_object_unref (file);
			g_object_unref (statements);
			g_object_unref (preupdate);
			g_string_free (where, TRUE);

			return FALSE;
		}

		mime_used = g_strdup (g_file_info_get_content_type (info));

		g_object_unref (info);
		g_object_unref (file);
	}

	/* Now we have sanity checked everything, actually get the
	 * data we need from the extractors.
	 */
	if (mime_used) {
		TrackerExtractMetadataFunc func;
		GModule *module;

		module = tracker_extract_module_manager_get_for_mimetype (mime_used, &func);

		if (module) {
			StatisticsData *data;
			gint items;

			(func) (uri, mime_used, preupdate, statements, where);

			items = tracker_sparql_builder_get_length (statements);

			data = g_hash_table_lookup (priv->statistics_data, module);

			if (!data) {
				data = g_slice_new0 (StatisticsData);
				g_hash_table_insert (priv->statistics_data, module, data);
			}

			data->extracted_count++;

			if (items > 0) {
				tracker_sparql_builder_insert_close (statements);

				*preupdate_out = preupdate;
				*statements_out = statements;
				*where_out = g_string_free (where, FALSE);

				return TRUE;
			} else {
				data->failed_count++;
			}
		} else {
			priv->unhandled_count++;
		}
	}

	if (tracker_sparql_builder_get_length (statements) > 0) {
		tracker_sparql_builder_insert_close (statements);
	}

	*preupdate_out = preupdate;
	*statements_out = statements;
	*where_out = g_string_free (where, FALSE);

	return TRUE;
}

static void
tracker_extract_info_free (TrackerExtractInfo *info)
{
	if (info->statements) {
		g_object_unref (info->statements);
	}

	if (info->preupdate) {
		g_object_unref (info->preupdate);
	}

	g_free (info->where);
	g_slice_free (TrackerExtractInfo, info);
}

static TrackerExtractTask *
extract_task_new (TrackerExtract *extract,
                  const gchar    *file,
                  const gchar    *mimetype,
                  GCancellable   *cancellable,
                  GAsyncResult   *res)
{
	TrackerExtractTask *task;

	task = g_slice_new0 (TrackerExtractTask);
	task->cancellable = cancellable;
	task->res = g_object_ref (res);
	task->file = g_strdup (file);
	task->mimetype = g_strdup (mimetype);
	task->extract = extract;

	return task;
}

static void
extract_task_free (TrackerExtractTask *task)
{
	g_object_unref (task->res);
	g_free (task->file);
	g_free (task->mimetype);
	g_slice_free (TrackerExtractTask, task);
}

static gboolean
get_metadata_cb (gpointer user_data)
{
	TrackerExtractTask *task = user_data;
	TrackerExtractInfo *info;

#ifdef THREAD_ENABLE_TRACE
	g_debug ("Thread:%p (Main) --> File:'%s' - Extracted",
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

	info = g_slice_new (TrackerExtractInfo);

	if (get_file_metadata (task->extract,
	                       task->file, task->mimetype,
	                       &info->preupdate,
	                       &info->statements,
	                       &info->where)) {
		g_simple_async_result_set_op_res_gpointer ((GSimpleAsyncResult *) task->res,
		                                           info,
		                                           (GDestroyNotify) tracker_extract_info_free);
	} else {
		g_simple_async_result_set_error ((GSimpleAsyncResult *) task->res,
		                                 TRACKER_DBUS_ERROR, 0,
		                                 "Could not get any metadata for uri:'%s' and mime:'%s'",
		                                 task->file, task->mimetype);
		tracker_extract_info_free (info);
	}

	g_simple_async_result_complete_in_idle ((GSimpleAsyncResult *) task->res);
	extract_task_free (task);

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
	g_debug ("Thread:%p (Main) <-- File:'%s' - Extracting\n",
	         g_thread_self (),
	         file);
#endif /* THREAD_ENABLE_TRACE */

	res = g_simple_async_result_new (G_OBJECT (extract), cb, user_data, NULL);

	task = extract_task_new (extract, file, mimetype, cancellable, G_ASYNC_RESULT (res));
	g_idle_add (get_metadata_cb, task);

	/* task takes a ref */
	g_object_unref (res);
}

void
tracker_extract_get_metadata_by_cmdline (TrackerExtract *object,
                                         const gchar    *uri,
                                         const gchar    *mime)
{
	TrackerSparqlBuilder *statements, *preupdate;
	gchar *where;
	TrackerExtractPrivate *priv;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);
	priv->disable_summary_on_finalize = TRUE;

	g_return_if_fail (uri != NULL);

	g_message ("Extracting...");

	if (get_file_metadata (object, uri, mime, &preupdate, &statements, &where)) {
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
}
