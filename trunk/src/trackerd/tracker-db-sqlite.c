/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <regex.h>
#include <zlib.h>

#include "tracker-db-sqlite.h"
#include "tracker-indexer.h"
#include "tracker-cache.h"
#include "tracker-metadata.h"
#include "tracker-utils.h"

#include "config.h"

#ifdef OS_WIN32
#include "mingw-compat.h"
#endif

#define MAX_TEXT_BUFFER 65567
#define MAX_COMPRESS_BUFFER 65565
#define DEFAULT_PAGE_CACHE 64
#define MIN_PAGE_CACHE 16

extern Tracker *tracker;

static GHashTable *prepared_queries;
//static GMutex *sequence_mutex;

gboolean use_nfs_safe_locking = FALSE;

typedef struct {
	guint32		service_id;
	int		service_type_id;
	DBConnection 	*db_con;
} ServiceTypeInfo;





/* sqlite utf-8 user defined collation sequence */

static int 
sqlite3_utf8_collation (void *NotUsed,  int len1, const void *str1,  int len2, const void *str2)
{
	char *word1, *word2;
	int result;

	/* collate words */

	word1 = g_utf8_collate_key_for_filename (str1, len1);
	word2 = g_utf8_collate_key_for_filename (str2, len2);
	
	result = strcmp (word1, word2);
	
	g_free (word1);
	g_free (word2);

	return result;
}
 	



/* sqlite user defined functions for use in sql */

/* converts date/time in UTC format to ISO 8160 standardised format for display */
static void
sqlite3_date_to_str (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	const char *output;

	switch (sqlite3_value_type (argv[0])) {

		case SQLITE_NULL: 
			sqlite3_result_null (context);
			break;
		

		default:
			output = tracker_date_to_str (sqlite3_value_double (argv[0]));
			sqlite3_result_text (context, output, strlen (output), g_free);
		
	}
}


/* implements regexp functionality */
static void
sqlite3_regexp (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	int	ret;
	regex_t	regex;

	if (argc != 2) {
		sqlite3_result_error (context, "Invalid argument count", -1);
		return;
	}

	ret = regcomp (&regex, (char *) sqlite3_value_text(argv[0]), REG_EXTENDED | REG_NOSUB);

	if (ret != 0) {
		sqlite3_result_error (context, "error compiling regular expression", -1);
		return;
	}

	ret = regexec (&regex, (char *) sqlite3_value_text(argv[1]), 0, NULL, 0);
	regfree (&regex);

	sqlite3_result_int (context, (ret == REG_NOMATCH) ? 0 : 1);
}


/* unzips data */
static void
sqlite3_uncompress (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	switch (sqlite3_value_type (argv[0])) {

		case SQLITE_NULL: {
			sqlite3_result_null (context);
			break;
		}

		default:{
			int  len1, len2;
			char *output;

			len1 = sqlite3_value_bytes (argv[0]);

			output = tracker_uncompress (sqlite3_value_blob (argv[0]), len1, &len2);

			if (output) {
				sqlite3_result_text (context, output, len2, g_free);
			} else {
				tracker_error ("ERROR: decompression failed");
				sqlite3_result_text (context, sqlite3_value_blob (argv[0]), len1, NULL);
			}
		}
	}
}
 

static void
sqlite3_get_service_name (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	switch (sqlite3_value_type (argv[0])) {

		case SQLITE_NULL: {
			sqlite3_result_null (context);
			break;
		}

		default:{
			char *output;

			output = tracker_get_service_by_id (sqlite3_value_int (argv[0]));
			sqlite3_result_text (context, output, strlen (output), NULL);

		}
	}
}


static void
sqlite3_get_service_type (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	switch (sqlite3_value_type (argv[0])) {

		case SQLITE_NULL: {
			sqlite3_result_null (context);
			break;
		}

		default:{
			int output;

			output = tracker_get_id_for_service ((char *) sqlite3_value_text (argv[0]));
			sqlite3_result_int (context, output);
		}
	}
}


static void
sqlite3_get_max_service_type (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	switch (sqlite3_value_type (argv[0])) {

		case SQLITE_NULL: {
			sqlite3_result_null (context);
			break;
		}

		default:{
			int output;

			output = tracker_get_id_for_service ((char *) sqlite3_value_text (argv[0]));

			if (output == 0) {
				output = 8;
			}

			sqlite3_result_int (context, output);
		}
	}
}


static void
load_sql_file (DBConnection *db_con, const char *sql_file)
{
	char *filename, *query;
	
	filename = g_build_filename (TRACKER_DATADIR, "/tracker/", sql_file, NULL);

	if (!g_file_get_contents (filename, &query, NULL, NULL)) {
		tracker_error ("ERROR: Tracker cannot read required file %s - Please reinstall tracker or check read permissions on the file if it exists", sql_file);
		g_assert (FALSE);
	} else {
		char **queries, **queries_p ;

		queries = g_strsplit_set (query, ";", -1);

		for (queries_p = queries; *queries_p; queries_p++) {
			tracker_db_exec_no_reply (db_con, *queries_p);
		}
		g_strfreev (queries);
		g_free (query);
		tracker_log ("loaded sql file %s", sql_file);
	}

	g_free (filename);
}


static void
load_sql_trigger (DBConnection *db_con, const char *sql_file)
{
	char *filename, *query;
	
	filename = g_build_filename (TRACKER_DATADIR, "/tracker/", sql_file, NULL);

	if (!g_file_get_contents (filename, &query, NULL, NULL)) {
		tracker_error ("ERROR: Tracker cannot read required file %s - Please reinstall tracker or check read permissions on the file if it exists", sql_file);
		g_assert (FALSE);
	} else {
		char **queries, **queries_p ;

		queries = g_strsplit_set (query, "!", -1);

		for (queries_p = queries; *queries_p; queries_p++) {
			tracker_db_exec_no_reply (db_con, *queries_p);
		}
		g_strfreev (queries);
		g_free (query);
		tracker_log ("loaded sql file %s", sql_file);
	}

	g_free (filename);
}



FieldDef *
tracker_db_get_field_def (DBConnection *db_con, const char *field_name)
{
	FieldDef *def;
	char *name;

	name = g_utf8_strdown (field_name, -1);
	def = g_hash_table_lookup (tracker->metadata_table, name);
	g_free (name);

	return def;

}


void
tracker_db_free_field_def (FieldDef *def)
{
}


char **
tracker_db_get_row (char ***result, int num)
{
	if (!result) {
		return NULL;
	}

	if (result[num]) {
		return result[num];
	}

	return NULL;
}


void
tracker_db_log_result (char ***result)
{
	char ***rows;

	if (!result) {
		tracker_log ("no records");
		return;
	}

	for (rows = result; *rows; rows++) {
		char **row;
		char *str;

		str = NULL;

		for (row = *rows; *row; row++) {
			char *value;

                        value = tracker_string_replace (*row, "%", "%%");

			if (!value) {
				value = "NULL";
			}

			if (str) {
				char *tmp;

				tmp = g_strdup (str);
				g_free (str);
				str = g_strconcat (tmp, ", ", value, NULL);
				g_free (tmp);
			} else {
				str = g_strconcat (value, NULL);
			}

			g_free (value);
		}

		if (str) {
			tracker_log (str);
			g_free (str);
		}
	}
}


void
tracker_db_free_result (char ***result)
{
	char ***rows;

	if (!result) {
		return;
	}

	for (rows = result; *rows; rows++) {
		g_strfreev (*rows);
	}

	g_free (result);
}


int
tracker_get_row_count (char ***result)
{
	char ***rows;
	int  i;

	if (!result) {
		return 0;
	}

	i = 0;

	for (rows = result; *rows; rows++) {
		i++;
	}

	return i;
}


int
tracker_get_field_count (char ***result)
{
	char **row, **p;
	int  i;

	if (!result) {
		return 0;
	}

	row = tracker_db_get_row (result, 0);

	if (!row) {
		return 0;
	}

	i = 0;

	for (p = row; *p; p++) {
		i++;
	}

	return i;
}

static inline void
lock_connection (DBConnection *db_con) 
{
}


static inline void
unlock_connection (DBConnection *db_con) 
{
}


gboolean
tracker_db_initialize (const char *datadir)
{
	FILE	 *file;
	char	 *sql_file;
	GTimeVal *tv;
	int i = 0;


	//g_assert (sqlite3_threadsafe() != 0);

	tracker_log ("Using Sqlite version %s", sqlite3_version);

	//sequence_mutex = g_mutex_new ();

	/* load prepared queries */
	prepared_queries = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	tracker_log ("Loading prepared queries...");

	sql_file = g_strdup (TRACKER_DATADIR "/tracker/sqlite-stored-procs.sql");

	if (!g_file_test (sql_file, G_FILE_TEST_EXISTS)) {
		tracker_error ("ERROR: Tracker cannot read required file %s - Please reinstall tracker or check read permissions on the file if it exists", sql_file);
		g_assert (FALSE);
	}


	file = g_fopen (sql_file, "r");
	

	tv = tracker_timer_start ();

	while (!feof (file)) {
		char buffer[8192];
		char *sep;

		i++;

		if (!fgets (buffer, 8192, file)) {
                  /* An "if" to avoid warnings about value returned by fgets()
                     not handled...
                     We do not look the returned value since we will have NULL
                     with a file which terminates with '\n' on a empty line...
                  */
                }

		if (strlen (buffer) < 5) {
			continue;
		}

		sep = strchr (buffer, ' ');

		if (sep) {
			char *query, *name;

			*sep = '\0';

			query = g_strdup (sep + 1);
			name = g_strdup (buffer);

			//tracker_log ("installing query %s with sql %s", name, query);
			g_hash_table_insert (prepared_queries, name, query);
		} else {
			continue;
		}


	}

	fclose (file);

	g_free (sql_file);

	tracker_timer_end (tv, "File loaded in ");

	return TRUE;
}


void
tracker_db_thread_init (void)
{
//	sqlite3_enable_shared_cache (1);
}


void
tracker_db_thread_end (void)
{
	sqlite3_thread_cleanup ();
}


void
tracker_db_finalize (void)
{
}


static void
finalize_statement (gpointer key,
		    gpointer value,
		    gpointer user_data)
{
	sqlite3_stmt *stmt;

	stmt = value;

	if (key && stmt) {
		if (sqlite3_finalize (stmt)!= SQLITE_OK) {
			DBConnection *db_con;

			db_con = user_data;

			tracker_error ("ERROR: statement could not be finalized for %s with error %s", (char *) key, sqlite3_errmsg (db_con->db));
		}
	}
}



static inline void
close_db (DBConnection *db_con)
{
	if (db_con->statements) {
		g_hash_table_foreach (db_con->statements, finalize_statement, db_con);
	}

	g_hash_table_destroy (db_con->statements);
	db_con->statements = NULL;

	if (sqlite3_close (db_con->db) != SQLITE_OK) {
		tracker_error ("ERROR: database close operation failed for thread %s due to %s", db_con->thread, sqlite3_errmsg (db_con->db));
	} else {
		tracker_debug ("Database closed for thread %s", db_con->thread);
	}


}


void
tracker_db_close (DBConnection *db_con)
{
	if (!db_con->thread) {
		db_con->thread = "main";
	}

	close_db (db_con);
}





/*
static void
test_data (gpointer key,
	   gpointer value,
	   gpointer user_data)
{
	DBConnection *db_con;
	sqlite3_stmt *stmt;
	int	     rc;

	db_con = user_data;

	rc = sqlite3_prepare (db_con->db, query, -1, &stmt, 0);

	if (rc == SQLITE_OK && stmt != NULL) {
		char *procedure, *query;

		procedure = (char *) key;
		query = (char *) value;

		//tracker_log ("successfully prepared query %s", procedure);
		//g_hash_table_insert (db_con->statements, g_strdup (procedure), stmt);
	} else {
		tracker_error ("ERROR: failed to prepare query %s with sql %s due to %s", procedure, query, sqlite3_errmsg (db_con->db));
		return;
	}
}
*/


static sqlite3 *
open_user_db (const char *name, gboolean *create_table)
{
	char	     	*dbname;
	sqlite3 	*db;

	*create_table = FALSE;
	
	dbname = g_build_filename (tracker->user_data_dir, name, NULL);

	if (!g_file_test (dbname, G_FILE_TEST_IS_REGULAR)) {
		tracker_log ("database file %s is not present - will create", dbname);
		*create_table = TRUE;
	} 


	if (sqlite3_open (dbname, &db) != SQLITE_OK) {
		tracker_error ("FATAL ERROR: can't open database at %s: %s", dbname, sqlite3_errmsg (db));
		exit (1);
	}

	sqlite3_extended_result_codes (db, 0);

	g_free (dbname);

	sqlite3_busy_timeout (db, 10000);

	return db;

}



static sqlite3 *
open_db (const char *name, gboolean *create_table)
{
	char	     	*dbname;
	sqlite3 	*db;

	*create_table = FALSE;
	
	dbname = g_build_filename (tracker->data_dir, name, NULL);

	if (!g_file_test (dbname, G_FILE_TEST_IS_REGULAR)) {
		tracker_log ("database file %s is not present - will create", dbname);
		*create_table = TRUE;
	} 


	if (sqlite3_open (dbname, &db) != SQLITE_OK) {
		tracker_error ("FATAL ERROR: can't open database at %s: %s", dbname, sqlite3_errmsg (db));
		exit (1);
	}

	sqlite3_extended_result_codes (db, 0);

	g_free (dbname);

	sqlite3_busy_timeout (db, 10000);

	return db;

}


static void
set_params (DBConnection *db_con, int cache_size, gboolean add_functions)
{
	db_con->statements = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	tracker_db_set_default_pragmas (db_con);

	tracker_db_exec_no_reply (db_con, "PRAGMA page_size = 4096");

	char *prag;

	if (tracker->use_extra_memory) {
		prag = g_strdup_printf ("PRAGMA cache_size = %d", cache_size);
	} else {
		prag = g_strdup_printf ("PRAGMA cache_size = %d", cache_size/2);
	}

	tracker_db_exec_no_reply (db_con, prag);

	g_free (prag);

	if (add_functions) {
		/* create user defined utf-8 collation sequence */
		if (SQLITE_OK != sqlite3_create_collation (db_con->db, "UTF8", SQLITE_UTF8, 0, &sqlite3_utf8_collation)) {
			tracker_error ("ERROR: collation sequence failed due to %s", sqlite3_errmsg (db_con->db));
		}
	

		/* create user defined functions that can be used in sql */
		if (SQLITE_OK != sqlite3_create_function (db_con->db, "FormatDate", 1, SQLITE_ANY, NULL, &sqlite3_date_to_str, NULL, NULL)) {
			tracker_error ("ERROR: function FormatDate failed due to %s", sqlite3_errmsg (db_con->db));
		}
		if (SQLITE_OK != sqlite3_create_function (db_con->db, "GetServiceName", 1, SQLITE_ANY, NULL, &sqlite3_get_service_name, NULL, NULL)) {
			tracker_error ("ERROR: function GetServiceName failed due to %s", sqlite3_errmsg (db_con->db));
		}
		if (SQLITE_OK != sqlite3_create_function (db_con->db, "GetServiceTypeID", 1, SQLITE_ANY, NULL, &sqlite3_get_service_type, NULL, NULL)) {
			tracker_error ("ERROR: function GetServiceTypeID failed due to %s", sqlite3_errmsg (db_con->db));
		}
		if (SQLITE_OK != sqlite3_create_function (db_con->db, "GetMaxServiceTypeID", 1, SQLITE_ANY, NULL, &sqlite3_get_max_service_type, NULL, NULL)) {
			tracker_error ("ERROR: function GetMaxServiceTypeID failed due to %s", sqlite3_errmsg (db_con->db));
		}
		if (SQLITE_OK != sqlite3_create_function (db_con->db, "REGEXP", 2, SQLITE_ANY, NULL, &sqlite3_regexp, NULL, NULL)) {
			tracker_error ("ERROR: function REGEXP failed due to %s", sqlite3_errmsg (db_con->db));
		}
	}
}



static void
open_common_db (DBConnection *db_con)
{
	gboolean create;

	db_con->db = open_user_db ("common.db", &create);

	set_params (db_con, 16, FALSE);

}


DBConnection *
tracker_db_connect_common (void)
{

	DBConnection *db_con;

	db_con = g_new0 (DBConnection, 1);

	open_common_db (db_con);

	db_con->db_type = DB_COMMON;
	
	db_con->cache = NULL;
	db_con->emails = NULL;
	db_con->others = NULL;
	db_con->blob = NULL;
	db_con->common = db_con;

	db_con->thread = NULL;

	db_con->in_transaction = FALSE;

	return db_con;
}

void
tracker_db_attach_db (DBConnection *db_con, const char *name)
{

	char *sql = NULL;

	if (strcmp (name, "common") == 0) {
	
		char *path = g_build_filename (tracker->user_data_dir, "common.db", NULL);
		sql = g_strdup_printf ("ATTACH '%s' as %s", path, name);
		g_free (path);

		tracker_db_exec_no_reply (db_con, sql);

	} else if (strcmp (name, "cache") == 0) {
	
		char *path = g_build_filename (tracker->sys_tmp_root_dir, "cache.db", NULL);
		sql = g_strdup_printf ("ATTACH '%s' as %s", path, name);
		g_free (path);

		tracker_db_exec_no_reply (db_con, sql);

	}

	g_free (sql);
}

static inline void
free_db_con (DBConnection *db_con)
{
	g_free (db_con);
	db_con = NULL;
}

/* convenience function for process files thread */
DBConnection *
tracker_db_connect_all (gboolean indexer_process)
{

	DBConnection *db_con;
	DBConnection *blob_db_con = NULL;
	DBConnection *word_index_db_con = NULL;

	DBConnection *common_db_con = NULL;

	DBConnection *emails_blob_db_con = NULL;
	DBConnection *emails_db_con= NULL;
	DBConnection *email_word_index_db_con= NULL;

	if (!indexer_process) {
		db_con = tracker_db_connect ();
		emails_db_con = tracker_db_connect_emails ();
	} else {
		db_con = tracker_db_connect_file_meta ();
		emails_db_con = tracker_db_connect_email_meta ();
	}

	blob_db_con = tracker_db_connect_file_content ();
	emails_blob_db_con = tracker_db_connect_email_content ();
	common_db_con  = tracker_db_connect_common ();

	word_index_db_con = tracker->file_index;
	email_word_index_db_con = tracker->email_index;

	db_con->cache = tracker_db_connect_cache ();

	db_con->blob = blob_db_con;
	db_con->data = db_con;
	db_con->emails = emails_db_con;
	db_con->common = common_db_con;
	db_con->word_index = word_index_db_con;
	db_con->index = db_con;

	emails_db_con->common = common_db_con;
	emails_db_con->blob = emails_blob_db_con;
	emails_db_con->data = db_con;
	emails_db_con->word_index = email_word_index_db_con;
	emails_db_con->index = emails_db_con;
	emails_db_con->cache = db_con->cache;

	return db_con;

}

