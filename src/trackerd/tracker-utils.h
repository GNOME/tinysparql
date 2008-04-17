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

#include <time.h>
#include <glib.h>

#include "config.h"
#include "tracker-parser.h"

#define MAX_HITS_FOR_WORD 30000

/* set merge limit default to 64MB */
#define MERGE_LIMIT 671088649

/* max default file pause time in ms  = FILE_PAUSE_PERIOD * FILE_SCHEDULE_PERIOD */
#define FILE_PAUSE_PERIOD		1
#define FILE_SCHEDULE_PERIOD		300

#define TRACKER_DB_VERSION_REQUIRED	13
#define TRACKER_VERSION			VERSION
#define TRACKER_VERSION_INT		604

/* default performance options */
#define MAX_INDEX_TEXT_LENGTH		1048576
#define MAX_PROCESS_QUEUE_SIZE		100
#define MAX_EXTRACT_QUEUE_SIZE		500
#define OPTIMIZATION_COUNT		10000
#define MAX_WORDS_TO_INDEX		10000

typedef struct {                         
	int	 	id;              /* word ID of the cached word */
	int 		count;     	 /* cummulative count of the cached word */
} CacheWord;

typedef enum {
	DATA_KEYWORD,	
	DATA_INDEX,
	DATA_FULLTEXT,
	DATA_STRING,
	DATA_INTEGER,
	DATA_DOUBLE,
	DATA_DATE,
	DATA_BLOB,
	DATA_STRUCT,
	DATA_LINK
} DataTypes;


typedef enum {
	DB_CATEGORY_FILES, 
	DB_CATEGORY_EMAILS,
	DB_CATEGORY_USER
} DBCategory;


typedef enum {
	STATUS_INIT,		/* tracker is initializing */
	STATUS_WATCHING,	/* tracker is setting up watches for directories */
	STATUS_INDEXING,	/* tracker is indexing stuff */
	STATUS_PENDING, 	/* tracker has entities awaiting index */
	STATUS_OPTIMIZING,	/* tracker is optimizing its databases */
	STATUS_IDLE,		/* tracker is idle and awaiting new events or requests */
	STATUS_SHUTDOWN		/* tracker is in the shutdown process */
} TrackerStatus;


typedef enum {
	INDEX_CONFIG,
	INDEX_APPLICATIONS,
	INDEX_FILES,
	INDEX_WEBHISTORY,
	INDEX_CRAWL_FILES,
	INDEX_CONVERSATIONS,	
	INDEX_EXTERNAL,	
	INDEX_EMAILS,
	INDEX_FINISHED
} IndexStatus;


typedef struct {
	char		*id;
	DataTypes	type;
	char 		*field_name;
	int		weight;
	guint           embedded : 1;
	guint           multiple_values : 1;
	guint           delimited : 1;
	guint           filtered : 1;
	guint           store_metadata : 1;

	GSList		*child_ids; /* related child metadata ids */

} FieldDef;


typedef struct {
	char 		*alias;
	char 	 	*field_name;
	char	 	*select_field;
	char	 	*where_field;
	char	 	*table_name;
	char	 	*id_field;
	DataTypes	data_type;
	guint           multiple_values : 1;
	guint           is_select : 1;
	guint           is_condition : 1;
	guint           needs_join : 1;

} FieldData;


typedef struct {
	char 		*name;
	char		*type;
} ServiceInfo;


typedef enum {
	EVENT_NOTHING,
	EVENT_SHUTDOWN,
	EVENT_DISABLE,
	EVENT_PAUSE,
	EVENT_CACHE_FLUSHED
} LoopEvent;


typedef struct {
	gchar	*uri;
	time_t	first_change_time;
	gint    num_of_change;
} FileChange;


