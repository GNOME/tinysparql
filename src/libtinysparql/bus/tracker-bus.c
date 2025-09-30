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

#include "tracker-bus.h"

#include <errno.h>

#include <gio/gunixfdlist.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <tracker-common.h>

#include "tracker-bus-batch.h"
#include "tracker-bus-cursor.h"
#include "tracker-bus-statement.h"
#include "tracker-notifier-private.h"

#define DBUS_PEER_IFACE "org.freedesktop.DBus.Peer"

#define PORTAL_NAME "org.freedesktop.portal.Tracker"
#define PORTAL_PATH "/org/freedesktop/portal/Tracker"
#define PORTAL_IFACE "org.freedesktop.portal.Tracker"

#define ENDPOINT_IFACE "org.freedesktop.Tracker3.Endpoint"

#define DBUS_TIMEOUT 30000

struct _TrackerBusConnection {
	TrackerSparqlConnection parent_instance;

	GDBusConnection *dbus_conn;
	TrackerNamespaceManager *namespaces;
	GList *notifiers;
	gchar *dbus_name;
	gchar *object_path;
	gboolean sandboxed;
};

enum {
	PROP_0,
	PROP_BUS_NAME,
	PROP_BUS_OBJECT_PATH,
	PROP_BUS_CONNECTION,
	N_PROPS
};

static GParamSpec *props[N_PROPS] = { 0, };

typedef struct {
	GMainLoop *loop;
	gpointer retval;
	GError *error;
} AsyncData;

typedef struct {
	struct {
		GError *error;
		gboolean finished;
	} dbus, splice;
} DeserializeTaskData;

typedef struct {
	struct {
		GError *error;
		gboolean finished;
	} dbus, write;
	GVariant *retval;
} UpdateTaskData;

static void tracker_bus_connection_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (TrackerBusConnection, tracker_bus_connection,
                         TRACKER_TYPE_SPARQL_CONNECTION,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                                tracker_bus_connection_async_initable_iface_init))

static GDBusMessage *
create_portal_create_session_message (TrackerBusConnection *conn)
{
	GDBusMessage *message;
	gchar *dbus_uri;
	const gchar *object_path;

	object_path = conn->object_path;
	if (g_strcmp0 (object_path, "/org/freedesktop/Tracker3/Endpoint") == 0)
		object_path = NULL;

	dbus_uri = tracker_util_build_dbus_uri (G_BUS_TYPE_SESSION,
	                                        conn->dbus_name,
	                                        object_path);
	message = g_dbus_message_new_method_call (PORTAL_NAME,
	                                          PORTAL_PATH,
	                                          PORTAL_IFACE,
	                                          "CreateSession");
	g_dbus_message_set_body (message, g_variant_new ("(s)", dbus_uri));
	g_free (dbus_uri);

	return message;
}

static GDBusMessage *
create_portal_close_session_message (TrackerBusConnection *bus)
{
	GDBusMessage *message;

	message = g_dbus_message_new_method_call (PORTAL_NAME,
	                                          PORTAL_PATH,
	                                          PORTAL_IFACE,
	                                          "CloseSession");
	g_dbus_message_set_body (message,
	                         g_variant_new ("(o)", bus->object_path));

	return message;
}

static GDBusMessage *
create_query_message (TrackerBusConnection *conn,
		      const gchar          *sparql,
		      GVariant             *arguments,
		      GUnixFDList          *fd_list,
		      int                   fd_idx)
{
	GDBusMessage *message;
	GVariant *body;

	if (!arguments)
		arguments = g_variant_new ("a{sv}", NULL);

	message = g_dbus_message_new_method_call (conn->dbus_name,
						  conn->object_path,
						  ENDPOINT_IFACE,
						  "Query");
	body = g_variant_new ("(sh@a{sv})", sparql, fd_idx, arguments);
	g_dbus_message_set_body (message, body);
	g_dbus_message_set_unix_fd_list (message, fd_list);

	return message;
}

static GDBusMessage *
create_serialize_message (TrackerBusConnection  *conn,
			  const gchar           *sparql,
			  TrackerSerializeFlags  flags,
			  TrackerRdfFormat       format,
			  GVariant              *arguments,
			  GUnixFDList           *fd_list,
			  int                    fd_idx)
{
	GDBusMessage *message;
	GVariant *body;

	if (!arguments)
		arguments = g_variant_new ("a{sv}", NULL);

	message = g_dbus_message_new_method_call (conn->dbus_name,
						  conn->object_path,
						  ENDPOINT_IFACE,
						  "Serialize");
	body = g_variant_new ("(shii@a{sv})",
			      sparql, fd_idx,
			      flags, format, arguments);
	g_dbus_message_set_body (message, body);
	g_dbus_message_set_unix_fd_list (message, fd_list);

	return message;
}

static GDBusMessage *
create_update_message (TrackerBusConnection *conn,
		       const gchar          *request,
		       GUnixFDList          *fd_list,
		       int                   fd_idx)
{
	GDBusMessage *message;
	GVariant *body;

	message = g_dbus_message_new_method_call (conn->dbus_name,
						  conn->object_path,
						  ENDPOINT_IFACE,
						  request);
	body = g_variant_new ("(h)", fd_idx);
	g_dbus_message_set_body (message, body);
	g_dbus_message_set_unix_fd_list (message, fd_list);

	return message;
}

static GDBusMessage *
create_deserialize_message (TrackerBusConnection    *conn,
			    TrackerDeserializeFlags  flags,
			    TrackerRdfFormat         format,
			    const gchar             *default_graph,
			    GUnixFDList             *fd_list,
			    int                      fd_idx)
{
	GDBusMessage *message;
	GVariant *body;

	message = g_dbus_message_new_method_call (conn->dbus_name,
						  conn->object_path,
						  ENDPOINT_IFACE,
						  "Deserialize");
	body = g_variant_new ("(hiisa{sv})",
			      fd_idx, flags, format,
			      default_graph ? default_graph : "",
			      NULL);
	g_dbus_message_set_body (message, body);
	g_dbus_message_set_unix_fd_list (message, fd_list);

	return message;
}

static void
write_sparql_query_in_thread (GTask        *task,
                              gpointer      source_object,
                              gpointer      task_data,
                              GCancellable *cancellable)
{
	GOutputStream *ostream = source_object;
	GDataOutputStream *data;
	const gchar *query;
	GError *error = NULL;
	int len;

	query = g_task_get_task_data (task);
	len = strlen (query);
	data = g_data_output_stream_new (ostream);
	g_data_output_stream_set_byte_order (data, G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN);

	if (!g_data_output_stream_put_uint32 (data, TRACKER_BUS_OP_SPARQL, cancellable, &error))
		goto error;
	if (!g_data_output_stream_put_int32 (data, len, cancellable, &error))
		goto error;
	if (!g_data_output_stream_put_string (data, query, cancellable, &error))
		goto error;
	if (!g_data_output_stream_put_int32 (data, 0, cancellable, &error))
		goto error;

 error:
	if (error)
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, TRUE);

	g_object_unref (data);
}

