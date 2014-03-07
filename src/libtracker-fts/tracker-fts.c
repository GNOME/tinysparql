/*
 * Copyright (C) 2011 Nokia <ivan.frade@nokia.com>
 *
 * Author: Carlos Garnacho <carlos@lanedo.com>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"
#include <sqlite3.h>
#include "tracker-fts-tokenizer.h"
#include "tracker-fts.h"

#ifndef HAVE_BUILTIN_FTS
#  include "fts3.h"
#endif

static gchar **property_names;

gboolean
tracker_fts_init (void) {
#ifdef HAVE_BUILTIN_FTS
	/* SQLite has all needed FTS4 features compiled in */
	return TRUE;
#else
	static gsize module_initialized = 0;
	int rc = SQLITE_OK;

	if (g_once_init_enter (&module_initialized)) {
		rc = sqlite3_auto_extension ((void (*) (void)) fts4_extension_init);
		g_once_init_leave (&module_initialized, (rc == SQLITE_OK));
	}

	return (module_initialized != 0);
#endif
}

static void
function_rank (sqlite3_context *context,
               int              argc,
               sqlite3_value   *argv[])
{
	guint *matchinfo, *weights;
	gdouble rank = 0;
	gint i, n_columns;

	if (argc != 2) {
		sqlite3_result_error(context,
		                     "wrong number of arguments to function rank()",
		                     -1);
		return;
	}

	matchinfo = (unsigned int *) sqlite3_value_blob (argv[0]);
	weights = (unsigned int *) sqlite3_value_blob (argv[1]);
	n_columns = matchinfo[0];

	for (i = 0; i < n_columns; i++) {
		if (matchinfo[i + 1] != 0) {
			rank += (gdouble) weights[i];
		}
	}

	sqlite3_result_double(context, rank);
}

static void
function_offsets (sqlite3_context *context,
                  int              argc,
                  sqlite3_value   *argv[])
{
	gchar *offsets;
	const gchar * const * names;
	gint offset_values[4];
	GString *result = NULL;
	gint i = 0;

	if (argc != 2) {
		sqlite3_result_error(context,
		                     "wrong number of arguments to function tracker_offsets()",
		                     -1);
		return;
	}

	offsets = (gchar *) sqlite3_value_text (argv[0]);
	names = sqlite3_value_blob (argv[1]);

	while (offsets && *offsets) {
		offset_values[i] = g_strtod (offsets, &offsets);

		/* All 4 values from the quartet have been gathered */
		if (i == 3) {
			if (!result) {
				result = g_string_new ("");
			} else {
				g_string_append_c (result, ',');
			}

			g_string_append_printf (result,
						"%s,%d",
						names[offset_values[0]],
						offset_values[2]);

		}

		i = (i + 1) % 4;
	}

	sqlite3_result_text (context,
			     (result) ? g_string_free (result, FALSE) : NULL,
			     -1, g_free);
}

static void
function_weights (sqlite3_context *context,
                  int              argc,
                  sqlite3_value   *argv[])
{
	static guint *weights = NULL;
	static GMutex mutex;
	int rc = SQLITE_DONE;

	g_mutex_lock (&mutex);

	if (G_UNLIKELY (weights == NULL)) {
		GArray *weight_array;
		sqlite3_stmt *stmt;
		sqlite3 *db;

		weight_array = g_array_new (FALSE, FALSE, sizeof (guint));
		db = sqlite3_context_db_handle (context);
		rc = sqlite3_prepare_v2 (db,
		                         "SELECT \"rdf:Property\".\"tracker:weight\" "
		                         "FROM \"rdf:Property\" "
		                         "WHERE \"rdf:Property\".\"tracker:fulltextIndexed\" = 1 "
		                         "ORDER BY \"rdf:Property\".ID ",
		                         -1, &stmt, NULL);

		while ((rc = sqlite3_step (stmt)) != SQLITE_DONE) {
			if (rc == SQLITE_ROW) {
				guint weight;
				weight = sqlite3_column_int (stmt, 0);
				g_array_append_val (weight_array, weight);
			} else if (rc != SQLITE_BUSY) {
				break;
			}
		}

		sqlite3_finalize (stmt);

		if (rc == SQLITE_DONE) {
			weights = (guint *) g_array_free (weight_array, FALSE);
		} else {
			g_array_free (weight_array, TRUE);
		}
	}

	g_mutex_unlock (&mutex);

	if (rc == SQLITE_DONE)
		sqlite3_result_blob (context, weights, sizeof (weights), NULL);
	else
		sqlite3_result_error_code (context, rc);
}

static void
function_property_names (sqlite3_context *context,
                         int              argc,
                         sqlite3_value   *argv[])
{
	sqlite3_result_blob (context, property_names, sizeof (property_names), NULL);
}

