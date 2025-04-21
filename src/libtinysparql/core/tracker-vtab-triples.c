/*
 * Copyright (C) 2019, Red Hat Inc.
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

#include "tracker-ontologies.h"
#include "tracker-vtab-triples.h"
#include "tracker-rowid.h"

/* Avoid casts everywhere. */
#define sqlite3_value_text(x) ((const gchar *) sqlite3_value_text(x))
#define sqlite3_column_text(x, y) ((const gchar *) sqlite3_column_text(x, y))

/* Define some constraints for older SQLite, we will never get
 * those in older versions, and simplifies checks in code.
 */
#if SQLITE_VERSION_NUMBER<=3021000
#define SQLITE_INDEX_CONSTRAINT_NE        68
#define SQLITE_INDEX_CONSTRAINT_ISNOTNULL 70
#define SQLITE_INDEX_CONSTRAINT_ISNULL    71
#endif

/* Properties are additional columns after graph and rowid */
#define FIRST_PROPERTY_COLUMN 2

enum {
	COL_GRAPH,
	COL_SUBJECT,
	COL_PREDICATE,
	COL_OBJECT,
	COL_OBJECT_TYPE,
	N_COLS
};

enum {
	IDX_COL_GRAPH           = 1 << 0,
	IDX_COL_SUBJECT         = 1 << 1,
	IDX_COL_PREDICATE       = 1 << 2,
	IDX_MATCH_GRAPH_NEG     = 1 << 3,
	IDX_MATCH_SUBJECT_NEG   = 1 << 4,
	IDX_MATCH_PREDICATE_NEG = 1 << 5,
};

enum {
	WEIGHT_GRAPH     = 1 << 10,
	WEIGHT_PREDICATE = 1 << 20,
	WEIGHT_SUBJECT   = 1 << 30,
};

typedef struct {
	sqlite3 *db;
	TrackerDataManager *data_manager;
} TrackerTriplesModule;

typedef struct {
	struct sqlite3_vtab parent;
	TrackerTriplesModule *module;
	GList *cursors;
} TrackerTriplesVTab;

typedef struct {
	struct sqlite3_vtab_cursor parent;
	TrackerTriplesVTab *vtab;
	struct sqlite3_stmt *stmt;
	TrackerProperty **column_properties;
	guint n_alloc_column_properties;

	struct {
		sqlite3_value *graph;
		sqlite3_value *subject;
		sqlite3_value *predicate;
		sqlite3_value *object;
		guint idxFlags;
	} match;

	GHashTable *query_graphs;
	GList *properties;
	GList *classes;
	GList *graphs;

	const GList *cur_property;
	const GList *cur_class;
	const GList *cur_graph;
	gint column;

	guint64 rowid;
	guint finished : 1;
} TrackerTriplesCursor;

static void
tracker_triples_module_free (gpointer data)
{
	TrackerTriplesModule *module = data;

	g_free (module);
}

static void
tracker_triples_vtab_free (gpointer data)
{
	TrackerTriplesVTab *vtab = data;

	g_list_free (vtab->cursors);
	g_free (vtab);
}

static void
tracker_triples_cursor_reset (TrackerTriplesCursor *cursor)
{
	g_clear_pointer (&cursor->stmt, sqlite3_finalize);
	g_clear_pointer (&cursor->match.graph, sqlite3_value_free);
	g_clear_pointer (&cursor->match.subject, sqlite3_value_free);
	g_clear_pointer (&cursor->match.predicate, sqlite3_value_free);
	g_clear_pointer (&cursor->properties, g_list_free);
	g_clear_pointer (&cursor->classes, g_list_free);
	g_clear_pointer (&cursor->graphs, g_list_free);
	g_clear_pointer (&cursor->query_graphs, g_hash_table_unref);
	cursor->match.idxFlags = 0;
	cursor->rowid = 0;
	cursor->finished = FALSE;
}

static void
tracker_triples_cursor_free (gpointer data)
{
	TrackerTriplesCursor *cursor = data;

	tracker_triples_cursor_reset (cursor);
	g_clear_pointer (&cursor->column_properties, g_free);
	g_free (cursor);
}

