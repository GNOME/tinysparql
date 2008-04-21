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

#include "config.h"

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
#include <regex.h>
#include <zlib.h>

#ifdef OS_WIN32
#include "mingw-compat.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-config.h>

#include "tracker-db-interface-sqlite.h"
#include "tracker-db-sqlite.h"
#include "tracker-indexer.h"
#include "tracker-cache.h"
#include "tracker-metadata.h"
#include "tracker-utils.h"
#include "tracker-watch.h"
#include "tracker-service-manager.h"
#include "tracker-query-tree.h"

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
utf8_collation_func (gchar *str1, gint len1, gchar *str2, int len2)
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
static GValue
function_date_to_str (TrackerDBInterface *interface,
		      gint                argc,
		      GValue              values[])
{
	GValue result = { 0, };
	gchar *str;

	str = tracker_date_to_str (g_value_get_double (&values[0]));
	g_value_init (&result, G_TYPE_STRING);
	g_value_take_string (&result, str);

	return result;
}

static GValue
function_regexp (TrackerDBInterface *interface,
		 gint                argc,
		 GValue              values[])
{
	GValue result = { 0, };
	regex_t	regex;
	int	ret;

	if (argc != 2) {
		g_critical ("Invalid argument count");
		return result;
	}

	ret = regcomp (&regex,
		       g_value_get_string (&values[0]),
		       REG_EXTENDED | REG_NOSUB);

	if (ret != 0) {
		g_critical ("Error compiling regular expression");
		return result;
	}

	ret = regexec (&regex,
		       g_value_get_string (&values[1]),
		       0, NULL, 0);

	g_value_init (&result, G_TYPE_INT);
	g_value_set_int (&result, (ret == REG_NOMATCH) ? 0 : 1);
	regfree (&regex);

	return result;
}

/* unzips data */
static GValue
function_uncompress (TrackerDBInterface *interface,
		     gint                argc,
		     GValue              values[])
{
	GByteArray *array;
	GValue result = { 0, };
	gchar *output;
	gint len;

	array = g_value_get_boxed (&values[0]);

	if (!array) {
		return result;
	}

	output = tracker_uncompress ((const gchar *) array->data, array->len, &len);

	if (!output) {
		g_warning ("Uncompress failed");
		return result;
	}

	g_value_init (&result, G_TYPE_STRING);
	g_value_take_string (&result, output);

	return result;
}

static GValue
function_get_service_name (TrackerDBInterface *interface,
			   gint                argc,
			   GValue              values[])
{
	GValue result = { 0, };
	gchar *str;

	str = tracker_service_manager_get_service_by_id (g_value_get_int (&values[0]));
	g_value_init (&result, G_TYPE_STRING);
	g_value_take_string (&result, str);

	return result;
}

static GValue
function_get_service_type (TrackerDBInterface *interface,
			   gint                argc,
			   GValue              values[])
{
	GValue result = { 0, };
	gint id;

	id = tracker_service_manager_get_id_for_service (g_value_get_string (&values[0]));
	g_value_init (&result, G_TYPE_INT);
	g_value_set_int (&result, id);

	return result;
}

static GValue
function_get_max_service_type (TrackerDBInterface *interface,
			       gint                argc,
			       GValue              values[])
{
	GValue result = { 0, };
	gint id;

	id = tracker_service_manager_get_id_for_service (g_value_get_string (&values[0]));
	g_value_init (&result, G_TYPE_INT);
	g_value_set_int (&result, id);

	return result;
}

