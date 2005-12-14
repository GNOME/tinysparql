#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tracker-db.h"


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


	if (!mysql_real_connect (db, NULL, "root", NULL, dbname, 0, NULL, 0)) {
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
	int attempt, fd, fd2;	
	char *lock_file, *tmp, *tmp_file;
	
	if (!use_nfs_safe_locking) {
		return TRUE;
	}

	lock_file = g_build_filename (g_get_home_dir (), ".Tracker", "tracker.lock", NULL);
	tmp = g_build_filename (g_get_home_dir (), ".Tracker", g_get_host_name (), NULL);
	tmp_file = g_strdup_printf ("%s_%ld.lock", tmp, (long)getpid ());
	g_free (tmp);

	for ( attempt=0; attempt < 1000; ++attempt) {
		
		/* delete existing lock file if older than 10 secs */
		if (g_file_test (lock_file, G_FILE_TEST_EXISTS) && ( time((time_t *)NULL) - get_mtime (lock_file)) > 10) {
			unlink (lock_file); 
		}

		fd = open (lock_file, O_CREAT|O_EXCL, 0644);
		if (fd >= 0) {
		
			/* create host specific file and link to lock file */
			link ( lock_file, tmp_file);
			
			/* for atomic NFS safe locks, stat links = 2 if file locked. If greater than 2 then we have a race condition */
			if ( get_nlinks (lock_file) == 2) {
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
	char 		params[16][255];
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
		
		if (strlen(str) > 255) {
			tracker_log ("Warning - length of parameter %s is too long", str);
		}

		strncpy (params[i], str, 255);
		length[i] = strlen (params[i]);
	
	  	/* Bind input buffers */
		bind[i].buffer_type = MYSQL_TYPE_STRING;
		bind[i].buffer = (char *)params[i];
		bind[i].buffer_length = 255;
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

	return mysql_stmt_affected_rows (stmt);
	
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
		bind[i].buffer_type = MYSQL_TYPE_STRING;
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
		bind[i].buffer_type= MYSQL_TYPE_STRING;
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
	mysql_free_result(prepare_meta_result);
	mysql_stmt_free_result (stmt);
	return (char ***)result;
}
 

void
tracker_db_prepare_queries (DBConnection *db_con)
{

	/* prepare queries to be used */

	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->select_file_id_stmt, SELECT_FILE_ID) == 2);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->select_file_child_stmt, SELECT_FILE_CHILD) == 1);
	//g_assert (tracker_db_prepare_statement (db_con->db, &db_con->select_file_watches_stmt, SELECT_FILE_WATCHES) == 0);

	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->insert_file_stmt, INSERT_FILE) == 6);

	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->update_file_stmt, UPDATE_FILE) == 2);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->update_file_move_stmt, UPDATE_FILE_MOVE) == 4);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->update_file_watch_stmt, UPDATE_FILE_WATCH) == 3);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->update_file_watch_child_stmt, UPDATE_FILE_WATCH_CHILD) == 4);

	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->delete_file_stmt, DELETE_FILE) == 1);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->delete_file_child_stmt, DELETE_FILE_CHILD) == 2);

	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->select_metadata_stmt, SELECT_METADATA) == 2);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->select_metadata_indexed_stmt, SELECT_METADATA_INDEXED) == 2);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->select_metadata_integer_stmt, SELECT_METADATA_INTEGER) == 2);

	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->insert_metadata_stmt, INSERT_METADATA) == 3);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->insert_metadata_indexed_stmt, INSERT_METADATA_INDEXED) == 3);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->insert_metadata_integer_stmt, INSERT_METADATA_INTEGER) == 3);

	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->delete_metadata_derived_stmt, DELETE_METADATA_DERIVED) == 1);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->delete_metadata_all_stmt, DELETE_METADATA_ALL) == 1);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->delete_metadata_child_all_stmt, DELETE_METADATA_CHILD_ALL) == 2);

	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->select_metadata_type_stmt, SELECT_METADATA_TYPE) == 1);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->select_metadata_type_like_stmt, SELECT_METADATA_TYPE_LIKE) == 1);	
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->select_metadata_type_file_stmt, SELECT_METADATA_TYPE_FILE) == 1);
	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->insert_metadata_type_stmt, INSERT_METADATA_TYPE) == 4);

	g_assert (tracker_db_prepare_statement (db_con->db, &db_con->select_search_text_stmt, SELECT_SEARCH_TEXT) == 1);
}


