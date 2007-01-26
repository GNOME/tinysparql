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


#include <glib.h>
#include <magic.h>
#include "depot.h"
#include "curia.h"

#include "config.h"
#include "tracker-parser.h"
#include "../libstemmer/include/libstemmer.h"

typedef struct {
	CURIA  *word_index;	/* file hashtable handle for the word -> {serviceID, MetadataID, ServiceTypeID, Score}  */
	GMutex *word_mutex;
} Indexer;


/* max default file pause time in ms  = FILE_PAUSE_PERIOD * FILE_SCHEDULE_PERIOD */
#define FILE_PAUSE_PERIOD		1
#define FILE_SCHEDULE_PERIOD		300

#define TRACKER_DB_VERSION_REQUIRED	13
#define TRACKER_VERSION			"0.5.4"
#define TRACKER_VERSION_INT		504

/* default performance options */
#define MAX_INDEX_TEXT_LENGTH		1048576
#define MAX_PROCESS_QUEUE_SIZE		100
#define MAX_EXTRACT_QUEUE_SIZE		500
#define	OPTIMIZATION_COUNT		10000

/* default indexer options */
#define MIN_INDEX_BUCKET_COUNT		65536    /* minimum bucket number of word index per division (total buckets = INDEXBNUM * INDEXDIV) */
#define INDEX_DIVISIONS		        4        /* no. of divisions of file */
#define MAX_INDEX_BUCKET_COUNT 		524288	 /* max no of buckets to use  */
#define INDEX_BUCKET_RATIO 		1	 /* desired ratio of unused buckets to have (range 0 to 4)*/
#define INDEX_PADDING	 		2

typedef struct {                         /* type of structure for an element of search result */
	guint32 	id;              /* Service ID number of the document */
	int 		amalgamated;     /* amalgamation of service_type and score of the word in the document's metadata */
} WordDetails;


typedef struct {                         
	int	 	id;              /* word ID of the cached word */
	int 		count;     	 /* cummulative count of the cached word */
} CacheWord;

typedef struct {

	/* config options */
	GSList 		*watch_directory_roots_list;
	GSList 		*no_watch_directory_list;
	GSList		*no_index_file_types;

	gboolean	enable_indexing;
	gboolean	enable_watching; /* can disable all forms of directory watching */
	gboolean	enable_content_indexing; /* enables indexing of a file's text contents */
	gboolean	enable_thumbnails;

	guint32		watch_limit;
	guint32		poll_interval;

	/* controls how much to output to screen/log file */
	int		verbosity;

	
	magic_t 	magic;

	/* data directories */
	char 		*data_dir;
	char		*backup_dir;
	char		*sys_tmp_root_dir;

	/* performance and memory usage options */
	int		max_index_text_length; /* max size of file's text contents to index */
	int		max_process_queue_size;
	int		max_extract_queue_size;
	int		optimization_count;   /* no of updates or inserts to be performed before optimizing hashtable */
	int		throttle;
	int		default_throttle;
	int		battery_throttle;
	gboolean	use_extra_memory;

	/* indexing options */
	int	 	max_index_bucket_count;
	int	 	index_bucket_ratio; /* 0 = 50%, 1 = 100%, 2 = 200%, 3 = 300%, 4+ = 400% */
	int		min_index_bucket_count;
	int		index_divisions;
	int 		padding; /* values 1-8 */

	int		min_word_length;  	/* words shorter than this are not parsed */
	int		max_word_length;  	/* words longer than this are cropped */
	gboolean	use_stemmer;	 	/* enables stemming support */
	char		*language;		/* the language specific stemmer and stopwords to use */	
	gpointer	stemmer;		/* pointer to stemmer */	
	GMutex		*stemmer_mutex;

	GHashTable	*stop_words;	  	/* table of stop words that are to be ignored by the parser */
	gboolean	use_pango_word_break;

	gboolean	index_numbers;

	gboolean	first_time_index;
	gboolean	first_flush;
	gboolean	do_optimize;

	

	/* service/mime directory table - this is used to either store the service name or a pseudo mime type for all files under a certain directory root 
	 * pseduo mime types always start with "service/" followed by a unique name all in lowercase - EG "service/tomboy" 
	 * if the directory root represents a built in service which is handled by tracker then it will simply contain the service name - EG "Emails", "Conversations" etc 
	*/
	GHashTable	*service_directory_table;
	GSList		*service_directory_list;

	/* email config options */
	gboolean	index_evolution_emails;
	gboolean	index_thunderbird_emails;
	gboolean	index_kmail_emails;
	GSList		*additional_mboxes_to_index;

	/* nfs options */
	gboolean	use_nfs_safe_locking; /* use safer but much slower external lock file when users home dir is on an nfs systems */


	/* application run time values */
	gboolean	is_indexing;
	gboolean	in_flush;
	int		index_count;
	int		index_counter;
	int		update_count;


	/* cache words before saving to word index */
	GHashTable	*cached_table;
	GMutex		*cache_table_mutex;
	int		word_detail_limit;
	int		word_detail_count;
	int		word_detail_min;
	int		word_count;
	int		word_count_limit;
	int		word_count_min;
	int		flush_count;



	GSList 		*poll_list;
	
	int		file_update_count;
	int		email_update_count;

 	GHashTable  	*file_scheduler;
 	GMutex 		*scheduler_mutex;

 	gboolean 	is_running;
	gboolean	is_dir_scan;
	GMainLoop 	*loop;

	Indexer		*file_indexer;

	GMutex 		*log_access_mutex;
	char	 	*log_file;

	GAsyncQueue 	*file_process_queue;
	GAsyncQueue 	*file_metadata_queue;
	GAsyncQueue 	*user_request_queue;
	GAsyncQueue 	*dir_queue;

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
	char *lang;
	char *name;
} Matches;





