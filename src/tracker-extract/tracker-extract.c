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

#define EXTRACT_FUNCTION "tracker_extract_get_data"

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
	GArray *specific_extractors;
	GArray *generic_extractors;
	gboolean disable_shutdown;
	gboolean force_internal_extractors;
	gboolean disable_summary_on_finalize;
} TrackerExtractPrivate;

typedef struct {
	const GModule *module;
	const TrackerExtractData *edata;
	GPatternSpec *pattern; /* For a fast g_pattern_match() */
	gint extracted_count;
	gint failed_count;
} ModuleData;

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
#ifdef HAVE_LIBSTREAMANALYZER
	tracker_topanalyzer_init ();
#endif /* HAVE_STREAMANALYZER */
}

static void
tracker_extract_finalize (GObject *object)
{
	TrackerExtractPrivate *priv;
	gint i;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

	if (!priv->disable_summary_on_finalize) {
		report_statistics (object);
	}

#ifdef HAVE_LIBSTREAMANALYZER
	tracker_topanalyzer_shutdown ();
#endif /* HAVE_STREAMANALYZER */

	for (i = 0; i < priv->specific_extractors->len; i++) {
		ModuleData *mdata;

		mdata = &g_array_index (priv->specific_extractors, ModuleData, i);
		g_pattern_spec_free (mdata->pattern);
	}
	g_array_free (priv->specific_extractors, TRUE);

	for (i = 0; i < priv->generic_extractors->len; i++) {
		ModuleData *mdata;

		mdata = &g_array_index (priv->generic_extractors, ModuleData, i);
		g_pattern_spec_free (mdata->pattern);
	}
	g_array_free (priv->generic_extractors, TRUE);

	G_OBJECT_CLASS (tracker_extract_parent_class)->finalize (object);
}

static void
report_statistics (GObject *object)
{
	TrackerExtractPrivate *priv;
	GHashTable *reported = NULL;
	gint i;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

	g_message ("--------------------------------------------------");
	g_message ("Statistics:");
	g_message ("  Specific Extractors:");

	reported = g_hash_table_new (g_direct_hash, g_direct_equal);

	for (i = 0; i < priv->specific_extractors->len; i++) {
		ModuleData *mdata;
		const gchar *name;

		mdata = &g_array_index (priv->specific_extractors, ModuleData, i);
		name = g_module_name ((GModule*) mdata->module);

		if ((mdata->extracted_count > 0 || mdata->failed_count > 0) &&
		    !g_hash_table_lookup (reported, name)) {
			const gchar *name_without_path;

			name_without_path = strrchr (name, G_DIR_SEPARATOR) + 1;

			g_message ("    Module:'%s', extracted:%d, failures:%d",
			           name_without_path,
			           mdata->extracted_count,
			           mdata->failed_count);
			g_hash_table_insert (reported, (gpointer) name, GINT_TO_POINTER(1));
		}
	}

	if (g_hash_table_size (reported) < 1) {
		g_message ("    No files handled");
	}

	g_hash_table_remove_all (reported);

	g_message ("  Generic Extractors:");

	for (i = 0; i < priv->generic_extractors->len; i++) {
		ModuleData *mdata;
		const gchar *name;

		mdata = &g_array_index (priv->generic_extractors, ModuleData, i);
		name = g_module_name ((GModule*) mdata->module);

		if ((mdata->extracted_count > 0 || mdata->failed_count > 0) &&
		    !g_hash_table_lookup (reported, name)) {
			const gchar *name_without_path;

			name_without_path = strrchr (name, G_DIR_SEPARATOR) + 1;

			g_message ("    Module:'%s', extracted:%d, failed:%d",
			           name_without_path,
			           mdata->extracted_count,
			           mdata->failed_count);
			g_hash_table_insert (reported, (gpointer) name, GINT_TO_POINTER(1));
		}
	}

	if (g_hash_table_size (reported) < 1) {
		g_message ("    No files handled");
	}

	g_message ("--------------------------------------------------");

	g_hash_table_unref (reported);
}

