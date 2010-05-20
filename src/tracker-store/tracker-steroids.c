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
#include <gio/gunixoutputstream.h>
#include <gio/gunixinputstream.h>

#include "tracker-dbus.h"
#include "tracker-steroids.h"
#include "tracker-store.h"

#define DBUS_ERROR_UNKNOWN_METHOD_NAME "org.freedesktop.DBus.Error.UnknownMethod"
#define DBUS_ERROR_UNKNOWN_METHOD_MESSAGE "Method \"%s\" with signature \"%s\" on interface \"%s\" doesn't exist"

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

#define TRACKER_STEROIDS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_STEROIDS, TrackerSteroidsPrivate))

typedef struct {
	GHashTable *clients;
} TrackerSteroidsPrivate;

typedef struct {
	TrackerSteroids *parent;
	DBusMessage *call_message;
	int fd;
	unsigned int send_buffer_index;
	char send_buffer[TRACKER_STEROIDS_BUFFER_SIZE];
	guint request_id;
	DBusConnection *connection;
} ClientInfo;

typedef struct {
	GError *error;
	gpointer user_data;
} InThreadPtr;

static void tracker_steroids_finalize (GObject *object);

static void
tracker_steroids_class_init (TrackerSteroidsClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_steroids_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerSteroidsPrivate));
}

static void
tracker_steroids_init (TrackerSteroids *object)
{
	TrackerSteroidsPrivate *priv = TRACKER_STEROIDS_GET_PRIVATE (object);

	priv->clients = g_hash_table_new (g_direct_hash, g_direct_equal);
}

TrackerSteroids*
tracker_steroids_new (void)
{
	return g_object_new (TRACKER_TYPE_STEROIDS, NULL);
}

static void
destroy_client_info (gpointer user_data)
{
	ClientInfo *info = user_data;

	dbus_message_unref (info->call_message);
	dbus_connection_unref (info->connection);

	if (info->fd) {
		close (info->fd);
	}

	g_slice_free (ClientInfo, user_data);
}

static void
query_callback (gpointer  inthread_data,
                GError   *error,
                gpointer  user_data)
{
	InThreadPtr *ptr  = inthread_data;
	ClientInfo  *info = user_data;
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
		tracker_dbus_request_success (info->request_id,
		                              NULL);
		reply = dbus_message_new_method_return (info->call_message);
		dbus_connection_send (info->connection, reply, NULL);
		dbus_message_unref (reply);
	}

	if (ptr) {
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

	tracker_dbus_request_success (info->request_id,
	                              NULL);
	reply = dbus_message_new_method_return (info->call_message);
	dbus_connection_send (info->connection, reply, NULL);
	dbus_message_unref (reply);
}

static void
marshall_hash_table_item (gpointer key, gpointer value, gpointer user_data)
{
	DBusMessageIter *iter = user_data;
	DBusMessageIter subiter;

	dbus_message_iter_open_container (iter, DBUS_TYPE_DICT_ENTRY, NULL, &subiter);
	dbus_message_iter_append_basic (&subiter, DBUS_TYPE_STRING, &key);
	dbus_message_iter_append_basic (&subiter, DBUS_TYPE_STRING, &value);
	dbus_message_iter_close_container (iter, &subiter);
}

static void
marshall_hash_table (DBusMessageIter *iter, GHashTable *hash)
{
	DBusMessageIter subiter;

	dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, "{ss}", &subiter);
	g_hash_table_foreach (hash, marshall_hash_table_item, &subiter);
	dbus_message_iter_close_container (iter, &subiter);
}

static void
update_blank_callback (GPtrArray *blank_nodes, GError *error, gpointer user_data)
{
	ClientInfo *info = user_data;
	DBusMessage *reply;
	int i, j;
	/* Reply type is aaa{ss} */
	DBusMessageIter iter;
	DBusMessageIter subiter;
	DBusMessageIter subsubiter;

	if (info->fd) {
		close (info->fd);
	}

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

	tracker_dbus_request_success (info->request_id,
	                              NULL);
	reply = dbus_message_new_method_return (info->call_message);
	dbus_message_iter_init_append (reply, &iter);

	dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY, "aa{ss}", &subiter);
	for (i = 0; i < blank_nodes->len; i++) {
		GPtrArray *inner_array;
		inner_array = g_ptr_array_index (blank_nodes, i);

		dbus_message_iter_open_container (&subiter, DBUS_TYPE_ARRAY, "a{ss}", &subsubiter);
		for (j = 0; j < inner_array->len; j++) {
			GHashTable *hash = g_ptr_array_index (inner_array, j);

			marshall_hash_table (&subsubiter, hash);
		}
		dbus_message_iter_close_container (&subiter, &subsubiter);
	}
	dbus_message_iter_close_container (&iter, &subiter);

	dbus_connection_send (info->connection, reply, NULL);
	dbus_message_unref (reply);
}

