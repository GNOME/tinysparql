/*
 * Copyright (C) 2019, Red Hat, Inc
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

/**
 * TrackerEndpointDBus:
 *
 * `TrackerEndpointDBus` makes the RDF data in a [class@SparqlConnection]
 * accessible to other processes via DBus.
 *
 * This object is a [class@Endpoint] subclass that exports
 * a [class@SparqlConnection] so its RDF data is accessible to other
 * processes through the given [class@Gio.DBusConnection].
 *
 * ```c
 * // This process already has org.example.Endpoint bus name
 * endpoint = tracker_endpoint_dbus_new (sparql_connection,
 *                                       dbus_connection,
 *                                       NULL,
 *                                       NULL,
 *                                       &error);
 *
 * // From another process
 * connection = tracker_sparql_connection_bus_new ("org.example.Endpoint",
 *                                                 NULL,
 *                                                 dbus_connection,
 *                                                 &error);
 * ```
 *
 * The `TrackerEndpointDBus` will manage a DBus object at the given path
 * with the `org.freedesktop.Tracker3.Endpoint` interface, if no path is
 * given the object will be at the default `/org/freedesktop/Tracker3/Endpoint`
 * location.
 *
 * Access to D-Bus endpoints may be managed via the
 * [signal@EndpointDBus::block-call] signal, the boolean
 * return value expressing whether the request is blocked or not.
 * Inspection of the requester address is left up to the user. The
 * default value allows all requests independently of their provenance.
 *
 * However, moderating access to D-Bus interfaces is typically not necessary
 * in user code, as access to public D-Bus endpoints will be transparently
 * managed through the Tracker portal service for applications sandboxed
 * via XDG portals. These already have access to D-Bus SPARQL endpoints and
 * their data naturally filtered as defined in the application manifest.
 *
 * A `TrackerEndpointDBus` may be created on a different thread/main
 * context from the one that created [class@SparqlConnection].
 */

#include "config.h"

#include "bus/tracker-bus.h"

#include "tracker-endpoint-dbus.h"
#include "tracker-notifier.h"
#include "tracker-notifier-private.h"
#include "tracker-private.h"

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <gio/gunixfdlist.h>
#include <glib-unix.h>

static const gchar introspection_xml[] =
	"<node>"
	"  <interface name='org.freedesktop.Tracker3.Endpoint'>"
	"    <method name='Query'>"
	"      <arg type='s' name='query' direction='in' />"
	"      <arg type='h' name='output_stream' direction='in' />"
	"      <arg type='a{sv}' name='arguments' direction='in' />"
	"      <arg type='as' name='result' direction='out' />"
	"    </method>"
	"    <method name='Serialize'>"
	"      <arg type='s' name='query' direction='in' />"
	"      <arg type='h' name='output_stream' direction='in' />"
	"      <arg type='i' name='flags' direction='in' />"
	"      <arg type='i' name='format' direction='in' />"
	"      <arg type='a{sv}' name='arguments' direction='in' />"
	"    </method>"
	"    <method name='Update'>"
	"      <arg type='h' name='input_stream' direction='in' />"
	"    </method>"
	"    <method name='UpdateArray'>"
	"      <arg type='h' name='input_stream' direction='in' />"
	"    </method>"
	"    <method name='UpdateBlank'>"
	"      <arg type='h' name='input_stream' direction='in' />"
	"      <arg type='aaa{ss}' name='result' direction='out' />"
	"    </method>"
	"    <method name='Deserialize'>"
	"      <arg type='h' name='input_stream' direction='in' />"
	"      <arg type='i' name='flags' direction='in' />"
	"      <arg type='i' name='format' direction='in' />"
	"      <arg type='s' name='default_graph' direction='in' />"
	"      <arg type='a{sv}' name='arguments' direction='in' />"
	"    </method>"
	"    <signal name='GraphUpdated'>"
	"      <arg type='sa{ii}' name='updates' />"
	"    </signal>"
	"  </interface>"
	"</node>";

enum {
	BLOCK_CALL,
	N_SIGNALS
};

enum {
	PROP_0,
	PROP_DBUS_CONNECTION,
	PROP_OBJECT_PATH,
	N_PROPS
};

typedef struct {
	TrackerEndpointDBus *endpoint;
	GDBusMethodInvocation *invocation;
	GDataOutputStream *data_stream;
	GCancellable *global_cancellable;
	GCancellable *cancellable;
	gulong cancellable_id;
	GSource *source;
} QueryRequest;

typedef struct {
	TrackerEndpointDBus *endpoint;
	GDBusMethodInvocation *invocation;
	GDataInputStream *input_stream;
	gchar *query;
} UpdateRequest;