void
tracker_db_close_all (DBConnection *db_con)
{

	DBConnection *email_db_con = db_con->emails;
	DBConnection *email_blob_db_con = email_db_con->blob;

	DBConnection *common_db_con = db_con->common;
	DBConnection *cache_db_con = db_con->cache;

	DBConnection *file_blob_db_con = db_con->blob;


	/* close emails */
	if (email_blob_db_con) {
		tracker_db_close (email_blob_db_con);
		free_db_con (email_blob_db_con);
	}
		
	if (email_db_con) {
		tracker_db_close (email_db_con);
		g_free (email_db_con);
	}


	/* close files */
	if (file_blob_db_con) {
		tracker_db_close (file_blob_db_con);
		free_db_con (file_blob_db_con);
	}

	tracker_db_close (db_con);
	g_free (db_con);


	/* close others */
	if (common_db_con) {
		tracker_db_close (common_db_con);
		free_db_con (common_db_con);
	}

	
	if (cache_db_con) {
		tracker_db_close (cache_db_con);
		free_db_con (cache_db_con);
	}


}

void
tracker_db_start_index_transaction (DBConnection *db_con)
{
	DBConnection *tmp;
	DBConnection *email_db_con = db_con->emails;


	tmp = db_con->common;
	if (!tmp->in_transaction) tracker_db_start_transaction (tmp);
	

	/* files */
	if (!db_con->in_transaction) tracker_db_start_transaction (db_con);

	tmp = db_con->blob;
	if (!tmp->in_transaction) tracker_db_start_transaction (tmp);

	/* emails */
	if (!email_db_con->in_transaction) tracker_db_start_transaction (email_db_con);

	tmp = email_db_con->blob;
	if (!tmp->in_transaction) tracker_db_start_transaction (tmp);
}



void
tracker_db_end_index_transaction (DBConnection *db_con)
{
	DBConnection *tmp;
	DBConnection *email_db_con = db_con->emails;

	tmp = db_con->common;
	if (tmp->in_transaction) {
		tracker_db_end_transaction (tmp);
	}

	/* files */
	if (db_con->in_transaction) {
		tracker_db_end_transaction (db_con);
	}

	tmp = db_con->blob;
	if (tmp->in_transaction) {
		tracker_db_end_transaction (tmp);
	}

	/* emails */
	if (email_db_con->in_transaction) {
		tracker_db_end_transaction (email_db_con);
	}

	tmp = email_db_con->blob;
	if (tmp->in_transaction) {
		tracker_db_end_transaction (tmp);
	}

}


void
tracker_db_set_default_pragmas (DBConnection *db_con)
{
	tracker_db_exec_no_reply (db_con, "PRAGMA synchronous = OFF;");

	tracker_db_exec_no_reply (db_con, "PRAGMA count_changes = 0;");

	tracker_db_exec_no_reply (db_con, "PRAGMA temp_store = FILE;");

	tracker_db_exec_no_reply (db_con, "PRAGMA encoding = \"UTF-8\"");

	tracker_db_exec_no_reply (db_con, "PRAGMA auto_vacuum = 0;");

}


DBConnection *
tracker_db_connect (void)
{
	char	     *dbname;
	DBConnection *db_con;

	gboolean create_table = FALSE;

	dbname = g_build_filename (tracker->data_dir, "file-meta.db", NULL);

	if (!g_file_test (dbname, G_FILE_TEST_IS_REGULAR)) {
		tracker_log ("database file %s is not present - will create", dbname);
		create_table = TRUE;
	} 

	db_con = g_new0 (DBConnection, 1);

	if (sqlite3_open (dbname, &db_con->db) != SQLITE_OK) {
		tracker_error ("FATAL ERROR: can't open database at %s: %s", dbname, sqlite3_errmsg (db_con->db));
		exit (1);
	}

	g_free (dbname);

	db_con->db_type = DB_DATA;
	db_con->db_category = DB_CATEGORY_FILES;

	sqlite3_busy_timeout (db_con->db, 10000);

	db_con->data = db_con;

	db_con->statements = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	tracker_db_set_default_pragmas (db_con);
	
	if (tracker->use_extra_memory) {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 32");
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 16");
	}

	

	/* create user defined utf-8 collation sequence */
	if (SQLITE_OK != sqlite3_create_collation (db_con->db, "UTF8", SQLITE_UTF8, 0, &sqlite3_utf8_collation)) {
		tracker_error ("ERROR: collation sequence failed due to %s", sqlite3_errmsg (db_con->db));
	}
	

	/* create user defined functions that can be used in sql */
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "FormatDate", 1, SQLITE_ANY, NULL, &sqlite3_date_to_str, NULL, NULL)) {
		tracker_error ("ERROR: function FormatDate failed due to %s", sqlite3_errmsg (db_con->db));
	}
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "GetServiceName", 1, SQLITE_ANY, NULL, &sqlite3_get_service_name, NULL, NULL)) {
		tracker_error ("ERROR: function GetServiceName failed due to %s", sqlite3_errmsg (db_con->db));
	}
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "GetServiceTypeID", 1, SQLITE_ANY, NULL, &sqlite3_get_service_type, NULL, NULL)) {
		tracker_error ("ERROR: function GetServiceTypeID failed due to %s", sqlite3_errmsg (db_con->db));
	}
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "GetMaxServiceTypeID", 1, SQLITE_ANY, NULL, &sqlite3_get_max_service_type, NULL, NULL)) {
		tracker_error ("ERROR: function GetMaxServiceTypeID failed due to %s", sqlite3_errmsg (db_con->db));
	}
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "REGEXP", 2, SQLITE_ANY, NULL, &sqlite3_regexp, NULL, NULL)) {
		tracker_error ("ERROR: function REGEXP failed due to %s", sqlite3_errmsg (db_con->db));
	}

	if (create_table) {
		tracker_log ("Creating file database...");
		load_sql_file (db_con, "sqlite-service.sql");
		load_sql_trigger (db_con, "sqlite-service-triggers.sql");

		load_sql_file (db_con, "sqlite-metadata.sql");
	
		tracker_db_load_service_file (db_con, "default.metadata", FALSE);
		tracker_db_load_service_file (db_con, "file.metadata", FALSE);
		tracker_db_load_service_file (db_con, "audio.metadata", FALSE);
		tracker_db_load_service_file (db_con, "application.metadata", FALSE);
		tracker_db_load_service_file (db_con, "document.metadata", FALSE);
		tracker_db_load_service_file (db_con, "email.metadata", FALSE);
		tracker_db_load_service_file (db_con, "image.metadata", FALSE);	
		tracker_db_load_service_file (db_con, "video.metadata", FALSE);	
	
		tracker_db_exec_no_reply (db_con, "ANALYZE");
	}


	tracker_db_attach_db (db_con, "common");


	db_con->thread = NULL;

	return db_con;
}


void
tracker_db_fsync (DBConnection *db_con, gboolean enable)
{
	if (!enable) {
		tracker_db_exec_no_reply (db_con, "PRAGMA synchronous = OFF;");
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA synchronous = NORMAL;");
	}

}

static inline void
open_file_db (DBConnection *db_con)
{
	gboolean create;

	db_con->db = open_db ("file-meta.db", &create);	

	set_params (db_con, 128, TRUE);
}

DBConnection *
tracker_db_connect_file_meta (void)
{
	DBConnection *db_con;

	db_con = g_new0 (DBConnection, 1);

	db_con->db_type = DB_INDEX;
	db_con->db_category = DB_CATEGORY_FILES;
	db_con->index = db_con;

	open_file_db (db_con);

	db_con->thread = NULL;

	return db_con;
}


static inline void
open_email_db (DBConnection *db_con)
{
	gboolean create;

	db_con->db = open_db ("email-meta.db", &create);	

	set_params (db_con, 128, TRUE);
}

DBConnection *
tracker_db_connect_email_meta (void)
{
	DBConnection *db_con;

	db_con = g_new0 (DBConnection, 1);

	db_con->db_type = DB_INDEX;
	db_con->db_category = DB_CATEGORY_EMAILS;

	db_con->index = db_con;
	db_con->emails = db_con;

	open_email_db (db_con);

	db_con->thread = NULL;

	return db_con;
}


static inline void
open_file_content_db (DBConnection *db_con)
{
	gboolean create;

	db_con->db = open_db ("file-contents.db", &create);	

	set_params (db_con, 256, FALSE);

	if (create) {
		tracker_db_exec_no_reply (db_con, "CREATE TABLE ServiceContents (ServiceID Int not null, MetadataID Int not null, Content Text, primary key (ServiceID, MetadataID))");
		tracker_log ("creating file content table");
	}

	sqlite3_create_function (db_con->db, "uncompress", 1, SQLITE_ANY, NULL, &sqlite3_uncompress, NULL, NULL);

	

}

DBConnection *
tracker_db_connect_file_content (void)
{
	DBConnection *db_con;

	db_con = g_new0 (DBConnection, 1);

	db_con->db_type = DB_CONTENT;
	db_con->db_category = DB_CATEGORY_FILES;
	db_con->blob = db_con;

	open_file_content_db (db_con);

	db_con->thread = NULL;

	return db_con;
}


static inline void
open_email_content_db (DBConnection *db_con)
{
	gboolean create;

	db_con->db = open_db ("email-contents.db", &create);	

	set_params (db_con, 256, FALSE);

	if (create) {
		tracker_db_exec_no_reply (db_con, "CREATE TABLE ServiceContents (ServiceID Int not null, MetadataID Int not null, Content Text, primary key (ServiceID, MetadataID))");
		tracker_log ("creating email content table");
	}

	sqlite3_create_function (db_con->db, "uncompress", 1, SQLITE_ANY, NULL, &sqlite3_uncompress, NULL, NULL);

	

}

DBConnection *
tracker_db_connect_email_content (void)
{
	DBConnection *db_con;

	db_con = g_new0 (DBConnection, 1);

	db_con->db_type = DB_CONTENT;
	db_con->db_category = DB_CATEGORY_EMAILS;
	db_con->blob = db_con;

	open_email_content_db (db_con);

	db_con->thread = NULL;

	return db_con;
}


void
tracker_db_refresh_all (DBConnection *db_con)
{
	gboolean cache_trans = FALSE;
	DBConnection *cache = db_con->cache;
	DBConnection *emails = db_con->emails;

	if (cache && cache->in_transaction) {
		tracker_db_end_transaction (cache);
		cache_trans = TRUE;
	}

	/* close and reopen all databases */	
	close_db (db_con);	
	close_db (db_con->blob);

	close_db (emails->blob);
	close_db (emails->common);
	close_db (emails);

	open_file_db (db_con);
	open_file_content_db (db_con->blob);

	open_email_content_db (emails->blob);
	open_common_db (emails->common);
	open_email_db (emails);
		
	if (cache_trans) {
		tracker_db_start_transaction (cache);
	}


}

void
tracker_db_refresh_email (DBConnection *db_con)
{
	gboolean cache_trans = FALSE;
	DBConnection *cache = db_con->cache;

	if (cache && cache->in_transaction) {
		tracker_db_end_transaction (cache);
		cache_trans = TRUE;
	}

	/* close email DBs and reopen them */

	DBConnection *emails = db_con->emails;

	close_db (emails->blob);
	close_db (emails->common);
	close_db (emails);

	open_email_content_db (emails->blob);
	open_common_db (emails->common);
	open_email_db (emails);

	if (cache_trans) {
		tracker_db_start_transaction (cache);
	}

	
}

DBConnection *
tracker_db_connect_cache (void)
{
	gboolean     create_table;
	char	     *dbname;
	DBConnection *db_con;

	create_table = FALSE;

	if (!tracker || !tracker->sys_tmp_root_dir) {
		tracker_error ("FATAL ERROR: system TMP dir for cache set to %s", tracker->sys_tmp_root_dir);
		exit (1);
	}

	dbname = g_build_filename (tracker->sys_tmp_root_dir, "cache.db", NULL);

	if (!tracker_file_is_valid (dbname)) {
		create_table = TRUE;
	}

	db_con = g_new0 (DBConnection, 1);

	if (sqlite3_open (dbname, &db_con->db) != SQLITE_OK) {
		tracker_error ("FATAL ERROR: can't open database at %s: %s", dbname, sqlite3_errmsg (db_con->db));
		exit (1);
	}

	g_free (dbname);

	db_con->db_type = DB_CACHE;
	db_con->cache = db_con;

	sqlite3_busy_timeout (db_con->db, 10000);

	db_con->statements = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	tracker_db_set_default_pragmas (db_con);

	if (tracker->use_extra_memory) {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 128");
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 32");
	}


	if (create_table) {
		load_sql_file (db_con, "sqlite-cache.sql");
		tracker_db_exec_no_reply (db_con, "ANALYZE");
	}

	db_con->thread = NULL;

	return db_con;
}


DBConnection *
tracker_db_connect_emails (void)
{
	gboolean     create_table;
	char	     *dbname;
	DBConnection *db_con;
	
	create_table = FALSE;

	dbname = g_build_filename (tracker->data_dir, "email-meta.db", NULL);


	if (!g_file_test (dbname, G_FILE_TEST_IS_REGULAR)) {
		tracker_log ("database file %s is not present - will create", dbname);
		create_table = TRUE;
	} 


	db_con = g_new0 (DBConnection, 1);

	if (sqlite3_open (dbname, &db_con->db) != SQLITE_OK) {
		tracker_error ("FATAL ERROR: can't open database at %s: %s", dbname, sqlite3_errmsg (db_con->db));
		exit (1);
	}

	g_free (dbname);


	db_con->db_type = DB_EMAIL;
	db_con->db_category = DB_CATEGORY_EMAILS;

	db_con->emails = db_con;

	sqlite3_busy_timeout (db_con->db, 10000);

	db_con->statements = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	tracker_db_exec_no_reply (db_con, "PRAGMA page_size = 4096");

	tracker_db_set_default_pragmas (db_con);

	if (tracker->use_extra_memory) {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 8");
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 8");
	}


	/* create user defined utf-8 collation sequence */
	if (SQLITE_OK != sqlite3_create_collation (db_con->db, "UTF8", SQLITE_UTF8, 0, &sqlite3_utf8_collation)) {
		tracker_error ("ERROR: collation sequence failed due to %s", sqlite3_errmsg (db_con->db));
	}
	

	/* create user defined functions that can be used in sql */
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "FormatDate", 1, SQLITE_ANY, NULL, &sqlite3_date_to_str, NULL, NULL)) {
		tracker_error ("ERROR: function FormatDate failed due to %s", sqlite3_errmsg (db_con->db));
	}
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "GetServiceName", 1, SQLITE_ANY, NULL, &sqlite3_get_service_name, NULL, NULL)) {
		tracker_error ("ERROR: function GetServiceName failed due to %s", sqlite3_errmsg (db_con->db));
	}
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "GetServiceTypeID", 1, SQLITE_ANY, NULL, &sqlite3_get_service_type, NULL, NULL)) {
		tracker_error ("ERROR: function GetServiceTypeID failed due to %s", sqlite3_errmsg (db_con->db));
	}
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "GetMaxServiceTypeID", 1, SQLITE_ANY, NULL, &sqlite3_get_max_service_type, NULL, NULL)) {
		tracker_error ("ERROR: function GetMaxServiceTypeID failed due to %s", sqlite3_errmsg (db_con->db));
	}
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "REGEXP", 2, SQLITE_ANY, NULL, &sqlite3_regexp, NULL, NULL)) {
		tracker_error ("ERROR: function REGEXP failed due to %s", sqlite3_errmsg (db_con->db));
	}

	if (create_table) {
		tracker_log ("Creating email database...");
		load_sql_file (db_con, "sqlite-service.sql");
		load_sql_trigger (db_con, "sqlite-service-triggers.sql");
		load_sql_file (db_con, "sqlite-email.sql");

		tracker_db_exec_no_reply (db_con, "ANALYZE");
	}

	tracker_db_attach_db (db_con, "common");
	tracker_db_attach_db (db_con, "cache");

	db_con->thread = NULL;

	return db_con;
}


char *
tracker_db_get_alias (const char *service)
{
	int id = tracker_get_id_for_parent_service (service);
	char *parent = tracker_get_service_by_id (id);

	if (strcmp (parent, "Files") == 0) {
		g_free (parent);
		return g_strdup ("files");
	}

	if (strcmp (parent, "Emails") == 0) {
		g_free (parent);
		return g_strdup ("emails");
	}

	g_free (parent);
	return g_strdup ("misc");




}


/* get no of links to a file - used for safe NFS atomic file locking */
static int
get_nlinks (const char *name)
{
	struct stat st;

	if (g_stat (name, &st) == 0) {
		return st.st_nlink;
	} else {
		return -1;
	}
}


static int
get_mtime (const char *name)
{
	struct stat st;

	if (g_stat (name, &st) == 0) {
		return st.st_mtime;
	} else {
		return -1;
	}
}


/* serialises db access via a lock file for safe use on (lock broken) NFS mounts */
static gboolean
lock_db (void)
{
	int attempt;
	char *lock_file, *tmp, *tmp_file;

	if (!use_nfs_safe_locking) {
		return TRUE;
	}

	lock_file = g_build_filename (tracker->root_dir, "tracker.lock", NULL);
	tmp = g_build_filename (tracker->root_dir, g_get_host_name (), NULL);
	tmp_file = g_strdup_printf ("%s_%d.lock", tmp, (guint32) getpid ());
	g_free (tmp);

	for (attempt = 0; attempt < 10000; ++attempt) {
		int fd;

		/* delete existing lock file if older than 5 mins */
		if (g_file_test (lock_file, G_FILE_TEST_EXISTS) && ( time((time_t *) NULL) - get_mtime (lock_file)) > 300) {
			g_unlink (lock_file);
		}

		fd = g_open (lock_file, O_CREAT|O_EXCL, 0644);

		if (fd >= 0) {

			/* create host specific file and link to lock file */
                        if (link (lock_file, tmp_file) == -1) {
                                goto error;
                        }

			/* for atomic NFS-safe locks, stat links = 2 if file locked. If greater than 2 then we have a race condition */
			if (get_nlinks (lock_file) == 2) {
				close (fd);
				g_free (lock_file);
				g_free (tmp_file);

				return TRUE;
			} else {
				close (fd);
				g_usleep (g_random_int_range (1000, 100000));
			}
		}
	}

 error:
	tracker_error ("ERROR: lock failure");
	g_free (lock_file);
	g_free (tmp_file);

	return FALSE;
}


static void
unlock_db (void)
{
	char *lock_file, *tmp, *tmp_file;

	if (!use_nfs_safe_locking) {
		return;
	}

	lock_file = g_build_filename (tracker->root_dir, "tracker.lock", NULL);
	tmp = g_build_filename (tracker->root_dir,  g_get_host_name (), NULL);
	tmp_file = g_strdup_printf ("%s_%d.lock", tmp, (guint32) getpid ());
	g_free (tmp);

	unlink (tmp_file);
	unlink (lock_file);

	g_free (tmp_file);
	g_free (lock_file);
}


void
tracker_db_prepare_queries (DBConnection *db_con)
{
}