static void
tracker_fts_register_functions (sqlite3 *db)
{
	sqlite3_create_function (db, "tracker_rank", 2, SQLITE_ANY,
	                         NULL, &function_rank,
	                         NULL, NULL);
	sqlite3_create_function (db, "tracker_offsets", 2, SQLITE_ANY,
	                         NULL, &function_offsets,
	                         NULL, NULL);
	sqlite3_create_function (db, "fts_column_weights", 0, SQLITE_ANY,
	                         NULL, &function_weights,
	                         NULL, NULL);
	sqlite3_create_function (db, "fts_property_names", 0, SQLITE_ANY,
	                         NULL, &function_property_names,
	                         NULL, NULL);
}

static void
tracker_fts_init_property_names (GHashTable *tables)
{
	GHashTableIter iter;
	GList *c;
	GList *columns;
	GList *table_columns;
	gchar **ptr;

	columns = NULL;
	g_hash_table_iter_init (&iter, tables);
	while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &table_columns)) {
		columns = g_list_concat (columns, g_list_copy (table_columns));
	}

	ptr = property_names = g_new0 (gchar *, g_list_length (columns));
	for (c = columns; c!= NULL ; c = c->next) {
		*ptr = g_strdup (c->data);
		ptr ++;
	}

	g_list_free (columns);
}

gboolean
tracker_fts_init_db (sqlite3 *db,
                     GHashTable *tables)
{
	if (!tracker_tokenizer_initialize (db)) {
		return FALSE;
	}

	tracker_fts_init_property_names (tables);

	tracker_fts_register_functions (db);
	return TRUE;
}

gboolean
tracker_fts_create_table (sqlite3    *db,
                          gchar      *table_name,
                          GHashTable *tables,
                          GHashTable *grouped_columns)
{
	GString *str, *from, *fts;
	GHashTableIter iter;
	gchar *index_table;
	GList *columns;
	gint rc;

	/* Create view on tables/columns marked as FTS-indexed */
	g_hash_table_iter_init (&iter, tables);
	str = g_string_new ("CREATE VIEW fts_view AS SELECT Resource.ID as rowid ");
	from = g_string_new ("FROM Resource ");

	fts = g_string_new ("CREATE VIRTUAL TABLE ");
	g_string_append_printf (fts, "%s USING fts4(content=\"fts_view\", ",
				table_name);

	while (g_hash_table_iter_next (&iter, (gpointer *) &index_table,
				       (gpointer *) &columns)) {
		while (columns) {
			if (grouped_columns &&
			    g_hash_table_lookup (grouped_columns, columns->data)) {
				g_string_append_printf (str, ", group_concat(\"%s\".\"%s\")",
							index_table,
							(gchar *) columns->data);
			} else {
				g_string_append_printf (str, ", \"%s\".\"%s\"",
							index_table,
							(gchar *) columns->data);
			}

			g_string_append_printf (str, " AS \"%s\" ",
						(gchar *) columns->data);
			g_string_append_printf (fts, "\"%s\", ",
						(gchar *) columns->data);

			columns = columns->next;
		}

		g_string_append_printf (from, "LEFT OUTER JOIN \"%s\" ON "
					" Resource.ID = \"%s\".ID ",
					index_table, index_table);
	}

	g_string_append (str, from->str);
	g_string_free (from, TRUE);

	rc = sqlite3_exec(db, str->str, NULL, 0, NULL);
	g_string_free (str, TRUE);

	if (rc != SQLITE_OK) {
		return FALSE;
	}

	g_string_append (fts, "tokenize=TrackerTokenizer)");
	rc = sqlite3_exec(db, fts->str, NULL, 0, NULL);
	g_string_free (fts, TRUE);

	return (rc == SQLITE_OK);
}

gboolean
tracker_fts_alter_table (sqlite3    *db,
			 gchar      *table_name,
			 GHashTable *tables,
			 GHashTable *grouped_columns)
{
	gchar *query, *tmp_name;
	int rc;

	tmp_name = g_strdup_printf ("%s_TMP", table_name);

	query = g_strdup_printf ("DROP VIEW fts_view");
	rc = sqlite3_prepare_v2 (db, query, -1, NULL, NULL);

	if (!tracker_fts_create_table (db, tmp_name, tables, grouped_columns)) {
		g_free (tmp_name);
		g_free (query);
		return FALSE;
	}

	query = g_strdup_printf ("INSERT INTO %s (docid) SELECT docid FROM %s",
				 tmp_name, table_name);
	rc = sqlite3_prepare_v2 (db, query, -1, NULL, NULL);
	g_free (query);

	if (rc != SQLITE_OK) {
		g_free (tmp_name);
		return FALSE;
	}

	query = g_strdup_printf ("INSERT INTO %s(%s) VALUES('rebuild')",
				 tmp_name, tmp_name);
	rc = sqlite3_prepare_v2 (db, query, -1, NULL, NULL);
	g_free (query);

	if (rc != SQLITE_OK) {
		g_free (tmp_name);
		return FALSE;
	}

	query = g_strdup_printf ("ALTER TABLE %s RENAME TO %s",
				 tmp_name, table_name);
	rc = sqlite3_prepare_v2 (db, query, -1, NULL, NULL);
	g_free (query);
	g_free (tmp_name);


	if (rc != SQLITE_OK) {
		g_free (tmp_name);
		return FALSE;
	}

	return TRUE;
}
