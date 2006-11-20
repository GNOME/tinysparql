#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <regex.h>

#include "tracker-db-sqlite.h"
#include "tracker-indexer.h"
#include "tracker-metadata.h"

extern Tracker *tracker;

static GHashTable *prepared_queries;
static GMutex *sequence_mutex;
static GMutex *data_mutex;
static GMutex *blob_mutex;
static GMutex *cache_mutex;

gboolean use_nfs_safe_locking = FALSE;


/* sqlite user defined functions for use in sql */

/* converts date/time in UTC format to ISO 8160 standardised format for display */
static void
sqlite3_date_to_str (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	switch (sqlite3_value_type (argv[0])) {

		case SQLITE_NULL: {
			sqlite3_result_null (context);
			break;
		}

		default:{
			const char *output;

			output = tracker_date_to_str (sqlite3_value_double (argv[0]));
			sqlite3_result_text (context, output, strlen (output), g_free);
		}
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
				tracker_log ("decompression failed");
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
			const char *output;

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


FieldDef *
tracker_db_get_field_def (DBConnection *db_con, const char *field_name)
{
	FieldDef *def;
	char	 ***res;
	char	 **row;

	def = g_slice_new0 (FieldDef);

	res = tracker_exec_proc (db_con, "GetMetadataTypeInfo", 1, field_name);

	row = NULL;

	if (res) {
		row = tracker_db_get_row (res, 0);
	}

	if (res && row && row[0]) {
		def->id = g_strdup (row[0]);
	} else {
		g_slice_free (FieldDef, def);
		tracker_db_free_result (res);
		return NULL;
	}

	if (res && row && row[1]) {
		def->type = atoi (row[1]);
	}

	if (res && row && row[2]) {
		def->embedded = (strcmp ("1", row[2]) == 0);
	}

	if (res && row && row[3]) {
		def->writeable = (strcmp ("1", row[3]) == 0);
	}

	tracker_db_free_result (res);

	return def;
}


void
tracker_db_free_field_def (FieldDef *def)
{
	g_return_if_fail (def);

	if (def->id) {
		g_free (def->id);
	}

	g_slice_free (FieldDef, def);
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

			value = *row;

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

	if (!result || tracker_get_row_count == 0) {
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

	tracker_log ("Using Sqlite version %s", sqlite3_version);

	sequence_mutex = g_mutex_new ();
	data_mutex = g_mutex_new ();
	blob_mutex = g_mutex_new ();
	cache_mutex = g_mutex_new ();

	/* load prepared queries */
	prepared_queries = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	tracker_log ("Loading prepared queries...");

	sql_file = g_strdup (DATADIR "/tracker/sqlite-stored-procs.sql");

	if (!g_file_test (sql_file, G_FILE_TEST_EXISTS)) {
		tracker_log ("Tracker cannot read required file %s - Please reinstall tracker or check read permissions on the file if it exists", sql_file);
		g_assert (FALSE);
	}

	file = g_fopen (sql_file, "r");
	g_free (sql_file);

	tv = tracker_timer_start ();

	while (!feof (file)) {
		char buffer[8192];
		char *sep;

		fgets (buffer, 8192, file);

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

	tracker_timer_end (tv, "File loaded in ");


	tracker_log ("initialising the indexer");
	/* create and initialise indexer */
	tracker->file_indexer = tracker_indexer_open ("Files");

	/* end tracker test */

	return TRUE;
}


void
tracker_db_thread_init (void)
{
}


void
tracker_db_thread_end (void)
{
	//sqlite3_thread_cleanup ();
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

			tracker_log ("Error statement could not be finalized for %s with error %s", (char *) key, sqlite3_errmsg (db_con->db));
		}
	}
}


void
tracker_db_close (DBConnection *db_con)
{
	if (!db_con->thread) {
		db_con->thread = "main";
	}

	tracker_log ("starting database closure for thread %s", db_con->thread);

	//sqlite3_interrupt (db_con->db);

	/* clear prepared queries */
	if (db_con->statements) {
		g_hash_table_foreach (db_con->statements, finalize_statement, db_con);
	}


	g_hash_table_destroy (db_con->statements);

	if (sqlite3_close (db_con->db) != SQLITE_OK) {
		tracker_log ("ERROR : Database close operation failed for thread %s due to %s", db_con->thread, sqlite3_errmsg (db_con->db));
	} else {
		tracker_log ("Database closed for thread %s", db_con->thread);
	}
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
		tracker_log ("ERROR : failed to prepare query %s with sql %s due to %s", procedure, query, sqlite3_errmsg (db_con->db));
		return;
	}
}
*/


DBConnection *
tracker_db_connect (void)
{
	char	     *base_dir, *dbname;
	DBConnection *db_con;

	base_dir = g_build_filename (g_get_home_dir (), ".Tracker", "databases", NULL);
	dbname = g_build_filename (base_dir, "data", NULL);

	if (!tracker_file_is_valid (base_dir)) {
		g_mkdir_with_parents (base_dir, 00755);
	}

	g_free (base_dir);


	db_con = g_new (DBConnection, 1);

	if (sqlite3_open (dbname, &db_con->db) != SQLITE_OK) {
		tracker_log ("Fatal Error : Can't open database at %s: %s", dbname, sqlite3_errmsg (db_con->db));
		exit (1);
	}

	g_free (dbname);

	db_con->db_type = DB_DATA;

	sqlite3_busy_timeout (db_con->db, 10000);
	
	db_con->user_data = NULL;

	db_con->statements = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	if (tracker->slow) {
		tracker_db_exec_no_reply (db_con, "PRAGMA synchronous = 1");
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA synchronous = 0");
	}
	tracker_db_exec_no_reply (db_con, "PRAGMA count_changes = 0");

	if (tracker->use_extra_memory) {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 500");
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 100");
	}

	tracker_db_exec_no_reply (db_con, "PRAGMA encoding = \"UTF-8\"");

	/* create user defined functions that can be used in sql */
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "FormatDate", 1, SQLITE_ANY, NULL, &sqlite3_date_to_str, NULL, NULL)) {
		tracker_log ("Function FormatDate failed due to %s", sqlite3_errmsg (db_con->db));
	}
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "GetServiceName", 1, SQLITE_ANY, NULL, &sqlite3_get_service_name, NULL, NULL)) {
		tracker_log ("Function GetServiceName failed due to %s", sqlite3_errmsg (db_con->db));
	}
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "GetServiceTypeID", 1, SQLITE_ANY, NULL, &sqlite3_get_service_type, NULL, NULL)) {
		tracker_log ("Function GetServiceTypeID failed due to %s", sqlite3_errmsg (db_con->db));
	}
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "GetMaxServiceTypeID", 1, SQLITE_ANY, NULL, &sqlite3_get_max_service_type, NULL, NULL)) {
		tracker_log ("Function GetMaxServiceTypeID failed due to %s", sqlite3_errmsg (db_con->db));
	}
	if (SQLITE_OK != sqlite3_create_function (db_con->db, "REGEXP", 2, SQLITE_ANY, NULL, &sqlite3_regexp, NULL, NULL)) {
		tracker_log ("Function REGEXP failed due to %s", sqlite3_errmsg (db_con->db));
	}

	db_con->thread = NULL;

	return db_con;
}


