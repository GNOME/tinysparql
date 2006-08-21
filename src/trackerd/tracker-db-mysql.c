#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mysql/mysql.h>
#include "tracker-db-mysql.h"

gboolean 	use_nfs_safe_locking;

gboolean
tracker_db_initialize (const char *data_dir)
{
	/* mysql vars */
	static char **server_options;
	static char *server_groups[] = {"libmysqd_server", "libmysqd_client", NULL};

	char *str = g_strdup (DATADIR "/tracker/english");

	if (!tracker_file_is_valid (str)) {
		g_warning ("could not open mysql language file %s", str);
		return FALSE;
	}

	/* initialise embedded mysql with options*/
	server_options = g_new (char *, 11);
  	server_options[0] = "anything";
  	server_options[1] = g_strconcat  ("--datadir=", data_dir, NULL);
  	server_options[2] = "--myisam-recover=FORCE";
	server_options[3] = "--skip-grant-tables";
	server_options[4] = "--skip-innodb";
	server_options[5] = "--key_buffer_size=1M";
	server_options[6] = "--character-set-server=utf8";
	server_options[7] = "--ft_max_word_len=45";
	server_options[8] = "--ft_min_word_len=3";
	server_options[9] = "--ft_stopword_file=" DATADIR "/tracker/tracker-stop-words.txt";
	server_options[10] =  g_strconcat ("--language=", str,  NULL);


	mysql_server_init ( 11, server_options, server_groups);

	if (mysql_get_client_version () < 50019) {
		g_warning ("The currently installed version of mysql is too outdated (you need 5.0.19 or higher). Exiting...");
		return FALSE;
	}



	g_free (str);
		
	
	tracker_log ("DB initialised - embedded mysql version is %d", mysql_get_client_version () );

	return TRUE;

}


void
tracker_db_thread_init ()
{
	mysql_thread_init ();

}


void
tracker_db_thread_end ()
{
	mysql_thread_end ();

}


void
tracker_db_finalize ()
{
	mysql_server_end ();

}


void
tracker_db_close (DBConnection *db_con)
{
	mysql_close (db_con->db);

}


static DBConnection *
db_connect (const char* dbname)
{
	

	DBConnection *db_con = g_new (DBConnection, 1);

	db_con->db = mysql_init (NULL);
	
	if (!db_con->db) {
		tracker_log ( "Fatal error - mysql_init failed");
		exit (1);
	}

	mysql_options (db_con->db, MYSQL_OPT_USE_EMBEDDED_CONNECTION, NULL);


	if (!mysql_real_connect (db_con->db, NULL, "root", NULL, dbname, 0, NULL, CLIENT_MULTI_STATEMENTS)) {
    		tracker_log ("Fatal error : mysql_real_connect failed: %s", mysql_error (db_con->db));
		exit (1);
	}
	
	return db_con;
}

DBConnection *
tracker_db_connect ()
{
	return db_connect ("tracker");
}