typedef struct {

	gboolean	readonly;

	TrackerStatus	status;
	int		pid;

	gpointer	dbus_con;
	gpointer	hal;

	gboolean	reindex;

        gpointer        config;
        gpointer        language;

	/* config options */
	guint32		watch_limit;

	gboolean	fatal_errors;

	gpointer	index_db;

	/* data directories */
	char 		*data_dir;
	char		*config_dir;
	char 		*root_dir;
	char		*user_data_dir;
	char		*sys_tmp_root_dir;
        char            *email_attachements_dir;
	char 		*services_dir;

	/* performance and memory usage options */
	int		max_index_text_length; /* max size of file's text contents to index */
	int		max_process_queue_size;
	int		max_extract_queue_size;
	int 		memory_limit;
	int 		thread_stack_size;

	/* HAL battery */
	char		*battery_udi;

	/* pause/shutdown vars */
	gboolean	shutdown;
	gboolean	pause_manual;
	gboolean	pause_battery;
	gboolean	pause_io;

	/* indexing options */
	gpointer	file_index;
	gpointer	file_update_index;
	gpointer	email_index;

	guint32		merge_limit; 		/* size of index in MBs when merging is triggered -1 == no merging*/
	gboolean	active_file_merge;
	gboolean	active_email_merge;

	GHashTable	*stop_words;	  	/* table of stop words that are to be ignored by the parser */

	gboolean	index_numbers;
	int		index_number_min_length;
	gboolean	strip_accents;

	gboolean	first_time_index;
	gboolean	first_flush;
	gboolean	do_optimize;
	
	time_t		index_time_start;
	int		folders_count;
	int		folders_processed;
	int		mbox_count;
	int		mbox_processed;


	const char	*current_uri;
	
	IndexStatus	index_status;

	int		grace_period;
	gboolean	request_waiting;

	char *		xesam_dir;

	/* service directory table - this is used to store a ServiceInfo struct for a directory path - used for determining which service a uri belongs to for things like files, emails, conversations etc*/
	GHashTable	*service_directory_table;
	GSList		*service_directory_list;

	/* lookup tables for service and metadata IDs */
	GHashTable	*metadata_table;

	/* email config options */
	GSList		*additional_mboxes_to_index;

	int		email_service_min;
	int		email_service_max;

	/* nfs options */
	gboolean	use_nfs_safe_locking; /* use safer but much slower external lock file when users home dir is on an nfs systems */

	/* Queue for recorad file changes */
	GQueue		*file_change_queue;
	gboolean	black_list_timer_active;
	
	/* progress info for merges */
	int		merge_count;
	int		merge_processed;
	

	/* application run time values */
	gboolean	is_indexing;
	gboolean	in_flush;
	gboolean	in_merge;
	int		index_count;
	int		index_counter;
	int		update_count;

	/* cache words before saving to word index */
	GHashTable	*file_word_table;
	GHashTable	*file_update_word_table;
	GHashTable	*email_word_table;

	int		word_detail_limit;
	int		word_detail_count;
	int		word_detail_min;
	int		word_count;
	int		word_update_count;
	int		word_count_limit;
	int		word_count_min;
	int		flush_count;

	int		file_update_count;
	int		email_update_count;

 	GMutex 		*scheduler_mutex;

 	gboolean 	is_running;
	gboolean	is_dir_scan;
	GMainLoop 	*loop;

	GMutex 		*log_access_mutex;
	char	 	*log_file;

	GAsyncQueue 	*file_process_queue;
	GAsyncQueue 	*file_metadata_queue;
	GAsyncQueue 	*user_request_queue;

	GAsyncQueue 	*dir_queue;

	GMutex		*files_check_mutex;
	GMutex		*metadata_check_mutex;
	GMutex		*request_check_mutex;

	GMutex		*files_stopped_mutex;
	GMutex		*metadata_stopped_mutex;
	GMutex		*request_stopped_mutex;

	GThread 	*file_metadata_thread;

	GCond 		*file_thread_signal;
	GCond 		*metadata_thread_signal;
	GCond 		*request_thread_signal;

	GMutex		*metadata_signal_mutex;
	GMutex		*files_signal_mutex;
	GMutex		*request_signal_mutex;

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
	TRACKER_ACTION_DIRECTORY_UNMOUNTED,
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
	guint32			file_id;

	TrackerChangeAction  	action;
	guint32        		cookie;
	int  		     	counter;
	FileTypes		file_type;
	WatchTypes		watch_type;

	/* symlink info - File ID of link might not be in DB so need to store path/filename too */
	gint32                  link_id;
	char                    *link_path;
	char                    *link_name;

	char                    *mime;
	int                     service_type_id;
	guint32                 file_size;
	char                    *permissions;
	gint32                  mtime;
	gint32                  atime;
	gint32                  indextime;
	gint32                  offset;

	/* options */
	char			*moved_to_uri;

	int			aux_id;

	guint                   is_new : 1;
	guint                   is_directory : 1;
	guint                   is_link : 1;
	guint                   extract_embedded : 1;
	guint                   extract_contents : 1;
	guint                   extract_thumbs : 1;
	guint                   is_hidden : 1;

	/* we ref count FileInfo as it has a complex lifespan and is tossed between various threads, lists, queues and hash tables */
	int			ref_count;

} FileInfo;

