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

#include "tracker-dbus.h"
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

#define TRACKER_EXTRACT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_EXTRACT, TrackerExtractPrivate))

extern gboolean debug;

typedef struct {
	GArray *specific_extractors;
	GArray *generic_extractors;
	gboolean disable_shutdown;
	gboolean force_internal_extractors;
} TrackerExtractPrivate;

typedef struct {
	const GModule *module;
	const TrackerExtractData *edata;
}  ModuleData;


static void tracker_extract_finalize (GObject *object);

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

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

#ifdef HAVE_LIBSTREAMANALYZER
	tracker_topanalyzer_shutdown ();
#endif /* HAVE_STREAMANALYZER */

	g_array_free (priv->specific_extractors, TRUE);
	g_array_free (priv->generic_extractors, TRUE);

	G_OBJECT_CLASS (tracker_extract_parent_class)->finalize (object);
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
                   guint                   request_id,
                   DBusGMethodInvocation  *context,
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
		tracker_dbus_request_comment (request_id, context,
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
		tracker_dbus_request_comment (request_id, context,
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
			tracker_dbus_request_comment (request_id,
			                              context,
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

		tracker_dbus_request_comment (request_id,
		                              context,
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

		for (i = 0; i < priv->specific_extractors->len; i++) {
			const TrackerExtractData *edata;
			ModuleData mdata;

			mdata = g_array_index (priv->specific_extractors, ModuleData, i);
			edata = mdata.edata;

			if (g_pattern_match_simple (edata->mime, mime_used)) {
				gint items;

				tracker_dbus_request_comment (request_id,
				                              context,
				                              "  Extracting with module:'%s'",
				                              g_module_name ((GModule*) mdata.module));

				(*edata->func) (uri, preupdate, statements);

				items = tracker_sparql_builder_get_length (statements);

				tracker_dbus_request_comment (request_id,
				                              context,
				                              "  Found %d metadata items",
				                              items);
				if (items == 0) {
					continue;
				}

				tracker_sparql_builder_insert_close (statements);

				g_free (mime_used);

				*preupdate_out = preupdate;
				*statements_out = statements;

				return TRUE;
			}
		}

		for (i = 0; i < priv->generic_extractors->len; i++) {
			const TrackerExtractData *edata;
			ModuleData mdata;

			mdata = g_array_index (priv->generic_extractors, ModuleData, i);
			edata = mdata.edata;

			if (g_pattern_match_simple (edata->mime, mime_used)) {
				gint items;

				tracker_dbus_request_comment (request_id,
				                              context,
				                              "  Extracting with module:'%s'",
				                              g_module_name ((GModule*) mdata.module));

				(*edata->func) (uri, preupdate, statements);

				items = tracker_sparql_builder_get_length (statements);

				tracker_dbus_request_comment (request_id,
				                              context,
				                              "  Found %d metadata items",
				                              items);
				if (items == 0) {
					continue;
				}

				tracker_sparql_builder_insert_close (statements);

				g_free (mime_used);

				*preupdate_out = preupdate;
				*statements_out = statements;

				return TRUE;
			}
		}

		g_free (mime_used);

		tracker_dbus_request_comment (request_id,
		                              context,
		                              "  Could not find any extractors to handle metadata type");
	} else {
		tracker_dbus_request_comment (request_id,
		                              context,
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
	guint request_id;
	TrackerSparqlBuilder *statements, *preupdate;

	request_id = tracker_dbus_get_next_request_id ();

	g_return_if_fail (uri != NULL);

	tracker_dbus_request_new (request_id,
	                          NULL,
	                          "%s(uri:'%s', mime:%s)",
	                          __FUNCTION__,
	                          uri,
	                          mime);

	/* NOTE: Don't reset the timeout to shutdown here */

	if (get_file_metadata (object, request_id,
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

		tracker_dbus_request_info (request_id, NULL, "%s",
					   preupdate_str ? preupdate_str : "");
		tracker_dbus_request_info (request_id, NULL, "%s",
					   statements_str ? statements_str : "");

		g_object_unref (statements);
		g_object_unref (preupdate);
	}

	tracker_dbus_request_success (request_id, NULL);
}

void
tracker_extract_get_pid (TrackerExtract         *object,
                         DBusGMethodInvocation  *context,
                         GError                **error)
{
	guint request_id;
	pid_t value;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
	                          context,
	                          "%s()",
	                          __FUNCTION__);

	value = getpid ();
	tracker_dbus_request_debug (request_id,
	                            context,
	                            "PID is %d",
	                            value);

	tracker_dbus_request_success (request_id, context);
	dbus_g_method_return (context, value);
}

void
tracker_extract_get_metadata (TrackerExtract         *object,
                              const gchar            *uri,
                              const gchar            *mime,
                              DBusGMethodInvocation  *context,
                              GError                **error)
{
	guint request_id;
	TrackerExtractPrivate *priv;
	TrackerSparqlBuilder *sparql, *preupdate;
	gboolean extracted = FALSE;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
	                          context,
	                          "%s(uri:'%s', mime:%s)",
	                          __FUNCTION__,
	                          uri,
	                          mime);

	tracker_dbus_request_debug (request_id,
	                            context,
	                            "  Resetting shutdown timeout");

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

	tracker_main_quit_timeout_reset ();
	if (!priv->disable_shutdown) {
		alarm (MAX_EXTRACT_TIME);
	}

	extracted = get_file_metadata (object, request_id, context, uri, mime, &preupdate, &sparql);

	if (extracted) {
		tracker_dbus_request_success (request_id, context);

		if (tracker_sparql_builder_get_length (sparql) > 0) {
			const gchar *preupdate_str = NULL;

			if (tracker_sparql_builder_get_length (preupdate) > 0) {
				preupdate_str = tracker_sparql_builder_get_result (preupdate);
			}

			dbus_g_method_return (context,
			                      preupdate_str ? preupdate_str : "",
			                      tracker_sparql_builder_get_result (sparql));
		} else {
			dbus_g_method_return (context, "", "");
		}

		g_object_unref (sparql);
		g_object_unref (preupdate);
	} else {
		GError *actual_error = NULL;

		tracker_dbus_request_failed (request_id,
		                             context,
		                             &actual_error,
		                             "Could not get any metadata for uri:'%s' and mime:'%s'",
		                             uri,
		                             mime);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
	}

	if (!priv->disable_shutdown) {
		/* Unset alarm so the extractor doesn't die when it's idle */
		alarm (0);
	}
}

#ifdef HAVE_DBUS_FD_PASSING
static void
get_metadata_fast (TrackerExtract *object,
                   DBusConnection *connection,
                   DBusMessage    *message)
{
	guint request_id;
	const gchar *expected_signature;
	TrackerExtractPrivate *priv;
	DBusError dbus_error;
	DBusMessage *reply;
	const gchar *uri, *mime;
	int fd;
	GOutputStream *unix_output_stream;
	GOutputStream *buffered_output_stream;
	GDataOutputStream *data_output_stream;
	GError *error = NULL;
	TrackerSparqlBuilder *sparql, *preupdate;
	gboolean extracted = FALSE;

	request_id = tracker_dbus_get_next_request_id ();

	expected_signature = DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_UNIX_FD_AS_STRING;

	if (g_strcmp0 (dbus_message_get_signature (message), expected_signature)) {
		gchar *error_message = g_strdup_printf (UNKNOWN_METHOD_MESSAGE,
		                                        "GetMetadataFast",
		                                        dbus_message_get_signature (message),
		                                        dbus_message_get_interface (message),
		                                        expected_signature);
		tracker_dbus_request_new (request_id,
		                          NULL,
		                          "%s()",
		                          __FUNCTION__);

		reply = dbus_message_new_error (message,
		                                DBUS_ERROR_UNKNOWN_METHOD,
		                                error_message);
		dbus_connection_send (connection, reply, NULL);

		tracker_dbus_request_failed (request_id,
		                             NULL,
		                             NULL,
		                             error_message);

		dbus_message_unref (reply);
		g_free (error_message);

		return;
	}

	dbus_error_init (&dbus_error);

	dbus_message_get_args (message,
	                       &dbus_error,
	                       DBUS_TYPE_STRING, &uri,
	                       DBUS_TYPE_STRING, &mime,
	                       DBUS_TYPE_UNIX_FD, &fd,
	                       DBUS_TYPE_INVALID);

	if (dbus_error_is_set (&dbus_error)) {
		tracker_dbus_request_new (request_id,
		                          NULL,
		                          "%s()",
		                          __FUNCTION__);

		reply = dbus_message_new_error (message, dbus_error.name, dbus_error.message);
		dbus_connection_send (connection, reply, NULL);

		tracker_dbus_request_failed (request_id,
		                             NULL,
		                             NULL,
		                             dbus_error.message);

		dbus_message_unref (reply);
		dbus_error_free (&dbus_error);

		return;
	}

	tracker_dbus_request_new (request_id,
	                          NULL,
	                          "%s(uri:'%s', mime:%s)",
	                          __FUNCTION__,
	                          uri,
	                          mime);

	tracker_dbus_request_debug (request_id,
	                            NULL,
	                            "  Resetting shutdown timeout");

	priv = TRACKER_EXTRACT_GET_PRIVATE (object);

	tracker_main_quit_timeout_reset ();
	if (!priv->disable_shutdown) {
		alarm (MAX_EXTRACT_TIME);
	}

	extracted = get_file_metadata (object, request_id, NULL, uri, mime, &preupdate, &sparql);

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
			tracker_dbus_request_failed (request_id,
			                             NULL,
			                             &error,
			                             NULL);
			reply = dbus_message_new_error (message,
			                                TRACKER_EXTRACT_SERVICE ".GetMetadataFastError",
			                                error->message);
			dbus_connection_send (connection, reply, NULL);
			dbus_message_unref (reply);
			g_error_free (error);
		} else {
			tracker_dbus_request_success (request_id, NULL);
			reply = dbus_message_new_method_return (message);
			dbus_connection_send (connection, reply, NULL);
			dbus_message_unref (reply);
		}
	} else {
		gchar *error_message = g_strdup_printf ("Could not get any metadata for uri:'%s' and mime:'%s'", uri, mime);
		tracker_dbus_request_failed (request_id,
		                             NULL,
		                             NULL,
		                             error_message);
		reply = dbus_message_new_error (message,
		                                TRACKER_EXTRACT_SERVICE ".GetMetadataFastError",
		                                error_message);
		close (fd);
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);
		g_free (error_message);
	}

	if (!priv->disable_shutdown) {
		/* Unset alarm so the extractor doesn't die when it's idle */
		alarm (0);
	}
}

DBusHandlerResult
tracker_extract_connection_filter (DBusConnection *connection,
                                   DBusMessage    *message,
                                   void           *user_data)
{
	TrackerExtract *extract;

	g_return_val_if_fail (connection != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
	g_return_val_if_fail (message != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (g_strcmp0 (TRACKER_EXTRACT_PATH, dbus_message_get_path (message))) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (g_strcmp0 (TRACKER_EXTRACT_INTERFACE, dbus_message_get_interface (message))) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	/* Only check if the user_data is our TrackerExtract AFTER having checked that
	 * the message matches expected path and interface. */
	extract = user_data;
	g_return_val_if_fail (TRACKER_IS_EXTRACT (extract), DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (!g_strcmp0 ("GetMetadataFast", dbus_message_get_member (message))) {
		get_metadata_fast (extract, connection, message);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
#endif /* HAVE_DBUS_FD_PASSING */