static GParamSpec *props[N_PROPS] = { 0, };

static guint signals[N_SIGNALS] = { 0, };

static void tracker_endpoint_dbus_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (TrackerEndpointDBus, tracker_endpoint_dbus, TRACKER_TYPE_ENDPOINT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, tracker_endpoint_dbus_initable_iface_init))

static gboolean
tracker_endpoint_dbus_block_call (TrackerEndpointDBus   *endpoint_dbus,
                                  GDBusMethodInvocation *invocation)
{
	gboolean block;

	g_signal_emit (endpoint_dbus, signals[BLOCK_CALL], 0,
	               g_dbus_method_invocation_get_sender (invocation),
	               &block);

	return block;
}

static gboolean
fd_watch_cb (gint          fd,
             GIOCondition  condition,
             gpointer      user_data)
{
	QueryRequest *request = user_data;

	if ((condition & (G_IO_ERR | G_IO_HUP)) != 0) {
		g_cancellable_cancel (request->cancellable);
		return G_SOURCE_REMOVE;
	}

	return G_SOURCE_CONTINUE;
}

static void
propagate_cancelled (GCancellable *cancellable,
                     GCancellable *connected_cancellable)
{
	g_cancellable_cancel (connected_cancellable);
}

static QueryRequest *
query_request_new (TrackerEndpointDBus   *endpoint,
                   GDBusMethodInvocation *invocation,
                   int                    fd)
{
	GOutputStream *stream, *buffered_stream;
	QueryRequest *request;

	request = g_new0 (QueryRequest, 1);
	request->invocation = g_object_ref (invocation);
	request->endpoint = endpoint;
	request->global_cancellable = g_object_ref (endpoint->cancellable);
	request->cancellable = g_cancellable_new ();
	request->cancellable_id =
		g_cancellable_connect (request->global_cancellable,
		                       G_CALLBACK (propagate_cancelled),
		                       g_object_ref (request->cancellable),
		                       g_object_unref);

	request->source = g_unix_fd_source_new (fd, G_IO_ERR | G_IO_HUP);

	g_source_set_callback (request->source,
	                       G_SOURCE_FUNC (fd_watch_cb),
	                       request,
	                       NULL);
	g_source_attach (request->source, g_main_context_get_thread_default ());

	stream = g_unix_output_stream_new (fd, TRUE);
	buffered_stream = g_buffered_output_stream_new_sized (stream,
	                                                      sysconf (_SC_PAGE_SIZE));

	request->data_stream = g_data_output_stream_new (buffered_stream);
	g_data_output_stream_set_byte_order (request->data_stream,
	                                     G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN);

	g_object_unref (buffered_stream);
	g_object_unref (stream);

	return request;
}

static void
query_request_free (QueryRequest *request)
{
	g_cancellable_disconnect (request->global_cancellable,
	                          request->cancellable_id);
	g_object_unref (request->global_cancellable);

	g_source_destroy (request->source);
	g_source_unref (request->source);
	g_object_unref (request->cancellable);

	g_output_stream_close_async (G_OUTPUT_STREAM (request->data_stream),
				     G_PRIORITY_DEFAULT,
				     NULL, NULL, NULL);

	g_object_unref (request->invocation);
	g_object_unref (request->data_stream);
	g_free (request);
}

static UpdateRequest *
update_request_new (TrackerEndpointDBus   *endpoint,
                    GDBusMethodInvocation *invocation,
                    int                    fd)
{
	UpdateRequest *request;
	GInputStream *stream;

	request = g_new0 (UpdateRequest, 1);
	request->invocation = g_object_ref (invocation);
	request->endpoint = endpoint;

	stream = g_unix_input_stream_new (fd, TRUE);
	request->input_stream = g_data_input_stream_new (stream);
	g_buffered_input_stream_set_buffer_size (G_BUFFERED_INPUT_STREAM (request->input_stream),
	                                         sysconf (_SC_PAGE_SIZE));
	g_data_input_stream_set_byte_order (request->input_stream,
	                                    G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN);
	g_object_unref (stream);

	return request;
}

static gchar *
read_string (GDataInputStream  *istream,
             gsize             *len_out,
             GCancellable      *cancellable,
             GError           **error)
{
	gchar *buf;
	guint32 len;

	len = g_data_input_stream_read_int32 (istream, NULL, error);
	if (len == 0)
		return NULL;

	buf = g_new0 (gchar, len + 1);

	if (!g_input_stream_read_all (G_INPUT_STREAM (istream),
	                              buf,
	                              len,
	                              NULL,
	                              cancellable,
	                              error)) {
		g_free (buf);
		return NULL;
	}

	if (len_out)
		*len_out = len;

	return buf;
}

