/* Tracker
 * Copyright (C) 2005, Mr Jamie McCracken
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _TRACKER_DB_H_
#define _TRACKER_DB_H_

#include <mysql/mysql.h>
#include <glib.h>
#include "tracker-utils.h"

/* Paramterised queries : */

#define INSERT_CONTENTS		"INSERT INTO ServiceMetaData (ServiceID, MetaDataID, MetaDataIndexValue) VALUES (?,?,?)"



typedef struct {
	MYSQL 		*db;

	/* statement for  parameterised query */	

	MYSQL_STMT  	*insert_contents_stmt;

} DBConnection;
 
typedef enum {
	DATA_INDEX_STRING,
	DATA_STRING,
	DATA_NUMERIC,
	DATA_DATE
} DataTypes;

typedef struct {

	char		*id;
	DataTypes	type;
	gboolean	writeable;
	gboolean	embedded;
	
} FieldDef;


MYSQL * 	tracker_db_connect 		();
gboolean	tracker_update_db 		();
char *		tracker_escape_string 		(MYSQL *db, const char *in);
int		tracker_db_prepare_statement 	(MYSQL *db, MYSQL_STMT **stmt, const char *query); 
void		tracker_db_prepare_queries 	(DBConnection *db_con);
MYSQL_RES *	tracker_exec_proc 		(MYSQL *db, const char *procedure, int param_count, ...);
//char ***	tracker_db_exec_stmt_result 	(MYSQL_STMT *stmt, int param_count,  ...);
//int		tracker_db_exec_stmt 		(MYSQL_STMT *stmt, int param_count,  ...);
void		tracker_db_free_result 		(char ***result);
void		tracker_db_log_result 		(char ***result);
FileInfo *	tracker_db_get_file_info 	(DBConnection *db_con, FileInfo *info);
int		tracker_db_get_file_id	 	(DBConnection *db_con, const char* uri);
gboolean	tracker_is_valid_service 	(DBConnection *db_con, const char *service);
char *		tracker_db_get_id	 	(DBConnection *db_con, const char* service, const char *uri);
MYSQL_RES *	tracker_exec_sql   		(MYSQL *db, const char *query);
void		tracker_log_sql	   		(MYSQL *db, const char *query);
void		tracker_create_db  		();
void		tracker_db_load_stored_procs 	(MYSQL *db);
gboolean	tracker_db_save_file 		(FileInfo *info);
void		tracker_db_save_metadata 	(DBConnection *db_con, GHashTable *table, long file_id);
void		tracker_db_save_thumbs		(DBConnection *db_con, const char *small_thumb, const char *large_thumb, long file_id);
void		tracker_db_save_file_contents	(DBConnection *db_con, const char *file_name, long file_id);
char **		tracker_db_get_files_in_folder 	(DBConnection *db_con, const char *folder_uri);
FieldDef *	tracker_db_get_field_def	(DBConnection *db_con, const char *field_name);
void		tracker_db_free_field_def 	(FieldDef *def);
gboolean	tracker_metadata_is_date 	(DBConnection *db_con, const char* meta);
FileInfo *	tracker_db_get_pending_file 	(DBConnection *db_con, const char *uri);
void		tracker_db_update_pending_file 	(DBConnection *db_con, const char* uri, int counter, TrackerChangeAction action);
void		tracker_db_insert_pending_file 	(DBConnection *db_con, long file_id, const char *uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory);

 
#endif
