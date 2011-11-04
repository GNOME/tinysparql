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

gboolean
tracker_fts_init_db (sqlite3 *db) {
	if (!tracker_tokenizer_initialize (db)) {
		return FALSE;
	}

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