int
tracker_db_get_file_id (DBConnection *db_con, const char* uri)
{
	char ***res_str = NULL;
	char *path;
	char *name;
	int id;

	if (!db_con || !uri) {
		return -1;
	}

	/* TO DO  - file might contain a file uri header (file://) that needs filtering out*/
	path = g_path_get_dirname (uri);
	name = g_path_get_basename (uri);
	
	res_str = tracker_db_exec_stmt_result (db_con->select_file_id_stmt, 2, path, name);	
		
	if (res_str && res_str[0] && res_str[0][0]) {
		id = atoi (res_str[0][0]); 
	} else {
		id = -1;
	}			

	g_free (path);
	g_free (name);
	
	tracker_db_free_result (res_str);
	
	return id;

}


FileInfo *
tracker_db_get_file_info (DBConnection *db_con, FileInfo *info)
{
	char ***res_str = NULL;

	if (!db_con || !info->path || !info->name || !tracker_file_info_is_valid (info)) {
		return info;
	}

	res_str = tracker_db_exec_stmt_result (db_con->select_file_id_stmt, 2, info->path, info->name);	
		
	if (res_str && res_str[0] && res_str[0][0]) {
		info->file_id = atol (res_str[0][0]); 
	}			

	if (res_str && res_str[0] && res_str[0][1]) {
		info->indextime = atoi (res_str[0][1]); 
	}			

	if (res_str && res_str[0] && res_str[0][2]) {
		info->is_directory = (strcmp (res_str[0][2], "1") == 0) ; 
	}			

	tracker_db_free_result (res_str);

	return info;

}

MYSQL_RES *
tracker_exec_sql (MYSQL *db, const char *query)
{
	MYSQL_RES *res = NULL;
	GTimeVal before, after;
	double elapsed;

	tracker_log ("executing query:\n%s\n", query);
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

	tracker_log ("Query execution time is %f ms\n\n", elapsed);

	if (mysql_field_count (db) > 0) {
	    	if (!(res = mysql_store_result (db))) {
			tracker_log ("tracker_exec_sql failed: %s [%s]", mysql_error (db), query);
		} 
	}
	return res;

}


