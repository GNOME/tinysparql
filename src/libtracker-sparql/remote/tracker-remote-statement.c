/*
 * Copyright (C) 2021, Red Hat Inc.
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

#include "tracker-remote-statement.h"

#include <tracker-common.h>

#include "core/tracker-sparql-grammar.h"
#include "core/tracker-sparql-parser.h"
#include "tracker-private.h"

struct _TrackerRemoteStatement
{
	TrackerSparqlStatement parent_instance;
	TrackerNodeTree *parser_tree;
	GHashTable *bindings;
};

G_DEFINE_TYPE (TrackerRemoteStatement,
               tracker_remote_statement,
               TRACKER_TYPE_SPARQL_STATEMENT)

static void
tracker_remote_statement_finalize (GObject *object)
{
	TrackerRemoteStatement *remote_stmt = TRACKER_REMOTE_STATEMENT (object);

	if (remote_stmt->parser_tree)
		tracker_node_tree_free (remote_stmt->parser_tree);
	g_hash_table_unref (remote_stmt->bindings);

	G_OBJECT_CLASS (tracker_remote_statement_parent_class)->finalize (object);
}

static void
tracker_remote_statement_bind_int (TrackerSparqlStatement *stmt,
                                   const gchar            *name,
                                   gint64                  value)
{
	TrackerRemoteStatement *remote_stmt = TRACKER_REMOTE_STATEMENT (stmt);
	GValue *val;

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_INT64);
	g_value_set_int64 (val, value);

	g_hash_table_insert (remote_stmt->bindings,
	                     g_strdup (name),
	                     val);
}

static void
tracker_remote_statement_bind_boolean (TrackerSparqlStatement *stmt,
                                       const gchar            *name,
                                       gboolean                value)
{
	TrackerRemoteStatement *remote_stmt = TRACKER_REMOTE_STATEMENT (stmt);
	GValue *val;

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_BOOLEAN);
	g_value_set_boolean (val, value);

	g_hash_table_insert (remote_stmt->bindings,
	                     g_strdup (name),
	                     val);
}

static void
tracker_remote_statement_bind_string (TrackerSparqlStatement *stmt,
                                      const gchar            *name,
                                      const gchar            *value)
{
	TrackerRemoteStatement *remote_stmt = TRACKER_REMOTE_STATEMENT (stmt);
	GValue *val;

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_STRING);
	g_value_set_string (val, value);

	g_hash_table_insert (remote_stmt->bindings,
	                     g_strdup (name),
	                     val);
}

static void
tracker_remote_statement_bind_double (TrackerSparqlStatement *stmt,
                                      const gchar            *name,
                                      gdouble                 value)
{
	TrackerRemoteStatement *remote_stmt = TRACKER_REMOTE_STATEMENT (stmt);
	GValue *val;

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_DOUBLE);
	g_value_set_double (val, value);

	g_hash_table_insert (remote_stmt->bindings,
	                     g_strdup (name),
	                     val);
}

static void
tracker_remote_statement_bind_datetime (TrackerSparqlStatement *stmt,
                                        const gchar            *name,
                                        GDateTime              *value)
{
	TrackerRemoteStatement *remote_stmt = TRACKER_REMOTE_STATEMENT (stmt);
	GValue *val;

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_DATE_TIME);
	g_value_set_boxed (val, value);

	g_hash_table_insert (remote_stmt->bindings,
	                     g_strdup (name),
	                     val);
}

static void
tracker_remote_statement_bind_langstring (TrackerSparqlStatement *stmt,
                                          const gchar            *name,
                                          const gchar            *value,
                                          const gchar            *langtag)
{
	TrackerRemoteStatement *remote_stmt = TRACKER_REMOTE_STATEMENT (stmt);
	GValue *val;

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_BYTES);
	g_value_take_boxed (val, tracker_sparql_make_langstring (value, langtag));

	g_hash_table_insert (remote_stmt->bindings,
	                     g_strdup (name),
	                     val);
}

static void
append_gvalue (GString *str,
               const GValue  *value)
{
	if (G_VALUE_HOLDS_BOOLEAN (value)) {
		g_string_append_printf (str, "%s",
		                        g_value_get_boolean (value) ?
		                        "true" : "false");
	} else if (G_VALUE_HOLDS_INT64 (value)) {
		g_string_append_printf (str, "%" G_GINT64_FORMAT,
		                        g_value_get_int64 (value));
	} else if (G_VALUE_HOLDS_DOUBLE (value)) {
		gchar buf[G_ASCII_DTOSTR_BUF_SIZE + 1];

		g_ascii_dtostr (buf, sizeof (buf), g_value_get_double (value));
		g_string_append (str, buf);
	} else if (G_VALUE_TYPE (value) == G_TYPE_DATE_TIME) {
		GDateTime *datetime;
		gchar *datetime_str;

		datetime = g_value_get_boxed (value);
		datetime_str = tracker_date_format_iso8601 (datetime);
		g_string_append_printf (str, "\"%s\"", datetime_str);
		g_free (datetime_str);
	} else if (G_VALUE_TYPE (value) == G_TYPE_BYTES) {
		GBytes *bytes;
		const gchar *data;
		gsize len, str_len;

		bytes = g_value_get_boxed (value);
		data = g_bytes_get_data (bytes, &len);
		str_len = strlen (data);
		g_string_append_printf (str, "\"%s\"", data);
		if (str_len < len) {
			const gchar *langtag;
			langtag = &data[str_len + 1];
			g_string_append_printf (str, "@%s", langtag);
		}
	} else if (G_VALUE_HOLDS_STRING (value)) {
		const gchar *val = g_value_get_string (value);
		int len = strlen (val);
		gchar *end;
		gboolean is_number = FALSE;

		/* Try to detect numbers anyway, since we use to allow
		 * loose typing in other connection types.
		 */
		g_ascii_strtoll (val, &end, 10);
		is_number = (end == &val[len]);

		if (!is_number) {
			g_ascii_strtod (val, &end);
			is_number = (end == &val[len]);
		}

		if (is_number)
			g_string_append (str, val);
		else
			g_string_append_printf (str, "\"%s\"", val);
	}
}

