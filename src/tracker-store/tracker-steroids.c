/*
 * Copyright (C) 2010, Codeminded BVBA <abustany@gnome.org>
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

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <alloca.h>

#include <dbus/dbus.h>

#include <gio/gunixoutputstream.h>
#include <gio/gunixinputstream.h>

#include "tracker-dbus.h"
#include "tracker-steroids.h"
#include "tracker-store.h"

#define UNKNOWN_METHOD_MESSAGE "Method \"%s\" with signature \"%s\" on " \
                               "interface \"%s\" doesn't exist, expected \"%s\""

/**
 * /!\ IMPORTANT WARNING /!\
 *
 * DBus 1.3 is required for this feature to work, since UNIX FD passing is not
 * present in earlier versions. However, using UNIX FD passing with DBus will
 * make Valgrind stop, since the fcntl command F_DUPFD_CLOEXEC is not supported
 * as of version 3.5.
 * This has been reported here: https://bugs.kde.org/show_bug.cgi?id=238696
 */

G_DEFINE_TYPE (TrackerSteroids, tracker_steroids, G_TYPE_OBJECT)

typedef struct {
	DBusMessage *call_message;
	int fd;
	guint request_id;
	DBusConnection *connection;
	struct {
		int query_count;
		int seen;
		GPtrArray *errors;
	} array_info;
} ClientInfo;

typedef struct {
	GError *error;
	gpointer user_data;
	GStrv variable_names;
} InThreadPtr;

static void
tracker_steroids_class_init (TrackerSteroidsClass *klass)
{
}

static void
tracker_steroids_init (TrackerSteroids *object)
{
}

TrackerSteroids*
tracker_steroids_new (void)
{
	return g_object_new (TRACKER_TYPE_STEROIDS, NULL);
}

static void
client_info_destroy (gpointer user_data)
{
	ClientInfo *info = user_data;

	dbus_message_unref (info->call_message);
	dbus_connection_unref (info->connection);

	g_slice_free (ClientInfo, user_data);
}

static void
query_callback (gpointer  inthread_data,
                GError   *error,
                gpointer  user_data)
{
	InThreadPtr *ptr = inthread_data;
	ClientInfo *info = user_data;
	DBusMessage *reply;

	if (ptr && ptr->error) {
		/* Client is still there, but query failed */
		tracker_dbus_request_failed (info->request_id,
		                             NULL,
		                             &ptr->error,
		                             NULL);
		reply = dbus_message_new_error (info->call_message,
		                                TRACKER_STEROIDS_INTERFACE ".QueryError",
		                                ptr->error->message);
		dbus_connection_send (info->connection, reply, NULL);
		dbus_message_unref (reply);
		g_error_free (ptr->error);
	} else if (error) {
		/* Client has disappeared */
		tracker_dbus_request_failed (info->request_id,
		                             NULL,
		                             &error,
		                             NULL);
		reply = dbus_message_new_error (info->call_message,
		                                TRACKER_STEROIDS_INTERFACE ".QueryError",
		                                error->message);
		dbus_connection_send (info->connection, reply, NULL);
		dbus_message_unref (reply);
	} else {
		GStrv variable_names = ptr->variable_names;
		DBusMessageIter iter, subiter;
		guint i;

		tracker_dbus_request_success (info->request_id,
		                              NULL);
		reply = dbus_message_new_method_return (info->call_message);

		dbus_message_iter_init_append (reply, &iter);

		dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY, "s", &subiter);
		for (i = 0; variable_names[i] != NULL; i++) {
			gchar *variable_name = variable_names[i];
			dbus_message_iter_append_basic (&subiter, DBUS_TYPE_STRING, &variable_name);
		}
		dbus_message_iter_close_container (&iter, &subiter);

		dbus_connection_send (info->connection, reply, NULL);
		dbus_message_unref (reply);
	}

	if (ptr) {
		if (ptr->variable_names) {
			g_strfreev (ptr->variable_names);
		}
		g_slice_free (InThreadPtr, ptr);
	}
}