static void
write_sparql_query_async (GOutputStream       *ostream,
                          const gchar         *query,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  cb,
                          gpointer             user_data)
{
	GTask *task;

	task = g_task_new (ostream, cancellable, cb, user_data);
	g_task_set_task_data (task, g_strdup (query), g_free);
	g_task_run_in_thread (task, write_sparql_query_in_thread);
	g_object_unref (task);
}

static gboolean
write_sparql_query_finish (GOutputStream  *stream,
                           GAsyncResult   *res,
                           GError        **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static GVariant *
convert_params (GHashTable *parameters)
{
	GHashTableIter iter;
	const gchar *name;
	GVariant *value;
	GVariantBuilder builder;

	g_hash_table_iter_init (&iter, parameters);
	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

	while (g_hash_table_iter_next (&iter, (gpointer*) &name, (gpointer*) &value)) {
		g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
		g_variant_builder_add (&builder, "s", name);
		g_variant_builder_add (&builder, "v", value);
		g_variant_builder_close (&builder);
	}

	return g_variant_ref_sink (g_variant_builder_end (&builder));
}

static void
write_sparql_queries_in_thread (GTask        *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
	GOutputStream *ostream = source_object;
	GArray *op_array;
	GDataOutputStream *data;
	GOutputStream *rdf_stream = NULL;
	GBytes *bytes = NULL;
	gchar *params_str = NULL;
	GError *error = NULL;
	TrackerBusOp *first_op = NULL;
	guint i;

	op_array = g_task_get_task_data (task);
	data = g_data_output_stream_new (ostream);
	g_data_output_stream_set_byte_order (data, G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN);

	if (op_array->len >= 1)
		first_op = &g_array_index (op_array, TrackerBusOp, 0);

	/* Short circuit the forwarding of a single D-Bus FD */
	if (op_array->len == 1 &&
	    first_op && first_op->type == TRACKER_BUS_OP_DBUS_FD) {
		if (g_output_stream_splice (G_OUTPUT_STREAM (data),
		                            first_op->d.fd.stream,
		                            G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
		                            G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
		                            cancellable,
		                            &error) < 0)
			goto error;

		g_task_return_boolean (task, TRUE);
		return;
	}

	if (!g_data_output_stream_put_int32 (data, op_array->len, cancellable, &error))
		goto error;

	for (i = 0; i < op_array->len; i++) {
		TrackerBusOp *op = &g_array_index (op_array, TrackerBusOp, i);

		if (!g_data_output_stream_put_int32 (data, op->type, cancellable, &error))
			goto error;

		if (op->type == TRACKER_BUS_OP_SPARQL) {
			if (!g_data_output_stream_put_int32 (data,
			                                     strlen (op->d.sparql.sparql),
			                                     cancellable, &error))
				goto error;
			if (!g_data_output_stream_put_string (data, op->d.sparql.sparql,
			                                      cancellable, &error))
				goto error;

			if (op->d.sparql.parameters) {
				GVariant *variant;

				variant = convert_params (op->d.sparql.parameters);
				params_str = g_variant_print (variant, TRUE);
				g_variant_unref (variant);

				if (!g_data_output_stream_put_int32 (data,
				                                     strlen (params_str),
				                                     cancellable, &error))
					goto error;
				if (!g_data_output_stream_put_string (data, params_str,
				                                      cancellable, &error))
					goto error;

				g_clear_pointer (&params_str, g_free);
			} else {
				if (!g_data_output_stream_put_int32 (data, 0, cancellable, &error))
					goto error;
			}
		} else if (op->type == TRACKER_BUS_OP_RDF) {
			if (!g_data_output_stream_put_uint32 (data, op->d.rdf.flags, cancellable, &error))
				goto error;
			if (!g_data_output_stream_put_uint32 (data, op->d.rdf.format, cancellable, &error))
				goto error;

			if (op->d.rdf.default_graph) {
				if (!g_data_output_stream_put_int32 (data,
				                                     strlen (op->d.rdf.default_graph),
				                                     cancellable, &error))
					goto error;
				if (!g_data_output_stream_put_string (data, op->d.rdf.default_graph,
				                                      cancellable, &error))
					goto error;
			} else {
				if (!g_data_output_stream_put_int32 (data, 0, cancellable, &error))
					goto error;
			}

			rdf_stream = g_memory_output_stream_new_resizable ();
			if (g_output_stream_splice (rdf_stream,
			                            op->d.rdf.stream,
			                            G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
			                            G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
			                            cancellable,
			                            &error) < 0)
				goto error;

			bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (rdf_stream));
			g_clear_object (&rdf_stream);

			if (!g_data_output_stream_put_uint32 (data, g_bytes_get_size (bytes), cancellable, &error))
				goto error;
			if (!g_output_stream_write_all (G_OUTPUT_STREAM (data),
			                                g_bytes_get_data (bytes, NULL),
			                                g_bytes_get_size (bytes),
			                                NULL,
			                                cancellable,
			                                &error))
				goto error;

			g_clear_pointer (&bytes, g_bytes_unref);
		} else {
			g_assert_not_reached ();
		}
	}

 error:
	g_clear_object (&rdf_stream);
	g_clear_pointer (&bytes, g_bytes_unref);
	g_clear_pointer (&params_str, g_free);
	g_object_unref (data);

	if (error)
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, TRUE);
}

static void
write_sparql_queries_async (GOutputStream       *ostream,
                            GArray              *ops,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  cb,
                            gpointer             user_data)
{
	GTask *task;

	task = g_task_new (ostream, cancellable, cb, user_data);
	g_task_set_task_data (task, g_array_ref (ops), (GDestroyNotify) g_array_unref);
	g_task_run_in_thread (task, write_sparql_queries_in_thread);
	g_object_unref (task);
}

static gboolean
write_sparql_queries_finish (GOutputStream  *stream,
                             GAsyncResult   *res,
                             GError        **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
query_namespaces_cb (GObject      *source,
                     GAsyncResult *res,
                     gpointer      user_data)
{
	TrackerBusConnection *bus = TRACKER_BUS_CONNECTION (source);
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GTask *task = user_data;

	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (source),
	                                                 res, &error);
	if (cursor) {
		bus->namespaces = tracker_namespace_manager_new ();

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			const gchar *prefix, *uri;

			prefix = tracker_sparql_cursor_get_string (cursor, 0, NULL);
			uri = tracker_sparql_cursor_get_string (cursor, 1, NULL);

			tracker_namespace_manager_add_prefix (bus->namespaces,
			                                      prefix, uri);
		}

		tracker_namespace_manager_seal (bus->namespaces);
		tracker_sparql_cursor_close (cursor);
		g_task_return_boolean (task, TRUE);
		g_object_unref (cursor);
	} else {
		g_task_return_error (task, error);
	}

	g_object_unref (task);
}

static void
init_namespaces (TrackerBusConnection *conn,
                 GTask                *task)
{
	tracker_sparql_connection_query_async (TRACKER_SPARQL_CONNECTION (conn),
	                                       "SELECT ?prefix ?name { ?name nrl:prefix ?prefix }",
	                                       NULL,
	                                       query_namespaces_cb,
	                                       task);
}

