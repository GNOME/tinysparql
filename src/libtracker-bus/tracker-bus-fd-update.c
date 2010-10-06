/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008-2010, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2010, Codeminded BVBA <philip@codeminded.be>
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
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib-object.h>
#include <gio/gunixoutputstream.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-bus-fd-update.h"
#include "tracker-bus.h"
#include "tracker-bus-shared.h"

typedef enum {
	FAST_UPDATE,
	FAST_UPDATE_BLANK,
	FAST_UPDATE_BATCH
} FastOperationType;

typedef struct {
	DBusConnection *connection;
	FastOperationType operation_type;
	GCancellable *cancellable;
	DBusPendingCall *dbus_call;
	GSimpleAsyncResult *res;
	gpointer user_data;
	gulong cancelid;
} FastAsyncData;


static void
fast_async_data_free (gpointer data)
{
	FastAsyncData *fad = data;

	if (fad) {
		if (fad->cancellable) {
			if (fad->cancelid != 0)
				g_cancellable_disconnect (fad->cancellable, fad->cancelid);
			g_object_unref (fad->cancellable);
		}

		if (fad->connection) {
			dbus_connection_unref (fad->connection);
		}

		if (fad->res) {
			/* Don't free, weak */
		}

		g_slice_free (FastAsyncData, fad);
	}
}

static gboolean
on_cancel_idle (gpointer data)
{
	FastAsyncData *fad = data;
	GError *error = NULL;

	g_set_error_literal (&error,
	                     G_IO_ERROR,
	                     G_IO_ERROR_CANCELLED,
	                     "Operation was cancelled");

	g_simple_async_result_set_from_error (fad->res, error);

	g_simple_async_result_complete (fad->res);

	g_error_free (error);
	fast_async_data_free (fad);

	return FALSE;
}

static void
on_cancel (GCancellable *cancellable,
           FastAsyncData *fad)
{
	if (fad->dbus_call) {
		dbus_pending_call_cancel (fad->dbus_call);
		dbus_pending_call_unref (fad->dbus_call);
		fad->dbus_call = NULL;
	}

	g_idle_add (on_cancel_idle, fad);
}

static FastAsyncData *
fast_async_data_new (DBusConnection    *connection,
                     FastOperationType  operation_type,
                     GCancellable      *cancellable,
                     gpointer           user_data)
{
	FastAsyncData *data;

	data = g_slice_new0 (FastAsyncData);

	data->connection = dbus_connection_ref (connection);
	data->operation_type = operation_type;
	if (cancellable) {
		data->cancellable = g_object_ref (cancellable);

		data->cancelid = g_cancellable_connect (cancellable, G_CALLBACK (on_cancel), data, NULL);
	}
	data->user_data = user_data;

	return data;
}

static GError *
sparql_error_from_dbus_message (DBusMessage *reply)
{
	DBusError dbus_error;
	GError *error;

	dbus_error_init (&dbus_error);
	dbus_set_error_from_message (&dbus_error, reply);

	error = g_error_new_literal (TRACKER_SPARQL_ERROR, 0, dbus_error.message);

	dbus_error_free (&dbus_error);

	return error;
}

static void
sparql_update_fast_callback (DBusPendingCall *call,
                             void            *user_data)
{
	FastAsyncData *fad = user_data;
	DBusMessage *reply;
	GError *error = NULL;
	GVariant *result;

	/* Check for errors */
	reply = dbus_pending_call_steal_reply (call);

	if (dbus_message_get_type (reply) == DBUS_MESSAGE_TYPE_ERROR) {
		error = sparql_error_from_dbus_message (reply);

		g_simple_async_result_set_from_error (fad->res, error);

		dbus_message_unref (reply);

		g_simple_async_result_complete (fad->res);

		fast_async_data_free (fad);

		dbus_pending_call_unref (call);

		return;
	}

	/* Call iterator callback */
	switch (fad->operation_type) {
	case FAST_UPDATE:
	case FAST_UPDATE_BATCH:
		g_simple_async_result_complete (fad->res);
		break;
	case FAST_UPDATE_BLANK:
		result = tracker_bus_message_to_variant (reply);
		g_simple_async_result_set_op_res_gpointer (fad->res, result, NULL);
		g_simple_async_result_complete (fad->res);
		g_variant_unref (result);

		break;
	default:
		g_assert_not_reached ();
		break;
	}

	/* Clean up */
	dbus_message_unref (reply);

	fast_async_data_free (fad);

	dbus_pending_call_unref (call);
}

