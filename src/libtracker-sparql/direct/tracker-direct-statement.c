/*
 * Copyright (C) 2018, Red Hat, Inc.
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

#include "tracker-direct-statement.h"
#include "tracker-private.h"

#include <libtracker-sparql/core/tracker-data.h>
#include <libtracker-sparql/tracker-serializer.h>

typedef struct _TrackerDirectStatementPrivate TrackerDirectStatementPrivate;

struct _TrackerDirectStatementPrivate
{
	TrackerSparql *sparql;
	GHashTable *values;
};

typedef struct
{
	GHashTable *values;
	TrackerRdfFormat format;
} RdfSerializationData;

G_DEFINE_TYPE_WITH_PRIVATE (TrackerDirectStatement,
                            tracker_direct_statement,
                            TRACKER_TYPE_SPARQL_STATEMENT)

static void
tracker_direct_statement_finalize (GObject *object)
{
	TrackerDirectStatementPrivate *priv;

	priv = tracker_direct_statement_get_instance_private (TRACKER_DIRECT_STATEMENT (object));
	g_hash_table_destroy (priv->values);
	g_clear_object (&priv->sparql);

	G_OBJECT_CLASS (tracker_direct_statement_parent_class)->finalize (object);
}

static void
tracker_direct_statement_constructed (GObject *object)
{
	TrackerDirectStatementPrivate *priv;
	TrackerSparqlConnection *conn;
	gchar *sparql;

	priv = tracker_direct_statement_get_instance_private (TRACKER_DIRECT_STATEMENT (object));

	g_object_get (object,
	              "sparql", &sparql,
	              "connection", &conn,
	              NULL);

	priv->sparql = tracker_sparql_new (tracker_direct_connection_get_data_manager (TRACKER_DIRECT_CONNECTION (conn)),
	                                   sparql);
	g_object_unref (conn);
	g_free (sparql);

	G_OBJECT_CLASS (tracker_direct_statement_parent_class)->constructed (object);
}

static GValue *
insert_value (TrackerDirectStatement *stmt,
	      const gchar            *name,
	      GType                   type)
{
	TrackerDirectStatementPrivate *priv;
	GValue *value;

	priv = tracker_direct_statement_get_instance_private (stmt);
	value = g_new0 (GValue, 1);
	g_value_init (value, type);

	g_hash_table_insert (priv->values, g_strdup (name), value);

	return value;
}

static void
tracker_direct_statement_bind_int (TrackerSparqlStatement *stmt,
                                   const gchar            *name,
                                   gint64                  value)
{
	GValue *gvalue;

	gvalue = insert_value (TRACKER_DIRECT_STATEMENT (stmt), name, G_TYPE_INT64);
	g_value_set_int64 (gvalue, value);
}

static void
tracker_direct_statement_bind_double (TrackerSparqlStatement *stmt,
                                      const gchar            *name,
                                      double                  value)
{
	GValue *gvalue;

	gvalue = insert_value (TRACKER_DIRECT_STATEMENT (stmt), name, G_TYPE_DOUBLE);
	g_value_set_double (gvalue, value);
}

static void
tracker_direct_statement_bind_boolean (TrackerSparqlStatement *stmt,
                                       const gchar            *name,
                                       gboolean                value)
{
	GValue *gvalue;

	gvalue = insert_value (TRACKER_DIRECT_STATEMENT (stmt), name, G_TYPE_BOOLEAN);
	g_value_set_boolean (gvalue, value);
}

static void
tracker_direct_statement_bind_string (TrackerSparqlStatement *stmt,
                                      const gchar            *name,
                                      const gchar            *value)
{
	GValue *gvalue;

	gvalue = insert_value (TRACKER_DIRECT_STATEMENT (stmt), name, G_TYPE_STRING);
	g_value_set_string (gvalue, value);
}

static void
tracker_direct_statement_bind_datetime (TrackerSparqlStatement *stmt,
                                        const gchar            *name,
                                        GDateTime              *datetime)
{
	GValue *gvalue;

	gvalue = insert_value (TRACKER_DIRECT_STATEMENT (stmt), name, G_TYPE_DATE_TIME);
	g_value_set_boxed (gvalue, datetime);
}

static void
tracker_direct_statement_clear_bindings (TrackerSparqlStatement *stmt)
{
	TrackerDirectStatementPrivate *priv;

	priv = tracker_direct_statement_get_instance_private (TRACKER_DIRECT_STATEMENT (stmt));
	g_hash_table_remove_all (priv->values);
}

static TrackerSparqlCursor *
tracker_direct_statement_execute (TrackerSparqlStatement  *stmt,
                                  GCancellable            *cancellable,
                                  GError                 **error)
{
	TrackerDirectStatementPrivate *priv;
	TrackerSparqlCursor *cursor;
	GError *inner_error = NULL;

	priv = tracker_direct_statement_get_instance_private (TRACKER_DIRECT_STATEMENT (stmt));

	cursor = tracker_sparql_execute_cursor (priv->sparql, priv->values, &inner_error);
	if (inner_error)
		g_propagate_error (error, _translate_internal_error (inner_error));

	return cursor;
}

static void
execute_in_thread (GTask        *task,
                   gpointer      object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
	TrackerDirectStatementPrivate *priv;
	TrackerSparqlCursor *cursor;
	GHashTable *values = task_data;
	GError *error = NULL;

	if (g_task_return_error_if_cancelled (task))
		return;

	priv = tracker_direct_statement_get_instance_private (object);
	cursor = tracker_sparql_execute_cursor (priv->sparql, values, &error);

	if (error)
		g_task_return_error (task, error);
	else
		g_task_return_pointer (task, cursor, g_object_unref);
}

static void
rdf_serialization_data_free (RdfSerializationData *data)
{
	g_hash_table_unref (data->values);
	g_free (data);
}

static TrackerSerializerFormat
convert_format (TrackerRdfFormat format)
{
	switch (format) {
	case TRACKER_RDF_FORMAT_TURTLE:
		return TRACKER_SERIALIZER_FORMAT_TTL;
	case TRACKER_RDF_FORMAT_TRIG:
		return TRACKER_SERIALIZER_FORMAT_TRIG;
	default:
		g_assert_not_reached ();
	}
}

static void
serialize_in_thread (GTask        *task,
                     gpointer      object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
	TrackerSparqlConnection *conn;
	TrackerDirectStatementPrivate *priv;
	TrackerSparqlCursor *cursor = NULL;
	RdfSerializationData *data = task_data;
	GInputStream *istream = NULL;
	GError *error = NULL;

	if (g_task_return_error_if_cancelled (task))
		return;

	priv = tracker_direct_statement_get_instance_private (object);
	if (!tracker_sparql_is_serializable (priv->sparql)) {
		g_set_error (&error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_PARSE,
		             "Query is not DESCRIBE or CONSTRUCT");
		goto out;
	}

	cursor = tracker_sparql_execute_cursor (priv->sparql, data->values, &error);
	if (!cursor)
		goto out;

	conn = tracker_sparql_statement_get_connection (object);
	tracker_direct_connection_update_timestamp (TRACKER_DIRECT_CONNECTION (conn));
	tracker_sparql_cursor_set_connection (cursor, conn);
	istream = tracker_serializer_new (cursor,
	                                  tracker_sparql_connection_get_namespace_manager (conn),
	                                  convert_format (data->format));

 out:
	g_clear_object (&cursor);

	if (istream)
		g_task_return_pointer (task, istream, g_object_unref);
	else
		g_task_return_error (task, error);
}

static void
free_gvalue (gpointer data)
{
	g_value_unset (data);
	g_free (data);
}

static GHashTable *
create_values_ht (void)
{
	return g_hash_table_new_full (g_str_hash, g_str_equal,
				      g_free, free_gvalue);
}

static GHashTable *
copy_values_deep (GHashTable *values)
{
	GHashTable *copy;
	GHashTableIter iter;
	gpointer key, val;

	copy = create_values_ht ();
	g_hash_table_iter_init (&iter, values);

	while (g_hash_table_iter_next (&iter, &key, &val)) {
		GValue *copy_value;

		copy_value = g_new0 (GValue, 1);
		g_value_init (copy_value, G_VALUE_TYPE (val));
		g_value_copy (val, copy_value);

		g_hash_table_insert (copy, g_strdup (key), copy_value);
	}

	return copy;
}

static void
tracker_direct_statement_execute_async (TrackerSparqlStatement *stmt,
                                        GCancellable           *cancellable,
                                        GAsyncReadyCallback     callback,
                                        gpointer                user_data)
{
	TrackerDirectStatementPrivate *priv;
	GHashTable *values;
	GTask *task;

	priv = tracker_direct_statement_get_instance_private (TRACKER_DIRECT_STATEMENT (stmt));

	values = copy_values_deep (priv->values);

	task = g_task_new (stmt, cancellable, callback, user_data);
	g_task_set_task_data (task, values, (GDestroyNotify) g_hash_table_unref);
	g_task_run_in_thread (task, execute_in_thread);
	g_object_unref (task);
}

static TrackerSparqlCursor *
tracker_direct_statement_execute_finish (TrackerSparqlStatement  *stmt,
                                         GAsyncResult            *res,
                                         GError                 **error)
{
	TrackerDirectConnection *conn;
	TrackerSparqlCursor *cursor;
	GError *inner_error = NULL;

	cursor = g_task_propagate_pointer (G_TASK (res), &inner_error);
	if (inner_error)
		g_propagate_error (error, _translate_internal_error (inner_error));

	g_object_get (stmt, "connection", &conn, NULL);
	tracker_direct_connection_update_timestamp (conn);
	g_object_unref (conn);

	return cursor;
}

static void
tracker_direct_statement_serialize_async (TrackerSparqlStatement *stmt,
                                          TrackerSerializeFlags   flags,
                                          TrackerRdfFormat        format,
                                          GCancellable           *cancellable,
                                          GAsyncReadyCallback     callback,
                                          gpointer                user_data)
{
	TrackerDirectStatementPrivate *priv;
	RdfSerializationData *data;
	GTask *task;

	priv = tracker_direct_statement_get_instance_private (TRACKER_DIRECT_STATEMENT (stmt));

	data = g_new0 (RdfSerializationData, 1);
	data->values = copy_values_deep (priv->values);
	data->format = format;

	task = g_task_new (stmt, cancellable, callback, user_data);
	g_task_set_task_data (task, data, (GDestroyNotify) rdf_serialization_data_free);
	g_task_run_in_thread (task, serialize_in_thread);
	g_object_unref (task);
}

static GInputStream *
tracker_direct_statement_serialize_finish (TrackerSparqlStatement  *stmt,
                                           GAsyncResult            *res,
                                           GError                 **error)
{
	GError *inner_error = NULL;
	GInputStream *istream;

	istream = g_task_propagate_pointer (G_TASK (res), &inner_error);
	if (inner_error)
		g_propagate_error (error, _translate_internal_error (inner_error));

	return istream;
}

static void
tracker_direct_statement_class_init (TrackerDirectStatementClass *klass)
{
	TrackerSparqlStatementClass *stmt_class = (TrackerSparqlStatementClass *) klass;
	GObjectClass *object_class = (GObjectClass *) klass;

	object_class->finalize = tracker_direct_statement_finalize;
	object_class->constructed = tracker_direct_statement_constructed;

	stmt_class->bind_int = tracker_direct_statement_bind_int;
	stmt_class->bind_boolean = tracker_direct_statement_bind_boolean;
	stmt_class->bind_double = tracker_direct_statement_bind_double;
	stmt_class->bind_string = tracker_direct_statement_bind_string;
	stmt_class->bind_datetime = tracker_direct_statement_bind_datetime;
	stmt_class->clear_bindings = tracker_direct_statement_clear_bindings;
	stmt_class->execute = tracker_direct_statement_execute;
	stmt_class->execute_async = tracker_direct_statement_execute_async;
	stmt_class->execute_finish = tracker_direct_statement_execute_finish;
	stmt_class->serialize_async = tracker_direct_statement_serialize_async;
	stmt_class->serialize_finish = tracker_direct_statement_serialize_finish;
}

static void
tracker_direct_statement_init (TrackerDirectStatement *stmt)
{
	TrackerDirectStatementPrivate *priv;

	priv = tracker_direct_statement_get_instance_private (stmt);
	priv->values = create_values_ht ();
}

TrackerDirectStatement *
tracker_direct_statement_new (TrackerSparqlConnection  *conn,
                              const gchar              *sparql,
                              GError                  **error)
{
	return g_object_new (TRACKER_TYPE_DIRECT_STATEMENT,
	                     "sparql", sparql,
	                     "connection", conn,
	                     NULL);
}