static void
load_sql_file (DBConnection *db_con, const char *sql_file)
{
	char *filename, *query;
	
	filename = g_build_filename (SHAREDIR, "tracker", sql_file, NULL);

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
	
	filename = g_build_filename (SHAREDIR, "tracker", sql_file, NULL);

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
tracker_db_initialize (void)
{
	FILE	 *file;
	char	 *sql_file;
	GTimeVal *tv;
	int i = 0;

	prepared_queries = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	tracker_log ("Loading prepared queries...");

	sql_file = g_build_filename (SHAREDIR, "tracker", "sqlite-stored-procs.sql", NULL);

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
			const char *query, *name;

			*sep = '\0';

			query = sep + 1;
			name = buffer;

			//tracker_log ("installing query %s with sql %s", name, query);
			g_hash_table_insert (prepared_queries, g_strdup (name), g_strdup (query));
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
tracker_db_finalize (void)
{
}

static inline void
close_db (DBConnection *db_con)
{
	if (db_con->db) {
		g_object_unref (db_con->db);
		db_con->db = NULL;
	}

	tracker_debug ("Database closed for thread %s", db_con->thread);
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


static TrackerDBInterface *
open_user_db (const char *name, gboolean *create_table)
{
	TrackerDBInterface *db;
	gboolean db_exists;
	char *dbname;

	dbname = g_build_filename (tracker->user_data_dir, name, NULL);
	db_exists = g_file_test (dbname, G_FILE_TEST_IS_REGULAR);

	if (!db_exists) {
		tracker_log ("database file %s is not present - will create", dbname);
	}

	if (create_table) {
		*create_table = db_exists;
	}

	db = tracker_db_interface_sqlite_new (dbname);
	tracker_db_interface_set_procedure_table (db, prepared_queries);
	g_free (dbname);

	return db;
}



static TrackerDBInterface *
open_db (const char *name, gboolean *create_table)
{
	TrackerDBInterface *db;
	gboolean db_exists;
	char *dbname;

	dbname = g_build_filename (tracker->data_dir, name, NULL);
	db_exists = g_file_test (dbname, G_FILE_TEST_IS_REGULAR);

	if (!db_exists) {
		tracker_log ("database file %s is not present - will create", dbname);
	}

	if (create_table) {
		*create_table = db_exists;
	}

	db = tracker_db_interface_sqlite_new (dbname);
	tracker_db_interface_set_procedure_table (db, prepared_queries);
	g_free (dbname);

	return db;

}


static void
set_params (DBConnection *db_con, int cache_size, gboolean add_functions)
{
	tracker_db_set_default_pragmas (db_con);

	tracker_db_exec_no_reply (db_con, "PRAGMA page_size = %d", 4096);

	if (tracker_config_get_low_memory_mode (tracker->config)) {
		cache_size /= 2;
	}

	tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = %d", cache_size);

	if (add_functions) {
		if (! tracker_db_interface_sqlite_set_collation_function (TRACKER_DB_INTERFACE_SQLITE (db_con->db),
									  "UTF8", utf8_collation_func)) {
			tracker_error ("ERROR: collation sequence failed");
		}

		/* create user defined functions that can be used in sql */
		tracker_db_interface_sqlite_create_function (db_con->db, "FormatDate", function_date_to_str, 1);
		tracker_db_interface_sqlite_create_function (db_con->db, "GetServiceName", function_get_service_name, 1);
		tracker_db_interface_sqlite_create_function (db_con->db, "GetServiceTypeID", function_get_service_type, 1);
		tracker_db_interface_sqlite_create_function (db_con->db, "GetMaxServiceTypeID", function_get_max_service_type, 1);
		tracker_db_interface_sqlite_create_function (db_con->db, "REGEXP", function_regexp, 2);
	}
}



static void
open_common_db (DBConnection *db_con)
{
	gboolean create;

	db_con->db = open_user_db ("common.db", &create);

	set_params (db_con, 32, FALSE);

}


DBConnection *
tracker_db_connect_common (void)
{

	DBConnection *db_con;

	db_con = g_new0 (DBConnection, 1);

	open_common_db (db_con);

	db_con->db_type = TRACKER_DB_TYPE_COMMON;
	
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
	gchar *path;

	if (strcmp (name, "common") == 0) {
		path = g_build_filename (tracker->user_data_dir, "common.db", NULL);
		tracker_db_exec_no_reply (db_con, "ATTACH '%s' as %s", path, name);
	} else if (strcmp (name, "cache") == 0) {
		path = g_build_filename (tracker->sys_tmp_root_dir, "cache.db", NULL);
		tracker_db_exec_no_reply (db_con, "ATTACH '%s' as %s", path, name);
	}

	g_free (path);
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
	DBConnection *email_db_con = db_con->emails;

	tracker_db_interface_start_transaction (db_con->common->db);

	/* files */
	tracker_db_interface_start_transaction (db_con->db);
	tracker_db_interface_start_transaction (db_con->blob->db);

	/* emails */
	tracker_db_interface_start_transaction (email_db_con->db);
	tracker_db_interface_start_transaction (email_db_con->blob->db);
}



void
tracker_db_end_index_transaction (DBConnection *db_con)
{
	DBConnection *email_db_con = db_con->emails;

	tracker_db_interface_end_transaction (db_con->common->db);

	/* files */
	tracker_db_interface_end_transaction (db_con->db);
	tracker_db_interface_end_transaction (db_con->blob->db);

	/* emails */
	tracker_db_interface_end_transaction (email_db_con->db);
	tracker_db_interface_end_transaction (email_db_con->blob->db);
}


void
tracker_db_set_default_pragmas (DBConnection *db_con)
{
	tracker_db_exec_no_reply (db_con, "PRAGMA synchronous = NORMAL;");

	tracker_db_exec_no_reply (db_con, "PRAGMA count_changes = 0;");

	tracker_db_exec_no_reply (db_con, "PRAGMA temp_store = FILE;");

	tracker_db_exec_no_reply (db_con, "PRAGMA encoding = \"UTF-8\"");

	tracker_db_exec_no_reply (db_con, "PRAGMA auto_vacuum = 0;");

}


DBConnection *
tracker_db_connect (void)
{
	DBConnection *db_con;
	gboolean create_table = FALSE;
	char *dbname;

	dbname = g_build_filename (tracker->data_dir, "file-meta.db", NULL);

	if (!g_file_test (dbname, G_FILE_TEST_IS_REGULAR)) {
		tracker_log ("database file %s is not present - will create", dbname);
		create_table = TRUE;
	}

	db_con = g_new0 (DBConnection, 1);
	db_con->db = tracker_db_interface_sqlite_new (dbname);
	tracker_db_interface_set_procedure_table (db_con->db, prepared_queries);
	g_free (dbname);

	db_con->db_type = TRACKER_DB_TYPE_DATA;
	db_con->db_category = DB_CATEGORY_FILES;

	db_con->data = db_con;

	tracker_db_set_default_pragmas (db_con);
	
	if (!tracker_config_get_low_memory_mode (tracker->config)) {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = %d", 32);
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = %d", 16);
	}

	/* create user defined utf-8 collation sequence */
	if (! tracker_db_interface_sqlite_set_collation_function (TRACKER_DB_INTERFACE_SQLITE (db_con->db),
								  "UTF8", utf8_collation_func)) {
		tracker_error ("ERROR: collation sequence failed");
	}

	/* create user defined functions that can be used in sql */
	tracker_db_interface_sqlite_create_function (db_con->db, "FormatDate", function_date_to_str, 1);
	tracker_db_interface_sqlite_create_function (db_con->db, "GetServiceName", function_get_service_name, 1);
	tracker_db_interface_sqlite_create_function (db_con->db, "GetServiceTypeID", function_get_service_type, 1);
	tracker_db_interface_sqlite_create_function (db_con->db, "GetMaxServiceTypeID", function_get_max_service_type, 1);
	tracker_db_interface_sqlite_create_function (db_con->db, "REGEXP", function_regexp, 2);

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
	tracker_db_attach_db (db_con, "cache");

	db_con->cache = db_con;
	db_con->common = db_con;

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

	set_params (db_con, 512, TRUE);
}

DBConnection *
tracker_db_connect_file_meta (void)
{
	DBConnection *db_con;

	db_con = g_new0 (DBConnection, 1);

	db_con->db_type = TRACKER_DB_TYPE_INDEX;
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

	set_params (db_con, 512, TRUE);
}

DBConnection *
tracker_db_connect_email_meta (void)
{
	DBConnection *db_con;

	db_con = g_new0 (DBConnection, 1);

	db_con->db_type = TRACKER_DB_TYPE_INDEX;
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

	set_params (db_con, 1024, FALSE);

	if (create) {
		tracker_db_exec_no_reply (db_con, "CREATE TABLE ServiceContents (ServiceID Int not null, MetadataID Int not null, Content Text, primary key (ServiceID, MetadataID))");
		tracker_log ("creating file content table");
	}

	tracker_db_interface_sqlite_create_function (db_con->db, "uncompress", function_uncompress, 1);
}

DBConnection *
tracker_db_connect_file_content (void)
{
	DBConnection *db_con;

	db_con = g_new0 (DBConnection, 1);

	db_con->db_type = TRACKER_DB_TYPE_CONTENT;
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

	set_params (db_con, 512, FALSE);

	if (create) {
		tracker_db_exec_no_reply (db_con, "CREATE TABLE ServiceContents (ServiceID Int not null, MetadataID Int not null, Content Text, primary key (ServiceID, MetadataID))");
		tracker_log ("creating email content table");
	}

	tracker_db_interface_sqlite_create_function (db_con->db, "uncompress", function_uncompress, 1);
}

DBConnection *
tracker_db_connect_email_content (void)
{
	DBConnection *db_con;

	db_con = g_new0 (DBConnection, 1);

	db_con->db_type = TRACKER_DB_TYPE_CONTENT;
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

	if (cache && tracker_db_interface_end_transaction (cache->db)) {
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
		tracker_db_interface_start_transaction (cache->db);
	}


}

void
tracker_db_refresh_email (DBConnection *db_con)
{
	gboolean cache_trans = FALSE;
	DBConnection *cache = db_con->cache;

	if (cache && tracker_db_interface_end_transaction (cache->db)) {
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
		tracker_db_interface_start_transaction (cache->db);
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
	db_con->db = tracker_db_interface_sqlite_new (dbname);
	tracker_db_interface_set_procedure_table (db_con->db, prepared_queries);
	g_free (dbname);

	db_con->db_type = TRACKER_DB_TYPE_CACHE;
	db_con->cache = db_con;

	tracker_db_set_default_pragmas (db_con);

	if (!tracker_config_get_low_memory_mode (tracker->config)) {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = %d", 128);
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = %d", 32);
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
	db_con->db = tracker_db_interface_sqlite_new (dbname);
	tracker_db_interface_set_procedure_table (db_con->db, prepared_queries);
	g_free (dbname);


	db_con->db_type = TRACKER_DB_TYPE_EMAIL;
	db_con->db_category = DB_CATEGORY_EMAILS;

	db_con->emails = db_con;

	tracker_db_exec_no_reply (db_con, "PRAGMA page_size = %d", 4096);

	tracker_db_set_default_pragmas (db_con);

	tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = %d", 8);

	/* create user defined utf-8 collation sequence */
	if (! tracker_db_interface_sqlite_set_collation_function (TRACKER_DB_INTERFACE_SQLITE (db_con->db),
								  "UTF8", utf8_collation_func)) {
		tracker_error ("ERROR: collation sequence failed");
	}

	/* create user defined functions that can be used in sql */
	tracker_db_interface_sqlite_create_function (db_con->db, "FormatDate", function_date_to_str, 1);
	tracker_db_interface_sqlite_create_function (db_con->db, "GetServiceName", function_get_service_name, 1);
	tracker_db_interface_sqlite_create_function (db_con->db, "GetServiceTypeID", function_get_service_type, 1);
	tracker_db_interface_sqlite_create_function (db_con->db, "GetMaxServiceTypeID", function_get_max_service_type, 1);
	tracker_db_interface_sqlite_create_function (db_con->db, "REGEXP", function_regexp, 2);

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
	gchar *parent;
	gint   id;

        id = tracker_service_manager_get_id_for_parent_service (service);
        parent = tracker_service_manager_get_service_by_id (id);

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
tracker_db_exec_no_reply (DBConnection *db_con, const char *query, ...)
{
	TrackerDBResultSet *result_set;
	va_list args;

	lock_db();

	va_start (args, query);
	result_set = tracker_db_interface_execute_vquery (db_con->db, NULL, query, args);
	va_end (args);

	/* This function is meant for queries that don't return any result set,
	 * if it's passed some query that returns a result set, just discard it.
	 */
	if (G_UNLIKELY (result_set)) {
		g_object_unref (result_set);
	}

	unlock_db ();

	return TRUE;
}

void
tracker_db_release_memory (void)
{

}

char *
tracker_escape_string (const char *in)
{
	gchar **array, *out;

	if (strchr (in, '\'')) {
		return g_strdup (in);
	}

	/* double single quotes */
	array = g_strsplit (in, "'", -1);
	out = g_strjoinv ("''", array);
	g_strfreev (array);

	return out;
}

TrackerDBResultSet *
tracker_exec_proc (DBConnection *db_con, const char *procedure, ...)
{
	TrackerDBResultSet *result_set;
	va_list args;

	va_start (args, procedure);
	result_set = tracker_db_interface_execute_vprocedure (db_con->db, NULL, procedure, args);
	va_end (args);

	return result_set;
}


gboolean
tracker_exec_proc_no_reply (DBConnection *db_con, const char *procedure, ...)
{
	TrackerDBResultSet *result_set;
	va_list args;

	va_start (args, procedure);
	result_set = tracker_db_interface_execute_vprocedure (db_con->db, NULL, procedure, args);
	va_end (args);

	if (result_set) {
		g_object_unref (result_set);
	}

	return TRUE;
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
	TrackerDBResultSet *result_set;
	char *version;
	int i;

	result_set = tracker_db_interface_execute_query
		(db_con->db, NULL, "SELECT OptionValue FROM Options WHERE OptionKey = 'DBVersion'");

	if (!result_set) {
		return FALSE;
	}

	tracker_db_result_set_get (result_set, 0, &version, -1);
	i = atoi (version);

	g_object_unref (result_set);
	g_free (version);

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

		tracker_db_interface_execute_query (db_con->db, NULL,
						    "update Options set OptionValue = '16' where OptionKey = 'DBVersion'");
		tracker_db_interface_execute_query (db_con->db, NULL, "ANALYZE");
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


gint
tracker_metadata_is_key (const gchar *service, const gchar *meta_name)
{
	return tracker_service_manager_metadata_in_service (service, meta_name);
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
	TrackerDBResultSet *result_set;
	char *str_file_id;
	gboolean valid = TRUE;

	str_file_id = tracker_uint_to_str (id);

	lock_connection (db_con);
	result_set = tracker_db_interface_execute_procedure (db_con->db, NULL, "GetAllContents", str_file_id, NULL);
	unlock_connection (db_con);

	g_free (str_file_id);

	if (!result_set)
		return NULL;

	while (valid) {
		gchar *st;

		tracker_db_result_set_get (result_set, 0, &st, -1);
		old_table = tracker_parse_text (old_table, st, 1, TRUE, FALSE);

		valid = tracker_db_result_set_iter_next (result_set);
		g_free (st);
	}

	g_object_unref (result_set);

	return old_table;
}


GHashTable *
tracker_db_get_indexable_content_words (DBConnection *db_con, guint32 id, GHashTable *table, gboolean embedded_only)
{
	TrackerDBResultSet *result_set;
	char *str_id;

	str_id = tracker_uint_to_str (id);

	if (embedded_only) {
		result_set = tracker_exec_proc (db_con, "GetAllIndexable", str_id, "1", NULL);
	} else {
		result_set = tracker_exec_proc (db_con, "GetAllIndexable", str_id, "0", NULL);
	}

	if (result_set) {
		gboolean valid = TRUE;
		gchar *value;
		gint weight;

		while (valid) {
			tracker_db_result_set_get (result_set,
						   0, &value,
						   1, &weight,
						   -1);

			table = tracker_parse_text_fast (table, value, weight);

			g_free (value);
			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}

	if (embedded_only) {
		result_set = tracker_exec_proc (db_con, "GetAllIndexableKeywords", str_id, "1", NULL);
	} else {
		result_set = tracker_exec_proc (db_con, "GetAllIndexableKeywords", str_id, "0", NULL);
	}

	if (result_set) {
		gboolean valid = TRUE;
		gboolean filtered, delimited;
		gchar *value;
		gint weight;

		while (valid) {
			tracker_db_result_set_get (result_set,
						   0, &value,
						   1, &weight,
						   2, &filtered,
						   3, &delimited
						   -1);

			table = tracker_parse_text (table, value, weight, filtered, delimited);
			valid = tracker_db_result_set_iter_next (result_set);
			g_free (value);
		}

		g_object_unref (result_set);
	}

	g_free (str_id);

	return table;
}

static void
save_full_text_bytes (DBConnection *blob_db_con, const char *str_file_id, GByteArray *byte_array)
{
	FieldDef *def;

	def = tracker_db_get_field_def (blob_db_con, "File:Contents");

	if (!def) {
		tracker_error ("WARNING: metadata not found for type %s", "File:Contents");
		return;
	}

	tracker_db_interface_execute_procedure_len (blob_db_con->db,
						    NULL,
						    "SaveServiceContents",
						    str_file_id, -1,
						    def->id, -1,
						    byte_array->data, byte_array->len,
						    NULL);
}


static void
save_full_text (DBConnection *blob_db_con, const char *str_file_id, const char *text, int length)
{
	gchar *compressed, *value = NULL;
	gint bytes_compressed;
	FieldDef *def;

	compressed = tracker_compress (text, length, &bytes_compressed);

	if (compressed) {
		tracker_debug ("compressed full text size of %d to %d", length, bytes_compressed);
		value = compressed;
	} else {
		tracker_error ("WARNING: compression has failed");
		value = g_strdup (text);
		bytes_compressed = length;
	}


	def = tracker_db_get_field_def (blob_db_con, "File:Contents");

	if (!def) {
		tracker_error ("WARNING: metadata not found for type %s", "File:Contents");
		g_free (value);
		return;
	}

	tracker_db_interface_execute_procedure_len (blob_db_con->db,
						    NULL,
						    "SaveServiceContents",
						    str_file_id, -1,
						    def->id, -1,
						    value, bytes_compressed,
						    NULL);
	g_free (value);
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

                gint throttle;

                throttle = tracker_config_get_throttle (tracker->config);
		if (throttle > 9) {
			tracker_throttle (throttle * 100);
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
	tracker_db_interface_start_transaction (db_con->cache->db);
	tracker_db_exec_no_reply (db_con->cache, "DELETE FROM FilePending");
	tracker_db_exec_no_reply (db_con->cache, "DELETE FROM FileWatches");
	tracker_db_interface_end_transaction (db_con->cache->db);
}

void
tracker_db_check_tables (DBConnection *db_con)
{
}


TrackerDBResultSet *
tracker_db_search_text (DBConnection *db_con, const char *service, const char *search_string, int offset, int limit, gboolean save_results, gboolean detailed)
{
	TrackerQueryTree *tree;
	TrackerDBResultSet *result_set, *result;
	char 		**array;
	GArray          *hits;
	int 		count;
	gboolean	detailed_emails = FALSE, detailed_apps = FALSE;
	int		service_array[255];
	const gchar     *procedure;
	GArray          *services = NULL;
	GSList          *duds = NULL;
	guint           i = 0;

	array = tracker_parse_text_into_array (search_string);

	result_set = tracker_exec_proc (db_con->common, "GetRelatedServiceIDs", service, service, NULL);

	if (result_set) {
		gboolean valid = TRUE;
		gint type_id;

		while (valid) {
			g_print ("aaaandaya %d\n", i);
			tracker_db_result_set_get (result_set, 0, &type_id, -1);
			service_array[i] = type_id;
			i++;

			valid = tracker_db_result_set_iter_next (result_set);
		}

		service_array[i] = 0;
		services = g_array_new (TRUE, TRUE, sizeof (gint));
		g_array_append_vals (services, service_array, i);

		g_object_unref (result_set);
	}

	tree = tracker_query_tree_new (search_string, db_con->word_index, services);
	hits = tracker_query_tree_get_hits (tree, offset, limit);
	result = NULL;

	if (!save_results) {
		count = hits->len;

		if (count > limit) count = limit;
	} else {
		tracker_db_interface_start_transaction (db_con->db);
		tracker_exec_proc (db_con, "DeleteSearchResults1", NULL);
	}

	count = 0;

	for (i = 0; i < hits->len; i++) {
		TrackerSearchHit hit;
		char	  *str_id;

		if (count >= limit) break;

		hit = g_array_index (hits, TrackerSearchHit, i);

		str_id = tracker_uint_to_str (hit.service_id);

		/* we save results into SearchResults table instead of returing an array of array of strings */
		if (save_results) {
			char *str_score;

			str_score = tracker_int_to_str (hit.score);

			tracker_exec_proc (db_con, "InsertSearchResult1", str_id, str_score, NULL);

			g_free (str_id);
			g_free (str_score);

			continue;
		}

		if (detailed) {
			if (strcmp (service, "Emails") == 0) {
				detailed_emails = TRUE;
				procedure = "GetEmailByID";
			} else if (strcmp (service, "Applications") == 0) {
				detailed_apps = TRUE;
				procedure = "GetApplicationByID";
			} else {
				procedure = "GetFileByID2";
			}
		} else {
			procedure = "GetFileByID";
		}

		result_set = tracker_exec_proc (db_con, procedure, str_id, NULL);
		g_free (str_id);

		if (result_set) {
			gchar *path;

			tracker_db_result_set_get (result_set, 0, &path, -1);

			if (!detailed || detailed_emails || detailed_apps ||
			    (detailed && g_file_test (path, G_FILE_TEST_EXISTS))) {
				guint columns, i;

				columns = tracker_db_result_set_get_n_columns (result_set);

				if (G_UNLIKELY (!result)) {
					guint columns;

					columns = tracker_db_result_set_get_n_columns (result_set);
					result = _tracker_db_result_set_new (columns);
				}

				_tracker_db_result_set_append (result);

				for (i = 0; i < columns; i++) {
					GValue value = { 0, };

					_tracker_db_result_set_get_value (result_set, i, &value);
					_tracker_db_result_set_set_value (result, i, &value);
					g_value_unset (&value);
				}
			}

			g_free (path);
			g_object_unref (result_set);
		} else {
			tracker_log ("dud hit for search detected");
			/* add to dud list */
			duds = g_slist_prepend (duds, &hit);
		}

	}

	if (save_results) {
		tracker_db_interface_end_transaction (db_con->db);
	}

	/* delete duds */
	if (duds) {
		GSList *words, *w;
		Indexer *indexer;

		words = tracker_query_tree_get_words (tree);
		indexer = tracker_query_tree_get_indexer (tree);

		for (w = words; w; w = w->next) {
			tracker_remove_dud_hits (indexer, (const gchar *) w->data, duds);
		}

		g_slist_free (words);
	}

	g_object_unref (tree);
	g_array_free (hits, TRUE);
	g_array_free (services, TRUE);

	if (!result)
		return NULL;

	if (tracker_db_result_set_get_n_rows (result) == 0) {
		g_object_unref (result);
		return NULL;
	}

	tracker_db_result_set_rewind (result);

	return result;
}

TrackerDBResultSet *
tracker_db_search_files_by_text (DBConnection *db_con, const char *text, int offset, int limit, gboolean sort)
{
	return NULL;
}


TrackerDBResultSet *
tracker_db_search_metadata (DBConnection *db_con, const char *service, const char *field, const char *text, int offset, int limit)
{
	FieldDef *def;
	TrackerDBResultSet *result_set;

	g_return_val_if_fail ((service && field && text), NULL);

	def = tracker_db_get_field_def (db_con, field);

	if (!def) {
		tracker_error ("ERROR: metadata not found for type %s", field);
		return NULL;
	}

	switch (def->type) {

		case 0: 
		case 1: result_set = tracker_exec_proc (db_con, "SearchMetadata", def->id, text, NULL); break;

		case 2:
		case 3: result_set = tracker_exec_proc (db_con, "SearchMetadataNumeric", def->id, text, NULL); break;

		case 5: result_set = tracker_exec_proc (db_con, "SearchMetadataKeywords", def->id, text, NULL); break;

		default: tracker_error ("ERROR: metadata could not be retrieved as type %d is not supported", def->type); result_set = NULL;
	}


	return result_set;
}


TrackerDBResultSet *
tracker_db_search_matching_metadata (DBConnection *db_con, const char *service, const char *id, const char *text)
{
	g_return_val_if_fail (id, NULL);

	return NULL;
}

TrackerDBResultSet *
tracker_db_get_metadata (DBConnection *db_con, const char *service, const char *id, const char *key)
{
	TrackerDBResultSet *result_set;
	FieldDef *def;

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
			result_set = tracker_exec_proc (db_con, "GetMetadata", id, def->id, NULL);
			break;
		case DATA_INTEGER:
		case DATA_DATE:
			result_set = tracker_exec_proc (db_con, "GetMetadataNumeric", id, def->id, NULL);
			break;
		case DATA_FULLTEXT:
			result_set = tracker_exec_proc (db_con, "GetContents", id, def->id, NULL);
			break;
		case DATA_KEYWORD:
			result_set = tracker_exec_proc (db_con, "GetMetadataKeyword", id, def->id, NULL);
			break;

		default:
			tracker_error ("ERROR: metadata could not be retrieved as type %d is not supported", def->type); result_set = NULL;
	}

	return result_set;
}


/* gets specified metadata value as a single row (multple values for a metadata type are returned delimited by  "|" ) */
char *	
tracker_db_get_metadata_delimited (DBConnection *db_con, const char *service, const char *id, const char *key)
{
	GString *gstr = NULL;
	TrackerDBResultSet *result_set;

	result_set = tracker_db_get_metadata (db_con, service, id, key);

	if (result_set) {
		gboolean valid = TRUE;
		gchar *str;

		while (valid) {
			tracker_db_result_set_get (result_set, 0, &str, -1);

			if (gstr) {
				g_string_append_printf (gstr, "|%s", str);
			} else {
				gstr = g_string_new (str);
			}

			g_free (str);
			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
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
	gint        sid;

	if (!def) {
		tracker_error ("ERROR: cannot find details for metadata type");
		return;
	}


	old_table = NULL;
	new_table = NULL;

	if (old_value) {
		old_table = tracker_parse_text (old_table, 
                                                old_value, 
                                                def->weight, 
                                                def->filtered, 
                                                def->delimited);
	}

	/* parse new metadata value */
	if (new_value) {
		new_table = tracker_parse_text (new_table, new_value, def->weight, def->filtered, def->delimited);
	}

	/* we only do differential updates so only changed words scores are updated */
	sid = tracker_service_manager_get_id_for_service (service);
	tracker_db_update_differential_index (db_con, old_table, new_table, id, sid);

	tracker_word_table_free (old_table);
	tracker_word_table_free (new_table);
}



char *
tracker_get_related_metadata_names (DBConnection *db_con, const char *name)
{
	TrackerDBResultSet *result_set;

	result_set = tracker_exec_proc (db_con, "GetMetadataAliasesForName", name, name, NULL);

	if (result_set) {
		GString *gstr = NULL;
		gboolean valid = TRUE;
		gint id;

		while (valid) {
			tracker_db_result_set_get (result_set, 1, &id, -1);

			if (gstr) {
				g_string_append_printf (gstr, ", %d", id);
			} else {
				gstr = g_string_new ("");
				g_string_append_printf (gstr, "%d", id);
			}

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);

		return g_string_free (gstr, FALSE);
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
					gchar *mvalue = tracker_parse_text_to_string (values[i], FALSE, FALSE, FALSE);

					table = tracker_parse_text_fast (table, mvalue, def->weight);

					g_free (mvalue);
				}
	
				tracker_exec_proc (db_con, "SetMetadataKeyword", id, def->id, values[i], NULL);
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

				mvalue = tracker_parse_text_to_string (values[i], def->filtered, def->filtered, def->delimited);

				if (table) {
					table = tracker_parse_text_fast (table, mvalue, def->weight);
				}
				
				tracker_exec_proc (db_con, "SetMetadata", id, def->id, mvalue, values[i], NULL);
				
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

				tracker_exec_proc (db_con, "SetMetadata", id, def->id, " ", values[i], NULL);
			}

                        break;
                }
                case DATA_STRING: {
                        gint i;
			for (i = 0; i < length; i++) {
                                if (!values[i]) {
                                        continue;
                                }

				gchar *mvalue = tracker_parse_text_to_string (values[i], def->filtered,  def->filtered, def->delimited);

				tracker_exec_proc (db_con, "SetMetadata", id, def->id, mvalue, values[i], NULL);

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

				tracker_exec_proc (db_con, "SetMetadataNumeric", id, def->id, values[i], NULL);
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

				tracker_exec_proc (db_con, "SetMetadataNumeric", id, def->id, mvalue, NULL);

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

			tracker_db_exec_no_reply (db_con,
						  "update Services set KeyMetadata%d = '%s' where id = %s",
						  key_field, esc_value, id);

			g_free (esc_value);
		}
	}
}


static char *
get_backup_id (DBConnection *db_con, const char *id)
{
	TrackerDBResultSet *result_set;
	char *backup_id = NULL;

	result_set = tracker_exec_proc (db_con, "GetBackupServiceByID", id, NULL);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &backup_id, -1);
		g_object_unref (result_set);
	}

	if (!backup_id) {
		gint64 id;

		tracker_exec_proc (db_con, "InsertBackupService", id, NULL);
		id = tracker_db_interface_sqlite_get_last_insert_id (TRACKER_DB_INTERFACE_SQLITE (db_con->db));
		backup_id = tracker_int_to_str (id);
	}

	return backup_id;
}


static inline void
backup_non_embedded_metadata (DBConnection *db_con, const char *id, const char *key_id, const char *value)
{

	char *backup_id = get_backup_id (db_con, id);

	if (backup_id) {
		tracker_exec_proc (db_con->common, "SetBackupMetadata", backup_id, key_id, value, NULL);
		g_free (backup_id);
	}

}



static inline void
backup_delete_non_embedded_metadata_value (DBConnection *db_con, const char *id, const char *key_id, const char *value)
{

	char *backup_id = get_backup_id (db_con, id);

	if (backup_id) {
		tracker_exec_proc (db_con->common, "DeleteBackupMetadataValue", backup_id, key_id, value, NULL);
		g_free (backup_id);
	}

}

static inline void
backup_delete_non_embedded_metadata (DBConnection *db_con, const char *id, const char *key_id)
{

	char *backup_id = get_backup_id (db_con, id);

	if (backup_id) {
		tracker_exec_proc (db_con->common, "DeleteBackupMetadata", backup_id, key_id, NULL);
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

				tracker_exec_proc (db_con, "SetMetadataKeyword", id, def->id, values[i], NULL);

				/* backup non-embedded data for embedded services */
				if (do_backup && 
                                    !def->embedded && 
                                    tracker_service_manager_is_service_embedded (service)) {
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
				if (do_backup &&
                                    !def->embedded && 
                                    tracker_service_manager_is_service_embedded (service)) {
					backup_non_embedded_metadata (db_con, id, def->id, values[i]);
				}


				char *mvalue = tracker_parse_text_to_string (values[i], def->filtered,  def->filtered, def->delimited);

				tracker_exec_proc (db_con, "SetMetadata", id, def->id, mvalue, values[i], NULL);

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
				if (do_backup && 
                                    !def->embedded && 
                                    tracker_service_manager_is_service_embedded (service)) {
					backup_non_embedded_metadata (db_con, id, def->id, values[i]);
				}

				char *mvalue = tracker_parse_text_to_string (values[i], def->filtered,  def->filtered, def->delimited);

				tracker_exec_proc (db_con, "SetMetadata", id, def->id, mvalue, values[i], NULL);

				g_free (mvalue);
			}
			break;

		case DATA_DOUBLE:

			
			for (i=0; i<length; i++) {

				if (!values[i] || !values[i][0]) continue;

				/* backup non-embedded data for embedded services */
				if (do_backup && 
                                    !def->embedded && 
                                    tracker_service_manager_is_service_embedded (service)) {
					backup_non_embedded_metadata (db_con, id, def->id, values[i]);
				}


				tracker_exec_proc (db_con, "SetMetadata", id, def->id, " ", values[i], NULL);

			}
			break;

		

		case DATA_INTEGER:
	
			for (i=0; i<length; i++) {
				if (!values[i] || !values[i][0]) continue;

				/* backup non-embedded data for embedded services */
				if (do_backup && 
                                    !def->embedded && 
                                    tracker_service_manager_is_service_embedded (service)) {
					backup_non_embedded_metadata (db_con, id, def->id, values[i]);
				}


				tracker_exec_proc (db_con, "SetMetadataNumeric", id, def->id, values[i], NULL);
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

				tracker_exec_proc (db_con, "SetMetadataNumeric", id, def->id, mvalue, NULL);

				/* backup non-embedded data for embedded services */
				if (do_backup && 
                                    !def->embedded && 
                                    tracker_service_manager_is_service_embedded (service)) {
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

			tracker_db_exec_no_reply (db_con,
						  "update Services set KeyMetadata%d = '%s' where id = %s",
						  key_field, esc_value, id);

			g_free (esc_value);
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


	if (!def->embedded && 
            tracker_service_manager_is_service_embedded (service)) {
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
			mvalue = tracker_parse_text_to_string (value, def->filtered,  def->filtered, def->delimited);
			tracker_exec_proc (db_con, "DeleteMetadataValue", id, def->id, mvalue, NULL);
			g_free (mvalue);
			break;


		case DATA_DOUBLE:
			tracker_exec_proc (db_con, "DeleteMetadataValue", id, def->id, value, NULL);
			break;

		
		case DATA_INTEGER:
		case DATA_DATE:

			tracker_exec_proc (db_con, "DeleteMetadataNumericValue", id, def->id, value, NULL);
			break;

		
		case DATA_KEYWORD:
			
			tracker_exec_proc (db_con, "DeleteMetadataKeywordValue", id, def->id, value, NULL);
			break;
		
		default:	
			tracker_error ("ERROR: metadata could not be deleted as type %d for metadata %s is not supported", def->type, key);
			break;


	}

	if (key_field > 0) {
		TrackerDBResultSet *result_set;
		gchar *value;

		result_set = tracker_db_get_metadata (db_con, service, id, key);

		if (result_set) {
			tracker_db_result_set_get (result_set, 0, &value, -1);

			if (value) {
				char *esc_value = tracker_escape_string (value);

				tracker_db_exec_no_reply (db_con,
							 "update Services set KeyMetadata%d = '%s' where id = %s",
							  key_field, esc_value, id);

				g_free (esc_value);
				g_free (value);
			} else {
				tracker_db_exec_no_reply (db_con,
							  "update Services set KeyMetadata%d = NULL where id = %s",
							  key_field, id);
			}

			g_object_unref (result_set);
		} else {
			tracker_db_exec_no_reply (db_con,
						  "update Services set KeyMetadata%d = NULL where id = %s",
						  key_field, id);
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
	
	if (!def->embedded && 
            tracker_service_manager_is_service_embedded (service)) {
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
		tracker_db_exec_no_reply (db_con,
					  "update Services set KeyMetadata%d = NULL where id = %s",
					  key_field, id);
	}
	
	
	/* perform deletion */
	switch (def->type) {

		case DATA_INDEX:
		case DATA_STRING:
		case DATA_DOUBLE:
			tracker_exec_proc (db_con, "DeleteMetadata", id, def->id, NULL);
			break;

		case DATA_INTEGER:
		case DATA_DATE:
			tracker_exec_proc (db_con, "DeleteMetadataNumeric", id, def->id, NULL);
			break;

		
		case DATA_KEYWORD:
			tracker_exec_proc (db_con, "DeleteMetadataKeyword", id, def->id, NULL);
			break;

		case DATA_FULLTEXT:

			tracker_exec_proc (db_con, "DeleteContent", id, def->id, NULL);
			break;

		default:
			tracker_error ("ERROR: metadata could not be deleted as this operation is not supported by type %d for metadata %s", def->type, key);
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
	TrackerDBResultSet *result_set;
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

	result_set = tracker_exec_proc (db_con->common, "GetNewID", NULL);

	if (!result_set) {
		tracker_error ("ERROR: could not create service - GetNewID failed");
		return 0;
	}

	tracker_db_result_set_get (result_set, 0, &sid, -1);
	i = atoi (sid);
	g_free (sid);
	i++;

	sid = tracker_int_to_str (i);
	tracker_exec_proc (db_con->common, "UpdateNewID", sid, NULL);

	g_object_unref (result_set);

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

	service_type_id = tracker_service_manager_get_id_for_service (service);

	if (info->mime) {
		tracker_debug ("service id for %s is %d and sid is %s with mime %s", service, service_type_id, sid, info->mime);
	} else {
		tracker_debug ("service id for %s is %d and sid is %s", service, service_type_id, sid);
        }

	str_service_type_id = tracker_int_to_str (service_type_id);

	str_aux = tracker_int_to_str (info->aux_id);

	if (service_type_id != -1) {
		gchar *parent;

              //  gchar *apath = tracker_escape_string (path);
             //   gchar *aname = tracker_escape_string (name);

		tracker_exec_proc (db_con, "CreateService", sid, path, name,
                                   str_service_type_id, info->mime, str_filesize,
                                   str_is_dir, str_is_link, str_offset, str_mtime, str_aux, NULL);
              //  g_free (apath);
             //   g_free (aname);

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
		id = tracker_db_interface_sqlite_get_last_insert_id (TRACKER_DB_INTERFACE_SQLITE (db_con->db));

		if (info->is_hidden) {
			tracker_db_exec_no_reply (db_con,
						  "Update services set Enabled = 0 where ID = %d",
						  (int) id);
		}

		tracker_exec_proc (db_con->common, "IncStat", service, NULL);

                parent = tracker_service_manager_get_parent_service (service);
		
		if (parent) {
			tracker_exec_proc (db_con->common, "IncStat", parent, NULL);
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

	tracker_exec_proc (db_con->blob, "DeleteAllContents", str_file_id, NULL);

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
	gchar *service;
        
        service = tracker_service_manager_get_service_by_id (id);

	if (service) {
		gchar *parent;

		tracker_exec_proc (db_con->common, "DecStat", service, NULL);

                parent = tracker_service_manager_get_parent_service (service);
		
		if (parent) {
			tracker_exec_proc (db_con->common, "DecStat", parent, NULL);
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
	gint    service_id;
	guint32	id;

	service_id = tracker_service_manager_get_id_for_service (service);

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
	TrackerDBResultSet *result_set;
	gint id;

	delete_index_for_service (db_con, file_id);

	str_file_id = tracker_uint_to_str (file_id);

	result_set = tracker_exec_proc (db_con, "GetFileByID3", str_file_id, NULL);

	if (result_set) {
		tracker_db_result_set_get (result_set,
					   0, &name,
					   1, &path,
					   3, &id,
					   -1);

		if (name && path) {
			dec_stat (db_con, id);

			tracker_exec_proc (db_con, "DeleteService1", str_file_id, NULL);
			tracker_exec_proc (db_con->common, "DeleteService6", path, name, NULL);
			tracker_exec_proc (db_con->common, "DeleteService7", path, name, NULL);
			tracker_exec_proc (db_con->common, "DeleteService9", path, name, NULL);

			g_free (name);
			g_free (path);
		}

		g_object_unref (result_set);
	}

	g_free (str_file_id);
}


void
tracker_db_delete_directory (DBConnection *db_con, guint32 file_id, const char *uri)
{
	TrackerDBResultSet *result_set;
	char *str_file_id, *uri_prefix;

	str_file_id = tracker_uint_to_str (file_id);

	uri_prefix = g_strconcat (uri, G_DIR_SEPARATOR_S, "*", NULL);

	delete_index_for_service (db_con, file_id);

	/* get all file id's for all files recursively under directory amd delete them */
	result_set = tracker_exec_proc (db_con, "SelectSubFileIDs", uri, uri_prefix, NULL);

	if (result_set) {
		gboolean valid = TRUE;
		gint id;

		while (valid) {
			tracker_db_result_set_get (result_set, 0, &id, -1);
			tracker_db_delete_file (db_con, id);

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}


	/* delete all files underneath directory 
	tracker_db_interface_start_transaction (db_con->db);
	tracker_exec_proc (db_con, "DeleteService2", uri, NULL);
	tracker_exec_proc (db_con, "DeleteService3", uri_prefix, NULL);
	tracker_exec_proc (db_con, "DeleteService4", uri, NULL);
	tracker_exec_proc (db_con, "DeleteService5", uri_prefix, NULL);
	tracker_exec_proc (db_con, "DeleteService8", uri, uri_prefix, NULL);
	tracker_exec_proc (db_con, "DeleteService10", uri, uri_prefix, NULL);
	tracker_db_interface_end_transaction (db_con->db);
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

	tracker_exec_proc (db_con->index, "UpdateFile", str_service_type_id, path, name, info->mime, str_size, str_mtime, str_offset, str_file_id, NULL);
	
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
	TrackerDBResultSet *result_set;
	gboolean has_pending;

	if (!tracker->is_running) {
		return FALSE;
	}

	has_pending = FALSE;

	result_set = tracker_exec_proc (db_con->cache, "ExistsPendingFiles", NULL);

	if (result_set) {
		gint pending_file_count;

		tracker_db_result_set_get (result_set, 0, &pending_file_count, -1);
		has_pending = (pending_file_count > 0);

		g_object_unref (result_set);
	}

	return has_pending;
}


gboolean
tracker_db_has_pending_metadata (DBConnection *db_con)
{
	TrackerDBResultSet *result_set;
	gboolean has_pending;

	if (!tracker->is_running) {
		return FALSE;
	}

	has_pending = FALSE;

	result_set = tracker_exec_proc (db_con->cache, "CountPendingMetadataFiles", 0);

	if (result_set) {
		gint pending_file_count;

		tracker_db_result_set_get (result_set, 0, &pending_file_count, -1);
		has_pending = (pending_file_count > 0);

		g_object_unref (result_set);
	}

	return has_pending;
}


TrackerDBResultSet *
tracker_db_get_pending_files (DBConnection *db_con)
{
	DBConnection *cache;
	time_t time_now;

	if (!tracker->is_running) {
		return NULL;
	}

	cache = db_con->cache;
	time (&time_now);

	tracker_db_exec_no_reply (cache, "DELETE FROM FileTemp");

	tracker_db_exec_no_reply (cache,
				  "INSERT INTO FileTemp (ID, FileID, Action, FileUri, MimeType,"
				  " IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID) "
				  "SELECT ID, FileID, Action, FileUri, MimeType,"
				  " IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID "
				  "FROM FilePending WHERE (PendingDate < %d) AND (Action <> 20) LIMIT 250",
				  (gint) time_now);

	tracker_db_exec_no_reply (cache, "DELETE FROM FilePending WHERE ID IN (SELECT ID FROM FileTemp)");

	return tracker_db_interface_execute_query (cache->db, NULL, "SELECT FileID, FileUri, Action, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM FileTemp ORDER BY ID");
}


void
tracker_db_remove_pending_files (DBConnection *db_con)
{
	tracker_db_exec_no_reply (db_con->cache, "DELETE FROM FileTemp");
}


TrackerDBResultSet *
tracker_db_get_pending_metadata (DBConnection *db_con)
{
	DBConnection *cache;
	const char *str;

	if (!tracker->is_running) {
		return NULL;
	}

	cache = db_con->cache;

	str = "INSERT INTO MetadataTemp (ID, FileID, Action, FileUri, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID) SELECT ID, FileID, Action, FileUri, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM FilePending WHERE Action = 20 LIMIT 250";

	tracker_db_exec_no_reply (cache, "DELETE FROM MetadataTemp");
	tracker_db_exec_no_reply (cache, str);
	tracker_db_exec_no_reply (cache, "DELETE FROM FilePending WHERE ID IN (SELECT ID FROM MetadataTemp)");

	return tracker_db_interface_execute_query (cache->db, NULL, "SELECT FileID, FileUri, Action, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM MetadataTemp ORDER BY ID");
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
		tracker_exec_proc (db_con->cache, "InsertPendingFile", id, action, time_str, uri, mime, "1", str_new, "1", "1", str_service_type_id, NULL);
	} else {
		tracker_exec_proc (db_con->cache, "InsertPendingFile", id, action, time_str, uri, mime, "0", str_new, "1", "1", str_service_type_id, NULL);
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

	tracker_exec_proc (db_con->cache, "UpdatePendingFile", time_str, action, uri, NULL);

	g_free (time_str);
}


TrackerDBResultSet *
tracker_db_get_files_by_service (DBConnection *db_con, const char *service, int offset, int limit)
{
	TrackerDBResultSet *result_set;
	char *str_limit, *str_offset;

	str_limit = tracker_int_to_str (limit);
	str_offset = tracker_int_to_str (offset);

	result_set = tracker_exec_proc (db_con, "GetByServiceType", service, service, str_offset, str_limit, NULL);

	g_free (str_offset);
	g_free (str_limit);

	return result_set;
}

TrackerDBResultSet *
tracker_db_get_files_by_mime (DBConnection *db_con, char **mimes, int n, int offset, int limit, gboolean vfs)
{
	TrackerDBResultSet *result_set;
	int	i;
	char *service;
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

	result_set = tracker_db_interface_execute_query (db_con->db, NULL, query);

	g_free (query);

	return result_set;
}

TrackerDBResultSet *
tracker_db_search_text_mime (DBConnection *db_con, const char *text, char **mime_array)
{
	TrackerQueryTree *tree;
	TrackerDBResultSet *result;
	GArray       *hits;
	GSList 	     *result_list;
	guint        i;
	int 	     count;
	gint         service_array[8];
	GArray       *services;

	result = NULL;
	result_list = NULL;

	service_array[0] = tracker_service_manager_get_id_for_service ("Files");
	service_array[1] = tracker_service_manager_get_id_for_service ("Folders");
	service_array[2] = tracker_service_manager_get_id_for_service ("Documents");
	service_array[3] = tracker_service_manager_get_id_for_service ("Images");
	service_array[4] = tracker_service_manager_get_id_for_service ("Music");
	service_array[5] = tracker_service_manager_get_id_for_service ("Videos");
	service_array[6] = tracker_service_manager_get_id_for_service ("Text");
	service_array[7] = tracker_service_manager_get_id_for_service ("Other");

	services = g_array_new (TRUE, TRUE, sizeof (gint));
	g_array_append_vals (services, service_array, 8);

	tree = tracker_query_tree_new (text, db_con->word_index, services);
	hits = tracker_query_tree_get_hits (tree, 0, 0);
	count = 0;

	for (i = 0; i < hits->len; i++) {
		TrackerDBResultSet *result_set;
		TrackerSearchHit hit;
		char *str_id, *mimetype;

		hit = g_array_index (hits, TrackerSearchHit, i);

		str_id = tracker_uint_to_str (hit.service_id);

		result_set = tracker_exec_proc (db_con, "GetFileByID", str_id, NULL);

		g_free (str_id);

		if (result_set) {
			tracker_db_result_set_get (result_set, 2, &mimetype, -1);

			if (tracker_str_in_array (mimetype, mime_array) != -1) {
				GValue value = { 0, };

				if (G_UNLIKELY (!result)) {
					result = _tracker_db_result_set_new (2);
				}

				_tracker_db_result_set_append (result);

				/* copy value in column 0 */
				_tracker_db_result_set_get_value (result_set, 0, &value);
				_tracker_db_result_set_set_value (result, 0, &value);
				g_value_unset (&value);

				/* copy value in column 1 */
				_tracker_db_result_set_get_value (result_set, 1, &value);
				_tracker_db_result_set_set_value (result, 1, &value);
				g_value_unset (&value);

				count++;
			}

			g_free (mimetype);
			g_object_unref (result_set);
		}

		if (count > 2047) {
			break;
		}
	}

	g_object_unref (tree);
	g_array_free (hits, TRUE);
	g_array_free (services, TRUE);

	if (!result)
		return NULL;

	if (tracker_db_result_set_get_n_rows (result) == 0) {
		g_object_unref (result);
		return NULL;
	}

	tracker_db_result_set_rewind (result);

	return result;
}

TrackerDBResultSet *
tracker_db_search_text_location (DBConnection *db_con, const char *text, const char *location)
{
	TrackerDBResultSet *result;
	TrackerQueryTree *tree;
	GArray       *hits;
	char	     *location_prefix;
	int 	     count;
	gint         service_array[8];
	guint        i;
	GArray       *services;

	location_prefix = g_strconcat (location, G_DIR_SEPARATOR_S, NULL);

	service_array[0] = tracker_service_manager_get_id_for_service ("Files");
	service_array[1] = tracker_service_manager_get_id_for_service ("Folders");
	service_array[2] = tracker_service_manager_get_id_for_service ("Documents");
	service_array[3] = tracker_service_manager_get_id_for_service ("Images");
	service_array[4] = tracker_service_manager_get_id_for_service ("Music");
	service_array[5] = tracker_service_manager_get_id_for_service ("Videos");
	service_array[6] = tracker_service_manager_get_id_for_service ("Text");
	service_array[7] = tracker_service_manager_get_id_for_service ("Other");

	services = g_array_new (TRUE, TRUE, sizeof (gint));
	g_array_append_vals (services, service_array, 8);

	tree = tracker_query_tree_new (text, db_con->word_index, services);
	hits = tracker_query_tree_get_hits (tree, 0, 0);
	count = 0;

	for (i = 0; i < hits->len; i++) {
		TrackerDBResultSet *result_set;
		TrackerSearchHit hit;
		char *str_id, *path;

		hit = g_array_index (hits, TrackerSearchHit, i);

		str_id = tracker_uint_to_str (hit.service_id);

		result_set = tracker_exec_proc (db_con, "GetFileByID", str_id, NULL);

		g_free (str_id);

		if (result_set) {
			tracker_db_result_set_get (result_set, 0, &path, -1);

			if (g_str_has_prefix (path, location_prefix) || (strcmp (path, location) == 0)) {
				GValue value = { 0, };

				if (G_UNLIKELY (!result)) {
					result = _tracker_db_result_set_new (2);
				}

				_tracker_db_result_set_append (result);

				/* copy value in column 0 */
				_tracker_db_result_set_get_value (result_set, 0, &value);
				_tracker_db_result_set_set_value (result, 0, &value);
				g_value_unset (&value);

				/* copy value in column 1 */
				_tracker_db_result_set_get_value (result_set, 1, &value);
				_tracker_db_result_set_set_value (result, 1, &value);
				g_value_unset (&value);

				count++;
			}

			g_object_unref (result_set);
		}

		if (count > 2047) {
			break;
		}
	}

	g_free (location_prefix);

	g_object_unref (tree);
	g_array_free (hits, TRUE);
	g_array_free (services, TRUE);

	if (!result)
		return NULL;

	if (tracker_db_result_set_get_n_rows (result) == 0) {
		g_object_unref (result);
		return NULL;
	}

	tracker_db_result_set_rewind (result);

	return result;
}

TrackerDBResultSet *
tracker_db_search_text_mime_location (DBConnection *db_con, const char *text, char **mime_array, const char *location)
{
	TrackerDBResultSet *result;
	TrackerQueryTree *tree;
	GArray       *hits;
	char	     *location_prefix;
	int	     count;
	gint         service_array[8];
	guint        i;
	GArray       *services;

	location_prefix = g_strconcat (location, G_DIR_SEPARATOR_S, NULL);

	service_array[0] = tracker_service_manager_get_id_for_service ("Files");
	service_array[1] = tracker_service_manager_get_id_for_service ("Folders");
	service_array[2] = tracker_service_manager_get_id_for_service ("Documents");
	service_array[3] = tracker_service_manager_get_id_for_service ("Images");
	service_array[4] = tracker_service_manager_get_id_for_service ("Music");
	service_array[5] = tracker_service_manager_get_id_for_service ("Videos");
	service_array[6] = tracker_service_manager_get_id_for_service ("Text");
	service_array[7] = tracker_service_manager_get_id_for_service ("Other");

	services = g_array_new (TRUE, TRUE, sizeof (gint));
	g_array_append_vals (services, service_array, 8);

	tree = tracker_query_tree_new (text, db_con->word_index, services);
	hits = tracker_query_tree_get_hits (tree, 0, 0);
	count = 0;

	for (i = 0; i < hits->len; i++) {
		TrackerDBResultSet *result_set;
		TrackerSearchHit hit;
		char *str_id, *path, *mimetype;

		hit = g_array_index (hits, TrackerSearchHit, i);

		str_id = tracker_uint_to_str (hit.service_id);

		result_set = tracker_exec_proc (db_con, "GetFileByID", str_id, NULL);

		g_free (str_id);

		if (result_set) {
			tracker_db_result_set_get (result_set,
						   0, &path,
						   2, &mimetype,
						   -1);

			if ((g_str_has_prefix (path, location_prefix) || (strcmp (path, location) == 0)) &&
			    tracker_str_in_array (mimetype, mime_array) != -1) {
				GValue value = { 0, };

				if (G_UNLIKELY (!result)) {
					result = _tracker_db_result_set_new (2);
				}

				_tracker_db_result_set_append (result);

				/* copy value in column 0 */
				_tracker_db_result_set_get_value (result_set, 0, &value);
				_tracker_db_result_set_set_value (result, 0, &value);
				g_value_unset (&value);

				/* copy value in column 1 */
				_tracker_db_result_set_get_value (result_set, 1, &value);
				_tracker_db_result_set_set_value (result, 1, &value);
				g_value_unset (&value);

				count++;
			}

			g_free (path);
			g_free (mimetype);
			g_object_unref (result_set);
		}

		if (count > 2047) {
			break;
		}
	}

	g_free (location_prefix);

	g_object_unref (tree);
	g_array_free (hits, TRUE);
	g_array_free (services, TRUE);

	if (!result)
		return NULL;

	if (tracker_db_result_set_get_n_rows (result) == 0) {
		g_object_unref (result);
		return NULL;
	}

	tracker_db_result_set_rewind (result);

	return result;
}

TrackerDBResultSet *
tracker_db_get_metadata_types (DBConnection *db_con, const char *class, gboolean writeable)
{
	if (strcmp (class, "*") == 0) {
		if (writeable) {
			return tracker_exec_proc (db_con, "GetWriteableMetadataTypes", NULL);
		} else {
			return tracker_exec_proc (db_con, "GetMetadataTypes", NULL);
		}

	} else {
		if (writeable) {
			return tracker_exec_proc (db_con, "GetWriteableMetadataTypesLike", class, NULL);
		} else {
			return tracker_exec_proc (db_con, "GetMetadataTypesLike", class, NULL);
		}
	}
}

TrackerDBResultSet *
tracker_db_get_sub_watches (DBConnection *db_con, const char *dir)
{
	TrackerDBResultSet *result_set;
	char *folder;

	folder = g_strconcat (dir, G_DIR_SEPARATOR_S, "*", NULL);

	result_set = tracker_exec_proc (db_con->cache, "GetSubWatches", folder, NULL);

	g_free (folder);

	return result_set;
}


TrackerDBResultSet *
tracker_db_delete_sub_watches (DBConnection *db_con, const char *dir)
{
	TrackerDBResultSet *result_set;
	char *folder;

	folder = g_strconcat (dir, G_DIR_SEPARATOR_S, "*", NULL);

	result_set = tracker_exec_proc (db_con->cache, "DeleteSubWatches", folder, NULL);

	g_free (folder);

	return result_set;
}


void
tracker_db_move_file (DBConnection *db_con, const char *moved_from_uri, const char *moved_to_uri)
{

	tracker_log ("Moving file %s to %s", moved_from_uri, moved_to_uri);

	/* if orig file not in DB, treat it as a create action */
	guint32 id = tracker_db_get_file_id (db_con, moved_from_uri);
	if (id == 0) {
		tracker_debug ("WARNING: original file %s not found in DB", moved_from_uri);
		tracker_db_insert_pending_file (db_con, id, moved_to_uri,  NULL, "unknown", 0, TRACKER_ACTION_WRITABLE_FILE_CLOSED, FALSE, TRUE, -1);
		tracker_db_interface_end_transaction (db_con->db);
		return;
	}

	char *str_file_id = tracker_uint_to_str (id);
	char *name = g_path_get_basename (moved_to_uri);
	char *path = g_path_get_dirname (moved_to_uri);
	char *old_name = g_path_get_basename (moved_from_uri);
	char *old_path = g_path_get_dirname (moved_from_uri);


	/* update db so that fileID reflects new uri */
	tracker_exec_proc (db_con, "UpdateFileMove", path, name, str_file_id, NULL);

	/* update File:Path and File:Filename metadata */
	tracker_db_set_single_metadata (db_con, "Files", str_file_id, "File:Path", path, FALSE);
	tracker_db_set_single_metadata (db_con, "Files", str_file_id, "File:Name", name, FALSE);

	char *ext = strrchr (moved_to_uri, '.');
	if (ext) {
		ext++;
		tracker_db_set_single_metadata (db_con, "Files", str_file_id,  "File:Ext", ext, FALSE);
	}

	/* update backup service if necessary */
	tracker_exec_proc (db_con->common, "UpdateBackupService", path, name, old_path, old_name, NULL);

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
	TrackerDBResultSet *result_set;

	/* get all sub files (excluding folders) that were moved and add watches */
	result_set = tracker_exec_proc (db_con, "SelectFileChildWithoutDirs", moved_from_uri, NULL);

	if (result_set) {
		gboolean valid = TRUE;
		gchar *prefix, *name, *file_name, *moved_file_name;

		while (valid) {
			tracker_db_result_set_get (result_set,
						   0, &prefix,
						   1, &name,
						   -1);

			if (prefix && name) {
				file_name = g_build_filename (prefix, name, NULL);
				moved_file_name = g_build_filename (moved_to_uri, name, NULL);

				tracker_db_move_file (db_con, file_name, moved_file_name);

				g_free (moved_file_name);
				g_free (file_name);
				g_free (prefix);
				g_free (name);
			}

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
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
	TrackerDBResultSet *result_set;
	char *old_path;

	old_path = g_strconcat (moved_from_uri, G_DIR_SEPARATOR_S, NULL);

	/* get all sub folders that were moved and add watches */
	result_set = tracker_db_get_file_subfolders (db_con, moved_from_uri);

	if (result_set) {
		gboolean valid = TRUE;

		while (valid) {
			gchar *prefix, *name;
			gchar *dir_name, *sep, *new_path;

			tracker_db_result_set_get (result_set,
						   1, &prefix,
						   2, &name,
						   -1);

			dir_name = g_build_filename (prefix, name, NULL);
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

			g_free (prefix);
			g_free (name);
			g_free (new_path);
			g_free (dir_name);

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}

	move_directory (db_con, moved_from_uri, moved_to_uri);

	g_free (old_path);
}


TrackerDBResultSet *
tracker_db_get_file_subfolders (DBConnection *db_con, const char *uri)
{
	TrackerDBResultSet *result_set;
	char *folder;

	folder = g_strconcat (uri, G_DIR_SEPARATOR_S, "*", NULL);

	result_set = tracker_exec_proc (db_con, "SelectFileSubFolders", uri, folder, NULL);

	g_free (folder);

	return result_set;
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

	tracker_debug ("updating index for word %s with score %d", word, score);
	
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


TrackerDBResultSet *
tracker_db_get_keyword_list (DBConnection *db_con, const char *service)
{

	tracker_debug (service);
	return tracker_exec_proc (db_con, "GetKeywordList", service, service, NULL);
}

GSList *
tracker_db_mime_query (DBConnection *db_con, 
                       const gchar  *stored_proc, 
                       gint          service_id)
{
	TrackerDBResultSet *result_set;
	GSList  *result = NULL;
	gchar   *service_id_str;

	service_id_str = g_strdup_printf ("%d", service_id);
	result_set = tracker_exec_proc (db_con, stored_proc, service_id_str, NULL);
	g_free (service_id_str);

	if (result_set) {
		gboolean valid = TRUE;
		gchar *str;

		while (valid) {
			tracker_db_result_set_get (result_set, 0, &str, -1);
			result = g_slist_prepend (result, str);
			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}

	return result;
}

GSList *
tracker_db_get_mimes_for_service_id (DBConnection *db_con, 
                                     gint          service_id) 
{
	return  tracker_db_mime_query (db_con, "GetMimeForServiceId", service_id);
}

GSList *
tracker_db_get_mime_prefixes_for_service_id (DBConnection *db_con,
                                             gint          service_id) 
{
	return tracker_db_mime_query (db_con, "GetMimePrefixForServiceId", service_id);
}

static TrackerService *
db_row_to_service (TrackerDBResultSet *result_set)
{
        TrackerService *service;
        GSList         *new_list = NULL;
        gint            id, i;
	gchar          *name, *parent, *content_metadata;
	gboolean        enabled, embedded, has_metadata, has_fulltext;
	gboolean        has_thumbs, show_service_files, show_service_directories;

        service = tracker_service_new ();

	tracker_db_result_set_get (result_set,
				   0, &id,
				   1, &name,
				   2, &parent,
				   3, &enabled,
				   4, &embedded,
				   5, &has_metadata,
				   6, &has_fulltext,
				   7, &has_thumbs,
				   8, &content_metadata,
				   10, &show_service_files,
				   11, &show_service_directories,
				   -1);

        tracker_service_set_id (service, id);
        tracker_service_set_name (service, name);
        tracker_service_set_parent (service, parent);
        tracker_service_set_enabled (service, enabled);
        tracker_service_set_embedded (service, embedded);
        tracker_service_set_has_metadata (service, has_metadata);
        tracker_service_set_has_full_text (service, has_fulltext);
        tracker_service_set_has_thumbs (service, has_thumbs);
	tracker_service_set_content_metadata (service, content_metadata);

        if (g_str_has_prefix (name, "Email") ||
            g_str_has_suffix (name, "Emails")) {
                tracker_service_set_db_type (service, TRACKER_DB_TYPE_EMAIL);
                
                if (tracker->email_service_min == 0 || 
                    id < tracker->email_service_min) {
                        tracker->email_service_min = id;
                }
                
                if (tracker->email_service_max == 0 || 
                    id > tracker->email_service_max) {
                        tracker->email_service_max = id;
                }
        } else {
                tracker_service_set_db_type (service, TRACKER_DB_TYPE_DATA);
        }
        
        tracker_service_set_show_service_files (service, show_service_files);
        tracker_service_set_show_service_directories (service, show_service_directories);
        
        for (i = 12; i < 23; i++) {
		gchar *metadata;

		tracker_db_result_set_get (result_set, i, &metadata, -1);

		if (metadata) {
			new_list = g_slist_prepend (new_list, metadata);
		}
        }
        
        /* Hack to prevent db change late in the cycle, check the
         * service name matches "Applications", then add some voodoo.
         */
        if (strcmp (name, "Applications") == 0) {
                /* These strings should be definitions at the top of
                 * this file somewhere really.
                 */
                new_list = g_slist_prepend (new_list, g_strdup ("App:DisplayName"));
                new_list = g_slist_prepend (new_list, g_strdup ("App:Exec"));
                new_list = g_slist_prepend (new_list, g_strdup ("App:Icon"));
        }
        
        new_list = g_slist_reverse (new_list);
        
        tracker_service_set_key_metadata (service, new_list);
	g_slist_foreach (new_list, (GFunc) g_free, NULL);
        g_slist_free (new_list);

        return service;
} 

/* get static data like metadata field definitions and services definitions and load them into hashtables */
void
tracker_db_get_static_data (DBConnection *db_con)
{
	TrackerDBResultSet *result_set;

	/* get static metadata info */
	result_set  = tracker_exec_proc (db_con, "GetMetadataTypes", 0);

	if (result_set) {
		gboolean valid = TRUE;
		gchar *name;
		gint id;

		while (valid) {
			TrackerDBResultSet *result_set2;
			gboolean embedded, multiple_values, delimited, filtered, store_metadata;
			FieldDef *def;

			def = g_new0 (FieldDef, 1);

			tracker_db_result_set_get (result_set,
						   0, &id,
						   1, &name,
						   2, &def->type,
						   3, &def->field_name,
						   4, &def->weight,
						   5, &embedded,
						   6, &multiple_values,
						   7, &delimited,
						   8, &filtered,
						   9, &store_metadata,
						   -1);

			def->id = tracker_int_to_str (id);
			def->embedded = embedded;
			def->multiple_values = multiple_values;
			def->delimited = delimited;
			def->filtered = filtered;
			def->store_metadata = store_metadata;

			result_set2 = tracker_exec_proc (db_con, "GetMetadataAliases", def->id, NULL);

			if (result_set2) {
				valid = TRUE;

				while (valid) {
					tracker_db_result_set_get (result_set2, 1, &id, -1);
					def->child_ids = g_slist_prepend (def->child_ids,
									  tracker_int_to_str (id));

					valid = tracker_db_result_set_iter_next (result_set2);
				}

				g_object_unref (result_set2);
			}

			g_hash_table_insert (tracker->metadata_table, g_utf8_strdown (name, -1), def);
			tracker_debug ("loading metadata def %s with weight %d", def->field_name, def->weight);

			g_free (name);

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}

	/* get static service info */	
	result_set  = tracker_exec_proc (db_con, "GetAllServices", 0);

	if (result_set) {
		gboolean valid = TRUE;
		
		tracker->email_service_min = 0;
		tracker->email_service_max = 0;

		while (valid) {
			TrackerService *service;
			GSList *mimes, *mime_prefixes;
			const gchar *name;
			gint id;

                        service = db_row_to_service (result_set);

                        if (!service) {
                                continue;
			}

                        id = tracker_service_get_id (service);
                        name = tracker_service_get_name (service);

                        mimes = tracker_db_get_mimes_for_service_id (db_con, id);
                        mime_prefixes = tracker_db_get_mime_prefixes_for_service_id (db_con, id);

                        tracker_debug ("Adding service definition for %s with id %d", name, id);
                        tracker_service_manager_add_service (service, 
                                                             mimes, 
                                                             mime_prefixes);

                        g_slist_free (mimes);
                        g_slist_free (mime_prefixes);
                        g_object_unref (service);

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);

		/* check for web history */
		if (!tracker_service_manager_get_service ("Webhistory")) {
			tracker_log ("Adding missing Webhistory service");
			tracker_exec_proc (db_con, "InsertServiceType", "Webhistory", NULL);
		}
	}
}

DBConnection *
tracker_db_get_service_connection (DBConnection *db_con, const char *service)
{
	TrackerDBType type;

	type = tracker_service_manager_get_db_for_service (service);

	if (type == TRACKER_DB_TYPE_EMAIL) {
		return db_con->emails;
	}

	return db_con;
}


char *
tracker_db_get_service_for_entity (DBConnection *db_con, const char *id)
{
	TrackerDBResultSet *result_set;
	char *result = NULL;

	result_set = tracker_exec_proc (db_con, "GetFileByID2", id, NULL);

	if (result_set) {
		tracker_db_result_set_get (result_set, 1, &result, -1);
		g_object_unref (result_set);
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
	char 			*service_file;
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
					tracker_exec_proc (db_con, "InsertMetadataType", *array, NULL);
					id = tracker_db_interface_sqlite_get_last_insert_id (TRACKER_DB_INTERFACE_SQLITE (db_con->db));
				} else {
					id = atoi (def->id);
				}
			} else if (is_service) {
				TrackerService *service;
				
				tracker_log ("Trying to obtain service %s in cache", *array);
				service = tracker_service_manager_get_service (*array);

				if (!service) {
					tracker_exec_proc (db_con, "InsertServiceType", *array, NULL);
					id = tracker_db_interface_sqlite_get_last_insert_id (TRACKER_DB_INTERFACE_SQLITE (db_con->db));
				} else {
					id = tracker_service_get_id (service);
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

							tracker_exec_proc (db_con, "InsertMetaDataChildren", str_id, value, NULL);

						} else if (strcasecmp (*array2, "DataType") == 0) {

							int data_id = tracker_str_in_array (value, DataTypeArray);

							if (data_id != -1) {
								tracker_db_exec_no_reply (db_con,
											  "update MetaDataTypes set DataTypeID = %d where ID = %s",
											  data_id, str_id);
							}
						

						} else {
							char *esc_value = tracker_escape_string (value);

							tracker_db_exec_no_reply (db_con,
										  "update MetaDataTypes set  %s = '%s' where ID = %s",
										  *array2, esc_value, str_id);
							g_free (esc_value);
						}
	
					} else 	if (is_service) {

						if (strcasecmp (*array2, "TabularMetadata") == 0) {

							char **tab_array = g_key_file_get_string_list (key_file, *array, *array2, NULL, NULL);

							char **tmp;
							for (tmp = tab_array; *tmp; tmp++) { 			

								tracker_exec_proc (db_con, "InsertServiceTabularMetadata", str_id, *tmp, NULL);
								
							}

							g_strfreev (tab_array);



						} else if (strcasecmp (*array2, "TileMetadata") == 0) {

							char **tab_array = g_key_file_get_string_list (key_file, *array, *array2, NULL, NULL);

							char **tmp;
							for (tmp = tab_array; *tmp; tmp++) { 			

								tracker_exec_proc (db_con, "InsertServiceTileMetadata", str_id, *tmp, NULL);
							}

							g_strfreev (tab_array);

						} else if (strcasecmp (*array2, "Mimes") == 0) {

							char **tab_array = g_key_file_get_string_list (key_file, *array, *array2, NULL, NULL);

							char **tmp;
							for (tmp = tab_array; *tmp; tmp++) { 			
								tracker_exec_proc (db_con, "InsertMimes", *tmp, NULL);
							
								tracker_db_exec_no_reply (db_con,
											  "update FileMimes set ServiceTypeID = %s where Mime = '%s'",
											  str_id, *tmp);
							}

							g_strfreev (tab_array);

						} else if (strcasecmp (*array2, "MimePrefixes") == 0) {

							char **tab_array = g_key_file_get_string_list (key_file, *array, *array2, NULL, NULL);

							char **tmp;
							for (tmp = tab_array; *tmp; tmp++) { 			
								tracker_exec_proc (db_con, "InsertMimePrefixes", *tmp, NULL);

								tracker_db_exec_no_reply (db_con,
											  "update FileMimePrefixes set ServiceTypeID = %s where MimePrefix = '%s'",
											  str_id, *tmp);
							}

							g_strfreev (tab_array);


						} else {
							char *esc_value = tracker_escape_string (value);

							tracker_db_exec_no_reply (db_con,
										  "update ServiceTypes set  %s = '%s' where TypeID = %s",
										  *array2, esc_value, str_id);
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
	TrackerDBResultSet *result_set;
	gchar *value = NULL;

	result_set = tracker_exec_proc (db_con->common, "GetOption", option, NULL);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &value, -1);
		g_object_unref (result_set);
	}

	return value;
}


void
tracker_db_set_option_string (DBConnection *db_con, const char *option, const char *value)
{
	tracker_exec_proc (db_con->common, "SetOption", value, option, NULL);
}


int
tracker_db_get_option_int (DBConnection *db_con, const char *option)
{
	TrackerDBResultSet *result_set;
	gchar *str;
	int value = 0;

	result_set = tracker_exec_proc (db_con->common, "GetOption", option, NULL);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &str, -1);

		if (str) {
			value = atoi (str);
			g_free (str);
		}

		g_object_unref (result_set);
	}

	return value;
}


void
tracker_db_set_option_int (DBConnection *db_con, const char *option, int value)
{
	char *str_value = tracker_int_to_str (value);

	tracker_exec_proc (db_con->common, "SetOption", str_value, option, NULL);

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

gboolean
tracker_db_integrity_check (DBConnection *db_con)
{
	TrackerDBResultSet *result_set;
	gboolean integrity_check = TRUE;

	result_set = tracker_db_interface_execute_query (db_con->db, NULL, "pragma integrity_check;");

	if (!result_set) {
		integrity_check = FALSE;
	} else {
		gchar *check;

		tracker_db_result_set_get (result_set, 0, &check, -1);

		if (check) {
			integrity_check = (strcasecmp (check, "ok") == 0);
			g_free (check);
		}

		g_object_unref (result_set);
	}

	return integrity_check;
}