static void
sparql_update_array_fast_callback (DBusPendingCall *call,
                                   void            *user_data)
{
	FastAsyncData *fad = user_data;
	DBusMessage *reply;
	GError *error = NULL;
	GPtrArray *errors;

	/* Check for errors */
	reply = dbus_pending_call_steal_reply (call);

	if (dbus_message_get_type (reply) == DBUS_MESSAGE_TYPE_ERROR) {
		error = sparql_error_from_dbus_message (reply);

		g_simple_async_result_set_from_error (fad->res, error);

		dbus_message_unref (reply);

		g_simple_async_result_complete (fad->res);

		fast_async_data_free (fad);

		dbus_pending_call_unref (call);

		return;
	}

	/* Call iterator callback */
	switch (fad->operation_type) {
	case FAST_UPDATE:
	case FAST_UPDATE_BATCH:
		// todo: read errors into errors

		g_simple_async_result_set_op_res_gpointer (fad->res, errors, NULL);
		g_simple_async_result_complete (fad->res);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	/* Clean up */
	dbus_message_unref (reply);

	fast_async_data_free (fad);

	dbus_pending_call_unref (call);
}

static DBusPendingCall *
sparql_update_fast_send (DBusConnection     *connection,
                         const gchar        *query,
                         FastOperationType   type,
                         GError            **error)
{
	const gchar *dbus_method;
	DBusMessage *message;
	DBusMessageIter iter;
	DBusPendingCall *call;
	int pipefd[2];
	GOutputStream *output_stream;
	GOutputStream *buffered_output_stream;
	GDataOutputStream *data_output_stream;
	GError *inner_error = NULL;

	g_return_val_if_fail (query != NULL, NULL);

	if (pipe (pipefd) < 0) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_UNSUPPORTED,
		             "Cannot open pipe");
		return NULL;
	}

	switch (type) {
	case FAST_UPDATE:
		dbus_method = "Update";
		break;
	case FAST_UPDATE_BLANK:
		dbus_method = "UpdateBlank";
		break;
	case FAST_UPDATE_BATCH:
		dbus_method = "BatchUpdate";
		break;
	default:
		g_assert_not_reached ();
	}

	message = dbus_message_new_method_call (TRACKER_DBUS_SERVICE,
	                                        TRACKER_DBUS_OBJECT_STEROIDS,
	                                        TRACKER_DBUS_INTERFACE_STEROIDS,
	                                        dbus_method);
	dbus_message_iter_init_append (message, &iter);
	dbus_message_iter_append_basic (&iter, DBUS_TYPE_UNIX_FD, &pipefd[0]);
	dbus_connection_send_with_reply (connection, message, &call, -1);
	dbus_message_unref (message);
	close (pipefd[0]);

	if (!call) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_UNSUPPORTED,
		             "FD passing unsupported or connection disconnected");
		return NULL;
	}

	output_stream = g_unix_output_stream_new (pipefd[1], TRUE);
	buffered_output_stream = g_buffered_output_stream_new_sized (output_stream,
	                                                             TRACKER_DBUS_PIPE_BUFFER_SIZE);
	data_output_stream = g_data_output_stream_new (buffered_output_stream);

	g_data_output_stream_put_int32 (data_output_stream, strlen (query),
	                                NULL, &inner_error);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		g_object_unref (data_output_stream);
		g_object_unref (buffered_output_stream);
		g_object_unref (output_stream);
		return NULL;
	}

	g_data_output_stream_put_string (data_output_stream,
	                                 query,
	                                 NULL,
	                                 &inner_error);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		g_object_unref (data_output_stream);
		g_object_unref (buffered_output_stream);
		g_object_unref (output_stream);
		return NULL;
	}

	g_object_unref (data_output_stream);
	g_object_unref (buffered_output_stream);
	g_object_unref (output_stream);

	return call;
}