static void
create_portal_session_cb (GObject      *source,
                          GAsyncResult *res,
                          gpointer      user_data)
{
	TrackerBusConnection *bus;
	GTask *task = user_data;
	GDBusMessage *reply;
	GError *error = NULL;
	GVariant *body;

	bus = g_task_get_source_object (task);
	reply = g_dbus_connection_send_message_with_reply_finish (G_DBUS_CONNECTION (source),
	                                                          res, &error);

	if (reply && !g_dbus_message_to_gerror (reply, &error)) {
		body = g_dbus_message_get_body (reply);

		/* We have now access through the portal, replace bus name/path */
		bus->sandboxed = TRUE;
		g_clear_pointer (&bus->object_path, g_free);
		g_variant_get_child (body, 0, "o", &bus->object_path);
		g_clear_pointer (&bus->dbus_name, g_free);
		bus->dbus_name = g_strdup (PORTAL_NAME);

		init_namespaces (bus, task);
	} else {
		g_task_return_error (task, error);
		g_object_unref (task);
	}

	g_clear_object (&reply);
}

static void
init_sandbox (TrackerBusConnection *conn,
              GTask                *task)
{
	GDBusMessage *message;

	/* We are in a flatpak sandbox, check going through the portal */
	message = create_portal_create_session_message (conn);
	g_dbus_connection_send_message_with_reply (conn->dbus_conn,
	                                           message,
	                                           G_DBUS_SEND_MESSAGE_FLAGS_NONE,
	                                           DBUS_TIMEOUT,
						   NULL,
	                                           g_task_get_cancellable (task),
	                                           create_portal_session_cb,
	                                           task);
	g_object_unref (message);
}

static void
ping_peer_cb (GObject      *source,
              GAsyncResult *res,
              gpointer      user_data)
{
	TrackerBusConnection *bus;
	GTask *task = user_data;
	GError *error = NULL;
	GDBusMessage *reply;

	bus = g_task_get_source_object (task);
	reply = g_dbus_connection_send_message_with_reply_finish (G_DBUS_CONNECTION (source),
	                                                          res, &error);

	if (!reply || g_dbus_message_to_gerror (reply, &error)) {
		if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS)) {
			/* We do not see the endpoint directly but this is a sandbox.
			 * Check for portal existence.
			 */
			g_clear_error (&error);
			init_sandbox (bus, task);
		} else {
			/* We do not see the endpoint, and this is not a sandboxed situation */
			g_dbus_error_strip_remote_error (error);
			g_task_return_error (task, error);
			g_object_unref (task);
		}
	} else {
		init_namespaces (bus, task);
	}

	g_clear_object (&reply);
}

static void
ping_peer (TrackerBusConnection *bus,
	   GTask                *task)
{
	/* If this environment variable is present, we always go via the portal */
	if (g_getenv ("TRACKER_TEST_PORTAL_FLATPAK_INFO") == NULL) {
		GDBusMessage *message;

		message = g_dbus_message_new_method_call (bus->dbus_name,
		                                          bus->object_path,
		                                          DBUS_PEER_IFACE,
		                                          "Ping");
		g_dbus_connection_send_message_with_reply (bus->dbus_conn,
		                                           message,
		                                           G_DBUS_SEND_MESSAGE_FLAGS_NONE,
		                                           DBUS_TIMEOUT,
							   NULL,
		                                           g_task_get_cancellable (task),
		                                           ping_peer_cb,
		                                           task);
		g_object_unref (message);
	} else {
		init_sandbox (bus, task);
	}
}

static void
get_bus_cb (GObject      *source,
	    GAsyncResult *res,
	    gpointer      user_data)
{
	TrackerBusConnection *bus;
	GTask *task = user_data;
	GError *error = NULL;

	bus = g_task_get_source_object (task);
	bus->dbus_conn = g_bus_get_finish (res, &error);

	if (bus->dbus_conn) {
		ping_peer (bus, task);
	} else {
		g_task_return_error (task, error);
		g_object_unref (task);
	}
}

static void
tracker_bus_connection_async_initable_init_async (GAsyncInitable      *async_initable,
                                                  gint                 priority,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data)
{
	TrackerBusConnection *bus;
	GTask *task;

	bus = TRACKER_BUS_CONNECTION (async_initable);
	task = g_task_new (async_initable, cancellable, callback, user_data);
	g_task_set_priority (task, priority);

	if (!bus->dbus_conn)
		g_bus_get (G_BUS_TYPE_SESSION, cancellable, get_bus_cb, task);
	else
		ping_peer (bus, task);
}

