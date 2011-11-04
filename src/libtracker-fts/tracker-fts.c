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

		weights = g_array_free (weight_array, FALSE);
		g_once_init_leave (&weights_initialized, (rc == SQLITE_OK));
	}

	sqlite3_result_blob (context, weights, sizeof (weights), NULL);
}


static void
tracker_fts_register_functions (sqlite3 *db)
{
	sqlite3_create_function (db, "tracker_rank", 2, SQLITE_ANY,
	                         NULL, &function_rank,
	                         NULL, NULL);
	sqlite3_create_function (db, "fts_column_weights", 0, SQLITE_ANY,
	                         NULL, &function_weights,
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