gboolean
tracker_db_exec_no_reply (DBConnection *db_con, const char *query)
{
	char *msg = NULL;	
	int rc,  busy_count = 0;

	lock_db();

	while ((rc = sqlite3_exec (db_con->db, query, NULL, NULL, &msg)) == SQLITE_BUSY) {

		unlock_db ();
		busy_count++;

		if (msg) {
			g_free (msg);
			msg = NULL;
		}

		if (busy_count > 1000000) {
			tracker_error ("WARNING: excessive busy count in query %s and thread %s", "save file contents", db_con->thread);
			busy_count = 0;
		}
			

		if (busy_count > 50) {
			g_usleep (g_random_int_range (1000, busy_count * 200));
		} else {
			g_usleep (100);
		}

		lock_db();
		
	}
	unlock_db ();

	if (rc != SQLITE_OK) {

		if (msg) {
			tracker_error ("WARNING: sql query %s failed because %s", query, msg);	
			g_free (msg);
		} else {
			tracker_error ("WARNING: sql query %s failed with code %d", query, rc);	
		}

		return FALSE;
	}

	return TRUE;

}



static char ***
exec_sql (DBConnection *db_con, const char *query, gboolean ignore_nulls)
{
	char **array, **result;
	int  cols, rows;
	char *msg;
	int  i, busy_count, totalrows, k;

	g_return_val_if_fail (query, NULL);

	array = NULL;
	msg = NULL;

	if (!lock_db ()) {
		return NULL;
	}

	busy_count = 0;

	lock_connection (db_con);

	i = sqlite3_get_table (db_con->db, query, &array, &rows, &cols, &msg);

	unlock_connection (db_con);

	while (i == SQLITE_BUSY) {

		
		if (array) {
			sqlite3_free_table (array);
			array = NULL;
		}

		if (msg) {
			g_free (msg);
			msg = NULL;
		}

		busy_count++;

		if (busy_count > 10000) {
			tracker_error ("ERROR: excessive busy count in query %s and thread %s", query, db_con->thread);
			busy_count =0;
		}

		if (busy_count > 50) {
			g_usleep (g_random_int_range (1000, busy_count * 200));
		} else {
			g_usleep (100);
		}

		lock_connection (db_con);

		i = sqlite3_get_table (db_con->db, query, &array, &rows, &cols, &msg);

		unlock_connection (db_con);
	}

	if (i != SQLITE_OK) {
		unlock_db ();
		tracker_error ("ERROR: query %s failed with error: %s", query, msg);
		g_free (msg);
		return NULL;
	}

	unlock_db ();

	if (msg) {
		g_free (msg);
	}

	if (!array || rows == 0 || cols == 0) {
		return NULL;
	}


	result = g_new (char *, rows+1);
	result[rows] = NULL;

	totalrows = (rows+1) * cols;

	//tracker_log ("total rows is %d", totalrows);
	//for (i=0;i<totalrows;i++) tracker_log (array[i]);

	for (i = cols, k = 0; i < totalrows; i += cols, k++) {
		char **row;
		int  j;

		row = g_new (char *, cols + 1);
		row[cols] = NULL;

		for (j = 0; j < cols; j++) {
			if (ignore_nulls && !array[i+j]) {
				row[j] = g_strdup ("");
			} else {
				row[j] = g_strdup (array[i+j]);
			}
			//tracker_log ("data for row %d, col %d is %s", k, j, row[j]);
		}

		result[k] = (gpointer) row;
	}

	sqlite3_free_table (array);

	return (char ***) result;
}

void
tracker_db_release_memory ()
{

}


char ***
tracker_exec_sql (DBConnection *db_con, const char *query)
{
	return exec_sql (db_con, query, FALSE);
}


char ***
tracker_exec_sql_ignore_nulls (DBConnection *db_con, const char *query)
{
	return exec_sql (db_con, query, TRUE);
}


char *
tracker_escape_string (const char *in)
{
	return sqlite3_mprintf ("%q", in);

/*
	GString *gs;

	if (!in) {
		return NULL;
	}

	gs = g_string_new ("");

	for(; *in; in++) {

		if (*in == '\'') {
			g_string_append (gs, "'\\''");
		} else {
			g_string_append_c (gs, *in);
		}
	}

	return g_string_free (gs, FALSE);
*/
}



static sqlite3_stmt *
get_prepared_query (DBConnection *db_con, const char *procedure)
{
	sqlite3_stmt *stmt;

	/* check if query is already prepared (i.e. is in table) */
	stmt = g_hash_table_lookup (db_con->statements, procedure);

	/* check if query is in list and prepare it if so */
	if (!stmt || sqlite3_expired (stmt) != 0) {
		char *query;

		query = g_hash_table_lookup (prepared_queries, procedure);

		if (!query) {
			tracker_error ("ERROR: prepared query %s not found", procedure);
			return NULL;
		} else {
			int rc;

			/* prepare the query */
			rc = sqlite3_prepare_v2 (db_con->db, query, -1, &stmt, NULL);

			if (rc == SQLITE_OK && stmt) {
				//tracker_log ("successfully prepared query %s", procedure);
				g_hash_table_insert (db_con->statements, g_strdup (procedure), stmt);
			} else {

				rc = sqlite3_step (stmt);
	
				if (rc == SQLITE_OK && stmt) {
					//tracker_log ("successfully prepared query %s", procedure);
	
					g_hash_table_insert (db_con->statements, g_strdup (procedure), stmt);
				} else {
					tracker_error ("ERROR: failed to prepare query %s with sql %s due to code %d and %s", procedure, query, sqlite3_errcode(db_con->db), sqlite3_errmsg (db_con->db));
					return NULL;
				}
			}
		}

	} else {
		sqlite3_reset (stmt);
	}

	return stmt;
}


char ***
tracker_exec_proc (DBConnection *db_con, const char *procedure, int param_count, ...)
{
	va_list      args;
	int 	     i, busy_count, cols, row;
	sqlite3_stmt *stmt;
	char 	     **res;
	GSList	     *result;
	int	     rc;
	const GSList *tmp;

	stmt = get_prepared_query (db_con, procedure);

	va_start (args, param_count);

	if (param_count != sqlite3_bind_parameter_count (stmt)) {
		tracker_error ("ERROR: incorrect no of parameters %d supplied to %s", param_count, procedure);
	}

	for (i = 0; i < param_count; i++) {
		char *str;

		str = va_arg (args, char *);

		if (!str) {
			tracker_debug ("WARNING: parameter %d is null when executing SP %s", i, procedure);
			if  (sqlite3_bind_null (stmt, i+1)) {
				tracker_error ("ERROR: null parameter %d could not be bound to %s", i, procedure);
			}
		} else {

			if (sqlite3_bind_text (stmt, i+1, str, strlen (str), SQLITE_TRANSIENT) != SQLITE_OK) {
				tracker_error ("ERROR: parameter %d could not be bound to %s", i, procedure);
			}
		}
	}

	va_end (args);

	cols = sqlite3_column_count (stmt);

	busy_count = 0;
	row = 0;

	result = NULL;
	
	lock_connection (db_con);
	while (TRUE) {

		if (!lock_db ()) {
			unlock_connection (db_con);	
			return NULL;
		}
		
		db_con->in_error = FALSE;
		
		rc = sqlite3_step (stmt);

		if (rc == SQLITE_ERROR) {
			sqlite3_reset (stmt);
			unlock_db ();
			db_con->in_error = TRUE;
			break;
		}
		

		if (rc == SQLITE_BUSY) {
			unlock_db ();
			unlock_connection (db_con);
			busy_count++;

			if (busy_count > 1000) {
				tracker_error ("ERROR: excessive busy count in query %s and thread %s", procedure, db_con->thread);
				exit (1);
			}

			if (busy_count > 50) {
				g_usleep (g_random_int_range (1000, busy_count * 200));
			} else {
				g_usleep (100);
			}

			lock_connection (db_con);
			continue;
		}

		if (rc == SQLITE_ROW) {
			char **new_row;

			new_row = g_new0 (char *, cols+1);
			new_row[cols] = NULL;

			unlock_db ();
			

			for (i = 0; i < cols; i++) {
				const char *st;

				st = (char *) sqlite3_column_text (stmt, i);

				if (st) {
					new_row[i] = g_strdup (st);
					//tracker_log ("%s : row %d, col %d is %s", procedure, row, i, st);
				} else {
					tracker_info ("WARNING: null detected in query return result for %s", procedure);
				}
			}

			if (new_row && new_row[0]) {
				result = g_slist_prepend (result, new_row);
				row++;
			}

			continue;
		}

		unlock_db ();
		break;
	}

	unlock_connection (db_con);

	if (rc != SQLITE_DONE) {
		tracker_error ("ERROR: execution of prepared query %s failed due to %s with return code %d", procedure, sqlite3_errmsg (db_con->db), rc);
		db_con->in_error = TRUE;

	}

	if (!result || (row == 0)) {
		return NULL;
	}

	result = g_slist_reverse (result);

	res = g_new0 (char *, row+1);
	res[row] = NULL;

	tmp = result;

	for (i = 0; i < row; i++) {
		if (tmp) {
			res[i] = tmp->data;
			tmp = tmp->next;
		} else {
			tracker_error ("WARNING: exec proc has a dud entry");
		}
	}

	g_slist_free (result);

	return (char ***) res;
}


gboolean
tracker_exec_proc_no_reply (DBConnection *db_con, const char *procedure, int param_count, ...)
{
	va_list      args;
	int 	     i, busy_count, cols, row;
	sqlite3_stmt *stmt;
	GSList	     *result;
	int	     rc;

	stmt = get_prepared_query (db_con, procedure);

	va_start (args, param_count);

	if (param_count != sqlite3_bind_parameter_count (stmt)) {
		tracker_error ("ERROR: incorrect no of parameters %d supplied to %s", param_count, procedure);
	}

	for (i = 0; i < param_count; i++) {
		char *str;

		str = va_arg (args, char *);

		if (!str) {
			tracker_debug ("WARNING: parameter %d is null when executing SP %s", i, procedure);
			if  (sqlite3_bind_null (stmt, i+1)) {
				tracker_error ("ERROR: null parameter %d could not be bound to %s", i, procedure);
			}
		} else {

			if (sqlite3_bind_text (stmt, i+1, str, strlen (str), SQLITE_TRANSIENT) != SQLITE_OK) {
				tracker_error ("ERROR: parameter %d could not be bound to %s", i, procedure);
			}
		}
	}

	va_end (args);

	cols = sqlite3_column_count (stmt);

	busy_count = 0;
	row = 0;

	result = NULL;
	
	lock_connection (db_con);
	while (TRUE) {

		if (!lock_db ()) {
			unlock_connection (db_con);	
			return FALSE;
		}
		
		db_con->in_error = FALSE;
		
		rc = sqlite3_step (stmt);

		if (rc == SQLITE_ERROR) {
			sqlite3_reset (stmt);
			unlock_db ();
			db_con->in_error = TRUE;
			break;
		}
		

		if (rc == SQLITE_BUSY) {
			unlock_db ();
			unlock_connection (db_con);
			busy_count++;

			if (busy_count > 1000) {
				tracker_error ("ERROR: excessive busy count in query %s and thread %s", procedure, db_con->thread);
				exit (1);
			}

			if (busy_count > 50) {
				g_usleep (g_random_int_range (1000, busy_count * 200));
			} else {
				g_usleep (100);
			}

			lock_connection (db_con);
			continue;
		}

		

		unlock_db ();
		break;
	}

	unlock_connection (db_con);

	if (rc != SQLITE_DONE) {
		tracker_error ("ERROR: execution of prepared query %s failed due to %s with return code %d", procedure, sqlite3_errmsg (db_con->db), rc);
		db_con->in_error = TRUE;
		return FALSE;

	}

	return TRUE;
}


char ***
tracker_exec_proc_ignore_nulls (DBConnection *db_con, const char *procedure, int param_count, ...)
{
	va_list      args;
	int 	     i, busy_count, cols, row;
	sqlite3_stmt *stmt;
	char 	     **res;
	GSList	     *result;
	int	     rc;
	const GSList *tmp;

	stmt = get_prepared_query (db_con, procedure);

	va_start (args, param_count);

	if (param_count != sqlite3_bind_parameter_count (stmt)) {
		tracker_error ("ERROR: incorrect no. of paramters %d supplied to %s", param_count, procedure);
	}

	for (i = 0; i < param_count; i++) {
		char *str;

		str = va_arg (args, char *);

		if (!str) {
			tracker_debug ("WARNING: parameter %d is null when executing SP %s", i, procedure);
			if  (sqlite3_bind_null (stmt, i+1)) {
				tracker_error ("ERROR: null parameter %d could not be bound to %s", i, procedure);
			}
		} else {

			if (sqlite3_bind_text (stmt, i+1, str, strlen (str), SQLITE_TRANSIENT) != SQLITE_OK) {
				tracker_error ("ERROR: parameter %d could not be bound to %s", i, procedure);
			}
		}
	}

	va_end (args);

	cols = sqlite3_column_count (stmt);

	busy_count = 0;
	row = 0;

	result = NULL;
	
	lock_connection (db_con);
	while (TRUE) {

		if (!lock_db ()) {
			unlock_connection (db_con);	
			return NULL;
		}

		
		rc = sqlite3_step (stmt);
		

		if (rc == SQLITE_BUSY) {
			unlock_db ();
			unlock_connection (db_con);
			busy_count++;

			if (busy_count > 1000) {
				tracker_error ("ERROR: excessive busy count in query %s and thread %s", procedure, db_con->thread);
				exit (1);
			}

			if (busy_count > 50) {
				g_usleep (g_random_int_range (1000, busy_count * 200));
			} else {
				g_usleep (100);
			}

			lock_connection (db_con);
			continue;
		}

		if (rc == SQLITE_ROW) {
			char **new_row;

			new_row = g_new0 (char *, cols+1);
			new_row[cols] = NULL;

			unlock_db ();
			

			for (i = 0; i < cols; i++) {
				const char *st;

				st = (char *) sqlite3_column_text (stmt, i);

				if (st) {
					new_row[i] = g_strdup (st);
				} else {
					new_row[i] = g_strdup (" ");
				}
			}

			if (new_row && new_row[0]) {
				result = g_slist_prepend (result, new_row);
				row++;
			}

			continue;
		}

		unlock_db ();
		break;
	}

	unlock_connection (db_con);

	if (rc != SQLITE_DONE) {
		tracker_error ("ERROR: prepared query %s failed due to %s", procedure, sqlite3_errmsg (db_con->db));
	}

	if (!result || (row == 0)) {
		return NULL;
	}

	result = g_slist_reverse (result);

	res = g_new0 (char *, row+1);
	res[row] = NULL;

	tmp = result;

	for (i = 0; i < row; i++) {
		if (tmp) {
			res[i] = tmp->data;
			tmp = tmp->next;
		} else {
			tracker_error ("WARNING: exec proc has a dud entry");
		}
	}

	g_slist_free (result);

	return (char ***) res;
}




void
tracker_db_load_stored_procs (DBConnection *db_con)
{
}




void
tracker_create_db (void)
{
	DBConnection *db_con;

	tracker_log ("Creating tracker database...");

	
	/* create common db first */

	db_con = tracker_db_connect_common ();
	
	load_sql_file (db_con, "sqlite-tracker.sql");
	load_sql_file (db_con, "sqlite-service-types.sql");
	load_sql_file (db_con, "sqlite-metadata.sql");
	load_sql_trigger (db_con, "sqlite-tracker-triggers.sql");

	tracker_db_load_service_file (db_con, "default.metadata", FALSE);
	tracker_db_load_service_file (db_con, "file.metadata", FALSE);
	tracker_db_load_service_file (db_con, "audio.metadata", FALSE);
	tracker_db_load_service_file (db_con, "application.metadata", FALSE);
	tracker_db_load_service_file (db_con, "document.metadata", FALSE);
	tracker_db_load_service_file (db_con, "email.metadata", FALSE);
	tracker_db_load_service_file (db_con, "image.metadata", FALSE);	
	tracker_db_load_service_file (db_con, "video.metadata", FALSE);	

	tracker_db_load_service_file (db_con, "default.service", FALSE);

	tracker_db_exec_no_reply (db_con, "ANALYZE");

	tracker_db_close (db_con);

	g_free (db_con);



	
}





void
tracker_log_sql (DBConnection *db_con, const char *query)
{
	char ***res;

	res = tracker_exec_sql (db_con, query);

	if (!res) {
		return;
	}

	tracker_db_log_result (res);

	tracker_db_free_result (res);
}



static gboolean
db_exists (const char *name)
{
	gboolean is_present = FALSE;
	
	char *dbname = g_build_filename (tracker->data_dir, name, NULL);

	if (g_file_test (dbname, G_FILE_TEST_EXISTS)) {
		is_present = TRUE;
	}

	g_free (dbname);

	return is_present;
}


gboolean
tracker_db_needs_setup ()
{
	return (!db_exists ("file-meta.db") || !db_exists ("file-index.db") || !db_exists ("file-contents.db"));
}


gboolean 
tracker_db_needs_data ()
{
	gboolean need_setup;
	char	 *dbname;

	need_setup = FALSE;

	dbname = g_build_filename (tracker->user_data_dir, "common.db", NULL);

	if (!g_file_test (dbname, G_FILE_TEST_EXISTS)) {
		need_setup = TRUE;
	}

	g_free (dbname);

	return need_setup;

}


gboolean
tracker_update_db (DBConnection *db_con)
{
	char ***res;
	char **row;
	char *version;
	int i;

	res = tracker_exec_sql (db_con, "SELECT OptionValue FROM Options WHERE OptionKey = 'DBVersion'");

	if (!res) {
		return FALSE;
	}

	row = tracker_db_get_row (res, 0);

	if (!row || !row[0]) {
		tracker_db_free_result (res);
		return FALSE;
	}


	version = row[0];
	i = atoi (version);

	tracker_db_free_result (res);

	tracker_log ("Checking tracker DB version...Current version is %d and needed version is %d", i, TRACKER_DB_VERSION_REQUIRED);

	if (i < TRACKER_DB_VERSION_REQUIRED) {
		tracker_log ("Your database is too out of date and will need to be rebuilt and all your files reindexed.\nPlease wait while we reindex...\n");
		tracker_remove_dirs (tracker->data_dir);
		return TRUE;
	}

	if (i < 15) {
		tracker_db_exec_no_reply (db_con, "delete from MetaDataTypes where MetaName = 'Email:Body'");
		tracker_db_exec_no_reply (db_con, "delete from MetaDataTypes where MetaName = 'File:Contents'");
		tracker_db_exec_no_reply (db_con, "insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Body', 0, 0, 1)");
		
	} 

	if (i < 16) {
		tracker_db_exec_no_reply (db_con, "drop table ServiceTypes");
		tracker_db_exec_no_reply (db_con, "drop table MetaDataTypes");
		tracker_db_exec_no_reply (db_con, "drop table MetaDataChildren");

		load_sql_file (db_con, "sqlite-service-types.sql");
		load_sql_file (db_con, "sqlite-metadata.sql");

		tracker_exec_sql (db_con, "update Options set OptionValue = '16' where OptionKey = 'DBVersion'");

		tracker_exec_sql (db_con, "ANALYZE");
	}

	/* apply and table changes for each version update */
/*	while (i < TRACKER_DB_VERSION_REQUIRED) {
		char *sql_file, *query;

		i++;

		sql_file = g_strconcat (TRACKER_DATADIR, "/tracker/tracker-db-table-update", version, ".sql", NULL);

		tracker_log ("Please wait while database is being updated to the latest version");

		if (g_file_get_contents (sql_file, &query, NULL, NULL)) {
			char **queries, **queries_p ;

			queries = g_strsplit_set (query, ";", -1);

			for (queries_p = queries; *queries_p; queries_p++) {
				tracker_exec_sql (db, *queries_p);
			}

			g_strfreev (queries);
			g_free (query);
		}

		g_free (sql_file);
	}
*/

	return FALSE;
}