GSList *	tracker_filename_array_to_list		(gchar **array);
GSList *	tracker_string_list_to_gslist   	(const gchar **array);
gchar **	tracker_gslist_to_string_list 		(GSList *list);
gchar **	tracker_make_array_null_terminated 	(gchar **array, gint length);

void		tracker_free_array 		(char **array, int row_count);
gboolean        tracker_is_empty_string         (const char *s);
gchar *         tracker_long_to_str             (glong i);
gchar *		tracker_int_to_str		(gint i);
gchar *		tracker_uint_to_str		(guint i);
gchar *		tracker_gint32_to_str		(gint32 i);
gchar *		tracker_guint32_to_str		(guint32 i);
gboolean	tracker_str_to_uint		(const char *s, guint *ret);
char *		tracker_format_date 		(const char *time_string);
time_t		tracker_str_to_date 		(const char *time_string);
char *		tracker_date_to_str 		(time_t date_time);
int		tracker_str_in_array 		(const char *str, char **array);

char *		tracker_get_radix_by_suffix	(const char *str, const char *suffix);

char *		tracker_escape_metadata 	(const char *in);
char *		tracker_unescape_metadata 	(const char *in);

void		tracker_remove_dirs 		(const char *root_dir);
char *		tracker_format_search_terms 	(const char *str, gboolean *do_bool_search);

gint32		tracker_get_file_mtime 		(const char *uri);

char *		tracker_array_to_str 		(char **array, int length, char sep);

GSList *	tracker_get_service_dirs 	(const char *service);
void		tracker_add_service_path 	(const char *service, const char *path);
void		tracker_del_service_path 	(const char *service,  const char *path);

char *		tracker_get_service_for_uri 	(const char *uri);
gboolean	tracker_is_service_file 	(const char *uri);

FileInfo *	tracker_create_file_info 	(const char *uri, TrackerChangeAction action, int counter, WatchTypes watch);
FileInfo * 	tracker_get_file_info  	 	(FileInfo *info);
FileInfo * 	tracker_copy_file_info   	(FileInfo *info);
FileInfo *	tracker_inc_info_ref 		(FileInfo *info);
FileInfo *	tracker_dec_info_ref 		(FileInfo *info);

FileInfo *	tracker_free_file_info   	(FileInfo *info);

char *		tracker_get_vfs_path 		(const char *uri);

char *		tracker_get_vfs_name 		(const char *uri);
char * 		tracker_get_mime_type 	 	(const char *uri);

gboolean 	tracker_file_is_valid 		(const char *uri);

gboolean	tracker_file_is_indexable 	(const char *uri);

gboolean	tracker_is_mounted	        (const char *dir);
gboolean 	tracker_is_directory 		(const char *dir);

void		tracker_load_config_file 	(void);

GSList * 	tracker_get_watch_root_dirs 	(void);

void		tracker_print_object_allocations (void);

void		tracker_throttle 		(int multiplier);

void		tracker_notify_file_data_available 	(void);
void		tracker_notify_meta_data_available 	(void);
void		tracker_notify_request_data_available 	(void);

GTimeVal *	tracker_timer_start 		(void);
void		tracker_timer_end 		(GTimeVal *before, const char *str);

char *		tracker_compress 		(const char *ptr, int size, int *compressed_size);
char *		tracker_uncompress 		(const char *ptr, int size, int *uncompressed_size);

char *		tracker_get_snippet 		(const char *txt, char **terms, int length);

gboolean	tracker_spawn 			(char **argv, int timeout, char **tmp_stdout, int *exit_status);

char*	 	tracker_string_replace 		(const char *haystack, char *needle, char *replacement);

void		tracker_add_metadata_to_table 	(GHashTable *meta_table, const char *key, const char *value);

void		tracker_free_metadata_field 	(FieldData *field_data);

gboolean	tracker_unlink 			(const char *uri);

int 		tracker_get_memory_usage 	(void);

guint32		tracker_file_size 		(const char *name);
int		tracker_file_open 		(const char *file_name, gboolean readahead);
void		tracker_file_close 		(int fd, gboolean no_longer_needed);

void		tracker_add_io_grace 		(const char *uri);

char *		tracker_get_status 		(void);

void		free_file_change		(FileChange **user_data);
gboolean	tracker_do_cleanup 		(const gchar *sig_msg);
gboolean        tracker_watch_dir               (const gchar *uri);
void            tracker_scan_directory          (const gchar *uri);

gboolean	tracker_pause_on_battery 	(void);
gboolean	tracker_low_diskspace		(void);
gboolean	tracker_pause			(void);

void		tracker_set_status		(Tracker *tracker, TrackerStatus status, gdouble percentage, gboolean signal);

#endif