static gchar *
read_query (GDataInputStream     *istream,
            TrackerEndpointDBus  *endpoint,
            GCancellable         *cancellable,
            GError              **error)
{
	gchar *sparql;

	sparql = read_string (istream, NULL, NULL, error);
	if (!sparql)
		return NULL;

	tracker_endpoint_rewrite_query (TRACKER_ENDPOINT (endpoint), &sparql);

	return sparql;
}

static void
handle_read_update (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
	UpdateRequest *request = task_data;
	TrackerBusOpType op_type;
	GError *error = NULL;

	op_type = g_data_input_stream_read_uint32 (request->input_stream, NULL, &error);
	if (error)
		goto end;

	g_assert (op_type == TRACKER_BUS_OP_SPARQL);
	request->query = read_query (request->input_stream, request->endpoint,
	                             cancellable, &error);
	if (error)
		goto end;

	g_assert (g_data_input_stream_read_uint32 (request->input_stream, NULL, &error) == 0);

 end:
	if (error)
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, TRUE);
}

static void
update_request_free (UpdateRequest *request)
{
	g_input_stream_close_async (G_INPUT_STREAM (request->input_stream),
	                            G_PRIORITY_DEFAULT,
	                            NULL, NULL, NULL);

	g_object_unref (request->invocation);
	g_object_unref (request->input_stream);
	g_free (request->query);
	g_free (request);
}

static gboolean
write_cursor (QueryRequest          *request,
              TrackerSparqlCursor   *cursor,
              GError               **error)
{
	const gchar **values = NULL, **langtags = NULL;
	glong *offsets = NULL;
	gint i, n_columns = 0;
	GError *inner_error = NULL;

	n_columns = tracker_sparql_cursor_get_n_columns (cursor);
	values = g_new0 (const char *, n_columns);
	langtags = g_new0 (const gchar *, n_columns);
	offsets = g_new0 (glong, n_columns);

	while (tracker_sparql_cursor_next (cursor, request->cancellable, &inner_error)) {
		glong cur_offset = -1;

		if (g_cancellable_set_error_if_cancelled (request->cancellable, &inner_error))
			goto out;

		if (!g_data_output_stream_put_int32 (request->data_stream, n_columns,
		                                     NULL,
		                                     &inner_error))
			goto out;

		for (i = 0; i < n_columns; i++) {
			glong len;

			if (!g_data_output_stream_put_int32 (request->data_stream,
			                                     tracker_sparql_cursor_get_value_type (cursor, i),
			                                     NULL,
			                                     &inner_error))
				goto out;

			values[i] = tracker_sparql_cursor_get_langstring (cursor, i, &langtags[i], &len);

			if (langtags[i])
				len += strlen (langtags[i]) + 1;
			len++;
			cur_offset += len;
			offsets[i] = cur_offset;
		}

		for (i = 0; i < n_columns; i++) {
			if (!g_data_output_stream_put_int32 (request->data_stream,
			                                     offsets[i],
			                                     NULL,
			                                     &inner_error))
				goto out;
		}

		for (i = 0; i < n_columns; i++) {
			if (g_cancellable_set_error_if_cancelled (request->cancellable, &inner_error))
				goto out;

			if (!g_data_output_stream_put_string (request->data_stream,
			                                      values[i] ? values[i] : "",
			                                      request->cancellable,
			                                      &inner_error))
				goto out;

			if (langtags[i]) {
				if (!g_data_output_stream_put_byte (request->data_stream, 0,
				                                    NULL,
				                                    &inner_error))
					goto out;

				if (!g_data_output_stream_put_string (request->data_stream,
				                                      langtags[i],
				                                      request->cancellable,
				                                      &inner_error))
					goto out;
			}

			if (!g_data_output_stream_put_byte (request->data_stream, 0,
			                                    NULL,
			                                    &inner_error))
				goto out;
		}
	}

out:

	g_free (values);
	g_free (offsets);
	g_free (langtags);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	} else {
		return TRUE;
	}
}

static void
handle_cursor_reply (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
	TrackerSparqlCursor *cursor = source_object;
	QueryRequest *request = task_data;
	const gchar **variable_names = NULL;
	GError *error = NULL;
	gboolean retval;
	gint i, n_columns;

	n_columns = tracker_sparql_cursor_get_n_columns (cursor);
	variable_names = g_new0 (const gchar *, n_columns + 1);
	for (i = 0; i < n_columns; i++)
		variable_names[i] = tracker_sparql_cursor_get_variable_name (cursor, i);

	g_dbus_method_invocation_return_value (request->invocation, g_variant_new ("(^as)", variable_names));

	retval = write_cursor (request, cursor, &error);
	g_free (variable_names);

	tracker_sparql_cursor_close (cursor);

	if (error)
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, retval);
}

