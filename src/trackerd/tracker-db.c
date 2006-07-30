#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mysql/mysql.h>
#include "tracker-db.h"

GMutex		*metadata_available_mutex;
GMutex		*files_available_mutex;

typedef struct {

	DBConnection 	*db_con;
	char 		*file_id;
} DatabaseAction;

gboolean 	use_nfs_safe_locking;


static MYSQL *
db_connect (const char* dbname)
{
	

	MYSQL *db = mysql_init (NULL);
	
	if (!db) {
		tracker_log ( "Fatal error - mysql_init failed due to no memory available");
		exit (1);
	}

	mysql_options (db, MYSQL_OPT_USE_EMBEDDED_CONNECTION, NULL);


	if (!mysql_real_connect (db, NULL, "root", NULL, dbname, 0, NULL, CLIENT_MULTI_STATEMENTS)) {
    		tracker_log ("Fatal error : mysql_real_connect failed: %s", mysql_error (db));
		exit (1);
	}
	
	return db;
}

MYSQL *
tracker_db_connect ()
{
	return db_connect ("tracker");
}

int
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



char ***
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



char *
tracker_db_get_id (DBConnection *db_con, const char* service, const char *uri)
{

	int service_id = tracker_str_in_array (service, serice_index_array);

	if ( service_id == -1) {
		return NULL;
	}

	

	if (tracker_str_in_array (service, file_service_array) != -1) {
		int id = tracker_db_get_file_id (db_con, uri);

		if (id != -1) {
			return g_strdup_printf ("%d", id);
		}
	}


	return NULL;

}



