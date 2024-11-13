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

#include "tracker-bus-cursor.h"

struct _TrackerBusCursor
{
	TrackerDeserializer parent_instance;
	GDataInputStream *data_stream;
	GVariant *variables;
	const gchar **variable_names;
	gint n_columns;
	TrackerSparqlValueType *types;
	gchar *row_data;
	gint32 *offsets;
	const gchar **values;
	gboolean finished;
};

enum {
	PROP_0,
	PROP_VARIABLES,
	N_PROPS
};

/* Set a ridiculously high limit on the row size,
 * but a limit nonetheless. We can store up to 1GB
 * in a single column/row, so make room for 2GiB.
 */
#define MAX_ROW_SIZE (2 * 1000 * 1000 * 1000)

static GParamSpec *props[N_PROPS] = { 0, };

G_DEFINE_TYPE (TrackerBusCursor,
               tracker_bus_cursor,
               TRACKER_TYPE_DESERIALIZER)

static void
tracker_bus_cursor_finalize (GObject *object)
{
	TrackerBusCursor *bus_cursor = TRACKER_BUS_CURSOR (object);

	g_clear_object (&bus_cursor->data_stream);
	g_clear_pointer (&bus_cursor->variables, g_variant_unref);
	g_clear_pointer (&bus_cursor->types, g_free);
	g_clear_pointer (&bus_cursor->row_data, g_free);
	g_clear_pointer (&bus_cursor->values, g_free);
	g_clear_pointer (&bus_cursor->variable_names, g_free);
	g_clear_pointer (&bus_cursor->offsets, g_free);

	G_OBJECT_CLASS (tracker_bus_cursor_parent_class)->finalize (object);
}

static void
tracker_bus_cursor_constructed (GObject *object)
{
	TrackerDeserializer *deserializer = TRACKER_DESERIALIZER (object);
	TrackerBusCursor *cursor = TRACKER_BUS_CURSOR (object);

	G_OBJECT_CLASS (tracker_bus_cursor_parent_class)->constructed (object);

	cursor->variable_names = g_variant_get_strv (cursor->variables, NULL);
	cursor->n_columns = g_strv_length ((gchar **) cursor->variable_names);
	cursor->data_stream =
		g_data_input_stream_new (tracker_deserializer_get_stream (deserializer));
	g_data_input_stream_set_byte_order (cursor->data_stream,
					    G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN);
}

