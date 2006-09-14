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

#include "tracker-db-sqlite.h"
#include "tracker-indexer.h"


extern Tracker *tracker;

static GHashTable *prepared_queries;
static GMutex *sequence_mutex;

gboolean use_nfs_safe_locking = FALSE;


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
		g_free (def);
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
	char **row;
	char *str = NULL, *tmp = NULL, *value;
	
	if (!result) {
		return;
	}

	for (rows = result; *rows; rows++) {
		if (*rows) {
			for (row = *rows; *row; row++) {
				value = *row;
				if (!value) {
					value = "NULL";
				}
				if (str) {
					tmp = g_strdup (str);
					g_free (str);
					str = g_strconcat (tmp, ", ", value, NULL);
					g_free (tmp);
				} else {
					str = g_strconcat (value, NULL);
				} 	
			}
			tracker_log (str);
			g_free (str);
			str = NULL;
			
		}
	}


}

void
tracker_db_free_result (char ***result)
{
	char ***rows;

	if (!result ||  !tracker->is_running ) {
		return;
	}

	for (rows = result; *rows; rows++) {
		if (*rows) {
			g_strfreev  (*rows);
		}
	}

	g_free (result);

}


int
tracker_get_row_count (char ***result)
{
	char ***rows;
	int i;

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
	char **p;
	int i;

	if (!result || (tracker_get_row_count == 0)) {
		return 0;
	}

	i = 0;

	char **row = tracker_db_get_row (result, 0);

	if (!row) {
		return 0;
	}

	for (p = row; *p; p++) {
		i++;			
	}

	return i;

}


gboolean
tracker_db_initialize (const char *datadir)
{

	FILE 	*file;
	char 	buffer[8192];

	tracker_log ("Using Sqlite version %s", sqlite3_version);

	sequence_mutex = g_mutex_new ();

	/* load prepared queries */
	prepared_queries = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	tracker_log ("Loading prepared queries...");

	char *sql_file = g_strdup (DATADIR "/tracker/sqlite-stored-procs.sql");
	
	if (!g_file_test (sql_file, G_FILE_TEST_EXISTS)) {
		tracker_log ("Tracker cannot read required file %s - Please reinstall tracker or check read permissions on the file if it exists", sql_file); 
		g_assert (FALSE);
	}

	file = fopen (sql_file, "r");
	g_free (sql_file);

	while (feof (file) == 0)  {
	
		fgets (buffer, 8192, file);

		if (strlen (buffer) < 5) {
			continue;
		}

		char *sep = strchr (buffer, ' ');
				
		sep = strchr (buffer, ' ');
					  			
		if (sep) {
			*sep = '\0';

			char *query = g_strdup (sep + 1);
			char *name = g_strdup (buffer);					

			//tracker_log ("installing query %s with sql %s", name, query);
			g_hash_table_insert (prepared_queries, name, query);

		} else {
			continue;
		}


	}
	fclose (file);


	return TRUE;
}


void
tracker_db_thread_init ()
{
	return;
}


void
tracker_db_thread_end ()
{
	//sqlite3_thread_cleanup ();
}


void
tracker_db_finalize ()
{
	return;

}


