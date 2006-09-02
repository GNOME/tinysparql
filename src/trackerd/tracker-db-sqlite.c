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

gboolean
tracker_db_initialize (const char *datadir)
{
	tracker_log ("Using Sqlite version %s", sqlite3_version);
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


void
tracker_db_close (DBConnection *db_con)
{
	sqlite3_close (db_con->files_db);

}




DBConnection *
tracker_db_connect ()
{
	DBConnection *db_con = g_new (DBConnection, 1);

	char *dbname = g_build_filename (g_get_home_dir (), ".Tracker", "files.db", NULL);

	if (!sqlite3_open (dbname, &db_con->files_db)) {
		tracker_log ("Fatal Error : Can't open database: %s", sqlite3_errmsg (db_con->files_db);
		exit (1);
	}

	g_free (dbname);

	
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

	int i =  sqlite3_get_table (db_con->files_db, query, &array, &rows, &cols, &msg);

	while (i == SQLITE_BUSY) {
		g_usleep (1000);
		i = sqlite3_get_table (db_con->files_db, query, &array, &rows, &cols, &msg);
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
	char *str, *str2;
	int i,j;

	if (!in) {
		return NULL;
	}

	i = strlen (in);

	str = g_new (char, (i*2)+1);

	j = g_strdup (in);
	
	//j = mysql_real_escape_string (db_con->db, str, in, i);
	
	str2 = g_strndup (str, j);
	
	g_free (str);
	
	return str2;

} 

char ***
tracker_exec_proc (DBConnection *db_con, const char *procedure, int param_count, ...)
{
	va_list 	args;
	int 		i;
	char 		*str, *param, *query_str;
	char 	 	*query;
	MYSQL_RES 	*result;		


	va_start (args, param_count);

	query = g_hash_table_lookup (prepared_queries, procedure);
	
	if (!query) {
		tracker_log ("ERROR : query %s not found", procedure);
		return NULL;
	}

	

	for (i = 0; i < param_count; i++ ) {

		str = va_arg (args, char *);
		
		if (!str) {
			tracker_log ("Warning - parameter %d is null", i);
			param = NULL;	
		} else {
			param = tracker_escape_string (db_con, str);
		}

		if (i > 0) {
			g_string_append (query, ", ");
		}

		if (param) {
			g_string_append (query, "'");

			g_string_append (query, param);

			g_string_append (query, "'");
		
			g_free (param);

		} else {
			g_string_append (query, "NULL ");		
		}

		
	}
 	
	va_end (args);

	g_string_append (query, ")");

	query_str =  g_string_free (query, FALSE);

	result = tracker_exec_sql (db_con, query_str);

	g_free (query_str);

	return result;

}


void
tracker_db_load_stored_procs (DBConnection *db_con)
{
	char  *sql_file;
	char *query;
	char **queries, **queries_p ;
	char ***res;

	tracker_log ("loading queries...");
	sql_file = g_strdup (DATADIR "/tracker/sqlite-stored-procs.sql");
	
	if (!g_file_get_contents (sql_file, &query, NULL, NULL)) {
		tracker_log ("Tracker cannot read required file %s - Please reinstall tracker or check read permissions on the file if it exists", sql_file); 
		g_assert (FALSE);
	} else {

		prepared_queries = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

		queries = g_strsplit_set (query, "\n", -1);

		for (queries_p = queries; *queries_p; queries_p++) {

			if (*queries_p && (strlen (*queries_p) > 10)) {
				
				char *str = g_strdup (g_strstrip (*queries_p));
				char *sep;
		
				sep = strchr (str, ' ');
      						
				if (sep) {
					char *name = g_strndup (str, (int) (sep - str));
				 	char *query_body  = g_strdup (sep + 1);
					g_hash_table_insert (prepared_queries, name, query_body);
				}

				g_free (str):
			}
		}
		g_strfreev (queries);
		g_free (query);
	} 	
	
	g_free (sql_file);


}




void
tracker_create_db ()
{
	DBConnection *db_con;
	char  *sql_file;
	char *query;
	char **queries, **queries_p ;

	

	tracker_log ("Creating tracker database...");
	db_con = db_connect (NULL);
	tracker_exec_sql (db, "CREATE DATABASE tracker");
	mysql_close (db);
	db_con = db_connect ("tracker");
	sql_file = g_strdup (DATADIR "/tracker/mysql-tracker.sql");
	tracker_log ("Creating tables...");
	if (!g_file_get_contents (sql_file, &query, NULL, NULL)) {
		tracker_log ("Tracker cannot read required file %s - Please reinstall tracker or check read permissions on the file if it exists", sql_file); 
		g_assert (FALSE);
	} else {
		queries = g_strsplit_set (query, ";", -1);
		for (queries_p = queries; *queries_p; queries_p++) {
			if (*queries_p) {
		     		tracker_mysql_exec_sql (db_con->db, *queries_p);
			}
		}
		g_strfreev (queries);
		g_free (query);
		
	} 	
	
	g_free (sql_file);

	mysql_close (db_con->db);
	g_free (db_con);
}


void
tracker_log_sql	 (DBConnection *db_con, const char *query)
{
	MYSQL_RES *res = NULL;
	MYSQL_ROW  row, end_row;
	char *str = NULL;
	GString *contents;
	int num_fields;

	contents = g_string_new ("");

	res = tracker_mysql_exec_sql (db_con->db, query);

	if (!res) {
		return;
	}

	num_fields = mysql_num_fields(res);
   	
	while ((row = mysql_fetch_row (res))) {

		for (end_row = row + num_fields; row < end_row; ++row) {
			
			if (*row) {
				g_string_append (contents, (char *) *row);
			} else {
				g_string_append (contents, "NULL");
			} 

			g_string_append (contents, ", ");
	
		}
		g_string_append (contents, "\n");
	}
	
	str = g_string_free (contents, FALSE);
	tracker_log ("results of query %s is \n%s\n\n", query, str);
	g_free (str);
	mysql_free_result (res);
}

gboolean
tracker_update_db (DBConnection *db_con)
{
	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;	
	char  *sql_file;
	char *query;
	char **queries, **queries_p;
	gboolean refresh = FALSE;

	res = tracker_mysql_exec_sql  (db_con->db, "show tables");
	tracker_log ("Checking for Options table...");

	gboolean found_options = FALSE;
	if (res) {
		
		while ((row = mysql_fetch_row (res))) {
			if (((row[0]) && (strcmp (row[0], "Options") == 0))) {
				tracker_log ("Options table is present");
				found_options = TRUE;
				break;
			}	
		}
		
		mysql_free_result (res);

		if (!found_options) {
			tracker_log ("Cannot find Options table - your database is out of date and will need to be rebuilt");
			tracker_exec_sql (db, "Drop Database tracker");
			tracker_create_db ();
			return TRUE;
		}
	}

	res = tracker_mysql_exec_sql (db_con->db, "SELECT OptionValue FROM Options WHERE OptionKey = 'DBVersion'");
	
	if (!res) {
		return FALSE;
	}

	row = mysql_fetch_row (res);
	
	if (!row || !row[0]) {
		mysql_free_result (res);
		return FALSE;
	}

	char* version =  row[0];
	int i = atoi (version);
	mysql_free_result (res);

	tracker_log ("Checking tracker DB version...Current version is %d and needed version is %d", i, TRACKER_DB_VERSION_REQUIRED);
	if (i < 3) {
		tracker_log ("Your database is out of date and will need to be rebuilt and all your files reindexed.\nThis may take a while...please wait...");
		tracker_exec_sql (db, "Drop Database tracker");
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
	/* regenerate SPs */
	tracker_log ("Creating stored procedures...");
	sql_file = g_strdup (DATADIR "/tracker/mysql-stored-procs.sql");
	
	if (!g_file_get_contents (sql_file, &query, NULL, NULL)) {
		tracker_log ("Tracker cannot read required file %s - Please reinstall tracker or check read permissions on the file if it exists", sql_file); 
		g_assert (FALSE);
	} else {
		queries = g_strsplit_set (query, "|", -1);
		for (queries_p = queries; *queries_p; queries_p++) {
			if (*queries_p) {
		     		res = tracker_mysql_exec_sql (db_con->db, *queries_p);

				if (res) {
					mysql_free_result (res);
				}
			}
		}
		g_strfreev (queries);
		g_free (query);
	} 	
	
	g_free (sql_file);
	
	return refresh;
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
	

	memset (bind, 0, sizeof(bind));

	bind[0].buffer_type= MYSQL_TYPE_VAR_STRING;
	bind[0].buffer= str_file_id;
	bind[0].is_null= 0;
	bind[0].length = &file_id_length;

	bind[1].buffer_type= MYSQL_TYPE_VAR_STRING;
	bind[1].buffer= str_meta_id;
	bind[1].is_null= 0;
	bind[1].length = &meta_id_length;

	bind[2].buffer_type= MYSQL_TYPE_VAR_STRING;
	bind[2].length= &length;
	bind[2].is_null= 0;


	/* Bind the buffers */
	if (mysql_stmt_bind_param (db_con->insert_contents_stmt, bind)) {
		tracker_log ("binding error : %s\n", mysql_stmt_error (db_con->insert_contents_stmt));
		g_free (str_meta_id);
		g_free (str_file_id);
		fclose (file);
		
		return;
	}

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