static int
triples_connect (sqlite3            *db,
                 gpointer            data,
                 int                 argc,
                 const char *const  *argv,
                 sqlite3_vtab      **vtab_out,
                 char              **err_out)
{
	TrackerTriplesModule *module = data;
	TrackerTriplesVTab *vtab;
	int rc;

	vtab = g_new0 (TrackerTriplesVTab, 1);
	vtab->module = module;

	rc = sqlite3_declare_vtab (module->db,
	                           "CREATE TABLE x("
	                           "    graph INTEGER,"
	                           "    subject INTEGER, "
	                           "    predicate INTEGER, "
	                           "    object INTEGER, "
	                           "    object_type INTEGER "
	                           ")");

	if (rc == SQLITE_OK) {
		*vtab_out = &vtab->parent;
	} else {
		g_free (vtab);
	}

	return rc;
}

static int
triples_best_index (sqlite3_vtab       *vtab,
                    sqlite3_index_info *info)
{
	gboolean order_by_consumed = FALSE;
	int i, argv_idx = 1, idx = 0;
	int cost_divisor = 1;
	char *idx_str;

	idx_str = sqlite3_malloc (sizeof (char) * N_COLS);
	bzero (idx_str, sizeof (char) * N_COLS);

	for (i = 0; i < info->nConstraint; i++) {
		struct {
			int mask;
			int negated_mask;
		} masks [] = {
			{ IDX_COL_GRAPH, IDX_MATCH_GRAPH_NEG },
			{ IDX_COL_SUBJECT, IDX_MATCH_SUBJECT_NEG },
			{ IDX_COL_PREDICATE, IDX_MATCH_PREDICATE_NEG },
			{ 0, 0 },
		};

		if (!info->aConstraint[i].usable)
			continue;

		/* We let object be matched in upper layers, where proper
		 * translation to strings can be done.
		 */
		if (info->aConstraint[i].iColumn == COL_OBJECT ||
		    info->aConstraint[i].iColumn == COL_OBJECT_TYPE)
			continue;

		/* We can only check for (in)equality */
		if (info->aConstraint[i].op != SQLITE_INDEX_CONSTRAINT_EQ &&
		    info->aConstraint[i].op != SQLITE_INDEX_CONSTRAINT_NE &&
		    info->aConstraint[i].op != SQLITE_INDEX_CONSTRAINT_ISNULL &&
		    info->aConstraint[i].op != SQLITE_INDEX_CONSTRAINT_ISNOTNULL) {
			sqlite3_free (idx_str);
			return SQLITE_ERROR;
		}

		/* idxNum encodes the used columns and their operators */
		idx |= masks[info->aConstraint[i].iColumn].mask;

		if (info->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_NE ||
		    info->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_ISNOTNULL)
			idx |= masks[info->aConstraint[i].iColumn].negated_mask;

		/* idxStr stores the mapping between columns and filter arguments */
		idx_str[info->aConstraint[i].iColumn] = argv_idx - 1;
		info->aConstraintUsage[i].argvIndex = argv_idx;
		info->aConstraintUsage[i].omit = FALSE;
		argv_idx++;

		if (info->aConstraint[i].iColumn == COL_SUBJECT)
			cost_divisor |= WEIGHT_SUBJECT;
		else if (info->aConstraint[i].iColumn == COL_PREDICATE)
			cost_divisor |= WEIGHT_PREDICATE;
		else if (info->aConstraint[i].iColumn == COL_GRAPH)
			cost_divisor |= WEIGHT_GRAPH;
	}

	info->idxNum = idx;
	info->orderByConsumed = order_by_consumed;
	info->idxStr = idx_str;
	info->needToFreeIdxStr = TRUE;
	info->estimatedCost = info->estimatedCost / cost_divisor;

	return SQLITE_OK;
}

static int
triples_disconnect (sqlite3_vtab *vtab)
{
	tracker_triples_vtab_free (vtab);
	return SQLITE_OK;
}