static void
finish_query (GObject      *source_object,
              GAsyncResult *res,
              gpointer      user_data)
{
	TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
	GError *error = NULL;

	if (!g_task_propagate_boolean (G_TASK (res), &error)) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_critical ("Error writing cursor: %s\n", error->message);
	}

	g_object_unref (cursor);
	g_clear_error (&error);
}

static void
stmt_execute_cb (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
	QueryRequest *request = user_data;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GTask *task;

	cursor = tracker_sparql_statement_execute_finish (TRACKER_SPARQL_STATEMENT (object),
	                                                  res, &error);
	if (!cursor) {
		g_dbus_method_invocation_return_gerror (request->invocation, error);
		g_error_free (error);
		query_request_free (request);
		return;
	}

	task = g_task_new (cursor, request->cancellable, finish_query, NULL);
	g_task_set_task_data (task, request, (GDestroyNotify) query_request_free);
	g_task_run_in_thread (task, handle_cursor_reply);
	g_object_unref (task);
}

static void
splice_rdf_cb (GObject      *object,
               GAsyncResult *res,
               gpointer      user_data)
{
	QueryRequest *request = user_data;
	GError *error = NULL;

	g_output_stream_splice_finish (G_OUTPUT_STREAM (object),
	                               res, &error);

	if (error) {
		/* The query request method invocations has been already replied */
		g_warning ("Error splicing RDF data: %s", error->message);
		g_error_free (error);
	}

	query_request_free (request);
}

static void
stmt_serialize_cb (GObject      *object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
	QueryRequest *request = user_data;
	GInputStream *istream;
	GError *error = NULL;

	istream = tracker_sparql_statement_serialize_finish (TRACKER_SPARQL_STATEMENT (object),
	                                                     res, &error);
	if (!istream) {
		g_dbus_method_invocation_return_gerror (request->invocation, error);
		g_error_free (error);
		query_request_free (request);
		return;
	}

	g_dbus_method_invocation_return_value (request->invocation, NULL);
	g_output_stream_splice_async (G_OUTPUT_STREAM (request->data_stream),
	                              istream,
	                              G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
	                              G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
	                              G_PRIORITY_DEFAULT,
	                              request->global_cancellable,
	                              splice_rdf_cb,
	                              request);
	g_object_unref (istream);
}

static void
update_cb (GObject      *object,
           GAsyncResult *res,
           gpointer      user_data)
{
	UpdateRequest *request = user_data;
	GError *error = NULL;

	tracker_sparql_connection_update_finish (TRACKER_SPARQL_CONNECTION (object),
	                                         res, &error);

	if (error) {
		g_dbus_method_invocation_return_gerror (request->invocation, error);
		g_error_free (error);
	} else {
		g_dbus_method_invocation_return_value (request->invocation, NULL);
	}

	update_request_free (request);
}

static void
read_update_cb (GObject      *object,
                GAsyncResult *res,
                gpointer      user_data)
{
	UpdateRequest *request = user_data;
	TrackerSparqlConnection *conn;
	GError *error = NULL;

	if (!g_task_propagate_boolean (G_TASK (res), &error)) {
		g_dbus_method_invocation_return_gerror (request->invocation, error);
		update_request_free (request);
		return;
	}

	conn = tracker_endpoint_get_sparql_connection (TRACKER_ENDPOINT (request->endpoint));
	tracker_sparql_connection_update_async (conn,
	                                        request->query,
	                                        request->endpoint->cancellable,
	                                        update_cb,
	                                        request);
}

static void
update_blank_cb (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
	UpdateRequest *request = user_data;
	GError *error = NULL;
	GVariant *results;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	results = tracker_sparql_connection_update_blank_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                         res, &error);
	G_GNUC_END_IGNORE_DEPRECATIONS

	if (results) {
		GVariantBuilder builder;

		g_variant_builder_init (&builder, G_VARIANT_TYPE ("(aaa{ss})"));
		g_variant_builder_add_value (&builder, results);
		g_dbus_method_invocation_return_value (request->invocation,
		                                       g_variant_builder_end (&builder));
	} else {
		g_dbus_method_invocation_return_gerror (request->invocation, error);
	}

	update_request_free (request);
}