int
tracker_metadata_is_key (const char *service, const char *meta_name)
{
	int	 i;

	char *name = g_utf8_strdown (service, -1);

	ServiceDef *def =  g_hash_table_lookup (tracker->service_table, name);

	g_free (name);

	if (!def) {
		tracker_log ("WARNING: service %s not found", service);
		return 0;
	}

	GSList *list;
	i = 0;
	for (list=def->key_metadata; list; list=list->next) {

		i++;		
		if (list->data) {
			char *meta = (char *) list->data;

			if (strcasecmp (meta, meta_name) == 0) {
				return i;
			}
		}

	}

	return 0;

}


static inline gboolean
is_equal (const char *s1, const char *s2)
{
	return (strcasecmp (s1, s2) == 0);
}

char *
tracker_db_get_field_name (const char *service, const char *meta_name)
{
	int key_field = tracker_metadata_is_key (service, meta_name);

	if (key_field > 0) {
		return g_strdup_printf ("KeyMetadata%d", key_field);

	} 

	if (is_equal (meta_name, "File:Path")) return g_strdup ("Path");
	if (is_equal (meta_name, "File:Name")) return g_strdup ("Name");
	if (is_equal (meta_name, "File:Mime")) return g_strdup ("Mime");
	if (is_equal (meta_name, "File:Size")) return g_strdup ("Size");
	if (is_equal (meta_name, "File:Rank")) return g_strdup ("Rank");
	if (is_equal (meta_name, "File:Modified")) return g_strdup ("IndexTime");

	return NULL;

}


char *
tracker_db_get_display_field (FieldDef *def)
{
	if (def->type == DATA_INDEX || def->type == DATA_STRING || def->type == DATA_DOUBLE) {
		return g_strdup ("MetaDataDisplay");
	}

	return g_strdup ("MetaDataValue");

}




GHashTable *
tracker_db_get_file_contents_words (DBConnection *db_con, guint32 id, GHashTable *old_table)
{
	sqlite3_stmt 	*stmt;
	char		*str_file_id;
	int 		busy_count;
	int		rc;

	str_file_id = tracker_uint_to_str (id);

	stmt = get_prepared_query (db_con, "GetAllContents");

	sqlite3_bind_text (stmt, 1, str_file_id, strlen (str_file_id), SQLITE_STATIC);

	busy_count = 0;

	lock_connection (db_con);
	while (TRUE) {

		if (!lock_db ()) {
			unlock_connection (db_con);
			g_free (str_file_id);
			return NULL;
		}

		
		rc = sqlite3_step (stmt);
		

		if (rc == SQLITE_BUSY) {
			unlock_db ();
			unlock_connection (db_con);
			busy_count++;

			if (busy_count > 1000) {
				tracker_error ("ERROR: excessive busy count in query %s and thread %s", "save file contents", db_con->thread);
				exit (1);
			}

			if (busy_count > 50) {
				g_usleep (g_random_int_range (1000, (busy_count * 200) ));
			} else {
				g_usleep (100);
			}
			lock_connection (db_con);
			continue;
		}


		if (rc == SQLITE_ROW) {
			const char *st;

			st = (char *) sqlite3_column_text (stmt, 0);

			unlock_db ();

			if (st) {

				old_table = tracker_parse_text (old_table, st, 1, TRUE, FALSE);
			}

			continue;
		}

		unlock_db ();
		break;
	}
	unlock_connection (db_con);

	if (rc != SQLITE_DONE) {
		tracker_error ("WARNING: retrieval of text contents has failed");
	}

	g_free (str_file_id);

	return old_table;
}


GHashTable *
tracker_db_get_indexable_content_words (DBConnection *db_con, guint32 id, GHashTable *table, gboolean embedded_only)
{
	char ***res;
	char *str_id;

	str_id = tracker_uint_to_str (id);

	if (embedded_only) {
		res = tracker_exec_proc (db_con, "GetAllIndexable", 2, str_id, "1");
	} else {
		res = tracker_exec_proc (db_con, "GetAllIndexable", 2, str_id, "0");
	}

	if (res) {
		int  k;
		char **row;

		for (k = 0; (row = tracker_db_get_row (res, k)); k++) {

			if (row[0] && row[1]) {
				table = tracker_parse_text_fast (table, row[0], atoi (row[1]));
			}
		}

		tracker_db_free_result (res);
	}

	if (embedded_only) {
		res = tracker_exec_proc (db_con, "GetAllIndexableKeywords", 2, str_id, "1");
	} else {
		res = tracker_exec_proc (db_con, "GetAllIndexableKeywords", 2, str_id, "0");
	}


	if (res) {
		int  k;
		char **row;

		for (k = 0; (row = tracker_db_get_row (res, k)); k++) {

			if (row[0] && row[1]) {
				table = tracker_parse_text_fast (table, row[0], atoi (row[1]));
			}
		}

		tracker_db_free_result (res);
	}

	g_free (str_id);

	return table;
}

static void
save_full_text_bytes (DBConnection *blob_db_con, const char *str_file_id, GByteArray *byte_array)
{
	char		*value;
	sqlite3_stmt 	*stmt;
	int		busy_count;
	int		rc;

	stmt = get_prepared_query (blob_db_con, "SaveServiceContents");

	FieldDef *def = tracker_db_get_field_def (blob_db_con, "File:Contents");

	if (!def) {
		tracker_error ("WARNING: metadata not found for type %s", "File:Contents");
		return;
	}

	sqlite3_bind_text (stmt, 1, str_file_id, strlen (str_file_id), SQLITE_STATIC);
	sqlite3_bind_text (stmt, 2, def->id, strlen (def->id), SQLITE_STATIC);
	sqlite3_bind_blob (stmt, 3, (void *) byte_array->data, byte_array->len, SQLITE_STATIC);
	sqlite3_bind_int  (stmt, 4, 0);

	busy_count = 0;
	lock_connection (blob_db_con);

	while (TRUE) {

		if (!lock_db ()) {
			
			if (value) {
				g_free (value);
			}

			unlock_connection (blob_db_con);

			return;
		}

		
		rc = sqlite3_step (stmt);
		

		if (rc == SQLITE_BUSY) {
			unlock_db ();
			unlock_connection (blob_db_con);
			busy_count++;

			if (busy_count > 1000000) {
				tracker_log ("WARNING: excessive busy count in query %s and thread %s", "save file contents", blob_db_con->thread);
				busy_count = 0;
			}
			

			if (busy_count > 50) {
				g_usleep (g_random_int_range (1000, busy_count * 200));
			} else {
				g_usleep (100);
			}
			lock_connection (blob_db_con);
			continue;
		}

		unlock_db ();
		break;
	}

	unlock_connection (blob_db_con);
	
	if (rc != SQLITE_DONE) {
		tracker_error ("WARNING: failed to update contents ");
	}
}


static void
save_full_text (DBConnection *blob_db_con, const char *str_file_id, const char *text, int length)
{
	char		*value;
	sqlite3_stmt 	*stmt;
	char		*compressed;
	int		bytes_compressed;
	int		busy_count;
	int		rc;

	stmt = get_prepared_query (blob_db_con, "SaveServiceContents");

	compressed = tracker_compress (text, length, &bytes_compressed);

	if (compressed) {
		tracker_debug ("compressed full text size of %d to %d", length, bytes_compressed);
		value = compressed;
		
	} else {
		tracker_error ("WARNING: compression has failed");
		value = g_strdup (text);
		bytes_compressed = length;
	}


	FieldDef *def = tracker_db_get_field_def (blob_db_con, "File:Contents");

	if (!def) {
		tracker_error ("WARNING: metadata not found for type %s", "File:Contents");
		g_free (value);
		return;
	}

	sqlite3_bind_text (stmt, 1, str_file_id, strlen (str_file_id), SQLITE_STATIC);
	sqlite3_bind_text (stmt, 2, def->id, strlen (def->id), SQLITE_STATIC);
	sqlite3_bind_text (stmt, 3, value, bytes_compressed, SQLITE_STATIC);
	sqlite3_bind_int (stmt, 4, 0);

	busy_count = 0;
	lock_connection (blob_db_con);

	while (TRUE) {

		if (!lock_db ()) {
			

			if (value) {
				g_free (value);
			}

			unlock_connection (blob_db_con);

			return;
		}

		
		rc = sqlite3_step (stmt);
		

		if (rc == SQLITE_BUSY) {
			unlock_db ();
			unlock_connection (blob_db_con);
			busy_count++;

			if (busy_count > 1000000) {
				tracker_log ("WARNING: excessive busy count in query %s and thread %s", "save file contents", blob_db_con->thread);
				busy_count = 0;
			}
			

			if (busy_count > 50) {
				g_usleep (g_random_int_range (1000, busy_count * 200));
			} else {
				g_usleep (100);
			}
			lock_connection (blob_db_con);
			continue;
		}

		unlock_db ();
		break;
	}

	unlock_connection (blob_db_con);

	if (value) {
		g_free (value);
	}

	if (rc != SQLITE_DONE) {
		tracker_error ("WARNING: failed to update contents ");
	}
}


