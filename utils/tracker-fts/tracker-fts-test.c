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
callback (void   *NotUsed, 
          gint    argc, 
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
	gint      rc;
	gboolean  db_exists = FALSE;
	gchar    *st = NULL;
        gchar    *sql;

	g_type_init ();
        g_thread_init (NULL);
        
	if (argc != 2) {
		g_printerr ("Usage: %s MATCH_TERM\n", argv[0]);
		g_printerr ("EG: %s stew\n", argv[0]);
		return EXIT_FAILURE;
	}
	g_unlink ("/tmp/test.db");
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
		exec_sql (db, "create virtual table recipe using trackerfts (cat, col_default, col_1, col_2)");
		exec_sql (db, "insert into recipe (cat, col_default, col_1, col_2) values (3, 'broccoli stew stew stew', 'broccoli,peppers,cheese and tomatoes', 'mix them all up and have fun')");
		exec_sql (db, "insert into recipe (cat, col_default, col_1, col_2) values (4, 'pumpkin stew stew stew', 'pumpkin,onions,garlic and celery', 'spread them thinly')");
		exec_sql (db, "insert into recipe (cat, col_default, col_1, col_2) values (2, 'broccoli pie stew', 'broccoli,cheese,onions and flour.', 'mash em up')");
		exec_sql (db, "insert into recipe (cat, col_default, col_1, col_2) values (7, 'stew pumpkin pie stew', 'pumpkin,sugar,flour and butter.', 'spread them all thinly')");
		exec_sql (db, "insert into recipe (cat, col_default, col_1, col_2) values (6, 'stew pumpkin pie stew', 'pumpkin,sugar,flour and butter.', 'mash and spread')");
	}
//	sql = g_strdup_printf ("select cat, count (*) from recipe where recipe match '%s' group by Cat", argv[1]);
//	exec_sql (db, sql);
//	g_free (sql);
	sql = g_strdup_printf ("select rowid, cat, col_default, col_1, col_2, offsets(recipe), rank(recipe) from recipe where recipe match '%s' and Cat<8 order by rank(recipe) desc", argv[1]);
	exec_sql (db, sql);
	g_free (sql);
	
		
	sqlite3_close(db);

	return EXIT_SUCCESS;
}