static void
read_update_blank_cb (GObject      *object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
	TrackerSparqlConnection *conn;
	UpdateRequest *request = user_data;
	GError *error = NULL;

	if (!g_task_propagate_boolean (G_TASK (res), &error)) {
		g_dbus_method_invocation_return_gerror (request->invocation, error);
		update_request_free (request);
		return;
	}

	conn = tracker_endpoint_get_sparql_connection (TRACKER_ENDPOINT (request->endpoint));
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	tracker_sparql_connection_update_blank_async (conn,
	                                              request->query,
	                                              request->endpoint->cancellable,
	                                              update_blank_cb,
	                                              request);
	G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
batch_execute_cb (GObject      *object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
	GDBusMethodInvocation *invocation = user_data;
	GError *error = NULL;

	if (!tracker_batch_execute_finish (TRACKER_BATCH (object), res, &error))
		g_dbus_method_invocation_return_gerror (invocation, error);
	else
		g_dbus_method_invocation_return_value (invocation, NULL);

	g_clear_error (&error);
	g_object_unref (object);
}

static void
deserialize_cb (GObject      *object,
                GAsyncResult *res,
                gpointer      user_data)
{
	GDBusMethodInvocation *invocation = user_data;
	GError *error = NULL;

	if (!tracker_sparql_connection_deserialize_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                   res, &error)) {
		g_dbus_method_invocation_return_gerror (invocation, error);
		return;
	}

	g_dbus_method_invocation_return_value (invocation, NULL);
}

static void
bind_arguments (TrackerSparqlStatement *stmt,
                GVariantIter           *arguments)
{
	GVariant *value;
	const gchar *arg;

	while (g_variant_iter_loop (arguments, "{sv}", &arg, &value)) {
		if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING)) {
			tracker_sparql_statement_bind_string (stmt, arg,
			                                      g_variant_get_string (value, NULL));
		} else if (g_variant_is_of_type (value, G_VARIANT_TYPE_DOUBLE)) {
			tracker_sparql_statement_bind_double (stmt, arg,
			                                      g_variant_get_double (value));
		} else if (g_variant_is_of_type (value, G_VARIANT_TYPE_INT64)) {
			tracker_sparql_statement_bind_int (stmt, arg,
			                                   g_variant_get_int64 (value));
		} else if (g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN)) {
			tracker_sparql_statement_bind_boolean (stmt, arg,
			                                       g_variant_get_boolean (value));
		} else if (g_variant_is_of_type (value, G_VARIANT_TYPE_BYTESTRING)) {
			const gchar *data, *langtag = NULL;
			gsize len, str_len;

			data = g_variant_get_fixed_array (value, &len, sizeof (guint8));
			str_len = strlen (data);

			if (str_len < len)
				langtag = &data[str_len + 1];

			tracker_sparql_statement_bind_langstring (stmt, arg, data, langtag);
		} else {
			g_warning ("Unhandled type '%s' for argument %s",
			           g_variant_get_type_string (value),
			           arg);
		}
	}
}