static gpointer
query_inthread (TrackerDBCursor *cursor,
                GError          *error,
                gpointer         user_data)
{
	InThreadPtr *ptr  = g_slice_new0 (InThreadPtr);
	ClientInfo  *info = user_data;
	GError *loop_error = NULL;
	GOutputStream *output_stream;
	GDataOutputStream *data_output_stream;
	guint n_columns;
	int *column_sizes;
	int *column_offsets;
	const gchar **column_data;

	output_stream = g_buffered_output_stream_new_sized (g_unix_output_stream_new (info->fd, TRUE),
	                                                    TRACKER_STEROIDS_BUFFER_SIZE);
	data_output_stream = g_data_output_stream_new (output_stream);

	if (error) {
		g_data_output_stream_put_int32 (data_output_stream,
		                                TRACKER_STEROIDS_RC_ERROR,
		                                NULL,
		                                NULL);
		g_object_unref (data_output_stream);
		g_object_unref (output_stream);
		ptr->error = g_error_copy (error);
		return ptr;
	}

	n_columns = tracker_db_cursor_get_n_columns (cursor);

	column_sizes = g_malloc (n_columns * sizeof (int));
	column_offsets = g_malloc (n_columns * sizeof (int));
	column_data = g_malloc (n_columns * sizeof (char*));

	while (tracker_db_cursor_iter_next (cursor, &loop_error)) {
		int i;
		guint last_offset = -1;

		if (loop_error != NULL) {
			goto end_query_inthread;
		}

		for (i = 0; i < n_columns ; i++) {
			const gchar *str;

			str = tracker_db_cursor_get_string (cursor, i);

			column_sizes[i] = str ? strlen (str) : 0;
			column_data[i]  = str;

			last_offset += column_sizes[i] + 1;
			column_offsets[i] = last_offset;
		}

		g_data_output_stream_put_int32 (data_output_stream,
		                                TRACKER_STEROIDS_RC_ROW,
		                                NULL,
		                                &loop_error);

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

	if (!loop_error) {
		g_data_output_stream_put_int32 (data_output_stream,
		                                TRACKER_STEROIDS_RC_DONE,
		                                NULL,
		                                &loop_error);
	}

	/* Will force flushing */
	g_object_unref (data_output_stream);
	g_object_unref (output_stream);

	if (loop_error) {
		ptr->error = loop_error;
	}

	g_free (column_sizes);
	g_free (column_offsets);
	g_free (column_data);

	return ptr;
}

static void
tracker_steroids_query (TrackerSteroids *steroids,
						DBusConnection  *connection,
						DBusMessage     *message)
{
	ClientInfo  *info;
	guint        request_id;
	const gchar *sender;
	DBusMessage *reply;
	DBusError    dbus_error;
	gchar       *query;

	if (g_strcmp0 (dbus_message_get_signature (message), DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_UNIX_FD_AS_STRING)) {
		reply = dbus_message_new_error_printf (message,
		                                       DBUS_ERROR_UNKNOWN_METHOD_NAME,
		                                       DBUS_ERROR_UNKNOWN_METHOD_MESSAGE,
		                                       "Query",
		                                       dbus_message_get_signature (message),
		                                       dbus_message_get_interface (message));
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);
		return;
	}

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
	                          NULL,
	                          "%s()",
	                          __FUNCTION__);

	info = g_slice_new0 (ClientInfo);
	info->parent = steroids;
	info->connection = dbus_connection_ref (connection);
	info->call_message = dbus_message_ref (message);
	info->request_id = request_id;

	dbus_error_init (&dbus_error);

	dbus_message_get_args (message,
	                       &dbus_error,
	                       DBUS_TYPE_STRING, &query,
	                       DBUS_TYPE_UNIX_FD, &info->fd,
	                       DBUS_TYPE_INVALID);

	sender = dbus_message_get_sender (message);

	tracker_store_sparql_query (query, TRACKER_STORE_PRIORITY_HIGH,
	                            query_inthread, query_callback, sender,
	                            info, destroy_client_info);
}

