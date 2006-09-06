#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tracker-db-sqlite.h"
#include "tracker-indexer.h"

static GHashTable *prepared_queries;


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

	if (!result) {
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

			tracker_log ("installing query %s with sql %s", name, query);
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
	sqlite3_thread_cleanup ();
}


void
tracker_db_finalize ()
{
	return;

}

static void
get_meta_table_data (gpointer key,
   		     gpointer value,
		     gpointer user_data)
{
	DatabaseAction *db_action;	
	char *mtype, *mvalue, *avalue, *dvalue = NULL, *evalue;
	
	mtype = (char *)key;
	avalue = (char *)value;

	db_action = user_data;

	



}

static void
finalize_statement (gpointer key,
   		   gpointer value,
		   gpointer user_data)
{
	sqlite3_stmt *stmt = value;

	sqlite3_finalize (stmt);	
	
}

void
tracker_db_close (DBConnection *db_con)
{
	/* clear prepared queries */
	if (db_con->statements) {
		g_hash_table_foreach (db_con->statements, finalize_statement, NULL);
	}

	g_mutex_free (db_con->write_mutex);

	g_hash_table_free (db_con->statements);

	sqlite3_close (db_con->db);

}




DBConnection *
tracker_db_connect ()
{
	DBConnection *db_con = g_new (DBConnection, 1);

	char *dbname = g_build_filename (g_get_home_dir (), ".Tracker", "tracker.db", NULL);

	if (!sqlite3_open (dbname, &db_con->db)) {
		tracker_log ("Fatal Error : Can't open database: %s", sqlite3_errmsg (db_con->db);
		exit (1);
	}

	g_free (dbname);

	db_con->statements = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	db_con->write_mutex = g_mutex_new ();
	
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
				g_usleep (5000); 
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
	char ***result = NULL;
	int cols, rows;
	char *msg;
	char **row;

	g_return_val_if_fail (query, NULL);

	if (!lock_db ()) {
		return NULL;
	}

	int i =  sqlite3_get_table (db_con->db, query, &array, &rows, &cols, &msg);

	while (i == SQLITE_BUSY) {
		unlock_db ();
		g_usleep (1000);
		lock_db ();
		i = sqlite3_get_table (db_con->db, query, &array, &rows, &cols, &msg);
	}

	if (i != SQLITE_OK) {
		unlock_db ();
		tracker_log ("query %s failed with error : %s", query, msg);
		g_free (msg);
		return;
	} 

	unlock_db ();
	

	if (!array) {
		return NULL;
	}

	result = g_new ( char *, rows);
	result [rows] = NULL;	

	int totalrows = (rows+1) * cols;
	int k = 0;

	for (i=cols; i < totalrows; i+cols) {

		char **row = g_new (char *, cols + 1);
		row [cols] = NULL;

		for (j = 0; j < cols; j++ ) {
			row[j] = g_strdup (array[i+j+1]);
		}


		result[k]  = (gpointer)row;
		k++;

	}
	
	sqlite3_free_table (array);

	return result;

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


char ***
tracker_exec_proc (DBConnection *db_con, const char *procedure, int param_count, ...)
{
	va_list 	args;
	int 		i;
	char 		*str, *param, *query, *s;
	sqlite3_stmt 	*stmt = 0;
	char 		***res = NULL;

	va_start (args, param_count);

	/* check if query is already prepared (ie is in table) */
	stmt = g_hash_table_lookup (db_con->statements, procedure);

	/* check if query is in list and prepare it if so */
	if (!stmt) {
		query = g_hash_table_lookup (prepared_queries, procedure);
		
		if (!query) {

			tracker_log ("FATAL ERROR : prepared query %s not found", procedure);

			g_assert (FALSE);

		} else {
	
			/* prepare the query */	
			
			int rc = sqlite3_prepare (db, query, -1, &stmt, 0);

			if (rc == SQLITE_OK && stmt != NULL) {
				tracker_log ("successfully prepared query %s", procedure);
				g_hash_table_insert (db_con->statements, procedure, stmt);
			} else {
				tracker_log ("FATAL ERROR : failed to prepare query %s with sql %s", procedure, query);
				g_assert (FALSE);
			}
		}
	}

	sqlite3_reset (stmt);

	for (i = 0; i < param_count; i++ ) {

		str = va_arg (args, char *);
		
		if (!str) {
			tracker_log ("Warning - parameter %d is null", i);
			param = g_strdup (" ");	
		} else {
			param = tracker_escape_string (db_con, str);
		}

		sqlite3_bind_text (stmt, i, param, strlen (param), g_free);
		
	}
 	
	va_end (args);

	int rc, cols, row = 0;

	cols = sqlite3_column_count (stmt);	

	GSList *result = NULL;

	while (TRUE) {
		
		lock_db ();
		rc = sqlite3_step (stmt);
		
		if (rc == SQLITE_BUSY) {
			unlock_db ();
			g_usleep (1000);
			continue;
		}

		if (rc == SQLITE_ROW) {
	
			char **new_row = g_new (char *, cols);
			new_row [cols] = NULL;

			unlock_db ();

			for (i = 0; i < cols; i++) {
				new_row[i] = sqlite3_column_text (stmt, i);
			}

			result = g_slist_prepend (result, new_row);

			row++;

			continue;
		}
	
		unlock_db ();

		break;
	}

	if (!result || row == 0) {
		return NULL;
	}

	result = g_slist_reverse (result);

	row++;
	res = g_new ( char *, row);
	res[row] = NULL;	

	GSlist *tmp = result;

	for (i=0; i < row; i++) {
		
		if (tmp) {
			res[i]  = tmp->data;
			tmp = tmp->next;
		}
	}
	
	g_slist_free (result);

	return res;

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
		     		tracker_exec_sql (db_con, *queries_p);
			}
		}
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
tracker_update_db (DBConnection *db_con)
{
	char ***res;
	char  *sql_file;
	char *query;
	char **queries, **queries_p;


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

		char *db_name = g_build_filename (g_get_home_dir (), ".Tracker", "tracker.db", NULL);
		unlink (db_name);
		g_free (db_name);
	
		create_tracker_db ();

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
			return NULL;
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



void
tracker_db_save_file_contents	(DBConnection *db_con, const char *file_name, long file_id)
{

	FILE 		*file;
	char 		buffer[16384];
	int  		bytes_read = 0;
	unsigned long 	file_id_length, meta_id_length;
	MYSQL_BIND 	bind[3];
	unsigned long  	length;
	char		*str_file_id, *str_meta_id, *value;
	FieldDef 	*def;


	tracker_log ("parsing file");
	tracker_metadata_parse_text_contents (file_as_text, file_id);
	tracker_log ("parsing finished");


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

	str_file_id = g_strdup_printf ("%ld", file_id);
	file_id_length = strlen (str_file_id);
	meta_id_length = strlen (str_meta_id);
	

	
	/* Supply file's text in chunks to server as it could be huge */
	/* to do - need a gconf setting to define max bytes to be read */
	while ( (feof (file) == 0) && (bytes_read < (1024 * 2048)) ) {
	
		/* leave 2x space in buffer so that potential utf8 conversion can fit */
		fgets (buffer, 8192, file);

		if (strlen (buffer) < 2) {
			continue;
		}

		if (!g_utf8_validate (buffer, -1, NULL)) {
		
			/* attempt to convert it into valid utf8 using locale */
			value = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
			
			if (!value) {
				continue;
			} else {
				if (strlen (value) > 16383) {
					g_free (value);
					continue;
				} else {
				 	strncpy (buffer, value, 16384);
					g_free (value);
				}
			}
		} 

		bytes_read += strlen (buffer);
		
		

		if (mysql_stmt_send_long_data (db_con->insert_contents_stmt, 2, buffer, strlen (buffer)) != 0) {	

			tracker_log ("error sending data : %s\n", mysql_stmt_error (db_con->insert_contents_stmt));
			g_free (str_meta_id);
			g_free (str_file_id);
			fclose (file);
			return;

		} 
	}

	if (!lock_db ()) {
		g_free (str_file_id);
		g_free (str_meta_id);
		fclose (file);
		return;
	}

	if (bytes_read > 3) {
		if (mysql_stmt_execute (db_con->insert_contents_stmt) != 0) {
	
			tracker_log ("insert metadata indexed query failed :%s", mysql_stmt_error (db_con->insert_contents_stmt));
		} else {
	
			//tracker_log ("%d bytes of text successfully inserted into file id %s", bytes_read, str_file_id);
		}
	}
	unlock_db ();
	g_free (str_meta_id);
	g_free (str_file_id);	
	fclose (file);
	

	if (mysql_stmt_free_result (db_con->insert_contents_stmt)) {
		tracker_log ("ERROR Freeing file contents statement");
	}


}


void
tracker_db_clear_temp (DBConnection *db_con)
{
	tracker_exec_sql (db_con, "Flush Table FilePending");
	tracker_exec_sql (db_con, "Flush Table FileWatches");
	tracker_exec_sql (db_con, "TRUNCATE TABLE FilePending");
	tracker_exec_sql (db_con, "TRUNCATE TABLE FileWatches");
}


void
tracker_db_check_tables (DBConnection *db_con)
{
	tracker_exec_sql (db_con, "select * from sequence limit 1");
	tracker_exec_sql (db_con, "select * from ServiceMetaData limit 1");
}


char ***
tracker_db_search_text (DBConnection *db_con, const char *service, const char *search_string, int offset, int limit, gboolean sort)
{
	char ***result;
	gboolean use_boolean_search = FALSE;

	/* check search string for embedded special chars like hyphens and format appropriately */
	char *search_term = tracker_format_search_terms (search_string, &use_boolean_search);

	char *str_limit = tracker_int_to_str (limit);
	char *str_offset = tracker_int_to_str (offset);
	char *str_sort = tracker_int_to_str (sort);
	char *str_bool =  tracker_int_to_str (use_boolean_search);


	result = tracker_exec_proc  (db_con, "SearchText", 6, service, search_term, str_offset, str_limit, str_sort, str_bool);	

	g_free (search_term);
	g_free (str_limit);
	g_free (str_offset);
	g_free (str_bool);
	g_free (str_sort);

	return result;

}

	
char ***
tracker_db_search_files_by_text (DBConnection *db_con, const char *text, int offset, int limit, gboolean sort)
{

	char *str_offset = tracker_int_to_str (offset);
	char *str_limit = tracker_int_to_str (limit);
	char *str_sort = tracker_int_to_str (sort);
	
	gboolean na;
	char *search_term = tracker_format_search_terms (text, &na);

	char ***res = tracker_exec_proc  (db_con,  "SearchFilesText", 4, search_term, str_offset, str_limit, str_sort);

	g_free (search_term);			
	g_free (str_limit);
	g_free (str_offset);
	g_free (str_sort);

	return res;
}

char ***
tracker_db_search_metadata (DBConnection *db_con, const char *service, const char *field, const char *text, int offset, int limit)
{

	char *str_offset = tracker_int_to_str (offset);	
	char *str_limit = tracker_int_to_str (limit);
	gboolean na;
	char *search_term = tracker_format_search_terms (text, &na);

	char ***res = tracker_exec_proc  (db_con,  "SearchMetaData", 5, service, field, search_term, str_offset, str_limit);
		
	g_free (search_term);		
	g_free (str_limit);
	g_free (str_offset);

	return res;
}

char ***
tracker_db_search_matching_metadata (DBConnection *db_con, const char *service, const char *id, const char *text)
{
	g_return_val_if_fail (id, NULL);

	char *name, *path;

	if (id[0] == '/') {
		name = g_path_get_basename (id);
		path = g_path_get_dirname (id);
	} else {
		name = tracker_get_vfs_name (id);
		path = tracker_get_vfs_path (id);
	}

	gboolean na;
	char *search_term = tracker_format_search_terms (text, &na);

	char ***res = tracker_exec_proc  (db_con,  "SearchMatchingMetaData", 4, service, path, name, search_term);
	
	g_free (search_term);			
	g_free (name);
	g_free (path);
	
	return res;
}



char ***
tracker_db_get_metadata (DBConnection *db_con, const char *service, const char *id, const char *key)
{
	g_return_val_if_fail (id, NULL);

	return tracker_exec_proc  (db_con, "GetMetadata", 3, service, id, key);	
}


void 
tracker_db_set_metadata (DBConnection *db_con, const char *service, const char *id, const char *key, const char *value, gboolean overwrite)
{
	char *str_write;	

	if (overwrite) {
		str_write = "1";
	} else {
		str_write = "0";
	}

	tracker_exec_proc  (db_con, "SetMetadata", 5, service, id, key, value, str_write);	
}

void 
tracker_db_create_service (DBConnection *db_con, const char *path, const char *name, const char *service,  gboolean is_dir, gboolean is_link, 
			   gboolean is_source,  int offset, long mtime)
{
	char *str_is_dir, *str_is_link, *str_is_source, *str_offset;

	char *str_mtime = g_strdup_printf ("%ld", mtime);

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

	tracker_exec_proc  (db_con, "CreateService", 8, path, name, service, str_is_dir, str_is_link, str_is_source, str_offset,  str_mtime);

	g_free (str_mtime);

	g_free (str_offset);

}


void
tracker_db_delete_file (DBConnection *db_con, long file_id)
{
	char *str_file_id = tracker_long_to_str (file_id);

	tracker_exec_proc  (db_con, "DeleteFile", 1,  str_file_id);

	g_free (str_file_id);
}

void
tracker_db_delete_directory (DBConnection *db_con, long file_id, const char *uri)
{
	char *str_file_id = tracker_long_to_str (file_id);

	tracker_exec_proc  (db_con, "DeleteDirectory", 2,  str_file_id, uri);

	g_free (str_file_id);
}



void
tracker_db_update_file (DBConnection *db_con, long file_id, long mtime)
{
	char *str_file_id = tracker_long_to_str (file_id);
	char *str_mtime = tracker_long_to_str (mtime);

	tracker_exec_proc  (db_con, "UpdateFile", 2, str_file_id, str_mtime);

	g_free (str_file_id);
	g_free (str_mtime);
}

gboolean 
tracker_db_has_pending_files (DBConnection *db_con)
{
	char ***res = NULL;
	char **  row;
	gboolean has_pending = FALSE;
	
	res = tracker_exec_proc (db_con, "ExistsPendingFiles", 0); 


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


gboolean 
tracker_db_has_pending_metadata (DBConnection *db_con)
{
	char ***res = NULL;
	char **  row;
	gboolean has_pending = FALSE;
	
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
	return tracker_exec_proc (db_con, "GetPendingFiles", 0);
}


void
tracker_db_remove_pending_files (DBConnection *db_con)
{
	tracker_exec_proc (db_con, "RemovePendingFiles", 0);
}



char ***
tracker_db_get_pending_metadata (DBConnection *db_con)
{
	return tracker_exec_proc (db_con, "GetPendingMetadataFiles", 0);
}


void
tracker_db_remove_pending_metadata (DBConnection *db_con)
{
	tracker_exec_proc (db_con, "RemovePendingMetadataFiles", 0);
}

void
tracker_db_insert_pending (DBConnection *db_con, const char *id, const char *action, const char *counter, const char *uri, const char *mime, gboolean is_dir)
{

	if (is_dir) {
		tracker_exec_proc  (db_con, "InsertPendingFile", 6, id, action, counter, uri, mime, "1");
	} else {
		tracker_exec_proc  (db_con, "InsertPendingFile", 6, id, action, counter, uri,  mime, "0");
	}
}	


void
tracker_db_update_pending (DBConnection *db_con, const char *counter, const char *action, const char *uri )
{
	tracker_exec_proc  (db_con, "UpdatePendingFile", 3, counter, action, uri);
}


char ***
tracker_db_get_files_by_service (DBConnection *db_con, const char *service, int offset, int limit)
{

	char *str_limit = tracker_int_to_str (limit);
	char *str_offset = tracker_int_to_str (offset);

	char ***res = tracker_exec_proc  (db_con,  "GetFilesByServiceType", 3, service, str_offset, str_limit);
		
	g_free (str_offset);
	g_free (str_limit);

	return res;

}


char ***
tracker_db_get_files_by_mime (DBConnection *db_con, char **mimes, int n, int offset, int limit, gboolean vfs)
{
	int i;
	char ***res = NULL;

	GString *str = g_string_new ("");

	str = g_string_append (str, mimes[0]);

	for (i=1; i<n; i++) {
		g_string_append_printf (str, ",%s", mimes[i]); 
	}


	char *str_mimes = g_string_free (str, FALSE);
	char *str_limit = tracker_int_to_str (limit);
	char *str_offset = tracker_int_to_str (offset);

	if (!vfs) {
		res = tracker_exec_proc  (db_con,  "GetFilesByMimeType", 3, str_mimes, str_offset, str_limit);
	} else {
		res = tracker_exec_proc  (db_con,  "GetVFSFilesByMimeType", 3, str_mimes, str_offset, str_limit);
	}
		
	g_free (str_mimes);
	g_free (str_limit);
	g_free (str_offset);

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

	char ***res = tracker_exec_proc  (db_con, "SearchTextMime", 2, search_term , mime_list);

	g_free (search_term);	
	g_free (mime_list);

	return res;
	
}


char ***
tracker_db_search_text_location  (DBConnection *db_con, const char *text ,const char *location)
{
	gboolean use_boolean_search;

	char *search_term = tracker_format_search_terms (text, &use_boolean_search);

	char ***res = tracker_exec_proc  (db_con, "SearchTextLocation", 2, search_term , location);

	g_free (search_term);

	return res;

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

	char ***res = tracker_exec_proc  (db_con, "SearchTextMimeLocation", 3, search_term , mime_list, location);

	g_free (search_term);	
	g_free (mime_list);

	return res;
	
}



void
tracker_db_update_file_move (DBConnection *db_con, long file_id, const char *path, const char *name, long mtime)
{
	char *str_file_id = g_strdup_printf ("%ld", file_id);
	char *index_time = g_strdup_printf ("%ld", mtime);

	tracker_exec_proc  (db_con, "UpdateFileMove", 4, str_file_id, path, name, index_time);

	g_free (str_file_id);
	g_free (index_time);
}

char ***
tracker_db_get_file_subfolders (DBConnection *db_con, const char *uri)
{
	return tracker_exec_proc (db_con, "SelectFileSubFolders", 1, uri);
}