static int
triples_open (sqlite3_vtab         *vtab_sqlite,
	      sqlite3_vtab_cursor **cursor_ret)
{
	TrackerTriplesVTab *vtab = (TrackerTriplesVTab *) vtab_sqlite;
	TrackerTriplesCursor *cursor;

	cursor = g_new0 (TrackerTriplesCursor, 1);
	cursor->vtab = vtab;
	vtab->cursors = g_list_prepend (vtab->cursors, cursor);

	*cursor_ret = &cursor->parent;
	return SQLITE_OK;
}

static int
triples_close (sqlite3_vtab_cursor *vtab_cursor)
{
	TrackerTriplesCursor *cursor = (TrackerTriplesCursor *) vtab_cursor;
	TrackerTriplesVTab *vtab = cursor->vtab;

	vtab->cursors = g_list_remove (vtab->cursors, cursor);
	tracker_triples_cursor_free (cursor);
	return SQLITE_OK;
}

static void
collect_tables (TrackerTriplesCursor *cursor)
{
	TrackerOntologies *ontologies;
	TrackerProperty *property = NULL;
	const gchar *uri = NULL;
	gboolean pred_negated;

	ontologies = tracker_data_manager_get_ontologies (cursor->vtab->module->data_manager);
	pred_negated = !!(cursor->match.idxFlags & IDX_MATCH_PREDICATE_NEG);

	if (cursor->match.predicate) {
		uri = tracker_ontologies_get_uri_by_id (ontologies,
		                                        sqlite3_value_int64 (cursor->match.predicate));
	}

	if (uri) {
		property = tracker_ontologies_get_property_by_uri (ontologies, uri);
	}

	if (property && !pred_negated) {
		cursor->properties = g_list_prepend (cursor->properties, property);
	} else {
		TrackerProperty **properties;
		guint n_properties, i;

		properties = tracker_ontologies_get_properties (ontologies, &n_properties);
		for (i = 0; i < n_properties; i++) {
			if (tracker_property_get_multiple_values (properties[i])) {
				if (pred_negated && property == properties[i])
					continue;

				cursor->properties = g_list_prepend (cursor->properties,
				                                     properties[i]);
			} else {
				TrackerClass *class;

				class = tracker_property_get_domain (properties[i]);

				if (!g_list_find (cursor->classes, class))
					cursor->classes = g_list_prepend (cursor->classes, class);
			}
		}
	}
}

static int
compare_graphs (TrackerRowid *graph1,
                TrackerRowid *graph2)
{
	return (int) (*graph1 - *graph2);
}

static int
collect_graphs (TrackerTriplesCursor *cursor)
{
	sqlite3_stmt *stmt;
	int rc;

	rc = sqlite3_prepare_v2 (cursor->vtab->module->db,
	                         "SELECT ID, "
	                         "       (SELECT Uri from Resource where Resource.ID = Graph.ID) "
	                         "FROM Graph",
	                         -1, &stmt, 0);
	if (rc != SQLITE_OK)
		return rc;

	cursor->query_graphs = g_hash_table_new_full (tracker_rowid_hash,
	                                              tracker_rowid_equal,
	                                              (GDestroyNotify) tracker_rowid_free,
	                                              g_free);

	while ((rc = sqlite3_step (stmt)) == SQLITE_ROW) {
		const gchar *uri;
		TrackerRowid id;

		id = sqlite3_column_int64 (stmt, 0);
		uri = sqlite3_column_text (stmt, 1);

		if (g_strcmp0 (uri, TRACKER_DEFAULT_GRAPH) == 0)
			uri = NULL;

		if (cursor->match.graph) {
			gboolean negated = !!(cursor->match.idxFlags & IDX_MATCH_GRAPH_NEG);
			gboolean equals = (sqlite3_value_int64 (cursor->match.graph) == id);

			if (equals == negated)
				continue;
		}

		g_hash_table_insert (cursor->query_graphs,
		                     tracker_rowid_copy (&id),
		                     g_strdup (uri));
	}

	if (rc == SQLITE_DONE) {
		cursor->graphs = g_hash_table_get_keys (cursor->query_graphs);
		cursor->graphs = g_list_sort (cursor->graphs, (GCompareFunc) compare_graphs);
	}

	sqlite3_finalize (stmt);

	return rc;
}

