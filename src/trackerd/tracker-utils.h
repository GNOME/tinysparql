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


#ifndef _TRACKER_UTILS_H_
#define _TRACKER_UTILS_H_

extern char *type_array[];
extern char *implemented_services[];
extern char *file_service_array[] ;
extern char *serice_index_array[];
extern char *service_table_names[];
extern char *service_metadata_table_names[];
extern char *service_metadata_join_names[];

extern char *tracker_actions[];


#include <glib.h>
#include "config.h"
#include <depot.h>
//include <curia.h>

#include "tracker-parser.h"

typedef struct {
	DEPOT  *word_index;                  /* file hashtable handle for the word -> {serviceID, MetadataID, ServiceTypeID, Score}  */
	GMutex *word_mutex;
} Indexer;



/* max default file pause time in ms  = FILE_PAUSE_PERIOD * FILE_SCHEDULE_PERIOD */
#define FILE_PAUSE_PERIOD		1
#define FILE_SCHEDULE_PERIOD		500
#define TRACKER_DB_VERSION_REQUIRED	4
#define TRACKER_VERSION			"0.5.0"
#define TRACKER_VERSION_INT		500

/* just for now, make tracker_log actually call g_message */
#define tracker_log g_message

typedef struct {
	GSList 		*watch_directory_roots_list;
	GSList 		*no_watch_directory_list;
	GSList 		*poll_list;
	gboolean	use_nfs_safe_locking;

	gboolean	index_emails;
	gboolean	index_evolution_emails;
	gboolean	index_thunderbird_emails;
	GSList		*additional_mboxes_to_index;
	gboolean	do_thumbnails;

 	GHashTable  	*file_scheduler;
 	GMutex 		*scheduler_mutex;

 	gboolean 	is_running;
	GMainLoop 	*loop;
	
	Indexer		*file_indexer;
	Indexer		*email_indexer;
	TextParser	*parser;

	GMutex 		*log_access_mutex;
	char	 	*log_file;

	GAsyncQueue 	*file_process_queue;
	GAsyncQueue 	*file_metadata_queue;
	GAsyncQueue 	*user_request_queue;

	GMutex		*files_check_mutex;
	GMutex		*metadata_check_mutex;
	GMutex		*request_check_mutex;

	GMutex		*poll_access_mutex;

	GMutex		*files_stopped_mutex;
	GMutex		*metadata_stopped_mutex;
	GMutex		*request_stopped_mutex;
	GMutex		*poll_stopped_mutex;

	GThread 	*file_metadata_thread;
	GThread 	*file_process_thread;
	GThread 	*user_request_thread;
	GThread 	*file_poll_thread;

	GCond 		*file_thread_signal;
	GCond 		*metadata_thread_signal;
	GCond 		*request_thread_signal;
	GCond 		*poll_thread_signal;

	GMutex		*metadata_signal_mutex;
	GMutex		*files_signal_mutex;
	GMutex		*request_signal_mutex;
	GMutex		*poll_signal_mutex;

} Tracker;


/* Actions can represent events from FAM/iNotify or be artificially created */
typedef enum {

	TRACKER_ACTION_IGNORE,  		/* do nothing for this action */

	/* actions for when we dont know whether they are on a file or a directory */
	TRACKER_ACTION_CHECK,
	TRACKER_ACTION_DELETE,
	TRACKER_ACTION_DELETE_SELF,		/* actual file/dir being watched was deleted */
	TRACKER_ACTION_CREATE,
	TRACKER_ACTION_MOVED_FROM,		/* file or dir was moved from (must be a pair with MOVED_TO action)*/
	TRACKER_ACTION_MOVED_TO,		/* file or dir was moved to */
//6
	/* file specific actions */
	TRACKER_ACTION_FILE_CHECK, 		/* checks file is up to date and indexed */
	TRACKER_ACTION_FILE_CHANGED, 		/* Inotify must ignore this - see below */
	TRACKER_ACTION_FILE_DELETED,
	TRACKER_ACTION_FILE_CREATED,
	TRACKER_ACTION_FILE_MOVED_FROM,
	TRACKER_ACTION_FILE_MOVED_TO,
	TRACKER_ACTION_WRITABLE_FILE_CLOSED, 	/* inotify should use this instead of File Changed action*/
//13
	/* directory specific actions */
	TRACKER_ACTION_DIRECTORY_CHECK,
	TRACKER_ACTION_DIRECTORY_CREATED,
	TRACKER_ACTION_DIRECTORY_DELETED,
	TRACKER_ACTION_DIRECTORY_MOVED_FROM,
	TRACKER_ACTION_DIRECTORY_MOVED_TO,
	TRACKER_ACTION_DIRECTORY_REFRESH, 	/* re checks all files in folder */

	TRACKER_ACTION_EXTRACT_METADATA,

	TRACKER_ACTION_FORCE_REFRESH

} TrackerChangeAction;