static void
tracker_bus_cursor_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	TrackerBusCursor *cursor = TRACKER_BUS_CURSOR (object);

	switch (prop_id) {
	case PROP_VARIABLES:
		cursor->variables = g_value_dup_variant (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gint
tracker_bus_cursor_get_n_columns (TrackerSparqlCursor *cursor)
{
	TrackerBusCursor *bus_cursor = TRACKER_BUS_CURSOR (cursor);

	return bus_cursor->n_columns;
}

static TrackerSparqlValueType
tracker_bus_cursor_get_value_type (TrackerSparqlCursor *cursor,
                                   gint                 column)
{
	TrackerBusCursor *bus_cursor = TRACKER_BUS_CURSOR (cursor);

	if (bus_cursor->finished)
		return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
	if (column < 0 || column >= bus_cursor->n_columns)
		return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
	if (!bus_cursor->types)
		return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;

	return bus_cursor->types[column];
}

static const gchar *
tracker_bus_cursor_get_variable_name (TrackerSparqlCursor *cursor,
                                      gint                 column)
{
	TrackerBusCursor *bus_cursor = TRACKER_BUS_CURSOR (cursor);

	if (column < 0 || column >= bus_cursor->n_columns)
		return NULL;

	return bus_cursor->variable_names[column];
}

static const gchar *
tracker_bus_cursor_get_string (TrackerSparqlCursor  *cursor,
                               gint                  column,
                               const gchar         **langtag,
			       glong                *len)
{
	TrackerBusCursor *bus_cursor = TRACKER_BUS_CURSOR (cursor);
	const gchar *str;

	if (len)
		*len = 0;
	if (langtag)
		*langtag = NULL;

	if (bus_cursor->finished)
		return NULL;
	if (column < 0 || column >= bus_cursor->n_columns)
		return NULL;
	if (!bus_cursor->types)
		return NULL;

	/* Return null instead of empty string for unbound values */
	if (bus_cursor->types[column] == TRACKER_SPARQL_VALUE_TYPE_UNBOUND)
		return NULL;

	str = bus_cursor->values[column];

	if (len || langtag) {
		int str_len;

		str_len = strlen (str);
		if (len)
			*len = str_len;
		if (langtag && str_len < bus_cursor->offsets[column])
			*langtag = &str[str_len + 1];
	}

	return str;
}

static gboolean
tracker_bus_cursor_next (TrackerSparqlCursor  *cursor,
                         GCancellable         *cancellable,
                         GError              **error)
{
	TrackerBusCursor *bus_cursor = TRACKER_BUS_CURSOR (cursor);
	gint32 n_columns, i;
	gssize data_size;

	if (bus_cursor->finished)
		return FALSE;
	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	/* So, the make up on each cursor segment is:
	 *
	 * iteration = [4 bytes for number of columns,
	 *              columns x 4 bytes for types
	 *              columns x 4 bytes for offsets]
	 */
	n_columns = g_data_input_stream_read_int32 (bus_cursor->data_stream,
	                                            cancellable, NULL);

	if (n_columns == 0) {
		bus_cursor->finished = TRUE;
		return FALSE;
	}

	g_clear_pointer (&bus_cursor->types, g_free);
	bus_cursor->types = g_new0 (TrackerSparqlValueType, n_columns);

	if (!g_input_stream_read_all (G_INPUT_STREAM (bus_cursor->data_stream),
	                              bus_cursor->types,
	                              n_columns * sizeof (gint32),
	                              NULL, cancellable, error))
		return FALSE;

	g_clear_pointer (&bus_cursor->offsets, g_free);
	bus_cursor->offsets = g_new0 (gint32, n_columns);
	if (!g_input_stream_read_all (G_INPUT_STREAM (bus_cursor->data_stream),
	                              bus_cursor->offsets,
	                              n_columns * sizeof (gint32),
	                              NULL, cancellable, error))
		return FALSE;

	for (i = 0; i < n_columns - 1; i++) {
		gint32 cur = bus_cursor->offsets[i];

		if (cur < 0 ||
		    cur > MAX_ROW_SIZE ||
		    (i > 0 && cur <= bus_cursor->offsets[i - 1])) {
			g_set_error (error,
			             G_IO_ERROR,
			             G_IO_ERROR_INVALID_DATA,
			             "Corrupted cursor data");
			return FALSE;
		}
	}

	/* The last offset says how long we have to go to read
	 * the whole row data.
	 */
	g_clear_pointer (&bus_cursor->row_data, g_free);
	data_size = bus_cursor->offsets[n_columns - 1] + 1;
	g_assert (data_size >= 0 && data_size <= MAX_ROW_SIZE);
	bus_cursor->row_data = g_new0 (gchar, data_size);

	if (!g_input_stream_read_all (G_INPUT_STREAM (bus_cursor->data_stream),
	                              bus_cursor->row_data,
	                              bus_cursor->offsets[n_columns - 1] + 1,
	                              NULL, cancellable, error))
		return FALSE;

	g_clear_pointer (&bus_cursor->values, g_free);
	bus_cursor->values = g_new0 (const gchar *, n_columns);

	for (i = 0; i < n_columns; i++) {
		gint32 offset;

		offset = (i == 0) ? 0 : bus_cursor->offsets[i - 1] + 1;
		g_assert (offset >= 0 && offset <= MAX_ROW_SIZE);
		bus_cursor->values[i] = &bus_cursor->row_data[offset];
	}

	return TRUE;
}

static void
next_in_thread (GTask        *task,
		gpointer      source_object,
		gpointer      task_data,
		GCancellable *cancellable)
{
	GError *error = NULL;
	gboolean retval;

	retval = tracker_sparql_cursor_next (source_object, cancellable, &error);

	if (error)
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, retval);
}

static void
tracker_bus_cursor_next_async (TrackerSparqlCursor  *cursor,
                               GCancellable         *cancellable,
                               GAsyncReadyCallback   cb,
                               gpointer              user_data)
{
	GTask *task;

	task = g_task_new (cursor, cancellable, cb, user_data);
	g_task_run_in_thread (task, next_in_thread);
	g_object_unref (task);
}

static gboolean
tracker_bus_cursor_next_finish (TrackerSparqlCursor  *cursor,
                                GAsyncResult         *res,
                                GError              **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
tracker_bus_cursor_close (TrackerSparqlCursor *cursor)
{
	TrackerBusCursor *bus_cursor = TRACKER_BUS_CURSOR (cursor);

	g_input_stream_close (G_INPUT_STREAM (bus_cursor->data_stream),
			      NULL, NULL);

	TRACKER_SPARQL_CURSOR_CLASS (tracker_bus_cursor_parent_class)->close (cursor);
}

static void
tracker_bus_cursor_class_init (TrackerBusCursorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerSparqlCursorClass *cursor_class =
		TRACKER_SPARQL_CURSOR_CLASS (klass);

	object_class->finalize = tracker_bus_cursor_finalize;
	object_class->constructed = tracker_bus_cursor_constructed;
	object_class->set_property = tracker_bus_cursor_set_property;

	cursor_class->get_n_columns = tracker_bus_cursor_get_n_columns;
	cursor_class->get_value_type = tracker_bus_cursor_get_value_type;
	cursor_class->get_variable_name = tracker_bus_cursor_get_variable_name;
	cursor_class->get_string = tracker_bus_cursor_get_string;
	cursor_class->next = tracker_bus_cursor_next;
	cursor_class->next_async = tracker_bus_cursor_next_async;
	cursor_class->next_finish = tracker_bus_cursor_next_finish;
	cursor_class->close = tracker_bus_cursor_close;

	props[PROP_VARIABLES] =
		g_param_spec_variant ("variables",
		                      "Variables",
		                      "Variables",
		                      G_VARIANT_TYPE ("as"),
		                      NULL,
		                      G_PARAM_WRITABLE |
		                      G_PARAM_STATIC_STRINGS |
		                      G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, props);

}

static void
tracker_bus_cursor_init (TrackerBusCursor *cursor)
{
}

TrackerSparqlCursor *
tracker_bus_cursor_new (GInputStream *stream,
			GVariant     *variables)

{
	return g_object_new (TRACKER_TYPE_BUS_CURSOR,
	                     "stream", stream,
	                     "variables", variables,
	                     NULL);
}
