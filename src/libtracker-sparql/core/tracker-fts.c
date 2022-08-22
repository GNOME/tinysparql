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

#include <libtracker-common/tracker-common.h>

#include "tracker-fts-tokenizer.h"
#include "tracker-fts.h"

static gchar **
get_fts_properties (GHashTable  *tables)
{
	GList *table_columns;
	GArray *property_names;
	GList *keys, *l;

	keys = g_hash_table_get_keys (tables);
	keys = g_list_sort (keys, (GCompareFunc) strcmp);

	property_names = g_array_new (TRUE, FALSE, sizeof (gchar *));

	for (l = keys; l; l = l->next) {
		table_columns = g_hash_table_lookup (tables, l->data);

		while (table_columns) {
			gchar *str;

			str = g_strdup (table_columns->data);
			g_array_append_val (property_names, str);
			table_columns = table_columns->next;
		}
	}

	g_list_free (keys);

	return (gchar **) g_array_free (property_names, FALSE);
}

gboolean
tracker_fts_init_db (sqlite3                *db,
                     TrackerDBInterface     *interface,
                     TrackerDBManagerFlags   flags,
                     GHashTable             *tables,
                     GError                **error)
{
	gchar **property_names;
	gboolean retval;

	property_names = get_fts_properties (tables);
	retval = tracker_tokenizer_initialize (db, interface, flags, (const gchar **) property_names, error);
	g_strfreev (property_names);

	return retval;
}

gboolean
tracker_fts_create_table (sqlite3      *db,
                          const gchar  *database,
                          gchar        *table_name,
                          GHashTable   *tables,
                          GHashTable   *grouped_columns,
                          GError      **error)
{
	GString *str, *from, *fts;
	gchar *index_table;
	GList *columns, *keys, *l;
	gint rc;

	if (g_hash_table_size (tables) == 0)
		return TRUE;

	/* Create view on tables/columns marked as FTS-indexed */
	str = g_string_new ("CREATE VIEW ");
	g_string_append_printf (str, "\"%s\".fts_view AS SELECT \"rdfs:Resource\".ID as rowid ",
	                        database);
	from = g_string_new (NULL);
	g_string_append_printf (from, "FROM \"%s\".\"rdfs:Resource\" ", database);

	fts = g_string_new ("CREATE VIRTUAL TABLE ");
	g_string_append_printf (fts, "\"%s\".%s USING fts5(content=\"fts_view\", ",
				database, table_name);

	keys = g_hash_table_get_keys (tables);
	keys = g_list_sort (keys, (GCompareFunc) strcmp);

	for (l = keys; l; l = l->next) {
		index_table = l->data;
		columns = g_hash_table_lookup (tables, l->data);

		while (columns) {
			if (grouped_columns &&
			    g_hash_table_lookup (grouped_columns, index_table)) {
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

		g_string_append_printf (from, "LEFT OUTER JOIN \"%s\".\"%s\" ON "
					" \"rdfs:Resource\".ID = \"%s\".ID ",
					database, index_table, index_table);
	}

	g_list_free (keys);

	g_string_append (from, "GROUP BY ROWID");
	g_string_append (str, from->str);
	g_string_free (from, TRUE);

	rc = sqlite3_exec(db, str->str, NULL, NULL, NULL);
	g_string_free (str, TRUE);

	if (rc != SQLITE_OK)
		goto error;

	g_string_append (fts, "tokenize=TrackerTokenizer)");
	rc = sqlite3_exec(db, fts->str, NULL, NULL, NULL);

	if (rc != SQLITE_OK)
		goto error;

	str = g_string_new (NULL);
	g_string_append_printf (str,
	                        "INSERT INTO %s(%s, rank) VALUES('rank', 'tracker_rank()')",
	                        table_name, table_name);
	rc = sqlite3_exec (db, str->str, NULL, NULL, NULL);
	g_string_free (str, TRUE);

error:
	g_string_free (fts, TRUE);

	if (rc != SQLITE_OK) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_OPEN_ERROR,
		             "%s", sqlite3_errstr (rc));
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_fts_delete_table (sqlite3      *db,
                          const gchar  *database,
                          gchar        *table_name,
                          GError      **error)
{
	gchar *query;
	int rc;

	query = g_strdup_printf ("DROP VIEW IF EXISTS \"%s\".fts_view", database);
	rc = sqlite3_exec (db, query, NULL, NULL, NULL);
	g_free (query);

	if (rc == SQLITE_OK) {
		query = g_strdup_printf ("DROP TABLE IF EXISTS \"%s\".%s",
					 database, table_name);
		rc = sqlite3_exec (db, query, NULL, NULL, NULL);
		g_free (query);
	}

	if (rc != SQLITE_OK) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_OPEN_ERROR,
		             "%s", sqlite3_errstr (rc));
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_fts_alter_table (sqlite3      *db,
                         const gchar  *database,
                         gchar        *table_name,
                         GHashTable   *tables,
                         GHashTable   *grouped_columns,
                         GError      **error)
{
	gchar *query, *tmp_name;
	int rc;

	if (g_hash_table_size (tables) == 0)
		return TRUE;

	tmp_name = g_strdup_printf ("%s_TMP", table_name);

	if (!tracker_fts_create_table (db, database, tmp_name, tables, grouped_columns, error)) {
		g_free (tmp_name);
		return FALSE;
	}

	query = g_strdup_printf ("INSERT INTO \"%s\".%s (rowid) SELECT rowid FROM fts_view",
				 database, tmp_name);
	rc = sqlite3_exec (db, query, NULL, NULL, NULL);
	g_free (query);

	if (rc != SQLITE_OK)
		goto error;

	query = g_strdup_printf ("INSERT INTO \"%s\".%s(%s) VALUES('rebuild')",
				 database, tmp_name, tmp_name);
	rc = sqlite3_exec (db, query, NULL, NULL, NULL);
	g_free (query);

	if (rc != SQLITE_OK)
		goto error;

	query = g_strdup_printf ("ALTER TABLE \"%s\".%s RENAME TO %s",
				 database, tmp_name, table_name);
	rc = sqlite3_exec (db, query, NULL, NULL, NULL);
	g_free (query);

error:
	g_free (tmp_name);

	if (rc != SQLITE_OK) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_OPEN_ERROR,
		             "%s", sqlite3_errstr (rc));
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_fts_rebuild_tokens (sqlite3      *db,
			    const gchar  *database,
                            const gchar  *table_name,
                            GError      **error)
{
	gchar *query;
	gint rc;

	/* This special query rebuilds the tokens in the given FTS table */
	query = g_strdup_printf ("INSERT INTO \"%s\".%s(%s) VALUES('rebuild')",
				 database, table_name, table_name);
	rc = sqlite3_exec(db, query, NULL, NULL, NULL);
	g_free (query);

	if (rc != SQLITE_OK) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_OPEN_ERROR,
		             "%s", sqlite3_errstr (rc));
		return FALSE;
	}

	return TRUE;
}