static void
add_arg_check (GString       *str,
	       sqlite3_value *value,
	       gboolean       negated,
	       const gchar   *var_name)
{
	if (sqlite3_value_type (value) == SQLITE_NULL) {
		if (negated)
			g_string_append (str, "IS NOT NULL ");
		else
			g_string_append (str, "IS NULL ");
	} else {
		if (negated)
			g_string_append_printf (str, "!= %s ", var_name);
		else
			g_string_append_printf (str, "= %s ", var_name);
	}
}

static void
bind_arg (sqlite3_stmt  *stmt,
	  sqlite3_value *value,
	  const gchar   *var_name)
{
	gint idx;

	if (sqlite3_value_type (value) == SQLITE_NULL)
		return;

	idx = sqlite3_bind_parameter_index (stmt, var_name);
	if (idx == 0)
		return;

	sqlite3_bind_value (stmt, idx, value);
}

static TrackerProperty *
get_column_property (TrackerTriplesCursor *cursor,
                     int                   n_col)
{
	TrackerOntologies *ontologies;
	TrackerProperty *property;
	const gchar *col_name;
	int n_cols;

	n_cols = sqlite3_column_count (cursor->stmt);
	g_assert ((guint) n_cols <= cursor->n_alloc_column_properties);

	if (n_col < 0 || n_col >= n_cols)
		return NULL;

	property = cursor->column_properties[n_col];

	if (!property) {
		ontologies = tracker_data_manager_get_ontologies (cursor->vtab->module->data_manager);
		col_name = sqlite3_column_name (cursor->stmt, n_col);
		property = tracker_ontologies_get_property_by_uri (ontologies, col_name);
		cursor->column_properties[n_col] = property;
	}

	return property;
}

static gboolean
iterate_next_stmt (TrackerTriplesCursor  *cursor,
                   const gchar          **graph,
                   TrackerRowid          *graph_id,
                   TrackerClass         **class,
                   TrackerProperty      **property)
{
	TrackerRowid *id;

	*graph_id = 0;
	*graph = NULL;
	*class = NULL;
	*property = NULL;

	if (cursor->finished)
		return FALSE;

	if (cursor->cur_class)
		cursor->cur_class = cursor->cur_class->next;
	else if (cursor->cur_property)
		cursor->cur_property = cursor->cur_property->next;

	if (!cursor->cur_class && !cursor->cur_property) {
		if (cursor->cur_graph)
			cursor->cur_graph = cursor->cur_graph->next;
		else
			cursor->cur_graph = cursor->graphs;

		cursor->cur_class = cursor->classes;
		cursor->cur_property = cursor->properties;
	}

	if (!cursor->cur_graph)
		return FALSE;

	id = cursor->cur_graph->data;
	*graph_id = *id;
	*graph = g_hash_table_lookup (cursor->query_graphs, id);

	if (cursor->cur_class)
		*class = cursor->cur_class->data;
	else if (cursor->cur_property)
		*property = cursor->cur_property->data;

	return TRUE;
}

