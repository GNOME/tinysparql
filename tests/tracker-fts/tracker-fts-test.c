/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>

#include <sqlite3.h>

#include <glib.h>
#include <glib-object.h>

static gint
callback (void	 *NotUsed,
	  gint	  argc,
	  gchar **argv,
	  gchar **azColName)
{
	gint i;

	for (i = 0; i < argc; i++) {
		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}

	printf("\n");

	return 0;
}

static void
exec_sql (sqlite3     *db,
	  const gchar *sql)
{
	gchar *zErrMsg;
	gint   rc;

	rc = sqlite3_exec (db, sql , callback, 0, &zErrMsg);

	if (rc != SQLITE_OK) {
		g_printerr ("SQL error: %s\n", zErrMsg);
		sqlite3_free (zErrMsg);
	}
}

int
main (int argc, char **argv)
{
	sqlite3  *db;
	gint	  rc;
	gboolean  db_exists = FALSE;
	gchar	 *st = NULL;
	gchar	 *sql;

	g_type_init ();
	g_thread_init (NULL);

	/* FOR NOW! Return EXIT_SUCCESS (martyn)
	 *
	 * This has to work with no parameters. These tests are for
	 * the unit tests, for any tests which are not for the Glib
	 * unit test frame work, we should be adding those binaries to
	 * the utils/ directory.
	 */
	return EXIT_SUCCESS;

	if (argc != 2) {
		g_printerr ("Usage: %s MATCH_TERM\n", argv[0]);
		g_printerr ("EG: %s stew\n", argv[0]);
		return EXIT_FAILURE;
	}

	db_exists = g_file_test ("/tmp/test.db", G_FILE_TEST_EXISTS);

	rc = sqlite3_open ("/tmp/test.db", &db);
	if (rc) {
		g_printerr ("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return EXIT_FAILURE;
	}

	sqlite3_enable_load_extension (db, 1);
	sqlite3_load_extension (db, "tracker-fts.so", NULL, &st);

	if (st) {
		fprintf(stderr, "SQL error: %s\n", st);
		sqlite3_free(st);
	}

	if (!db_exists) {
		exec_sql (db, "create virtual table recipe using trackerfts (name, ingredients)");
		exec_sql (db, "insert into recipe (name, ingredients) values ('broccoli stew', 'broccoli,peppers,cheese and tomatoes')");
		exec_sql (db, "insert into recipe (name, ingredients) values ('pumpkin stew', 'pumpkin,onions,garlic and celery')");
		exec_sql (db, "insert into recipe (name, ingredients) values ('broccoli pie', 'broccoli,cheese,onions and flour.')");
		exec_sql (db, "insert into recipe (name, ingredients) values ('pumpkin pie', 'pumpkin,sugar,flour and butter.')");
	}

	sql = g_strdup_printf ("select rowid, name, ingredients, snippet(recipe), offsets(recipe) from recipe where recipe match '%s'", argv[1]);
	exec_sql (db, sql);
	g_free (sql);

	sqlite3_close(db);

	return EXIT_SUCCESS;
}

