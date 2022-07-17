/*
 * Copyright (C) 2020, Red Hat Ltd.
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

#include "config.h"

#include <libtracker-common/tracker-common.h>

#include "tracker-bus-statement.h"

struct _TrackerBusStatement
{
	TrackerSparqlStatement parent_instance;
	GHashTable *arguments;
};

G_DEFINE_TYPE (TrackerBusStatement,
               tracker_bus_statement,
	       TRACKER_TYPE_SPARQL_STATEMENT)

static void
tracker_bus_statement_finalize (GObject *object)
{
	TrackerBusStatement *bus_stmt = TRACKER_BUS_STATEMENT (object);

	g_hash_table_unref (bus_stmt->arguments);

	G_OBJECT_CLASS (tracker_bus_statement_parent_class)->finalize (object);
}

static void
tracker_bus_statement_bind_string (TrackerSparqlStatement *stmt,
				   const gchar            *name,
				   const gchar            *value)
{
	TrackerBusStatement *bus_stmt = TRACKER_BUS_STATEMENT (stmt);

	g_hash_table_insert (bus_stmt->arguments,
	                     g_strdup (name),
	                     g_variant_new_string (value));
}

static void
tracker_bus_statement_bind_boolean (TrackerSparqlStatement *stmt,
                                    const gchar            *name,
                                    gboolean                value)
{
	TrackerBusStatement *bus_stmt = TRACKER_BUS_STATEMENT (stmt);

	g_hash_table_insert (bus_stmt->arguments,
	                     g_strdup (name),
	                     g_variant_new_boolean (value));
}

static void
tracker_bus_statement_bind_double (TrackerSparqlStatement *stmt,
                                   const gchar            *name,
                                   gdouble                 value)
{
	TrackerBusStatement *bus_stmt = TRACKER_BUS_STATEMENT (stmt);

	g_hash_table_insert (bus_stmt->arguments,
	                     g_strdup (name),
	                     g_variant_new_double (value));
}

static void
tracker_bus_statement_bind_int (TrackerSparqlStatement *stmt,
                                const gchar            *name,
                                gint64                  value)
{
	TrackerBusStatement *bus_stmt = TRACKER_BUS_STATEMENT (stmt);

	g_hash_table_insert (bus_stmt->arguments,
	                     g_strdup (name),
	                     g_variant_new_int64 (value));
}

static void
tracker_bus_statement_bind_datetime (TrackerSparqlStatement *stmt,
                                     const gchar            *name,
                                     GDateTime              *value)
{
	TrackerBusStatement *bus_stmt = TRACKER_BUS_STATEMENT (stmt);
	gchar *date_str;

	date_str = tracker_date_format_iso8601 (value);
	g_hash_table_insert (bus_stmt->arguments,
	                     g_strdup (name),
	                     g_variant_new_string (date_str));
	g_free (date_str);
}

static void
tracker_bus_statement_clear_bindings (TrackerSparqlStatement *stmt)
{
	TrackerBusStatement *bus_stmt = TRACKER_BUS_STATEMENT (stmt);

	g_hash_table_remove_all (bus_stmt->arguments);
}

static GVariant *
get_arguments (TrackerBusStatement *bus_stmt)
{
	GVariantBuilder builder;
	GHashTableIter iter;
	gpointer key, value;

	if (g_hash_table_size (bus_stmt->arguments) == 0)
		return NULL;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
	g_hash_table_iter_init (&iter, bus_stmt->arguments);

	while (g_hash_table_iter_next (&iter, &key, &value))
		g_variant_builder_add (&builder, "{sv}", key, g_variant_ref_sink (value));

	return g_variant_builder_end (&builder);
}

static TrackerSparqlCursor *
tracker_bus_statement_execute (TrackerSparqlStatement  *stmt,
                               GCancellable            *cancellable,
                               GError                 **error)
{
	TrackerBusStatement *bus_stmt = TRACKER_BUS_STATEMENT (stmt);
	TrackerSparqlConnection *conn;

	conn = tracker_sparql_statement_get_connection (stmt);

	return tracker_bus_connection_perform_query (TRACKER_BUS_CONNECTION (conn),
						     tracker_sparql_statement_get_sparql (stmt),
						     get_arguments (bus_stmt),
						     cancellable, error);
}

static void
execute_cb (GObject      *source,
	    GAsyncResult *res,
	    gpointer      user_data)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GTask *task = user_data;

	cursor = tracker_bus_connection_perform_query_finish (TRACKER_BUS_CONNECTION (source),
							      res, &error);
	if (cursor)
		g_task_return_pointer (task, cursor, g_object_unref);
	else
		g_task_return_error (task, error);

	g_object_unref (task);
}

static void
tracker_bus_statement_execute_async (TrackerSparqlStatement *stmt,
                                     GCancellable           *cancellable,
                                     GAsyncReadyCallback     callback,
                                     gpointer                user_data)
{
	TrackerBusStatement *bus_stmt = TRACKER_BUS_STATEMENT (stmt);
	TrackerSparqlConnection *conn;
	GTask *task;

	task = g_task_new (stmt, cancellable, callback, user_data);
	conn = tracker_sparql_statement_get_connection (stmt);
	tracker_bus_connection_perform_query_async (TRACKER_BUS_CONNECTION (conn),
						    tracker_sparql_statement_get_sparql (stmt),
						    get_arguments (bus_stmt),
						    cancellable,
						    execute_cb,
						    task);
}

static TrackerSparqlCursor *
tracker_bus_statement_execute_finish (TrackerSparqlStatement  *stmt,
                                      GAsyncResult            *res,
                                      GError                 **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
serialize_cb (GObject      *source,
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
tracker_bus_statement_serialize_async (TrackerSparqlStatement *stmt,
                                       TrackerSerializeFlags   flags,
                                       TrackerRdfFormat        format,
                                       GCancellable           *cancellable,
                                       GAsyncReadyCallback     callback,
                                       gpointer                user_data)
{
	TrackerBusStatement *bus_stmt = TRACKER_BUS_STATEMENT (stmt);
	TrackerSparqlConnection *conn;
	GTask *task;

	task = g_task_new (stmt, cancellable, callback, user_data);
	conn = tracker_sparql_statement_get_connection (stmt);
	tracker_bus_connection_perform_serialize_async (TRACKER_BUS_CONNECTION (conn),
							flags, format,
							tracker_sparql_statement_get_sparql (stmt),
							get_arguments (bus_stmt),
							cancellable,
							serialize_cb,
							task);
}

static GInputStream *
tracker_bus_statement_serialize_finish (TrackerSparqlStatement  *stmt,
                                        GAsyncResult            *res,
                                        GError                 **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
tracker_bus_statement_class_init (TrackerBusStatementClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerSparqlStatementClass *stmt_class =
		TRACKER_SPARQL_STATEMENT_CLASS (klass);

	object_class->finalize = tracker_bus_statement_finalize;

	stmt_class->bind_boolean = tracker_bus_statement_bind_boolean;
	stmt_class->bind_double = tracker_bus_statement_bind_double;
	stmt_class->bind_int = tracker_bus_statement_bind_int;
	stmt_class->bind_string = tracker_bus_statement_bind_string;
	stmt_class->bind_datetime = tracker_bus_statement_bind_datetime;
	stmt_class->clear_bindings = tracker_bus_statement_clear_bindings;
	stmt_class->execute = tracker_bus_statement_execute;
	stmt_class->execute_async = tracker_bus_statement_execute_async;
	stmt_class->execute_finish = tracker_bus_statement_execute_finish;
	stmt_class->serialize_async = tracker_bus_statement_serialize_async;
	stmt_class->serialize_finish = tracker_bus_statement_serialize_finish;
}

static void
tracker_bus_statement_init (TrackerBusStatement *stmt)
{
	stmt->arguments = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                         g_free,
						 (GDestroyNotify) g_variant_unref);
}

TrackerSparqlStatement *
tracker_bus_statement_new (TrackerBusConnection *conn,
                           const gchar          *query)
{
	return g_object_new (TRACKER_TYPE_BUS_STATEMENT,
	                     "connection", conn,
	                     "sparql", query,
	                     NULL);
}