static gboolean
tracker_bus_connection_async_initable_init_finish (GAsyncInitable  *async_initable,
                                                   GAsyncResult    *res,
                                                   GError         **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
tracker_bus_connection_async_initable_iface_init (GAsyncInitableIface *iface)
{
	iface->init_async = tracker_bus_connection_async_initable_init_async;
	iface->init_finish = tracker_bus_connection_async_initable_init_finish;
}

static void
notifier_weak_ref_cb (gpointer  data,
                      GObject  *prev_location)
{
	TrackerBusConnection *bus = data;

	bus->notifiers = g_list_remove (bus->notifiers, prev_location);
}

static void
clear_notifiers (TrackerBusConnection *bus)
{
	while (bus->notifiers) {
		TrackerNotifier *notifier = bus->notifiers->data;

		tracker_notifier_stop (notifier);
		g_object_weak_unref (G_OBJECT (notifier),
		                     notifier_weak_ref_cb,
		                     bus);
		bus->notifiers = g_list_remove (bus->notifiers, notifier);
	}
}

static void
tracker_bus_connection_finalize (GObject *object)
{
	TrackerBusConnection *bus = TRACKER_BUS_CONNECTION (object);

	clear_notifiers (bus);

	g_clear_object (&bus->dbus_conn);
	g_clear_pointer (&bus->dbus_name, g_free);
	g_clear_pointer (&bus->object_path, g_free);
	g_clear_object (&bus->namespaces);

	G_OBJECT_CLASS (tracker_bus_connection_parent_class)->finalize (object);
}

static void
tracker_bus_connection_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
	TrackerBusConnection *bus = TRACKER_BUS_CONNECTION (object);

	switch (prop_id) {
	case PROP_BUS_NAME:
		bus->dbus_name = g_value_dup_string (value);
		break;
	case PROP_BUS_OBJECT_PATH:
		bus->object_path = g_value_dup_string (value);
		break;
	case PROP_BUS_CONNECTION:
		bus->dbus_conn = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_bus_connection_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
	TrackerBusConnection *bus = TRACKER_BUS_CONNECTION (object);

	switch (prop_id) {
	case PROP_BUS_NAME:
		g_value_set_string (value, bus->dbus_name);
		break;
	case PROP_BUS_OBJECT_PATH:
		g_value_set_string (value, bus->object_path);
		break;
	case PROP_BUS_CONNECTION:
		g_value_set_object (value, bus->dbus_conn);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gboolean
create_pipe (int     *input,
             int     *output,
             GError **error)
{
	int fds[2];

	if (pipe (fds) < 0) {
		g_set_error (error,
		             G_IO_ERROR,
		             g_io_error_from_errno (errno),
		             "Pipe creation failed: %m");
		return FALSE;
	}

	*input = fds[0];
	*output = fds[1];
	return TRUE;
}

static gboolean
create_pipe_for_write (GOutputStream **ostream,
		       GUnixFDList   **fd_list,
		       int            *fd_idx,
		       GError        **error)
{
	int input, output, idx;
	GUnixFDList *list;

	if (!create_pipe (&input, &output, error))
		return FALSE;

	list = g_unix_fd_list_new ();
	idx = g_unix_fd_list_append (list, input, error);
	close (input);

	if (idx < 0) {
		g_object_unref (list);
		close (output);
		return FALSE;
	}

	*fd_list = list;
	*fd_idx = idx;
	*ostream = g_unix_output_stream_new (output, TRUE);

	return TRUE;
}

static gboolean
create_pipe_for_read (GInputStream **istream,
		      GUnixFDList   **fd_list,
		      int            *fd_idx,
		      GError        **error)
{
	int input, output, idx;
	GUnixFDList *list;

	if (!create_pipe (&input, &output, error))
		return FALSE;

	list = g_unix_fd_list_new ();
	idx = g_unix_fd_list_append (list, output, error);
	close (output);

	if (idx < 0) {
		g_object_unref (list);
		close (input);
		return FALSE;
	}

	*fd_list = list;
	*fd_idx = idx;
	*istream = g_unix_input_stream_new (input, TRUE);

	return TRUE;
}

static TrackerSparqlCursor *
tracker_bus_connection_query (TrackerSparqlConnection  *self,
                              const gchar              *sparql,
                              GCancellable             *cancellable,
                              GError                  **error)
{
	return tracker_bus_connection_perform_query (TRACKER_BUS_CONNECTION (self),
						     sparql, NULL,
						     cancellable, error);
}

static void
query_async_cb (GObject      *source,
                GAsyncResult *res,
                gpointer      user_data)
{
	TrackerSparqlCursor *cursor;
	GTask *task = user_data;
	GError *error = NULL;

	cursor = tracker_bus_connection_perform_query_finish (TRACKER_BUS_CONNECTION (source),
							      res, &error);

	if (cursor)
		g_task_return_pointer (task, cursor, g_object_unref);
	else
		g_task_return_error (task, error);

	g_object_unref (task);
}

static void
tracker_bus_connection_query_async (TrackerSparqlConnection *self,
                                    const gchar             *sparql,
                                    GCancellable            *cancellable,
                                    GAsyncReadyCallback      callback,
                                    gpointer                 user_data)
{
	GTask *task;

	task = g_task_new (self, cancellable, callback, user_data);
	tracker_bus_connection_perform_query_async (TRACKER_BUS_CONNECTION (self),
						    sparql,
						    NULL,
						    cancellable,
						    query_async_cb,
						    task);
}

static TrackerSparqlCursor *
tracker_bus_connection_query_finish (TrackerSparqlConnection  *self,
                                     GAsyncResult             *res,
                                     GError                  **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static TrackerSparqlStatement *
tracker_bus_connection_query_statement (TrackerSparqlConnection  *self,
                                        const gchar              *query,
                                        GCancellable             *cancellable,
                                        GError                  **error)
{
	return tracker_bus_statement_new (TRACKER_BUS_CONNECTION (self), query);
}

static TrackerSparqlStatement *
tracker_bus_connection_update_statement (TrackerSparqlConnection  *self,
                                         const gchar              *query,
                                         GCancellable             *cancellable,
                                         GError                  **error)
{
	return tracker_bus_statement_new_update (TRACKER_BUS_CONNECTION (self), query);
}

static void
update_dbus_call_cb (GObject      *source,
		     GAsyncResult *res,
		     gpointer      user_data)
{
	GTask *task = user_data;
	GDBusMessage *reply;
	GError *error = NULL;

	reply = g_dbus_connection_send_message_with_reply_finish (G_DBUS_CONNECTION (source),
	                                                          res, &error);
	if (reply && !g_dbus_message_to_gerror (reply, &error)) {
		GVariant *body;

		body = g_dbus_message_get_body (reply);
		g_task_return_pointer (task,
				       body ? g_variant_ref (body) : NULL,
				       (GDestroyNotify) g_variant_unref);
	} else {
		g_dbus_error_strip_remote_error (error);
		g_task_return_error (task, error);
	}

	g_object_unref (task);
	g_clear_object (&reply);
}

static void
perform_update_async (TrackerBusConnection *bus,
		      const gchar          *request,
		      GUnixFDList          *fd_list,
		      int                   fd_idx,
		      GCancellable         *cancellable,
		      GAsyncReadyCallback   callback,
		      gpointer              user_data)
{
	GDBusMessage *message;
	GTask *task;

	task = g_task_new (bus, cancellable, callback, user_data);
	message = create_update_message (bus, request, fd_list, fd_idx);
	g_dbus_connection_send_message_with_reply (bus->dbus_conn,
	                                           message,
	                                           G_DBUS_SEND_MESSAGE_FLAGS_NONE,
						   G_MAXINT,
	                                           NULL,
	                                           cancellable,
	                                           update_dbus_call_cb,
	                                           task);
	g_object_unref (message);
}

static GVariant *
perform_update_finish (TrackerBusConnection  *conn,
		       GAsyncResult          *res,
		       GError               **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
update_sync_cb (GObject      *source,
		GAsyncResult *res,
		gpointer      user_data)
{
	AsyncData *data = user_data;

	tracker_sparql_connection_update_finish (TRACKER_SPARQL_CONNECTION (source),
						 res, &data->error);
	g_main_loop_quit (data->loop);
}

static void
tracker_bus_connection_update (TrackerSparqlConnection  *self,
                               const gchar              *sparql,
                               GCancellable             *cancellable,
                               GError                  **error)
{
	GMainContext *context;
	AsyncData data = { 0, };

	context = g_main_context_new ();
	data.loop = g_main_loop_new (context, FALSE);
	g_main_context_push_thread_default (context);

	tracker_sparql_connection_update_async (self,
						sparql,
						cancellable,
						update_sync_cb,
						&data);
	g_main_loop_run (data.loop);

	g_main_context_pop_thread_default (context);

	g_main_loop_unref (data.loop);
	g_main_context_unref (context);

	if (data.error)
		g_propagate_error (error, data.error);
}

static void
check_finish_update (GTask *task)
{
	UpdateTaskData *data;

	data = g_task_get_task_data (task);

	if (!data->dbus.finished || !data->write.finished)
		return;

	if (data->dbus.error) {
		g_dbus_error_strip_remote_error (data->dbus.error);
		g_task_return_error (task, g_steal_pointer (&data->dbus.error));
	} else if (data->write.error) {
		g_task_return_error (task, g_steal_pointer (&data->write.error));
	} else {
		g_task_return_pointer (task, g_steal_pointer (&data->retval),
		                       (GDestroyNotify) g_variant_unref);
	}

	g_object_unref (task);
}

static void
update_cb (GObject      *object,
           GAsyncResult *res,
           gpointer      user_data)
{
	GTask *task = user_data;
	UpdateTaskData *data;

	data = g_task_get_task_data (task);
	data->retval = perform_update_finish (TRACKER_BUS_CONNECTION (object),
	                                      res, &data->dbus.error);
	data->dbus.finished = TRUE;
	check_finish_update (task);
}

static void
write_query_cb (GObject      *object,
                GAsyncResult *res,
                gpointer      user_data)
{
	GTask *task = user_data;
	UpdateTaskData *data;

	data = g_task_get_task_data (task);
	write_sparql_query_finish (G_OUTPUT_STREAM (object),
	                           res, &data->write.error);
	data->write.finished = TRUE;
	check_finish_update (task);
}

static void
update_task_data_free (gpointer data)
{
	UpdateTaskData *task_data = data;

	g_clear_error (&task_data->dbus.error);
	g_clear_error (&task_data->write.error);
	g_clear_pointer (&task_data->retval, g_variant_unref);
	g_free (task_data);
}

static void
tracker_bus_connection_update_async (TrackerSparqlConnection *self,
                                     const gchar             *sparql,
                                     GCancellable            *cancellable,
                                     GAsyncReadyCallback      callback,
                                     gpointer                 user_data)
{
	UpdateTaskData *data;
	GUnixFDList *fd_list;
	GOutputStream *ostream;
	GError *error = NULL;
	GTask *task;
	int fd_idx;

	task = g_task_new (self, cancellable, callback, user_data);

	data = g_new0 (UpdateTaskData, 1);
	g_task_set_task_data (task, data, update_task_data_free);

	if (!create_pipe_for_write (&ostream, &fd_list, &fd_idx, &error)) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	perform_update_async (TRACKER_BUS_CONNECTION (self),
	                      "Update",
	                      fd_list, fd_idx,
	                      cancellable,
	                      update_cb,
	                      task);

	write_sparql_query_async (ostream, sparql,
	                          cancellable,
	                          write_query_cb, task);
	g_object_unref (ostream);
	g_object_unref (fd_list);
}

static void
tracker_bus_connection_update_finish (TrackerSparqlConnection  *self,
                                      GAsyncResult             *res,
                                      GError                  **error)
{
	GVariant *retval;

	retval = g_task_propagate_pointer (G_TASK (res), error);
	g_clear_pointer (&retval, g_variant_unref);
}

static void
tracker_bus_connection_update_array_async (TrackerSparqlConnection  *self,
                                           gchar                   **updates,
                                           gint                      n_updates,
                                           GCancellable             *cancellable,
                                           GAsyncReadyCallback       callback,
                                           gpointer                  user_data)
{
	GArray *ops;
	gint i;

	ops = g_array_sized_new (FALSE, FALSE, sizeof (TrackerBusOp), n_updates);

	for (i = 0; i < n_updates; i++) {
		TrackerBusOp op = { 0, };

		op.type = TRACKER_BUS_OP_SPARQL;
		op.d.sparql.sparql = updates[i];
		g_array_append_val (ops, op);
	}

	tracker_bus_connection_perform_update_async (TRACKER_BUS_CONNECTION (self),
	                                             ops,
	                                             cancellable,
	                                             callback,
	                                             user_data);
	g_array_unref (ops);
}

static gboolean
tracker_bus_connection_update_array_finish (TrackerSparqlConnection  *self,
                                            GAsyncResult             *res,
                                            GError                  **error)
{
	return tracker_bus_connection_perform_update_finish (TRACKER_BUS_CONNECTION (self),
	                                                     res, error);
}

static void
update_blank_sync_cb (GObject      *source,
		      GAsyncResult *res,
		      gpointer      user_data)
{
	AsyncData *data = user_data;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	data->retval =
		tracker_sparql_connection_update_blank_finish (TRACKER_SPARQL_CONNECTION (source),
							       res, &data->error);
	G_GNUC_END_IGNORE_DEPRECATIONS

	g_main_loop_quit (data->loop);
}

static GVariant *
tracker_bus_connection_update_blank (TrackerSparqlConnection  *self,
                                     const gchar              *sparql,
                                     GCancellable             *cancellable,
                                     GError                  **error)
{
	GMainContext *context;
	AsyncData data = { 0, };

	context = g_main_context_new ();
	data.loop = g_main_loop_new (context, FALSE);
	g_main_context_push_thread_default (context);

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	tracker_sparql_connection_update_blank_async (self,
						      sparql,
						      cancellable,
						      update_blank_sync_cb,
						      &data);
	G_GNUC_END_IGNORE_DEPRECATIONS
	g_main_loop_run (data.loop);

	g_main_context_pop_thread_default (context);

	g_main_loop_unref (data.loop);
	g_main_context_unref (context);

	if (data.error) {
		g_propagate_error (error, data.error);
		return NULL;
	}

	return data.retval;
}

static void
tracker_bus_connection_update_blank_async (TrackerSparqlConnection *self,
                                           const gchar             *sparql,
                                           GCancellable            *cancellable,
                                           GAsyncReadyCallback      callback,
                                           gpointer                 user_data)
{
	UpdateTaskData *data;
	GUnixFDList *fd_list;
	GOutputStream *ostream;
	GError *error = NULL;
	GTask *task;
	int fd_idx;

	task = g_task_new (self, cancellable, callback, user_data);

	data = g_new0 (UpdateTaskData, 1);
	g_task_set_task_data (task, data, update_task_data_free);

	if (!create_pipe_for_write (&ostream, &fd_list, &fd_idx, &error)) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	perform_update_async (TRACKER_BUS_CONNECTION (self),
	                      "UpdateBlank",
	                      fd_list, fd_idx,
	                      cancellable,
	                      update_cb,
	                      task);

	write_sparql_query_async (ostream, sparql,
	                          cancellable, write_query_cb,
	                          task);

	g_object_unref (ostream);
	g_object_unref (fd_list);
}

static GVariant *
tracker_bus_connection_update_blank_finish (TrackerSparqlConnection  *self,
                                            GAsyncResult             *res,
                                            GError                  **error)
{
	GVariant *retval;

	retval = g_task_propagate_pointer (G_TASK (res), error);

	if (retval) {
		GVariant *child;

		child = g_variant_get_child_value (retval, 0);
		g_variant_unref (retval);
		return child;
	}

	return NULL;
}

static TrackerNamespaceManager *
tracker_bus_connection_get_namespace_manager (TrackerSparqlConnection *self)
{
	TrackerBusConnection *bus = TRACKER_BUS_CONNECTION (self);

	return bus->namespaces;
}

static TrackerNotifier *
tracker_bus_connection_create_notifier (TrackerSparqlConnection *self)
{
	TrackerBusConnection *bus = TRACKER_BUS_CONNECTION (self);
	TrackerNotifier *notifier;

	notifier = g_object_new (TRACKER_TYPE_NOTIFIER,
	                         "connection", self,
	                         NULL);

	tracker_notifier_signal_subscribe (notifier,
	                                   bus->dbus_conn,
	                                   bus->dbus_name,
	                                   bus->object_path,
	                                   NULL);

	g_object_weak_ref (G_OBJECT (notifier), notifier_weak_ref_cb, self);
	bus->notifiers = g_list_prepend (bus->notifiers, notifier);

	return notifier;
}

static void
tracker_bus_connection_close (TrackerSparqlConnection *self)
{
	TrackerBusConnection *bus = TRACKER_BUS_CONNECTION (self);

	clear_notifiers (bus);

	if (bus->sandboxed) {
		GDBusMessage *message;

		message = create_portal_close_session_message (bus);
		g_dbus_connection_send_message (bus->dbus_conn,
		                                message,
		                                G_DBUS_SEND_MESSAGE_FLAGS_NONE,
		                                NULL, NULL);
		g_object_unref (message);
	}
}

static void
tracker_bus_connection_close_async (TrackerSparqlConnection *connection,
                                    GCancellable            *cancellable,
                                    GAsyncReadyCallback      callback,
                                    gpointer                 user_data)
{
	TrackerBusConnection *bus = TRACKER_BUS_CONNECTION (connection);
	GTask *task;

	task = g_task_new (connection, cancellable, callback, user_data);

	clear_notifiers (bus);

	if (bus->sandboxed) {
		GDBusMessage *message;
		GError *error = NULL;

		message = create_portal_close_session_message (bus);

		if (!g_dbus_connection_send_message (bus->dbus_conn,
		                                     message,
		                                     G_DBUS_SEND_MESSAGE_FLAGS_NONE,
		                                     NULL, &error))
			g_task_return_error (task, error);
		else
			g_task_return_boolean (task, TRUE);

		g_object_unref (message);
	} else {
		g_task_return_boolean (task, TRUE);
	}

	g_object_unref (task);
}

static gboolean
tracker_bus_connection_close_finish (TrackerSparqlConnection  *connection,
                                     GAsyncResult             *res,
                                     GError                  **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static gboolean
tracker_bus_connection_update_resource (TrackerSparqlConnection  *self,
                                        const gchar              *graph,
                                        TrackerResource          *resource,
                                        GCancellable             *cancellable,
                                        GError                  **error)
{
	TrackerBatch *batch;
	gboolean retval;

	batch = tracker_sparql_connection_create_batch (self);
	tracker_batch_add_resource (batch, graph, resource);
	retval = tracker_batch_execute (batch, cancellable, error);
	g_object_unref (batch);

	return retval;
}

static void
update_resource_cb (GObject      *source,
                    GAsyncResult *res,
                    gpointer      user_data)
{
	GTask *task = user_data;
	GError *error = NULL;
	gboolean retval;

	retval = tracker_batch_execute_finish (TRACKER_BATCH (source),
					       res, &error);

	if (!retval)
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, retval);

	g_object_unref (task);
}

static void
tracker_bus_connection_update_resource_async (TrackerSparqlConnection *self,
                                              const gchar             *graph,
                                              TrackerResource         *resource,
                                              GCancellable            *cancellable,
                                              GAsyncReadyCallback      callback,
                                              gpointer                 user_data)
{
	TrackerBatch *batch;
	GTask *task;

	task = g_task_new (self, cancellable, callback, user_data);

	batch = tracker_sparql_connection_create_batch (self);
	tracker_batch_add_resource (batch, graph, resource);
	tracker_batch_execute_async (batch, cancellable, update_resource_cb, task);
	g_object_unref (batch);
}

static gboolean
tracker_bus_connection_update_resource_finish (TrackerSparqlConnection  *connection,
                                               GAsyncResult             *res,
                                               GError                  **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static TrackerBatch *
tracker_bus_connection_create_batch (TrackerSparqlConnection *connection)
{
	TrackerBusConnection *bus = TRACKER_BUS_CONNECTION (connection);

	return tracker_bus_batch_new (bus);
}

static void
perform_serialize_cb (GObject      *source,
		      GAsyncResult *res,
		      gpointer      user_data)
{
	GInputStream *istream;
	GError *error = NULL;
	GTask *task = user_data;

	istream = tracker_bus_connection_perform_serialize_finish (TRACKER_BUS_CONNECTION (source),
								   res, &error);
	if (istream)
		g_task_return_pointer (task, istream, g_object_unref);
	else
		g_task_return_error (task, error);

	g_object_unref (task);
}

static void
tracker_bus_connection_serialize_async (TrackerSparqlConnection  *self,
                                        TrackerSerializeFlags     flags,
                                        TrackerRdfFormat          format,
                                        const gchar              *query,
                                        GCancellable             *cancellable,
                                        GAsyncReadyCallback       callback,
                                        gpointer                  user_data)
{
	GTask *task;

	task = g_task_new (self, cancellable, callback, user_data);
	tracker_bus_connection_perform_serialize_async (TRACKER_BUS_CONNECTION (self),
							flags, format, query, NULL,
							cancellable,
							perform_serialize_cb,
							task);
}

static GInputStream *
tracker_bus_connection_serialize_finish (TrackerSparqlConnection  *connection,
                                         GAsyncResult             *res,
                                         GError                  **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
check_finish_deserialize (GTask *task)
{
	DeserializeTaskData *data;

	data = g_task_get_task_data (task);

	if (!data->dbus.finished || !data->splice.finished)
		return;

	if (data->dbus.error) {
		g_dbus_error_strip_remote_error (data->dbus.error);
		g_task_return_error (task, g_steal_pointer (&data->dbus.error));
	} else if (data->splice.error) {
		g_task_return_error (task, g_steal_pointer (&data->splice.error));
	} else {
		g_task_return_boolean (task, TRUE);
	}

	g_object_unref (task);
}

static void
deserialize_cb (GObject      *source,
		GAsyncResult *res,
		gpointer      user_data)
{
	GDBusMessage *reply;
	GTask *task = user_data;
	DeserializeTaskData *data;
	GError *error = NULL;

	data = g_task_get_task_data (task);

	reply = g_dbus_connection_send_message_with_reply_finish (G_DBUS_CONNECTION (source),
	                                                          res, &error);
	if (!reply || g_dbus_message_to_gerror (reply, &error))
		data->dbus.error = error;

	g_clear_object (&reply);

	data->dbus.finished = TRUE;
	check_finish_deserialize (task);
}

static void
splice_cb (GObject      *source,
	   GAsyncResult *res,
	   gpointer      user_data)
{
	GTask *task = user_data;
	DeserializeTaskData *data;
	GError *error = NULL;

	data = g_task_get_task_data (task);

	g_output_stream_splice_finish (G_OUTPUT_STREAM (source),
				       res, &error);
	data->splice.finished = TRUE;
	data->splice.error = error;
	check_finish_deserialize (task);
}

static void
deserialize_task_data_free (gpointer data)
{
	DeserializeTaskData *task_data = data;

	g_clear_error (&task_data->dbus.error);
	g_clear_error (&task_data->splice.error);
	g_free (task_data);
}

static void
tracker_bus_connection_deserialize_async (TrackerSparqlConnection *self,
                                          TrackerDeserializeFlags  flags,
                                          TrackerRdfFormat         format,
                                          const gchar             *default_graph,
                                          GInputStream            *istream,
                                          GCancellable            *cancellable,
                                          GAsyncReadyCallback      callback,
                                          gpointer                 user_data)
{
	TrackerBusConnection *bus = TRACKER_BUS_CONNECTION (self);
	GDBusMessage *message;
	GUnixFDList *fd_list;
	GOutputStream *ostream;
	DeserializeTaskData *data;
	GError *error = NULL;
	GTask *task;
	int fd_idx;

	task = g_task_new (self, cancellable, callback, user_data);

	data = g_new0 (DeserializeTaskData, 1);
	g_task_set_task_data (task, data, deserialize_task_data_free);

	if (!create_pipe_for_write (&ostream, &fd_list, &fd_idx, &error)) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	message = create_deserialize_message (bus, flags, format,
					      default_graph,
					      fd_list, fd_idx);

	g_dbus_connection_send_message_with_reply (bus->dbus_conn,
	                                           message,
	                                           G_DBUS_SEND_MESSAGE_FLAGS_NONE,
						   G_MAXINT,
	                                           NULL,
	                                           cancellable,
	                                           deserialize_cb,
	                                           task);
	g_output_stream_splice_async (ostream, istream,
				      G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
				      G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
				      G_PRIORITY_DEFAULT,
				      cancellable,
				      splice_cb,
				      task);

	g_object_unref (message);
	g_object_unref (fd_list);
	g_object_unref (ostream);
}

static gboolean
tracker_bus_connection_deserialize_finish (TrackerSparqlConnection  *connection,
                                           GAsyncResult             *res,
                                           GError                  **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
tracker_bus_connection_class_init (TrackerBusConnectionClass *klass)
{
	TrackerSparqlConnectionClass *sparql_connection_class;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	sparql_connection_class = TRACKER_SPARQL_CONNECTION_CLASS (klass);

	object_class->finalize = tracker_bus_connection_finalize;
	object_class->set_property = tracker_bus_connection_set_property;
	object_class->get_property = tracker_bus_connection_get_property;

	sparql_connection_class->query = tracker_bus_connection_query;
	sparql_connection_class->query_async = tracker_bus_connection_query_async;
	sparql_connection_class->query_finish = tracker_bus_connection_query_finish;
	sparql_connection_class->query_statement = tracker_bus_connection_query_statement;
	sparql_connection_class->update_statement = tracker_bus_connection_update_statement;
	sparql_connection_class->update = tracker_bus_connection_update;
	sparql_connection_class->update_async = tracker_bus_connection_update_async;
	sparql_connection_class->update_finish = tracker_bus_connection_update_finish;
	sparql_connection_class->update_array_async = tracker_bus_connection_update_array_async;
	sparql_connection_class->update_array_finish = tracker_bus_connection_update_array_finish;
	sparql_connection_class->update_blank = tracker_bus_connection_update_blank;
	sparql_connection_class->update_blank_async = tracker_bus_connection_update_blank_async;
	sparql_connection_class->update_blank_finish = tracker_bus_connection_update_blank_finish;
	sparql_connection_class->get_namespace_manager = tracker_bus_connection_get_namespace_manager;
	sparql_connection_class->create_notifier = tracker_bus_connection_create_notifier;
	sparql_connection_class->close = tracker_bus_connection_close;
	sparql_connection_class->close_async = tracker_bus_connection_close_async;
	sparql_connection_class->close_finish = tracker_bus_connection_close_finish;
	sparql_connection_class->update_resource = tracker_bus_connection_update_resource;
	sparql_connection_class->update_resource_async = tracker_bus_connection_update_resource_async;
	sparql_connection_class->update_resource_finish = tracker_bus_connection_update_resource_finish;
	sparql_connection_class->create_batch = tracker_bus_connection_create_batch;
	sparql_connection_class->serialize_async = tracker_bus_connection_serialize_async;
	sparql_connection_class->serialize_finish = tracker_bus_connection_serialize_finish;
	sparql_connection_class->deserialize_async = tracker_bus_connection_deserialize_async;
	sparql_connection_class->deserialize_finish = tracker_bus_connection_deserialize_finish;

	props[PROP_BUS_NAME] =
		g_param_spec_string ("bus-name",
		                     "Bus name",
		                     "Bus name",
		                     NULL,
		                     G_PARAM_READWRITE |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_CONSTRUCT_ONLY);
	props[PROP_BUS_OBJECT_PATH] =
		g_param_spec_string ("bus-object-path",
		                     "Bus object path",
		                     "Bus object path",
		                     NULL,
		                     G_PARAM_READWRITE |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_CONSTRUCT_ONLY);
	props[PROP_BUS_CONNECTION] =
		g_param_spec_object ("bus-connection",
		                     "Bus connection",
		                     "Bus connection",
		                     G_TYPE_DBUS_CONNECTION,
		                     G_PARAM_READWRITE |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
tracker_bus_connection_init (TrackerBusConnection *conn)
{
}

static void
bus_new_cb (GObject      *source,
            GAsyncResult *res,
            gpointer      user_data)
{
	AsyncData *data = user_data;

	data->retval = tracker_bus_connection_new_finish (res, &data->error);
	g_main_loop_quit (data->loop);
}

TrackerSparqlConnection *
tracker_bus_connection_new (const gchar      *service,
                            const gchar      *object_path,
                            GDBusConnection  *conn,
                            GError          **error)
{
	GMainContext *context;
	AsyncData data = { 0, };

	context = g_main_context_new ();
	data.loop = g_main_loop_new (context, FALSE);
	g_main_context_push_thread_default (context);

	tracker_bus_connection_new_async (service,
	                                  object_path,
	                                  conn,
					  NULL,
	                                  bus_new_cb,
	                                  &data);

	g_main_loop_run (data.loop);

	g_main_context_pop_thread_default (context);

	g_main_loop_unref (data.loop);
	g_main_context_unref (context);

	if (data.error) {
		g_propagate_error (error, data.error);
		return NULL;
	}

	return TRACKER_SPARQL_CONNECTION (data.retval);
}

void
tracker_bus_connection_new_async (const gchar         *service,
                                  const gchar         *object_path,
                                  GDBusConnection     *conn,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  cb,
                                  gpointer             user_data)
{
	g_async_initable_new_async (TRACKER_TYPE_BUS_CONNECTION,
	                            G_PRIORITY_DEFAULT,
	                            cancellable,
	                            cb,
	                            user_data,
	                            "bus-name", service,
	                            "bus-object-path", object_path,
	                            "bus-connection", conn,
	                            NULL);
}

TrackerSparqlConnection *
tracker_bus_connection_new_finish (GAsyncResult  *res,
                                   GError       **error)
{
	GAsyncInitable *initable;

	initable = g_task_get_source_object (G_TASK (res));

	return TRACKER_SPARQL_CONNECTION (g_async_initable_new_finish (initable,
								       res,
	                                                               error));
}

static void
query_dbus_call_cb (GObject      *source,
                    GAsyncResult *res,
                    gpointer      user_data)
{
	GTask *task = user_data;
	GDBusMessage *reply;
	GError *error = NULL;

	reply = g_dbus_connection_send_message_with_reply_finish (G_DBUS_CONNECTION (source),
	                                                          res, &error);
	if (reply && !g_dbus_message_to_gerror (reply, &error)) {
		TrackerSparqlCursor *cursor;
		GVariant *body, *child;
		GInputStream *istream;

		body = g_dbus_message_get_body (reply);
		istream = g_task_get_task_data (task);
		child = g_variant_get_child_value (body, 0);
		cursor = tracker_bus_cursor_new (istream, child);
		g_task_return_pointer (task, cursor, g_object_unref);
		g_variant_unref (child);
	} else {
		g_dbus_error_strip_remote_error (error);
		g_task_return_error (task, error);
	}

	g_object_unref (task);
	g_clear_object (&reply);
}

void
tracker_bus_connection_perform_query_async (TrackerBusConnection *bus,
					    const gchar          *sparql,
					    GVariant             *arguments,
					    GCancellable         *cancellable,
					    GAsyncReadyCallback   callback,
					    gpointer              user_data)
{
	GDBusMessage *message;
	GUnixFDList *fd_list;
	GInputStream *istream;
	GError *error = NULL;
	GTask *task;
	int fd_idx;

	task = g_task_new (bus, cancellable, callback, user_data);

	if (!create_pipe_for_read (&istream, &fd_list, &fd_idx, &error)) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	g_task_set_task_data (task, istream, g_object_unref);

	message = create_query_message (bus, sparql, arguments,
					fd_list, fd_idx);
	g_dbus_connection_send_message_with_reply (bus->dbus_conn,
	                                           message,
	                                           G_DBUS_SEND_MESSAGE_FLAGS_NONE,
						   G_MAXINT,
	                                           NULL,
	                                           cancellable,
	                                           query_dbus_call_cb,
	                                           task);
	g_object_unref (message);
	g_object_unref (fd_list);
}

TrackerSparqlCursor *
tracker_bus_connection_perform_query_finish (TrackerBusConnection  *conn,
					     GAsyncResult          *res,
					     GError               **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
perform_query_call_cb (GObject      *source,
		       GAsyncResult *res,
		       gpointer      user_data)
{
	AsyncData *data = user_data;

	data->retval = tracker_bus_connection_perform_query_finish (TRACKER_BUS_CONNECTION (source),
								    res, &data->error);
	g_main_loop_quit (data->loop);
}

TrackerSparqlCursor *
tracker_bus_connection_perform_query (TrackerBusConnection  *conn,
				      const gchar           *sparql,
				      GVariant              *arguments,
				      GCancellable          *cancellable,
				      GError               **error)
{
	GMainContext *context;
	AsyncData data = { 0, };

	context = g_main_context_new ();
	data.loop = g_main_loop_new (context, FALSE);
	g_main_context_push_thread_default (context);

	tracker_bus_connection_perform_query_async (conn,
						    sparql,
						    arguments,
						    cancellable,
						    perform_query_call_cb,
						    &data);
	g_main_loop_run (data.loop);

	g_main_context_pop_thread_default (context);

	g_main_loop_unref (data.loop);
	g_main_context_unref (context);

	if (data.error) {
		g_propagate_error (error, data.error);
		return NULL;
	}

	return TRACKER_SPARQL_CURSOR (data.retval);
}

static void
serialize_call_cb (GObject      *source,
		   GAsyncResult *res,
		   gpointer      user_data)
{
	GTask *task = user_data;
	GDBusMessage *reply;
	GError *error = NULL;

	reply = g_dbus_connection_send_message_with_reply_finish (G_DBUS_CONNECTION (source),
	                                                          res, &error);
	if (reply && !g_dbus_message_to_gerror (reply, &error)) {
		GInputStream *istream;

		istream = g_task_get_task_data (task);
		g_task_return_pointer (task, g_object_ref (istream), g_object_unref);
		g_object_unref (task);
	} else {
		g_dbus_error_strip_remote_error (error);
		g_task_return_error (task, error);
		g_object_unref (task);
	}

	g_clear_object (&reply);
}

void
tracker_bus_connection_perform_serialize_async (TrackerBusConnection  *bus,
						TrackerSerializeFlags  flags,
						TrackerRdfFormat       format,
						const gchar           *query,
						GVariant              *arguments,
						GCancellable          *cancellable,
						GAsyncReadyCallback    callback,
						gpointer               user_data)
{
	GDBusMessage *message;
	GUnixFDList *fd_list;
	GInputStream *istream;
	GError *error = NULL;
	GTask *task;
	int fd_idx;

	task = g_task_new (bus, cancellable, callback, user_data);

	if (!create_pipe_for_read (&istream, &fd_list, &fd_idx, &error)) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	g_task_set_task_data (task, istream, g_object_unref);
	message = create_serialize_message (bus, query, flags, format,
					    arguments, fd_list, fd_idx);
	g_dbus_connection_send_message_with_reply (bus->dbus_conn,
	                                           message,
	                                           G_DBUS_SEND_MESSAGE_FLAGS_NONE,
						   G_MAXINT,
	                                           NULL,
	                                           cancellable,
	                                           serialize_call_cb,
	                                           task);
	g_object_unref (message);
	g_object_unref (fd_list);
}

GInputStream *
tracker_bus_connection_perform_serialize_finish (TrackerBusConnection  *conn,
						 GAsyncResult          *res,
						 GError               **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
write_queries_cb (GObject      *object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
	GTask *task = user_data;
	UpdateTaskData *data;

	data = g_task_get_task_data (task);
	write_sparql_queries_finish (G_OUTPUT_STREAM (object),
	                             res, &data->write.error);
	data->write.finished = TRUE;
	check_finish_update (task);
}

void
tracker_bus_connection_perform_update_async (TrackerBusConnection  *self,
                                             GArray                *ops,
                                             GCancellable          *cancellable,
                                             GAsyncReadyCallback    callback,
                                             gpointer               user_data)
{
	UpdateTaskData *data;
	GUnixFDList *fd_list;
	GOutputStream *ostream;
	GError *error = NULL;
	GTask *task;
	int fd_idx;

	task = g_task_new (self, cancellable, callback, user_data);

	if (ops->len == 0) {
		g_task_return_pointer (task, NULL, NULL);
		g_object_unref (task);
		return;
	}

	data = g_new0 (UpdateTaskData, 1);
	g_task_set_task_data (task, data, update_task_data_free);

	if (!create_pipe_for_write (&ostream, &fd_list, &fd_idx, &error)) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	perform_update_async (TRACKER_BUS_CONNECTION (self),
	                      "UpdateArray",
	                      fd_list, fd_idx,
	                      cancellable,
	                      update_cb,
	                      task);

	write_sparql_queries_async (ostream, ops,
	                            cancellable, write_queries_cb,
	                            task);

	g_object_unref (ostream);
	g_object_unref (fd_list);
}

gboolean
tracker_bus_connection_perform_update_finish (TrackerBusConnection  *self,
                                              GAsyncResult          *res,
                                              GError               **error)
{
	GError *inner_error = NULL;
	GVariant *retval;

	retval = g_task_propagate_pointer (G_TASK (res), &inner_error);
	g_clear_pointer (&retval, g_variant_unref);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}