static int
init_stmt (TrackerTriplesCursor *cursor)
{
	TrackerOntologies *ontologies;
	TrackerProperty *property;
	TrackerClass *class;
	const gchar *graph;
	TrackerRowid graph_id;
	int rc = SQLITE_DONE;

	ontologies = tracker_data_manager_get_ontologies (cursor->vtab->module->data_manager);

	while (iterate_next_stmt (cursor, &graph, &graph_id, &class, &property)) {
		GString *sql;

		sql = g_string_new (NULL);

		if (class) {
			TrackerProperty **properties;
			guint n_properties, i;

			g_string_append_printf (sql,
			                        "SELECT %" G_GINT64_FORMAT ", ROWID ",
			                        graph_id);


			properties = tracker_ontologies_get_properties (ontologies, &n_properties);
			for (i = 0; i < n_properties; i++) {
				if (!tracker_property_get_multiple_values (properties[i]) &&
				    tracker_property_get_domain (properties[i]) == class) {
					g_string_append_printf (sql, ", \"%s\" ",
					                        tracker_property_get_name (properties[i]));
				}
			}

			g_string_append_printf (sql,
			                        "FROM \"%s%s%s\" AS t ",
			                        graph ? graph : "",
			                        graph ? "_" : "",
			                        tracker_class_get_name (class));
		} else if (property) {
			if (tracker_property_get_multiple_values (property)) {
				g_string_append_printf (sql,
				                        "SELECT %" G_GINT64_FORMAT ", * "
				                        "FROM \"%s%s%s\" AS t ",
				                        graph_id,
				                        graph ? graph : "",
				                        graph ? "_" : "",
				                        tracker_property_get_table_name (property));
			} else {
				g_string_append_printf (sql,
				                        "SELECT %" G_GINT64_FORMAT ", ROWID, \"%s\" "
				                        "FROM \"%s%s%s\" AS t ",
				                        graph_id,
				                        tracker_property_get_name (property),
				                        graph ? graph : "",
				                        graph ? "_" : "",
				                        tracker_property_get_table_name (property));
			}
		}

		if (cursor->match.subject) {
			g_string_append (sql, "WHERE t.ID ");
			add_arg_check (sql, cursor->match.subject,
			               !!(cursor->match.idxFlags & IDX_MATCH_SUBJECT_NEG),
			               "@s");
		}

		rc = sqlite3_prepare_v2 (cursor->vtab->module->db,
		                         sql->str, -1, &cursor->stmt, 0);
		g_string_free (sql, TRUE);

		if (rc == SQLITE_OK) {
			if (cursor->match.subject)
				bind_arg (cursor->stmt, cursor->match.subject, "@s");

			rc = sqlite3_step (cursor->stmt);
		}

		if (rc != SQLITE_DONE)
			break;

		g_clear_pointer (&cursor->stmt, sqlite3_finalize);
	}

	if (rc == SQLITE_ROW) {
		int columns;

		columns = sqlite3_column_count (cursor->stmt);

		if ((guint) columns > cursor->n_alloc_column_properties) {
			g_free (cursor->column_properties);
			cursor->column_properties =
				g_new0 (TrackerProperty*, columns);
			cursor->n_alloc_column_properties = columns;
		} else {
			bzero (cursor->column_properties,
			       sizeof (TrackerProperty*) *
			       cursor->n_alloc_column_properties);
		}
	}

	return rc;
}

static int
triples_filter (sqlite3_vtab_cursor  *vtab_cursor,
                int                   idx,
                const char           *idx_str,
                int                   argc,
                sqlite3_value       **argv)
{
	TrackerTriplesCursor *cursor = (TrackerTriplesCursor *) vtab_cursor;
	int rc;

	tracker_triples_cursor_reset (cursor);

	if (idx & IDX_COL_GRAPH) {
		int idx = idx_str[COL_GRAPH];
		cursor->match.graph = sqlite3_value_dup (argv[idx]);
	}

	if (idx & IDX_COL_SUBJECT) {
		int idx = idx_str[COL_SUBJECT];
		cursor->match.subject = sqlite3_value_dup (argv[idx]);
	}

	if (idx & IDX_COL_PREDICATE) {
		int idx = idx_str[COL_PREDICATE];
		cursor->match.predicate = sqlite3_value_dup (argv[idx]);
	}

	cursor->match.idxFlags = idx;

	if ((rc = collect_graphs (cursor)) != SQLITE_DONE)
		return rc;

	collect_tables (cursor);

	cursor->cur_graph = NULL;
	cursor->cur_class = NULL;
	cursor->cur_property = NULL;

	rc = init_stmt (cursor);
	cursor->column = FIRST_PROPERTY_COLUMN;

	if (rc == SQLITE_DONE)
		cursor->finished = TRUE;

	if (rc == SQLITE_ROW || rc == SQLITE_DONE)
		return SQLITE_OK;

	return rc;
}