static void
update_callback (GError *error, gpointer user_data)
{
	ClientInfo *info = user_data;
	DBusMessage *reply;

	if (error) {
		tracker_dbus_request_failed (info->request_id,
		                             NULL,
		                             &error,
		                             NULL);
		reply = dbus_message_new_error (info->call_message,
		                                TRACKER_STEROIDS_INTERFACE ".UpdateError",
		                                error->message);
		dbus_connection_send (info->connection, reply, NULL);
		dbus_message_unref (reply);
		return;
	}

	tracker_dbus_request_success (info->request_id, NULL);
	reply = dbus_message_new_method_return (info->call_message);
	dbus_connection_send (info->connection, reply, NULL);
	dbus_message_unref (reply);
}

static void
update_array_callback (GError *error, gpointer user_data)
{
	ClientInfo *info = user_data;
	DBusMessage *reply;

	info->array_info.seen++;

	if (!info->array_info.errors)
		info->array_info.errors = g_ptr_array_new ();

	if (error) {
		g_ptr_array_add (info->array_info.errors, g_error_copy (error));
	} else {
		g_ptr_array_add (info->array_info.errors, NULL);
	}

	if (info->array_info.seen == info->array_info.query_count) {
		guint i;
		DBusMessageIter iter, subiter;

		tracker_dbus_request_success (info->request_id, NULL);
		reply = dbus_message_new_method_return (info->call_message);

		dbus_message_iter_init_append (reply, &iter);
		dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY, "ss", &subiter);

		for (i = 0; i < info->array_info.errors->len; i++) {
			GError *error = g_ptr_array_index (info->array_info.errors, i);
			const gchar *str = "";
			const gchar *message = "";

			if (error) {
				str = TRACKER_STEROIDS_INTERFACE ".UpdateError";
				message = error->message;
			}

			dbus_message_iter_append_basic (&subiter, DBUS_TYPE_STRING, &str);
			dbus_message_iter_append_basic (&subiter, DBUS_TYPE_STRING, &message);

			if (error)
				g_error_free (error);
		}
		g_ptr_array_free (info->array_info.errors, TRUE);

		dbus_message_iter_close_container (&iter, &subiter);

		dbus_connection_send (info->connection, reply, NULL);
		dbus_message_unref (reply);

		client_info_destroy (info);
	}
}

static void
marshal_hash_table_item (gpointer key,
                         gpointer value,
                         gpointer user_data)
{
	DBusMessageIter *iter = user_data;
	DBusMessageIter subiter;

	dbus_message_iter_open_container (iter, DBUS_TYPE_DICT_ENTRY, NULL, &subiter);
	dbus_message_iter_append_basic (&subiter, DBUS_TYPE_STRING, &key);
	dbus_message_iter_append_basic (&subiter, DBUS_TYPE_STRING, &value);
	dbus_message_iter_close_container (iter, &subiter);
}

static void
marshal_hash_table (DBusMessageIter *iter, GHashTable *hash)
{
	DBusMessageIter subiter;

	dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, "{ss}", &subiter);
	g_hash_table_foreach (hash, marshal_hash_table_item, &subiter);
	dbus_message_iter_close_container (iter, &subiter);
}

static void
update_blank_callback (GPtrArray *blank_nodes,
                       GError    *error,
                       gpointer   user_data)
{
	ClientInfo *info;
	DBusMessage *reply;
	gint i, j;
	/* Reply type is aaa{ss} */
	DBusMessageIter iter;
	DBusMessageIter subiter;
	DBusMessageIter subsubiter;

	info = user_data;

	if (error) {
		tracker_dbus_request_failed (info->request_id,
		                             NULL,
		                             &error,
		                             NULL);
		reply = dbus_message_new_error (info->call_message,
		                                TRACKER_STEROIDS_INTERFACE ".UpdateBlankError",
		                                error->message);
		dbus_connection_send (info->connection, reply, NULL);
		dbus_message_unref (reply);
		return;
	}

	tracker_dbus_request_success (info->request_id, NULL);
	reply = dbus_message_new_method_return (info->call_message);
	dbus_message_iter_init_append (reply, &iter);

	dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY, "aa{ss}", &subiter);
	for (i = 0; i < blank_nodes->len; i++) {
		GPtrArray *inner_array;

		inner_array = g_ptr_array_index (blank_nodes, i);

		dbus_message_iter_open_container (&subiter, DBUS_TYPE_ARRAY, "a{ss}", &subsubiter);
		for (j = 0; j < inner_array->len; j++) {
			GHashTable *hash = g_ptr_array_index (inner_array, j);

			marshal_hash_table (&subsubiter, hash);
		}
		dbus_message_iter_close_container (&subiter, &subsubiter);
	}
	dbus_message_iter_close_container (&iter, &subiter);

	dbus_connection_send (info->connection, reply, NULL);
	dbus_message_unref (reply);
}

