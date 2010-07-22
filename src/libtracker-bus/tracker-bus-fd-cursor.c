/*
 * Copyright (C) 2008-2010, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <libtracker-common/tracker-common.h>

#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-bus.h"
#include "tracker-bus-fd-cursor.h"

#ifdef HAVE_DBUS_FD_PASSING

#define TRACKER_TYPE_BUS_FD_CURSOR           (tracker_bus_fd_cursor_get_type ())
#define TRACKER_BUS_FD_CURSOR(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_BUS_FD_CURSOR, TrackerBusFDCursor))
#define TRACKER_BUS_FD_CURSOR_CLASS(c)       (G_TYPE_CHECK_CLASS_CAST ((c),      TRACKER_TYPE_BUS_FD_CURSOR, TrackerBusFDCursorClass))
#define TRACKER_IS_BUS_FD_CURSOR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_BUS_FD_CURSOR))
#define TRACKER_IS_BUS_FD_CURSOR_CLASS(c)    (G_TYPE_CHECK_CLASS_TYPE ((o),      TRACKER_TYPE_BUS_FD_CURSOR))
#define TRACKER_BUS_FD_CURSOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),  TRACKER_TYPE_BUS_FD_CURSOR, TrackerBusFDCursorClass))

typedef struct TrackerBusFDCursor TrackerBusFDCursor;
typedef struct TrackerBusFDCursorClass TrackerBusFDCursorClass;

struct TrackerBusFDCursor {
	TrackerSparqlCursor parent_instance;

	char *buffer;
	int buffer_index;
	long buffer_size;

	guint n_columns;
	int *offsets;
	char *data;
};

struct TrackerBusFDCursorClass {
	TrackerSparqlCursorClass parent_class;
};

GType tracker_bus_fd_cursor_get_type (void);
void  tracker_bus_fd_cursor_finalize (GObject *object);

G_DEFINE_TYPE (TrackerBusFDCursor, tracker_bus_fd_cursor, TRACKER_SPARQL_TYPE_CURSOR)

static void
tracker_bus_fd_cursor_rewind (TrackerBusFDCursor *cursor)
{
	cursor->buffer_index = 0;
	cursor->data = cursor->buffer;
}

static inline int
buffer_read_int (TrackerBusFDCursor *cursor)
{
	int v = *((int *)(cursor->buffer + cursor->buffer_index));

	cursor->buffer_index += 4;

	return v;
}


static gboolean
tracker_bus_fd_cursor_iter_next (TrackerBusFDCursor  *cursor,
                                 GCancellable        *cancellable,
                                 GError             **error)
{
	int last_offset;

	if (cursor->buffer_index >= cursor->buffer_size) {
		return FALSE;
	}

	/* So, the make up on each cursor segment is:
	 *
	 * iteration = [4 bytes for number of columns,
	 *              4 bytes for last offset]
	 */

	cursor->n_columns = buffer_read_int (cursor);
	cursor->offsets = (int *)(cursor->buffer + cursor->buffer_index);
	cursor->buffer_index += sizeof (int) * (cursor->n_columns - 1);

	last_offset = buffer_read_int (cursor);
	cursor->data = cursor->buffer + cursor->buffer_index;
	cursor->buffer_index += last_offset + 1;

	return TRUE;
}

static void
tracker_bus_fd_cursor_iter_next_thread (GSimpleAsyncResult *res,
                                        GObject            *object,
                                        GCancellable       *cancellable)
{
	/* This is stolen from the direct access work, 
	 * do we REALLY need to do this in the next thread? 
	 */

	GError *error = NULL;
	gboolean result;

	result = tracker_bus_fd_cursor_iter_next (TRACKER_BUS_FD_CURSOR (object), cancellable, &error);
	if (error) {
		g_simple_async_result_set_from_error (res, error);
	} else {
		g_simple_async_result_set_op_res_gboolean (res, result);
	}
}

static void
tracker_bus_fd_cursor_iter_next_async (TrackerBusFDCursor    *cursor,
                                       GCancellable          *cancellable,
                                       GAsyncReadyCallback    callback,
                                       gpointer               user_data)
{
	GSimpleAsyncResult *res;

	res = g_simple_async_result_new (G_OBJECT (cursor), callback, user_data, tracker_bus_fd_cursor_iter_next_async);
	g_simple_async_result_run_in_thread (res, tracker_bus_fd_cursor_iter_next_thread, 0, cancellable);
}

static gboolean
tracker_bus_fd_cursor_iter_next_finish (TrackerBusFDCursor *cursor,
                                        GAsyncResult       *res,
                                        GError            **error)
{
	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error)) {
		return FALSE;
	}
	return g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (res));
}

static guint
tracker_bus_fd_cursor_get_n_columns (TrackerBusFDCursor *cursor)
{
	return cursor->n_columns;
}

static const gchar *
tracker_bus_fd_cursor_get_string (TrackerBusFDCursor *cursor,
                                  guint               column,
                                  glong              *length)
{
	const gchar *str = NULL;

	if (length) {
		*length = 0;
	}

	g_return_val_if_fail (column < tracker_bus_fd_cursor_get_n_columns (cursor), NULL);

	if (column == 0) {
		str = cursor->data;
	} else {
		str = cursor->data + cursor->offsets[column - 1] + 1;
	}

	if (length) {
		*length = strlen (str);
	}

	return str;
}