void
tracker_db_save_file_contents (DBConnection *db_con, GHashTable *index_table, GHashTable *old_table, const char *file_name, FileInfo *info)
{
	char 		buffer[MAX_TEXT_BUFFER], out[MAX_COMPRESS_BUFFER];
	int  		fd, bytes_read = 0, bytes_compressed = 0, flush;
	guint32		buffer_length;
	char		*str_file_id;
	z_stream 	strm;
	GByteArray	*byte_array;
	gboolean	finished = FALSE;
	int 		max_iterations = 10000;

	DBConnection *blob_db_con = db_con->blob;

	fd = tracker_file_open (file_name, TRUE);

	if (fd ==-1) {
		tracker_error ("ERROR: could not open file %s", file_name);
		return;
	}

 	strm.zalloc = Z_NULL;
    	strm.zfree = Z_NULL;
    	strm.opaque = Z_NULL;
    		
	if (deflateInit (&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
		tracker_error ("ERROR: could not initialise zlib");
		close (fd);
		return;
	}

	str_file_id = g_strdup_printf ("%d", info->file_id);

	if (!index_table) {
		index_table = g_hash_table_new (g_str_hash, g_str_equal);
	}

	byte_array = g_byte_array_sized_new (MAX_TEXT_BUFFER);

	while (!finished) {

		char *value = NULL;
		gboolean use_buffer = TRUE;

		max_iterations--;

		if (max_iterations < 0) break;

		buffer_length = read (fd, buffer, MAX_TEXT_BUFFER-1);

		if (buffer_length == 0) {
			finished = TRUE;
			break;
		} 

		bytes_read += buffer_length;

		buffer[buffer_length] = '\0';
								
		if (buffer_length == (MAX_TEXT_BUFFER - 1)) {

			/* seek beck to last line break so we get back a clean string for utf-8 validation */
			char *end = strrchr (buffer, '\n');			

			if (!end) {
				tracker_log ("Could not find line break in text chunk..exiting");
				break;
			}

			int bytes_backtracked = strlen (end) * -1;

			buffer_length += bytes_backtracked;

			buffer[buffer_length] = '\0';

			if (lseek (fd, bytes_backtracked, SEEK_CUR) == -1) {
				tracker_error ("Could not seek to line break in text chunk");
				break;
			}

		} else {
			finished = TRUE;
		}
		
	

		if (!g_utf8_validate (buffer, buffer_length, NULL)) {

			value = g_locale_to_utf8 (buffer, buffer_length, NULL, &buffer_length, NULL);

			if (!value) {
				finished = FALSE;
				tracker_info ("could not convert text to valid utf8");
				break;
			}

			use_buffer = FALSE;

		} 
		
		if (use_buffer) {
			index_table = tracker_parse_text (index_table, buffer, 1, TRUE, FALSE);
		} else {
			index_table = tracker_parse_text (index_table, value, 1, TRUE, FALSE);
		}

		strm.avail_in = buffer_length;

		if (use_buffer) {
			strm.next_in = (unsigned char *) buffer;
		} else {
			strm.next_in = (unsigned char *) value;	
		}

            	strm.avail_out = MAX_COMPRESS_BUFFER;
            	strm.next_out = (unsigned char *) out;
			
		/* set upper limit on text we read in */
		if (finished || bytes_read >= tracker->max_index_text_length) {
			finished = TRUE;
			flush = Z_FINISH;
		} else {
			flush = Z_NO_FLUSH;
		}


		/* compress */
       	        do {
               		int ret = deflate (&strm, flush);   
 
            		if (ret == Z_STREAM_ERROR) {
				finished = FALSE;
				tracker_error ("compression failed");
				if (!use_buffer) g_free (value);
				break;
			}

		        bytes_compressed = 65565 - strm.avail_out;

			byte_array =  g_byte_array_append (byte_array, (guint8 *) out, bytes_compressed);

			max_iterations--;

			if (max_iterations < 0) break;

              	} while (strm.avail_out == 0);

		if (!use_buffer) g_free (value);

		if (tracker->throttle > 9) {
			tracker_throttle (tracker->throttle * 100);
		}

	}        	

  
	deflateEnd(&strm);

	/* flush cache for file as we wont touch it again */
	tracker_file_close (fd, TRUE);

	if (finished && max_iterations > 0) {
		if (bytes_read > 2) {
			save_full_text_bytes (blob_db_con, str_file_id, byte_array);
		}
	} else {
		tracker_info ("An error prevented full text extraction");
	}
 
	g_byte_array_free (byte_array, TRUE);

	g_free (str_file_id);

	
}



void
tracker_db_clear_temp (DBConnection *db_con)
{
	tracker_db_start_transaction (db_con->cache);
	tracker_db_exec_no_reply (db_con->cache, "DELETE FROM FilePending");
	tracker_db_exec_no_reply (db_con->cache, "DELETE FROM FileWatches");
	tracker_db_end_transaction (db_con->cache);
}



gboolean
tracker_db_start_transaction (DBConnection *db_con)
{
	//if (db_con->in_transaction) {
	//	tracker_error ("Error - cannot start transaction - database is already in a transaction");
	//}

	if (!tracker_db_exec_no_reply (db_con, "BEGIN TRANSACTION")) {
		tracker_error ("could not start transaction");
		return FALSE;
	}

	db_con->in_transaction = TRUE;
	return TRUE;
}


gboolean
tracker_db_end_transaction (DBConnection *db_con)
{

	if (!db_con->in_transaction) {
		tracker_error ("Error - cannot end transaction. Rolling back...");
		return FALSE;
	}

	db_con->in_transaction = FALSE;

	if (!tracker_db_exec_no_reply (db_con, "COMMIT")) {
		tracker_error ("could not commit transaction");
		tracker_db_exec_no_reply (db_con, "ROLLBACK");		
		return FALSE;
	}	
	
	return TRUE;
}


void
tracker_db_check_tables (DBConnection *db_con)
{
}




char ***
tracker_db_search_text (DBConnection *db_con, const char *service, const char *search_string, int offset, int limit, gboolean save_results, gboolean detailed)
{
	char 		**result, **array;
	GSList 		*hit_list;
	int 		count;
	const GSList	*tmp;
	gboolean	detailed_emails = FALSE, detailed_apps = FALSE;
	int		service_array[255];

	array = tracker_parse_text_into_array (search_string);

	char ***res = tracker_exec_proc (db_con->common, "GetRelatedServiceIDs", 2, service, service);
	
	int i = 0,j =0;
	char **row;
	
	if (res) {	
	
		while ((row = tracker_db_get_row (res, i))) {

			if (row[0]) {
				service_array[j] = atoi (row[0]);
				j++;
			}

			i++;
		}
			
		tracker_db_free_result (res);
	}

	SearchQuery *query = tracker_create_query (db_con->word_index, service_array, j, offset, limit);

	char **pstr;

	for (pstr = array; *pstr; pstr++) {
		tracker_add_query_word (query, *pstr, WordNormal);	
		
	}

	g_strfreev (array);

	if (!tracker_indexer_get_hits (query)) {

		tracker_free_query (query);
		return NULL;

	}

	hit_list = query->hits;

	result = NULL;

	if (!save_results) {
		count = g_slist_length (hit_list);

		if (count > limit) count = limit;

		result = g_new (char *, count + 1);
	} else {
		tracker_db_start_transaction (db_con);
		tracker_exec_proc (db_con, "DeleteSearchResults1", 0);
	}

	count = 0;

	for (tmp = hit_list; tmp; tmp = tmp->next) {

		SearchHit *hit;
		char	  *str_id;
		char	  ***res;

		if (count >= limit) break;

		hit = tmp->data;

		str_id = tracker_uint_to_str (hit->service_id);

		/* we save results into SearchResults table instead of returing an array of array of strings */
		if (save_results) {
			char *str_score;

			str_score = tracker_int_to_str (hit->score);

			tracker_exec_proc (db_con, "InsertSearchResult1", 2, str_id, str_score);

			g_free (str_id);
			g_free (str_score);

			continue;
		}

		if (detailed) {

			if (strcmp (service, "Emails") == 0) {
				detailed_emails = TRUE;
				res = tracker_exec_proc_ignore_nulls (db_con, "GetEmailByID", 1, str_id);

			} else if (strcmp (service, "Applications") == 0) {
				detailed_apps = TRUE;
				res = tracker_exec_proc_ignore_nulls (db_con, "GetApplicationByID", 1, str_id);

			} else {
				res = tracker_exec_proc_ignore_nulls (db_con, "GetFileByID2", 1, str_id);
			}

		} else {
			res = tracker_exec_proc_ignore_nulls (db_con, "GetFileByID", 1, str_id);
		}

		g_free (str_id);

		if (res) {
			if (res[0] && res[0][0] && res[0][1]) {

				char **row = NULL;

				if (detailed) {

					if (detailed_emails) {
						row = g_new0 (char *, 6);

						row[0] = g_strdup (res[0][0]);
						row[1] = g_strdup (res[0][1]);
						row[2] = NULL;
						row[3] = NULL;
						row[4] = NULL;
						row[5] = NULL;

						if (res[0][2]) {
							row[2] = g_strdup (res[0][2]);						
							if (res[0][3]) {
								row[3] = g_strdup (res[0][3]);	
								if (res[0][4]) {
									row[4] = g_strdup (res[0][4]);						
								}	
							}
							

							 
						}					
						

					} else if (detailed_apps) {
						row = g_new0 (char *, 7);

						row[0] = g_strdup (res[0][0]);
						row[1] = g_strdup (res[0][1]);
						row[2] = g_strdup (res[0][2]);
						row[3] = NULL;
						row[4] = NULL;
						row[5] = NULL;
						row[6] = NULL;

						if (res[0][3]) {
							row[3] = g_strdup (res[0][3]);							
							if (res[0][4]) {
								row[4] = g_strdup (res[0][4]);	
								if (res[0][5]) {
									row[5] = g_strdup (res[0][5]);						
								}	
							}
							

							 
						}			


					} else {

						if (res[0][2] && g_file_test (res[0][0], G_FILE_TEST_EXISTS)) {

							row = g_new (char *, 4);

							row[0] = g_strdup (res[0][0]);
							row[1] = g_strdup (res[0][1]);
							row[2] = g_strdup (res[0][2]);
							row[3] = NULL;
						}
					}

				} else {

					row = g_new (char *, 3);

					row[0] = g_strdup (res[0][0]);
					row[1] = g_strdup (res[0][1]);
					row[2] = NULL;
				}
				
				result[count] = (char *) row;
				count++;
			}

			tracker_db_free_result (res);
		}

	}

	if (save_results) {
		tracker_db_end_transaction (db_con);
	} else {
		result[count] = NULL;
	}

	tracker_free_query (query);

	return (char ***)result;
}


char ***
tracker_db_search_files_by_text (DBConnection *db_con, const char *text, int offset, int limit, gboolean sort)
{
	char ***result;

	result = NULL;

	return result;
}


char ***
tracker_db_search_metadata (DBConnection *db_con, const char *service, const char *field, const char *text, int offset, int limit)
{
	FieldDef *def;
	char	 ***res;

	g_return_val_if_fail ((service && field && text), NULL);

	def = tracker_db_get_field_def (db_con, field);

	if (!def) {
		tracker_error ("ERROR: metadata not found for type %s", field);
		return NULL;
	}

	switch (def->type) {

		case 0: 
		case 1: res = tracker_exec_proc (db_con, "SearchMetadata", 2, def->id, text); break;

		case 2:
		case 3: res = tracker_exec_proc (db_con, "SearchMetadataNumeric", 2, def->id, text); break;

		case 5: res = tracker_exec_proc (db_con, "SearchMetadataKeywords", 2, def->id, text); break;

		default: tracker_error ("ERROR: metadata could not be retrieved as type %d is not supported", def->type); res = NULL;
	}


	return res;
}


char ***
tracker_db_search_matching_metadata (DBConnection *db_con, const char *service, const char *id, const char *text)
{
	char ***result;

	g_return_val_if_fail (id, NULL);

	result = NULL;

	return result;
}


/*
static int
get_metadata_type (DBConnection *db_con, const char *meta)
{
	char ***res;
	int  result;

	res = tracker_exec_proc (db_con, "GetMetaDataTypeID", 1, meta);

	if (res && res[0][0]) {
		result = atoi (res[0][0]);
		tracker_db_free_result (res);
	} else {
		result = -1;
	}

	return result;
}
*/



char ***
tracker_db_get_metadata (DBConnection *db_con, const char *service, const char *id, const char *key)
{
	FieldDef *def;
	char	 ***res;

	g_return_val_if_fail (id, NULL);

	def = tracker_db_get_field_def (db_con, key);

	if (!def) {
		tracker_error ("ERROR: metadata not found for id %s and type %s", id, key);
		return NULL;
	}

	switch (def->type) {
		
		case DATA_INDEX:
		case DATA_STRING:
		case DATA_DOUBLE:
			res = tracker_exec_proc (db_con, "GetMetadata", 2, id, def->id); break;

		case DATA_INTEGER:
		case DATA_DATE: res = tracker_exec_proc (db_con, "GetMetadataNumeric", 2, id, def->id); break;

		case DATA_FULLTEXT: res = tracker_exec_proc (db_con, "GetContents", 2, id, def->id); break;
			
		case DATA_KEYWORD:
			res = tracker_exec_proc (db_con, "GetMetadataKeyword", 2, id, def->id); break;

		default: tracker_error ("ERROR: metadata could not be retrieved as type %d is not supported", def->type); res = NULL;
	}


	return res;
}


/* gets specified metadata value as a single row (multple values for a metadata type are returned delimited by  "|" ) */
char *	
tracker_db_get_metadata_delimited (DBConnection *db_con, const char *service, const char *id, const char *key)
{
	GString *gstr = NULL;
	char **row;

	char ***res = tracker_db_get_metadata (db_con, service, id, key);
	
	if (res) {
		int i = 0;

		while ((row = tracker_db_get_row (res, i))) {

			if (row[0]) {
				if (gstr) {
					g_string_append_printf (gstr, "|%s", row[0]);
				} else {
					gstr = g_string_new (row[0]);
				}						
			}
		
			i++;
		}

		tracker_db_free_result (res);

	}

	if (gstr) {
		return g_string_free (gstr, FALSE);
	} else {
		return NULL;
	}

}


static void
update_metadata_index (DBConnection *db_con, const char *id, const char *service, FieldDef *def, const char *old_value, const char *new_value) 
{
	GHashTable *old_table, *new_table;

	if (!def) {
		tracker_error ("ERROR: cannot find details for metadata type");
		return;
	}


	old_table = NULL;
	new_table = NULL;

	if (old_value) {
		old_table = tracker_parse_text (old_table, old_value, def->weight, def->filtered, def->delimited);
	}

	/* parse new metadata value */
	if (new_value) {
		new_table = tracker_parse_text (new_table, new_value, def->weight, def->filtered, def->delimited);
	}

	/* we only do differential updates so only changed words scores are updated */
	
	int sid;

	sid = tracker_get_id_for_service (service);
	tracker_db_update_differential_index (db_con, old_table, new_table, id, sid);

	tracker_word_table_free (old_table);
	tracker_word_table_free (new_table);
}



char *
tracker_get_related_metadata_names (DBConnection *db_con, const char *name)
{
	char	 ***res = NULL;

	res = tracker_exec_proc (db_con, "GetMetadataAliasesForName", 2, name, name);

	if (res) {
		int k = 0;
		GString *str;
		char **row;

		str = g_string_new ("");

		while ((row = tracker_db_get_row (res, k))) {
			if (row[1]) {
				if (k==0) {
					g_string_append (str, row[1]);
				} else {
					g_string_append_printf (str, ", %s", row[1]);
				}
			}
		
			k++;
		}

		char *value = g_string_free (str, FALSE);
		
		tracker_db_free_result (res);

		return value;
	}

	return NULL;
}

char *
tracker_get_metadata_table (DataTypes type)
{
	switch (type) {

		case DATA_INDEX:
		case DATA_STRING:
		case DATA_DOUBLE:
			return g_strdup ("ServiceMetaData");
		
		case DATA_INTEGER:
		case DATA_DATE:
			return g_strdup ("ServiceNumericMetaData");

		case DATA_BLOB: return g_strdup("ServiceBlobMetaData");

		case DATA_KEYWORD: return g_strdup("ServiceKeywordMetaData");

		default: return NULL;
	}

	return NULL;
}


static char *
format_date (const char *avalue)
{

	char *dvalue;

	dvalue = tracker_format_date (avalue);

	if (dvalue) {
		time_t time;

		time = tracker_str_to_date (dvalue);

		g_free (dvalue);

		if (time != -1) {
			return (tracker_int_to_str (time));
		} 
	}

	return NULL;

}


/* fast insert of embedded metadata. Table parameter is used to build up a unique word list of indexable contents */ 
void
tracker_db_insert_single_embedded_metadata (DBConnection *db_con, const char *service, const char *id, const char *key, const char *value, GHashTable *table)
{
	char *array[1];

	array[0] = (char *)value;
		
	tracker_db_insert_embedded_metadata (db_con, service, id, key, array, 1, table);
}

void
tracker_db_insert_embedded_metadata (DBConnection *db_con, const gchar *service, const gchar *id, const gchar *key, gchar **values, gint length, GHashTable *table)
{
	gint	key_field = 0;

	if (!service || !id || !key || !values || !values[0]) {
		return;
	}

	FieldDef *def = tracker_db_get_field_def (db_con, key);

	if (!def) {
		tracker_error ("ERROR: metadata %s not found", key);
		return;
	}

	g_return_if_fail (def->embedded);

	if (length == -1) {
		length = 0;
		while (values[length] != NULL) {
			length++;
		}
	}
	
        key_field = tracker_metadata_is_key (service, key);

	switch (def->type) {

                case DATA_KEYWORD: {
                        gint i;
			for (i = 0; i < length; i++) {
                                if (!values[i] || !values[i][0]) {
                                        continue;
                                }

				if (table) {
					gchar *mvalue = tracker_parse_text_to_string (values[i], FALSE, FALSE);

					table = tracker_parse_text_fast (table, mvalue, def->weight);

					g_free (mvalue);
				}
	
				tracker_exec_proc (db_con, "SetMetadataKeyword", 3, id, def->id, values[i]); 
			}

			break;
                }
                case DATA_INDEX: {
                        gint i;
			for (i = 0; i < length; i++) {
                                gchar *mvalue;

                                if (!values[i] || !values[i][0]) {
                                        continue;
                                }

				mvalue = tracker_parse_text_to_string (values[i], def->filtered, def->delimited);

				if (table) {
					table = tracker_parse_text_fast (table, mvalue, def->weight);
				}
				
				tracker_exec_proc (db_con, "SetMetadata", 4, id, def->id, mvalue, values[i]); 
				
				g_free (mvalue);
			}

			break;
                }
                case DATA_FULLTEXT: {
                        gint i;
			for (i = 0; i < length; i++) {
                                if (!values[i] || !values[i][0]) {
                                        continue;
                                }

				if (table) {
					table = tracker_parse_text  (table, values[i], def->weight, def->filtered, def->delimited);
				}
	
				save_full_text (db_con->blob, id, values[i], strlen (values[i]));
			}

			break;
                }
                case DATA_DOUBLE: {
                        gint i;
			for (i = 0; i < length; i++) {
                                if (!values[i]) {
                                        continue;
                                }

				tracker_exec_proc (db_con, "SetMetadata", 4, id, def->id, " ", values[i]); 
			}

                        break;
                }
                case DATA_STRING: {
                        gint i;
			for (i = 0; i < length; i++) {
                                if (!values[i]) {
                                        continue;
                                }

				gchar *mvalue = tracker_parse_text_to_string (values[i], def->filtered, def->delimited);

				tracker_exec_proc (db_con, "SetMetadata", 4, id, def->id, mvalue, values[i]);

				g_free (mvalue);
			}

			break;
                }
                case DATA_INTEGER: {
                        gint i;
			for (i = 0; i < length; i++) {
                                if (!values[i]) {
                                        continue;
                                }

				tracker_exec_proc (db_con, "SetMetadataNumeric", 3, id, def->id, values[i]); 
			}

			break;
                }
                case DATA_DATE: {
                        gint i;
			for (i = 0; i < length; i++) {
                                if (!values[i]) {
                                        continue;
                                }

				gchar *mvalue = format_date (values[i]);

				if (!mvalue) {
					tracker_debug ("Could not format date %s", values[i]);
					continue;
				}

				tracker_exec_proc (db_con, "SetMetadataNumeric", 3, id, def->id, mvalue); 

				g_free (mvalue);
			}

			break;
                }
                default: {
			tracker_error ("ERROR: metadata could not be set as type %d for metadata %s is not supported",
                                       def->type, key);
			break;
                }
	}

	if (key_field > 0) {

		if (values[0]) {
			gchar *esc_value = NULL;

			if (def->type == DATA_DATE) {
				esc_value = format_date (values[0]);

				if (!esc_value) {
                                        return;
                                }

			} else {
				gchar *my_val = tracker_array_to_str (values, length, '|');
			
				esc_value = tracker_escape_string (my_val);
				g_free (my_val);
			}

			gchar *sql = g_strdup_printf ("update Services set KeyMetadata%d = '%s' where id = %s",
                                                      key_field, esc_value, id);

			tracker_db_exec_no_reply (db_con, sql);

			g_free (esc_value);
			g_free (sql);
		}
	}
}


static char *
get_backup_id (DBConnection *db_con, const char *id)
{
	char *backup_id = NULL;
	DBConnection *db_common = db_con->common;

	char ***res = tracker_exec_proc (db_common, "GetBackupServiceByID", 1, id);

	if (res) {
		if (res[0] && res[0][0]) {
			backup_id = g_strdup (res[0][0]);	
		} else {
			tracker_exec_proc (db_common, "InsertBackupService", 1, id);
			backup_id = tracker_int_to_str (sqlite3_last_insert_rowid (db_common->db));

		}

		tracker_db_free_result (res);

	} else {
		tracker_exec_proc (db_common, "InsertBackupService", 1, id);
		backup_id = tracker_int_to_str (sqlite3_last_insert_rowid (db_common->db));
	}


	return backup_id;


}


static inline void
backup_non_embedded_metadata (DBConnection *db_con, const char *id, const char *key_id, const char *value)
{

	char *backup_id = get_backup_id (db_con, id);

	if (backup_id) {
		tracker_exec_proc (db_con->common, "SetBackupMetadata", 3, backup_id, key_id, value);
		g_free (backup_id);
	}

}



static inline void
backup_delete_non_embedded_metadata_value (DBConnection *db_con, const char *id, const char *key_id, const char *value)
{

	char *backup_id = get_backup_id (db_con, id);

	if (backup_id) {
		tracker_exec_proc (db_con->common, "DeleteBackupMetadataValue", 3, backup_id, key_id, value);
		g_free (backup_id);
	}

}

static inline void
backup_delete_non_embedded_metadata (DBConnection *db_con, const char *id, const char *key_id)
{

	char *backup_id = get_backup_id (db_con, id);

	if (backup_id) {
		tracker_exec_proc (db_con->common, "DeleteBackupMetadata", 2, backup_id, key_id);
		g_free (backup_id);
	}

}


void
tracker_db_set_single_metadata (DBConnection *db_con, const char *service, const char *id, const char *key, const char *value, gboolean do_backup)
{
	char *array[1];

	array[0] = (char *)value;

	tracker_db_set_metadata (db_con, service, id, key, array, 1, do_backup);


}


char *
tracker_db_set_metadata (DBConnection *db_con, const char *service, const char *id, const char *key, char **values, int length, gboolean do_backup)
{
	FieldDef   	*def;
	char 		*old_value = NULL, *new_value = NULL;
	gboolean 	update_index;
	int		key_field = 0;
	int 		i;
	GString 	*str = NULL;
	char 		*res_service;
	

	g_return_val_if_fail (id && values && key && service, NULL);

	if (strcmp (id, "0") == 0) {
		return NULL;
	}

	def = tracker_db_get_field_def (db_con, key);

	if (!def) {
		tracker_error ("metadata type %s not found", key);
		return NULL;

	}
	
	res_service = tracker_db_get_service_for_entity (db_con, id);

	if (!res_service) {
		tracker_error ("ERROR: service not found for id %s", id);
		return NULL;
	}
	
	if (def->multiple_values && length > 1) {
		str = g_string_new ("");
	}



	key_field = tracker_metadata_is_key (res_service, key);
	update_index = (def->type == DATA_INDEX || def->type == DATA_KEYWORD || def->type ==  DATA_FULLTEXT);

	
	if (update_index) {
		old_value = tracker_db_get_metadata_delimited (db_con, service, id, key);
	}

	/* delete old value if metadata does not support multiple values */
	if (!def->multiple_values) {
		tracker_db_delete_metadata (db_con, service, id, key, FALSE);
	}


	switch (def->type) {

		case DATA_KEYWORD:

			for (i=0; i<length; i++) {

				if (!values[i] || !values[i][0]) continue;

				tracker_exec_proc (db_con, "SetMetadataKeyword", 3, id, def->id, values[i]);

				/* backup non-embedded data for embedded services */
				if (do_backup && !def->embedded && tracker_is_service_embedded (service)) {
					backup_non_embedded_metadata (db_con, id, def->id, values[i]);
				}

				if (str) {
					g_string_append_printf (str, " %s ", values[i]);
				} else {
					new_value = values[i];					
				}

				tracker_log ("saving keyword %s", values[i]);
			}

			break;

		case DATA_INDEX:
			
			for (i=0; i<length; i++) {

				if (!values[i] || !values[i][0]) continue;

				if (str) {
					g_string_append_printf (str, " %s ", values[i]);
				} else {
					new_value = values[i];					
				}

				/* backup non-embedded data for embedded services */
				if (do_backup && !def->embedded && tracker_is_service_embedded (service)) {
					backup_non_embedded_metadata (db_con, id, def->id, values[i]);
				}


				char *mvalue = tracker_parse_text_to_string (values[i], def->filtered, def->delimited);

				tracker_exec_proc (db_con, "SetMetadata", 4, id, def->id, mvalue, values[i]); 

				g_free (mvalue);

			}

			break;

		case DATA_FULLTEXT:
	
			/* we do not support multiple values for fulltext clobs */
						
			if (!values[0]) break;

			save_full_text (db_con->blob, id, values[0], strlen (values[0]));
			new_value = values[0];

			break;


		case DATA_STRING:

			for (i=0; i<length; i++) {

				if (!values[i] || !values[i][0]) continue;

				/* backup non-embedded data for embedded services */
				if (do_backup && !def->embedded && tracker_is_service_embedded (service)) {
					backup_non_embedded_metadata (db_con, id, def->id, values[i]);
				}

				char *mvalue = tracker_parse_text_to_string (values[i], def->filtered, def->delimited);

				tracker_exec_proc (db_con, "SetMetadata", 4, id, def->id, mvalue, values[i]);

				g_free (mvalue);
			}
			break;

		case DATA_DOUBLE:

			
			for (i=0; i<length; i++) {

				if (!values[i] || !values[i][0]) continue;

				/* backup non-embedded data for embedded services */
				if (do_backup && !def->embedded && tracker_is_service_embedded (service)) {
					backup_non_embedded_metadata (db_con, id, def->id, values[i]);
				}


				tracker_exec_proc (db_con, "SetMetadata", 4, id, def->id, " ", values[i]); 

			}
			break;

		

		case DATA_INTEGER:
	
			for (i=0; i<length; i++) {
				if (!values[i] || !values[i][0]) continue;

				/* backup non-embedded data for embedded services */
				if (do_backup && !def->embedded && tracker_is_service_embedded (service)) {
					backup_non_embedded_metadata (db_con, id, def->id, values[i]);
				}


				tracker_exec_proc (db_con, "SetMetadataNumeric", 3, id, def->id, values[i]); 
			}

			break;

		case DATA_DATE:

			for (i=0; i<length; i++) {

				if (!values[i] || !values[i][0]) continue;

				char *mvalue = format_date (values[i]);

				if (!mvalue) {
					tracker_debug ("Could not format date %s", values[i]);
					continue;

				}

				tracker_exec_proc (db_con, "SetMetadataNumeric", 3, id, def->id, mvalue); 

				/* backup non-embedded data for embedded services */
				if (do_backup && !def->embedded && tracker_is_service_embedded (service)) {
					backup_non_embedded_metadata (db_con, id, def->id, mvalue);
				}


				g_free (mvalue);
			}

			break;

		default :
			
			tracker_error ("ERROR: metadata could not be set as type %d for metadata %s is not supported", def->type, key);
			break;

		

		
	}

	if (key_field > 0) {



		if (values[0]) {
			char *esc_value = NULL;

			if (def->type == DATA_DATE) {
				esc_value = format_date (values[0]);

				if (!esc_value) return NULL;

			} else {

				char *my_val = tracker_array_to_str (values, length, '|');
			
				esc_value = tracker_escape_string (my_val);
				g_free (my_val);

			}

			char *sql = g_strdup_printf ("update Services set KeyMetadata%d = '%s' where id = %s", key_field, esc_value, id);

			tracker_db_exec_no_reply (db_con, sql);

			g_free (esc_value);	
			g_free (sql);
		}

	}


	

	/* update fulltext index differentially with current and new values */
	if (update_index) {

		if (str) {
			update_metadata_index (db_con, id, res_service, def, old_value, str->str);
			g_string_free (str, TRUE);
		} else {
			update_metadata_index (db_con, id, res_service, def, old_value, new_value);	
		}
	}

	g_free (old_value);
	g_free (res_service);

	return NULL;

}

static char *
remove_value (const char *str, const char *del_str) 
{
	char **tmp, **array = g_strsplit (str, "|", -1);

	GString *s = NULL;

	for (tmp = array; *tmp; tmp++) {

		if (tracker_is_empty_string (*tmp)) {
			continue;
		}

		if (strcmp (del_str, *tmp) != 0) {
			
			if (!s) {
				s = g_string_new (*tmp);
			} else {
				g_string_append_printf (s, "%s%s", "|", *tmp);
			}
		}
	}

	g_strfreev (array);

	if (!s) {
		return NULL;
	}

	return g_string_free (s, FALSE);

}

void 
tracker_db_delete_metadata_value (DBConnection *db_con, const char *service, const char *id, const char *key, const char *value) 
{

	char 		*old_value = NULL, *new_value = NULL, *mvalue;
	FieldDef	*def;
	gboolean 	update_index;

	g_return_if_fail (id && key && service && db_con);

	/* get type details */
	def = tracker_db_get_field_def (db_con, key);

	if (!def) {
		return;
	}


	if (!def->embedded && tracker_is_service_embedded (service)) {
		backup_delete_non_embedded_metadata_value (db_con, id, def->id, value);
	}


	char *res_service = tracker_db_get_service_for_entity (db_con, id);

	if (!res_service) {
		tracker_error ("ERROR: entity not found");
		return;
	}

	int key_field = tracker_metadata_is_key (res_service, key);

	update_index = (def->type == DATA_INDEX || def->type == DATA_KEYWORD);

	if (update_index) {

		/* get current value and claculate the new value */	

		old_value = tracker_db_get_metadata_delimited (db_con, service, id, key);
	
		if (old_value) {
			new_value = remove_value (old_value, value);
		} else {
			g_free (res_service);
			return;
		}

	}


	/* perform deletion */
	switch (def->type) {

		case DATA_INDEX:
		case DATA_STRING:
			mvalue = tracker_parse_text_to_string (value, def->filtered, def->delimited);
			tracker_exec_proc (db_con, "DeleteMetadataValue", 3, id, def->id, mvalue); 
			g_free (mvalue);
			break;


		case DATA_DOUBLE:
			tracker_exec_proc (db_con, "DeleteMetadataValue", 3, id, def->id, value); 
			break;

		
		case DATA_INTEGER:
		case DATA_DATE:

			tracker_exec_proc (db_con, "DeleteMetadataNumericValue", 3, id, def->id, value);  
			break;

		
		case DATA_KEYWORD:
			
			tracker_exec_proc (db_con, "DeleteMetadataKeywordValue", 3, id, def->id, value); 
			break;
		
		default:	
			tracker_error ("ERROR: metadata could not be deleted as type %d for metadata %s is not supported", def->type, key);
			break;


	}

	if (key_field > 0) {
	
		char ***res = tracker_db_get_metadata (db_con, service, id, key);
	
		if (res) {
			char **row;
	
			row = tracker_db_get_row (res, 0);

			if (row && row[0]) {
				char *esc_value = tracker_escape_string (row[0]);
				char *sql = g_strdup_printf ("update Services set KeyMetadata%d = '%s' where id = %s", key_field, esc_value, id);

				tracker_db_exec_no_reply (db_con, sql);

				g_free (esc_value);	
				g_free (sql);	

			} else {
				char *sql = g_strdup_printf ("update Services set KeyMetadata%d = NULL where id = %s", key_field, id);

				tracker_db_exec_no_reply (db_con, sql);
		
				g_free (sql);

			}

			tracker_db_free_result (res);
		
		} else {
			char *sql = g_strdup_printf ("update Services set KeyMetadata%d = NULL where id = %s", key_field, id);

			tracker_db_exec_no_reply (db_con, sql);
		
			g_free (sql);


		}
				
	} 


	/* update fulltext index differentially with old and new values */
	if (update_index) {
		update_metadata_index (db_con, id, service, def, old_value, new_value);
	}

	g_free (new_value);
	g_free (old_value);

	g_free (res_service);
	
}


void 
tracker_db_delete_metadata (DBConnection *db_con, const char *service, const char *id, const char *key, gboolean update_indexes) 
{
	char 		*old_value = NULL;
	FieldDef	*def;
	gboolean 	update_index;

	g_return_if_fail (id && key && service && db_con);


	/* get type details */
	def = tracker_db_get_field_def (db_con, key);

	if (!def) {
		return;
	}
	
	if (!def->embedded && tracker_is_service_embedded (service)) {
		backup_delete_non_embedded_metadata (db_con, id, def->id);
	}


	char *res_service = tracker_db_get_service_for_entity (db_con, id);

	if (!res_service) {
		tracker_error ("ERROR: entity not found");
		return;
	}


	int key_field = tracker_metadata_is_key (res_service, key);

	update_index = update_indexes && (def->type == DATA_INDEX || def->type == DATA_KEYWORD);


	if (update_index) {
		/* get current value */	
		old_value = tracker_db_get_metadata_delimited (db_con, service, id, key);
		tracker_debug ("old value is %s", old_value);
		
	}


	
	if (key_field > 0) {
		char *sql = g_strdup_printf ("update Services set KeyMetadata%d = NULL where id = %s", key_field, id);

		tracker_db_exec_no_reply (db_con, sql);
		
		g_free (sql);
	}
	
	
	/* perform deletion */
	switch (def->type) {

		case DATA_INDEX:
		case DATA_STRING:
		case DATA_DOUBLE:
			tracker_exec_proc (db_con, "DeleteMetadata", 2, id, def->id); 
			break;

		case DATA_INTEGER:
		case DATA_DATE:
			tracker_exec_proc (db_con, "DeleteMetadataNumeric", 2, id, def->id);  
			break;

		
		case DATA_KEYWORD:
			tracker_exec_proc (db_con, "DeleteMetadataKeyword", 2, id, def->id); 
			break;

		case DATA_FULLTEXT:

			tracker_exec_proc (db_con, "DeleteContent", 2, id, def->id); 
			break;						

		default:
			tracker_error ("ERROR: metadata could not be deleted as this operation is not suppprted by type %d for metadata %s", def->type, key);
			break;

	}

	
	/* update fulltext index differentially with old values and NULL */
	if (update_index && old_value) {
		update_metadata_index (db_con, id, service, def, old_value, " ");
	}

	
	g_free (old_value);
	g_free (res_service);


}

guint32
tracker_db_create_service (DBConnection *db_con, const char *service, FileInfo *info)
{
	char	   ***res;
	int	   i;
	guint32	   id = 0;
	char	   *sid;
	char	   *str_mtime;
	const char *str_is_dir, *str_is_link;
	char	   *str_filesize, *str_offset, *str_aux;
	int	   service_type_id;
	char	   *str_service_type_id, *path, *name;

	if (!info || !info->uri || !info->uri[0] || !service || !db_con) {
		tracker_error ("ERROR: cannot create service");
		return 0;

	}

	if (info->uri[0] == G_DIR_SEPARATOR) {
		name = g_path_get_basename (info->uri);
		path = g_path_get_dirname (info->uri);
	} else {
		name = tracker_get_vfs_name (info->uri);
		path = tracker_get_vfs_path (info->uri);
	}


	/* get a new unique ID for the service - use mutex to prevent race conditions */

	res = tracker_exec_proc (db_con->common, "GetNewID", 0);

	if (!res || !res[0] || !res[0][0]) {
		tracker_error ("ERROR: could not create service - GetNewID failed");
		return 0;
	}

	i = atoi (res[0][0]);
	i++;

	sid = tracker_int_to_str (i);
	tracker_exec_proc (db_con->common, "UpdateNewID",1, sid);

	tracker_db_free_result (res);

	if (info->is_directory) {
		str_is_dir = "1";
	} else {
		str_is_dir = "0";
	}

	if (info->is_link) {
		str_is_link = "1";
	} else {
		str_is_link = "0";
	}

	str_filesize = tracker_guint32_to_str (info->file_size);
	str_mtime = tracker_gint32_to_str (info->mtime);
	str_offset = tracker_gint32_to_str (info->offset);

	service_type_id = tracker_get_id_for_service (service);

	if (info->mime)
		tracker_debug ("service id for %s is %d and sid is %s with mime %s", service, service_type_id, sid, info->mime);
	else
		tracker_debug ("service id for %s is %d and sid is %s", service, service_type_id, sid);

	str_service_type_id = tracker_int_to_str (service_type_id);

	str_aux = tracker_int_to_str (info->aux_id);

	if (service_type_id != -1) {
		tracker_exec_proc (db_con, "CreateService", 11, sid, path, name, str_service_type_id, info->mime, str_filesize,
                                   str_is_dir, str_is_link, str_offset, str_mtime, str_aux);

		if (db_con->in_error) {
			tracker_error ("ERROR: CreateService uri is %s/%s", path, name);
			g_free (name);
			g_free (path);
			g_free (str_aux);
			g_free (str_service_type_id);
			g_free (sid);
			g_free (str_filesize);
			g_free (str_mtime);
			g_free (str_offset);
			return 0;
		}
		id = sqlite3_last_insert_rowid (db_con->db);

		if (info->is_hidden) {
			char *sql = g_strdup_printf ("Update services set Enabled = 0 where ID = %d", (int) id);

			tracker_db_exec_no_reply (db_con, sql);

			g_free (sql);
		}

		tracker_exec_proc (db_con->common, "IncStat", 1, service);

		char *parent = tracker_get_parent_service (service);
		
		if (parent) {
			tracker_exec_proc (db_con->common, "IncStat", 1, parent);
			g_free (parent);
		}

	}
	g_free (name);
	g_free (path);
	g_free (str_aux);
	g_free (str_service_type_id);
	g_free (sid);
	g_free (str_filesize);
	g_free (str_mtime);
	g_free (str_offset);

	return id;
}



/*
static void
delete_index_data (gpointer key,
		   gpointer value,
		   gpointer user_data)
{
	char	*word;
	guint32	id;

	word = (char *) key;

	id = GPOINTER_TO_UINT (user_data);
g_hash_table_foreach (table, delete_index_data, GUINT_TO_POINTER (id));
	tracker_indexer_update_word (tracker->file_indexer, word, id, 0, 1, TRUE);
}
*/

static void
delete_index_for_service (DBConnection *db_con, guint32 id)
{
	char	   *str_file_id;

	str_file_id = tracker_uint_to_str (id);

	tracker_exec_proc (db_con->blob, "DeleteAllContents", 1, str_file_id);

	g_free (str_file_id);

	/* disable deletion of words in index for performance reasons - we can filter out deletes when we search
	GHashTable *table;

	table = get_indexable_content_words (db_con, id, table);

	table = get_file_contents_words (blob_db_con, id);

	if (table) {
		g_hash_table_foreach (table, delete_index_data, GUINT_TO_POINTER (id));
		g_hash_table_destroy (table);
	}

	*/

}




/*
static gint
delete_id_from_list (gpointer         key,
		     gpointer         value,
		     gpointer         data)
{
	
  	GSList *list, *l;
	WordDetails *wd;
	guint32 file_id = GPOINTER_TO_UINT (data);

	list = value;

	if (!list) {
		return 1;
	}
	
	for (l=list;l;l=l->next) {
		wd = l->data;
		if (wd->id == file_id) {
			
			list = g_slist_remove (list, l->data);
			g_slice_free (WordDetails, wd);
			value = list;
			tracker->word_detail_count--;

			if (g_slist_length (list) == 0) {
				tracker->word_count--;
			}

			return 1;
		}
	}

  	return 1;
}
*/

/*
static void
delete_cache_words (guint32 file_id)
{
  	g_hash_table_foreach (tracker->cached_table, (GHFunc) delete_id_from_list, GUINT_TO_POINTER (file_id));	
}
*/

static void
dec_stat (DBConnection *db_con, int id)
{
	char *service = tracker_get_service_by_id (id);

	if (service) {
		tracker_exec_proc (db_con->common, "DecStat", 1, service);

		char *parent = tracker_get_parent_service (service);
		
		if (parent) {
			tracker_exec_proc (db_con->common, "DecStat", 1, parent);
			g_free (parent);
		}

		g_free (service);
	
		
	} else {
		tracker_debug ("could not dec stat for service ID %d", id);
	}

}


char *
tracker_db_get_id (DBConnection *db_con, const char *service, const char *uri)
{
	int	service_id;
	guint32	id;

	service_id = tracker_get_id_for_service (service);

	if (service_id == -1) {
		return NULL;
	}

	id = tracker_db_get_file_id (db_con, uri);

	if (id > 0) {
		return tracker_uint_to_str (id);
	}

	return NULL;
}



void
tracker_db_delete_file (DBConnection *db_con, guint32 file_id)
{
	char *str_file_id, *name = NULL, *path;

	delete_index_for_service (db_con, file_id);

	str_file_id = tracker_uint_to_str (file_id);

	char ***res = tracker_exec_proc (db_con, "GetFileByID3", 1, str_file_id);

	if (res) {
		
		if (res[0] && res[0][0] && res[0][1]) {
			name = res[0][0];
			path = res[0][1];
		} else {
			tracker_db_free_result (res);
			g_free (str_file_id);
			return;
		}

		if (res[0] && res[0][3]) {
			dec_stat (db_con, atoi (res[0][3]));
		}

		tracker_exec_proc (db_con, "DeleteService1", 1, str_file_id);
		tracker_exec_proc (db_con->common, "DeleteService6", 2, path, name);
		tracker_exec_proc (db_con->common, "DeleteService7", 2, path, name);
		tracker_exec_proc (db_con->common, "DeleteService9", 2, path, name);

		tracker_db_free_result (res);
	}

	g_free (str_file_id);
}


void
tracker_db_delete_directory (DBConnection *db_con, guint32 file_id, const char *uri)
{
	char ***res;
	char *str_file_id, *uri_prefix;

	str_file_id = tracker_uint_to_str (file_id);

	uri_prefix = g_strconcat (uri, G_DIR_SEPARATOR_S, "*", NULL);

	delete_index_for_service (db_con, file_id);

	/* get all file id's for all files recursively under directory amd delete them */
	res = tracker_exec_proc (db_con, "SelectSubFileIDs", 2, uri, uri_prefix);

	if (res) {
		char **row;
		int  i;

		for (i = 0; (row = tracker_db_get_row (res, i)); i++) {
			if (row[0]) {
				tracker_db_delete_file (db_con, atoi (row[0]));
		
			}


		}

		tracker_db_free_result (res);
	}


	/* delete all files underneath directory 
	tracker_db_start_transaction (db_con);
	tracker_exec_proc (db_con, "DeleteService2", 1, uri);
	tracker_exec_proc (db_con, "DeleteService3", 1, uri_prefix);
	tracker_exec_proc (db_con, "DeleteService4", 1, uri);
	tracker_exec_proc (db_con, "DeleteService5", 1, uri_prefix);
	tracker_exec_proc (db_con, "DeleteService8", 2, uri, uri_prefix);
	tracker_exec_proc (db_con, "DeleteService10", 2, uri, uri_prefix);
	tracker_db_end_transaction (db_con);
	*/

	/* delete directory */
	tracker_db_delete_file (db_con, file_id);


	g_free (uri_prefix);
	g_free (str_file_id);
}

void
tracker_db_delete_service (DBConnection *db_con, guint32 id, const char *uri)
{
	tracker_db_delete_directory (db_con, id, uri);

}



void
tracker_db_update_file (DBConnection *db_con, FileInfo *info)
{
	char *str_file_id;
	char *str_service_type_id;
	char *str_size;
	char *str_mtime;
	char *str_offset;
	char *name, *path;

	str_file_id = tracker_uint_to_str (info->file_id);
	str_service_type_id = tracker_int_to_str (info->service_type_id);
	str_size = tracker_int_to_str (info->file_size);
	str_mtime = tracker_int_to_str (info->mtime);
	str_offset = tracker_int_to_str (info->offset);

	name = g_path_get_basename (info->uri);
	path = g_path_get_dirname (info->uri);

	tracker_exec_proc (db_con->index, "UpdateFile", 8, str_service_type_id, path, name, info->mime, str_size, str_mtime, str_offset, str_file_id);
	
	g_free (str_service_type_id);
	g_free (str_size);
	g_free (str_offset);
	g_free (name);
 	g_free (path);
	g_free (str_file_id);
	g_free (str_mtime);
}


gboolean
tracker_db_has_pending_files (DBConnection *db_con)
{
	char	 ***res;
	gboolean has_pending;

	if (!tracker->is_running) {
		return FALSE;
	}

	has_pending = FALSE;

	res = tracker_exec_proc (db_con->cache, "ExistsPendingFiles", 0);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			int pending_file_count;

			pending_file_count = atoi (row[0]);
			tracker_debug ("%d files are pending with count %s", pending_file_count, row[0]);
			has_pending = (pending_file_count > 0);
		}

		tracker_db_free_result (res);
	}

	return has_pending;
}


