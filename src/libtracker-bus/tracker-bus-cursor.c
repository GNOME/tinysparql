/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#define TRACKER_TYPE_BUS_CURSOR           (tracker_bus_cursor_get_type ())
#define TRACKER_BUS_CURSOR(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_BUS_CURSOR, TrackerBusCursor))
#define TRACKER_BUS_CURSOR_CLASS(c)       (G_TYPE_CHECK_CLASS_CAST ((c),      TRACKER_TYPE_BUS_CURSOR, TrackerBusCursorClass))
#define TRACKER_IS_BUS_CURSOR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_BUS_CURSOR))
#define TRACKER_IS_BUS_CURSOR_CLASS(c)    (G_TYPE_CHECK_CLASS_TYPE ((o),      TRACKER_TYPE_BUS_CURSOR))
#define TRACKER_BUS_CURSOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),  TRACKER_TYPE_BUS_CURSOR, TrackerBusCursorClass))

typedef struct TrackerBusCursor TrackerBusCursor;
typedef struct TrackerBusCursorClass TrackerBusCursorClass;

struct TrackerBusCursor {
	TrackerSparqlCursor parent_instance;

	char *buffer;
	int buffer_index;
	long buffer_size;

	guint n_columns;
	int *offsets;
	char *data;
};

struct TrackerBusCursorClass {
	TrackerSparqlCursorClass parent_class;
};

GType tracker_bus_cursor_get_type (void);
void  tracker_bus_cursor_finalize (GObject *object);

G_DEFINE_TYPE (TrackerBusCursor, tracker_bus_cursor, TRACKER_SPARQL_TYPE_CURSOR)

static void
tracker_bus_cursor_rewind (TrackerBusCursor *cursor)
{
	/* FIXME: Implement */
}

static inline int
buffer_read_int (TrackerBusCursor *cursor)
{
	int v = *((int *)(cursor->buffer + cursor->buffer_index));

	cursor->buffer_index += 4;

	return v;
}

static gboolean
tracker_bus_cursor_iter_next (TrackerBusCursor  *cursor,
              	              GCancellable      *cancellable,
              	              GError           **error)
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
tracker_bus_cursor_iter_next_thread (GSimpleAsyncResult *res,
                                     GObject            *object,
                                     GCancellable       *cancellable)
{
	/* This is stolen from the direct access work, 
	 * do we REALLY need to do this in the next thread? 
	 */

	GError *error = NULL;
	gboolean result;

	result = tracker_bus_cursor_iter_next (TRACKER_BUS_CURSOR (object), cancellable, &error);
	if (error) {
		g_simple_async_result_set_from_error (res, error);
	} else {
		g_simple_async_result_set_op_res_gboolean (res, result);
	}
}

static void
tracker_bus_cursor_iter_next_async (TrackerBusCursor    *cursor,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
	GSimpleAsyncResult *res;

	res = g_simple_async_result_new (G_OBJECT (cursor), callback, user_data, tracker_bus_cursor_iter_next_async);
	g_simple_async_result_run_in_thread (res, tracker_bus_cursor_iter_next_thread, 0, cancellable);
}

static gboolean
tracker_bus_cursor_iter_next_finish (TrackerBusCursor *cursor,
                                     GAsyncResult     *res,
                                     GError          **error)
{
	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error)) {
		return FALSE;
	}
	return g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (res));
}

static guint
tracker_bus_cursor_get_n_columns (TrackerBusCursor *cursor)
{
	return cursor->n_columns;
}

static const gchar *
tracker_bus_cursor_get_string (TrackerBusCursor *cursor, 
	                           guint             column,
	                           gint             *length)
{
	g_return_val_if_fail (column < tracker_bus_cursor_get_n_columns (cursor), NULL);

	if (column == 0) {
		return cursor->data;
	} else {
		return cursor->data + cursor->offsets[column - 1] + 1;
	}
}

static void
tracker_bus_cursor_class_init (TrackerBusCursorClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
        TrackerSparqlCursorClass *sparql_cursor_class = TRACKER_SPARQL_CURSOR_CLASS (class);

        object_class->finalize = tracker_bus_cursor_finalize;

        sparql_cursor_class->get_n_columns = (gint (*) (TrackerSparqlCursor *)) tracker_bus_cursor_get_n_columns;
        sparql_cursor_class->get_string = (const gchar * (*) (TrackerSparqlCursor *, gint, gint*)) tracker_bus_cursor_get_string;
        sparql_cursor_class->next = (gboolean (*) (TrackerSparqlCursor *, GCancellable *, GError **)) tracker_bus_cursor_iter_next;
        sparql_cursor_class->next_async = (void (*) (TrackerSparqlCursor *, GCancellable *, GAsyncReadyCallback, gpointer)) tracker_bus_cursor_iter_next_async;
        sparql_cursor_class->next_finish = (gboolean (*) (TrackerSparqlCursor *, GAsyncResult *, GError **)) tracker_bus_cursor_iter_next_finish;
        sparql_cursor_class->rewind = (void (*) (TrackerSparqlCursor *)) tracker_bus_cursor_rewind;
}

void
tracker_bus_cursor_init (TrackerBusCursor *cursor)
{
}

void
tracker_bus_cursor_finalize (GObject *object)
{
	TrackerBusCursor *cursor;

	cursor = TRACKER_BUS_CURSOR (object);

	g_free (cursor->buffer);

	G_OBJECT_CLASS (tracker_bus_cursor_parent_class)->finalize (object);
}


// Public API

TrackerSparqlCursor *
tracker_bus_query (DBusGConnection  *gconnection,
                   const gchar     *query,
                   GError         **error)
{
#ifdef HAVE_DBUS_FD_PASSING
	TrackerBusCursor *cursor;
	DBusConnection *connection;
	DBusMessage *message;
	DBusMessageIter iter;
	int pipefd[2];
	GError *inner_error = NULL;

	g_return_val_if_fail (query, NULL);

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

	cursor = g_object_new (TRACKER_TYPE_BUS_CURSOR, NULL);

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
#else  /* HAVE_DBUS_FD_PASSING */
	return NULL;
#endif /* HAVE_DBUS_FD_PASSING */
}