DBConnection *
tracker_db_connect_full_text (void)
{
	gboolean     create_table;
	char	     *base_dir, *dbname;
	DBConnection *db_con;

	create_table = FALSE;

	base_dir = g_build_filename (g_get_home_dir (), ".Tracker", "databases", NULL);
	dbname = g_build_filename (base_dir, "fulltext", NULL);

	if (!tracker_file_is_valid (base_dir)) {
		g_mkdir_with_parents (base_dir, 00755);
	}

	g_free (base_dir);

	if (!g_file_test (dbname, G_FILE_TEST_IS_REGULAR)) {
		tracker_log ("database file %s is not present - will create", dbname);
		create_table = TRUE;
	} else {
		tracker_log ("database file %s is present", dbname);
	}


	db_con = g_new0 (DBConnection, 1);

	if (sqlite3_open (dbname, &db_con->db) != SQLITE_OK) {
		tracker_log ("Fatal Error : Can't open database at %s: %s", dbname, sqlite3_errmsg (db_con->db));
		exit (1);
	}

	g_free (dbname);

	db_con->db_type = DB_BLOB;

	sqlite3_busy_timeout (db_con->db, 10000);

	db_con->statements = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	if (tracker->slow) {
		tracker_db_exec_no_reply (db_con, "PRAGMA synchronous = 1");
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA synchronous = 0");
	}
	tracker_db_exec_no_reply (db_con, "PRAGMA page_size = 8192");
	tracker_db_exec_no_reply (db_con, "PRAGMA count_changes = 0");

	if (tracker->use_extra_memory) {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 256");
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 32");
	}

	tracker_db_exec_no_reply (db_con, "PRAGMA encoding = \"UTF-8\"");

	if (create_table) {
		tracker_db_exec_no_reply (db_con, "CREATE TABLE ServiceContents (ServiceID Int primary key not null, Content Text, ContainsWordScores int, Compressed int default 1)");
		tracker_log ("creating fulltext table");
	}

	sqlite3_create_function (db_con->db, "uncompress", 1, SQLITE_ANY, NULL, &sqlite3_uncompress, NULL, NULL);

	db_con->thread = NULL;

	return db_con;
}