static void
tracker_bus_fd_cursor_class_init (TrackerBusFDCursorClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	TrackerSparqlCursorClass *sparql_cursor_class = TRACKER_SPARQL_CURSOR_CLASS (class);

	object_class->finalize = tracker_bus_fd_cursor_finalize;

	sparql_cursor_class->get_n_columns = (gint (*) (TrackerSparqlCursor *)) tracker_bus_fd_cursor_get_n_columns;
	sparql_cursor_class->get_string = (const gchar * (*) (TrackerSparqlCursor *, gint, glong*)) tracker_bus_fd_cursor_get_string;
	sparql_cursor_class->next = (gboolean (*) (TrackerSparqlCursor *, GCancellable *, GError **)) tracker_bus_fd_cursor_iter_next;
	sparql_cursor_class->next_async = (void (*) (TrackerSparqlCursor *, GCancellable *, GAsyncReadyCallback, gpointer)) tracker_bus_fd_cursor_iter_next_async;
	sparql_cursor_class->next_finish = (gboolean (*) (TrackerSparqlCursor *, GAsyncResult *, GError **)) tracker_bus_fd_cursor_iter_next_finish;
	sparql_cursor_class->rewind = (void (*) (TrackerSparqlCursor *)) tracker_bus_fd_cursor_rewind;
}

void
tracker_bus_fd_cursor_init (TrackerBusFDCursor *cursor)
{
}

void
tracker_bus_fd_cursor_finalize (GObject *object)
{
	TrackerBusFDCursor *cursor;

	cursor = TRACKER_BUS_FD_CURSOR (object);

	g_free (cursor->buffer);

	G_OBJECT_CLASS (tracker_bus_fd_cursor_parent_class)->finalize (object);
}

/* Public API */

TrackerSparqlCursor *
tracker_bus_fd_query (DBusGConnection  *gconnection,
                      const gchar      *query,
                      GError          **error)
{
	DBusConnection *connection;
	DBusMessage *message;
	DBusMessageIter iter;
	TrackerBusFDCursor *cursor;
	GError *inner_error = NULL;
	int pipefd[2];

	g_return_val_if_fail (gconnection != NULL, NULL);
	g_return_val_if_fail (query != NULL, NULL);

	if (pipe (pipefd) < 0) {
		/* FIXME: Use proper error domain/code */
		g_set_error (error, 0, 0, "Cannot open pipe");
		return NULL;
	}

	connection = dbus_g_connection_get_connection (gconnection);

	message = dbus_message_new_method_call (TRACKER_DBUS_SERVICE,
	                                        TRACKER_DBUS_OBJECT_STEROIDS,
	                                        TRACKER_DBUS_INTERFACE_STEROIDS,
	                                        "Query");

	dbus_message_iter_init_append (message, &iter);
	dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &query);
	dbus_message_iter_append_basic (&iter, DBUS_TYPE_UNIX_FD, &pipefd[1]);
	close (pipefd[1]);

	cursor = g_object_new (TRACKER_TYPE_BUS_FD_CURSOR, NULL);

	tracker_dbus_send_and_splice (connection,
	                              message,
	                              pipefd[0],
	                              NULL,
	                              (void **) &cursor->buffer,
	                              &cursor->buffer_size,
	                              &inner_error);
	/* message is destroyed by tracker_dbus_send_and_splice */

	if (G_UNLIKELY (inner_error)) {
		g_propagate_error (error, inner_error);
		g_object_unref (cursor);
		cursor = NULL;
	}
	return TRACKER_SPARQL_CURSOR (cursor);
}

static void
query_async_cb (gpointer  buffer,
                gssize    buffer_size,
                GError   *error,
                gpointer  user_data)
{
	GSimpleAsyncResult *res;

	res = G_SIMPLE_ASYNC_RESULT (user_data);

	if (G_UNLIKELY (error)) {
		g_simple_async_result_set_from_error (res, error);
	} else {
		TrackerBusFDCursor *cursor;

		cursor = g_object_new (TRACKER_TYPE_BUS_FD_CURSOR, NULL);

		cursor->buffer = buffer;
		cursor->buffer_size = buffer_size;

		g_simple_async_result_set_op_res_gpointer (res, cursor, g_object_unref);
	}

	g_simple_async_result_complete (res);
	g_object_unref (res);
}

void
tracker_bus_fd_query_async (DBusGConnection     *gconnection,
                            const gchar         *query,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
	GSimpleAsyncResult *res;
	DBusConnection *connection;
	DBusMessage *message;
	DBusMessageIter iter;
	int pipefd[2];

	g_return_if_fail (gconnection != NULL);
	g_return_if_fail (query != NULL);

	if (pipe (pipefd) < 0) {
		/* FIXME: Use proper error domain/code */
		res = g_simple_async_result_new_error (NULL,
		                                       callback, user_data,
		                                       0, 0, "Cannot open pipe");
		g_simple_async_result_complete_in_idle (res);
		g_object_unref (res);
		return;
	}

	connection = dbus_g_connection_get_connection (gconnection);

	message = dbus_message_new_method_call (TRACKER_DBUS_SERVICE,
	                                        TRACKER_DBUS_OBJECT_STEROIDS,
	                                        TRACKER_DBUS_INTERFACE_STEROIDS,
	                                        "Query");

	dbus_message_iter_init_append (message, &iter);
	dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &query);
	dbus_message_iter_append_basic (&iter, DBUS_TYPE_UNIX_FD, &pipefd[1]);
	close (pipefd[1]);

	res = g_simple_async_result_new (NULL,
	                                 callback,
	                                 user_data,
	                                 tracker_bus_fd_query_async);

	tracker_dbus_send_and_splice_async (connection,
	                                    message,
	                                    pipefd[0],
	                                    NULL,
	                                    query_async_cb, res);
	/* message is destroyed by tracker_dbus_send_and_splice_async */
}

TrackerSparqlCursor *
tracker_bus_fd_query_finish (GAsyncResult     *res,
                             GError          **error)
{
	g_return_val_if_fail (res != NULL, NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error)) {
		return NULL;
	}

	return g_object_ref (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
}

#endif /* HAVE_DBUS_FD_PASSING */