static void
tracker_steroids_update (TrackerSteroids *steroids,
                         DBusConnection  *connection,
                         DBusMessage     *message,
                         gboolean         batch,
                         gboolean         update_blank)
{
	DBusError               dbus_error;
	ClientInfo             *info;
	GInputStream           *input_stream;
	GDataInputStream       *data_input_stream;
	GError                 *error = NULL;
	gsize                   bytes_read;
	guint                   request_id;
	const gchar            *sender;
	int                     query_size;
	DBusMessage            *reply;
	gchar                  *query;

	if (g_strcmp0 (dbus_message_get_signature (message), DBUS_TYPE_UNIX_FD_AS_STRING)) {
		reply = dbus_message_new_error_printf (message,
		                                       DBUS_ERROR_UNKNOWN_METHOD_NAME,
		                                       DBUS_ERROR_UNKNOWN_METHOD_MESSAGE,
		                                       "Update",
		                                       dbus_message_get_signature (message),
		                                       dbus_message_get_interface (message));
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);
		return;
	}

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
	                          NULL,
	                          "%s()",
	                          __FUNCTION__);

	info = g_slice_new0 (ClientInfo);
	info->parent = steroids;
	info->connection = dbus_connection_ref (connection);
	info->call_message = dbus_message_ref (message);

	dbus_error_init (&dbus_error);

	dbus_message_get_args (message,
	                       &dbus_error,
	                       DBUS_TYPE_UNIX_FD, &info->fd,
	                       DBUS_TYPE_INVALID);

	info->request_id = request_id;

	sender = dbus_message_get_sender (message);

	input_stream = g_buffered_input_stream_new_sized (g_unix_input_stream_new (info->fd, TRUE),
	                                                  TRACKER_STEROIDS_BUFFER_SIZE);
	data_input_stream = g_data_input_stream_new (input_stream);

	query_size = g_data_input_stream_read_int32 (data_input_stream,
	                                             NULL,
	                                             &error);

	if (error) {
		reply = dbus_message_new_error (info->call_message,
		                                TRACKER_STEROIDS_INTERFACE ".UpdateError",
		                                error->message);
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);

		g_object_unref (data_input_stream);
		g_object_unref (input_stream);
		g_error_free (error);
		destroy_client_info (info);

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

		g_free (query);
		g_object_unref (data_input_stream);
		g_object_unref (input_stream);
		g_error_free (error);
		destroy_client_info (info);

		return;
	}

	g_object_unref (data_input_stream);
	g_object_unref (input_stream);

	if (update_blank) {
		tracker_store_sparql_update_blank (query, TRACKER_STORE_PRIORITY_HIGH,
		                                   update_blank_callback, sender,
		                                   info, destroy_client_info);
	} else {
		tracker_store_sparql_update (query,
		                             batch ? TRACKER_STORE_PRIORITY_LOW : TRACKER_STORE_PRIORITY_HIGH,
		                             FALSE,
		                             update_callback, sender,
		                             info, destroy_client_info);
	}
}

DBusHandlerResult
tracker_steroids_connection_filter (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data)
{
	TrackerSteroids *steroids = user_data;

	g_return_val_if_fail (TRACKER_IS_STEROIDS (steroids), DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (g_strcmp0 (TRACKER_STEROIDS_PATH, dbus_message_get_path (message))) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (g_strcmp0 (TRACKER_STEROIDS_INTERFACE, dbus_message_get_interface (message))) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (!g_strcmp0 ("Query", dbus_message_get_member (message))) {
		tracker_steroids_query (steroids, connection, message);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!g_strcmp0 ("Update", dbus_message_get_member (message))) {
		tracker_steroids_update (steroids, connection, message, FALSE, FALSE);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!g_strcmp0 ("UpdateBlank", dbus_message_get_member (message))) {
		tracker_steroids_update (steroids, connection, message, FALSE, TRUE);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!g_strcmp0 ("BatchUpdate", dbus_message_get_member (message))) {
		tracker_steroids_update (steroids, connection, message, TRUE, FALSE);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
tracker_steroids_finalize (GObject *object)
{
	TrackerSteroidsPrivate *priv = TRACKER_STEROIDS_GET_PRIVATE (object);

	g_hash_table_unref (priv->clients);
}
