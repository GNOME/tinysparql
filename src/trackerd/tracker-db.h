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

#include <mysql/mysql.h>
#include <glib.h>
#include "tracker-utils.h"

/* Paramterised queries : */

/* queries for Files */
#define SELECT_FILE_ID 		"SELECT ID, IndexTime, IsDirectory FROM Files WHERE Path = ? AND FileName = ?"
#define SELECT_FILE_CHILD	"SELECT ID, Path, FileName FROM Files WHERE Path = ?"
#define SELECT_FILE_SUB_FOLDERS	"SELECT ID, Path, FileName FROM Files WHERE (Path = ? or Path like ?) AND IsDirectory = 1"
#define SELECT_FILE_WATCHES	"SELECT Path, FileName FROM Files WHERE WatchType < 4 and IsWatched = 1"	
#define SELECT_FILE_WATCH_ROOTS	"SELECT CONCAT (Path, '/',  FileName) FROM Files WHERE WatchType = 0 and IsWatched = 1"	

#define INSERT_FILE 		"INSERT INTO Files (Path, FileName, IsVFS, IsDirectory, IsLink, IndexTime) VALUES (?,?,?,?,?,?)" 

#define UPDATE_FILE		"UPDATE Files SET IndexTime = ? WHERE ID = ?"
#define UPDATE_FILE_MOVE	"UPDATE Files SET Path = ?, FileName = ?, IndexTime = ? WHERE ID = ?"
#define UPDATE_FILE_MOVE_CHILD	"UPDATE Files SET Path = ? WHERE Path = ?"
#define UPDATE_FILE_MOVE_CHILD2	"UPDATE Files SET Path = REPLACE(path,?,?) WHERE Path like ?"

#define UPDATE_FILE_WATCH	"UPDATE Files SET IsWatched = ?, WatchType = ? WHERE ID = ?"
#define UPDATE_FILE_WATCH_CHILD	"UPDATE Files SET IsWatched = ?, WatchType = ? WHERE (Path = ?) OR (Path like ?)"

#define DELETE_FILE		"DELETE FROM Files WHERE ID = ?"
#define DELETE_FILE_CHILD	"DELETE FROM Files WHERE (Path = ?) OR (Path like ?)"

/* queries for MetaData */
#define SELECT_METADATA		"SELECT MetaDataValue FROM FileMetaData WHERE FileID = ? AND MetaDataID = ? "
#define SELECT_METADATA_INDEXED	"SELECT MetaDataIndexValue FROM FileMetaData WHERE FileID = ? AND MetaDataID = ? "
#define SELECT_METADATA_INTEGER	"SELECT MetaDataIntegerValue FROM FileMetaData WHERE FileID = ? AND MetaDataID = ? "

#define INSERT_METADATA 	"INSERT INTO FileMetaData (FileID, MetaDataID, MetaDataValue) VALUES (?,?,?)"
#define INSERT_METADATA_INDEXED	"INSERT INTO FileMetaData (FileID, MetaDataID, MetaDataIndexValue) VALUES (?,?,?)"
#define INSERT_METADATA_INTEGER	"INSERT INTO FileMetaData (FileID, MetaDataID, MetaDataIntegerValue) VALUES (?,?,?)"
 
#define DELETE_METADATA_DERIVED		"DELETE FROM FileMetaData WHERE FileID = ? AND MetaDataID IN (SELECT ID FROM MetaDataTypes WHERE Writeable = 0)" 
#define DELETE_METADATA_ALL		"DELETE FROM FileMetaData WHERE FileID = ?"
#define DELETE_METADATA_CHILD_ALL	"DELETE M FROM FileMetaData M, Files F WHERE M.FileID = F.ID AND F.Path = ? OR F.Path like ?"

/* queries for MetaData Types */
#define SELECT_METADATA_TYPE	 	"SELECT ID, DataTypeID, Indexable, Writeable FROM MetaDataTypes WHERE MetaName = ?"
#define SELECT_METADATA_TYPE_LIKE 	"SELECT MetaName, ID, DataTypeID, Indexable, Writeable  FROM MetaDataTypes WHERE MetaName like ?"
#define SELECT_METADATA_TYPE_FILE	"SELECT DISTINCT T.MetaName, T.DataTypeID, T.Indexable, T.Writeable FROM MetaDataTypes T, FileMetaData M WHERE T.ID  = M.MetaDataID and M.FileID = ?"

#define INSERT_METADATA_TYPE	 	"INSERT INTO MetaDataTypes (MetaName, DataTypeID, Indexable, Writeable) VALUES (?,?,?,?)" 

