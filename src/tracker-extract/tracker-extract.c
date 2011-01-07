/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
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

#include <libtracker-common/tracker-common.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-extract.h"
#include "tracker-main.h"
#include "tracker-marshal.h"

#ifdef HAVE_LIBSTREAMANALYZER
#include "tracker-topanalyzer.h"
#endif /* HAVE_STREAMANALYZER */

#define EXTRACT_FUNCTION "tracker_extract_get_data"

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
	GDBusConnection *d_connection;
	GDBusNodeInfo *introspection_data;
	guint registration_id;
	guint own_id;
} TrackerExtractPrivate;

typedef struct {
	const GModule *module;
	const TrackerExtractData *edata;
	GPatternSpec *pattern; /* For a fast g_pattern_match() */
	gint extracted_count;
	gint failed_count;
} ModuleData;

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
			ModuleData mdata;

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
                   TrackerDBusRequest     *request,
                   GDBusMethodInvocation  *invocation,
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

	priv = TRACKER_EXTRACT_GET_PRIVATE (extract);

	*preupdate_out = NULL;
	*statements_out = NULL;

	/* Create sparql builders to send back */
	preupdate = tracker_sparql_builder_new_update ();
	statements = tracker_sparql_builder_new_embedded_insert ();

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
			return FALSE;
		}

		info = g_file_query_info (file,
		                          G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
		                          G_FILE_QUERY_INFO_NONE,
		                          NULL,
		                          &error);

		if (error || !info) {
			tracker_dbus_request_comment (request,
			                              "  Could not create GFileInfo for file size check, %s",
			                              error ? error->message : "no error given");
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

		tracker_dbus_request_comment (request,
		                              "  Guessing mime type as '%s' for uri:'%s'",
		                              mime_used,
		                              uri);

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

				tracker_dbus_request_comment (request,
				                              "  Extracting with module:'%s'",
				                              g_module_name ((GModule*) mdata->module));

				(*edata->func) (uri, preupdate, statements);

				items = tracker_sparql_builder_get_length (statements);

				tracker_dbus_request_comment (request,
				                              "  Found %d metadata items",
				                              items);

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

				tracker_dbus_request_comment (request,
				                              "  Extracting with module:'%s'",
				                              g_module_name ((GModule*) mdata->module));

				(*edata->func) (uri, preupdate, statements);

				items = tracker_sparql_builder_get_length (statements);

				tracker_dbus_request_comment (request,
				                              "  Found %d metadata items",
				                              items);

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

				return TRUE;
			}
		}

		tracker_dbus_request_comment (request,
		                              "  Could not find any extractors to handle metadata type "
		                              "(mime: %s)",
		                              mime_used);

		g_free (mime_used);
		g_free (reversed);
	} else {
		tracker_dbus_request_comment (request,
		                              "  No mime available, not extracting data");
	}

	if (tracker_sparql_builder_get_length (statements) > 0) {
		tracker_sparql_builder_insert_close (statements);
	}

	*preupdate_out = preupdate;
	*statements_out = statements;

	return TRUE;
}

void
tracker_extract_get_metadata_by_cmdline (TrackerExtract *object,
                                         const gchar    *uri,
                                         const gchar    *mime)
{
	TrackerDBusRequest *request;
	TrackerSparqlBuilder *statements, *preupdate;
	TrackerExtractPrivate *priv;

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);
	priv->disable_summary_on_finalize = TRUE;

	g_return_if_fail (uri != NULL);

	request = tracker_dbus_request_begin (NULL,
	                                      "%s(uri:'%s', mime:%s)",
	                                      __FUNCTION__,
	                                      uri,
	                                      mime);

	/* NOTE: Don't reset the timeout to shutdown here */

	if (get_file_metadata (object, request,
			       NULL, uri, mime,
			       &preupdate, &statements)) {
		const gchar *preupdate_str, *statements_str;

		preupdate_str = statements_str = NULL;

		if (tracker_sparql_builder_get_length (statements) > 0) {
			statements_str = tracker_sparql_builder_get_result (statements);
		}

		if (tracker_sparql_builder_get_length (preupdate) > 0) {
			preupdate_str = tracker_sparql_builder_get_result (preupdate);
		}

		tracker_dbus_request_info (request, "%s",
		                           preupdate_str ? preupdate_str : "");
		tracker_dbus_request_info (request, "%s",
		                           statements_str ? statements_str : "");

		g_object_unref (statements);
		g_object_unref (preupdate);
	}

	tracker_dbus_request_end (request, NULL);
}

static void
handle_method_call_get_pid (TrackerExtract        *object,
                            GDBusMethodInvocation *invocation,
                            GVariant              *parameters)
{
	TrackerDBusRequest *request;
	pid_t value;

	request = tracker_g_dbus_request_begin (invocation,
	                                        "%s()",
	                                        __FUNCTION__);

	value = getpid ();
	tracker_dbus_request_debug (request,
	                            "PID is %d",
	                            value);

	tracker_dbus_request_end (request, NULL);

	g_dbus_method_invocation_return_value (invocation,
	                                       g_variant_new ("(i)", (gint) value));
}