DBConnection *
tracker_db_connect_cache (void)
{
	gboolean     create_table;
	char	     *base_dir, *dbname;
	DBConnection *db_con;

	create_table = FALSE;

	if (!tracker || !tracker->sys_tmp_root_dir) {
		tracker_log ("Fatal Error : system TMP dir for cache set to NULL");
		exit (1);
	}

	base_dir = g_build_filename (tracker->sys_tmp_root_dir, "sqlite3-base", NULL);
	dbname = g_build_filename (base_dir, "cache", NULL);

	if (!tracker_file_is_valid (base_dir)) {
		g_mkdir_with_parents (base_dir, 00755);
	}

	g_free (base_dir);

	if (!tracker_file_is_valid (dbname)) {
		create_table = TRUE;
	}

	db_con = g_new0 (DBConnection, 1);

	if (sqlite3_open (dbname, &db_con->db) != SQLITE_OK) {
		tracker_log ("Fatal Error : Can't open database at %s: %s", dbname, sqlite3_errmsg (db_con->db));
		exit (1);
	}

	g_free (dbname);

	db_con->db_type = DB_CACHE;

	sqlite3_busy_timeout (db_con->db, 10000);

	db_con->statements = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	tracker_db_exec_no_reply (db_con, "PRAGMA synchronous = 0");
	tracker_db_exec_no_reply (db_con, "PRAGMA count_changes = 0");

	if (tracker->use_extra_memory) {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 500");
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 200");
	}


	tracker_db_exec_no_reply (db_con, "PRAGMA encoding = \"UTF-8\"");

	if (create_table) {
		tracker_db_exec_no_reply (db_con, "CREATE TABLE Words (WordID Integer primary key AUTOINCREMENT not null, Word Text, WordCount int)");
		tracker_db_exec_no_reply (db_con, "CREATE TABLE ServiceWords (WordID Int, ServiceID int, ServiceType int, score int, primary key (WordID, ServiceID))");
		tracker_db_exec_no_reply (db_con, "CREATE INDEX  WordWord ON Words (Word)");
		tracker_db_exec_no_reply (db_con, "CREATE INDEX  WordWordCount ON Words (WordCount)");
		tracker_db_exec_no_reply (db_con, "CREATE INDEX  ServiceWordID ON ServiceWords (ServiceID)");
	}

	db_con->thread = NULL;

	return db_con;
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

	lock_file = g_build_filename (g_get_home_dir (), ".Tracker", "tracker.lock", NULL);
	tmp = g_build_filename (g_get_home_dir (), ".Tracker", g_get_host_name (), NULL);
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
			link (lock_file, tmp_file);

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

	tracker_log ("lock failure");
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

	lock_file = g_build_filename (g_get_home_dir (), ".Tracker", "tracker.lock", NULL);
	tmp = g_build_filename (g_get_home_dir (), ".Tracker", g_get_host_name (), NULL);
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


void
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
			tracker_log ("Warning: excessive busy count in query %s and thread %s", "save file contents", db_con->thread);
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

	if (rc != SQLITE_DONE && msg) {
		tracker_log ("WARNING: sql query %s failed because %s", query, msg);
		g_free (msg);
	}

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
			tracker_log ("excessive busy count in query %s and thread %s", query, db_con->thread);
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
		tracker_log ("query %s failed with error : %s", query, msg);
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

	//tracker_log ("totalrows is %d", totalrows);
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
tracker_escape_string (DBConnection *db_con, const char *in)
{

	GString *gs;

	if (!in) {
		return NULL;
	}

	if (!strchr (in, '\'')) {
		return g_strdup (in);
	}

	gs = g_string_new ("");

	for(; *in; in++) {
		if (*in == '\'') {
			g_string_append (gs, "'\\''");
		}
		else {
			g_string_append_c (gs, *in);
		}
	}

	return g_string_free (gs, FALSE);

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
			tracker_log ("ERROR : prepared query %s not found", procedure);
			return NULL;
		} else {
			int rc;

			/* prepare the query */
			rc = sqlite3_prepare (db_con->db, query, -1, &stmt, NULL);

			if (rc == SQLITE_OK && stmt) {
				//tracker_log ("successfully prepared query %s", procedure);
				g_hash_table_insert (db_con->statements, g_strdup (procedure), stmt);
			} else {
				tracker_log ("ERROR : failed to prepare query %s with sql %s due to %s", procedure, query, sqlite3_errmsg (db_con->db));
				return NULL;
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
		tracker_log ("ERROR : incorrect no of paramters %d supplied to %s", param_count, procedure);
	}

	for (i = 0; i < param_count; i++) {
		char *str;

		str = va_arg (args, char *);

		if (!str) {
			tracker_log ("Warning - parameter %d is null when executing SP %s", i, procedure);
			if  (sqlite3_bind_null (stmt, i+1)) {
				tracker_log ("ERROR : null parameter %d could not be bound to %s", i, procedure);
			}
		} else {

			if (sqlite3_bind_text (stmt, i+1, str, strlen (str), SQLITE_TRANSIENT) != SQLITE_OK) {
				tracker_log ("ERROR : parameter %d could not be bound to %s", i, procedure);
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
				tracker_log ("excessive busy count in query %s and thread %s", procedure, db_con->thread);
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

			new_row = g_new (char *, cols+1);
			new_row[cols] = NULL;

			unlock_db ();
			

			for (i = 0; i < cols; i++) {
				const char *st;

				st = (char *) sqlite3_column_text (stmt, i);

				if (st) {
					new_row[i] = g_strdup (st);
					//tracker_log ("%s : row %d, col %d is %s", procedure, row, i, st);
				} else {
					tracker_log ("warning - Null detected in query return result for %s", procedure);
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
		tracker_log ("ERROR : prepared query %s failed due to %s", procedure, sqlite3_errmsg (db_con->db));
	}

	if (!result || (row == 0)) {
		return NULL;
	}

	result = g_slist_reverse (result);

	res = g_new (char *, row+1);
	res[row] = NULL;

	tmp = result;

	for (i = 0; i < row; i++) {
		if (tmp) {
			res[i] = tmp->data;
			tmp = tmp->next;
		} else {
			tracker_log ("WARNING : exec proc has a dud emtry");
		}
	}

	g_slist_free (result);

	return (char ***) res;
}


static void
get_service_id_range (DBConnection *db_con, const char *service, int *min, int *max)
{
	*min = -1;
	*max = -1;

	if (strcmp (service, "Files") == 0) {
		*min = 0;
		*max = 8;
	} else if (strcmp (service, "VFS Files") == 0) {
		*min = 9;
		*max = 17;
	} else {
		*min = tracker_get_id_for_service (service);
		*max = tracker_get_id_for_service (service);
	}
}


void
tracker_db_load_stored_procs (DBConnection *db_con)
{
}


void
tracker_create_db (void)
{
	DBConnection *db_con;
	char	     *sql_file, *query;

	tracker_log ("Creating tracker database...");

	db_con = tracker_db_connect ();

	sql_file = g_strdup (DATADIR "/tracker/sqlite-tracker.sql");

	tracker_log ("Creating tables...");

	if (!g_file_get_contents (sql_file, &query, NULL, NULL)) {
		tracker_log ("Tracker cannot read required file %s - Please reinstall tracker or check read permissions on the file if it exists", sql_file);
		g_assert (FALSE);
	} else {
		char **queries, **queries_p ;

		queries = g_strsplit_set (query, ";", -1);

		for (queries_p = queries; *queries_p; queries_p++) {
			//tracker_log ("creating table %s", *queries_p);
			tracker_db_exec_no_reply (db_con, *queries_p);
			//tracker_log ("Table created");
		}

		tracker_log ("finished creating tables");
		g_strfreev (queries);
		g_free (query);
	}

	g_free (sql_file);

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


gboolean
tracker_db_needs_setup ()
{
	gboolean need_setup;
	char	 *str, *dbname;

	need_setup = FALSE;

	str = g_build_filename (g_get_home_dir (), ".Tracker", NULL);
	dbname = g_build_filename (str, "databases", "data", NULL);


	if (!g_file_test (str, G_FILE_TEST_IS_DIR)) {
		need_setup = TRUE;
		g_mkdir (str, 0700);
	}

	g_free (str);

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
		tracker_log ("FATAL ERROR : Your database is too out of date and will need to be rebuilt and all your files reindexed.\nPlease delete $HOME/.Tracker/databases directory and all files in it and restart trackerd\n");
		exit (1);
	}

	/* apply and table changes for each version update */
/*	while (i < TRACKER_DB_VERSION_REQUIRED) {
		char *sql_file, *query;

		i++;

		sql_file = g_strconcat (DATADIR, "/tracker/tracker-db-table-update", version, ".sql", NULL);

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


static GHashTable *
get_file_contents_words (DBConnection *db_con, guint32 id)
{
	GHashTable	*old_table;
	sqlite3_stmt 	*stmt;
	char		*str_file_id;
	int 		busy_count;
	int		rc;

	old_table = NULL;

	str_file_id = tracker_uint_to_str (id);

	stmt = get_prepared_query (db_con, "GetFileContents");

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
				tracker_log ("excessive busy count in query %s and thread %s", "save file contents", db_con->thread);
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

			old_table = tracker_parse_text (old_table, st, 1);

			continue;
		}

		unlock_db ();
		break;
	}
	unlock_connection (db_con);

	if (rc != SQLITE_DONE) {
		tracker_log ("WARNING: retrieval of text contents has failed");
	}

	return old_table;
}


static GHashTable *
get_indexable_content_words (DBConnection *db_con, guint32 id, GHashTable *table)
{
	char ***res;
	char *str_id;

	str_id = tracker_uint_to_str (id);

	res = tracker_exec_proc (db_con, "GetAllIndexable", 1, str_id);

	if (res) {
		int  k;
		char **row;

		for (k = 0; (row = tracker_db_get_row (res, k)); k++) {

			if (row[0] && row[1]) {
				table = tracker_parse_text (table, row[0], atoi (row[1]));
			}
		}

		tracker_db_free_result (res);
	}


	res = tracker_exec_proc (db_con, "GetAllIndexableBlob", 1, str_id);

	if (res) {
		int  k;
		char **row;

		for (k = 0; (row = tracker_db_get_row (res, k)); k++) {

			if (row[0] && row[1]) {
				table = tracker_parse_text (table, row[0], atoi (row[1]));
			}
		}

		tracker_db_free_result (res);
	}

	g_free (str_id);

	return table;
}


void
tracker_db_save_file_contents (DBConnection *db_con, DBConnection *blob_db_con, DBConnection *cache_db_con, const char *file_name, FileInfo *info)
{
	FILE 		*file;
	char 		buffer[65565];
	int  		bytes_read;
	char		*str_file_id, *value;
	GString 	*str;
	sqlite3_stmt 	*stmt;
	GHashTable 	*new_table;
	char		*compressed;
	int		bytes_compressed;
	int		busy_count;
	int		rc;

	file = g_fopen (file_name, "r");

	if (!file) {
		tracker_log ("Could not open file %s", file_name);
		return;
	}

	str_file_id = g_strdup_printf ("%d", info->file_id);

	str = g_string_new ("");

	value = NULL;

	new_table = NULL;

	bytes_read = 0;

	while (fgets (buffer, 65565, file)) {
		unsigned int buffer_length;

		buffer_length = strlen (buffer);

		if (buffer_length < 3) {
			continue;
		}

		if (!g_utf8_validate (buffer, buffer_length, NULL)) {

			value = g_locale_to_utf8 (buffer, buffer_length, NULL, NULL, NULL);

			if (!value) {
				continue;
			}

			if ((strlen (value) < buffer_length)) {
				g_free (value);
				continue;	
			}

			str = g_string_append (str, value);

			new_table = tracker_parse_text (new_table, value, 1);

			bytes_read += strlen (value);
			g_free (value);

		} else {
			str = g_string_append (str, buffer);
			new_table = tracker_parse_text (new_table, buffer, 1);
			bytes_read += buffer_length;
		}

		/* set upper limit on text we read in to approx 1MB */
		if (bytes_read > tracker->max_index_text_length) {
			break;
		}
	}

	if (bytes_read > 524288) {
			if (!tracker->turbo && !tracker->slow) {
				tracker_db_exec_no_reply (blob_db_con, "PRAGMA synchronous = 1");
			}
	} else {
		tracker->flush_count += bytes_read;
	}

	value = g_string_free (str, FALSE);

	//tracker_log ("text is %s", value);

	fclose (file);


	if (info->is_new) {

		tracker_db_update_indexes_for_new_service (db_con, cache_db_con, info->file_id, info->service_type_id, new_table);

		if (new_table) {
			g_hash_table_destroy (new_table);
		}

	} else {
		GHashTable *old_table;

		/* get old data and compare with new */
		old_table = get_file_contents_words (blob_db_con, info->file_id);

		tracker_db_update_differential_index (cache_db_con, old_table, new_table, str_file_id, info->service_type_id);

		if (new_table) {
			g_hash_table_destroy (new_table);
		}

		if (old_table) {
			g_hash_table_destroy (old_table);
		}
	}


	if (!lock_db ()) {
		if (value) {
			g_free (str_file_id);
			g_free (value);
		}
		return;
	}


	stmt = get_prepared_query (blob_db_con, "SaveFileContents");

	compressed = tracker_compress (value, bytes_read, &bytes_compressed);

	if (compressed) {
		g_debug ("compressed full text size of %d to %d", bytes_read, bytes_compressed);
		g_free (value);
		value = compressed;
		bytes_read = bytes_compressed;
	} else {
		tracker_log ("WARNING: compression of %s has failed", value);
	}

	sqlite3_bind_text (stmt, 1, str_file_id, strlen (str_file_id), SQLITE_STATIC);
	sqlite3_bind_text (stmt, 2, value, bytes_read, SQLITE_STATIC);
	sqlite3_bind_int (stmt, 3, 0);


	busy_count = 0;
	lock_connection (blob_db_con);

	while (TRUE) {

		if (!lock_db ()) {
			if (str_file_id) {
				g_free (str_file_id);
			}

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
				tracker_log ("Warning: excessive busy count in query %s and thread %s", "save file contents", blob_db_con->thread);
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
		tracker_log ("WARNING: Failed to update contents for %s", info->uri);
	}

	g_free (str_file_id);

	if (value) {
		g_free (value);
	}
}


void
tracker_db_clear_temp (DBConnection *db_con)
{
	tracker_db_exec_no_reply (db_con, "DELETE FROM FilePending");
	tracker_db_exec_no_reply (db_con, "DELETE FROM FileWatches");
}


void
tracker_db_start_transaction (DBConnection *db_con)
{
	tracker_db_exec_no_reply (db_con, "BEGIN EXCLUSIVE TRANSACTION");
}


void
tracker_db_end_transaction (DBConnection *db_con)
{
	tracker_db_exec_no_reply (db_con, "END TRANSACTION");
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
	int 		service_type_min, service_type_max, count;
	const GSList	*tmp;

	service_type_min = tracker_get_id_for_service (service);

	if (service_type_min == 0) {
		service_type_max= 9;
	} else {
		service_type_max = service_type_min;
	}

	array = tracker_parse_text_into_array (search_string);

	hit_list = tracker_indexer_get_hits (tracker->file_indexer, array, service_type_min, service_type_max, offset, limit, FALSE, &count);

	g_strfreev (array);


	result = NULL;

	if (!save_results) {
		count = g_slist_length (hit_list);
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
			res = tracker_exec_proc (db_con, "GetFileByID2", 1, str_id);
		} else {
			res = tracker_exec_proc (db_con, "GetFileByID", 1, str_id);
		}

		g_free (str_id);

		if (res) {
			if (res[0] && res[0][0] && res[0][1]) {

				char **row = NULL;

				if (detailed) {
					 if (res[0][2]) {

						row = g_new (char *, 4);

						row[0] = g_strdup (res[0][0]);
						row[1] = g_strdup (res[0][1]);
						row[2] = g_strdup (res[0][2]);
						row[3] = NULL;
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

	tracker_index_free_hit_list (hit_list);

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
		tracker_log ("metadata not found for type %s", field);
		return NULL;
	}

	switch (def->type) {

		case 0: res = tracker_exec_proc (db_con, "SearchMetadataIndex", 2, def->id, text); break;
		case 1: res = tracker_exec_proc (db_con, "SearchMetadataString", 2, def->id, text); break;
		case 2: res = tracker_exec_proc (db_con, "SearchMetadataNumeric", 2, def->id, text); break;
		case 3: res = tracker_exec_proc (db_con, "SearchMetadataNumeric", 2, def->id, text); break;
		default: tracker_log ("Error: metadata could not be retrieved as type %d is not supported", def->type); res = NULL;
	}

	tracker_db_free_field_def (def);

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
		tracker_log ("metadata not found for id %s and type %s", id, key);
		return NULL;
	}

	switch (def->type) {

		case 0: res = tracker_exec_proc (db_con, "GetMetadataIndex", 2, id, key); break;
		case 1: res = tracker_exec_proc (db_con, "GetMetadataString", 2, id, key); break;
		case 2: res = tracker_exec_proc (db_con, "GetMetadataNumeric", 2, id, key); break;
		case 3: res = tracker_exec_proc (db_con, "GetMetadataNumeric", 2, id, key); break;
		case 4: res = tracker_exec_proc (db_con, "GetMetadataBlob", 2, id, key); break;
		case 99: res = tracker_exec_proc (db_con, "GetMetadataIndex", 2, id, key); break;
		default: tracker_log ("Error: metadata could not be retrieved as type %d is not supported", def->type); res = NULL;
	}

	tracker_db_free_field_def (def);

	return res;
}


static void
update_file_index (DBConnection *db_con, const char *id, const char *service, const char *meta_name, const char *meta_value)
{
	int	   weight;
	char	   ***res;
	GHashTable *old_table, *new_table;

	weight = -1;

	/* get meta info for metadata type */
	res = tracker_exec_proc (db_con, "GetMetadataTypeInfo", 1, meta_name);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0] && row[1] && row[2] && row[3] && row[4]) {
			weight = atoi (row[4]);
		} else {
			tracker_db_free_result (res);
			tracker_log ("Error : Cannot find details for metadata type %s", meta_name);
			return;
		}

		tracker_db_free_result (res);
	} else {
		tracker_log ("Error : Cannot find details for metadata type %s", meta_name);
		return;
	}

	if (weight == -1) {
		tracker_log ("Error : Cannot find details for metadata type %s", meta_name);
		return;
	}

	old_table = NULL;
	new_table = NULL;

	/* get existing metadata for that field if any and parse it */
	res = tracker_db_get_metadata (db_con, service, id, meta_name);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			old_table = tracker_parse_text (old_table, row[0], weight);
		}

		tracker_db_free_result (res);
	}

	/* parse new metadata value */
	new_table = tracker_parse_text ( new_table, meta_value, weight);

	/* we only do differential updates so only changed words scores are updated */
	if (new_table) {
		int sid;

		sid = tracker_get_id_for_service (service);
		if (!db_con->user_data) {
			tracker_log ("WARNING: Cache Database not found");
		} else {
			tracker_db_update_differential_index (db_con->user_data, old_table, new_table, id, sid);
		}
		g_hash_table_destroy (new_table);
	}

	if (old_table) {
		g_hash_table_destroy (old_table);
	}
}


void
tracker_db_set_metadata (DBConnection *db_con, const char *service, const char *id, const char *key, const char *value, gboolean overwrite, gboolean index, gboolean embedded)
{
	FieldDef   *def;
	const char *str_embedded;

	g_return_if_fail (id);

	def = tracker_db_get_field_def (db_con, key);

	if (!def) {
		return;
	}

	if (!def->writeable && !overwrite) {
		tracker_db_free_field_def (def);
		return;
	}

	if (embedded) {
		str_embedded = "1";
	} else {
		str_embedded = "0";
	}

	switch (def->type) {

		case 0:
			if (index) {
				update_file_index (db_con, id, service, key, value);
			}

			tracker_exec_proc (db_con, "SetMetadataIndex", 4, id, def->id, value, str_embedded);

			break;

		case 1: tracker_exec_proc (db_con, "SetMetadataString", 4, id, def->id, value, str_embedded);  break;
		case 2: tracker_exec_proc (db_con, "SetMetadataNumeric", 4, id, def->id, value, str_embedded); break;
		case 3: tracker_exec_proc (db_con, "SetMetadataNumeric", 4, id, def->id, value, str_embedded); break;
		case 4:
			if (index) {
				update_file_index (db_con, id, service, key, value);
			}

			tracker_exec_proc (db_con, "SetMetadataBlob", 4, id, def->id, value, str_embedded);

			break;

		default: tracker_log ("Error: metadata could not be set as type %d for metadata %s is not supported", def->type, key);
	}

	tracker_db_free_field_def (def);
}


void
tracker_db_update_keywords (DBConnection *db_con, const char *service, const char *id, const char *value)
{
	FieldDef *def;

	g_return_if_fail (id);

	def = tracker_db_get_field_def (db_con, "Keywords");

	if (!def) {
		return;
	}

	update_file_index (db_con, id, service, "Keywords", value);

	tracker_exec_proc (db_con, "SetMetadataIndex", 4, id, def->id, value, "0");

	tracker_db_free_field_def (def);
}


void
tracker_db_create_service (DBConnection *db_con, const char *path, const char *name, const char *service, const char *mime, guint32 filesize, gboolean is_dir, gboolean is_link, int offset, guint32 mtime)
{
	char	   ***res;
	int	   i;
	char	   *sid;
	char	   *str_mtime;
	const char *str_is_dir, *str_is_link;
	char	   *str_filesize, *str_offset;
	int	   service_type_id;
	char	   *str_service_type_id;

	/* get a new unique ID for the service - use mutex to prevent race conditions */

	g_mutex_lock (sequence_mutex);

	res = tracker_exec_proc (db_con, "GetNewID", 0);

	if (!res || !res[0] || !res[0][0]) {
		g_mutex_unlock (sequence_mutex);
		tracker_log ("ERROR : could not create service - GetNewID failed");
		return;
	}

	i = atoi (res[0][0]);
	i++;

	sid = tracker_int_to_str (i);
	tracker_exec_proc (db_con, "UpdateNewID",1, sid);
	g_mutex_unlock (sequence_mutex);

	tracker_db_free_result (res);

	str_mtime = g_strdup_printf ("%d", mtime);

	if (is_dir) {
		str_is_dir = "1";
	} else {
		str_is_dir = "0";
	}

	if (is_link) {
		str_is_link = "1";
	} else {
		str_is_link = "0";
	}

	str_offset = tracker_uint_to_str (offset);
	str_filesize = tracker_uint_to_str (filesize);

	service_type_id = tracker_get_id_for_service (service);
	str_service_type_id = tracker_int_to_str (service_type_id);

	if (service_type_id != -1) {
		tracker_exec_proc (db_con, "CreateService", 10, sid, path, name, str_service_type_id, mime, str_filesize, str_is_dir, str_is_link, str_offset, str_mtime);
	}

	g_free (str_service_type_id);
	g_free (sid);
	g_free (str_filesize);
	g_free (str_mtime);
	g_free (str_offset);
}


static void
delete_index_data (gpointer key,
		   gpointer value,
		   gpointer user_data)
{
	char	*word;
	guint32	id;

	word = (char *) key;

	id = GPOINTER_TO_UINT (user_data);

	tracker_indexer_update_word (tracker->file_indexer, word, id, 0, 1, TRUE);
}


static void
delete_index_for_service (DBConnection *db_con, DBConnection *blob_db_con, guint32 id)
{
	GHashTable *table;
	char	   *str_file_id;

	str_file_id = tracker_uint_to_str (id);

	table = get_file_contents_words (blob_db_con, id);

	tracker_exec_proc (blob_db_con, "DeleteFileContents", 1, str_file_id);

	g_free (str_file_id);

	table = get_indexable_content_words (db_con, id, table);

	if (table) {
		g_hash_table_foreach (table, delete_index_data, GUINT_TO_POINTER (id));
		g_hash_table_destroy (table);
	}

}


void
tracker_db_delete_file (DBConnection *db_con, DBConnection *blob_db_con, guint32 file_id)
{
	char *str_file_id;

	delete_index_for_service (db_con, blob_db_con, file_id);

	str_file_id = tracker_uint_to_str (file_id);

	tracker_db_start_transaction (db_con);
	tracker_exec_proc (db_con, "DeleteFile1", 1, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile2", 1, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile3", 1, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile4", 2, str_file_id, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile5", 1, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile6", 1, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile7", 1, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile8", 1, str_file_id);

	if (db_con->user_data) {
		tracker_exec_proc (db_con->user_data, "DeleteFile9", 1, str_file_id);
	} else {
		tracker_log ("WARNING: Cache DB not found");
	}

	tracker_db_end_transaction (db_con);

	g_free (str_file_id);
}


void
tracker_db_delete_directory (DBConnection *db_con, DBConnection *blob_db_con, guint32 file_id, const char *uri)
{
	char ***res;
	char *str_file_id, *uri_prefix;

	str_file_id = tracker_uint_to_str (file_id);

	uri_prefix = g_strconcat (uri, G_DIR_SEPARATOR_S, "*", NULL);

	delete_index_for_service (db_con, blob_db_con, file_id);

	/* get all file id's for all files recursively under directory amd delete indexes for them */
	res = tracker_exec_proc (db_con, "SelectSubFileIDs", 2, uri, uri_prefix);

	if (res) {
		char **row;
		int  i;

		for (i = 0; (row = tracker_db_get_row (res, i)); i++) {
			if (row[0]) {
				int id;

				id = atoi (row[0]);
				delete_index_for_service (db_con, blob_db_con, id);
				if (db_con->user_data) {
					tracker_exec_proc (db_con->user_data, "DeleteServiceWordForID", 1, row[0]);
				}
				
			}
		}

		tracker_db_free_result (res);
	}

	tracker_db_start_transaction (db_con);
	tracker_exec_proc (db_con, "DeleteDirectory1", 2, uri, uri_prefix);
	tracker_exec_proc (db_con, "DeleteDirectory2", 2, uri, uri_prefix);
	tracker_exec_proc (db_con, "DeleteDirectory3", 2, uri, uri_prefix);
	tracker_exec_proc (db_con, "DeleteDirectory4", 2, uri, uri_prefix);
	tracker_exec_proc (db_con, "DeleteDirectory5", 2, uri, uri_prefix);
	tracker_exec_proc (db_con, "DeleteDirectory6", 2, uri, uri_prefix);
	tracker_exec_proc (db_con, "DeleteDirectory7", 2, uri, uri_prefix);
	
	tracker_exec_proc (db_con, "DeleteFile1", 1, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile2", 1, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile3", 1, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile4", 2, str_file_id, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile5", 1, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile6", 1, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile7", 1, str_file_id);
	tracker_exec_proc (db_con, "DeleteFile8", 1, str_file_id);
	if (db_con->user_data) {
		tracker_exec_proc (db_con->user_data, "DeleteFile9", 1, str_file_id);
	}
	tracker_db_end_transaction (db_con);

	g_free (uri_prefix);
	g_free (str_file_id);
}


void
tracker_db_update_file (DBConnection *db_con, guint32 file_id, guint32 mtime)
{
	char *str_file_id;
	char *str_mtime;

	str_file_id = tracker_uint_to_str (file_id);
	str_mtime = tracker_int_to_str (mtime);

	tracker_exec_proc (db_con, "UpdateFile", 2, str_mtime, str_file_id);

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

	res = tracker_exec_proc (db_con, "ExistsPendingFiles", 0);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			int pending_file_count;

			pending_file_count = atoi (row[0]);
			g_debug ("%d files are pending with count %s", pending_file_count, row[0]);
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

	res = tracker_exec_proc (db_con, "CountPendingMetadataFiles", 0);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			int pending_file_count;

			pending_file_count = atoi (row[0]);
			g_debug ("metadata queue has %d rows pending", atoi (row[0]));
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

	tracker_db_start_transaction (db_con);
	tracker_db_exec_no_reply (db_con, "DELETE FROM FileTemp");
	str = g_strconcat ("INSERT INTO FileTemp (ID, FileID, Action, FileUri, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID) SELECT ID, FileID, Action, FileUri, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM FilePending WHERE (PendingDate < ", time_str, ") AND (Action <> 20) LIMIT 250", NULL);
	tracker_db_exec_no_reply (db_con, str);
	tracker_db_exec_no_reply (db_con, "DELETE FROM FilePending WHERE ID IN (SELECT ID FROM FileTemp)");
	tracker_db_end_transaction (db_con);

	g_free (str);
	g_free (time_str);

	return tracker_exec_sql (db_con, "SELECT FileID, FileUri, Action, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM FileTemp ORDER BY ID");
}


void
tracker_db_remove_pending_files (DBConnection *db_con)
{
	tracker_db_exec_no_reply (db_con, "DELETE FROM FileTemp");
}


char ***
tracker_db_get_pending_metadata (DBConnection *db_con)
{
	const char *str;

	if (!tracker->is_running) {
		return NULL;
	}

	str = "INSERT INTO MetadataTemp (ID, FileID, Action, FileUri, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID) SELECT ID, FileID, Action, FileUri, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM FilePending WHERE Action = 20 LIMIT 250";

	tracker_db_start_transaction (db_con);
	tracker_db_exec_no_reply (db_con, "DELETE FROM MetadataTemp");
	tracker_db_exec_no_reply (db_con, str);
	tracker_db_exec_no_reply (db_con, "DELETE FROM FilePending WHERE ID IN (SELECT ID FROM MetadataTemp)");
	tracker_db_end_transaction (db_con);

	return tracker_exec_sql (db_con, "SELECT FileID, FileUri, Action, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM MetadataTemp ORDER BY ID");
}


unsigned int
tracker_db_get_last_id (DBConnection *db_con)
{
	return sqlite3_last_insert_rowid (db_con->db);
}


void
tracker_db_remove_pending_metadata (DBConnection *db_con)
{
	tracker_db_exec_no_reply (db_con, "DELETE FROM MetadataTemp");
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
		tracker_exec_proc (db_con, "InsertPendingFile", 10, id, action, time_str, uri, mime, "1", str_new, "1", "1", str_service_type_id);
	} else {
		tracker_exec_proc (db_con, "InsertPendingFile", 10, id, action, time_str, uri, mime, "0", str_new, "1", "1", str_service_type_id);
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

	tracker_exec_proc (db_con, "UpdatePendingFile", 3, time_str, action, uri);

	g_free (time_str);
}


char ***
tracker_db_get_files_by_service (DBConnection *db_con, const char *service, int offset, int limit)
{
	int  min, max;
	char *str_limit, *str_offset;
	char *str_min, *str_max;
	char ***res;

	str_limit = tracker_int_to_str (limit);
	str_offset = tracker_int_to_str (offset);

	get_service_id_range (db_con, service, &min, &max);

	str_min = tracker_int_to_str (min);
	str_max = tracker_int_to_str (max);

	res = tracker_exec_proc (db_con, "GetFilesByServiceType", 4, str_min, str_max, str_offset, str_limit);

	g_free (str_min);
	g_free (str_max);
	g_free (str_offset);
	g_free (str_limit);

	return res;
}


char ***
tracker_db_get_files_by_mime (DBConnection *db_con, char **mimes, int n, int offset, int limit, gboolean vfs)
{
	int	i, min, max;
	char	***res;
	char	*query;
	GString	*str;

	g_return_val_if_fail (mimes, NULL);

	if (vfs) {
		min = 9;
		max = 17;
	} else {
		min = 0;
		max = 8;
	}

	str = g_string_new ("SELECT  DISTINCT F.Path || '/' || F.Name AS uri FROM Services F INNER JOIN ServiceIndexMetaData M ON F.ID = M.ServiceID WHERE M.MetaDataID = (SELECT ID FROM MetaDataTypes WHERE MetaName ='File.Format') AND (M.MetaDataValue IN ");

	g_string_append_printf (str, "('%s'", mimes[0]);

	for (i = 1; i < n; i++) {
		g_string_append_printf (str, ", '%s'", mimes[i]);
	}

	g_string_append_printf (str, ")) AND (F.ServiceTypeID BETWEEN %d AND %d) LIMIT %d,%d", min, max, offset, limit);

	query = g_string_free (str, FALSE);

	tracker_log ("getting files with mimes using sql %s", query);

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

	array = tracker_parse_text_into_array ( text);

	hit_list = tracker_indexer_get_hits (tracker->file_indexer, array, 0, 9, 0, 999999, FALSE, &count);

	g_strfreev (array);

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

						g_debug ("hit is %s", row[1]);

						result_list = g_slist_prepend (result_list, row);

						count++;

						break;
					}
				}
			}

			tracker_db_free_result (res);
		}

		if (count > 511) {
			break;
		}
	}

	tracker_index_free_hit_list (hit_list);

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

	array = tracker_parse_text_into_array ( text);

	hit_list = tracker_indexer_get_hits (tracker->file_indexer, array, 0, 9, 0, 999999, FALSE, &count);

	g_strfreev (array);

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

		if (count > 511) {
			break;
		}
	}

	g_free (location_prefix);

	tracker_index_free_hit_list (hit_list);

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

	array = tracker_parse_text_into_array ( text);

	hit_list = tracker_indexer_get_hits (tracker->file_indexer, array, 0, 9, 0, 999999, FALSE, &count);

	g_strfreev (array);

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

		if (count > 511) {
			break;
		}
	}

	g_free (location_prefix);

	tracker_index_free_hit_list (hit_list);

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

	res = tracker_exec_proc (db_con, "GetSubWatches", 1, folder);

	g_free (folder);

	return res;
}