/* queries for searching files */
#define SELECT_SEARCH_TEXT		 "SELECT  F.Path, F.FileName FROM Files F, FileMetaData M WHERE F.ID = M.FileID AND F.IsVFS = 0 AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE) LIMIT 512"
#define SELECT_SEARCH_TEXT_MIME		 "SELECT  F.Path, F.FileName FROM Files F, FileMetaData M, FileMetaData M2 WHERE F.ID = M.FileID AND F.ID = M2.FileID AND F.IsVFS = 0 AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE) AND M2.MetaDataID = (SELECT ID FROM MetaDataTypes WHERE MetaName = 'File.Format') AND FIND_IN_SET(M2.MetaDataIndexValue, ?) LIMIT 512"
#define SELECT_SEARCH_TEXT_LOCATION	 "SELECT  F.Path, F.FileName FROM Files F, FileMetaData M WHERE F.ID = M.FileID AND F.IsVFS = 0 AND F.Path like ? AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE) LIMIT 512"
#define SELECT_SEARCH_TEXT_MIME_LOCATION "SELECT  F.Path, F.FileName FROM Files F, FileMetaData M, FileMetaData M2 WHERE F.ID = M.FileID AND F.ID = M2.FileID AND F.IsVFS = 0 AND F.Path like ? AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE) AND M2.MetaDataID = (SELECT ID FROM MetaDataTypes WHERE MetaName = 'File.Format') AND FIND_IN_SET(M2.MetaDataIndexValue, ?) LIMIT 512"


/* queries for processing pending files */

#define INSERT_PENDING_FILE		"INSERT INTO FilePending (FileID, Action, Counter, FileUri, MimeType, IsDir) VALUES (?,?,?,?,?,?)"

#define CREATE_PENDING_FILES		"CREATE TEMPORARY TABLE tmptable SELECT ID, FileID, FileUri, Action, MimeType, IsDir FROM FilePending WHERE Counter = 0 AND Action <> 20 ORDER BY ID LIMIT 400"
#define CREATE_PENDING_FILES_METADATA	"CREATE TEMPORARY TABLE tmptable SELECT ID, FileID, FileUri, Action, MimeType, IsDir FROM FilePending WHERE Counter = 0 AND Action = 20 ORDER BY ID LIMIT 400"
#define SELECT_PENDING_FILES		"SELECT FileID, FileUri, Action, MimeType, IsDir FROM tmptable ORDER BY ID"
#define DELETE_PENDING_FILES		"DELETE FROM FilePending USING tmptable, FilePending WHERE FilePending.ID = tmptable.ID"
#define DROP_PENDING_FILES		"DROP TEMPORARY TABLE tmptable"

#define SELECT_PENDING_FILE_BY_URI	"SELECT  FileID, FileUri, Action, MimeType, IsDir FROM FilePending WHERE FileUri = ?"

#define UPDATE_PENDING_FILES_COUNTER	"UPDATE FilePending SET Counter=Counter-1 WHERE Counter > 0"
#define UPDATE_PENDING_FILE		"UPDATE FilePending SET Counter = ?, Action = ? WHERE FileUri = ?"

#define DELETE_PENDING_FILE_BY_URI	"DELETE FROM FilePending WHERE FileUri = ?"


/* queries for inotify watches */

#define  CREATE_WATCH_TABLE 	"CREATE Table IF NOT EXISTS TMP_WATCH (URI varchar(255) not null, WatchID int not null, primary key (URI), key (WatchID))"
#define  SELECT_URI   		"select URI from TMP_WATCH where WatchID = ?"
#define  SELECT_WATCH  		"select WatchID from TMP_WATCH where URI = ?"
#define  SELECT_SUBWATCHES	"select WatchID from TMP_WATCH where URI like ?"
#define  SELECT_WATCH_COUNT	"select COUNT(URI) from TMP_WATCH where (WatchID > -1)"
#define  DELETE_WATCH  		"delete from TMP_WATCH where URI = ?"
#define  DELETE_SUBWATCHES	"delete from TMP_WATCH where URI like ?"
#define  INSERT_WATCH  		"insert into TMP_WATCH (URI, WatchID) values (?,?)"