static void
handle_method_call_get_metadata (TrackerExtract        *object,
                                 GDBusMethodInvocation *invocation,
                                 GVariant              *parameters)
{
	TrackerDBusRequest *request;
	TrackerExtractPrivate *priv;
	TrackerSparqlBuilder *sparql, *preupdate;
	gboolean extracted = FALSE;
	const gchar *uri = NULL, *mime = NULL;

	g_variant_get (parameters, "(&s&s)", &uri, &mime);

	tracker_gdbus_async_return_if_fail (uri != NULL, invocation);

	request = tracker_g_dbus_request_begin (invocation,
	                                        "%s(uri:'%s', mime:%s)",
	                                        __FUNCTION__,
	                                        uri,
	                                        mime);

	tracker_dbus_request_debug (request,
	                            "  Resetting shutdown timeout");

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

	tracker_main_quit_timeout_reset ();
	if (!priv->disable_shutdown) {
		alarm (MAX_EXTRACT_TIME);
	}

	extracted = get_file_metadata (object,
	                               request,
	                               invocation,
	                               uri,
	                               mime,
	                               &preupdate,
	                               &sparql);

	if (extracted) {
		tracker_dbus_request_end (request, NULL);

		if (tracker_sparql_builder_get_length (sparql) > 0) {
			const gchar *preupdate_str = NULL;

			if (tracker_sparql_builder_get_length (preupdate) > 0) {
				preupdate_str = tracker_sparql_builder_get_result (preupdate);
			}

			g_dbus_method_invocation_return_value (invocation,
			                                       g_variant_new ("(ss)",
			                                                      preupdate_str ? preupdate_str : "",
			                                                      tracker_sparql_builder_get_result (sparql)));
		} else {
			g_dbus_method_invocation_return_value (invocation,
			                                       g_variant_new ("(ss)", "", ""));
		}

		g_object_unref (sparql);
		g_object_unref (preupdate);
	} else {
		GError *actual_error;

		actual_error = g_error_new (TRACKER_DBUS_ERROR, 0,
		                            "Could not get any metadata for uri:'%s' and mime:'%s'",
		                            uri,
		                            mime);
		tracker_dbus_request_end (request, actual_error);
		g_dbus_method_invocation_return_gerror (invocation, actual_error);
		g_error_free (actual_error);
	}

	if (!priv->disable_shutdown) {
		/* Unset alarm so the extractor doesn't die when it's idle */
		alarm (0);
	}
}