static DBusPendingCall *
sparql_update_array_fast_send (DBusConnection     *connection,
                               const gchar       **queries,
                               guint               queries_len,
                               FastOperationType   type,
                               GError            **error)
{
	const gchar *dbus_method;
	DBusMessage *message;
	DBusMessageIter iter;
	DBusPendingCall *call;
	int pipefd[2], i;
	GOutputStream *output_stream;
	GOutputStream *buffered_output_stream;
	GDataOutputStream *data_output_stream;
	GError *inner_error = NULL;

	g_return_val_if_fail (queries != NULL, NULL);
	g_return_val_if_fail (queries_len != 0, NULL);

	if (pipe (pipefd) < 0) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_UNSUPPORTED,
		             "Cannot open pipe");
		return NULL;
	}

	switch (type) {
	case FAST_UPDATE:
		dbus_method = "Update";
		break;
	case FAST_UPDATE_BATCH:
		dbus_method = "BatchUpdate";
		break;
	default:
		g_assert_not_reached ();
	}

	message = dbus_message_new_method_call (TRACKER_DBUS_SERVICE,
	                                        TRACKER_DBUS_OBJECT_STEROIDS,
	                                        TRACKER_DBUS_INTERFACE_STEROIDS,
	                                        dbus_method);
	dbus_message_iter_init_append (message, &iter);
	dbus_message_iter_append_basic (&iter, DBUS_TYPE_UNIX_FD, &pipefd[0]);
	dbus_connection_send_with_reply (connection, message, &call, -1);
	dbus_message_unref (message);
	close (pipefd[0]);

	if (!call) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_UNSUPPORTED,
		             "FD passing unsupported or connection disconnected");
		return NULL;
	}

	output_stream = g_unix_output_stream_new (pipefd[1], TRUE);
	buffered_output_stream = g_buffered_output_stream_new_sized (output_stream,
	                                                             TRACKER_DBUS_PIPE_BUFFER_SIZE);
	data_output_stream = g_data_output_stream_new (buffered_output_stream);

	g_data_output_stream_put_uint32 (data_output_stream,
	                                 queries_len,
	                                 NULL,
	                                 &inner_error);

	for (i = 0; i < queries_len; i++) {
		const gchar *query = queries[i];

		g_data_output_stream_put_int32 (data_output_stream, strlen (query),
		                                NULL, &inner_error);

		if (inner_error) {
			g_propagate_error (error, inner_error);
			g_object_unref (data_output_stream);
			g_object_unref (buffered_output_stream);
			g_object_unref (output_stream);
			return NULL;
		}

		g_data_output_stream_put_string (data_output_stream,
		                                 query,
		                                 NULL,
		                                 &inner_error);

		if (inner_error) {
			g_propagate_error (error, inner_error);
			g_object_unref (data_output_stream);
			g_object_unref (buffered_output_stream);
			g_object_unref (output_stream);
			return NULL;
		}
	}

	g_object_unref (data_output_stream);
	g_object_unref (buffered_output_stream);
	g_object_unref (output_stream);

	return call;
}

static DBusMessage *
sparql_update_fast (DBusConnection     *connection,
                    const gchar        *query,
                    FastOperationType   type,
                    GError            **error)
{
	DBusPendingCall *call;
	DBusMessage *reply;

	call = sparql_update_fast_send (connection, query, type, error);
	if (!call) {
		return NULL;
	}

	dbus_pending_call_block (call);

	reply = dbus_pending_call_steal_reply (call);

	if (dbus_message_get_type (reply) == DBUS_MESSAGE_TYPE_ERROR) {
		g_propagate_error (error, sparql_error_from_dbus_message (reply));

		return NULL;
	}

	dbus_pending_call_unref (call);

	return reply;
}