typedef struct {
	MYSQL 		*db;

	/* statements for above parameterised queries */	
        MYSQL_STMT  	*select_file_id_stmt;
        MYSQL_STMT  	*select_file_index_time_stmt;
        MYSQL_STMT  	*select_file_child_stmt;
        MYSQL_STMT  	*select_file_sub_folders_stmt;
        MYSQL_STMT  	*select_file_watches_stmt;

        MYSQL_STMT  	*insert_file_stmt;

        MYSQL_STMT  	*update_file_stmt;
        MYSQL_STMT  	*update_file_move_stmt;
        MYSQL_STMT  	*update_file_move_child_stmt;
        MYSQL_STMT  	*update_file_move_child2_stmt;
        MYSQL_STMT  	*update_file_watch_stmt;
        MYSQL_STMT  	*update_file_watch_child_stmt;

        MYSQL_STMT  	*delete_file_stmt;
        MYSQL_STMT  	*delete_file_child_stmt;

	MYSQL_STMT  	*select_uri_stmt;
	MYSQL_STMT  	*select_watch_stmt;
	MYSQL_STMT  	*select_subwatches_stmt;
	MYSQL_STMT  	*delete_watch_stmt;
	MYSQL_STMT  	*delete_subwatches_stmt;
	MYSQL_STMT  	*insert_watch_stmt;

        MYSQL_STMT  	*select_metadata_stmt;
        MYSQL_STMT  	*select_metadata_indexed_stmt;
        MYSQL_STMT  	*select_metadata_integer_stmt;
  
        MYSQL_STMT  	*insert_metadata_stmt;
	MYSQL_STMT  	*insert_metadata_indexed_stmt;
	MYSQL_STMT  	*insert_metadata_integer_stmt;

        MYSQL_STMT  	*delete_metadata_derived_stmt;
        MYSQL_STMT  	*delete_metadata_all_stmt;
        MYSQL_STMT  	*delete_metadata_child_all_stmt;

        MYSQL_STMT  	*select_metadata_type_stmt;
        MYSQL_STMT  	*select_metadata_type_like_stmt;
        MYSQL_STMT  	*select_metadata_type_file_stmt;

        MYSQL_STMT  	*insert_metadata_type_stmt;

        MYSQL_STMT  	*select_search_text_stmt;
        MYSQL_STMT  	*select_search_text_mime_stmt;
        MYSQL_STMT  	*select_search_text_mime_location_stmt;
        MYSQL_STMT  	*select_search_text_location_stmt;

        MYSQL_STMT  	*select_pending_file_by_uri_stmt;
        MYSQL_STMT  	*insert_pending_file_stmt;
        MYSQL_STMT  	*update_pending_file_stmt;
	MYSQL_STMT  	*update_pending_files_counter_stmt;
        MYSQL_STMT  	*delete_pending_file_by_uri_stmt;

} DBConnection;
 
typedef enum {
	DATA_STRING,
	DATA_INTEGER,
	DATA_DATE
} DataTypes;

typedef struct {

	char		*id;
	DataTypes	type;
	gboolean	writeable;
	gboolean	indexable;
	
} FieldDef;


MYSQL * 	tracker_db_connect 		();
int		tracker_db_prepare_statement 	(MYSQL *db, MYSQL_STMT **stmt, const char *query); 
void		tracker_db_prepare_queries 	(DBConnection *db_con);
char ***	tracker_db_exec_stmt_result 	(MYSQL_STMT *stmt, int param_count,  ...);
int		tracker_db_exec_stmt 		(MYSQL_STMT *stmt, int param_count,  ...);
void		tracker_db_free_result 		(char ***result);
void		tracker_db_log_result 		(char ***result);
FileInfo *	tracker_db_get_file_info 	(DBConnection *db_con, FileInfo *info);
int		tracker_db_get_file_id	 	(DBConnection *db_con, const char* uri);
MYSQL_RES *	tracker_exec_sql   		(MYSQL *db, const char *query);
void		tracker_log_sql	   		(MYSQL *db, const char *query);
void		tracker_create_db  		();
gboolean	tracker_db_save_file 		(FileInfo *info);
void		tracker_db_save_metadata 	(DBConnection *db_con, GHashTable *table, long file_id);
void		tracker_db_save_thumbs		(DBConnection *db_con, const char *small_thumb, const char *large_thumb, long file_id);
void		tracker_db_save_file_contents	(DBConnection *db_con, const char *file_name, long file_id);
char **		tracker_db_get_files_in_folder 	(DBConnection *db_con, const char *folder_uri);
FieldDef *	tracker_db_get_field_def	(DBConnection *db_con, const char *field_name);
void		tracker_db_free_field_def 	(FieldDef *def);
FileInfo *	tracker_db_get_pending_file 	(DBConnection *db_con, const char *uri);
void		tracker_db_update_pending_file 	(DBConnection *db_con, const char* uri, int counter, TrackerChangeAction action);
void		tracker_db_insert_pending_file 	(DBConnection *db_con, long file_id, const char *uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory);

 