/* service type that the file represents */
typedef enum {
	FILE_ORDINARY,
	FILE_DESKTOP,
	FILE_BOOKMARKS,
	FILE_SMARTBOOKMARKS,
	FILE_WEBHISTORY,
	FILE_EMAILS,
	FILE_CONVERSATIONS,
	FILE_CONTACTS
} FileTypes;


typedef enum {
	WATCH_ROOT,
	WATCH_SUBFOLDER,
	WATCH_SPECIAL_FOLDER,
	WATCH_SPECIAL_FILE,
	WATCH_NO_INDEX,
	WATCH_OTHER
} WatchTypes;


typedef struct {

	/* file name/path related info */
	char 			*uri;
	gint32			file_id;

	gboolean		is_new;
	TrackerChangeAction  	action;
	guint32        		cookie;
	int  		     	counter;
	FileTypes		file_type;
	WatchTypes		watch_type;
	gboolean		is_directory;

	/* symlink info - File ID of link might not be in DB so need to store path/filename too */
	gboolean		is_link;
	gint32			link_id;
	char			*link_path;
	char			*link_name;

	char			*mime;
	int			service_type_id;
	guint32			file_size;
	char			*permissions;
	guint32			mtime;
	guint32			atime;
	guint32			indextime;

	/* options */
	char			*move_from_uri;
	gboolean		extract_embedded;
	gboolean		extract_contents;
	gboolean		extract_thumbs;

	/* we ref count FileInfo as it has a complex lifespan and is tossed between various threads, lists, queues and hash tables */
	int			ref_count;

} FileInfo;

int		tracker_get_id_for_service 		(const char *service);
const char *	tracker_get_service_by_id 		(int service_type_id);


char **		tracker_make_array_null_terminated (char **array, int length);

void		tracker_free_array 		(char **array, int row_count);
char *		tracker_int_to_str		(int i);
char *		tracker_long_to_str		(long i);
char *		tracker_format_date 		(const char *time_string);
time_t		tracker_str_to_date 		(const char *time_string);
char *		tracker_date_to_str 		(long date_time);
int		tracker_str_in_array 		(const char *str, char **array);

char *		tracker_format_search_terms 	(const char *str, gboolean *do_bool_search);


FileInfo *	tracker_create_file_info 	(const char *uri, TrackerChangeAction action, int counter, WatchTypes watch);
FileInfo * 	tracker_get_file_info  	 	(FileInfo *info);
FileInfo * 	tracker_copy_file_info   	(FileInfo *info);
FileInfo *	tracker_inc_info_ref 		(FileInfo *info);
FileInfo *	tracker_dec_info_ref 		(FileInfo *info);

FileInfo *	tracker_free_file_info   	(FileInfo *info);

gboolean	tracker_file_info_is_valid 	(FileInfo *info);

char *		tracker_get_vfs_path 		(const char *uri);

char *		tracker_get_vfs_name 		(const char *uri);

char * 		tracker_get_mime_type 	 	(const char *uri);

GSList * 	tracker_get_files 		(const char *dir, gboolean dir_only);

gboolean 	tracker_file_is_valid 		(const char *uri);

gboolean 	tracker_is_directory 		(const char *dir);

void 		tracker_get_dirs 		(const char *dir, GSList **file_list) ;

void		tracker_load_config_file 	(void);

GSList * 	tracker_get_watch_root_dirs 	(void);

gboolean	tracker_ignore_file 		(const char *uri);

void		tracker_print_object_allocations (void);

void		tracker_add_poll_dir 		(const char *dir);
void		tracker_remove_poll_dir 	(const char *dir);
gboolean	tracker_is_dir_polled 		(const char *dir);

void		tracker_notify_file_data_available 	(void);
void		tracker_notify_meta_data_available 	(void);
void		tracker_notify_request_data_available 	(void);

GTimeVal *	tracker_timer_start 		();
void		tracker_timer_end 		(GTimeVal *before, const char *str);

char *		tracker_compress 		(const char *ptr, int size, int *compressed_size);
char *		tracker_uncompress 		(const char *ptr, int size, int *uncompressed_size);


#endif