static void
endpoint_dbus_iface_method_call (GDBusConnection       *connection,
                                 const gchar           *sender,
                                 const gchar           *object_path,
                                 const gchar           *interface_name,
                                 const gchar           *method_name,
                                 GVariant              *parameters,
                                 GDBusMethodInvocation *invocation,
                                 gpointer               user_data)
{
	TrackerEndpointDBus *endpoint_dbus = user_data;
	GUnixFDList *fd_list;
	GError *error = NULL;
	GVariantIter *arguments;
	gchar *query;
	gint handle, fd = -1;

	if (tracker_endpoint_dbus_block_call (endpoint_dbus, invocation)) {
		g_dbus_method_invocation_return_error (invocation,
		                                       G_DBUS_ERROR,
		                                       G_DBUS_ERROR_ACCESS_DENIED,
		                                       "Operation not allowed");
		return;
	}

	fd_list = g_dbus_message_get_unix_fd_list (g_dbus_method_invocation_get_message (invocation));

	if (g_strcmp0 (method_name, "Query") == 0) {
		g_variant_get (parameters, "(sha{sv})", &query, &handle, &arguments);

		if (fd_list)
			fd = g_unix_fd_list_get (fd_list, handle, &error);

		if (fd < 0) {
			g_dbus_method_invocation_return_error (invocation,
			                                       G_DBUS_ERROR,
			                                       G_DBUS_ERROR_INVALID_ARGS,
			                                       "Did not get a file descriptor");
		} else {
			TrackerSparqlStatement *stmt;
			QueryRequest *request;

			tracker_endpoint_rewrite_query (TRACKER_ENDPOINT (endpoint_dbus),
			                                &query);

			request = query_request_new (endpoint_dbus, invocation, fd);

			stmt = tracker_endpoint_cache_select_sparql (TRACKER_ENDPOINT (endpoint_dbus),
			                                             query,
			                                             request->cancellable,
			                                             &error);

			if (stmt && arguments)
				bind_arguments (stmt, arguments);

			if (stmt) {
				tracker_sparql_statement_execute_async (stmt,
				                                        request->cancellable,
				                                        stmt_execute_cb,
				                                        request);
				/* Statements are single use here... */
				g_object_unref (stmt);
			} else {
				query_request_free (request);
				g_dbus_method_invocation_return_gerror (invocation,
				                                        error);
			}
		}

		g_variant_iter_free (arguments);
		g_free (query);
	} else if (g_strcmp0 (method_name, "Serialize") == 0) {
		TrackerSerializeFlags flags;
		TrackerRdfFormat format;

		g_variant_get (parameters, "(shiia{sv})", &query, &handle, &flags, &format, &arguments);

		if (fd_list)
			fd = g_unix_fd_list_get (fd_list, handle, &error);

		if (fd >= 0) {
			TrackerSparqlStatement *stmt;
			QueryRequest *request;

			tracker_endpoint_rewrite_query (TRACKER_ENDPOINT (endpoint_dbus),
			                                &query);

			request = query_request_new (endpoint_dbus, invocation, fd);

			stmt = tracker_endpoint_cache_select_sparql (TRACKER_ENDPOINT (endpoint_dbus),
			                                             query,
			                                             request->cancellable,
			                                             &error);

			if (stmt && arguments)
				bind_arguments (stmt, arguments);

			if (stmt) {
				tracker_sparql_statement_serialize_async (stmt,
				                                          flags,
				                                          format,
				                                          request->cancellable,
				                                          stmt_serialize_cb,
				                                          request);
				/* Statements are single use here... */
				g_object_unref (stmt);
			} else {
				query_request_free (request);
				g_dbus_method_invocation_return_gerror (invocation,
				                                        error);
			}
		}

		g_variant_iter_free (arguments);
		g_free (query);
	} else if (g_strcmp0 (method_name, "UpdateArray") == 0) {
		if (tracker_endpoint_get_readonly (TRACKER_ENDPOINT (endpoint_dbus))) {
			g_dbus_method_invocation_return_error (invocation,
			                                       G_DBUS_ERROR,
			                                       G_DBUS_ERROR_ACCESS_DENIED,
			                                       "Operation not allowed");
			return;
		}

		g_variant_get (parameters, "(h)", &handle);

		if (fd_list)
			fd = g_unix_fd_list_get (fd_list, handle, &error);

		if (fd >= 0) {
			TrackerSparqlConnection *conn;
			TrackerBatch *batch;
			GInputStream *istream;

			conn = tracker_endpoint_get_sparql_connection (TRACKER_ENDPOINT (endpoint_dbus));
			batch = tracker_sparql_connection_create_batch (conn);

			istream = g_unix_input_stream_new (fd, TRUE);
			tracker_batch_add_dbus_fd (batch, istream);
			g_object_unref (istream);

			tracker_batch_execute_async (batch,
			                             endpoint_dbus->cancellable,
			                             batch_execute_cb,
			                             invocation);
		}
	} else if (g_strcmp0 (method_name, "Update") == 0 ||
	           g_strcmp0 (method_name, "UpdateBlank") == 0) {
		if (tracker_endpoint_get_readonly (TRACKER_ENDPOINT (endpoint_dbus))) {
			g_dbus_method_invocation_return_error (invocation,
			                                       G_DBUS_ERROR,
			                                       G_DBUS_ERROR_ACCESS_DENIED,
			                                       "Operation not allowed");
			return;
		}

		g_variant_get (parameters, "(h)", &handle);

		if (fd_list)
			fd = g_unix_fd_list_get (fd_list, handle, &error);

		if (fd >= 0) {
			UpdateRequest *request;
			GTask *task;

			request = update_request_new (endpoint_dbus, invocation, fd);

			task = g_task_new (NULL, request->endpoint->cancellable,
			                   g_strcmp0 (method_name, "Update") == 0 ?
			                   read_update_cb : read_update_blank_cb,
			                   request);
			g_task_set_task_data (task, request, NULL);
			g_task_run_in_thread (task, handle_read_update);
			g_object_unref (task);
		}
	} else if (g_strcmp0 (method_name, "Deserialize") == 0) {
		TrackerDeserializeFlags flags;
		TrackerRdfFormat format;
		gchar *graph;

		if (tracker_endpoint_get_readonly (TRACKER_ENDPOINT (endpoint_dbus))) {
			g_dbus_method_invocation_return_error (invocation,
			                                       G_DBUS_ERROR,
			                                       G_DBUS_ERROR_ACCESS_DENIED,
			                                       "Operation not allowed");
			return;
		}

		g_variant_get (parameters, "(hiisa{sv})", &handle, &flags, &format, &graph, &arguments);

		if (fd_list)
			fd = g_unix_fd_list_get (fd_list, handle, &error);

		if (fd >= 0) {
			TrackerSparqlConnection *conn;
			GInputStream *istream;

			conn = tracker_endpoint_get_sparql_connection (TRACKER_ENDPOINT (endpoint_dbus));

			istream = g_unix_input_stream_new (fd, TRUE);

			tracker_sparql_connection_deserialize_async (conn,
			                                             flags,
			                                             format,
			                                             graph && *graph ? graph : NULL,
			                                             istream,
			                                             endpoint_dbus->cancellable,
			                                             deserialize_cb,
			                                             invocation);
			g_object_unref (istream);
		}

		g_variant_iter_free (arguments);
		g_free (graph);
	} else {
		g_dbus_method_invocation_return_error (invocation,
		                                       G_DBUS_ERROR,
		                                       G_DBUS_ERROR_UNKNOWN_METHOD,
		                                       "Unknown method '%s'", method_name);
	}

	if (fd < 0) {
		g_dbus_method_invocation_return_error (invocation,
		                                       G_DBUS_ERROR,
		                                       G_DBUS_ERROR_INVALID_ARGS,
		                                       "Did not get a file descriptor");
	}
}

