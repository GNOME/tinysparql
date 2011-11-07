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

#include <sqlite3.h>
#include "tracker-fts-tokenizer.h"
#include "tracker-fts.h"
#include "fts3.h"

gboolean
tracker_fts_init (void) {
	static gsize module_initialized = 0;
	int rc = SQLITE_OK;

	if (g_once_init_enter (&module_initialized)) {
		rc = sqlite3_auto_extension ((void (*) (void)) fts4_extension_init);
		g_once_init_leave (&module_initialized, (rc == SQLITE_OK));
	}

	return (module_initialized != 0);
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
	gchar *offsets, **names;
	gint offset_values[4];
	GString *result = NULL;
	gint i = 0;

	if (argc != 2) {
		sqlite3_result_error(context,
		                     "wrong number of arguments to function tracker_offsets()",
		                     -1);
		return;
	}

	offsets = sqlite3_value_text (argv[0]);
	names = (unsigned int *) sqlite3_value_blob (argv[1]);

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
	static gsize weights_initialized = 0;

	if (g_once_init_enter (&weights_initialized)) {
		GArray *weight_array;
		sqlite3_stmt *stmt;
		sqlite3 *db;
		int rc;

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
			}
		}

		if (rc == SQLITE_DONE) {
			rc = sqlite3_finalize (stmt);
		}

		weights = (guint *) g_array_free (weight_array, FALSE);
		g_once_init_leave (&weights_initialized, (rc == SQLITE_OK));
	}

	sqlite3_result_blob (context, weights, sizeof (weights), NULL);
}