static gboolean
load_modules (const gchar  *force_module,
              GArray      **specific_extractors,
              GArray      **generic_extractors)
{
	GDir *dir;
	GError *error = NULL;
	const gchar *name;
	gchar *force_module_checked;
	gboolean success;
	const gchar *extractors_dir;

	extractors_dir = g_getenv ("TRACKER_EXTRACTORS_DIR");
	if (G_LIKELY (extractors_dir == NULL)) {
		extractors_dir = TRACKER_EXTRACTORS_DIR;
	} else {
		g_message ("Extractor modules directory is '%s' (set in env)", extractors_dir);
	}

	dir = g_dir_open (extractors_dir, 0, &error);

	if (!dir) {
		g_error ("Error opening modules directory: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	if (G_UNLIKELY (force_module)) {
		if (!g_str_has_suffix (force_module, "." G_MODULE_SUFFIX)) {
			force_module_checked = g_strdup_printf ("%s.%s",
			                                        force_module,
			                                        G_MODULE_SUFFIX);
		} else {
			force_module_checked = g_strdup (force_module);
		}
	} else {
		force_module_checked = NULL;
	}

	*specific_extractors = g_array_new (FALSE,
	                                    TRUE,
	                                    sizeof (ModuleData));

	*generic_extractors = g_array_new (FALSE,
	                                   TRUE,
	                                   sizeof (ModuleData));

#ifdef HAVE_LIBSTREAMANALYZER
	if (!force_internal_extractors) {
		g_message ("Adding extractor for libstreamanalyzer");
		g_message ("  Generic  match for ALL (tried first before our module)");
		g_message ("  Specific match for NONE (fallback to our modules)");
	} else {
		g_message ("Not using libstreamanalyzer");
		g_message ("  It is available but disabled by command line");
	}
#endif /* HAVE_STREAMANALYZER */

	while ((name = g_dir_read_name (dir)) != NULL) {
		TrackerExtractDataFunc func;
		GModule *module;
		gchar *module_path;

		if (!g_str_has_suffix (name, "." G_MODULE_SUFFIX)) {
			continue;
		}

		if (force_module_checked && strcmp (name, force_module_checked) != 0) {
			continue;
		}

		module_path = g_build_filename (extractors_dir, name, NULL);

		module = g_module_open (module_path, G_MODULE_BIND_LOCAL);

		if (!module) {
			g_warning ("Could not load module '%s': %s",
			           name,
			           g_module_error ());
			g_free (module_path);
			continue;
		}

		g_module_make_resident (module);

		if (g_module_symbol (module, EXTRACT_FUNCTION, (gpointer *) &func)) {
			ModuleData mdata = { 0 };

			mdata.module = module;
			mdata.edata = (func) ();

			g_message ("Adding extractor:'%s' with:",
			           g_module_name ((GModule*) mdata.module));

			for (; mdata.edata->mime; mdata.edata++) {
				/* Compile pattern from mime */
				mdata.pattern = g_pattern_spec_new (mdata.edata->mime);

				if (G_UNLIKELY (strchr (mdata.edata->mime, '*') != NULL)) {
					g_message ("  Generic  match for mime:'%s'",
					           mdata.edata->mime);
					g_array_append_val (*generic_extractors, mdata);
				} else {
					g_message ("  Specific match for mime:'%s'",
					           mdata.edata->mime);
					g_array_append_val (*specific_extractors, mdata);
				}
			}
		} else {
			g_warning ("Could not load module '%s': Function %s() was not found, is it exported?",
			           name, EXTRACT_FUNCTION);
		}

		g_free (module_path);
	}

	if (G_UNLIKELY (force_module) &&
	    (!*specific_extractors || (*specific_extractors)->len < 1) &&
	    (!*generic_extractors || (*generic_extractors)->len < 1)) {
		g_warning ("Could not force module '%s', it was not found", force_module_checked);
		success = FALSE;
	} else {
		success = TRUE;
	}

	g_free (force_module_checked);
	g_dir_close (dir);

	return success;
}

TrackerExtract *
tracker_extract_new (gboolean     disable_shutdown,
                     gboolean     force_internal_extractors,
                     const gchar *force_module)
{
	TrackerExtract *object;
	TrackerExtractPrivate *priv;
	GArray *specific_extractors;
	GArray *generic_extractors;

	if (!g_module_supported ()) {
		g_error ("Modules are not supported for this platform");
		return NULL;
	}

	if (!load_modules (force_module, &specific_extractors, &generic_extractors)) {
		return NULL;
	}

	/* Set extractors */
	object = g_object_new (TRACKER_TYPE_EXTRACT, NULL);

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

	priv->disable_shutdown = disable_shutdown;
	priv->force_internal_extractors = force_internal_extractors;

	priv->specific_extractors = specific_extractors;
	priv->generic_extractors = generic_extractors;

	return object;
}

static gboolean
get_file_metadata (TrackerExtract         *extract,
                   const gchar            *uri,
                   const gchar            *mime,
                   TrackerSparqlBuilder  **preupdate_out,
                   TrackerSparqlBuilder  **statements_out)
{
	TrackerExtractPrivate *priv;
	TrackerSparqlBuilder *statements, *preupdate;
	gchar *mime_used = NULL;
#ifdef HAVE_LIBSTREAMANALYZER
	gchar *content_type = NULL;
#endif
	gint items;

	g_debug ("Extracting...");

	priv = TRACKER_EXTRACT_GET_PRIVATE (extract);

	*preupdate_out = NULL;
	*statements_out = NULL;

	/* Create sparql builders to send back */
	preupdate = tracker_sparql_builder_new_update ();
	statements = tracker_sparql_builder_new_embedded_insert ();

#ifdef HAVE_LIBSTREAMANALYZER
	if (!priv->force_internal_extractors) {
		g_debug ("  Using libstreamanalyzer...");

		tracker_topanalyzer_extract (uri, statements, &content_type);

		if (tracker_sparql_builder_get_length (statements) > 0) {
			g_free (content_type);
			tracker_sparql_builder_insert_close (statements);

			*preupdate_out = preupdate;
			*statements_out = statements;
			return TRUE;
		}
	} else {
		g_debug ("  Using internal extractors ONLY...");
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
		guint i;
		glong length;
		gchar *reversed;

		/* Using a reversed string while pattern matching is faster
		 * if we have lots of patterns with wildcards.
		 * We are assuming here that mime_used is ASCII always, so
		 * we avoid g_utf8_strreverse() */
		reversed = g_strdup (mime_used);
		g_strreverse (reversed);
		length = strlen (mime_used);

		for (i = 0; i < priv->specific_extractors->len; i++) {
			const TrackerExtractData *edata;
			ModuleData *mdata;

			mdata = &g_array_index (priv->specific_extractors, ModuleData, i);
			edata = mdata->edata;

			if (g_pattern_match (mdata->pattern, length, mime_used, reversed)) {
				gint items;

				g_debug ("  Using %s... (specific match)", 
				         g_module_name ((GModule*) mdata->module));

				(*edata->func) (uri, preupdate, statements);

				items = tracker_sparql_builder_get_length (statements);

				mdata->extracted_count++;

				if (items == 0) {
					mdata->failed_count++;
					continue;
				}

				tracker_sparql_builder_insert_close (statements);

				g_free (mime_used);
				g_free (reversed);

				*preupdate_out = preupdate;
				*statements_out = statements;

				g_debug ("Done (%d items)", items);

				return TRUE;
			}
		}

		for (i = 0; i < priv->generic_extractors->len; i++) {
			const TrackerExtractData *edata;
			ModuleData *mdata;

			mdata = &g_array_index (priv->generic_extractors, ModuleData, i);
			edata = mdata->edata;

			if (g_pattern_match (mdata->pattern, length, mime_used, reversed)) {
				gint items;

				g_debug ("  Using %s... (generic match)", 
				         g_module_name ((GModule*) mdata->module));

				(*edata->func) (uri, preupdate, statements);

				items = tracker_sparql_builder_get_length (statements);

				mdata->extracted_count++;

				if (items == 0) {
					mdata->failed_count++;
					continue;
				}

				tracker_sparql_builder_insert_close (statements);

				g_free (mime_used);
				g_free (reversed);

				*preupdate_out = preupdate;
				*statements_out = statements;

				g_debug ("Done (%d items)", items);

				return TRUE;
			}
		}

		g_debug ("  No extractor was available for this mime type:'%s'",
		         mime_used);

		g_free (mime_used);
		g_free (reversed);
	}

	items = tracker_sparql_builder_get_length (statements);

	if (items > 0) {
		tracker_sparql_builder_insert_close (statements);
	}

	*preupdate_out = preupdate;
	*statements_out = statements;

	g_debug ("No extractor or failed (%d items)", items);

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
	                       &info->statements)) {
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
	TrackerExtractPrivate *priv;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);
	priv->disable_summary_on_finalize = TRUE;

	g_return_if_fail (uri != NULL);

	if (get_file_metadata (object, uri, mime, &preupdate, &statements)) {
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

		g_object_unref (statements);
		g_object_unref (preupdate);
	}
}
