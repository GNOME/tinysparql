/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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
#include "tracker-extract-client.h"

#include <string.h>
#include <gio/gunixfdlist.h>
#include <gio/gunixinputstream.h>

/* Size of buffers used when sending data over a pipe, using DBus FD passing */
#define DBUS_PIPE_BUFFER_SIZE      65536

#define DBUS_SERVICE_EXTRACT       "org.freedesktop.Tracker1.Extract"
#define DBUS_PATH_EXTRACT          "/org/freedesktop/Tracker1/Extract"
#define DBUS_INTERFACE_EXTRACT     "org.freedesktop.Tracker1.Extract"

static GDBusConnection *connection = NULL;

struct TrackerExtractInfo {
	gchar *preupdate;
	gchar *update;
	gchar *where;
};

typedef void (* SendAndSpliceCallback) (void     *buffer,
                                        gssize    buffer_size,
                                        GError   *error, /* Don't free */
                                        gpointer  user_data);

typedef struct {
	GInputStream *unix_input_stream;
	GInputStream *buffered_input_stream;
	GOutputStream *output_stream;
	SendAndSpliceCallback callback;
	GCancellable *cancellable;
	GSimpleAsyncResult *res;
	gboolean splice_finished;
	gboolean dbus_finished;
	GError *error;
} SendAndSpliceData;

static TrackerExtractInfo *
tracker_extract_info_new (const gchar *preupdate,
                          const gchar *update,
                          const gchar *where)
{
	TrackerExtractInfo *info;

	info = g_slice_new0 (TrackerExtractInfo);
	info->preupdate = g_strdup (preupdate);
	info->update = g_strdup (update);
	info->where = g_strdup (where);

	return info;
}

static void
tracker_extract_info_free (TrackerExtractInfo *info)
{
	g_free (info->preupdate);
	g_free (info->update);
	g_free (info->where);

	g_slice_free (TrackerExtractInfo, info);
}

static SendAndSpliceData *
send_and_splice_data_new (GInputStream          *unix_input_stream,
                          GInputStream          *buffered_input_stream,
                          GOutputStream         *output_stream,
                          GCancellable          *cancellable,
                          SendAndSpliceCallback  callback,
                          GSimpleAsyncResult    *res)
{
	SendAndSpliceData *data;

	data = g_slice_new0 (SendAndSpliceData);
	data->unix_input_stream = unix_input_stream;
	data->buffered_input_stream = buffered_input_stream;
	data->output_stream = output_stream;

	if (cancellable) {
		data->cancellable = g_object_ref (cancellable);
	}

	data->callback = callback;
	data->res = g_object_ref (res);

	return data;
}

static void
send_and_splice_data_free (SendAndSpliceData *data)
{
	g_object_unref (data->unix_input_stream);
	g_object_unref (data->buffered_input_stream);
	g_object_unref (data->output_stream);

	if (data->cancellable) {
		g_object_unref (data->cancellable);
	}

	if (data->error) {
		g_error_free (data->error);
	}

	if (data->res) {
		g_object_unref (data->res);
	}

	g_slice_free (SendAndSpliceData, data);
}

static void
dbus_send_and_splice_async_finish (SendAndSpliceData *data)
{
	if (!data->error) {
		(* data->callback) (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (data->output_stream)),
		                    g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (data->output_stream)),
		                    NULL,
		                    data->res);
	} else {
		(* data->callback) (NULL, -1, data->error, data->res);
	}

	send_and_splice_data_free (data);
}

static void
send_and_splice_splice_callback (GObject      *source,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
	SendAndSpliceData *data = user_data;
	GError *error = NULL;

	g_output_stream_splice_finish (G_OUTPUT_STREAM (source), result, &error);

	if (error) {
		if (!data->error) {
			data->error = error;
		} else {
			g_error_free (error);
		}
	}

	data->splice_finished = TRUE;

	if (data->dbus_finished) {
		dbus_send_and_splice_async_finish (data);
	}
}

static void
send_and_splice_dbus_callback (GObject      *source,
                               GAsyncResult *result,
                               gpointer      user_data)
{
	SendAndSpliceData *data = user_data;
	GDBusMessage *reply;
	GError *error = NULL;

	reply = g_dbus_connection_send_message_with_reply_finish (G_DBUS_CONNECTION (source),
	                                                          result, &error);

	if (reply) {
		if (g_dbus_message_get_message_type (reply) == G_DBUS_MESSAGE_TYPE_ERROR) {
			g_dbus_message_to_gerror (reply, &error);
		}

		g_object_unref (reply);
	}

	if (error) {
		if (!data->error) {
			data->error = error;
		} else {
			g_error_free (error);
		}
	}

	data->dbus_finished = TRUE;

	if (data->splice_finished) {
		dbus_send_and_splice_async_finish (data);
	}
}