static void
sparql_update_fast_async (DBusConnection      *connection,
                          const gchar         *query,
                          FastAsyncData       *fad,
                          GError             **error)
{
	DBusPendingCall *call;

	call = sparql_update_fast_send (connection, query, fad->operation_type, error);
	if (!call) {
		/* Do some clean up ?*/
		return;
	}

	fad->dbus_call = call;

	dbus_pending_call_set_notify (call, sparql_update_fast_callback, fad, NULL);
}

static void
sparql_update_array_fast_async (DBusConnection      *connection,
                                const gchar        **queries,
                                guint                queries_len,
                                FastAsyncData       *fad,
                                GError             **error)
{
	DBusPendingCall *call;

	call = sparql_update_array_fast_send (connection, queries, queries_len, fad->operation_type, error);
	if (!call) {
		/* Do some clean up ?*/
		return;
	}

	fad->dbus_call = call;

	dbus_pending_call_set_notify (call, sparql_update_array_fast_callback, fad, NULL);
}

/* Public API */

void
tracker_bus_fd_sparql_update (DBusGConnection *connection,
                              const char      *query,
                              GError         **error)
{
	DBusMessage *reply;

	g_return_if_fail (query != NULL);

	reply = sparql_update_fast (dbus_g_connection_get_connection (connection),
	                            query, FAST_UPDATE, error);

	if (!reply) {
		return;
	}

	dbus_message_unref (reply);
}

void
tracker_bus_fd_sparql_update_async (DBusGConnection       *connection,
                                    const char            *query,
                                    GCancellable          *cancellable,
                                    GAsyncReadyCallback    callback,
                                    gpointer               user_data)
{
	FastAsyncData *fad;
	GError *error = NULL;

	g_return_if_fail (query != NULL);

	fad = fast_async_data_new (dbus_g_connection_get_connection (connection),
	                           FAST_UPDATE, cancellable, user_data);

	fad->res = g_simple_async_result_new (NULL, callback, user_data,
	                                      tracker_bus_fd_sparql_update_async);

	sparql_update_fast_async (dbus_g_connection_get_connection (connection),
	                          query, fad, &error);

	if (error) {
		g_critical ("Could not initiate update: %s", error->message);
		g_error_free (error);
		g_object_unref (fad->res);
		fast_async_data_free (fad);
	}
}

void
tracker_bus_fd_sparql_update_array_async (DBusGConnection       *connection,
                                          const char            **queries,
                                          guint                   queries_len,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data)
{
	FastAsyncData *fad;
	GError *error = NULL;

	g_return_if_fail (queries != NULL);
	g_return_if_fail (queries_len != 0);

	fad = fast_async_data_new (dbus_g_connection_get_connection (connection),
	                           FAST_UPDATE, cancellable, user_data);

	fad->res = g_simple_async_result_new (NULL, callback, user_data,
	                                      tracker_bus_fd_sparql_update_async);

	sparql_update_array_fast_async (dbus_g_connection_get_connection (connection),
	                                queries, queries_len, fad, &error);

	if (error) {
		g_critical ("Could not initiate update: %s", error->message);
		g_error_free (error);
		g_object_unref (fad->res);
		fast_async_data_free (fad);
	}
}

void
tracker_bus_fd_sparql_update_finish (GAsyncResult     *res,
                                     GError          **error)
{
	g_return_if_fail (res != NULL);

	g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}


GVariant *
tracker_bus_fd_sparql_update_blank_finish (GAsyncResult     *res,
                                           GError          **error)
{
	g_return_val_if_fail (res != NULL, NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error)) {
		return NULL;
	}

	return g_variant_ref (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
}

GVariant *
tracker_bus_fd_sparql_update_blank (DBusGConnection *connection,
                                    const gchar     *query,
                                    GError         **error)
{
	DBusMessage *reply;
	GVariant *result;

	g_return_val_if_fail (query != NULL, NULL);

	reply = sparql_update_fast (dbus_g_connection_get_connection (connection),
	                            query, FAST_UPDATE_BLANK, error);

	if (!reply) {
		return NULL;
	}

	if (g_strcmp0 (dbus_message_get_signature (reply), "aaa{ss}")) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_UNSUPPORTED,
		             "Server returned invalid results");
		dbus_message_unref (reply);
		return NULL;
	}

	result = tracker_bus_message_to_variant (reply);
	dbus_message_unref (reply);

	return result;
}