static int
triples_next (sqlite3_vtab_cursor *vtab_cursor)
{
	TrackerTriplesCursor *cursor = (TrackerTriplesCursor *) vtab_cursor;
	int rc, column_count;

	cursor->rowid++;
	column_count = sqlite3_column_count (cursor->stmt);

	while (cursor->column < column_count) {
		cursor->column++;

		if ((cursor->match.idxFlags & IDX_MATCH_PREDICATE_NEG) != 0) {
			TrackerProperty *property;

			/* Single valued properties skip the "predicate != ..." here */
			property = get_column_property (cursor, cursor->column);

			if (property &&
			    sqlite3_value_int64 (cursor->match.predicate) ==
			    tracker_property_get_id (property))
				cursor->column++;
		}

		if (sqlite3_column_type (cursor->stmt, cursor->column) != SQLITE_NULL)
			break;
	}

	if (cursor->column < column_count)
		return SQLITE_OK;

	rc = sqlite3_step (cursor->stmt);
	cursor->column = FIRST_PROPERTY_COLUMN;

	if (rc == SQLITE_DONE) {
		g_clear_pointer (&cursor->stmt, sqlite3_finalize);
		rc = init_stmt (cursor);
	}

	if (rc != SQLITE_ROW)
		cursor->finished = TRUE;
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
		return rc;

	return SQLITE_OK;
}

static int
triples_eof (sqlite3_vtab_cursor *vtab_cursor)
{
	TrackerTriplesCursor *cursor = (TrackerTriplesCursor *) vtab_cursor;

	return cursor->finished;
}

static int
triples_column (sqlite3_vtab_cursor *vtab_cursor,
                sqlite3_context     *context,
                int                  n_col)
{
	TrackerTriplesCursor *cursor = (TrackerTriplesCursor *) vtab_cursor;
	sqlite3_value *value;
	TrackerProperty *property;

	switch (n_col) {
	case COL_GRAPH:
		value = sqlite3_column_value (cursor->stmt, 0);
		sqlite3_result_value (context, value);
		break;
	case COL_SUBJECT:
		value = sqlite3_column_value (cursor->stmt, 1);
		sqlite3_result_value (context, value);
		break;
	case COL_OBJECT:
		value = sqlite3_column_value (cursor->stmt, cursor->column);
		sqlite3_result_value (context, value);
		break;
	case COL_PREDICATE:
	case COL_OBJECT_TYPE:
		property = get_column_property (cursor, cursor->column);
		if (!property) {
			sqlite3_result_error_code (context, SQLITE_CORRUPT);
			break;
		}

		if (n_col == COL_PREDICATE)
			sqlite3_result_int64 (context, tracker_property_get_id (property));
		else
			sqlite3_result_int64 (context, tracker_property_get_data_type (property));
		break;
	}

	return SQLITE_OK;
}

static int
triples_rowid (sqlite3_vtab_cursor *vtab_cursor,
               sqlite_int64        *rowid_out)
{
	TrackerTriplesCursor *cursor = (TrackerTriplesCursor *) vtab_cursor;

	*rowid_out = cursor->rowid;
	return SQLITE_OK;
}

void
tracker_vtab_triples_init (sqlite3            *db,
                           TrackerDataManager *data_manager)
{
	TrackerTriplesModule *module;
	static const sqlite3_module triples_module = {
		2, /* version */
		NULL, /* create(), null because this is an eponymous-only table */
		triples_connect,
		triples_best_index,
		triples_disconnect,
		triples_disconnect, /* destroy(), can be the same since no real tables are created */
		triples_open,
		triples_close,
		triples_filter,
		triples_next,
		triples_eof,
		triples_column,
		triples_rowid,
		NULL, /* update */
		NULL, /* begin */
		NULL, /* sync */
		NULL, /* commit */
		NULL, /* rollback */
		NULL, /* find function */
		NULL, /* rename */
		NULL, /* savepoint */
		NULL, /* release */
		NULL, /* rollback to */
	};

	module = g_new0 (TrackerTriplesModule, 1);
	module->db = db;
	module->data_manager = data_manager;
	sqlite3_create_module_v2 (db, "tracker_triples", &triples_module,
	                          module, tracker_triples_module_free);
}