static gchar *
apply_bindings (TrackerSparqlStatement  *stmt,
                GHashTable              *bindings,
                GError                 **error)
{
	TrackerRemoteStatement *remote_stmt = TRACKER_REMOTE_STATEMENT (stmt);
	const gchar *query = tracker_sparql_statement_get_sparql (stmt);
	GString *str = g_string_new (NULL);
	TrackerParserNode *node = tracker_node_tree_get_root (remote_stmt->parser_tree);

	for (node = tracker_sparql_parser_tree_find_first (node, TRUE);
	     node;
	     node = tracker_sparql_parser_tree_find_next (node, TRUE)) {
		const TrackerGrammarRule *rule;
		gssize start, end;

		if (!tracker_parser_node_get_extents (node, &start, &end)) {
			/* Skip over 0-len nodes */
			continue;
		}

		rule = tracker_parser_node_get_rule (node);

		if (tracker_grammar_rule_is_a (rule, RULE_TYPE_TERMINAL,
		                               TERMINAL_TYPE_PARAMETERIZED_VAR)) {
			gchar *param_name;
			const GValue *value;

			param_name = g_strndup (&query[start], end - start);
			value = g_hash_table_lookup (bindings, &param_name[1]);
			if (!value) {
				g_set_error (error,
				             TRACKER_SPARQL_ERROR,
				             TRACKER_SPARQL_ERROR_PARSE,
				             "No binding found for variable %s",
				             param_name);
				g_string_free (str, TRUE);
				g_free (param_name);
				return NULL;
			}

			append_gvalue (str, value);
			g_free (param_name);
		} else {
			g_string_append_len (str,
			                     &query[start],
			                     end - start);
		}

		g_string_append_c (str, ' ');
	}

	return g_string_free (str, FALSE);
}

static TrackerSparqlCursor *
execute_statement (TrackerSparqlStatement  *stmt,
                   GHashTable              *bindings,
                   GCancellable            *cancellable,
                   GError                 **error)
{
	TrackerSparqlCursor *cursor;
	gchar *rewritten_query = NULL;

	if (g_hash_table_size (bindings) > 0) {
		rewritten_query = apply_bindings (stmt, bindings, error);
		if (!rewritten_query)
			return NULL;
	}

	cursor = tracker_sparql_connection_query (tracker_sparql_statement_get_connection (stmt),
	                                          rewritten_query ? rewritten_query :
	                                          tracker_sparql_statement_get_sparql (stmt),
	                                          cancellable,
	                                          error);
	g_free (rewritten_query);

	return cursor;
}

TrackerSparqlCursor *
tracker_remote_statement_execute (TrackerSparqlStatement  *stmt,
                                  GCancellable            *cancellable,
                                  GError                 **error)
{
	TrackerRemoteStatement *remote_stmt = TRACKER_REMOTE_STATEMENT (stmt);

	return execute_statement (stmt, remote_stmt->bindings, cancellable, error);
}

static void
execute_in_thread (GTask        *task,
                   gpointer      object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
	TrackerSparqlCursor *cursor;
	GHashTable *bindings = task_data;
	GError *error = NULL;

	if (g_task_return_error_if_cancelled (task))
		return;

	cursor = execute_statement (object,
	                            bindings,
	                            g_task_get_cancellable (task),
	                            &error);
	if (error)
		g_task_return_error (task, error);
	else
		g_task_return_pointer (task, cursor, g_object_unref);

	g_object_unref (task);
}

static void
free_gvalue (gpointer data)
{
	g_value_unset (data);
	g_free (data);
}