static void
finalize_statement (gpointer key,
   		   gpointer value,
		   gpointer user_data)
{
	sqlite3_stmt *stmt = value;
	DBConnection *db_con = user_data;

	if (key && stmt) {
		if (sqlite3_finalize (stmt)!= SQLITE_OK) {
			tracker_log ("Error statement could not be finalized for %s with error %s", (char *)key, sqlite3_errmsg (db_con->db));
		
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

//	sqlite3_interrupt (db_con->db);

	/* clear prepared queries */
	if (db_con->statements) {
		g_hash_table_foreach (db_con->statements, finalize_statement, db_con);
	}

	g_mutex_free (db_con->write_mutex);

	g_hash_table_destroy (db_con->statements);

	if (sqlite3_close (db_con->db) != SQLITE_OK) {
		tracker_log ("ERROR : Database close operation failed for thread %s due to %s", db_con->thread, sqlite3_errmsg (db_con->db));
	} else {
		tracker_log ("Database closed for thread %s", db_con->thread);
	}

}




DBConnection *
tracker_db_connect ()
{
	DBConnection *db_con = g_new (DBConnection, 1);

	char *dbname = g_build_filename (g_get_home_dir (), ".Tracker", "database", NULL);


	if (sqlite3_open (dbname, &db_con->db) != SQLITE_OK) {
		tracker_log ("Fatal Error : Can't open database at %s: %s", dbname, sqlite3_errmsg (db_con->db));
		exit (1);
	}

	g_free (dbname);

	sqlite3_busy_timeout( db_con->db, 10000);

	db_con->statements = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	db_con->write_mutex = g_mutex_new ();

	tracker_exec_sql (db_con, "PRAGMA synchronous = OFF");
	tracker_exec_sql (db_con, "PRAGMA count_changes = 0");

	db_con->thread = NULL;
	
	return db_con;
}


/* get no of links to a file - used for safe NFS atomic file locking */
static int 
get_nlinks (const char *name)
{
	struct stat st;

	if (stat( name, &st) == 0) {

		return st.st_nlink;
	} else {
     		return -1;
    	}
}


static int 
get_mtime (const char *name)
{
	struct stat st;

	if (stat( name, &st) == 0) {

		return st.st_mtime;
	} else {
     		return -1;
    	}
}



/* serialises db access via a lock file for safe use on (lock broken) NFS mounts */
static gboolean 
lock_db ()
{
	int attempt, fd;
	char *lock_file, *tmp, *tmp_file;
	
	if (!use_nfs_safe_locking) {
		return TRUE;
	}

	lock_file = g_build_filename (g_get_home_dir (), ".Tracker", "tracker.lock", NULL);
	tmp = g_build_filename (g_get_home_dir (), ".Tracker", g_get_host_name (), NULL);
	tmp_file = g_strdup_printf ("%s_%ld.lock", tmp, (long)getpid ());
	g_free (tmp);

	for ( attempt=0; attempt < 10000; ++attempt) {
		
		/* delete existing lock file if older than 5 mins */
		if (g_file_test (lock_file, G_FILE_TEST_EXISTS) && ( time((time_t *)NULL) - get_mtime (lock_file)) > 300) {
			unlink (lock_file); 
		}

		fd = open (lock_file, O_CREAT|O_EXCL, 0644);
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
unlock_db ()
{		
	char *lock_file, *tmp, *tmp_file;

	if (!use_nfs_safe_locking) {
		return;
	}

	lock_file = g_build_filename (g_get_home_dir (), ".Tracker", "tracker.lock", NULL);
	tmp = g_build_filename (g_get_home_dir (), ".Tracker", g_get_host_name (), NULL);
	tmp_file = g_strdup_printf ("%s_%ld.lock", tmp, (long)getpid ());
	g_free (tmp);

	unlink (tmp_file);
	unlink (lock_file);	

	g_free (tmp_file);
	g_free (lock_file);

}




void
tracker_db_prepare_queries (DBConnection *db_con)
{
	
	return;

}




char ***
tracker_exec_sql (DBConnection *db_con, const char *query)
{
	char **array = NULL;
	char **result = NULL;
	int cols, rows;
	char *msg;
	int busy_count = 0;

	g_return_val_if_fail (query, NULL);

	if (!lock_db ()) {
		return NULL;
	}

	int i =  sqlite3_get_table (db_con->db, query, &array, &rows, &cols, &msg);

	while (i == SQLITE_BUSY) {
		unlock_db ();
		
		busy_count++;

		if (busy_count > 1000) {
			tracker_log ("excessive busy count in query %s and thread %s", query, db_con->thread);
			exit(1);
		}

		if (busy_count > 50) {
			g_usleep (g_random_int_range (1000, (busy_count * 200) ));
		} else {
			g_usleep (100);
		}
		lock_db ();
		i = sqlite3_get_table (db_con->db, query, &array, &rows, &cols, &msg);
	}

	if (i != SQLITE_OK) {
		unlock_db ();
		tracker_log ("query %s failed with error : %s", query, msg);
		g_free (msg);
		return NULL;
	} 

	unlock_db ();
	

	if (!array || rows == 0 || cols ==0) {
		return NULL;
	}

	result = g_new (char *, rows+1);
	result [rows] = NULL;	

	int totalrows = (rows+1) * cols;

	//tracker_log ("totalrows is %d", totalrows);
	//for (i=0;i<totalrows;i++) tracker_log (array[i]);

	int k = 0;
	int j;

	for (i=cols; i < totalrows; i=i+cols) {

		char **row = g_new (char *, cols + 1);
		row [cols] = NULL;

		for (j = 0; j < cols; j++ ) {
			row[j] = g_strdup (array[i+j]);
	//		tracker_log ("data for row %d, col %d is %s", k, j, row[j]);
		}


		result[k]  = (gpointer)row;
		k++;

	}
	
	sqlite3_free_table (array);

	return (char ***) result;

}


char *
tracker_escape_string (DBConnection *db_con, const char *in)
{

	if (!in) {
		return NULL;
	}
	
	if (!strchr (in, '\'')) {
		return g_strdup (in);
	} 

	GString *str = g_string_new ("");

	int length = strlen (in);
	int i;

	/* only need to escape single quotes */
	for (i=0; i < length; i++) {

		if (in[i] == '\'') {
			str = g_string_append (str, "''"); 
		} else {
			str = g_string_append_c (str, in[i]);	
		}
	}

	return g_string_free (str, FALSE);
} 


static sqlite3_stmt *
get_prepared_query (DBConnection *db_con, const char *procedure)
{
	sqlite3_stmt 	*stmt;
	char *query;

	/* check if query is already prepared (ie is in table) */
	stmt = g_hash_table_lookup (db_con->statements, procedure);

	/* check if query is in list and prepare it if so */
	if (!stmt || (sqlite3_expired (stmt) > 0)) {

		query = g_hash_table_lookup (prepared_queries, procedure);
		
		if (!query) {
			tracker_log ("ERROR : prepared query %s not found", procedure);
			return NULL;
		} else {
	
			/* prepare the query */	
			
			int rc = sqlite3_prepare (db_con->db, query, -1, &stmt, 0);

			if (rc == SQLITE_OK && stmt != NULL) {
				tracker_log ("successfully prepared query %s", procedure);
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
	va_list 	args;
	int 		i;
	char 		*str;
	sqlite3_stmt 	*stmt;
	char 		**res = NULL;



	stmt = get_prepared_query (db_con, procedure);

	va_start (args, param_count);

	if (param_count != sqlite3_bind_parameter_count (stmt)) {
		tracker_log ("ERROR : incorrect no of paramters %d supplied to %s", param_count, procedure);

	}

	for (i = 0; i < param_count; i++ ) {

		str = va_arg (args, char *);
		
		if (!str) {
			tracker_log ("Warning - parameter %d is null", i);
		} 
		
		if (sqlite3_bind_text (stmt, i+1, str, strlen (str), SQLITE_TRANSIENT) != SQLITE_OK) {
			tracker_log ("ERROR : paramter %d could not be bound to %s", i, procedure);
		}
		
	}
 	
	va_end (args);

	int rc, cols, row = 0;

	cols = sqlite3_column_count (stmt);	

	GSList *result = NULL;

	while (TRUE) {
		
		
		if (!lock_db ()) {
			return NULL;
		}

		rc = sqlite3_step (stmt);
		
		int busy_count = 0;
		if (rc == SQLITE_BUSY) {
			unlock_db ();
	
			busy_count++;
			
			if (busy_count > 1000) {
				tracker_log ("excessive busy count in query %s and thread %s", procedure, db_con->thread);
				exit(0);
			}


			if (busy_count > 50) {
				g_usleep (g_random_int_range (1000, (busy_count * 200) ));
			} else {
				g_usleep (100);
			}

			continue;
		}

		if (rc == SQLITE_ROW) {
	
			char **new_row = g_new (char *, cols+1);
			new_row [cols] = NULL;

			unlock_db ();

			for (i = 0; i < cols; i++) {
				const char *st = (char *)sqlite3_column_text (stmt, i);
				if (st) {
					new_row[i] = g_strdup (st);
					//tracker_log ("%s : row %d, col %d is %s", procedure, row, i, st);
				}
			}

			
			result = g_slist_prepend (result, new_row);

			row++;

			continue;
		}
	
		unlock_db ();

		break;
	}

	if (rc != SQLITE_DONE) {
		tracker_log ("ERROR : prepared query %s failed due to %s", procedure, sqlite3_errmsg (db_con->db));
	}

	if (!result || row == 0) {
		return NULL;
	}

	result = g_slist_reverse (result);

	
	res = g_new ( char *, row+1);
	res[row] = NULL;	

	GSList *tmp = result;

	for (i=0; i < row; i++) {
		
		if (tmp) {
			res[i]  = tmp->data;
			tmp = tmp->next;
		}
	}
	
	g_slist_free (result);

	return (char ***)res;

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
		char ***res;
	
		res = tracker_exec_proc  (db_con, "GetServiceTypeID", 1, service);	

		if (res) {
			if (res[0][0]) {
				*min = atoi (res[0][0]);
				*max = atoi (res[0][0]);
			}
			tracker_db_free_result (res);
		}
	}


}



void
tracker_db_load_stored_procs (DBConnection *db_con)
{
	return;
}




void
tracker_create_db ()
{
	DBConnection *db_con;
	char  *sql_file;
	char *query;
	char **queries, **queries_p ;

	

	tracker_log ("Creating tracker database...");
	db_con = tracker_db_connect ();

	sql_file = g_strdup (DATADIR "/tracker/sqlite-tracker.sql");
	tracker_log ("Creating tables...");
	if (!g_file_get_contents (sql_file, &query, NULL, NULL)) {
		tracker_log ("Tracker cannot read required file %s - Please reinstall tracker or check read permissions on the file if it exists", sql_file); 
		g_assert (FALSE);
	} else {
		queries = g_strsplit_set (query, ";", -1);
		for (queries_p = queries; *queries_p; queries_p++) {
			if (*queries_p) {
				//tracker_log ("creating table %s", *queries_p);
		     		tracker_exec_sql (db_con, *queries_p);
				//tracker_log ("Table created");
			}
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
tracker_log_sql	 (DBConnection *db_con, const char *query)
{
	char ***res = NULL;

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
	char *str, *dbname;
	gboolean need_setup;

	need_setup = FALSE;

	str = g_build_filename (g_get_home_dir (), ".Tracker", NULL);
	dbname = g_build_filename (str, "database", NULL);


	if (!g_file_test (str, G_FILE_TEST_IS_DIR)) {
		need_setup = TRUE;
		g_mkdir (str, 0700);
	}


	if (!g_file_test (dbname, G_FILE_TEST_EXISTS)) {
		need_setup = TRUE;
	}

	g_free (dbname);
	g_free (str);

	return need_setup;
}


gboolean
tracker_update_db (DBConnection *db_con)
{
	char ***res;
	char **row;


	res = tracker_exec_sql (db_con, "SELECT OptionValue FROM Options WHERE OptionKey = 'DBVersion'");
	
	if (!res) {
		return FALSE;
	}

	row = tracker_db_get_row (res, 1);
	
	if (!row || !row[0]) {
		tracker_db_free_result (res);
		return FALSE;
	}

	

	char *version =  row[0];
	int i = atoi (version);

	tracker_db_free_result (res);

	tracker_log ("Checking tracker DB version...Current version is %d and needed version is %d", i, TRACKER_DB_VERSION_REQUIRED);

	if (i < 4) {
		tracker_log ("Your database is out of date and will need to be rebuilt and all your files reindexed.\nThis may take a while...please wait...");

		tracker_db_close (db_con);

		char *db_name = g_build_filename (g_get_home_dir (), ".Tracker", "database", NULL);
		unlink (db_name);
		g_free (db_name);
	
		tracker_create_db ();

		return TRUE;
		
	}

	/* apply and table changes for each version update */
/*	while (i < TRACKER_DB_VERSION_REQUIRED) {
			
		i++;

		sql_file = g_strconcat (DATADIR, "/tracker/tracker-db-table-update", version, ".sql", NULL);
		
		tracker_log ("Please wait while database is being updated to the latest version");	

		if (g_file_get_contents (sql_file, &query, NULL, NULL)) {

			queries = g_strsplit_set (query, ";", -1);
			for (queries_p = queries; *queries_p; queries_p++) {
				if (*queries_p) {
					tracker_exec_sql (db, *queries_p);
				}
			}
			g_strfreev (queries);
			g_free (query);		
		} 	
	
		g_free (sql_file);

			
	}
*/

	
	return FALSE;
}

/*
static void
tracker_metadata_parse_text_contents (const char *file_as_text, unsigned int  ID)
{
	if (g_file_test (file_as_text, G_FILE_TEST_EXISTS)) {

	
		char *argv[4];
		char *temp_file_name;
		int fd;

		fd = g_file_open_tmp (NULL, &temp_file_name, NULL);

  		if (fd == -1) {
			g_warning ("make thumb file %s failed", temp_file_name);
			return;
      		} else {
			close (fd);
		}

		argv[0] = g_strdup ("tracker-convert-file");
		argv[1] = g_strdup (file_as_text);
		argv[2] = g_strdup (temp_file_name);
		argv[3] = NULL;

		tracker_log ("extracting parsed text for %s", file_as_text);
	
		if (g_spawn_sync (NULL,
				  argv,
				  NULL, 
				  G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
				  NULL,
				  NULL,
				  NULL,
				  NULL,
				  NULL,
				  NULL)) {

			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);
			

			FILE 	*file;
			char 	buffer[8192];
			char 	*word;
			int	count;

			file = fopen (temp_file_name,"r");

			while (feof (file) == 0)  {
	
				fgets (buffer, 8192, file);

				

				char *sep = strchr (buffer, '\n');
				
	  			
				if (sep) {
					*sep = '\0';
				} else {
					continue;
				}

				sep = strchr (buffer, ' ');
					  			
				if (sep) {
					*sep = '\0';
					word = g_strdup (sep + 1);
					count = atoi (buffer);					
				} else {
					continue;
				}

				tracker_indexer_insert_word (file_indexer, ID, word, count);
				g_free (word);

			}

			fclose (file);
			unlink (temp_file_name);
			return;

		} else {
			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);
			return;
		}

	} else {
		tracker_log ("Error : could not find file %s", file_as_text);
	}
	


}
*/


void
tracker_db_save_file_contents	(DBConnection *db_con, const char *file_name, FileInfo *info)
{

	FILE 		*file;
	char 		buffer[65565];
	int  		bytes_read = 0, buffer_length;
	char		*str_file_id, *str_meta_id, *value;
	FieldDef 	*def;
	GString 	*str;
	sqlite3_stmt 	*stmt;
	int 		rc;

	//tracker_log ("parsing file");
	//tracker_metadata_parse_text_contents (file_as_text, file_id);
	//tracker_log ("parsing finished");


	file = fopen (file_name,"r");
	//tracker_log ("saving text to db with file_id %ld", file_id);

	if (!file) {
		tracker_log ("Could not open file %s", file_name);
		return; 
	}

	def = tracker_db_get_field_def (db_con, "File.Content"); 
	if (!def || !def->id) {
		tracker_log ("Could not get metadata for File.Content");
		return; 
	}
	
	str_meta_id = g_strdup (def->id); 

	tracker_db_free_field_def (def);

	str_file_id = g_strdup_printf ("%ld", info->file_id);
	
	str = g_string_new ("");

	while ((feof (file) == 0)) {
	
		fgets (buffer, 65565, file);

		buffer_length = strlen (buffer);

		if (buffer_length < 3) {
			continue;
		}

		if (!g_utf8_validate (buffer, buffer_length, NULL)) {
		
			value = g_locale_to_utf8 (buffer, buffer_length, NULL, NULL, NULL);
			
			if (!value) {
				continue;
			}
			
			str = g_string_append (str, value);
			g_free (value);
			bytes_read += strlen (value);
			
		} else {
			str = g_string_append (str, buffer);
			bytes_read += buffer_length;
		}

		

		/* set upper limit on text we read in to approx 1MB (to do - make size a configurable option) */
		if (bytes_read > 1048576) {
			break;
		}		
	}

	value = g_string_free (str, FALSE);

	fclose (file);

	if (!lock_db ()) {
		g_free (str_file_id);
		g_free (str_meta_id);
		if (value) {
			g_free (value);
		}
		return;
	}

	if (bytes_read > 3) {

		stmt = get_prepared_query (db_con, "SaveFileContents");

		sqlite3_bind_text (stmt, 0, str_file_id, strlen (str_file_id), SQLITE_STATIC);
		sqlite3_bind_text (stmt, 1, str_meta_id, strlen (str_meta_id), SQLITE_STATIC);
		sqlite3_bind_text (stmt, 2, value, bytes_read, SQLITE_STATIC);

		while (TRUE) {
		

		
			if (!lock_db ()) {
				g_free (str_file_id);
				g_free (str_meta_id);
				if (value) {
					g_free (value);
				}
				return;
			}

			rc = sqlite3_step (stmt);
			int busy_count = 0;

			if (rc == SQLITE_BUSY) {
				unlock_db ();
				busy_count++;

				if (busy_count > 1000) {
					tracker_log ("excessive busy count in query %s and thread %s", "save file contents", db_con->thread);
					exit(0);
				}

				if (busy_count > 50) {
					g_usleep (g_random_int_range (1000, (busy_count * 200) ));
				} else {
					g_usleep (100);
				}

				continue;
			}

			unlock_db ();
			break;
		}

		if (rc == SQLITE_DONE) {
			tracker_log ("%d bytes of text successfully inserted into file id %s", bytes_read, str_file_id);
		}
		
		
	}

	if (value) {
		g_free (value);
	}
	g_free (str_meta_id);
	g_free (str_file_id);	


}


void
tracker_db_clear_temp (DBConnection *db_con)
{
	tracker_exec_sql (db_con, "DELETE FROM FilePending");
	tracker_exec_sql (db_con, "DELETE FROM FileWatches");
}


void
tracker_db_start_transaction (DBConnection *db_con)
{
	tracker_exec_sql (db_con, "BEGIN EXCLUSIVE TRANSACTION");
}


void
tracker_db_end_transaction (DBConnection *db_con)
{
	tracker_exec_sql (db_con, "END TRANSACTION");
}


void
tracker_db_check_tables (DBConnection *db_con)
{

}


char ***
tracker_db_search_text (DBConnection *db_con, const char *service, const char *search_string, int offset, int limit, gboolean sort)
{
	char ***result;

	result = NULL;

	return result;

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

	char ***result;

	result = NULL;

	return result;
}

char ***
tracker_db_search_matching_metadata (DBConnection *db_con, const char *service, const char *id, const char *text)
{
	g_return_val_if_fail (id, NULL);

	char ***result;

	result = NULL;

	return result;
}

/*
static int
get_metadata_type (DBConnection *db_con, const char *meta)
{
	char ***res;
	int result;

	res = tracker_exec_proc  (db_con, "GetMetaDataTypeID", 1, meta);	

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
	char ***res;

	g_return_val_if_fail (id, NULL);

	def = tracker_db_get_field_def (db_con, key);

	if (!def) {
		return NULL;
	}

	switch (def->type) {

		case 0: res = tracker_exec_proc  (db_con, "GetMetadataIndex", 2, id, def->id); break;	
		case 1: res = tracker_exec_proc  (db_con, "GetMetadataString", 2, id, def->id); break;	
		case 2: res = tracker_exec_proc  (db_con, "GetMetadataNumeric", 2, id, def->id); break;			
		case 3: res = tracker_exec_proc  (db_con, "GetMetadataNumeric", 2, id, def->id); break;
		default: tracker_log ("Error: metadata could not be retrieved as type %d is not supported", def->type); res = NULL;	
	}

	tracker_db_free_field_def (def);

	return res;
}


void 
tracker_db_set_metadata (DBConnection *db_con, const char *service, const char *id, const char *key, const char *value, gboolean overwrite)
{
	FieldDef *def;

	g_return_if_fail (id);

	def = tracker_db_get_field_def (db_con, key);

	if (!def) {
		return;
	}

	if ((!def->writeable) && (!overwrite)) {
		tracker_db_free_field_def (def);
		return;
	} 
	
	switch (def->type) {

		case 0: tracker_exec_proc  (db_con, "SetMetadataIndex", 3, id, def->id, value); break;	
		case 1: tracker_exec_proc  (db_con, "SetMetadataString", 3, id, def->id, value); break;	
		case 2: tracker_exec_proc  (db_con, "SetMetadataNumeric", 3, id, def->id, value); break;			
		case 3: tracker_exec_proc  (db_con, "SetMetadataNumeric", 3, id, def->id, value); break;	
		default: tracker_log ("Error: metadata could not be set as type %d is not supported", def->type);	
	}

	tracker_db_free_field_def (def);
	
}


void
tracker_db_update_keywords (DBConnection *db_con,  const char *service, const char *id, const char *value)
{
	FieldDef *def;

	g_return_if_fail (id);

	def = tracker_db_get_field_def (db_con, "Keywords");

	if (!def) {
		return;
	}

	tracker_exec_proc (db_con, "SetMetadataIndex", 3, id, def->id, value);

	tracker_db_free_field_def (def);
}


void 
tracker_db_create_service (DBConnection *db_con, const char *path, const char *name, const char *service,  gboolean is_dir, gboolean is_link, 
			   gboolean is_source,  int offset, long mtime)
{
	char *str_is_dir, *str_is_link, *str_is_source, *str_offset;
	char ***res;
	char *sid;
	char *str_mtime;
	int i;

	/* get a new unique ID for the service - use mutex to prevent race conditions */

	g_mutex_lock (sequence_mutex);
	
	res = tracker_exec_proc (db_con, "GetNewID", 0);
	
	if (!res || !res[0][0]) { 
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
	
	str_mtime = g_strdup_printf ("%ld", mtime);

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

	if (is_source) {
		str_is_source = "1";
	} else {
		str_is_source = "0";
	}

	str_offset = tracker_int_to_str (offset);

	res = tracker_exec_proc (db_con, "GetServiceTypeID", 1, service);
	char *service_type_id;
	if (res) {
		if (res[0][0]) {
			service_type_id = res[0][0];
		} else {
			service_type_id = "8";
		}
	
	}

	tracker_exec_proc  (db_con, "CreateService", 9, sid, path, name, service_type_id, str_is_dir, str_is_link, str_is_source, str_offset,  str_mtime);

	if (res[0][0]) {
		tracker_db_free_result (res);
	}

	g_free (sid);
	g_free (str_mtime);
	g_free (str_offset);

}


void
tracker_db_delete_file (DBConnection *db_con, long file_id)
{
	char *str_file_id = tracker_long_to_str (file_id);

	tracker_db_start_transaction (db_con);
	tracker_exec_proc  (db_con, "DeleteFile1", 1,  str_file_id);
	tracker_exec_proc  (db_con, "DeleteFile2", 1,  str_file_id);
	tracker_exec_proc  (db_con, "DeleteFile3", 1,  str_file_id);
	tracker_exec_proc  (db_con, "DeleteFile4", 2,  str_file_id, str_file_id);
	tracker_exec_proc  (db_con, "DeleteFile5", 1,  str_file_id);
	tracker_db_end_transaction (db_con);

	g_free (str_file_id);
}

void
tracker_db_delete_directory (DBConnection *db_con, long file_id, const char *uri)
{
	char *str_file_id = tracker_long_to_str (file_id);

	char *uri_prefix =  g_strconcat (uri, G_DIR_SEPARATOR_S, "*", NULL);

	tracker_db_start_transaction (db_con);
	tracker_exec_proc  (db_con, "DeleteDirectory1", 2,  uri, uri_prefix);
	tracker_exec_proc  (db_con, "DeleteDirectory2", 2,  uri, uri_prefix);
	tracker_exec_proc  (db_con, "DeleteDirectory3", 2,  uri, uri_prefix);
	tracker_exec_proc  (db_con, "DeleteDirectory4", 2,  uri, uri_prefix);
	tracker_exec_proc  (db_con, "DeleteDirectory5", 1,  str_file_id);
	tracker_exec_proc  (db_con, "DeleteDirectory6", 1,  str_file_id);
	tracker_exec_proc  (db_con, "DeleteDirectory7", 1,  str_file_id);
	tracker_exec_proc  (db_con, "DeleteDirectory8", 2,  str_file_id, str_file_id);
	tracker_exec_proc  (db_con, "DeleteDirectory9", 1,  str_file_id);
	tracker_db_end_transaction (db_con);

	g_free (uri_prefix);
	g_free (str_file_id);
}



void
tracker_db_update_file (DBConnection *db_con, long file_id, long mtime)
{
	char *str_file_id = tracker_long_to_str (file_id);
	char *str_mtime = tracker_long_to_str (mtime);

	tracker_exec_proc  (db_con, "UpdateFile", 2, str_mtime, str_file_id);

	g_free (str_file_id);
	g_free (str_mtime);
}

gboolean 
tracker_db_has_pending_files (DBConnection *db_con)
{
	char ***res = NULL;
	char **  row;
	gboolean has_pending = FALSE;
	

	if (!tracker->is_running) {
		return FALSE;
	}


	res = tracker_exec_proc (db_con, "ExistsPendingFiles", 0); 


	if (res) {

		row = tracker_db_get_row (res, 0);
				
		if (row && row[0]) {
			int pending_file_count  = atoi (row[0]);
			tracker_log ("%d files are pending", pending_file_count);				
			has_pending = (pending_file_count  > 0);
		}
					
		tracker_db_free_result (res);

	}

	return has_pending;

}


gboolean 
tracker_db_has_pending_metadata (DBConnection *db_con)
{
	char ***res = NULL;
	char **  row;
	gboolean has_pending = FALSE;

	if (!tracker->is_running) {
		return FALSE;
	}

	
	res = tracker_exec_proc (db_con, "CountPendingMetadataFiles", 0); 

	if (res) {

		row = tracker_db_get_row (res, 0);
				
		if (row && row[0]) {
			int pending_file_count  = atoi (row[0]);
						
			has_pending = (pending_file_count  > 0);
		}
					
		tracker_db_free_result (res);

	}

	return has_pending;

}


char ***
tracker_db_get_pending_files (DBConnection *db_con)
{

	char *str;
	char *time_str;

	if (!tracker->is_running) {
		return NULL;
	}

	time_t time_now;

	time (&time_now);

	time_str = tracker_long_to_str (time_now);

	tracker_db_start_transaction (db_con);
	tracker_exec_sql (db_con, "delete from FileTemp");
	str = g_strconcat ("Insert into FileTemp (ID, FileID, Action, FileUri, MimeType, IsDir) select ID, FileID, Action, FileUri, MimeType, IsDir From FilePending WHERE (PendingDate < ", time_str, " )  AND (Action <> 20) LIMIT 100", NULL);
	tracker_exec_sql (db_con, str);
	tracker_exec_sql (db_con, "DELETE FROM FilePending where ID in (select ID from FileTemp)");
	tracker_db_end_transaction (db_con);

	g_free (str);
	g_free (time_str);

	return tracker_exec_sql (db_con, "SELECT FileID, FileUri, Action, MimeType, IsDir FROM FileTemp ORDER BY ID");

}


void
tracker_db_remove_pending_files (DBConnection *db_con)
{
	tracker_exec_sql (db_con, "Delete from FileTemp");
}



char ***
tracker_db_get_pending_metadata (DBConnection *db_con)
{
	
	if (!tracker->is_running) {
		return NULL;
	}

	tracker_db_start_transaction (db_con);
	tracker_exec_sql (db_con, "delete from MetadataTemp");
	char *str = "Insert into MetadataTemp (ID, FileID, Action, FileUri, MimeType, IsDir) select ID, FileID, Action, FileUri, MimeType, IsDir From FilePending WHERE Action = 20 LIMIT 100";
	tracker_exec_sql (db_con, str);
	tracker_exec_sql (db_con, "DELETE FROM FilePending where ID in (select ID from MetadataTemp)");
	tracker_db_end_transaction (db_con);

	return tracker_exec_sql (db_con, "SELECT FileID, FileUri, Action, MimeType, IsDir FROM MetadataTemp ORDER BY ID");
	

}


void
tracker_db_remove_pending_metadata (DBConnection *db_con)
{
	tracker_exec_sql (db_con, "Delete from MetadataTemp");
}

void
tracker_db_insert_pending (DBConnection *db_con, const char *id, const char *action, const char *counter, const char *uri, const char *mime, gboolean is_dir)
{
	char 	*time_str;
	time_t  time_now;
	int	i;

	time (&time_now);
	i = atoi (counter);

	if (i==0) {
		time_str = tracker_int_to_str (i);
	} else {
		time_str = tracker_long_to_str (time_now + i);
		
	}

	//tracker_log ("inserting time of %s", time_str);

	if (is_dir) {
		tracker_exec_proc  (db_con, "InsertPendingFile", 6, id, action, time_str, uri, mime, "1");
	} else {
		tracker_exec_proc  (db_con, "InsertPendingFile", 6, id, action, time_str, uri,  mime, "0");
	}

	g_free (time_str);
}	


void
tracker_db_update_pending (DBConnection *db_con, const char *counter, const char *action, const char *uri )
{
	char 	*time_str;
	time_t  time_now;
	int	i;

	time (&time_now);
	i = atoi (counter);

	time_str = tracker_long_to_str (time_now + i);

	tracker_exec_proc  (db_con, "UpdatePendingFile", 3, time_str, action, uri);

	g_free (time_str);
}


char ***
tracker_db_get_files_by_service (DBConnection *db_con, const char *service, int offset, int limit)
{

	int min, max;

	char *str_limit = tracker_int_to_str (limit);
	char *str_offset = tracker_int_to_str (offset);

	get_service_id_range (db_con, service, &min, &max);

	char *str_min = tracker_int_to_str (min);
	char *str_max = tracker_int_to_str (max);

	char ***res = tracker_exec_proc  (db_con,  "GetFilesByServiceType", 4, str_min, str_max, str_offset, str_limit);
		
	g_free (str_min);
	g_free (str_max);	
	g_free (str_offset);
	g_free (str_limit);

	return res;

}


char ***
tracker_db_get_files_by_mime (DBConnection *db_con, char **mimes, int n, int offset, int limit, gboolean vfs)
{
	int i, min, max;
	char ***res = NULL;
	char *query;
	GString *str;

	g_return_val_if_fail (mimes, NULL);

	if (vfs) {
		min = 9;
		max = 17;
	} else {
		min = 0;
		max = 8;
	}

	str = g_string_new ("SELECT  DISTINCT F.Path || '/' || F.Name as uri  FROM Services F inner join ServiceMetaData M on F.ID = M.ServiceID WHERE M.MetaDataID = (select ID From MetaDataTypes where MetaName ='File.Format') AND (M.MetaDataIndexValue in ");

	g_string_append_printf (str, "('%s'", mimes[0]);

	for (i=1; i<n; i++) {
		g_string_append_printf (str, ", '%s'", mimes[i]); 
	}

	g_string_append_printf (str, ")) AND (F.ServiceTypeID between %d and %d) LIMIT %d,%d", min, max, offset, limit);

	query = g_string_free (str, FALSE);

	res = tracker_exec_sql (db_con, query);

	g_free (query);

	return res;
}



char ***
tracker_db_search_text_mime  (DBConnection *db_con, const char *text , char **mime_array, int n)
{
	gboolean use_boolean_search;
	int i;

	/* check search string for embedded special chars like hyphens and format appropriately */
	char *search_term = tracker_format_search_terms (text, &use_boolean_search);

	GString *mimes = NULL;

	/* build mimes string */
	for (i=0; i<n; i++) {
		if (mime_array[i] && strlen (mime_array[i]) > 0) {
			if (mimes) {
				g_string_append (mimes, ",");
				g_string_append (mimes, mime_array[i]);
			} else {
				mimes = g_string_new (mime_array[i]);
			}
			
		}
	}

	char *mime_list = g_string_free (mimes, FALSE);

	char ***res = NULL;

	//tracker_exec_proc  (db_con, "SearchTextMime", 2, search_term , mime_list);

	g_free (search_term);	
	g_free (mime_list);

	return res;
	
}


char ***
tracker_db_search_text_location  (DBConnection *db_con, const char *text ,const char *location)
{
	gboolean use_boolean_search;

	char *search_term = tracker_format_search_terms (text, &use_boolean_search);

	//char ***res = tracker_exec_proc  (db_con, "SearchTextLocation", 2, search_term , location);

	
	g_free (search_term);

	return NULL;

}


char ***
tracker_db_search_text_mime_location  (DBConnection *db_con, const char *text , char **mime_array, int n, const char *location)
{
	gboolean use_boolean_search;
	int i;
	/* check search string for embedded special chars like hyphens and format appropriately */
	char *search_term = tracker_format_search_terms (text, &use_boolean_search);

	GString *mimes = NULL;

	/* build mimes string */
	for (i=0; i<n; i++) {
		if (mime_array[i] && strlen (mime_array[i]) > 0) {
			if (mimes) {
				g_string_append (mimes, ",");
				g_string_append (mimes, mime_array[i]);
			} else {
				mimes = g_string_new (mime_array[i]);
			}
			
		}
	}
	
	char *mime_list = g_string_free (mimes, FALSE);

//	char ***res = tracker_exec_proc  (db_con, "SearchTextMimeLocation", 3, search_term , mime_list, location);

	g_free (search_term);	
	g_free (mime_list);

	return NULL;
	
}


char ***
tracker_db_get_metadata_types (DBConnection *db_con, const char *class, gboolean writeable) 
{
	if (writeable) {
		return tracker_exec_proc (db_con, "GetWriteableMetadataTypes", 2, class);
	} else {
		return tracker_exec_proc (db_con, "GetMetadataTypes", 2, class);
	}
}



char ***
tracker_db_get_sub_watches (DBConnection *db_con, const char *dir) 
{
	char ***res;
	char *folder;

	folder = g_strconcat (dir, "/%", NULL);
	
	res = tracker_exec_proc (db_con, "GetSubWatches", 1, folder);

	g_free (folder);

	return res;

}


char ***
tracker_db_delete_sub_watches (DBConnection *db_con, const char *dir) 
{
	char ***res;
	char *folder;

	folder = g_strconcat (dir, "/%", NULL);
	
	res = tracker_exec_proc (db_con, "DeleteSubWatches", 1, folder);

	g_free (folder);

	return res;

}



void
tracker_db_update_file_move (DBConnection *db_con, long file_id, const char *path, const char *name, long mtime)
{
	char *str_file_id = g_strdup_printf ("%ld", file_id);
	char *index_time = g_strdup_printf ("%ld", mtime);

	tracker_exec_proc  (db_con, "UpdateFileMove", 4,  path, name, index_time, str_file_id);

	g_free (str_file_id);
	g_free (index_time);
}

char ***
tracker_db_get_file_subfolders (DBConnection *db_con, const char *uri)
{
	char ***res;
	char *folder;

	folder = g_strconcat (uri, "/%", NULL);
	
	res = tracker_exec_proc (db_con, "SelectFileSubFolders", 2, uri, folder);

	g_free (folder);

	return res;

}