MYSQL_RES *
tracker_exec_sql (MYSQL *db, const char *query)
{
	MYSQL_RES *res = NULL;
	GTimeVal before, after;
	double elapsed;

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

char *
tracker_escape_string (MYSQL *db, const char *in)
{
	char *str, *str2;
	int i,j;

	if (!in) {
		return NULL;
	}

	i = strlen (in);

	str = g_new (char, (i*2)+1);
	
	j = mysql_real_escape_string (db, str, in, i);
	
	str2 = g_strndup (str, j);
	
	g_free (str);
	
	return str2;

} 

MYSQL_RES *
tracker_exec_proc (MYSQL *db, const char *procedure, int param_count, ...)
{
	va_list 	args;
	int 		i;
	char 		*str, *param, *query_str;
	GString 	*query;
	MYSQL_RES 	*result;		


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
			param = tracker_escape_string (db, str);
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

	result = tracker_exec_sql (db, query_str);

	g_free (query_str);

	return result;

}


int
tracker_db_get_file_id (DBConnection *db_con, const char* uri)
{
	
	char *path;
	char *name;
	int id;

	if (!db_con || !uri) {
		return -1;
	}

	if (uri[0] == '/') {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;


	res = tracker_exec_proc  (db_con->db, "GetServiceID", 2, path, name);

	if (res) {
		
		row = mysql_fetch_row (res);

		if (row && row[0]) {
			id = atoi (row[0]); 
		} else {
			id = -1;
		}

		if (id < 1) {
			id = -1;
		}

		mysql_free_result (res);
	}			
 

	g_free (path);
	g_free (name);

	return id;

}



FileInfo *
tracker_db_get_file_info (DBConnection *db_con, FileInfo *info)
{
	if (!db_con || !info || !tracker_file_info_is_valid (info)) {
		return info;
	}

	char *name = g_path_get_basename (info->uri);
	char *path = g_path_get_dirname (info->uri);

	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;

	char* apath = tracker_escape_string (db_con->db, path);
	char* aname = tracker_escape_string (db_con->db, name);

	char *query = g_strconcat ("SELECT ID, IndexTime, IsDirectory FROM Files WHERE Path = '", apath ,"'  AND FileName = '", aname , "'", NULL);

	g_free (aname);
	g_free (apath);

	res = tracker_exec_proc  (db_con->db, "GetServiceID", 2, path, name);
	g_free (query);

	if (res) {

		row = mysql_fetch_row (res);

		if (row && row[0]) {
			info->file_id = atol (row[0]); 
		}			

		if (row && row[1]) {
			info->indextime = atoi (row[1]); 
		}			

		if (row && row[2]) {
			info->is_directory = (strcmp (row[2], "1") == 0) ; 
		}			

		mysql_free_result (res);
	}


	g_free (name);
	g_free (path);

	return info;

}

static void
create_system_db ()
{
	MYSQL *db;
	char  *sql_file;
	char *query;
	char **queries, **queries_p ;


	tracker_log ("Creating system database...");
	db = db_connect (NULL);
	tracker_exec_sql (db, "CREATE DATABASE mysql");
	mysql_close (db);
	db = db_connect ("mysql");

	sql_file = g_strdup (DATADIR "/tracker/mysql-system.sql");
	
	if (!g_file_get_contents (sql_file, &query, NULL, NULL)) {
		tracker_log ("Tracker cannot read required file %s - Please reinstall tracker or check read permissions on the file if it exists", sql_file); 
		g_assert (FALSE);
	} else {
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

	mysql_close (db);

}


void
tracker_create_db ()
{
	MYSQL *db;
	char  *sql_file;
	char *query;
	char **queries, **queries_p ;

	create_system_db ();

	tracker_log ("Creating tracker database...");
	db = db_connect (NULL);
	tracker_exec_sql (db, "CREATE DATABASE tracker");
	mysql_close (db);
	db = db_connect ("tracker");
	sql_file = g_strdup (DATADIR "/tracker/mysql-tracker.sql");
	
	if (!g_file_get_contents (sql_file, &query, NULL, NULL)) {
		tracker_log ("Tracker cannot read required file %s - Please reinstall tracker or check read permissions on the file if it exists", sql_file); 
		g_assert (FALSE);
	} else {
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

	mysql_close (db);
	
}


void
tracker_log_sql	 (MYSQL *db, const char *query)
{
	MYSQL_RES *res = NULL;
	MYSQL_ROW  row, end_row;
	char *str = NULL;
	GString *contents;
	int num_fields;

	contents = g_string_new ("");

	res = tracker_exec_sql (db, query);

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
tracker_update_db (MYSQL *db)
{
	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;	
	char  *sql_file;
	char *query;
	char **queries, **queries_p;


	res = tracker_exec_sql  (db, "show tables");
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

	res = tracker_exec_sql (db, "SELECT OptionValue FROM Options WHERE OptionKey = 'DBVersion'");
	
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


	/* apply and table changes for each version update */
	while (i < TRACKER_DB_VERSION_REQUIRED) {
			
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
		     		res = tracker_exec_sql (db, *queries_p);

				if (res) {
					mysql_free_result (res);
				}
			}
		}
		g_strfreev (queries);
		g_free (query);
	} 	
	
	g_free (sql_file);
	



	return FALSE;
}



static void
get_meta_table_data (gpointer key,
   		     gpointer value,
		     gpointer user_data)
{
	DatabaseAction *db_action;	
	char *mtype, *mvalue, *avalue, *dvalue = NULL, *evalue;
	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;	

	mtype = (char *)key;
	avalue = (char *)value;

	db_action = user_data;

	if (mtype == NULL || avalue == NULL) {
		return;
	}

	if (tracker_metadata_is_date (db_action->db_con, mtype)) {

		dvalue = tracker_format_date (avalue);		
		
		if (dvalue) {

			time_t l = tracker_str_to_date (dvalue);

			g_free (dvalue);

			if (l == -1) {
				return;
			} else {
				evalue = tracker_long_to_str (l);
			}

		} else {
			return;
		}

		
	} else {
		evalue = g_strdup (avalue);
	}

	mvalue = tracker_escape_string (db_action->db_con->db, evalue);

	if (evalue) {
		g_free (evalue);
	}
 
	res = tracker_exec_proc  (db_action->db_con->db, "SetMetadata", 5, "Files", db_action->file_id, mtype, mvalue, "1");

	if (res) {
		
		if ((row = mysql_fetch_row (res))) {
			if (row[0]) {
				tracker_log ("Error %s saving metadata %s with value %s", row[0], mtype, mvalue);
			}

		}
		mysql_free_result (res);
	}

	if (mvalue) {
		g_free (mvalue);
	}



}

void
tracker_db_save_metadata (DBConnection *db_con, GHashTable *table, long file_id)
{
	
	DatabaseAction db_action;
	
	g_return_if_fail (file_id != -1 || table || db_con);

	db_action.db_con = db_con;

	db_action.file_id = g_strdup_printf ("%ld", file_id);

	if (table) {
		g_hash_table_foreach (table, get_meta_table_data, &db_action);
	}
	
	g_free (db_action.file_id);
	
}


void
tracker_db_save_thumbs	(DBConnection *db_con, const char *small_thumb, const char *large_thumb, long file_id)
{
	char *str_file_id;
	char *small_thumb_file;
	char *large_thumb_file;

	str_file_id = g_strdup_printf ("%ld", file_id);

	if (small_thumb) {
		small_thumb_file = tracker_escape_string (db_con->db, small_thumb);
		tracker_exec_proc  (db_con->db, "SetMetadata", 5, "Files", str_file_id, "File.SmallThumbnailPath", small_thumb_file, "1");	
		g_free (small_thumb_file);
	}



	if (large_thumb) {
		large_thumb_file = tracker_escape_string (db_con->db, large_thumb);
		tracker_exec_proc  (db_con->db, "SetMetadata", 5, "Files", str_file_id, "File.LargeThumbnailPath", large_thumb_file, "1");	
		g_free (large_thumb_file);
	}


	g_free (str_file_id);
	


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


char **
tracker_db_get_files_in_folder (DBConnection *db_con, const char *folder_uri)
{

	int 		i, row_count;
	char		 **array = NULL;

	g_return_val_if_fail (db_con && folder_uri && (strlen (folder_uri) > 0), NULL);

	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;

	res = tracker_exec_proc  (db_con->db, "SelectFileChild", 1, folder_uri);	

	if (res) {

		row_count = mysql_num_rows (res);
	
		if (row_count > 0) {

			array = g_new (char *, row_count +1);

			i = 0;

			while ((row = mysql_fetch_row (res))) {
				
				if (row[1] && row[2]) {
					array[i] = g_build_filename (row[1], row[2], NULL);

				} else {
					array[i] = NULL;
				}
				i++;
			}
			array [row_count] = NULL;
			
		} else {
			array = g_new (char *, 1);
			array[0] = NULL;
		}

		mysql_free_result (res);
			
	} else {
		array = g_new (char *, 1);
		array[0] = NULL;
	}


	return array;

} 



FieldDef *
tracker_db_get_field_def (DBConnection *db_con, const char *field_name)
{
	FieldDef *def;

	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;


	def = g_new (FieldDef, 1);

	res = tracker_exec_proc  (db_con->db, "GetMetadataTypeInfo", 1, field_name);	

	if (res) {
		row = mysql_fetch_row (res);
	}

	if (res && row && row[0]) {
		def->id = g_strdup (row[0]); 
	} else {
		g_free (def);
		mysql_free_result (res);
		return NULL;
	}			

	if (res && row && row[1]) {
		def->type = atoi (row[1]); 
	}			

	if (res && row && row[2]) {
		def->embedded = (strcmp ("1" , row[2]) == 0); 
	}			

	if (res && row && row[3]) {
		def->writeable = (strcmp ("1" , row[3]) == 0); 
	}			

	
		
	mysql_free_result (res);


	return def;
}

void
tracker_db_free_field_def (FieldDef *def)
{

	g_return_if_fail (def);
	
	if (def->id) {
		g_free (def->id);
	}

	g_free (def);

}


gboolean
tracker_metadata_is_date (DBConnection *db_con, const char* meta) 
{
	FieldDef *def;	
	gboolean res = FALSE;

	def = tracker_db_get_field_def (db_con, meta);

	res = (def->type == DATA_DATE);

	tracker_db_free_field_def (def);

	return res;

}

FileInfo *
tracker_db_get_pending_file (DBConnection *db_con, const char *uri)
{
	FileInfo *info = NULL;

	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;

	res = tracker_exec_proc  (db_con->db, "SelectPendingByUri", 1, uri);	

	if (res) {
		row = mysql_fetch_row (res);

		if (row && row[0] &&  row[1] && row[2] && row[3] && row[4]) {
			info = tracker_create_file_info (uri, atoi(row[2]), 0, 0);
			info->mime = g_strdup (row[3]);
			info->is_directory =  (strcmp (row[4], "0") == 0);
		}	

		mysql_free_result (res);
	}
		


	return info;

}



static void
make_pending_file (DBConnection *db_con, long file_id, const char *uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory)
{
	
	char *str_file_id, *str_action, *str_is_directory, *str_counter;

	g_return_if_fail (uri);

	str_file_id = g_strdup_printf ("%ld", file_id);
	str_action = g_strdup_printf ("%d", action);
	str_is_directory = g_strdup_printf ("%d", is_directory);
	str_counter = g_strdup_printf ("%d", counter);

	if (!mime) {
		tracker_exec_proc  (db_con->db, "InsertPendingFile", 6, str_file_id, str_action, str_counter, uri,  "unknown", str_is_directory);
	} else {
		tracker_exec_proc  (db_con->db, "InsertPendingFile", 6, str_file_id, str_action, str_counter, uri,  mime, str_is_directory);
	}

	//tracker_log ("inserting pending file for %s with action %s", uri, tracker_actions[action]);

	/* signal respective thread that data is available if its waiting */
	if (action == TRACKER_ACTION_EXTRACT_METADATA) {
		g_mutex_trylock (metadata_available_mutex);
	} else {
		g_mutex_trylock (files_available_mutex);
	}

	g_free (str_file_id);
	g_free (str_action);
	g_free (str_counter);
	g_free (str_is_directory);
}


void
tracker_db_update_pending_file (DBConnection *db_con, const char* uri, int counter, TrackerChangeAction action)
{

	char *str_counter;
	char *str_action;

	str_counter = g_strdup_printf ("%d", counter);
	str_action = g_strdup_printf ("%d", action);

	tracker_exec_proc  (db_con->db, "UpdatePendingFile", 3, str_counter, str_action, uri);

	g_free (str_counter);
	g_free (str_action);


}

void 
tracker_db_insert_pending_file (DBConnection *db_con, long file_id, const char *uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory)
{
	FileInfo *info = NULL;

	/* check if uri already has a pending action and update accordingly */
	info = tracker_db_get_pending_file (db_con, uri);
	
	if (info) {
		switch (action) {

		case TRACKER_ACTION_FILE_CHECK:


			/* update counter for any existing event in the file_scheduler */
									
			if ((info->action == TRACKER_ACTION_FILE_CHECK) || 
			    (info->action == TRACKER_ACTION_FILE_CREATED) ||
			    (info->action == TRACKER_ACTION_FILE_CHANGED)) {  
					
				tracker_db_update_pending_file (db_con, uri, counter, action);
			}

			break;


		case TRACKER_ACTION_FILE_CHANGED:

			tracker_db_update_pending_file (db_con, uri, counter, action);

			break;


		case TRACKER_ACTION_WRITABLE_FILE_CLOSED :

			tracker_db_update_pending_file (db_con, uri, 0, action);
						
			break;

		case TRACKER_ACTION_FILE_DELETED :
		case TRACKER_ACTION_FILE_CREATED :
		case TRACKER_ACTION_DIRECTORY_DELETED :
		case TRACKER_ACTION_DIRECTORY_CREATED :

			/* overwrite any existing event in the file_scheduler */
			tracker_db_update_pending_file (db_con, uri, 0, action);

			break;

		case TRACKER_ACTION_EXTRACT_METADATA :

			/* we only want to continue extracting metadata if file is not being changed/deleted in any way */
			if (info->action == TRACKER_ACTION_FILE_CHECK)	{
				tracker_db_update_pending_file (db_con, uri, 0, action);
			}					

			break;

		default :
			break;
		}

		info = tracker_free_file_info (info);

	} else {
		make_pending_file (db_con, file_id, uri, mime, counter, action, is_directory);
	}

}


gboolean
tracker_is_valid_service (DBConnection *db_con, const char *service)
{
	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;
	gboolean result = FALSE;

	res = tracker_exec_proc  (db_con->db, "ValidService", 1, service);	

	if (res) {
		row = mysql_fetch_row (res);

		if (row && row[0]) {
			if (strcmp (row[0], "1") == 0) {  
				result = TRUE;
			}
		}	

		mysql_free_result (res);
	}

	return result;

}
