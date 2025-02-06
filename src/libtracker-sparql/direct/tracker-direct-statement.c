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

#include "core/tracker-data.h"
#include "tracker-serializer.h"

typedef struct _TrackerDirectStatementPrivate TrackerDirectStatementPrivate;

struct _TrackerDirectStatementPrivate
{
	TrackerSparql *sparql;
	GHashTable *values;
};

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
tracker_direct_statement_bind_langstring (TrackerSparqlStatement *stmt,
                                          const gchar            *name,
                                          const gchar            *value,
                                          const gchar            *langtag)
{
	GValue *gvalue;
	GBytes *bytes;

	bytes = tracker_sparql_make_langstring (value, langtag);
	gvalue = insert_value (TRACKER_DIRECT_STATEMENT (stmt), name, G_TYPE_BYTES);
	g_value_take_boxed (gvalue, bytes);
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
execute_async_cb (GObject      *object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
	TrackerDirectConnection *conn;
	TrackerSparqlCursor *cursor;
	GTask *task = user_data;
	GError *error = NULL;

	conn = TRACKER_DIRECT_CONNECTION (object);
	cursor = tracker_direct_connection_execute_query_statement_finish (conn, res, &error);

	if (cursor)
		g_task_return_pointer (task, cursor, g_object_unref);
	else
		g_task_return_error (task, error);

	g_object_unref (task);
}

static void
tracker_direct_statement_execute_async (TrackerSparqlStatement *stmt,
                                        GCancellable           *cancellable,
                                        GAsyncReadyCallback     callback,
                                        gpointer                user_data)
{
	TrackerDirectStatement *direct_stmt = TRACKER_DIRECT_STATEMENT (stmt);
	TrackerDirectStatementPrivate *priv =
		tracker_direct_statement_get_instance_private (direct_stmt);
	TrackerDirectConnection *conn;
	GHashTable *values;
	GTask *task;

	task = g_task_new (stmt, cancellable, callback, user_data);

	values = copy_values_deep (priv->values);
	conn = TRACKER_DIRECT_CONNECTION (tracker_sparql_statement_get_connection (stmt));
	tracker_direct_connection_execute_query_statement_async (conn,
	                                                         stmt,
	                                                         values,
	                                                         cancellable,
	                                                         execute_async_cb,
	                                                         task);
	g_hash_table_unref (values);
}

static TrackerSparqlCursor *
tracker_direct_statement_execute_finish (TrackerSparqlStatement  *stmt,
                                         GAsyncResult            *res,
                                         GError                 **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
serialize_async_cb (GObject      *object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
	TrackerDirectConnection *conn;
	GInputStream *istream;
	GTask *task = user_data;
	GError *error = NULL;

	conn = TRACKER_DIRECT_CONNECTION (object);
	istream = tracker_direct_connection_execute_serialize_statement_finish (conn, res, &error);

	if (istream)
		g_task_return_pointer (task, istream, g_object_unref);
	else
		g_task_return_error (task, error);

	g_object_unref (task);
}

static void
tracker_direct_statement_serialize_async (TrackerSparqlStatement *stmt,
                                          TrackerSerializeFlags   flags,
                                          TrackerRdfFormat        format,
                                          GCancellable           *cancellable,
                                          GAsyncReadyCallback     callback,
                                          gpointer                user_data)
{
	TrackerDirectStatement *direct_stmt = TRACKER_DIRECT_STATEMENT (stmt);
	TrackerDirectStatementPrivate *priv =
		tracker_direct_statement_get_instance_private (direct_stmt);
	TrackerDirectConnection *conn;
	GHashTable *values;
	GTask *task;

	task = g_task_new (stmt, cancellable, callback, user_data);

	values = copy_values_deep (priv->values);
	conn = TRACKER_DIRECT_CONNECTION (tracker_sparql_statement_get_connection (stmt));
	tracker_direct_connection_execute_serialize_statement_async (conn,
	                                                             stmt,
	                                                             values,
	                                                             flags,
	                                                             format,
	                                                             cancellable,
	                                                             serialize_async_cb,
	                                                             task);
	g_hash_table_unref (values);
}

static GInputStream *
tracker_direct_statement_serialize_finish (TrackerSparqlStatement  *stmt,
                                           GAsyncResult            *res,
                                           GError                 **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static gboolean
tracker_direct_statement_update (TrackerSparqlStatement  *stmt,
                                 GCancellable            *cancellable,
                                 GError                 **error)
{
	TrackerSparqlConnection *conn;
	TrackerDirectStatementPrivate *priv;

	priv = tracker_direct_statement_get_instance_private (TRACKER_DIRECT_STATEMENT (stmt));
	conn = tracker_sparql_statement_get_connection (stmt);

	return tracker_direct_connection_execute_update_statement (TRACKER_DIRECT_CONNECTION (conn),
	                                                           stmt,
	                                                           priv->values,
	                                                           error);
}

static void
tracker_direct_statement_update_async (TrackerSparqlStatement *stmt,
                                       GCancellable           *cancellable,
                                       GAsyncReadyCallback     callback,
                                       gpointer                user_data)
{
	TrackerDirectStatementPrivate *priv;
	TrackerSparqlConnection *conn;
	GHashTable *values;

	priv = tracker_direct_statement_get_instance_private (TRACKER_DIRECT_STATEMENT (stmt));
	conn = tracker_sparql_statement_get_connection (stmt);
	values = copy_values_deep (priv->values);

	tracker_direct_connection_execute_update_statement_async (TRACKER_DIRECT_CONNECTION (conn),
	                                                          stmt,
	                                                          values,
	                                                          cancellable,
	                                                          callback,
	                                                          user_data);
	g_hash_table_unref (values);
}

static gboolean
tracker_direct_statement_update_finish (TrackerSparqlStatement  *stmt,
                                        GAsyncResult            *res,
                                        GError                 **error)
{
	TrackerSparqlConnection *conn;

	conn = tracker_sparql_statement_get_connection (stmt);

	return tracker_direct_connection_execute_update_statement_finish (TRACKER_DIRECT_CONNECTION (conn),
	                                                                  res, error);
}

static void
tracker_direct_statement_class_init (TrackerDirectStatementClass *klass)
{
	TrackerSparqlStatementClass *stmt_class = (TrackerSparqlStatementClass *) klass;
	GObjectClass *object_class = (GObjectClass *) klass;

	object_class->finalize = tracker_direct_statement_finalize;

	stmt_class->bind_int = tracker_direct_statement_bind_int;
	stmt_class->bind_boolean = tracker_direct_statement_bind_boolean;
	stmt_class->bind_double = tracker_direct_statement_bind_double;
	stmt_class->bind_string = tracker_direct_statement_bind_string;
	stmt_class->bind_datetime = tracker_direct_statement_bind_datetime;
	stmt_class->bind_langstring = tracker_direct_statement_bind_langstring;
	stmt_class->clear_bindings = tracker_direct_statement_clear_bindings;
	stmt_class->execute = tracker_direct_statement_execute;
	stmt_class->execute_async = tracker_direct_statement_execute_async;
	stmt_class->execute_finish = tracker_direct_statement_execute_finish;
	stmt_class->serialize_async = tracker_direct_statement_serialize_async;
	stmt_class->serialize_finish = tracker_direct_statement_serialize_finish;
	stmt_class->update = tracker_direct_statement_update;
	stmt_class->update_async = tracker_direct_statement_update_async;
	stmt_class->update_finish = tracker_direct_statement_update_finish;
}

static void
tracker_direct_statement_init (TrackerDirectStatement *stmt)
{
	TrackerDirectStatementPrivate *priv;

	priv = tracker_direct_statement_get_instance_private (stmt);
	priv->values = create_values_ht ();
}

/* Executes with the update lock held */
gboolean
tracker_direct_statement_execute_update (TrackerSparqlStatement  *stmt,
                                         GHashTable              *parameters,
                                         GHashTable              *bnode_labels,
                                         GError                 **error)
{
	TrackerDirectStatement *direct;
	TrackerDirectStatementPrivate *priv;

	direct = TRACKER_DIRECT_STATEMENT (stmt);
	priv = tracker_direct_statement_get_instance_private (direct);

	return tracker_sparql_execute_update (priv->sparql,
	                                      parameters,
	                                      bnode_labels,
	                                      NULL,
	                                      error);
}

TrackerSparql *
tracker_direct_statement_get_sparql (TrackerSparqlStatement *stmt)
{
	TrackerDirectStatement *direct;
	TrackerDirectStatementPrivate *priv;

	direct = TRACKER_DIRECT_STATEMENT (stmt);
	priv = tracker_direct_statement_get_instance_private (direct);

	return priv->sparql;
}

TrackerDirectStatement *
tracker_direct_statement_new (TrackerSparqlConnection  *conn,
                              const gchar              *sparql,
                              GError                  **error)
{
	TrackerDirectStatement *direct;
	TrackerDirectStatementPrivate *priv;
	TrackerSparql *parser;

	parser = tracker_sparql_new (tracker_direct_connection_get_data_manager (TRACKER_DIRECT_CONNECTION (conn)),
	                             sparql,
	                             error);
	if (!parser)
		return NULL;

	direct = g_object_new (TRACKER_TYPE_DIRECT_STATEMENT,
	                       "sparql", sparql,
	                       "connection", conn,
	                       NULL);

	priv = tracker_direct_statement_get_instance_private (direct);
	priv->sparql = parser;

	return direct;
}

TrackerDirectStatement *
tracker_direct_statement_new_update (TrackerSparqlConnection  *conn,
                                     const gchar              *sparql,
                                     GError                  **error)
{
	TrackerDirectStatement *direct;
	TrackerDirectStatementPrivate *priv;
	TrackerSparql *parser;

	parser = tracker_sparql_new_update (tracker_direct_connection_get_data_manager (TRACKER_DIRECT_CONNECTION (conn)),
	                                    sparql,
	                                    error);
	if (!parser)
		return NULL;

	direct = g_object_new (TRACKER_TYPE_DIRECT_STATEMENT,
	                       "sparql", sparql,
	                       "connection", conn,
	                       NULL);

	priv = tracker_direct_statement_get_instance_private (direct);
	priv->sparql = parser;

	return direct;
}