static gpointer
query_inthread (TrackerDBCursor *cursor,
                GCancellable    *cancellable,
                GError          *error,
                gpointer         user_data)
{
	InThreadPtr *ptr;
	ClientInfo *info;
	GError *loop_error = NULL;
	GOutputStream *unix_output_stream;
	GOutputStream *output_stream;
	GDataOutputStream *data_output_stream;
	guint n_columns;
	gint *column_sizes;
	gint *column_offsets;
	gint *column_types;
	const gchar **column_data;
	guint i;
	GStrv variable_names = NULL;

	ptr = g_slice_new0 (InThreadPtr);
	info = user_data;
	unix_output_stream = g_unix_output_stream_new (info->fd, TRUE);
	output_stream = g_buffered_output_stream_new_sized (unix_output_stream,
	                                                    TRACKER_STEROIDS_BUFFER_SIZE);
	data_output_stream = g_data_output_stream_new (output_stream);
	g_data_output_stream_set_byte_order (data_output_stream, G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN);

	if (error) {
		g_object_unref (data_output_stream);
		g_object_unref (output_stream);
		g_object_unref (unix_output_stream);
		ptr->error = g_error_copy (error);
		return ptr;
	}

	n_columns = tracker_db_cursor_get_n_columns (cursor);

	column_sizes = alloca (n_columns * sizeof (gint));
	column_offsets = alloca (n_columns * sizeof (gint));
	column_data = alloca (n_columns * sizeof (gchar*));
	column_types = alloca (n_columns * sizeof (gchar*));

	/* n_columns + 1 for NULL termination */
	variable_names = g_new0 (gchar *, n_columns + 1);
	for (i = 0; i < n_columns; i++) {
		variable_names[i] = g_strdup (tracker_db_cursor_get_variable_name (cursor, i));
	}

	while (tracker_db_cursor_iter_next (cursor, cancellable, &loop_error)) {
		gint i;
		guint last_offset = -1;

		if (loop_error != NULL) {
			goto end_query_inthread;
		}

		for (i = 0; i < n_columns ; i++) {
			const gchar *str;

			str = tracker_db_cursor_get_string (cursor, i, NULL);

			column_sizes[i] = str ? strlen (str) : 0;
			column_data[i]  = str;

			/* Cast from enum to int */
			column_types[i] = (gint) tracker_db_cursor_get_value_type (cursor, i);

			last_offset += column_sizes[i] + 1;
			column_offsets[i] = last_offset;
		}

		if (loop_error) {
			goto end_query_inthread;
		}

		g_data_output_stream_put_int32 (data_output_stream,
		                                n_columns,
		                                NULL,
		                                &loop_error);

		if (loop_error) {
			goto end_query_inthread;
		}

		for (i = 0; i < n_columns; i++) {
			g_data_output_stream_put_int32 (data_output_stream,
			                                column_types[i],
			                                NULL,
			                                &loop_error);
			if (loop_error) {
				goto end_query_inthread;
			}
		}

		for (i = 0; i < n_columns; i++) {
			g_data_output_stream_put_int32 (data_output_stream,
			                                column_offsets[i],
			                                NULL,
			                                &loop_error);
			if (loop_error) {
				goto end_query_inthread;
			}
		}

		for (i = 0; i < n_columns; i++) {
			g_data_output_stream_put_string (data_output_stream,
			                                 column_data[i] ? column_data[i] : "",
			                                 NULL,
			                                 &loop_error);

			if (loop_error) {
				goto end_query_inthread;
			}

			g_data_output_stream_put_byte (data_output_stream,
			                               0,
			                               NULL,
			                               &loop_error);

			if (loop_error) {
				goto end_query_inthread;
			}
		}
	}

end_query_inthread:
	/* Will force flushing */
	g_object_unref (data_output_stream);
	g_object_unref (output_stream);
	g_object_unref (unix_output_stream);

	if (loop_error) {
		ptr->error = loop_error;
		if (variable_names) {
			g_strfreev (variable_names);
		}
	} else {
		ptr->variable_names = variable_names;
	}

	return ptr;
}