gboolean
tracker_db_has_pending_metadata (DBConnection *db_con)
{
	char	 ***res;
	gboolean has_pending;

	if (!tracker->is_running) {
		return FALSE;
	}

	has_pending = FALSE;

	res = tracker_exec_proc (db_con->cache, "CountPendingMetadataFiles", 0);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			int pending_file_count;

			pending_file_count = atoi (row[0]);
			tracker_debug ("metadata queue has %d rows pending", atoi (row[0]));
			has_pending = (pending_file_count > 0);
		}

		tracker_db_free_result (res);
	}

	return has_pending;
}


char ***
tracker_db_get_pending_files (DBConnection *db_con)
{
	time_t time_now;
	char *time_str, *str;

	if (!tracker->is_running) {
		return NULL;
	}

	time (&time_now);

	time_str = tracker_int_to_str (time_now);


	tracker_db_exec_no_reply (db_con->cache, "DELETE FROM FileTemp");
	str = g_strconcat ("INSERT INTO FileTemp (ID, FileID, Action, FileUri, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID) SELECT ID, FileID, Action, FileUri, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM FilePending WHERE (PendingDate < ", time_str, ") AND (Action <> 20) LIMIT 250", NULL);
	tracker_db_exec_no_reply (db_con->cache, str);
	tracker_db_exec_no_reply (db_con->cache, "DELETE FROM FilePending WHERE ID IN (SELECT ID FROM FileTemp)");


	g_free (str);
	g_free (time_str);

	return tracker_exec_sql (db_con->cache, "SELECT FileID, FileUri, Action, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM FileTemp ORDER BY ID");
}