typedef struct {

	/* file name/path related info */
	char 			*uri;
	guint32			file_id;

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
	gint32			mtime;
	gint32			atime;
	gint32			indextime;
	gint32			offset;

	/* options */
	char			*move_from_uri;
	gboolean		extract_embedded;
	gboolean		extract_contents;
	gboolean		extract_thumbs;

	/* we ref count FileInfo as it has a complex lifespan and is tossed between various threads, lists, queues and hash tables */
	int			ref_count;

} FileInfo;

void 		tracker_log 	(const char *message, ...);
void		tracker_info	(const char *message, ...);
void		tracker_debug 	(const char *message, ...);


int		tracker_get_id_for_service 		(const char *service);
const char *	tracker_get_service_by_id 		(int service_type_id);

GSList *	tracker_array_to_list 			(char **array);
char **		tracker_make_array_null_terminated 	(char **array, int length);

void		tracker_free_array 		(char **array, int row_count);
char *		tracker_int_to_str		(gint i);
char *		tracker_uint_to_str		(guint i);
gboolean	tracker_str_to_uint		(const char *s, guint *ret);
char *		tracker_format_date 		(const char *time_string);
time_t		tracker_str_to_date 		(const char *time_string);
char *		tracker_date_to_str 		(gint32 date_time);
int		tracker_str_in_array 		(const char *str, char **array);

char *		tracker_get_radix_by_suffix	(const char *str, const char *suffix);

char *		tracker_escape_metadata 	(const char *str);
char *		tracker_unescape_metadata 	(const char *str);

void		tracker_remove_dirs 		(const char *root_dir);
char *		tracker_format_search_terms 	(const char *str, gboolean *do_bool_search);

gboolean	tracker_is_supported_lang 	(const char *lang);
void		tracker_set_language		(const char *language, gboolean create_stemmer);

gint32		tracker_get_file_mtime 		(const char *uri);

void		tracker_add_service_path 	(const char *service, const char *path);
char *		tracker_get_service_for_uri 	(const char *uri);

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

gboolean	tracker_file_is_indexable 	(const char *uri);

gboolean 	tracker_is_directory 		(const char *dir);

gboolean	tracker_file_is_no_watched 	(const char* uri);

void 		tracker_get_dirs 		(const char *dir, GSList **file_list) ;

void		tracker_load_config_file 	(void);

GSList * 	tracker_get_watch_root_dirs 	(void);

gboolean	tracker_ignore_file 		(const char *uri);

void		tracker_print_object_allocations (void);

void		tracker_add_poll_dir 		(const char *dir);
void		tracker_remove_poll_dir 	(const char *dir);
gboolean	tracker_is_dir_polled 		(const char *dir);

void		tracker_throttle 		(int multiplier);

void		tracker_flush_all_words 	();
void		tracker_flush_rare_words 	();

void		tracker_notify_file_data_available 	(void);
void		tracker_notify_meta_data_available 	(void);
void		tracker_notify_request_data_available 	(void);

GTimeVal *	tracker_timer_start 		();
void		tracker_timer_end 		(GTimeVal *before, const char *str);

char *		tracker_compress 		(const char *ptr, int size, int *compressed_size);
char *		tracker_uncompress 		(const char *ptr, int size, int *uncompressed_size);

char *		tracker_get_snippet 		(const char *txt, char **terms, int length);

gboolean	tracker_spawn 			(char **argv, int timeout, char **stdout, int *exit_status);
void		tracker_child_cb 		(gpointer user_data);

#endif