static int
tracker_db_prepare_statement (MYSQL *db, MYSQL_STMT **stmt, const char *query) 
{

	*stmt = mysql_stmt_init (db);

	if (!*stmt) {
		tracker_log (" mysql_stmt_init(), out of memory");
  		exit (0);
	}

	if (mysql_stmt_prepare (*stmt, query, strlen (query)) != 0) {
		tracker_log (" mysql_stmt_prepare(), query failed due to %s", mysql_stmt_error (*stmt));
		return -5;
	}
	return mysql_stmt_param_count (*stmt);
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



static int
tracker_db_exec_stmt (MYSQL_STMT *stmt, int param_count,  ...)
{

	va_list 	args;
	MYSQL_BIND 	bind[16];
	unsigned long 	length[16];
	char 		params[16][2048];
	int 		i;
	char 		*str;


	if (param_count > 16) {
		tracker_log ("Too many parameters to execute query");
		return -1;
	}

	

	memset(bind, 0, sizeof(bind));

	va_start(args, param_count);
	
	for (i = 0; i < param_count; i++ ) {

		str = va_arg (args, char *);
		
		if (strlen(str) > 2048) {
			tracker_log ("Warning - length of parameter %s is too long", str);
		}

		strncpy (params[i], str, 2048);
		length[i] = strlen (params[i]);
	
	  	/* Bind input buffers */
		bind[i].buffer_type = MYSQL_TYPE_VAR_STRING;
		bind[i].buffer = (char *)params[i];
		bind[i].buffer_length = 2048;
		bind[i].is_null = 0;
		bind[i].length = &length[i];

	} 	
	
  	if (mysql_stmt_bind_param (stmt, bind)) {
		tracker_log ("bind failed");
		return -1;
	}


  	/* Execute the select statement */
	if (!lock_db ()) {
		return -1;
	}
  	if (mysql_stmt_execute (stmt)) {
		unlock_db ();
		tracker_log ("error %s occured whilst executing stmt",  mysql_stmt_error (stmt));
		return -1;
	}
	unlock_db ();  
	va_end (args);

	int result = mysql_stmt_affected_rows (stmt);
	if (mysql_stmt_free_result (stmt)) {
		tracker_log ("ERROR Freeing statement %s", mysql_stmt_error (stmt));
	}

	//mysql_stmt_reset (stmt);

	return result;
	
}



static char ***
tracker_db_exec_stmt_result (MYSQL_STMT *stmt, int param_count,  ...)
{

	va_list 	args;
	MYSQL_BIND 	bind[16];
	unsigned long 	length[16];
	gboolean	is_null[16];
	char 		params[16][255];
	MYSQL_RES     	*prepare_meta_result;
	int 		i,  column_count, row_count, row_num;
	char 		*str, **result;


	if (param_count > 16) {
		tracker_log ("Too many parameters to execute query");
		return NULL;
	}

	

	memset(bind, 0, sizeof(bind));

	va_start(args, param_count);
	
	for (i = 0; i < param_count; i++ ) {

		str = va_arg (args, char *);
		
		if (strlen(str) > 254) {
			tracker_log ("Warning - length of parameter %s is too long", str);
		}

		strncpy (params[i], str, 255);

		length[i] = strlen (params[i]);
	
	  	/* Bind input buffers */
		bind[i].buffer_type = MYSQL_TYPE_VAR_STRING;
		bind[i].buffer = (char *)params[i];
		bind[i].buffer_length = 255;
		bind[i].is_null = 0;
		bind[i].length = &length[i];

	} 	
	
  	if (mysql_stmt_bind_param (stmt, bind)) {
		tracker_log ("bind failed");
	}

	prepare_meta_result = mysql_stmt_result_metadata (stmt);

	column_count = mysql_num_fields (prepare_meta_result);

  	/* Execute the select statement */
	if (!lock_db ()) {
		return NULL;
	}
  	if (mysql_stmt_execute (stmt)) {
		tracker_log ("error executing stmt");
	}
	unlock_db ();
  
	va_end (args);

	memset(bind, 0, sizeof(bind));


	for (i = 0; i < column_count; i++ ) {
		bind[i].buffer_type= MYSQL_TYPE_VAR_STRING;
		bind[i].buffer= (char *)params[i];
		bind[i].buffer_length= 255;
		bind[i].is_null = (my_bool*) &is_null[i];
		bind[i].length = &length[i];
	}

 	/* Bind the result buffers */
	if (mysql_stmt_bind_result (stmt, bind)) {
  		tracker_log (" mysql_stmt_bind_result() failed");
		tracker_log ( " %s", mysql_stmt_error(stmt));
		return NULL;
	}

	/* Now buffer all results to client */
	if (mysql_stmt_store_result (stmt)) {
		tracker_log ( " mysql_stmt_store_result() failed");
		tracker_log ( " %s", mysql_stmt_error(stmt));
		return NULL;
	}

	/* prepare result set string array */
	row_count = mysql_stmt_num_rows (stmt);

	if (!row_count > 0) {
		mysql_free_result (prepare_meta_result);
		mysql_stmt_free_result (stmt);
		return NULL;
	}

	result = g_new ( char *, row_count + 1);
	result [row_count] = NULL;

	/* Fetch all rows */
	row_num = 0;

	while (!mysql_stmt_fetch (stmt)) {
		char **row = g_new (char *, column_count + 1);
		row [column_count] = NULL;

		for (i = 0; i < column_count; i++ ) {
			if (length[i] > 10000) {
				row[i] = g_strndup (params[i], 10000);
			} else {
				row[i] = g_strndup (params[i], length[i]);
			}
		}
	
		result[row_num]  = (gpointer)row;
		row_num++;
	}


	/* Free the prepared result metadata */
	mysql_free_result (prepare_meta_result);

	if (mysql_stmt_free_result (stmt)) {
		tracker_log ("ERROR Freeing statement");
	}

	//mysql_stmt_reset (stmt);

	return (char ***)result;
}
 

void
tracker_db_prepare_queries (DBConnection *db_con)
{
	
	/* prepare queries to be used and bomb out if queries contain sql errors or erroneous parameter counts */

	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->insert_contents_stmt, INSERT_CONTENTS) == 3);

}