static void
dbus_send_and_splice_async (GDBusConnection       *connection,
                            GDBusMessage          *message,
                            int                    fd,
                            GCancellable          *cancellable,
                            SendAndSpliceCallback  callback,
                            GSimpleAsyncResult    *res)
{
	SendAndSpliceData *data;
	GInputStream *unix_input_stream;
	GInputStream *buffered_input_stream;
	GOutputStream *output_stream;

	unix_input_stream = g_unix_input_stream_new (fd, TRUE);
	buffered_input_stream = g_buffered_input_stream_new_sized (unix_input_stream,
	                                                           DBUS_PIPE_BUFFER_SIZE);
	output_stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

	data = send_and_splice_data_new (unix_input_stream,
	                                 buffered_input_stream,
	                                 output_stream,
	                                 cancellable,
	                                 callback,
	                                 res);

	g_dbus_connection_send_message_with_reply (connection,
	                                           message,
	                                           G_DBUS_SEND_MESSAGE_FLAGS_NONE,
	                                           -1,
	                                           NULL,
	                                           cancellable,
	                                           send_and_splice_dbus_callback,
	                                           data);

	g_output_stream_splice_async (data->output_stream,
	                              data->buffered_input_stream,
	                              G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
	                              G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
	                              0,
	                              cancellable,
	                              send_and_splice_splice_callback,
	                              data);
}

static void
get_metadata_fast_cb (void     *buffer,
                      gssize    buffer_size,
                      GError   *error,
                      gpointer  user_data)
{
	GSimpleAsyncResult *res;

	res = user_data;

	if (G_UNLIKELY (error)) {
		g_simple_async_result_set_from_error (res, error);
	} else {
		const gchar *preupdate, *sparql, *where, *end;
		TrackerExtractInfo *info;
		gsize len;

		preupdate = sparql = where = NULL;
		end = (gchar *) buffer + buffer_size;

		if (buffer) {
			preupdate = buffer;
			len = strlen (preupdate);

			if (preupdate + len < end) {
				buffer_size -= len;
				sparql = preupdate + len + 1;
				len = strlen (sparql);

				if (sparql + len < end) {
					where = sparql + len + 1;
				}
			}
		}

		info = tracker_extract_info_new (preupdate, sparql, where);
		g_simple_async_result_set_op_res_gpointer (res, info,
		                                           (GDestroyNotify) tracker_extract_info_free);
	}

	g_simple_async_result_complete_in_idle (res);
}

static void
get_metadata_fast_async (GDBusConnection    *connection,
                         const gchar        *uri,
                         const gchar        *mime_type,
                         GCancellable       *cancellable,
                         GSimpleAsyncResult *res)
{
	GDBusMessage *message;
	GUnixFDList *fd_list;
	int pipefd[2];

	if (pipe (pipefd) < 0) {
		g_critical ("Coudln't open pipe");
		/* FIXME: Report async error */
		return;
	}

	message = g_dbus_message_new_method_call (DBUS_SERVICE_EXTRACT,
	                                          DBUS_PATH_EXTRACT,
	                                          DBUS_INTERFACE_EXTRACT,
	                                          "GetMetadataFast");

	fd_list = g_unix_fd_list_new ();

	g_dbus_message_set_body (message,
	                         g_variant_new ("(ssh)",
	                                        uri,
	                                        mime_type,
	                                        g_unix_fd_list_append (fd_list,
	                                                               pipefd[1],
	                                                               NULL)));
	g_dbus_message_set_unix_fd_list (message, fd_list);

	/* We need to close the fd as g_unix_fd_list_append duplicates the fd */

	close (pipefd[1]);

	g_object_unref (fd_list);

	dbus_send_and_splice_async (connection,
	                            message,
	                            pipefd[0],
	                            cancellable,
	                            get_metadata_fast_cb,
	                            res);
	g_object_unref (message);
}

void
tracker_extract_client_get_metadata (GFile               *file,
                                     const gchar         *mime_type,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
	GSimpleAsyncResult *res;
	GError *error = NULL;
	gchar *uri;

	g_return_if_fail (G_IS_FILE (file));
	g_return_if_fail (mime_type != NULL);
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (callback != NULL);

	if (G_UNLIKELY (!connection)) {
		connection = g_bus_get_sync (G_BUS_TYPE_SESSION, cancellable, &error);

		if (error) {
			g_simple_async_report_gerror_in_idle (G_OBJECT (file), callback, user_data, error);
			g_error_free (error);
			return;
		}
	}

	uri = g_file_get_uri (file);

	res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, NULL);
	g_simple_async_result_set_handle_cancellation (res, TRUE);

	get_metadata_fast_async (connection, uri, mime_type, cancellable, res);
}

TrackerExtractInfo *
tracker_extract_client_get_metadata_finish (GFile         *file,
                                            GAsyncResult  *res,
                                            GError       **error)
{
	g_return_val_if_fail (G_IS_FILE (file), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error)) {
		return NULL;
	}

	return g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));
}

G_CONST_RETURN gchar *
tracker_extract_info_get_preupdate (TrackerExtractInfo *info)
{
	return info->preupdate;
}

G_CONST_RETURN gchar *
tracker_extract_info_get_update (TrackerExtractInfo *info)
{
	return info->update;
}

G_CONST_RETURN gchar *
tracker_extract_info_get_where_clause (TrackerExtractInfo *info)
{
	return info->where;
}

void
tracker_extract_client_cancel_for_prefix (GFile *prefix)
{
	GDBusMessage *message;
	gchar *uris[2];

	uris[0] = g_file_get_uri (prefix);
	uris[1] = NULL;

	message = g_dbus_message_new_method_call (DBUS_SERVICE_EXTRACT,
	                                          DBUS_PATH_EXTRACT,
	                                          DBUS_INTERFACE_EXTRACT,
	                                          "CancelTasks");

	g_dbus_message_set_body (message, g_variant_new ("(^as)", uris));
	g_dbus_connection_send_message (connection, message,
	                                G_DBUS_SEND_MESSAGE_FLAGS_NONE,
	                                NULL, NULL);

	g_free (uris[0]);
}