static void
notifier_events_cb (TrackerNotifier *notifier,
                    const gchar     *service,
                    const gchar     *graph,
                    GPtrArray       *events,
                    gpointer         user_data)
{
	TrackerEndpointDBus *endpoint_dbus = user_data;
	GVariantBuilder builder;
	GError *error = NULL;
	guint i;

	if (tracker_endpoint_is_graph_filtered (TRACKER_ENDPOINT (endpoint_dbus), graph))
		return;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("(sa{ii})"));
	g_variant_builder_add (&builder, "s", graph ? graph : "");
	g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{ii}"));

	for (i = 0; i < events->len; i++) {
		TrackerNotifierEvent *event;
		gint event_type, id;

		event = g_ptr_array_index (events, i);
		event_type = tracker_notifier_event_get_event_type (event);
		id = tracker_notifier_event_get_id (event);
		g_variant_builder_add (&builder, "{ii}", event_type, id);
	}

	g_variant_builder_close (&builder);

	if (!g_dbus_connection_emit_signal (endpoint_dbus->dbus_connection,
	                                    NULL,
	                                    endpoint_dbus->object_path,
	                                    "org.freedesktop.Tracker3.Endpoint",
	                                    "GraphUpdated",
	                                    g_variant_builder_end (&builder),
	                                    &error)) {
		g_warning ("Could not emit GraphUpdated signal: %s", error->message);
		g_error_free (error);
	}
}

static gboolean
tracker_endpoint_dbus_initable_init (GInitable     *initable,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
	TrackerEndpoint *endpoint = TRACKER_ENDPOINT (initable);
	TrackerEndpointDBus *endpoint_dbus = TRACKER_ENDPOINT_DBUS (endpoint);
	TrackerSparqlConnection *conn;

	endpoint_dbus->node_info = g_dbus_node_info_new_for_xml (introspection_xml,
	                                                         error);
	if (!endpoint_dbus->node_info)
		return FALSE;

	endpoint_dbus->register_id =
		g_dbus_connection_register_object (endpoint_dbus->dbus_connection,
		                                   endpoint_dbus->object_path,
		                                   endpoint_dbus->node_info->interfaces[0],
		                                   &(GDBusInterfaceVTable) {
			                                   endpoint_dbus_iface_method_call,
				                           NULL,
				                           NULL
				                   },
		                                   endpoint_dbus,
		                                   NULL,
		                                   error);

	conn = tracker_endpoint_get_sparql_connection (endpoint);
	endpoint_dbus->notifier = tracker_sparql_connection_create_notifier (conn);
	tracker_notifier_disable_urn_query (endpoint_dbus->notifier);
	g_signal_connect (endpoint_dbus->notifier, "events",
	                  G_CALLBACK (notifier_events_cb), endpoint);

	return TRUE;
}

static void
tracker_endpoint_dbus_initable_iface_init (GInitableIface *iface)
{
	iface->init = tracker_endpoint_dbus_initable_init;
}