static MYSQL_RES *
tracker_mysql_exec_sql (MYSQL *db, const char *query)
{
	MYSQL_RES *res = NULL;
	GTimeVal before, after;
	double elapsed;


	g_return_val_if_fail (query, NULL);

	//tracker_log ("executing query:\n%s\n", query);
	g_get_current_time (&before);
	if (!lock_db ()) {
		return NULL;
	}


	if (mysql_query (db, query) != 0) {
		unlock_db ();
    		tracker_log ("tracker_exec_sql failed: %s [%s]", mysql_error (db), query);
		return res;
	}
	unlock_db ();

	g_get_current_time (&after);

	elapsed =  (1000 * (after.tv_sec - before.tv_sec))  +  ((after.tv_usec - before.tv_usec) / 1000);

//	tracker_log ("Query execution time is %f ms\n\n", elapsed);

	if (mysql_field_count (db) > 0) {
	    	if (!(res = mysql_store_result (db))) {
			tracker_log ("tracker_exec_sql failed: %s [%s]", mysql_error (db), query);
		} 
	}
	return res;

}



char ***
tracker_exec_sql (DBConnection *db_con, const char *query)
{

	MYSQL_RES *res = NULL;
	MYSQL_ROW  myrow;
	char **result = NULL;
	int row_num = -1;

	res = tracker_mysql_exec_sql (db_con->db, query);

	if (res) {
		
		int column_count = mysql_num_fields(res);
	   	int row_count = mysql_num_rows (res);

		result = g_new ( char *, row_count + 1);
		result [row_count] = NULL;	

		while ((myrow = mysql_fetch_row (res))) {
			row_num++;

			char **row = g_new (char *, column_count + 1);

			row [column_count] = NULL;

			int i;
			for (i = 0; i < column_count; i++ ) {
				row[i] = g_strdup (myrow[i]);
			}

			result[row_num]  = (gpointer)row;

		}
	
		mysql_free_result (res);

	}

	return (char ***)result;

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
	
	j = mysql_real_escape_string (db_con->db, str, in, i);
	
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
	GString 	*query;
	char 		***result;		


	va_start (args, param_count);

	query = g_string_new ("Call ");

	g_string_append (query, procedure);

	g_string_append (query, "(");
	
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

static void
create_system_db ()
{

	DBConnection *db_con;
	char  *sql_file;
	char *query;
	char **queries, **queries_p ;


	tracker_log ("Creating system database...");
	db_con = db_connect (NULL);
	tracker_exec_sql (db_con, "CREATE DATABASE mysql");
	mysql_close (db_con->db);
	db_con = db_connect ("mysql");

	sql_file = g_strdup (DATADIR "/tracker/mysql-system.sql");
	
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
tracker_db_load_stored_procs (DBConnection *db_con)
{
	char  *sql_file;
	char *query;
	char **queries, **queries_p ;
	MYSQL_RES *res = NULL;

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


}


static void
create_tracker_db ()
{
	DBConnection *db_con;
	char  *sql_file;
	char *query;
	char **queries, **queries_p ;

	

	tracker_log ("Creating tracker database...");
	db_con = db_connect (NULL);
	tracker_mysql_exec_sql (db_con->db, "CREATE DATABASE tracker");
	mysql_close (db_con->db);
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
tracker_create_db ()
{
	create_system_db ();
	create_tracker_db ();
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
			tracker_mysql_exec_sql (db_con->db, "Drop Database tracker");
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
		tracker_exec_sql (db_con, "Drop Database tracker");
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


	file = fopen (file_name,"r");
	//tracker_log ("saving text to db with file_id %ld", file_id);

	if (!file) {
		tracker_log ("Could not open file %s", file_name);
		return; 
	}

	char ***res = tracker_exec_proc  (db_con, "GetMetadataTypeInfo", 1, "File.Content");	
	char **row, ***rows;

	if (res) {
		row = res[0];
	}

	if (res && row && row[0]) {
		str_meta_id = g_strdup (row[0]);		
		for (rows  = res; *rows; rows++) {
			if (*rows) {
				g_strfreev  (*rows);
			}
		}
		g_free (res);

	} else {
		tracker_log ("Could not get metadata for File.Content");
		return; 
	}
	

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