static void
handle_method_call_get_metadata_fast (TrackerExtract        *object,
                                      GDBusMethodInvocation *invocation,
                                      GVariant              *parameters)
{
	TrackerDBusRequest *request;
	TrackerExtractPrivate *priv;
	GDBusMessage *reply;
	const gchar *uri, *mime;
	int fd;
	GOutputStream *unix_output_stream;
	GOutputStream *buffered_output_stream;
	GDataOutputStream *data_output_stream;
	GError *error = NULL;
	TrackerSparqlBuilder *sparql, *preupdate;
	gboolean extracted = FALSE;
	GDBusMessage *method_message;
	GDBusConnection *connection;

	connection = g_dbus_method_invocation_get_connection (invocation);
	method_message = g_dbus_method_invocation_get_message (invocation);

	g_variant_get (parameters, "(&s&sh)", &uri, &mime, &fd);

	request = tracker_g_dbus_request_begin (invocation,
	                                        "%s(uri:'%s', mime:%s)",
	                                        __FUNCTION__,
	                                        uri,
	                                        mime);

	tracker_dbus_request_debug (request,
	                            "  Resetting shutdown timeout");

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

	tracker_main_quit_timeout_reset ();
	if (!priv->disable_shutdown) {
		alarm (MAX_EXTRACT_TIME);
	}

	extracted = get_file_metadata (object, request, NULL, uri, mime, &preupdate, &sparql);

	if (extracted) {
		unix_output_stream = g_unix_output_stream_new (fd, TRUE);
		buffered_output_stream = g_buffered_output_stream_new_sized (unix_output_stream,
		                                                             64*1024);
		data_output_stream = g_data_output_stream_new (buffered_output_stream);
		g_data_output_stream_set_byte_order (G_DATA_OUTPUT_STREAM (data_output_stream),
		                                     G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN);

		if (tracker_sparql_builder_get_length (sparql) > 0) {
			const gchar *preupdate_str = NULL;

			if (tracker_sparql_builder_get_length (preupdate) > 0) {
				preupdate_str = tracker_sparql_builder_get_result (preupdate);
			}

			g_data_output_stream_put_string (data_output_stream,
			                                 preupdate_str ? preupdate_str : "",
			                                 NULL,
			                                 &error);

			if (!error) {
				g_data_output_stream_put_byte (data_output_stream,
				                               0,
				                               NULL,
				                               &error);
			}

			if (!error) {
				g_data_output_stream_put_string (data_output_stream,
				                                 tracker_sparql_builder_get_result (sparql),
				                                 NULL,
				                                 &error);
			}

			if (!error) {
				g_data_output_stream_put_byte (data_output_stream,
				                               0,
				                               NULL,
				                               &error);
			}
		}

		g_object_unref (sparql);
		g_object_unref (preupdate);
		g_object_unref (data_output_stream);
		g_object_unref (buffered_output_stream);
		g_object_unref (unix_output_stream);

		if (error) {
			tracker_dbus_request_end (request, error);
			reply = g_dbus_message_new_method_error_literal (method_message,
			                                                 TRACKER_EXTRACT_SERVICE ".GetMetadataFastError",
			                                                 error->message);
			g_error_free (error);
		} else {
			tracker_dbus_request_end (request, NULL);
			reply = g_dbus_message_new_method_reply (method_message);
		}
	} else {
		error = g_error_new (TRACKER_DBUS_ERROR, 0,
		                     "Could not get any metadata for uri:'%s' and mime:'%s'", uri, mime);
		tracker_dbus_request_end (request, error);
		reply = g_dbus_message_new_method_error_literal (method_message,
		                                                 TRACKER_EXTRACT_SERVICE ".GetMetadataFastError",
		                                                 error->message);
		g_error_free (error);
		close (fd);
	}

	g_dbus_connection_send_message (connection, reply,
	                                G_DBUS_SEND_MESSAGE_FLAGS_NONE,
	                                NULL, NULL);

	g_object_unref (reply);

	if (!priv->disable_shutdown) {
		/* Unset alarm so the extractor doesn't die when it's idle */
		alarm (0);
	}
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
	TrackerExtract *extract = user_data;

	if (g_strcmp0 (method_name, "GetPid") == 0) {
		handle_method_call_get_pid (extract, invocation, parameters);
	} else
	if (g_strcmp0 (method_name, "GetMetadataFast") == 0) {
		handle_method_call_get_metadata_fast (extract, invocation, parameters);
	} else
	if (g_strcmp0 (method_name, "GetMetadata") == 0) {
		handle_method_call_get_metadata (extract, invocation, parameters);
	} else {
		g_assert_not_reached ();
	}
}

static GVariant *
handle_get_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data)
{
	g_assert_not_reached ();
	return NULL;
}

static gboolean
handle_set_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data)
{
	g_assert_not_reached ();
	return TRUE;
}

void
tracker_extract_dbus_start (TrackerExtract *extract)
{
	TrackerExtractPrivate *priv;
	GError *error = NULL;
	GDBusInterfaceVTable interface_vtable = {
		handle_method_call,
		handle_get_property,
		handle_set_property
	};

	priv = TRACKER_EXTRACT_GET_PRIVATE (extract);

	priv->d_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (!priv->d_connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            error ? error->message : "no error given.");
		g_clear_error (&error);
		return;
	}

	priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

	priv->own_id = g_bus_own_name_on_connection (priv->d_connection,
	                                             TRACKER_EXTRACT_SERVICE,
	                                             G_BUS_NAME_OWNER_FLAGS_NONE,
	                                             NULL, NULL, NULL, NULL);

	g_message ("Registering D-Bus object...");
	g_message ("  Path:'" TRACKER_EXTRACT_PATH "'");
	g_message ("  Object Type:'%s'", G_OBJECT_TYPE_NAME (extract));

	priv->registration_id =
		g_dbus_connection_register_object (priv->d_connection,
		                                   TRACKER_EXTRACT_PATH,
		                                   priv->introspection_data->interfaces[0],
		                                   &interface_vtable,
		                                   extract,
		                                   NULL,
		                                   &error);

	if (error) {
		g_critical ("Could not register the D-Bus object "TRACKER_EXTRACT_PATH", %s",
		            error ? error->message : "no error given.");
		g_clear_error (&error);
		return;
	}
}

void
tracker_extract_dbus_stop (TrackerExtract *extract)
{
	TrackerExtractPrivate *priv;

	priv = TRACKER_EXTRACT_GET_PRIVATE (extract);

	if (priv->own_id != 0) {
		g_bus_unown_name (priv->own_id);
	}

	if (priv->registration_id != 0) {
		g_dbus_connection_unregister_object (priv->d_connection,
		                                     priv->registration_id);
	}

	if (priv->introspection_data) {
		g_dbus_node_info_unref (priv->introspection_data);
	}

	if (priv->d_connection) {
		g_object_unref (priv->d_connection);
	}
}