static void
function_property_names (sqlite3_context *context,
                         int              argc,
                         sqlite3_value   *argv[])
{
	static gchar **names = NULL;
	static gsize names_initialized = 0;

	if (g_once_init_enter (&names_initialized)) {
		GPtrArray *names_array;
		sqlite3_stmt *stmt;
		sqlite3 *db;
		int rc;

		names_array = g_ptr_array_new ();
		db = sqlite3_context_db_handle (context);
		rc = sqlite3_prepare_v2 (db,
		                         "SELECT Uri "
		                         "FROM Resource "
		                         "JOIN \"rdf:Property\" "
		                         "ON Resource.ID = \"rdf:Property\".ID "
		                         "WHERE \"rdf:Property\".\"tracker:fulltextIndexed\" = 1 "
		                         "ORDER BY \"rdf:Property\".ID ",
		                         -1, &stmt, NULL);

		while ((rc = sqlite3_step (stmt)) != SQLITE_DONE) {
			if (rc == SQLITE_ROW) {
				const gchar *name;

				name = sqlite3_column_text (stmt, 0);
				g_ptr_array_add (names_array, g_strdup (name));
			}
		}

		if (rc == SQLITE_DONE) {
			rc = sqlite3_finalize (stmt);
		}

		names = (gchar **) g_ptr_array_free (names_array, FALSE);
		g_once_init_leave (&names_initialized, (rc == SQLITE_OK));
	}

	sqlite3_result_blob (context, names, sizeof (names), NULL);
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

gboolean
tracker_fts_init_db (sqlite3 *db) {
	if (!tracker_tokenizer_initialize (db)) {
		return FALSE;
	}

	tracker_fts_register_functions (db);
	return TRUE;
}

gboolean
tracker_fts_create_table (sqlite3  *db,
			  gchar    *table_name,
			  gchar   **column_names)
{
	GString *str;
	gint i, rc;

	str = g_string_new ("CREATE VIRTUAL TABLE ");
	g_string_append_printf (str, "%s USING fts4(", table_name);

	for (i = 0; column_names[i]; i++) {
		g_string_append_printf (str, "\"%s\", ", column_names[i]);
	}

	g_string_append (str, " tokenize=TrackerTokenizer)");

	rc = sqlite3_exec(db, str->str, NULL, 0, NULL);
	g_string_free (str, TRUE);

	return (rc == SQLITE_OK);
}

gboolean
tracker_fts_alter_table (sqlite3  *db,
			 gchar    *table_name,
			 gchar   **added_columns,
			 gchar   **removed_columns)
{
	GString *columns_str = NULL;
	GPtrArray *columns;
	sqlite3_stmt *stmt;
	gchar *query, *tmp_name;
	int rc, i;

	if (!added_columns && !removed_columns) {
		return TRUE;
	}

	query = g_strdup_printf ("PRAGMA table_info(%s)", table_name);
	rc = sqlite3_prepare_v2 (db, query, -1, &stmt, NULL);
	g_free (query);

	if (rc != SQLITE_OK) {
		return FALSE;
	}

	columns = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

	/* Fetch the old columns, don't add stuff in removed_columns */
	while ((rc = sqlite3_step (stmt)) != SQLITE_DONE) {
		if (rc == SQLITE_ROW) {
			const gchar *name;

			name = sqlite3_column_text (stmt, 1);

			for (i = 0; removed_columns && removed_columns[i]; i++) {
				if (g_strcmp0 (name, removed_columns[i]) == 0) {
					continue;
				}
			}

			g_ptr_array_add (columns, g_strdup (name));
		}
	}

	if (rc == SQLITE_DONE) {
		rc = sqlite3_finalize (stmt);
	}

	if (rc != SQLITE_OK) {
		g_ptr_array_free (columns, TRUE);
		return FALSE;
	}

	/* In columns we have the current columns, minus the removed columns,
	 * create the update we'll execute later on to dump data from one
	 * table to another.
	 */
	for (i = 0; i < columns->len; i++) {
		if (!columns_str) {
			columns_str = g_string_new ("");
		} else {
			g_string_append_c (columns_str, ',');
		}

		g_string_append_printf (columns_str, "\"%s\"",
					(gchar *) g_ptr_array_index (columns, i));
	}

	if (!columns_str) {
		g_ptr_array_free (columns, TRUE);
		return FALSE;
	}

	tmp_name = g_strdup_printf ("%s_TMP", table_name);

	query = g_strdup_printf ("INSERT INTO %s (%s) SELECT %s FROM %s",
				 tmp_name, columns_str->str,
				 columns_str->str, table_name);
	g_string_free (columns_str, TRUE);

	/* Now append stuff in added_columns and create the temporary table */
	for (i = 0; added_columns && added_columns[i]; i++) {
		g_ptr_array_add (columns, g_strdup (added_columns[i]));
	}

	/* Add trailing NULL */
	g_ptr_array_add (columns, NULL);

	if (!tracker_fts_create_table (db, tmp_name, (gchar **) columns->pdata)) {
		g_ptr_array_free (columns, TRUE);
		g_free (tmp_name);
		g_free (query);
		return FALSE;
	}

	/* Dump all content from one table to another */
	g_ptr_array_free (columns, TRUE);
	rc = sqlite3_exec(db, query, NULL, 0, NULL);
	g_free (query);

	if (rc != SQLITE_OK) {
		query = g_strdup_printf ("DROP TABLE %s", tmp_name);
		rc = sqlite3_exec(db, query, NULL, 0, NULL);
		g_free (query);
		g_free (tmp_name);
		return FALSE;
	}

	/* Drop the old table */
	query = g_strdup_printf ("DROP TABLE %s", table_name);
	rc = sqlite3_exec(db, query, NULL, 0, NULL);
	g_free (query);

	if (rc != SQLITE_OK) {
		/* FIXME: How can we leave such state? this is rather fatal */
		g_free (tmp_name);
		return FALSE;
	}

	/* And rename the previous one */
	query = g_strdup_printf ("ALTER TABLE %s RENAME TO %s",
				 tmp_name, table_name);
	rc = sqlite3_exec(db, query, NULL, 0, NULL);
	g_free (query);
	g_free (tmp_name);

	if (rc != SQLITE_OK) {
		return FALSE;
	}

	return TRUE;
}
