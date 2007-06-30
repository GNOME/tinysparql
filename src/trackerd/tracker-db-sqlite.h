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



#ifndef _TRACKER_SQLITE_DB_H_
#define _TRACKER_SQLITE_DB_H_

#include <sqlite3.h>
#include <glib.h>

#include "tracker-utils.h"



typedef enum {
	METADATA_INDEX,
	METADATA_REINDEX,
	METADATA_USER,
	METADATA_USER_REPLACE
} MetadataAction;




typedef struct {
	GMutex		*write_mutex;
	sqlite3		*db;
	DBTypes		db_type;
	char		*err;
	int		rc;
	char		*thread; /* name of the thread that created this */
	GHashTable	*statements;

	gboolean	in_error;

	/* pointers to other database connection objects */
	gpointer	data;
	gpointer	common;
	gpointer	files;
	gpointer	index;
	gpointer	emails;
	gpointer	others;
	gpointer	blob;
	gpointer	cache;
	gpointer	user;

} DBConnection;


char **		tracker_db_get_row		(char ***result, int num);
unsigned int	tracker_db_get_last_id		(DBConnection *db_con);
void		tracker_db_free_result		(char ***result);
void		tracker_db_log_result		(char ***result);
int		tracker_get_row_count		(char ***result);
int		tracker_get_field_count		(char ***result);

gboolean	tracker_db_needs_setup		(void);
gboolean 	tracker_db_needs_data 		(void);
gboolean	tracker_db_initialize		(const char *data_dir);
void		tracker_db_thread_init		(void);
void		tracker_db_thread_end		(void);
void		tracker_db_close		(DBConnection *db_con);
void		tracker_db_finalize		(void);
DBConnection *	tracker_db_connect		(void);
DBConnection *	tracker_db_connect_common	(void);
DBConnection *	tracker_db_connect_full_text	(void);
DBConnection *	tracker_db_connect_cache 	(void);
DBConnection *	tracker_db_connect_emails	(void);
DBConnection *	tracker_db_connect_emails_index (void);
DBConnection *	tracker_db_connect_file_index 	(void);
gboolean	tracker_update_db		(DBConnection *db_con);

char *		tracker_escape_string		(const char *in);


void		tracker_db_prepare_queries	(DBConnection *db_con);
char ***	tracker_exec_proc		(DBConnection *db_con, const char *procedure, int param_count, ...);
char ***	tracker_exec_sql		(DBConnection *db_con, const char *query);
char ***	tracker_exec_sql_ignore_nulls	(DBConnection *db_con, const char *query);
void		tracker_db_exec_no_reply 	(DBConnection *db_con, const char *query);
void		tracker_log_sql			(DBConnection *db_con, const char *query);
void		tracker_create_db		(void);
void		tracker_db_load_stored_procs	(DBConnection *db_con);
void		tracker_db_save_file_contents	(DBConnection *db_con, DBConnection *blob_db_con, GHashTable *index_table, GHashTable *old_table, const char *file_name, FileInfo *info);
void		tracker_db_clear_temp		(DBConnection *db_con);
void		tracker_db_check_tables		(DBConnection *db_con);
void		tracker_db_start_transaction	(DBConnection *db_con);
void		tracker_db_end_transaction	(DBConnection *db_con);

void		tracker_db_update_indexes_for_new_service	(guint32 service_id, int service_type_id, GHashTable *table);
void		tracker_db_update_differential_index		(GHashTable *old_table, GHashTable *new_table, const char *id, int service_type_id);
void		tracker_db_update_index_file_contents 		(DBConnection *blob_db_con, GHashTable *index_table);
int		tracker_db_flush_words_to_qdbm 			(DBConnection *db_con, int limit);

void		tracker_db_release_memory 	();


char *		tracker_get_related_metadata_names 	(DBConnection *db_con, const char *name);
char *		tracker_get_metadata_table 		(DataTypes type);

char ***	tracker_db_search_text		(DBConnection *db_con, const char *service, const char *search_string, int offset, int limit, gboolean save_results, gboolean detailed);
char ***	tracker_db_search_files_by_text	(DBConnection *db_con, const char *text, int offset, int limit, gboolean sort);
char ***	tracker_db_search_metadata	(DBConnection *db_con, const char *service, const char *field, const char *text, int offset, int limit);
char ***	tracker_db_search_matching_metadata (DBConnection *db_con, const char *service, const char *id, const char *text);

/* gets metadata as a single row (with multiple values delimited by semicolons) */
char ***	tracker_db_get_metadata		(DBConnection *db_con, const char *service, const char *id, const char *key);

/* gets metadata using a separate row for each value it has */
char *		tracker_db_get_metadata_delimited (DBConnection *db_con, const char *service, const char *id, const char *key);
char *		tracker_db_set_metadata		  (DBConnection *db_con, const char *service, const char *id, const char *key, char **values, int length);
void		tracker_db_set_single_metadata 	  (DBConnection *db_con, const char *service, const char *id, const char *key, const char *value);

void		tracker_db_insert_embedded_metadata		(DBConnection *db_con, const char *service, const char *id, const char *key, char **values, int length, GHashTable *table);
void		tracker_db_insert_single_embedded_metadata 	(DBConnection *db_con, const char *service, const char *id, const char *key, const char *value, GHashTable *table);