void
tracker_create_db ()
{
	MYSQL *db;
	char  *sql_file;
	char *query;
	char **queries, **queries_p ;

	tracker_log ("creating database");
	db = db_connect (NULL);
	tracker_exec_sql (db, "CREATE DATABASE tracker");
	mysql_close (db);
	db = db_connect ("tracker");
	sql_file = g_strdup (DATADIR "/tracker/mysql-tracker.sql");
	
	if (!g_file_get_contents (sql_file, &query, NULL, NULL)) {
		tracker_log ("Unable to load sql query file %s", sql_file); 
	} else {
		queries = g_strsplit_set (query, ";", -1);
		for (queries_p = queries; *queries_p; queries_p++) {
			if (*queries_p) {
		     		tracker_exec_sql (db, *queries_p);
			}
		}
		g_strfreev (queries);
		
	} 	
	
	g_free (sql_file);
	g_free (query);
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


static char *
escape_string (MYSQL *db, const char *in)
{
	char *str, *str2;
	int i,j;

	i = strlen (in);

	str = g_new (char, (i*2)+1);
	
	j = mysql_real_escape_string (db, str, in, i);
	
	str2 = g_strndup (str, j);
	
	g_free (str);
	
	return str2;

}

static void
get_meta_table_data (gpointer key,
   		     gpointer value,
		     gpointer user_data)
{
	DatabaseAction *db_action;	
	FieldDef *def;
	char *mtype, *mvalue, *avalue;
	
	mtype = (char *)key;
	avalue = (char *)value;

	db_action = user_data;

	mvalue = escape_string (db_action->db_con->db, avalue);

	g_return_if_fail (mtype != NULL && mvalue != NULL);

	g_assert (db_action->db_con);
	g_assert (db_action->db_con->select_metadata_type_stmt);

	def = tracker_db_get_field_def (db_action->db_con, mtype); 

	if (def && def->id) {
	
		if (def->indexable) {
			//tracker_log ("saving indexable %s : %s", key, mvalue);
			tracker_db_exec_stmt (db_action->db_con->insert_metadata_indexed_stmt, 3, db_action->file_id, def->id, mvalue);
		
		} else if (def->type != DATA_INTEGER) {
			//tracker_log ("saving normal %s : %s", key, mvalue);
			tracker_db_exec_stmt (db_action->db_con->insert_metadata_stmt, 3, db_action->file_id, def->id, mvalue);	
				
		} else {
			tracker_db_exec_stmt (db_action->db_con->insert_metadata_integer_stmt, 3, db_action->file_id, def->id, mvalue);
		}

	}
	
	if (mvalue) {
		g_free (mvalue);
	}

	tracker_db_free_field_def (def);

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
	FieldDef *def;

	small_thumb_file = escape_string (db_con->db, small_thumb);
	large_thumb_file = escape_string (db_con->db, large_thumb);

	str_file_id = g_strdup_printf ("%ld", file_id);

	def = tracker_db_get_field_def (db_con, "File.SmallThumbnailPath"); 

	if (def) {
		tracker_db_exec_stmt (db_con->insert_metadata_stmt, 3,  str_file_id, def->id, small_thumb_file);

		tracker_db_free_field_def (def);				
	}

	def = tracker_db_get_field_def (db_con, "File.LargeThumbnailPath"); 

	if (def) {
		tracker_db_exec_stmt (db_con->insert_metadata_stmt, 3,  str_file_id, def->id, large_thumb_file);

		tracker_db_free_field_def (def);
	}

	g_free (str_file_id);
	g_free (small_thumb_file);
	g_free (large_thumb_file);

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
	tracker_log ("saving text to db with file_id %ld", file_id);

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

	bind[0].buffer_type= MYSQL_TYPE_STRING;
	bind[0].buffer= str_file_id;
	bind[0].is_null= 0;
	bind[0].length = &file_id_length;

	bind[1].buffer_type= MYSQL_TYPE_STRING;
	bind[1].buffer= str_meta_id;
	bind[1].is_null= 0;
	bind[1].length = &meta_id_length;

	bind[2].buffer_type= MYSQL_TYPE_STRING;
	bind[2].length= &length;
	bind[2].is_null= 0;


	/* Bind the buffers */
	if (mysql_stmt_bind_param (db_con->insert_metadata_indexed_stmt, bind)) {
		tracker_log ("binding error : %s\n", mysql_stmt_error (db_con->insert_metadata_indexed_stmt));
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

		if (mysql_stmt_send_long_data (db_con->insert_metadata_indexed_stmt, 2, buffer, strlen (buffer)) != 0) {	

			tracker_log ("error sending data : %s\n", mysql_stmt_error (db_con->insert_metadata_indexed_stmt));
	
			g_free (str_file_id);
			fclose (file);
			return;

		} 
	}

	if (!lock_db ()) {
		g_free (str_file_id);
		fclose (file);
		return;
	}

	if (bytes_read > 3) {
		if (mysql_stmt_execute (db_con->insert_metadata_indexed_stmt) != 0) {
			unlock_db ();
			tracker_log ("query failed :%s", mysql_stmt_error (db_con->insert_metadata_indexed_stmt));
		} else {
			unlock_db ();
			tracker_log ("%d bytes of text successfully inserted into file id %s", bytes_read, str_file_id);
		}
	}

	g_free (str_file_id);	
	fclose (file);


}


FieldDef *
tracker_db_get_field_def (DBConnection *db_con, const char *field_name)
{
	FieldDef *def;
	char ***res_str = NULL;
	char *field = NULL;

	def = g_new (FieldDef, 1);

	/* get metadata typeID and whether its an indexable type of metadata */
	res_str = tracker_db_exec_stmt_result (db_con->select_metadata_type_stmt, 1, field_name);	
	
	// ID, DataTypeID, Indexable, Writeable, Field
	
	if (res_str && res_str[0] && res_str[0][0]) {
		def->id = g_strdup (res_str[0][0]); 
	} else {
		g_free (def);
		tracker_db_free_result (res_str);
		return NULL;
	}			

	if (res_str && res_str[0] && res_str[0][1]) {
		def->type = atoi (res_str[0][1]); 
	}			

	if (res_str && res_str[0] && res_str[0][2]) {
		def->indexable = (strcmp ("1" , res_str[0][2]) == 0); 
	}			

	if (res_str && res_str[0] && res_str[0][3]) {
		def->writeable = (strcmp ("1" , res_str[0][3]) == 0); 
	}			

	tracker_db_free_result (res_str);

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