void
tracker_db_remove_pending_files (DBConnection *db_con)
{
	tracker_db_exec_no_reply (db_con->cache, "DELETE FROM FileTemp");
}


char ***
tracker_db_get_pending_metadata (DBConnection *db_con)
{
	const char *str;

	if (!tracker->is_running) {
		return NULL;
	}

	str = "INSERT INTO MetadataTemp (ID, FileID, Action, FileUri, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID) SELECT ID, FileID, Action, FileUri, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM FilePending WHERE Action = 20 LIMIT 250";

	tracker_db_exec_no_reply (db_con->cache, "DELETE FROM MetadataTemp");
	tracker_db_exec_no_reply (db_con->cache, str);
	tracker_db_exec_no_reply (db_con->cache, "DELETE FROM FilePending WHERE ID IN (SELECT ID FROM MetadataTemp)");

	return tracker_exec_sql (db_con->cache, "SELECT FileID, FileUri, Action, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM MetadataTemp ORDER BY ID");
}


unsigned int
tracker_db_get_last_id (DBConnection *db_con)
{
	return sqlite3_last_insert_rowid (db_con->db);
}


void
tracker_db_remove_pending_metadata (DBConnection *db_con)
{
	tracker_db_exec_no_reply (db_con->cache, "DELETE FROM MetadataTemp");
}


void
tracker_db_insert_pending (DBConnection *db_con, const char *id, const char *action, const char *counter, const char *uri, const char *mime, gboolean is_dir, gboolean is_new, int service_type_id)
{
	time_t	   time_now;
	char 	   *time_str;
	int	   i;
	const char *str_new;
	char	   *str_service_type_id;

	time (&time_now);

	i = atoi (counter);

	if (i == 0) {
		time_str = tracker_int_to_str (i);
	} else {
		time_str = tracker_int_to_str (time_now + i);
	}

	if (is_new) {
		str_new = "1";
	} else {
		str_new = "0";
	}

	str_service_type_id = tracker_int_to_str (service_type_id);

	if (is_dir) {
		tracker_exec_proc (db_con->cache, "InsertPendingFile", 10, id, action, time_str, uri, mime, "1", str_new, "1", "1", str_service_type_id);
	} else {
		tracker_exec_proc (db_con->cache, "InsertPendingFile", 10, id, action, time_str, uri, mime, "0", str_new, "1", "1", str_service_type_id);
	}

	g_free (str_service_type_id);
	g_free (time_str);
}


void
tracker_db_update_pending (DBConnection *db_con, const char *counter, const char *action, const char *uri)
{
	time_t  time_now;
	char 	*time_str;
	int	i;

	time (&time_now);

	i = atoi (counter);

	time_str = tracker_int_to_str (time_now + i);

	tracker_exec_proc (db_con->cache, "UpdatePendingFile", 3, time_str, action, uri);

	g_free (time_str);
}


char ***
tracker_db_get_files_by_service (DBConnection *db_con, const char *service, int offset, int limit)
{
	char *str_limit, *str_offset;
	char ***res;

	str_limit = tracker_int_to_str (limit);
	str_offset = tracker_int_to_str (offset);

	res = tracker_exec_proc (db_con, "GetByServiceType", 4, service, service, str_offset, str_limit);

	g_free (str_offset);
	g_free (str_limit);

	return res;
}


char ***
tracker_db_get_files_by_mime (DBConnection *db_con, char **mimes, int n, int offset, int limit, gboolean vfs)
{
	int	i;
	char *service;
	char	***res;
	char	*query;
	GString	*str;

	g_return_val_if_fail (mimes, NULL);

	if (vfs) {
		service = "VFS";
	} else {
		service = "Files";
	}

	str = g_string_new ("SELECT  DISTINCT F.Path || '/' || F.Name AS uri FROM Services F INNER JOIN ServiceKeywordMetaData M ON F.ID = M.ServiceID WHERE M.MetaDataID = (SELECT ID FROM MetaDataTypes WHERE MetaName ='File:Mime') AND (M.MetaDataValue IN ");

	g_string_append_printf (str, "('%s'", mimes[0]);

	for (i = 1; i < n; i++) {
		g_string_append_printf (str, ", '%s'", mimes[i]);

	}

	g_string_append_printf (str, ")) AND (F.ServiceTypeID in (select TypeId from ServiceTypes where TypeName = '%s' or Parent = '%s')) LIMIT %d,%d", service, service, offset, limit);

	query = g_string_free (str, FALSE);

	tracker_debug ("getting files with mimes using sql %s", query);

	res = tracker_exec_sql (db_con, query);

	g_free (query);

	return res;
}


char ***
tracker_db_search_text_mime (DBConnection *db_con, const char *text, char **mime_array, int n)
{
	char 	     **result, **array;
	GSList 	     *hit_list, *result_list;
	const GSList *tmp;
	int 	     count;

	result = NULL;
	result_list = NULL;
	 
	int service_array[8];
	service_array[0] = tracker_get_id_for_service ("Files");
	service_array[1] = tracker_get_id_for_service ("Folders");
	service_array[2] = tracker_get_id_for_service ("Documents");
	service_array[3] = tracker_get_id_for_service ("Images");
	service_array[4] = tracker_get_id_for_service ("Music");
	service_array[5] = tracker_get_id_for_service ("Videos");
	service_array[6] = tracker_get_id_for_service ("Text");
	service_array[7] = tracker_get_id_for_service ("Other");

	SearchQuery *query = tracker_create_query (db_con->word_index, service_array, 8, 0, 999999);

	array = tracker_parse_text_into_array (text);

	char **pstr;

	for (pstr = array; *pstr; pstr++) {
		tracker_add_query_word (query, *pstr, WordNormal);	
	}

	g_strfreev (array);

	if (!tracker_indexer_get_hits (query)) {

		tracker_free_query (query);
		return NULL;

	}

	hit_list = query->hits;

	count = 0;

	for (tmp = hit_list; tmp; tmp = tmp->next) {
		SearchHit *hit;
		char	  *str_id;
		char	  ***res;

		hit = tmp->data;

		str_id = tracker_uint_to_str (hit->service_id);

		res = tracker_exec_proc (db_con, "GetFileByID", 1, str_id);

		g_free (str_id);

		if (res) {
			if (res[0] && res[0][0] && res[0][1] && res[0][2]) {
				int i;

				for (i = 0; i < n; i++) {

					if (strcasecmp (mime_array[i], res[0][2]) == 0) {
						char **row;

						row = g_new (char *, 3);

						row[0] = g_strdup (res[0][0]);
						row[1] = g_strdup (res[0][1]);
						row[2] = NULL;

						tracker_debug ("hit is %s", row[1]);

						result_list = g_slist_prepend (result_list, row);

						count++;

						break;
					}
				}
			}

			tracker_db_free_result (res);
		}

		if (count > 2047) {
			break;
		}
	}

	tracker_free_query (query);

	if (!result_list) {
		return NULL;
	}

	count = g_slist_length (result_list);
	result_list = g_slist_reverse (result_list);

	result = g_new ( char *, count + 1);
	result[count] = NULL;

	count = 0;

	for (tmp = result_list; tmp; tmp = tmp->next) {
		result[count] = (char *) tmp->data;
		count++;
	}

	g_slist_free (result_list);

	return (char ***) result;
}


char ***
tracker_db_search_text_location (DBConnection *db_con, const char *text, const char *location)
{
	char	     *location_prefix;
	char 	     **result, **array;
	GSList 	     *hit_list, *result_list;
	const GSList *tmp;
	int 	     count;

	location_prefix = g_strconcat (location, G_DIR_SEPARATOR_S, NULL);

	 
	int service_array[8];
	service_array[0] = tracker_get_id_for_service ("Files");
	service_array[1] = tracker_get_id_for_service ("Folders");
	service_array[2] = tracker_get_id_for_service ("Documents");
	service_array[3] = tracker_get_id_for_service ("Images");
	service_array[4] = tracker_get_id_for_service ("Music");
	service_array[5] = tracker_get_id_for_service ("Videos");
	service_array[6] = tracker_get_id_for_service ("Text");
	service_array[7] = tracker_get_id_for_service ("Other");

	SearchQuery *query = tracker_create_query (db_con->word_index, service_array, 8, 0, 999999);

	array = tracker_parse_text_into_array (text);

	char **pstr;

	for (pstr = array; *pstr; pstr++) {
		tracker_add_query_word (query, *pstr, WordNormal);	
	}

	g_strfreev (array);

	if (!tracker_indexer_get_hits (query)) {

		tracker_free_query (query);
		return NULL;

	}

	hit_list = query->hits;

	result_list = NULL;

	count = 0;

	for (tmp = hit_list; tmp; tmp = tmp->next) {
		SearchHit *hit;
		char	  *str_id;
		char	  ***res;

		hit = tmp->data;

		str_id = tracker_uint_to_str (hit->service_id);

		res = tracker_exec_proc (db_con, "GetFileByID", 1, str_id);

		g_free (str_id);

		if (res) {
			if (res[0] && res[0][0] && res[0][1]) {

				if (g_str_has_prefix (res[0][0], location_prefix) || (strcmp (res[0][0], location) == 0)) {
					char **row;

					row = g_new (char *, 3);

					row[0] = g_strdup (res[0][0]);
					row[1] = g_strdup (res[0][1]);
					row[2] = NULL;

					//tracker_log ("hit is %s", row[1]);

					result_list = g_slist_prepend (result_list, row);

					count++;
				}
			}

			tracker_db_free_result (res);
		}

		if (count > 2047) {
			break;
		}
	}

	g_free (location_prefix);

	tracker_free_query (query);

	if (!result_list) {
		return NULL;
	}

	count = g_slist_length (result_list);
	result_list = g_slist_reverse (result_list);

	result = g_new (char *, count + 1);
	result[count] = NULL;

	count = 0;

	for (tmp = result_list; tmp; tmp = tmp->next) {
		result[count] = (char *) tmp->data;
		count++;
	}

	g_slist_free (result_list);

	return (char ***) result;
}


char ***
tracker_db_search_text_mime_location (DBConnection *db_con, const char *text, char **mime_array, int n, const char *location)
{
	char	     *location_prefix;
	char 	     **result, **array;
	GSList 	     *hit_list, *result_list;
	const GSList *tmp;
	int	     count;

	location_prefix = g_strconcat (location, G_DIR_SEPARATOR_S, NULL);

		 
	int service_array[8];
	service_array[0] = tracker_get_id_for_service ("Files");
	service_array[1] = tracker_get_id_for_service ("Folders");
	service_array[2] = tracker_get_id_for_service ("Documents");
	service_array[3] = tracker_get_id_for_service ("Images");
	service_array[4] = tracker_get_id_for_service ("Music");
	service_array[5] = tracker_get_id_for_service ("Videos");
	service_array[6] = tracker_get_id_for_service ("Text");
	service_array[7] = tracker_get_id_for_service ("Other");

	SearchQuery *query = tracker_create_query (db_con->word_index, service_array, 8, 0, 999999);

	array = tracker_parse_text_into_array (text);

	char **pstr;

	for (pstr = array; *pstr; pstr++) {
		tracker_add_query_word (query, *pstr, WordNormal);	
	}

	g_strfreev (array);

	if (!tracker_indexer_get_hits (query)) {

		tracker_free_query (query);
		return NULL;

	}

	hit_list = query->hits;

	result_list = NULL;

	count = 0;

	for (tmp = hit_list; tmp; tmp = tmp->next) {
		SearchHit *hit;
		char	  *str_id;
		char	  ***res;

		hit = tmp->data;

		str_id = tracker_uint_to_str (hit->service_id);

		res = tracker_exec_proc (db_con, "GetFileByID", 1, str_id);

		g_free (str_id);

		if (res) {
			if (res[0] && res[0][0] && res[0][1] && res[0][2]) {

				if (g_str_has_prefix (res[0][0], location_prefix) || (strcmp (res[0][0], location) == 0)) {
					int i;

					for (i = 0; i < n; i++) {

						if ((mime_array[i]) && (res[0][2] != 0) && (strcasecmp (mime_array[i], res[0][2]) == 0)) {
							char **row;

							row = g_new (char *, 3);

							row[0] = g_strdup (res[0][0]);
							row[1] = g_strdup (res[0][1]);
							row[2] = NULL;

							//tracker_log ("hit is %s", row[1]);

							result_list = g_slist_prepend (result_list, row);

							count++;

							break;
						}
					}
				}
			}

			tracker_db_free_result (res);
		}

		if (count > 2047) {
			break;
		}
	}

	g_free (location_prefix);

	tracker_free_query (query);

	if (!result_list) {
		return NULL;
	}

	count = g_slist_length (result_list);
	result_list = g_slist_reverse (result_list);

	result = g_new (char *, count + 1);
	result[count] = NULL;

	count = 0;
	for (tmp = result_list; tmp; tmp = tmp->next) {
		result[count] = (char *) tmp->data;
		count++;
	}

	g_slist_free (result_list);

	return (char ***) result;
}


char ***
tracker_db_get_metadata_types (DBConnection *db_con, const char *class, gboolean writeable)
{
	if (strcmp (class, "*") == 0) {
		if (writeable) {
			return tracker_exec_proc (db_con, "GetWriteableMetadataTypes", 0);
		} else {
			return tracker_exec_proc (db_con, "GetMetadataTypes", 0);
		}

	} else {
		if (writeable) {
			return tracker_exec_proc (db_con, "GetWriteableMetadataTypesLike", 1, class);
		} else {
			return tracker_exec_proc (db_con, "GetMetadataTypesLike", 1, class);
		}
	}
}


char ***
tracker_db_get_sub_watches (DBConnection *db_con, const char *dir)
{
	char ***res;
	char *folder;

	folder = g_strconcat (dir, G_DIR_SEPARATOR_S, "*", NULL);

	res = tracker_exec_proc (db_con->cache, "GetSubWatches", 1, folder);

	g_free (folder);

	return res;
}


char ***
tracker_db_delete_sub_watches (DBConnection *db_con, const char *dir)
{
	char *folder;
	char ***res;

	folder = g_strconcat (dir, G_DIR_SEPARATOR_S, "*", NULL);

	res = tracker_exec_proc (db_con->cache, "DeleteSubWatches", 1, folder);

	g_free (folder);

	return res;
}


void
tracker_db_move_file (DBConnection *db_con, const char *moved_from_uri, const char *moved_to_uri)
{

	tracker_debug ("Moving file %s to %s", moved_from_uri, moved_to_uri);

	tracker_db_start_transaction (db_con);

	/* if orig file not in DB, treat it as a create action */
	guint32 id = tracker_db_get_file_id (db_con, moved_from_uri);
	if (id == 0) {
		tracker_debug ("WARNING: original file %s not found in DB", moved_from_uri);
		tracker_db_insert_pending_file (db_con, id, moved_to_uri, "unknown", 0, TRACKER_ACTION_FILE_CREATED, FALSE, TRUE, -1);
		tracker_db_end_transaction (db_con);
		return;
	}

	char *str_file_id = tracker_uint_to_str (id);
	char *name = g_path_get_basename (moved_to_uri);
	char *path = g_path_get_dirname (moved_to_uri);
	char *old_name = g_path_get_basename (moved_from_uri);
	char *old_path = g_path_get_dirname (moved_from_uri);


	/* update db so that fileID reflects new uri */
	tracker_exec_proc (db_con, "UpdateFileMove", 3, path, name, str_file_id);

	/* update File:Path and File:Filename metadata */
	tracker_db_set_single_metadata (db_con, "Files", str_file_id, "File:Path", path, FALSE);
	tracker_db_set_single_metadata (db_con, "Files", str_file_id, "File:Name", name, FALSE);

	char *ext = strrchr (moved_to_uri, '.');
	if (ext) {
		ext++;
		tracker_db_set_single_metadata (db_con, "Files", str_file_id,  "File:Ext", ext, FALSE);
	}

	/* update backup service if necessary */
	tracker_exec_proc (db_con->common, "UpdateBackupService", 4, path, name, old_path, old_name);
	

	tracker_db_end_transaction (db_con);

	tracker_notify_file_data_available ();

	g_free (str_file_id);
	g_free (name);
	g_free (path);
	g_free (old_name);
	g_free (old_path);


}



static char *
str_get_after_prefix (const char *source,
		      const char *delimiter)
{
	char *prefix_start, *str;

	g_return_val_if_fail (source != NULL, NULL);

	if (delimiter == NULL) {
		return g_strdup (source);
	}

	prefix_start = strstr (source, delimiter);

	if (prefix_start == NULL) {
		return NULL;
	}

	str = prefix_start + strlen (delimiter);

	return g_strdup (str);
}




/* update all non-dirs in a dir for a file move */
static void
move_directory_files (DBConnection *db_con, const char *moved_from_uri, const char *moved_to_uri)
{

	/* get all sub files (excluding folders) that were moved and add watches */
	char ***res = tracker_exec_proc (db_con, "SelectFileChildWithoutDirs", 1, moved_from_uri); 

	if (res) {
		char **row;
		int  k;

		k = 0;

		while ((row = tracker_db_get_row (res, k))) {

			k++;

			if (!row || !row[0] || !row[1]) {
				continue;
			}

			char *file_name = g_build_filename (row[0], row[1], NULL);
			char *moved_file_name = g_build_filename (moved_to_uri, row[1], NULL);

			tracker_db_move_file (db_con, file_name, moved_file_name);

			g_free (moved_file_name);
			g_free (file_name);
		}
	
		tracker_db_free_result (res);
		
	}
			
}


static inline void
move_directory (DBConnection *db_con, const char *moved_from_uri, const char *moved_to_uri)
{

	/* stop watching old dir, start watching new dir */
	tracker_remove_watch_dir (moved_from_uri, TRUE, db_con);
		
	tracker_db_move_file (db_con, moved_from_uri, moved_to_uri);
	move_directory_files (db_con, moved_from_uri, moved_to_uri);

	if (tracker_count_watch_dirs () < (int) tracker->watch_limit) {
		tracker_add_watch_dir (moved_to_uri, db_con);
	}
	
}


void
tracker_db_move_directory (DBConnection *db_con, const char *moved_from_uri, const char *moved_to_uri)
{
	char *old_path;
	char ***res;

	old_path = g_strconcat (moved_from_uri, G_DIR_SEPARATOR_S, NULL);

	/* get all sub folders that were moved and add watches */
	res = tracker_db_get_file_subfolders (db_con, moved_from_uri);

	if (res) {
		char **row;
		int  k;

		k = 0;

		while ((row = tracker_db_get_row (res, k))) {

			char *dir_name, *sep, *new_path;
			k++;

			if (!row || !row[0] || !row[1] || !row[2]) {
				continue;
			}

			dir_name = g_build_filename (row[1], row[2], NULL);

			sep = str_get_after_prefix (dir_name, old_path);

			if (!sep) {
				g_free (dir_name);
				continue;
			}

			new_path = g_build_filename (moved_to_uri, sep, NULL);
			g_free (sep);

			tracker_info ("moving subfolder %s to %s", dir_name, new_path);
			
			move_directory (db_con, dir_name, new_path);

			g_usleep (1000);
						
			g_free (new_path);
			g_free (dir_name);

		}

		tracker_db_free_result (res);
	}

	move_directory (db_con, moved_from_uri, moved_to_uri);	
	
	g_free (old_path);

}


