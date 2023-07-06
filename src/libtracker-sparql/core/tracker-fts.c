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
#include "tracker-ontologies.h"

static gboolean
has_fts_properties (TrackerOntologies *ontologies)
{
	TrackerProperty **properties;
	guint i, len;

	properties = tracker_ontologies_get_properties (ontologies, &len);

	for (i = 0; i < len; i++) {
		if (tracker_property_get_fulltext_indexed (properties[i]))
			return TRUE;
	}

	return FALSE;
}

static gchar **
get_fts_properties (TrackerOntologies *ontologies)
{
	TrackerProperty **properties;
	GArray *property_names;
	guint i, len;

	property_names = g_array_new (TRUE, FALSE, sizeof (gchar *));
	properties = tracker_ontologies_get_properties (ontologies, &len);

	for (i = 0; i < len; i++) {
		gchar *column;

		if (!tracker_property_get_fulltext_indexed (properties[i]))
			continue;

		column = g_strdup (tracker_property_get_name (properties[i]));
		g_array_append_val (property_names, column);
	}

	return (gchar **) g_array_free (property_names, FALSE);
}

gboolean
tracker_fts_init_db (sqlite3                *db,
                     TrackerDBInterface     *interface,
                     TrackerDBManagerFlags   flags,
                     TrackerOntologies      *ontologies,
                     GError                **error)
{
	gchar **property_names;
	gboolean retval;

	property_names = get_fts_properties (ontologies);
	retval = tracker_tokenizer_initialize (db, interface, flags, (const gchar **) property_names, error);
	g_strfreev (property_names);

	return retval;
}

gboolean
tracker_fts_create_table (sqlite3            *db,
                          const gchar        *database,
                          gchar              *table_name,
                          TrackerOntologies  *ontologies,
                          GError            **error)
{
	GString *str, *from, *fts, *column_names;
	TrackerProperty **properties;
	GHashTable *tables;
	guint i, len;
	gint rc;

	if (!has_fts_properties (ontologies))
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

	column_names = g_string_new (NULL);

	tables = g_hash_table_new (g_str_hash, g_str_equal);
	properties = tracker_ontologies_get_properties (ontologies, &len);

	for (i = 0; i < len; i++) {
		const gchar *name, *table_name;

		if (!tracker_property_get_fulltext_indexed (properties[i]))
			continue;

		name = tracker_property_get_name (properties[i]);
		table_name = tracker_property_get_table_name (properties[i]);

		if (tracker_property_get_multiple_values (properties[i])) {
			g_string_append_printf (str, ", group_concat(\"%s\".\"%s\")",
			                        table_name, name);
		} else {
			g_string_append_printf (str, ", \"%s\".\"%s\"",
			                        table_name, name);
		}

		g_string_append_printf (str, " AS \"%s\" ", name);
		g_string_append_printf (column_names, "\"%s\", ", name);

		if (!g_hash_table_contains (tables, table_name)) {
			g_string_append_printf (from, "LEFT OUTER JOIN \"%s\".\"%s\" ON "
			                        " \"rdfs:Resource\".ID = \"%s\".ID ",
			                        database, table_name, table_name);
			g_hash_table_add (tables, (gpointer) tracker_property_get_table_name (properties[i]));
		}
	}

	g_hash_table_unref (tables);

	g_string_append_printf (from, "WHERE COALESCE (%s NULL) IS NOT NULL ",
	                        column_names->str);
	g_string_append (from, "GROUP BY ROWID");
	g_string_append (str, from->str);
	g_string_free (from, TRUE);

	rc = sqlite3_exec(db, str->str, NULL, NULL, NULL);
	g_string_free (str, TRUE);

	if (rc != SQLITE_OK)
		goto error;

	g_string_append (fts, column_names->str);
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
	g_string_free (column_names, TRUE);

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
tracker_fts_alter_table (sqlite3            *db,
                         const gchar        *database,
                         gchar              *table_name,
                         TrackerOntologies  *ontologies,
                         GError            **error)
{
	gchar *query, *tmp_name;
	int rc;

	if (!has_fts_properties (ontologies))
		return TRUE;

	tmp_name = g_strdup_printf ("%s_TMP", table_name);

	if (!tracker_fts_create_table (db, database, tmp_name, ontologies, error)) {
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
tracker_fts_integrity_check (sqlite3      *db,
                             const gchar  *database,
                             const gchar  *table_name)
{
	gchar *query;
	gint rc;

	/* This special query rebuilds the tokens in the given FTS table */
	query = g_strdup_printf ("INSERT INTO \"%s\".%s(%s, rank) VALUES('integrity-check', 1)",
				 database, table_name, table_name);
	rc = sqlite3_exec (db, query, NULL, NULL, NULL);
	g_free (query);

	return rc == SQLITE_OK;
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