static void
tracker_endpoint_dbus_finalize (GObject *object)
{
	TrackerEndpointDBus *endpoint_dbus = TRACKER_ENDPOINT_DBUS (object);

	g_cancellable_cancel (endpoint_dbus->cancellable);

	if (endpoint_dbus->register_id != 0) {
		g_dbus_connection_unregister_object (endpoint_dbus->dbus_connection,
		                                     endpoint_dbus->register_id);
		endpoint_dbus->register_id = 0;
	}

	g_clear_object (&endpoint_dbus->notifier);
	g_clear_object (&endpoint_dbus->cancellable);
	g_clear_object (&endpoint_dbus->dbus_connection);
	g_clear_pointer (&endpoint_dbus->object_path, g_free);
	g_clear_pointer (&endpoint_dbus->node_info,
	                 g_dbus_node_info_unref);

	G_OBJECT_CLASS (tracker_endpoint_dbus_parent_class)->finalize (object);
}

static void
tracker_endpoint_dbus_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	TrackerEndpointDBus *endpoint_dbus = TRACKER_ENDPOINT_DBUS (object);

	switch (prop_id) {
	case PROP_DBUS_CONNECTION:
		endpoint_dbus->dbus_connection = g_value_dup_object (value);
		break;
	case PROP_OBJECT_PATH:
		endpoint_dbus->object_path = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_endpoint_dbus_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	TrackerEndpointDBus *endpoint_dbus = TRACKER_ENDPOINT_DBUS (object);

	switch (prop_id) {
	case PROP_DBUS_CONNECTION:
		g_value_set_object (value, endpoint_dbus->dbus_connection);
		break;
	case PROP_OBJECT_PATH:
		g_value_set_string (value, endpoint_dbus->object_path);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_endpoint_dbus_class_init (TrackerEndpointDBusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_endpoint_dbus_finalize;
	object_class->set_property = tracker_endpoint_dbus_set_property;
	object_class->get_property = tracker_endpoint_dbus_get_property;

	/**
	 * TrackerEndpointHttp::block-call:
	 * @self: The `TrackerEndpointDBus`
	 * @address: The D-Bus unique name of the remote connection
	 *
	 * Allows control over the connections stablished. The given
	 * address is that of the requesting peer.
	 *
	 * Returning %FALSE in this handler allows the connection,
	 * returning %TRUE blocks it. The default with no signal
	 * handlers connected is %FALSE.
	 */
	signals[BLOCK_CALL] =
		g_signal_new ("block-call",
		              TRACKER_TYPE_ENDPOINT_DBUS, 0, 0,
		              g_signal_accumulator_first_wins, NULL, NULL,
		              G_TYPE_BOOLEAN,
		              1, G_TYPE_STRING);

	/**
	 * TrackerEndpointDBus:dbus-connection:
	 *
	 * The [class@Gio.DBusConnection] where the connection is proxied through.
	 */
	props[PROP_DBUS_CONNECTION] =
		g_param_spec_object ("dbus-connection",
		                     "DBus connection",
		                     "DBus connection",
		                     G_TYPE_DBUS_CONNECTION,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	/**
	 * TrackerEndpointDBus:object-path:
	 *
	 * The DBus object path that this endpoint manages.
	 */
	props[PROP_OBJECT_PATH] =
		g_param_spec_string ("object-path",
		                     "DBus object path",
		                     "DBus object path",
		                     NULL,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
tracker_endpoint_dbus_init (TrackerEndpointDBus *endpoint)
{
	endpoint->cancellable = g_cancellable_new ();
}

/**
 * tracker_endpoint_dbus_new:
 * @sparql_connection: The [class@SparqlConnection] being made public
 * @dbus_connection: #GDBusConnection to expose the DBus object over
 * @object_path: (nullable): The object path to use, or %NULL to use the default
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Registers a Tracker endpoint object at @object_path on @dbus_connection.
 * The default object path is `/org/freedesktop/Tracker3/Endpoint`.
 *
 * Returns: (transfer full): a `TrackerEndpointDBus` object.
 */
TrackerEndpointDBus *
tracker_endpoint_dbus_new (TrackerSparqlConnection  *sparql_connection,
                           GDBusConnection          *dbus_connection,
                           const gchar              *object_path,
                           GCancellable             *cancellable,
                           GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (sparql_connection), NULL);
	g_return_val_if_fail (G_IS_DBUS_CONNECTION (dbus_connection), NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (!object_path)
		object_path = "/org/freedesktop/Tracker3/Endpoint";

	return g_initable_new (TRACKER_TYPE_ENDPOINT_DBUS, cancellable, error,
	                       "dbus-connection", dbus_connection,
	                       "sparql-connection", sparql_connection,
	                       "object-path", object_path,
	                       NULL);
}