static GHashTable *
create_bindings_ht (void)
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

	copy = create_bindings_ht ();
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
tracker_remote_statement_execute_async (TrackerSparqlStatement *stmt,
                                        GCancellable           *cancellable,
                                        GAsyncReadyCallback     callback,
                                        gpointer                user_data)
{
	TrackerRemoteStatement *remote_stmt = TRACKER_REMOTE_STATEMENT (stmt);
	GHashTable *bindings;
	GTask *task;

	bindings = copy_values_deep (remote_stmt->bindings);

	task = g_task_new (stmt, cancellable, callback, user_data);
	g_task_set_task_data (task, bindings, (GDestroyNotify) g_hash_table_unref);
	g_task_run_in_thread (task, execute_in_thread);
}

static TrackerSparqlCursor *
tracker_remote_statement_execute_finish (TrackerSparqlStatement  *stmt,
                                         GAsyncResult            *res,
                                         GError                 **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
tracker_remote_statement_clear_bindings (TrackerSparqlStatement *stmt)
{
	TrackerRemoteStatement *remote_stmt = TRACKER_REMOTE_STATEMENT (stmt);

	g_hash_table_remove_all (remote_stmt->bindings);
}

static void
serialize_cb (GObject      *source,
              GAsyncResult *res,
              gpointer      user_data)
{
	GInputStream *istream;
	GError *error = NULL;
	GTask *task = user_data;

	istream = tracker_sparql_connection_serialize_finish (TRACKER_SPARQL_CONNECTION (source),
	                                                      res, &error);
	if (error)
		g_task_return_error (task, error);
	else
		g_task_return_pointer (task, istream, g_object_unref);

	g_object_unref (task);
}

static void
tracker_remote_statement_serialize_async (TrackerSparqlStatement *stmt,
                                          TrackerSerializeFlags   flags,
                                          TrackerRdfFormat        format,
                                          GCancellable           *cancellable,
                                          GAsyncReadyCallback     callback,
                                          gpointer                user_data)
{
	TrackerRemoteStatement *remote_stmt = TRACKER_REMOTE_STATEMENT (stmt);
	gchar *rewritten_query = NULL;
	GError *error = NULL;
	GTask *task;

	task = g_task_new (stmt, cancellable, callback, user_data);

	if (g_hash_table_size (remote_stmt->bindings) > 0) {
		rewritten_query = apply_bindings (stmt,
		                                  remote_stmt->bindings,
		                                  &error);
		if (!rewritten_query) {
			g_task_return_error (task, error);
			g_object_unref (task);
			return;
		}
	}

	tracker_sparql_connection_serialize_async (tracker_sparql_statement_get_connection (stmt),
	                                           flags,
	                                           format,
	                                           rewritten_query ? rewritten_query :
	                                           tracker_sparql_statement_get_sparql (stmt),
	                                           cancellable,
	                                           serialize_cb,
	                                           task);
	g_free (rewritten_query);
}

static GInputStream *
tracker_remote_statement_serialize_finish (TrackerSparqlStatement  *stmt,
                                           GAsyncResult            *res,
                                           GError                 **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
tracker_remote_statement_class_init (TrackerRemoteStatementClass *klass)
{
	TrackerSparqlStatementClass *stmt_class = TRACKER_SPARQL_STATEMENT_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_remote_statement_finalize;

	stmt_class->bind_int = tracker_remote_statement_bind_int;
	stmt_class->bind_boolean = tracker_remote_statement_bind_boolean;
	stmt_class->bind_string = tracker_remote_statement_bind_string;
	stmt_class->bind_double = tracker_remote_statement_bind_double;
	stmt_class->bind_datetime = tracker_remote_statement_bind_datetime;
	stmt_class->bind_langstring = tracker_remote_statement_bind_langstring;
	stmt_class->execute = tracker_remote_statement_execute;
	stmt_class->execute_async = tracker_remote_statement_execute_async;
	stmt_class->execute_finish = tracker_remote_statement_execute_finish;
	stmt_class->clear_bindings = tracker_remote_statement_clear_bindings;
	stmt_class->serialize_async = tracker_remote_statement_serialize_async;
	stmt_class->serialize_finish = tracker_remote_statement_serialize_finish;
}

static void
tracker_remote_statement_init (TrackerRemoteStatement *stmt)
{
	stmt->bindings = create_bindings_ht ();
}

TrackerSparqlStatement *
tracker_remote_statement_new (TrackerSparqlConnection  *conn,
                              const gchar              *query,
                              GError                  **error)
{
	TrackerRemoteStatement *remote_stmt;
	TrackerSparqlStatement *stmt;

	stmt = g_object_new (TRACKER_TYPE_REMOTE_STATEMENT,
	                            "connection", conn,
	                            "sparql", query,
	                            NULL);
	remote_stmt = TRACKER_REMOTE_STATEMENT (stmt);
	remote_stmt->parser_tree =
		tracker_sparql_parse_query (tracker_sparql_statement_get_sparql (stmt),
		                            -1, NULL, error);
	if (!remote_stmt->parser_tree) {
		g_object_unref (stmt);
		return NULL;
	}

	return stmt;
}
