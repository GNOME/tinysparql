/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia

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
#include <locale.h>

#include <sqlite3.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

static sqlite3_stmt *query_prepare (sqlite3      *db,
                                    const gchar  *sql);
static void          query_run     (sqlite3      *db,
                                    sqlite3_stmt *stmt);
static void          query_close   (sqlite3_stmt *stmt);

static gchar	    **queries;
static gchar	    **attach_dbs;

static GOptionEntry   entries[] = {
	{ "query", 'q', 0, G_OPTION_ARG_STRING_ARRAY, &queries,
	  N_("SQL query to use (can be used multiple times)"),
	  N_("SQL")
        },
	{ G_OPTION_REMAINING, 0, 0,
	  G_OPTION_ARG_FILENAME_ARRAY, &attach_dbs,
	  N_("Files to attach"),
	  N_("FILE")
	},
	{ NULL }
};

static sqlite3 *
connection_open (void)
{
        sqlite3 *db = NULL;
        int i;

        /* Attach remaining dbs */
        for (i = 0; attach_dbs[i] != NULL; i++) {
                GFile *file;
                gchar *path;

                file = g_file_new_for_commandline_arg (attach_dbs[i]);
		path = g_file_get_path (file);

                if (file) {
                        g_object_unref (file);
                }               

                if (i == 0) {
                        int result;

                        g_print ("Opening DB:'%s'\n", path);
                        
                        result = sqlite3_open (attach_dbs[0], &db);
                        if (result) {
                                const gchar *str = sqlite3_errmsg (db);
                                
                                g_printerr ("Couldn't open database:'%s', %s\n", 
                                            path, str ? str : "no error given");

                                sqlite3_close (db);
                                g_free (path);

                                break;
                        }
                } else {
                        gchar *sql;
                        sqlite3_stmt *stmt;
                        
                        sql = g_strdup_printf ("attach \"%s\" as \"%d\";", 
                                               path, i);
                        stmt = query_prepare (db, sql);
                        g_free (sql);
                        
                        if (stmt) {
                                query_run (db, stmt);
                                query_close (stmt);
                        }
                }
                
                g_free (path);
        }

        return db;
}

static void
connection_close (sqlite3 *db)
{
        g_print ("Closing DB\n");
        sqlite3_close (db);   
}

static sqlite3_stmt *
query_prepare (sqlite3     *db, 
               const gchar *sql)
{
        sqlite3_stmt *stmt;
        int result;

        g_print ("Preparing query: '%s'\n", sql);

        result = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
        if (result != SQLITE_OK) {
                const gchar *str = sqlite3_errmsg (db);

                g_printerr ("Couldn't prepare query, error:%d->'%s'\n", 
                            result, str ? str : "no error given");

                return NULL;
        }       

        return stmt;
}

static void
query_run (sqlite3      *db,
           sqlite3_stmt *stmt) 
{
        int result;
        int row = 0;

        g_print ("Running query: %p\n", stmt);

        while ((result = sqlite3_step (stmt)) != SQLITE_DONE) {
                switch (result) {
                case SQLITE_BUSY:
                        g_print ("Database is busy, waiting 1 second...\n");
                        g_usleep (G_USEC_PER_SEC);
                        break;

                case SQLITE_ERROR:
                        g_printerr ("Database error, %s\n", 
                                    sqlite3_errmsg (db) ? sqlite3_errmsg (db) : "no error given");
                        break;

                case SQLITE_ROW: {
                        int i;

                        g_print ("[ROW %5.5d]\n", ++row);

                        for (i = 0; i < sqlite3_column_count (stmt); i++) {
                                int t;

                                t = sqlite3_column_type (stmt, i);

                                switch (t) {
                                case SQLITE_TEXT:
                                        g_print ("  [ text] %s = %s", 
                                                 sqlite3_column_name (stmt, i),
                                                 sqlite3_column_text (stmt, i));
                                        break;

                                case SQLITE_INTEGER:
                                        g_print ("  [  int] %s = %d", 
                                                 sqlite3_column_name (stmt, i),
                                                 sqlite3_column_int (stmt, i));
                                        break;

                                case SQLITE_FLOAT:
                                        g_print ("  [float] %s = %f", 
                                                 sqlite3_column_name (stmt, i),
                                                 sqlite3_column_double (stmt, i));
                                        break;

                                case SQLITE_BLOB:
                                        g_print ("  [ blob] %s = ?", 
                                                 sqlite3_column_name (stmt, i));
                                        break;

                                case SQLITE_NULL:
                                        g_print ("  [ null] %s = empty", 
                                                 sqlite3_column_name (stmt, i));
                                        break;

                                default:
                                        g_print ("  [?????] %s = (unknown:%d)", 
                                                 sqlite3_column_name (stmt, i),
                                                 t);
                                        break;
                                }

                                g_print ("\n");
                        }

                        break;
                }
                }
        }

        g_print ("Done\n");
}

static void 
query_close (sqlite3_stmt *stmt)
{
        g_print ("Closing query\n");

        sqlite3_reset (stmt);
        sqlite3_finalize (stmt);
}

int 
main (int argc, char *argv[]) 
{
	GOptionContext *context;
        gchar *summary;
	gchar **query;
	gint failures;
        sqlite3 *db;
        sqlite3_stmt *stmt;

	setlocale (LC_ALL, "");

        g_type_init ();

	context = g_option_context_new (_ ("- Test SQLite databases with queries"));
	summary = g_strconcat (_("You can use this utility to attach DBs and "
                                 "run queries using the SQLite API"),
                               "\n"
                               "\n"
                               "Example 1. Get options:\n"
                               "\n"
                               "  -q \"SELECT * FROM Options;\" ~/.local/share/tracker/data/common.db\n",
                               "\n"
                               "Example 2. Get Services table count:\n"
                               "\n"
                               "  -q \"SELECT COUNT(0) FROM Services;\" ~/.cache/tracker/file-meta.db\n",
                               "\n"
                               "Example 3. Get statistics: (you will need to common.db & file-meta.db)\n"
                               "\n"
                               "  SELECT T.TypeName, COUNT(1)\n"
                               "  FROM Services S, ServiceTypes T\n"
                               "  WHERE\n"
                               "    S.AuxilaryID IN (SELECT VolumeID FROM Volumes WHERE Enabled = 1)\n"
                               "  AND\n"
                               "    S.Enabled = 1 AND T.TypeID=S.ServiceTypeID\n"
                               "  GROUP BY ServiceTypeID\n"
                               "  ORDER BY T.TypeName;",
                               NULL);

	g_option_context_set_summary (context, summary);
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_free (summary);

	if (!queries || g_strv_length (queries) < 1) {
		gchar *help;

		g_printerr ("%s\n\n",
			    _ ("No query was provided"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	if (!attach_dbs || !*attach_dbs) {
		gchar *help;

		g_printerr ("%s\n\n",
			    _ ("No databases were provided"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

        /* Start DB stuff */
        db = connection_open ();

        if (!db) {
                return EXIT_FAILURE;
        }

	for (query = queries, failures = 0; *query; query++) {
		stmt = query_prepare (db, *query);

		if (!stmt) {
			failures++;
			continue;
		}

		query_run (db, stmt);
		query_close (stmt);
	}
      
        connection_close (db);
       
	return failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