static void
steroids_query (TrackerSteroids *steroids,
                DBusConnection  *connection,
                DBusMessage     *message)
{
	ClientInfo *info;
	guint request_id;
	const gchar *sender;
	const gchar *expected_signature;
	DBusMessage *reply;
	DBusError dbus_error;
	gchar *query;
	int fd;

	request_id = tracker_dbus_get_next_request_id ();

	expected_signature = DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_UNIX_FD_AS_STRING;

	if (g_strcmp0 (dbus_message_get_signature (message), expected_signature)) {
		tracker_dbus_request_new (request_id,
		                          NULL,
		                          "%s()",
		                          __FUNCTION__);

		reply = dbus_message_new_error_printf (message,
		                                       DBUS_ERROR_UNKNOWN_METHOD,
		                                       UNKNOWN_METHOD_MESSAGE,
		                                       "Query",
		                                       dbus_message_get_signature (message),
		                                       dbus_message_get_interface (message),
		                                       expected_signature);
		dbus_connection_send (connection, reply, NULL);

		tracker_dbus_request_failed (request_id,
		                             NULL,
		                             NULL,
		                             UNKNOWN_METHOD_MESSAGE,
		                             "Query",
		                             dbus_message_get_signature (message),
		                             dbus_message_get_interface (message),
		                             expected_signature);

		dbus_message_unref (reply);

		return;
	}

	dbus_error_init (&dbus_error);

	dbus_message_get_args (message,
	                       &dbus_error,
	                       DBUS_TYPE_STRING, &query,
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

	info = g_slice_new0 (ClientInfo);
	info->connection = dbus_connection_ref (connection);
	info->call_message = dbus_message_ref (message);
	info->request_id = request_id;
	info->fd = fd;

	tracker_dbus_request_new (request_id,
	                          NULL,
	                          "%s(query:'%s')",
	                          __FUNCTION__,
	                          query);

	sender = dbus_message_get_sender (message);

	tracker_store_sparql_query (query,
	                            TRACKER_STORE_PRIORITY_HIGH,
	                            query_inthread,
	                            query_callback,
	                            sender,
	                            info,
	                            client_info_destroy);
}

static void
steroids_update (TrackerSteroids *steroids,
                 DBusConnection  *connection,
                 DBusMessage     *message,
                 gboolean         batch,
                 gboolean         update_blank)
{
	DBusError dbus_error;
	ClientInfo *info;
	GInputStream *input_stream;
	GDataInputStream *data_input_stream;
	GError *error = NULL;
	gsize bytes_read;
	guint request_id;
	const gchar *sender;
	int query_size;
	DBusMessage *reply;
	gchar *query;
	int fd;

	request_id = tracker_dbus_get_next_request_id ();

	if (g_strcmp0 (dbus_message_get_signature (message), DBUS_TYPE_UNIX_FD_AS_STRING)) {
		tracker_dbus_request_new (request_id,
		                          NULL,
		                          "%s()",
		                          __FUNCTION__);

		reply = dbus_message_new_error_printf (message,
		                                       DBUS_ERROR_UNKNOWN_METHOD,
		                                       UNKNOWN_METHOD_MESSAGE,
		                                       "Update",
		                                       dbus_message_get_signature (message),
		                                       dbus_message_get_interface (message),
		                                       DBUS_TYPE_UNIX_FD_AS_STRING);
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);

		tracker_dbus_request_failed (request_id,
		                             NULL,
		                             NULL,
		                             UNKNOWN_METHOD_MESSAGE,
		                             "Update",
		                             dbus_message_get_signature (message),
		                             dbus_message_get_interface (message),
		                             DBUS_TYPE_UNIX_FD_AS_STRING);

		return;
	}

	dbus_error_init (&dbus_error);

	dbus_message_get_args (message,
	                       &dbus_error,
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
	                          "%s(fd:%d)",
	                          __FUNCTION__,
	                          fd);

	info = g_slice_new0 (ClientInfo);
	info->connection = dbus_connection_ref (connection);
	info->call_message = dbus_message_ref (message);
	info->request_id = request_id;
	info->fd = fd;

	sender = dbus_message_get_sender (message);

	input_stream = g_unix_input_stream_new (info->fd, TRUE);
	data_input_stream = g_data_input_stream_new (input_stream);
	g_buffered_input_stream_set_buffer_size (G_BUFFERED_INPUT_STREAM (data_input_stream),
	                                         TRACKER_STEROIDS_BUFFER_SIZE);

	query_size = g_data_input_stream_read_int32 (data_input_stream,
	                                             NULL,
	                                             &error);

	if (error) {
		reply = dbus_message_new_error (info->call_message,
		                                TRACKER_STEROIDS_INTERFACE ".UpdateError",
		                                error->message);
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);

		tracker_dbus_request_failed (request_id,
		                             NULL,
		                             NULL,
		                             error->message);

		g_object_unref (data_input_stream);
		g_object_unref (input_stream);
		g_error_free (error);
		client_info_destroy (info);

		return;
	}

	/* We malloc one more char to ensure string is 0 terminated */
	query = g_malloc0 ((1 + query_size) * sizeof (char));

	g_input_stream_read_all (input_stream,
	                         query,
	                         query_size,
	                         &bytes_read,
	                         NULL,
	                         &error);

	if (error) {
		reply = dbus_message_new_error (info->call_message,
		                                TRACKER_STEROIDS_INTERFACE ".UpdateError",
		                                error->message);
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);

		tracker_dbus_request_failed (request_id,
		                             NULL,
		                             NULL,
		                             error->message);

		g_free (query);
		g_object_unref (data_input_stream);
		g_object_unref (input_stream);
		g_error_free (error);
		client_info_destroy (info);

		return;
	}

	tracker_dbus_request_debug (request_id,
	                            NULL,
	                            "query: '%s'",
	                            query);

	g_object_unref (data_input_stream);
	g_object_unref (input_stream);

	if (update_blank) {
		tracker_store_sparql_update_blank (query,
		                                   TRACKER_STORE_PRIORITY_HIGH,
		                                   update_blank_callback,
		                                   sender,
		                                   info,
		                                   client_info_destroy);
	} else {
		tracker_store_sparql_update (query,
		                             batch ? TRACKER_STORE_PRIORITY_LOW : TRACKER_STORE_PRIORITY_HIGH,
		                             update_callback,
		                             sender,
		                             info,
		                             client_info_destroy);
	}

	g_free (query);
}