char ***
tracker_db_get_file_subfolders (DBConnection *db_con, const char *uri)
{
	char ***res;
	char *folder;

	folder = g_strconcat (uri, G_DIR_SEPARATOR_S, "*", NULL);

	res = tracker_exec_proc (db_con, "SelectFileSubFolders", 2, uri, folder);

	g_free (folder);

	return res;
}






static inline guint8
get_service_type_from_detail (WordDetails *details)
{
	return (details->amalgamated >> 24) & 0xFF;
}

static inline guint16
get_score_from_detail (WordDetails *details)
{
	unsigned char a[2];

	a[0] = (details->amalgamated >> 16) & 0xFF;
	a[1] = (details->amalgamated >> 8) & 0xFF;

	return (a[0] << 8) | (a[1]);
	
}


static void
append_index_data (gpointer key,
		   gpointer value,
		   gpointer user_data)
{
	char		*word;
	int		score;
	ServiceTypeInfo	*info;

	word = (char *) key;
	score = GPOINTER_TO_INT (value);
	info = user_data;

	if (score != 0) {
		/* cache word update */
		tracker_cache_add (word, info->service_id, info->service_type_id, score, TRUE);
	}


}


static void
update_index_data (gpointer key,
		   gpointer value,
		   gpointer user_data)
{
	char		*word;
	int		score;
	ServiceTypeInfo	*info;

	word = (char *) key;
	score = GPOINTER_TO_INT (value);
	info = user_data;

	if (score == 0) return;

	//tracker_debug ("updating index for word %s with score %d", word, score);
	
	tracker_cache_add (word, info->service_id, info->service_type_id, score, FALSE);

}


void
tracker_db_update_indexes_for_new_service (guint32 service_id, int service_type_id, GHashTable *table)
{
	
	if (table) {
		ServiceTypeInfo *info;

		info = g_slice_new (ServiceTypeInfo);

		info->service_id = service_id;
		info->service_type_id = service_type_id;
		info->db_con = NULL;
	
		g_hash_table_foreach (table, append_index_data, info);
		g_slice_free (ServiceTypeInfo, info);
	}
}





static void
cmp_data (gpointer key,
	  gpointer value,
	  gpointer user_data)
{
	char	   *word;
	int	   score;
	GHashTable *new_table;

	gpointer k=0,v=0;

	word = (char *) key;
	score = GPOINTER_TO_INT (value);
	new_table = user_data;

	if (!g_hash_table_lookup_extended (new_table, word, &k, &v)) {
		g_hash_table_insert (new_table, g_strdup (word), GINT_TO_POINTER (0 - score));
	} else {
		g_hash_table_insert (new_table, (char *) word, GINT_TO_POINTER (GPOINTER_TO_INT (v) - score));
	}
}


void
tracker_db_update_differential_index (DBConnection *db_con, GHashTable *old_table, GHashTable *new_table, const char *id, int service_type_id)
{
	ServiceTypeInfo *info;

	g_return_if_fail (id || service_type_id > -1);

	if (!new_table) {
		new_table = g_hash_table_new (g_str_hash, g_str_equal);
	}

	/* calculate the differential word scores between old and new data*/
	if (old_table) {
		g_hash_table_foreach (old_table, cmp_data, new_table);
	}

	info = g_new (ServiceTypeInfo, 1);

	info->service_id = strtoul (id, NULL, 10);
	info->service_type_id = service_type_id;
	info->db_con = db_con;

	g_hash_table_foreach (new_table, update_index_data, info);

	g_free (info);
}


char ***
tracker_db_get_keyword_list (DBConnection *db_con, const char *service)
{

	tracker_debug (service);
	char ***res = tracker_exec_proc (db_con, "GetKeywordList", 2, service, service);

	return res;
}


/* get static data like metadata field definitions and services definitions and load them into hashtables */
void
tracker_db_get_static_data (DBConnection *db_con)
{
	int i = 0, j;
	char ***res;


	/* get static metadata info */
	res  = tracker_exec_proc (db_con, "GetMetadataTypes", 0);


	if (res) {
		char **row;

		while ((row = tracker_db_get_row (res, i))) {

			i++;

			if (row[0] && row[1] && row[2] && row[3] && row[4] && row[5] && row[6] && row[7] && row[8] && row[9]) {
	
				FieldDef *def = NULL;				
	
				def = g_new (FieldDef, 1);
				def->id = g_strdup (row[0]);
				def->type = atoi (row[2]);
				def->field_name = g_strdup (row[3]);
				def->weight = atoi (row[4]);
				def->embedded = (row[5][0] == '1');
				def->multiple_values = (row[6][0] == '1');
				def->delimited = (row[7][0] == '1');
				def->filtered = (row[8][0] == '1');
				def->store_metadata = (row[9][0] == '1');
				

				def->child_ids = NULL;
				

				j=0;
				char ***res2 = tracker_exec_proc (db_con, "GetMetadataAliases", 1, def->id);

				if (res2) {
					char **row2;

					while ((row2 = tracker_db_get_row (res2, j))) {
				
						j++;

						if (row2[1]) {
							def->child_ids = g_slist_prepend (def->child_ids, g_strdup (row[1]));
						}
					}
					tracker_db_free_result (res2);	
				}

				g_hash_table_insert (tracker->metadata_table, g_utf8_strdown  (row[1], -1), def);
				tracker_debug ("loading metadata def %s with weight %d", def->field_name, def->weight);

			} 

		}		
		tracker_db_free_result (res);
	}


	/* get static service info */	
	
	res  = tracker_exec_proc_ignore_nulls (db_con, "GetAllServices", 0);
	
	if (res) {
		char **row;
		i = 0;

		 tracker->email_service_min = 0;
		 tracker->email_service_max = 0;

		while ((row = tracker_db_get_row (res, i))) {

			i++;

			if (row[0] && row[1] && row[2] && row[3] && row[4] && row[5] && row[6] && row[7] && row[8]) {
				ServiceDef *def = g_new0 (ServiceDef, 1);

				def->id = atoi (row[0]);
				def->name = g_strdup (row[1]);
				def->parent = g_strdup (row[2]);
				def->enabled = (row[3][0] == '1');
				def->embedded = (row[4][0] == '1');
				def->has_metadata = (row[5][0] == '1');
				def->has_fulltext = (row[6][0] == '1');
				def->has_thumbs = (row[7][0] == '1');

				def->content_metadata = NULL;
				if (row[8][1]) {
					def->content_metadata = g_strdup (row[8]);
				}

				if (g_str_has_prefix (def->name, "Email") || g_str_has_suffix (def->name, "Emails")) {
					def->database = DB_EMAIL;

					if (tracker->email_service_min == 0 || def->id < tracker->email_service_min) {
						tracker->email_service_min = def->id;
					}

					if (tracker->email_service_max == 0 || def->id > tracker->email_service_max) {
						tracker->email_service_max = def->id;
					}



				} else {
					def->database = DB_DATA;
				}

				def->show_service_files = (row[10][0] == '1');
			 	def->show_service_directories = (row[11][0] == '1');
				
				def->key_metadata = NULL;

				int j;

				for (j=12; j<23; j++) {
					if (row[j] && row[j][1]) {
						def->key_metadata = g_slist_prepend (def->key_metadata, g_strdup (row[j]));
					}
				}

				/* hack to prevent db change late in the cycle */
				if (strcmp (def->name, "Applications") == 0) {
					def->key_metadata = g_slist_prepend (def->key_metadata, g_strdup ("App:DisplayName"));
					def->key_metadata = g_slist_prepend (def->key_metadata, g_strdup ("App:Exec"));
					def->key_metadata = g_slist_prepend (def->key_metadata, g_strdup ("App:Icon"));
				}


				def->key_metadata = g_slist_reverse (def->key_metadata);

				tracker_debug ("adding service definition for %s with id %s", def->name, row[0]);
				g_hash_table_insert (tracker->service_table, g_utf8_strdown (def->name, -1), def);
				g_hash_table_insert (tracker->service_id_table, g_strdup (row[0]), def);
			} 

		}		
		tracker_db_free_result (res);
	}

}

DBConnection *
tracker_db_get_service_connection (DBConnection *db_con, const char *service)
{
	DBTypes type;

	type = tracker_get_db_for_service (service);

	if (type == DB_EMAIL) {
		return db_con->emails;
	}

	return db_con;
}


char *
tracker_db_get_service_for_entity (DBConnection *db_con, const char *id)
{
	char ***res;
	char *result = NULL;

	res  = tracker_exec_proc (db_con, "GetFileByID2", 1, id);
	
	if (res) {
		if (res[0][1]) {
			
			result = g_strdup (res[0][1]);
		}

		tracker_db_free_result (res);

	}

	return result;
	
}


gboolean
tracker_db_metadata_is_child (DBConnection *db_con, const char *child, const char *parent)
{
	FieldDef *def_child, *def_parent;

	def_child = tracker_db_get_field_def (db_con, child);

	if (!def_child) {
		return FALSE;
	}


	def_parent = tracker_db_get_field_def (db_con, parent);

	if (!def_parent) {
		return FALSE;
	}

	GSList *tmp;

	for (tmp = def_parent->child_ids; tmp; tmp = tmp->next) {
		
		if (!tmp->data) return FALSE;

		if (strcmp (def_child->id, tmp->data) == 0) {
			return TRUE;
		}
	}

	return FALSE;

}



gboolean
tracker_db_load_service_file (DBConnection *db_con, const char *filename, gboolean full_path)
{
	GKeyFile 		*key_file = NULL;
	const char * const 	*locale_array;
	char 			*service_file, *sql;
	gboolean		is_metadata = FALSE, is_service = FALSE, is_extractor = FALSE;
	int			id;

	char *DataTypeArray[11] = {"Keyword", "Indexable", "CLOB", "String", "Integer", "Double", "DateTime", "BLOB", "Struct", "Link", NULL};

	if (!full_path) {
		service_file = g_build_filename (tracker->services_dir, filename, NULL);
	} else {
		service_file = g_strdup (filename);
	}


	locale_array = g_get_language_names ();

	key_file = g_key_file_new ();

	if (g_key_file_load_from_file (key_file, service_file, G_KEY_FILE_NONE, NULL)) {
		
		if (g_str_has_suffix (filename, ".metadata")) {
			is_metadata = TRUE;
		} else if (g_str_has_suffix (filename, ".service")) {
			is_service = TRUE;
		} else if (g_str_has_suffix (filename, ".extractor")) {
			is_extractor = TRUE;
		} else {
			g_key_file_free (key_file);
			g_free (service_file);		
			return FALSE;
		} 


		char **groups = g_key_file_get_groups (key_file, NULL);
		char **array;

		for (array = groups; *array; array++) {

			if (is_metadata) {
				FieldDef *def = tracker_db_get_field_def (db_con, *array);

				if (!def) {
					tracker_exec_proc (db_con, "InsertMetadataType", 1, *array);			
					id = sqlite3_last_insert_rowid (db_con->db);		
				} else {
					id = atoi (def->id);
				}

			} else if (is_service) {
				
				char *name = g_utf8_strdown (*array, -1);

				ServiceDef *def =  g_hash_table_lookup (tracker->service_table, *array);

				g_free (name);

				if (!def) {
					tracker_exec_proc (db_con, "InsertServiceType", 1, *array);	
					id = sqlite3_last_insert_rowid (db_con->db);		
				} else {
					id = def->id;
				}

				
			} else {
				/* TODO add support for extractors here */;
			}
		
			/* get inserted ID */
			
			char *str_id = tracker_uint_to_str (id);

			char **keys = g_key_file_get_keys (key_file, *array, NULL, NULL);
			char **array2;
	
			for (array2 = keys; *array2; array2++) {
	
				char *value = g_key_file_get_locale_string (key_file, *array, *array2, locale_array[0], NULL);

				if (value) {

					if (strcasecmp (value, "true") == 0) {

						g_free (value);
						value = g_strdup ("1");

					} else if  (strcasecmp (value, "false") == 0) {

						g_free (value);
						value = g_strdup ("0");
					}

					if (is_metadata) {

						if (strcasecmp (*array2, "Parent") == 0) {

							tracker_exec_proc (db_con, "InsertMetaDataChildren", 2, str_id, value);		

						} else if (strcasecmp (*array2, "DataType") == 0) {

							int data_id = tracker_str_in_array (value, DataTypeArray);

							if (data_id != -1) {
								sql = g_strdup_printf ("update MetaDataTypes set DataTypeID = %d where ID = %s", data_id, str_id);
								tracker_db_exec_no_reply (db_con, sql);
								g_free (sql);
								
							}
						

						} else {
							char *esc_value = tracker_escape_string (value);

							sql = g_strdup_printf ("update MetaDataTypes set  %s = '%s' where ID = %s", *array2, esc_value, str_id);
								
							tracker_db_exec_no_reply (db_con, sql);
							g_free (sql);
							g_free (esc_value);
						}
	
					} else 	if (is_service) {

						if (strcasecmp (*array2, "TabularMetadata") == 0) {

							char **tab_array = g_key_file_get_string_list (key_file, *array, *array2, NULL, NULL);

							char **tmp;
							for (tmp = tab_array; *tmp; tmp++) { 			

								tracker_exec_proc (db_con, "InsertServiceTabularMetadata", 2, str_id, *tmp);		
								
							}

							g_strfreev (tab_array);



						} else if (strcasecmp (*array2, "TileMetadata") == 0) {

							char **tab_array = g_key_file_get_string_list (key_file, *array, *array2, NULL, NULL);

							char **tmp;
							for (tmp = tab_array; *tmp; tmp++) { 			

								tracker_exec_proc (db_con, "InsertServiceTileMetadata", 2, str_id, *tmp);		
							}

							g_strfreev (tab_array);

						} else if (strcasecmp (*array2, "Mimes") == 0) {

							char **tab_array = g_key_file_get_string_list (key_file, *array, *array2, NULL, NULL);

							char **tmp;
							for (tmp = tab_array; *tmp; tmp++) { 			
								tracker_exec_proc (db_con, "InsertMimes", 1, *tmp);		
							
								sql = g_strdup_printf ("update FileMimes set ServiceTypeID = %s where Mime = '%s'", str_id, *tmp);
								tracker_db_exec_no_reply (db_con, sql);
								g_free (sql);
							}

							g_strfreev (tab_array);

						} else if (strcasecmp (*array2, "MimePrefixes") == 0) {

							char **tab_array = g_key_file_get_string_list (key_file, *array, *array2, NULL, NULL);

							char **tmp;
							for (tmp = tab_array; *tmp; tmp++) { 			
								tracker_exec_proc (db_con, "InsertMimePrefixes", 1, *tmp);		
							
								sql = g_strdup_printf ("update FileMimePrefixes set ServiceTypeID = %s where MimePrefix = '%s'", str_id, *tmp);
								tracker_db_exec_no_reply (db_con, sql);
								g_free (sql);
							}

							g_strfreev (tab_array);


						} else {
							char *esc_value = tracker_escape_string (value);
							sql = g_strdup_printf ("update ServiceTypes set  %s = '%s' where TypeID = %s", *array2, esc_value, str_id);
							tracker_db_exec_no_reply (db_con, sql);
							g_free (sql);
							g_free (esc_value);
						}
	
					} else {
						/* to do - support extractors here */ ;
					}

					g_free (value);
					
				}
			}

			if (keys) {
				g_strfreev (keys);
			}

			g_free (str_id);

		}


		if (groups) {
			g_strfreev (groups);
		}
			

		g_key_file_free (key_file);

	} else {
		g_key_file_free (key_file);
		g_free (service_file);		
		return FALSE;
	}


		       
	g_free (service_file);		

	return TRUE;
}


FieldData *
tracker_db_get_metadata_field (DBConnection *db_con, const char *service, const char *field_name, int field_count, gboolean is_select, gboolean is_condition)
{
	FieldData    *field_data;

	field_data = NULL;

	FieldDef *def;

	field_data = g_new0 (FieldData, 1);

	field_data->is_select = is_select;
	field_data->is_condition = is_condition;
	field_data->field_name = g_strdup (field_name);

	def = tracker_db_get_field_def (db_con, field_name);

	if (def) {
	
		field_data->table_name = tracker_get_metadata_table (def->type);
		field_data->alias = g_strdup_printf ("M%d", field_count);
		field_data->data_type = def->type;
		field_data->id_field = g_strdup (def->id);
		field_data->multiple_values = def->multiple_values;
			
		char *my_field = tracker_db_get_field_name (service, field_name);

		if (my_field) {
			field_data->select_field = g_strdup_printf (" S.%s ", my_field);
			g_free (my_field);
			field_data->needs_join = FALSE;	
		} else {
			char *disp_field = tracker_db_get_display_field (def);
			field_data->select_field = g_strdup_printf ("M%d.%s", field_count, disp_field);
			g_free (disp_field);
			field_data->needs_join = TRUE;
		}
			
		if (def->type == DATA_DOUBLE) {
			field_data->where_field = g_strdup_printf ("M%d.MetaDataDisplay", field_count);
		} else {
			field_data->where_field = g_strdup_printf ("M%d.MetaDataValue", field_count);
		}

			
		tracker_db_free_field_def (def);

	} else {
		g_free (field_data);
		return NULL;
	}


	return field_data;
}

char *
tracker_db_get_option_string (DBConnection *db_con, const char *option)
{

	char *value;

	gchar ***res = tracker_exec_proc (db_con, "GetOption", 1, option);

	if (res) {
		if (res[0] && res[0][0]) {
			value = g_strdup (res[0][0]);
		}
		tracker_db_free_result (res);
	}

	return value;
}


void
tracker_db_set_option_string (DBConnection *db_con, const char *option, const char *value)
{
	tracker_exec_proc (db_con, "SetOption", 2, value, option);
}


int
tracker_db_get_option_int (DBConnection *db_con, const char *option)
{

	int value;

	gchar ***res = tracker_exec_proc (db_con, "GetOption", 1, option);

	if (res) {
		if (res[0] && res[0][0]) {
			value = atoi (res[0][0]);
		}
		tracker_db_free_result (res);
	}

	return value;
}


void
tracker_db_set_option_int (DBConnection *db_con, const char *option, int value)
{
	char *str_value = tracker_int_to_str (value);

	tracker_exec_proc (db_con, "SetOption", 2, str_value, option);
		
	g_free (str_value);
}


gboolean
tracker_db_regulate_transactions (DBConnection *db_con, int interval)
{
	tracker->index_count++;
			
	if ((tracker->index_count == 1 || tracker->index_count == interval  || (tracker->index_count >= interval && tracker->index_count % interval == 0))) {
			
		if (tracker->index_count > 1) {
			tracker_db_end_index_transaction (db_con);
			tracker_db_start_index_transaction (db_con);
			tracker_log ("Current memory usage is %d, word count %d and hits %d", tracker_get_memory_usage (), tracker->word_count, tracker->word_detail_count);
		}

		return TRUE;
			
	}

	return FALSE;

}