void
tracker_bus_fd_sparql_update_blank_async (DBusGConnection       *connection,
                                          const gchar           *query,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data)
{
	FastAsyncData *fad;
	GError *error = NULL;

	g_return_if_fail (query != NULL);
	g_return_if_fail (callback != NULL);

	fad = fast_async_data_new (dbus_g_connection_get_connection (connection),
	                           FAST_UPDATE_BLANK,
	                           cancellable,
	                           user_data);

	fad->res = g_simple_async_result_new (NULL, callback, user_data,
	                                      tracker_bus_fd_sparql_update_blank_async);

	sparql_update_fast_async (dbus_g_connection_get_connection (connection),
	                          query, fad, &error);

	if (error) {
		g_critical ("Could not initiate update: %s", error->message);
		g_error_free (error);
		g_object_unref (fad->res);
		fast_async_data_free (fad);
	}
}

void
tracker_bus_fd_sparql_batch_update (DBusGConnection *connection,
                                    const char      *query,
                                    GError         **error)
{
	DBusMessage *reply;

	g_return_if_fail (query != NULL);

	reply = sparql_update_fast (dbus_g_connection_get_connection (connection),
	                            query, FAST_UPDATE_BATCH, error);

	if (!reply) {
		return;
	}

	dbus_message_unref (reply);
}

void
tracker_bus_fd_sparql_batch_update_async (DBusGConnection       *connection,
                                          const char            *query,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data)
{
	FastAsyncData *fad;
	GError *error = NULL;

	g_return_if_fail (query != NULL);

	fad = fast_async_data_new (dbus_g_connection_get_connection (connection),
	                           FAST_UPDATE_BATCH, cancellable, user_data);

	fad->res = g_simple_async_result_new (NULL, callback, user_data,
	                                      tracker_bus_fd_sparql_batch_update_async);

	sparql_update_fast_async (dbus_g_connection_get_connection (connection),
	                          query, fad, &error);

	if (error) {
		g_critical ("Could not initiate update: %s", error->message);
		g_error_free (error);
		g_object_unref (fad->res);
		fast_async_data_free (fad);
	}
}


void
tracker_bus_fd_sparql_batch_update_array_async (DBusGConnection        *connection,
                                                const char            **queries,
                                                guint                   queries_len,
                                                GCancellable           *cancellable,
                                                GAsyncReadyCallback     callback,
                                                gpointer                 user_data)
{
	FastAsyncData *fad;
	GError *error = NULL;

	g_return_if_fail (queries != NULL);
	g_return_if_fail (queries_len != 0);

	fad = fast_async_data_new (dbus_g_connection_get_connection (connection),
	                           FAST_UPDATE_BATCH, cancellable, user_data);

	fad->res = g_simple_async_result_new (NULL, callback, user_data,
	                                      tracker_bus_fd_sparql_batch_update_async);

	sparql_update_array_fast_async (dbus_g_connection_get_connection (connection),
	                                queries, queries_len, fad, &error);

	if (error) {
		g_critical ("Could not initiate update: %s", error->message);
		g_error_free (error);
		g_object_unref (fad->res);
		fast_async_data_free (fad);
	}
}

void
tracker_bus_fd_sparql_batch_update_finish (GAsyncResult     *res,
                                           GError          **error)
{
	g_return_if_fail (res != NULL);

	g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

GPtrArray*
tracker_bus_fd_sparql_update_array_finish (GAsyncResult *res)
{
	g_return_val_if_fail (res != NULL, NULL);

	// todo: check if ref is needed here 
	return g_ptr_array_ref (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
}

GPtrArray*
tracker_bus_fd_sparql_batch_update_array_finish (GAsyncResult *res)
{
	g_return_val_if_fail (res != NULL, NULL);

	// todo: check if ref is needed here 
	return g_ptr_array_ref (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
}