static void
steroids_update_array (TrackerSteroids *steroids,
                       DBusConnection  *connection,
                       DBusMessage     *message,
                       gboolean         batch)
{
	DBusError dbus_error;
	ClientInfo *info;
	GInputStream *input_stream;
	GDataInputStream *data_input_stream;
	GError *error = NULL;
	guint request_id;
	const gchar *sender;
	int i;
	DBusMessage *reply;
	int fd;
	gchar **query_array;

	request_id = tracker_dbus_get_next_request_id ();

	if (g_strcmp0 (dbus_message_get_signature (message), DBUS_TYPE_UNIX_FD_AS_STRING)) {
		tracker_dbus_request_new (request_id,
		                          NULL,
		                          "%s()",
		                          __FUNCTION__);

		reply = dbus_message_new_error_printf (message,
		                                       DBUS_ERROR_UNKNOWN_METHOD,
		                                       UNKNOWN_METHOD_MESSAGE,
		                                       "Update",
		                                       dbus_message_get_signature (message),
		                                       dbus_message_get_interface (message),
		                                       DBUS_TYPE_UNIX_FD_AS_STRING);
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);

		tracker_dbus_request_failed (request_id,
		                             NULL,
		                             NULL,
		                             UNKNOWN_METHOD_MESSAGE,
		                             "Update",
		                             dbus_message_get_signature (message),
		                             dbus_message_get_interface (message),
		                             DBUS_TYPE_UNIX_FD_AS_STRING);

		return;
	}

	dbus_error_init (&dbus_error);

	dbus_message_get_args (message,
	                       &dbus_error,
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
	                          "%s(fd:%d)",
	                          __FUNCTION__,
	                          fd);

	info = g_slice_new0 (ClientInfo);
	info->connection = dbus_connection_ref (connection);
	info->call_message = dbus_message_ref (message);
	info->request_id = request_id;
	info->fd = fd;

	sender = dbus_message_get_sender (message);

	input_stream = g_unix_input_stream_new (info->fd, TRUE);
	data_input_stream = g_data_input_stream_new (input_stream);
	g_buffered_input_stream_set_buffer_size (G_BUFFERED_INPUT_STREAM (data_input_stream),
	                                         TRACKER_STEROIDS_BUFFER_SIZE);

	info->array_info.query_count = g_data_input_stream_read_uint32 (data_input_stream,
	                                                                NULL,
	                                                                &error);

	if (error) {
		reply = dbus_message_new_error (info->call_message,
		                                TRACKER_STEROIDS_INTERFACE ".UpdateError",
		                                error->message);
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);

		tracker_dbus_request_failed (request_id,
		                             NULL,
		                             NULL,
		                             error->message);

		g_object_unref (data_input_stream);
		g_object_unref (input_stream);
		g_error_free (error);
		client_info_destroy (info);

		return;
	}

	info->array_info.seen = 0;
	query_array = g_new0 (gchar*, info->array_info.query_count + 1);

	for (i = 0; i < info->array_info.query_count; i++) {
		gsize bytes_read;
		int query_size;

		query_size = g_data_input_stream_read_int32 (data_input_stream,
		                                             NULL,
		                                             &error);

		if (error) {
			reply = dbus_message_new_error (info->call_message,
			                                TRACKER_STEROIDS_INTERFACE ".UpdateError",
			                                error->message);
			dbus_connection_send (connection, reply, NULL);
			dbus_message_unref (reply);

			tracker_dbus_request_failed (request_id,
			                             NULL,
			                             NULL,
			                             error->message);

			g_strfreev (query_array);
			g_object_unref (data_input_stream);
			g_object_unref (input_stream);
			g_error_free (error);
			client_info_destroy (info);

			return;
		}

		/* We malloc one more char to ensure string is 0 terminated */
		query_array[i] = g_malloc0 ((1 + query_size) * sizeof (char));

		g_input_stream_read_all (input_stream,
		                         query_array[i],
		                         query_size,
		                         &bytes_read,
		                         NULL,
		                         &error);

		if (error) {
			reply = dbus_message_new_error (info->call_message,
			                                TRACKER_STEROIDS_INTERFACE ".UpdateError",
			                                error->message);
			dbus_connection_send (connection, reply, NULL);
			dbus_message_unref (reply);

			tracker_dbus_request_failed (request_id,
			                             NULL,
			                             NULL,
			                             error->message);

			g_strfreev (query_array);
			g_object_unref (data_input_stream);
			g_object_unref (input_stream);
			g_error_free (error);
			client_info_destroy (info);

			return;
		}

	}

	g_object_unref (data_input_stream);
	g_object_unref (input_stream);

	for (i = 0; query_array[i] != NULL; i++) {

		tracker_dbus_request_debug (request_id,
		                            NULL,
		                            "query: '%s'",
		                            query_array[i]);

		tracker_store_sparql_update (query_array[i],
		                             batch ? TRACKER_STORE_PRIORITY_LOW : TRACKER_STORE_PRIORITY_HIGH,
		                             update_array_callback,
		                             sender,
		                             info,
		                             NULL);
	}

	g_strfreev (query_array);
}