char ***
tracker_db_delete_sub_watches (DBConnection *db_con, const char *dir)
{
	char *folder;
	char ***res;

	folder = g_strconcat (dir, G_DIR_SEPARATOR_S, "*", NULL);

	res = tracker_exec_proc (db_con, "DeleteSubWatches", 1, folder);

	g_free (folder);

	return res;
}


void
tracker_db_update_file_move (DBConnection *db_con, guint32 file_id, const char *path, const char *name, guint32 mtime)
{
	char *str_file_id, *index_time;

	str_file_id = g_strdup_printf ("%d", file_id);
	index_time = g_strdup_printf ("%d", mtime);

	tracker_exec_proc (db_con, "UpdateFileMove", 4, path, name, index_time, str_file_id);

	g_free (str_file_id);
	g_free (index_time);
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


typedef struct {
	guint32		service_id;
	int		service_type_id;
	DBConnection 	*db_con;
} ServiceTypeInfo;



static inline void
add_word_to_hash (const char *word, int id, int count)
{
		CacheWord *cache_word = g_slice_new (CacheWord);

		cache_word->id = id;
		cache_word->count = count;
		g_mutex_lock (tracker->cached_word_table_mutex);
		g_hash_table_insert (tracker->cached_word_table, g_strdup (word), cache_word);
		g_mutex_unlock (tracker->cached_word_table_mutex);
}

static char *
cache_word_exists (DBConnection *db_con, const char *word) 
{
	char ***res;
	char *word_id = NULL;
	int  count = 1, id;

	if (tracker->use_extra_memory) {
		CacheWord *cache_word;

		g_mutex_lock (tracker->cached_word_table_mutex);
		cache_word = (g_hash_table_lookup (tracker->cached_word_table, word));
		g_mutex_unlock (tracker->cached_word_table_mutex);

		if (cache_word) {
			cache_word->count++;
			return tracker_int_to_str (cache_word->id);
		}
	}

	res = tracker_exec_proc (db_con, "GetWordID", 1, word);

	if (res) {

		if (res[0] && res[0][0]) {
			word_id = g_strdup (res[0][0]);
			id = atoi (res[0][0]);

			if (res[0][1]) {
				count = atoi (res[0][1]);
			}
		}

		tracker_db_free_result (res);
	}

	if (word_id) {

		count ++;

		if (tracker->use_extra_memory) {
			add_word_to_hash (word, id, count);

		} else {
			char *str_count = tracker_uint_to_str (count);

			tracker_exec_proc (db_con, "UpdateWordCount", 2, str_count, word_id);

			g_free (str_count);
		}

		return word_id;

	} else {
		tracker_exec_proc (db_con, "InsertWord", 1, word);
		tracker->number_of_cached_words++;
	}

	id = tracker_db_get_last_id (db_con);

	if (tracker->use_extra_memory) {
			add_word_to_hash (word, id, count);
	}

	return tracker_int_to_str (id);
}


static void
append_cache_word (DBConnection *db_con, const char *word, guint32 service_id, int service_type, int score) 
{
	char *word_id, *str_service_id, *str_service_type, *str_score;

	word_id = cache_word_exists (db_con, word);

	str_service_id = tracker_uint_to_str (service_id);
	str_service_type = tracker_int_to_str (service_type);
	str_score = tracker_int_to_str (score);

	tracker_exec_proc (db_con, "InsertServiceWord", 4, word_id, str_service_id, str_service_type, str_score);			

	g_free (word_id);
	g_free (str_service_id);
	g_free (str_service_type);
	g_free (str_score);
}


static void
update_cache_word (DBConnection *db_con, const char *word, guint32 service_id, int score) 
{
	char *str_service_id, *str_score;

	str_service_id = tracker_uint_to_str (service_id);
	str_score = tracker_int_to_str (score);

	tracker_exec_proc (db_con, "UpdateServiceWord", 3, str_score, word, str_service_id);			

	g_free (str_service_id);
	g_free (str_score);
}


static void
update_counts	 (gpointer key,
		  gpointer value,
		  gpointer user_data)
{
	DBConnection *db_con = user_data;
	CacheWord *cache_word = (CacheWord *) value;

	if (cache_word) {
		char *str_count = tracker_int_to_str (cache_word->count);
		char *str_id  =  tracker_int_to_str (cache_word->id);

		tracker_exec_proc (db_con, "UpdateWordCount", 2, str_count, str_id);

		g_free (str_count);
		g_free (str_id);

		g_slice_free (CacheWord, cache_word);

		cache_word = NULL;

	}
}


int
tracker_db_flush_words_to_qdbm (DBConnection *db_con, int limit) 
{

	char ***res = NULL, ***res2 = NULL, ***res3 = NULL;
	char *str_limit = tracker_int_to_str (limit);
	int result = 0, rc, count = 0;
	WordDetails *word_details;

	if (tracker->use_extra_memory) {
		/* update word counts */
		g_mutex_lock (tracker->cached_word_table_mutex);
		g_hash_table_foreach (tracker->cached_word_table, update_counts, db_con);
		g_hash_table_destroy (tracker->cached_word_table);
		tracker->cached_word_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		g_mutex_unlock (tracker->cached_word_table_mutex);
	}

	res = tracker_exec_proc (db_con, "GetWordsTop", 1, str_limit);
	
	g_free (str_limit);

	if (res) {
		int  k;
		char **row;
		char *word_id, *word;

		tracker_db_start_transaction (db_con);

		for (k = 0; (row = tracker_db_get_row (res, k)); k++) {

			if (row[0] && row[1]) {

				word_id = row[0];
				word = row[1];

				res2 = tracker_exec_proc (db_con, "GetServiceWordCount", 1, word_id);
				if (res2) {
					if (res2[0] && res2[0][0]) {
						count = atoi(res2[0][0]);
					} 

					tracker_db_free_result (res2);
				}

				if (count != 0) {
					sqlite3_stmt *stmt;

					word_details = g_malloc (sizeof (WordDetails) * count);
	
					stmt = get_prepared_query (db_con, "GetServiceWord");
				
					if (sqlite3_bind_text (stmt, 1, word_id, strlen (word_id), SQLITE_TRANSIENT) != SQLITE_OK) {
						tracker_log ("ERROR : parameter %d could not be bound to %s", 1, "GetServiceWord");
					}

				
					int i = 0;

					lock_connection (db_con);
					while (TRUE) {

						
						rc = sqlite3_step (stmt);
						

						if (rc == SQLITE_BUSY) {
							unlock_connection (db_con);
							g_usleep (100);
							lock_connection (db_con);
							continue;
						}

						if (rc == SQLITE_ERROR || rc == SQLITE_MISUSE) {
							tracker_log ("Error while executing SP GetServiceWord");
							break;
						}

						if (rc == SQLITE_DONE) {
							break;
						}

						if (rc == SQLITE_ROW) {
							guint32 service_id;
							int service_type, score;

							service_id = (guint32) sqlite3_column_int (stmt, 0);
							service_type = sqlite3_column_int (stmt, 1);
							score = sqlite3_column_int (stmt, 2);
						
							word_details[i].id = service_id;
							word_details[i].amalgamated = tracker_indexer_calc_amalgamated (service_type, score);

							i++;
						}

					}
					unlock_connection (db_con);

					if (i == count) {
						tracker_indexer_append_word_chunk (tracker->file_indexer, word, word_details, count);
						tracker->update_count++;
					} else {
						tracker_log ("flush failure - ignoring suspect data");
					}

					g_free (word_details);
				
				}	

				tracker_exec_proc (db_con, "DeleteServiceWords", 1, word_id);
				tracker_exec_proc (db_con, "DeleteWord", 1, word_id);

			} 
				
		}
		tracker_db_end_transaction (db_con);
		tracker_db_free_result (res);
	}
	
	res3 = tracker_exec_proc (db_con, "GetWordCount", 0);
	if (res3) {
		if (res3[0] && res3[0][0]) {
			result = atoi(res3[0][0]);
		} 

		tracker_db_free_result (res3);
	}

	return result;
		

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
	//db_con = info->db_con;

	if (score != 0) {
		/* cache word update */
		append_cache_word (info->db_con, word, info->service_id, info->service_type_id, score);

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
	char ***res;
	gboolean is_cached = FALSE;

	word = (char *) key;
	score = GPOINTER_TO_INT (value);
	info = user_data;
	char *str_service_id = tracker_uint_to_str (info->service_id);

	if (score != 0) {

		res = tracker_exec_proc (info->db_con, "ServiceCached", 2, str_service_id, word);

		g_free (str_service_id);

		if (res) {
			if (res[0] && res[0][0]) {
				if (res[0][0][0] ==  '1') {
					is_cached = TRUE;					
				}
			} 
	
			tracker_db_free_result (res);
		}

		if (!is_cached) {
			tracker_indexer_update_word (tracker->file_indexer, word, info->service_id, info->service_type_id, score, FALSE);
		} else {
			update_cache_word (info->db_con, word, info->service_id, score);
		}
	}
}


void
tracker_db_update_indexes_for_new_service (DBConnection *db_con, DBConnection *cache_db_con, guint32 service_id, int service_type_id, GHashTable *table)
{
	table = get_indexable_content_words (db_con, service_id, table);

	if (table) {
		ServiceTypeInfo *info;

		info = g_new (ServiceTypeInfo, 1);

		info->service_id = service_id;
		info->service_type_id = service_type_id;
		info->db_con = cache_db_con;

		tracker_db_start_transaction (db_con);
		g_hash_table_foreach (table, append_index_data, info);
		tracker_db_end_transaction (db_con);
		g_free (info);
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
	int	   lookup_score;

	word = (char *) key;
	score = GPOINTER_TO_INT (value);
	new_table = user_data;

	lookup_score = GPOINTER_TO_INT (g_hash_table_lookup (new_table, word));

	//g_debug ("word %s has old score %d and new score %d so updating with total score %d", word, score, lookup_score, lookup_score-score);

	/* subtract scores so only words with score != 0 are updated (when score is zero, old word score is same as new word so no updating necessary)
	   negative scores mean either word exists in old but no new data or has a lower score in new than old */
	g_hash_table_insert (new_table, g_strdup (word), GINT_TO_POINTER (lookup_score - score));
}


void
tracker_db_update_differential_index (DBConnection *db_con, GHashTable *old_table, GHashTable *new_table, const char *id, int service_type_id)
{
	ServiceTypeInfo *info;

	g_return_if_fail (new_table || id || service_type_id > -1);

	if (!new_table) {
		new_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
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
	int  smin, smax;
	char *str_min, *str_max;
	char ***res;

	smin = tracker_get_id_for_service (service);

	if (smin == 0) {
		smax = 8;
	} else {
		smax = smin;
	}

	str_min = tracker_int_to_str (smin);
	str_max = tracker_int_to_str (smax);

	res = tracker_exec_proc (db_con, "GetKeywordList", 2, str_min, str_max);

	g_free (str_min);
	g_free (str_max);

	return res;
}