void		tracker_db_delete_metadata_value (DBConnection *db_con, const char *service, const char *id, const char *key, const char *value);
void 		tracker_db_delete_metadata 	 (DBConnection *db_con, const char *service, const char *id, const char *key, gboolean update_indexes); 

char *		tracker_db_refresh_display_metadata 	(DBConnection *db_con, const char *id,  const char *metadata_id, int data_type, const char *key);
void		tracker_db_refresh_all_display_metadata (DBConnection *db_con, const char *id);

void		tracker_db_update_keywords	(DBConnection *db_con, const char *service, const char *id, const char *value);

guint32		tracker_db_create_service 	(DBConnection *db_con, const char *service, FileInfo *info);

void		tracker_db_delete_file		(DBConnection *db_con, DBConnection *blob_db_con, guint32 file_id);
void		tracker_db_delete_directory	(DBConnection *db_con, DBConnection *blob_db_con, guint32 file_id, const char *uri);
void		tracker_db_update_file		(DBConnection *db_con, FileInfo *info);
void		tracker_db_move_file 		(DBConnection *db_con, const char *moved_from_uri, const char *moved_to_uri);
void		tracker_db_move_directory 	(DBConnection *db_con, const char *moved_from_uri, const char *moved_to_uri);

guint32		tracker_db_get_file_id		(DBConnection *db_con, const char *uri);
void		tracker_db_insert_pending_file	(DBConnection *db_con, guint32 file_id, const char *uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory, gboolean is_new, int service_type_id);

gboolean	tracker_db_has_pending_files	(DBConnection *db_con);
gboolean	tracker_db_has_pending_metadata	(DBConnection *db_con);
char ***	tracker_db_get_pending_files	(DBConnection *db_con);
void		tracker_db_remove_pending_files	(DBConnection *db_con);
char ***	tracker_db_get_pending_metadata	(DBConnection *db_con);
void		tracker_db_remove_pending_metadata (DBConnection *db_con);
void		tracker_db_insert_pending	(DBConnection *db_con, const char *id, const char *action, const char *counter, const char *uri, const char *mime, gboolean is_dir, gboolean is_new, int service_type_id);
void		tracker_db_update_pending	(DBConnection *db_con, const char *counter, const char *action, const char *uri);

char ***	tracker_db_get_files_by_service	(DBConnection *db_con, const char *service, int offset, int limit);
char ***	tracker_db_get_files_by_mime	(DBConnection *db_con, char **mimes, int n, int offset, int limit, gboolean vfs);
char ***	tracker_db_search_text_mime	(DBConnection *db_con, const char *text, char **mime_array, int n);
char ***	tracker_db_search_text_location	(DBConnection *db_con, const char *text,const char *location);
char ***	tracker_db_search_text_mime_location (DBConnection *db_con, const char *text, char **mime_array, int n, const char *location);

char ***	tracker_db_get_file_subfolders	(DBConnection *db_con, const char *uri);

char ***	tracker_db_get_metadata_types	(DBConnection *db_con, const char *class, gboolean writeable);

char ***	tracker_db_get_sub_watches	(DBConnection *db_con, const char *dir);
char ***	tracker_db_delete_sub_watches	(DBConnection *db_con, const char *dir);

char ***	tracker_db_get_keyword_list	(DBConnection *db_con, const char *service);

void		tracker_db_update_index_multiple_metadata 	(DBConnection *db_con, const char *service, const char *id, const char *key, char **values);

void		tracker_db_get_static_data 		(DBConnection *db_con);

DBConnection *	tracker_db_get_service_connection 	(DBConnection *db_con, const char *service);

char *		tracker_db_get_service_for_entity 	(DBConnection *db_con, const char *id);

gboolean	tracker_db_metadata_is_child 		(DBConnection *db_con, const char *child, const char *parent);

GHashTable *	tracker_db_get_file_contents_words 	(DBConnection *db_con, guint32 id, GHashTable *old_table);

GHashTable *	tracker_db_get_indexable_content_words 	(DBConnection *db_con, guint32 id, GHashTable *table, gboolean embedded_only);

gboolean	tracker_db_has_display_metadata 	(FieldDef *def);

gboolean	tracker_db_load_service_file 		(DBConnection *db_con, const char *filename, gboolean full_path);

char *		tracker_db_get_field_name 		(const char *service, const char *meta_name);
int		tracker_metadata_is_key 		(const char *service, const char *meta_name);
char *		tracker_db_get_display_field 		(FieldDef *def);

void		tracker_db_delete_service 		(DBConnection *db_con, DBConnection *blob_db_con, guint32 id, const char *uri);

gboolean 	tracker_add_watch_dir 			(const char *dir, DBConnection *db_con);
void     	tracker_remove_watch_dir 		(const char *dir, gboolean delete_subdirs, DBConnection *db_con);
gboolean 	tracker_is_directory_watched 		(const char *dir, DBConnection *db_con);
int		tracker_count_watch_dirs 		(void);

FieldData *	tracker_db_get_metadata_field 		(DBConnection *db_con, const char *service, const char *field_name, int field_count, gboolean is_select, gboolean is_condition);


#endif