DBusHandlerResult
tracker_steroids_connection_filter (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data)
{
	TrackerSteroids *steroids;

	g_return_val_if_fail (connection != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
	g_return_val_if_fail (message != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (g_strcmp0 (TRACKER_STEROIDS_PATH, dbus_message_get_path (message))) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (g_strcmp0 (TRACKER_STEROIDS_INTERFACE, dbus_message_get_interface (message))) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	/* Only check if the user_data is our TrackerSteroids AFTER having checked that
	 * the message matches expected path and interface. */
	steroids = user_data;
	g_return_val_if_fail (TRACKER_IS_STEROIDS (steroids), DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (!g_strcmp0 ("Query", dbus_message_get_member (message))) {
		steroids_query (steroids, connection, message);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!g_strcmp0 ("UpdateArray", dbus_message_get_member (message))) {
		steroids_update_array (steroids, connection, message, FALSE);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!g_strcmp0 ("Update", dbus_message_get_member (message))) {
		steroids_update (steroids, connection, message, FALSE, FALSE);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!g_strcmp0 ("UpdateBlank", dbus_message_get_member (message))) {
		steroids_update (steroids, connection, message, FALSE, TRUE);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!g_strcmp0 ("BatchUpdate", dbus_message_get_member (message))) {
		steroids_update (steroids, connection, message, TRUE, FALSE);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!g_strcmp0 ("BatchUpdateArray", dbus_message_get_member (message))) {
		steroids_update_array (steroids, connection, message, TRUE);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
