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


/* Needed before including math.h for lrintf() */
#define _ISOC9X_SOURCE   1
#define _ISOC99_SOURCE   1

#define __USE_ISOC9X     1
#define __USE_ISOC99     1

/* Size of free block pool of inverted index */
#define INDEXFBP         32     
#define SCORE_MULTIPLIER 100000
#define MAX_HIT_BUFFER 480000

#define CREATE_INDEX                                                      \
        "CREATE TABLE HitIndex (Word Text not null "                      \
        "unique, HitCount Integer, HitArraySize Integer, HitArray Blob);"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <depot.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-config.h>

#include "tracker-query-tree.h"
#include "tracker-indexer.h"
#include "tracker-cache.h"
#include "tracker-dbus.h"
#include "tracker-service-manager.h"
#include "tracker-query-tree.h"
#include "tracker-hal.h"
#include "tracker-process-files.h"

extern Tracker *tracker;

struct Indexer_ {
	DEPOT  		*word_index;	/* file hashtable handle for the word -> {serviceID, ServiceTypeID, Score}  */
	GMutex 		*word_mutex;
	char   		*name;
	gpointer  	emails; /* pointer to email indexer */
	gpointer  	data; /* pointer to file indexer */
	gboolean	main_index;
	gboolean	needs_merge; /* should new stuff be added directly or merged later on from a new index */
};

static inline gint16
get_score (WordDetails *details)
{
	unsigned char a[2];

	a[0] = (details->amalgamated >> 16) & 0xFF;
	a[1] = (details->amalgamated >> 8) & 0xFF;

	return (gint16) (a[0] << 8) | (a[1]);	
}


static inline guint8
get_service_type (WordDetails *details)
{
	return (details->amalgamated >> 24) & 0xFF;
}


guint32
tracker_indexer_calc_amalgamated (gint service, gint score)
{
	unsigned char a[4];
	gint16 score16;
	guint8 service_type;

	if (score > 30000) {
		score16 = 30000;
	} else {
		score16 = (gint16) score;
	}

	service_type = (guint8) service;

	/* amalgamate and combine score and service_type into a single 32-bit int for compact storage */	
	a[0] = service_type;
	a[1] = (score16 >> 8 ) & 0xFF ;
	a[2] = score16 & 0xFF ;
	a[3] = 0;

	return (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];	
}


static int
get_preferred_bucket_count (Indexer *indexer)
{
	gint result;
        gint bucket_ratio;

        bucket_ratio = tracker_config_get_bucket_ratio (tracker->config);
        result = dprnum (indexer->word_index);

	if (bucket_ratio < 1) {
		result /= 2;
	} else if (bucket_ratio > 3) {
		result *= 4;
	} else {
		result *= bucket_ratio;
	}

	tracker_log ("Preferred bucket count is %d", result);

	return  result;
}

gboolean
tracker_indexer_repair (const char *name)
{
	gboolean result = TRUE;
	char *index_name = g_build_filename (tracker->data_dir, name, NULL);

	result =  dprepair (index_name);
	g_free (index_name);

	return result;
}


static inline DEPOT *
open_index (const gchar *name)
{
	DEPOT *word_index = NULL;

	if (!name) return NULL;

	tracker_log ("Opening index %s", name);

	if (strstr (name, "tmp")) {
		word_index = dpopen (name, DP_OWRITER | DP_OCREAT | DP_ONOLCK, 
                                     tracker_config_get_min_bucket_count (tracker->config));
	} else {
		word_index = dpopen (name, DP_OWRITER | DP_OCREAT | DP_ONOLCK, 
                                     tracker_config_get_max_bucket_count (tracker->config));
	}

	if (!word_index) {
		tracker_error ("%s index was not closed properly and caused error %s- attempting repair", name, dperrmsg (dpecode));
		if (dprepair (name)) {
			word_index = dpopen (name, DP_OWRITER | DP_OCREAT | DP_ONOLCK, 
                                             tracker_config_get_min_bucket_count (tracker->config));
		} else {
			g_assert ("FATAL: index file is dead (suggest delete index file and restart trackerd)");
		}
	}

	return word_index;

}


static inline char *
get_index_file (const char *name)
{
	char *word_dir;

	word_dir = g_build_filename (tracker->data_dir, name, NULL);

	return word_dir;
}

Indexer *
tracker_indexer_open (const gchar *name, gboolean main_index)
{
	char *word_dir;
	DEPOT *word_index;
	Indexer *result;

	if (!name) return NULL;

	word_dir = get_index_file (name);

	word_index = open_index (word_dir);
	
	g_free (word_dir);

	result = g_new0 (Indexer, 1);

	result->main_index = main_index;
	
	result->needs_merge = FALSE;

	result->name = g_strdup (name);

	result->word_index = word_index;

	result->word_mutex = g_mutex_new ();

	dpsetalign (word_index , 8);

	/* re optimize database if bucket count < rec count */

	int bucket_count, rec_count;

	bucket_count = dpbnum (result->word_index);
	rec_count = dprnum (result->word_index);

	tracker_log ("Bucket count (max is %d) is %d and Record Count is %d", 
                     tracker_config_get_max_bucket_count (tracker->config),
                     bucket_count, rec_count);

	return result;
}


void
tracker_indexer_close (Indexer *indexer)
{	
	g_return_if_fail (indexer);

	g_mutex_lock (indexer->word_mutex);

	if (!dpclose (indexer->word_index)) {
		tracker_log ("Index closure has failed due to %s", dperrmsg (dpecode));
	}

	g_mutex_unlock (indexer->word_mutex);
	g_mutex_free (indexer->word_mutex);
	g_free (indexer->name);
	g_free (indexer);
}


void
tracker_indexer_free (Indexer *indexer, gboolean remove_file)
{
	

	if (remove_file) {

		char *dbname = g_build_filename (tracker->data_dir, indexer->name, NULL);

		g_return_if_fail (indexer);

		g_mutex_lock (indexer->word_mutex);

		dpremove (dbname);

		g_mutex_unlock (indexer->word_mutex);

		g_free (dbname);
	} else {
		g_mutex_lock (indexer->word_mutex);
		dpclose (indexer->word_index);
		g_mutex_unlock (indexer->word_mutex);
	}

	g_mutex_free (indexer->word_mutex);

	g_free (indexer->name);

	g_free (indexer);

	
}

const gchar *   
tracker_indexer_get_name (Indexer *indexer) 
{
        g_return_val_if_fail (indexer != NULL, NULL);
        
        return dpname (indexer->word_index);
}


guint32
tracker_indexer_size (Indexer *indexer)
{
	return dpfsiz (indexer->word_index);
}


void
tracker_indexer_sync (Indexer *indexer)
{
	g_mutex_lock (indexer->word_mutex);
	dpsync (indexer->word_index);
	g_mutex_unlock (indexer->word_mutex);
}


gboolean
tracker_indexer_optimize (Indexer *indexer)
{
 
	int num, b_count;

        if (tracker->shutdown) {
                return FALSE;
        }

	/* set bucket count to bucket_ratio times no. of recs divided by no. of divisions */
        num = CLAMP (get_preferred_bucket_count (indexer), 
                     tracker_config_get_min_bucket_count (tracker->config),
                     tracker_config_get_max_bucket_count (tracker->config));

	b_count = num / tracker_config_get_divisions (tracker->config);
	tracker_log ("No. of buckets per division is %d", b_count);

	tracker_log ("Please wait while optimization of indexes takes place...");
	tracker_log ("Index has file size %10.0f and bucket count of %d of which %d are used...", tracker_indexer_size (indexer), dpbnum (indexer->word_index), dpbusenum (indexer->word_index));
	
	g_mutex_lock (indexer->word_mutex);

	if (!dpoptimize (indexer->word_index, b_count)) {

		g_mutex_unlock (indexer->word_mutex);
		tracker_log ("Optimization has failed due to %s", dperrmsg (dpecode));
		return FALSE;
	}

	g_mutex_unlock (indexer->word_mutex);

	tracker_log ("Index has been successfully optimized to file size %10.0f and with bucket count of %d of which %d are used...", tracker_indexer_size (indexer), dpbnum (indexer->word_index), dpbusenum (indexer->word_index));
	
	
	return TRUE;
}

static inline gboolean 
has_word (Indexer *index, const char *word)
{
	char buffer [32];

	int count = dpgetwb (index->word_index, word, -1, 0, 32, buffer);

	return (count > 7);

}



void
tracker_indexer_apply_changes (Indexer *dest, Indexer *src,  gboolean update)
{
	char 	*str;
	char 	buffer[MAX_HIT_BUFFER];
	int 	bytes;
	int 	sz = sizeof (WordDetails);
	int 	i = 0, interval;
	int 	buff_size = MAX_HITS_FOR_WORD * sz;

	tracker_log ("applying incremental changes to indexes");

	guint32 size = tracker_indexer_size (dest);

	if (size < (10 * 1024 * 1024)) {
		interval = 20000;
	} else if (size < (20 * 1024 * 1024)) {
		interval = 10000;
	} else if (size < (30 * 1024 * 1024)) {
		interval = 5000;
	} else if (size < (100 * 1024 * 1024)) {
		interval = 3000;
	} else {
		interval = 2000;
	}

#ifdef HAVE_HAL 
	/* halve the interval value as notebook hard drives are smaller */
	if (tracker_hal_get_battery_exists (tracker->hal)) {
                interval /= 2;
        }
#endif

	dpiterinit (src->word_index);
	
	tracker->in_merge = TRUE;
	tracker->merge_count = 1;
	tracker->merge_processed = 0;
	tracker_dbus_send_index_progress_signal ("Merging", "");
	
	
	while ((str = dpiternext (src->word_index, NULL))) {
		
		i++;

		if (i > 1 && (i % 200 == 0)) {
			if (!tracker_cache_process_events (NULL, FALSE)) {
				return;	
			}
		}

		if (i > 1 && (i % interval == 0)) {
			if (!tracker_config_get_fast_merges (tracker->config)) {
                                dpsync (dest->word_index);
                        }
		}
			
		bytes = dpgetwb (src->word_index, str, -1, 0, buff_size, buffer);

		if (bytes < 1) continue;

		if (bytes % sz != 0) {
			tracker_error ("possible corruption found during application of changes to index with word %s (ignoring update for this word)", str);
			continue;
		}

		if (update) {
			tracker_indexer_update_word_chunk (dest, str, (WordDetails *) buffer, bytes / sz);
		} else {
			tracker_indexer_append_word_chunk (dest, str, (WordDetails *) buffer, bytes / sz);
		}
		
		dpout (src->word_index, str, -1);

		g_free (str);
	}
	
	dpsync (dest->word_index);

	/* delete src and recreate if file update index */

	tracker_indexer_free (src, TRUE);

	if (update) {
		tracker->file_update_index = tracker_indexer_open ("file-update-index.db", FALSE);
	}
	
	tracker->in_merge = FALSE;
	tracker->merge_count = 1;
	tracker->merge_processed = 1;
	tracker_dbus_send_index_progress_signal ("Merging", "");

}

gboolean
tracker_indexer_has_tmp_merge_files (IndexType type)
{
	GSList *files = NULL;
	gboolean result = FALSE;


	if (type == INDEX_TYPE_FILES) {
		files =  tracker_process_files_get_files_with_prefix (tracker, tracker->data_dir, "file-index.tmp.");
	} else {
		files =  tracker_process_files_get_files_with_prefix (tracker, tracker->data_dir, "email-index.tmp.");
	}

	result = (files != NULL);

	if (result) {
		g_slist_foreach (files, (GFunc) g_free, NULL);
		g_slist_free (files);
	}

	return result;

}



gboolean
tracker_indexer_has_merge_files (IndexType type)
{
	GSList *files = NULL;
	gboolean result = FALSE;
	char *final;

	if (type == INDEX_TYPE_FILES) {
		files =  tracker_process_files_get_files_with_prefix (tracker, tracker->data_dir, "file-index.tmp.");
		final = g_build_filename(tracker->data_dir, "file-index-final", NULL);
	} else {
		files =  tracker_process_files_get_files_with_prefix (tracker, tracker->data_dir, "email-index.tmp.");
		final = g_build_filename (tracker->data_dir, "email-index-final", NULL);
	}

	result = (files != NULL);

	if (!result) {
		result = g_file_test (final, G_FILE_TEST_EXISTS);
	} else {
		g_slist_foreach (files, (GFunc) g_free, NULL);
		g_slist_free (files);
	}

	g_free (final);

	return result;

}

static void
move_index (Indexer *src_index, Indexer *dest_index, const char *fname)
{

	if (!src_index || !dest_index) {
		tracker_error ("cannot move indexes");
		return;
	}

	/* remove existing main index */
	g_mutex_lock (dest_index->word_mutex);

	dpclose (dest_index->word_index);

	dpremove (fname);

	char *final_name = dpname (src_index->word_index);
			
	tracker_indexer_close (src_index);
		
	/* rename and reopen final index as main index */
		
	tracker_log ("renaming %s to %s", final_name, fname);
	
	rename (final_name, fname);

	dest_index->word_index = open_index (fname);	

	if (!dest_index->word_index) {
		tracker_error ("index creation failure for %s from %s", fname, final_name);
	}

	g_free (final_name);		

	g_mutex_unlock (dest_index->word_mutex);

}


void
tracker_indexer_merge_indexes (IndexType type)
{
	GSList     *lst;
	Indexer    *final_index;
	GSList     *file_list = NULL, *index_list = NULL;
	const char *prefix;
	gint       i = 0, index_count, interval = 5000;
	gboolean   final_exists;

	if (tracker->shutdown) return;

	if (type == INDEX_TYPE_FILES) {

		g_return_if_fail (tracker->file_index);
		
		prefix = "file-index.tmp.";

		index_list = g_slist_prepend (index_list, tracker->file_index);

		char *tmp = g_build_filename (tracker->data_dir, "file-index-final", NULL);

		final_exists = g_file_test (tmp, G_FILE_TEST_EXISTS);

		g_free (tmp);

	} else {
		prefix = "email-index.tmp.";

		g_return_if_fail (tracker->email_index);
		
		index_list = g_slist_prepend (index_list, tracker->email_index);

		char *tmp = g_build_filename (tracker->data_dir, "email-index-final", NULL);

		final_exists = g_file_test (tmp, G_FILE_TEST_EXISTS);

		g_free (tmp);
	}
	
	file_list = tracker_process_files_get_files_with_prefix (tracker, tracker->data_dir, prefix);

	if (!file_list || !file_list->data) {
		
		g_slist_free (index_list);

		return;

	} else {
                GSList *file;

		for (file = file_list; file; file = file->next) {

			if (file->data) {
				char *name = g_path_get_basename (file->data);
				if (name) {

					if (g_file_test (file->data, G_FILE_TEST_EXISTS)) {

                                                Indexer *tmp_index = tracker_indexer_open (name, FALSE);
						if (tmp_index) {
							index_list = g_slist_prepend (index_list, tmp_index);
						}
					}

					g_free (name);
				}
			}
		}

		g_slist_foreach (file_list, (GFunc) g_free, NULL);
		g_slist_free (file_list);
	}

 	index_count = g_slist_length (index_list);

	if (index_count < 2) {

		g_slist_free (index_list);

		return;
	}

	tracker_log ("starting merge of %d indexes", index_count);
	tracker->in_merge = TRUE;
	tracker->merge_count = index_count;
	tracker->merge_processed = 0;
	tracker_dbus_send_index_progress_signal ("Merging", "");
	

	if (index_count == 2 && !final_exists) {

                Indexer *index1 = index_list->data ;
                Indexer *index2 = index_list->next->data ;

		if (tracker_indexer_size (index1) * 3 < tracker_indexer_size (index2)) {

			tracker_indexer_apply_changes (index2, index1, FALSE);

			g_slist_free (index_list);

			goto end_of_merging;
		}
	}

	tracker_dbus_send_index_status_change_signal ();

	if (type == INDEX_TYPE_FILES) {
		final_index = tracker_indexer_open ("file-index-final", TRUE);
	} else {
		final_index = tracker_indexer_open ("email-index-final", TRUE);
	}

	if (!final_index) {
		g_slist_free (index_list);
		tracker_error ("could not open final index - abandoning index merge");
		goto end_of_merging;
	}

	for (lst = index_list; lst && lst->data; lst = lst->next) {
                gchar   *str;
		Indexer *index = lst->data;

		dpiterinit (index->word_index);

		while ((str = dpiternext (index->word_index, NULL))) {

			gchar buffer[MAX_HIT_BUFFER];
			gint offset;
			gint sz = sizeof (WordDetails);
			gint buff_size = MAX_HITS_FOR_WORD * sz;

			if (!has_word (final_index, str)) {

				i++;

				if (i > 101 && (i % 100 == 0)) {
					if (!tracker_cache_process_events (NULL, FALSE)) {
						tracker_set_status (tracker, STATUS_SHUTDOWN, 0, TRUE);
						return;	
					}
				}

				if (i > interval && (i % interval == 0)) {

                                        if (!tracker_config_get_fast_merges (tracker->config)) {

						dpsync (final_index->word_index);

						guint32 size = tracker_indexer_size (final_index);

						if (size < (10 * 1024 * 1024)) {
							interval = 10000;
						} else if (size < (20 * 1024 * 1024)) {
							interval = 6000;
						} else if (size < (50 * 1024 * 1024)) {
							interval = 6000;
						} else if (size < (100 * 1024 * 1024)) {
							interval = 4000;
						} else {
							interval = 3000;
						}

#ifdef HAVE_HAL 
						/* halve the interval value as notebook hard drives are smaller */
                                                if (tracker_hal_get_battery_exists (tracker->hal)) {
                                                        interval /=  2;
                                                }
#endif
					}
				}
			
				offset = dpgetwb (index->word_index, str, -1, 0, buff_size, buffer);

				if (offset < 1) {
                                        continue;
                                }

				if (offset % sz != 0) {
					tracker_error ("possible corruption found during merge of word %s - purging word from index (it will not be searchable)", str);
					continue;
				}

				if (offset > 7 && offset < buff_size) {

					GSList *list;

					for (list = lst->next; list; list = list->next) {
                                                gchar   tmp_buffer[MAX_HIT_BUFFER];
						Indexer *tmp_index = list->data;

						if (!tmp_index) {
                                                        continue;
                                                }

						gint tmp_offset = dpgetwb (tmp_index->word_index, str, -1, 0, (buff_size - offset), tmp_buffer);	

						if (tmp_offset > 0 && (tmp_offset % sz != 0)) {
							tracker_error ("possible corruption found during merge of word %s - purging word from index", str);
							continue;
						}

						if (tmp_offset > 7 && (tmp_offset % sz == 0)) {
							memcpy (buffer + offset, tmp_buffer, tmp_offset);
							offset += tmp_offset;
						}												
					}

					dpput (final_index->word_index, str, -1, buffer, offset, DP_DOVER);
				}
			}

			g_free (str);
		}


		

		/* dont free last entry as that is the main index */
		if (lst->next) {

			if (index != tracker->file_index && index != tracker->email_index) {
				tracker_indexer_free (index, TRUE);
				tracker->merge_processed++;
				tracker_dbus_send_index_progress_signal ("Merging", "");	
			}


		} else {
			if (type == INDEX_TYPE_FILES) {

				char *fname = get_index_file ("file-index.db");
				move_index (final_index, tracker->file_index, fname);	
				g_free (fname);

			} else {
				char *fname = get_index_file ("email-index.db");
				move_index (final_index, tracker->email_index, fname);
				g_free (fname);
			}
		}		
	}
	
	g_slist_free (index_list);

	

 end_of_merging:
	tracker->in_merge = FALSE;
	tracker_dbus_send_index_status_change_signal ();
	
}



/* indexing api */

/* use for fast insertion of a word for multiple documents at a time */

gboolean
tracker_indexer_append_word_chunk (Indexer *indexer, const gchar *word, WordDetails *details, gint word_detail_count)
{
        if (tracker->shutdown) {
                return FALSE;
        }

	g_return_val_if_fail (indexer, FALSE);
	g_return_val_if_fail (indexer->word_index, FALSE);
	g_return_val_if_fail (word, FALSE);
	g_return_val_if_fail (details, FALSE);
	g_return_val_if_fail (word_detail_count > 0, FALSE);

	g_mutex_lock (indexer->word_mutex);
	if (!dpput (indexer->word_index, word, -1, (char *) details, (word_detail_count * sizeof (WordDetails)), DP_DCAT)) {
		g_mutex_unlock (indexer->word_mutex);
		return FALSE;
	}
	g_mutex_unlock (indexer->word_mutex);

	return TRUE;	
}


/* append individual word for a document */

gboolean
tracker_indexer_append_word (Indexer *indexer, const gchar *word, guint32 id, gint service, gint score)
{
        if (score < 1) {
                return FALSE;
        }

	g_return_val_if_fail (indexer, FALSE);
	g_return_val_if_fail (indexer->word_index, FALSE);
        g_return_val_if_fail (word, FALSE);

	WordDetails pair;

	pair.id = id;
	pair.amalgamated = tracker_indexer_calc_amalgamated (service, score);

	return tracker_indexer_append_word_chunk (indexer, word, &pair, 1);
}


/* append lists of words for a document - returns no. of hits added */
gint
tracker_indexer_append_word_list (Indexer *indexer, const gchar *word, GSList *list)
{
	WordDetails word_details[MAX_HITS_FOR_WORD], *wd;
	gint i;
	GSList *lst;

	g_return_val_if_fail (indexer, 0);
	g_return_val_if_fail (indexer->word_index, 0);
	g_return_val_if_fail (word, 0);

	i = 0;

	if (list) {
		for (lst = list; (lst && i < MAX_HITS_FOR_WORD); lst = lst->next) {

			if (lst->data) {
				wd = lst->data;
				word_details[i].id = wd->id;
				word_details[i].amalgamated = wd->amalgamated;
				i++;
			}
		}
	}

	if (i > 0) {
		tracker_indexer_append_word_chunk (indexer, word, word_details, i);
	}

	return i;
}



/* use for deletes or updates of multiple entities when they are not new */
gboolean
tracker_indexer_update_word_chunk (Indexer *indexer, const gchar *word, WordDetails *detail_chunk, gint word_detail_count)
{	
	int  tsiz, j, i, score;
	char *tmp;
	WordDetails *word_details;
	gboolean write_back = FALSE;
	GSList *list = NULL;

	g_return_val_if_fail (indexer, FALSE);
	g_return_val_if_fail (indexer->word_index, FALSE);
	g_return_val_if_fail (word, FALSE);
	g_return_val_if_fail (detail_chunk, FALSE);
	g_return_val_if_fail (word_detail_count > 0, FALSE);

	/* check if existing record is there */
	gint hit_count = 0;

	g_mutex_lock (indexer->word_mutex);

	if ((tmp = dpget (indexer->word_index, word, -1, 0, MAX_HIT_BUFFER, &tsiz)) != NULL) {


		WordDetails *details = (WordDetails *) tmp;
		hit_count = tsiz / sizeof (WordDetails);

		details = (WordDetails *) tmp;

		for (j = 0; j < word_detail_count; j++) {

			word_details = &detail_chunk[j];

			gboolean edited = FALSE;

			for (i = 0; i < hit_count; i++) {

				if (details[i].id == word_details->id) {

					write_back = TRUE;

					/* NB the paramter score can be negative */
					score = get_score (&details[i]) + get_score (word_details);
					//g_print ("current score for %s is %d and new is %d and final is %d\n", word, get_score (&details[i]), get_score (word_details), score); 

							
					/* check for deletion */		
					if (score < 1) {

						//g_print ("deleting word hit %s\n", word);
						
						gint k;
					
						/* shift all subsequent records in array down one place */
						for (k = i + 1; k < hit_count; k++) {
							details[k - 1] = details[k];
						}

						hit_count--;
	
					} else {
						details[i].amalgamated = tracker_indexer_calc_amalgamated (get_service_type (&details[i]), score);
					}

					edited = TRUE;
					break;
				}
			}

			/* add hits that could not be updated directly here so they can be appended later */
			if (!edited) {
				list = g_slist_prepend (list, &detail_chunk[j]);
				tracker_debug ("could not update word hit %s - appending", word);
			}
		}
	
		/* write back if we have modded anything */
		if (write_back) {
			dpput (indexer->word_index, word, -1, (char *) details, (hit_count * sizeof (WordDetails)), DP_DOVER);
		}

		g_mutex_unlock (indexer->word_mutex);	

		if (list) {
			tracker_indexer_append_word_list (indexer, word, list);
			g_slist_free (list);
		}
	
		return TRUE;
	}

	g_mutex_unlock (indexer->word_mutex);	

	/* none of the updates can be applied if word does not exist so return them all to be appended later */
	return tracker_indexer_append_word_chunk (indexer, word, detail_chunk, word_detail_count);

}


/* use for deletes or updates of multiple entities when they are not new */
gboolean
tracker_indexer_update_word_list (Indexer *indexer, const gchar *word, GSList *update_list)
{
	WordDetails word_details[MAX_HITS_FOR_WORD], *wd;
	gint i;
	GSList *lst;

	g_return_val_if_fail (indexer, 0);
	g_return_val_if_fail (indexer->word_index, 0);
	g_return_val_if_fail (word, 0);

	i = 0;

	if (update_list) {
		for (lst = update_list; (lst && i < MAX_HITS_FOR_WORD); lst = lst->next) {

			if (lst->data) {
				wd = lst->data;
				word_details[i].id = wd->id;
				word_details[i].amalgamated = wd->amalgamated;
				i++;
			}
		}
	}

	if (i > 0) {
		tracker_indexer_update_word_chunk (indexer, word, word_details, i);
	}

	return i;
}

WordDetails *
tracker_indexer_get_word_hits (Indexer     *indexer,
			       const gchar *word,
			       guint       *count)
{
	WordDetails *details;
	gint tsiz;
	gchar *tmp;

	g_mutex_lock (indexer->word_mutex);

	details = NULL;
	*count = 0;

	if ((tmp = dpget (indexer->word_index, word, -1, 0, MAX_HIT_BUFFER, &tsiz)) != NULL) {
		if (tsiz >= (int) sizeof (WordDetails)) {
			details = (WordDetails *) tmp;
			*count = tsiz / sizeof (WordDetails);
		}
	}

	g_mutex_unlock (indexer->word_mutex);

	return details;
}

/* use to delete dud hits for a word - dud_list is a list of TrackerSearchHit structs */
gboolean
tracker_remove_dud_hits (Indexer *indexer, const gchar *word, GSList *dud_list)
{
	gint tsiz;
	char *tmp;

	g_return_val_if_fail (indexer, FALSE);
	g_return_val_if_fail (indexer->word_index, FALSE);
	g_return_val_if_fail (word, FALSE);
	g_return_val_if_fail (dud_list, FALSE);
	
	g_mutex_lock (indexer->word_mutex);

	/* check if existing record is there  */
	if ((tmp = dpget (indexer->word_index, word, -1, 0, MAX_HIT_BUFFER, &tsiz)) != NULL) {

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *details;
			int wi, i, pnum;

			details = (WordDetails *) tmp;
			pnum = tsiz / sizeof (WordDetails);
			wi = 0;	

			for (i = 0; i < pnum; i++) {

				GSList *lst;

				for (lst = dud_list; lst; lst = lst->next) {

					TrackerSearchHit *hit = lst->data;

					if (hit) {
						if (details[i].id == hit->service_id) {
							int k;

							/* shift all subsequent records in array down one place */
							for (k = i + 1; k < pnum; k++) {
								details[k - 1] = details[k];
							}

							/* make size of array one size smaller */
							tsiz -= sizeof (WordDetails); 
							pnum--;

							break;
						}
					}
				}
			}

			dpput (indexer->word_index, word, -1, (char *) details, tsiz, DP_DOVER);
			
			g_mutex_unlock (indexer->word_mutex);	
	
			g_free (tmp);

			return TRUE;
		}

		g_free (tmp);
	}

	g_mutex_unlock (indexer->word_mutex);

	return FALSE;
}

static inline gint
count_hit_size_for_word (Indexer *indexer, const gchar *word)
{
	int  tsiz;

	g_mutex_lock (indexer->word_mutex);	
	tsiz = dpvsiz (indexer->word_index, word, -1);
	g_mutex_unlock (indexer->word_mutex);	

	return tsiz;
}

guint8
tracker_word_details_get_service_type (WordDetails *details)
{
	return (details->amalgamated >> 24) & 0xFF;
}

gint16
tracker_word_details_get_score (WordDetails *details)
{
	unsigned char a[2];

	a[0] = (details->amalgamated >> 16) & 0xFF;
	a[1] = (details->amalgamated >> 8) & 0xFF;

	return (gint16) (a[0] << 8) | (a[1]);	
}

/* int levenshtein ()
 * Original license: GNU Lesser Public License
 * from the Dixit project, (http://dixit.sourceforge.net/)
 * Author: Octavian Procopiuc <oprocopiuc@gmail.com>
 * Created: July 25, 2004
 * Copied into tracker, by Edward Duffy
 */

static int
levenshtein(const char *source, char *target, int maxdist)
{
	char n, m;
	int l;
	l = strlen (source);
	if (l > 50)
		return -1;
	n = l;

	l = strlen (target);
	if (l > 50)
		return -1;
	m = l;

	if (maxdist == 0)
		maxdist = MAX(m, n);
	if (n == 0)
		return MIN(m, maxdist);
	if (m == 0)
		return MIN(n, maxdist);

	// Store the min. value on each column, so that, if it reaches
	// maxdist, we break early.
	char mincolval;

	char matrix[51][51];

	char j;
	char i;
	char cell;

	for (j = 0; j <= m; j++)
		matrix[0][(int)j] = j;

	for (i = 1; i <= n; i++) {

		mincolval = MAX(m, i);
		matrix[(int)i][0] = i;

		char s_i = source[i-1];

		for (j = 1; j <= m; j++) {

			char t_j = target[j-1];

			char cost = (s_i == t_j ? 0 : 1);

			char above = matrix[i-1][(int)j];
			char left = matrix[(int)i][j-1];
			char diag = matrix[i-1][j-1];
			cell = MIN(above + 1, MIN(left + 1, diag + cost));

			// Cover transposition, in addition to deletion,
			// insertion and substitution. This step is taken from:
			// Berghel, Hal ; Roach, David : "An Extension of Ukkonen's 
			// Enhanced Dynamic Programming ASM Algorithm"
			// (http://www.acm.org/~hlb/publications/asm/asm.html)

			if (i > 2 && j > 2) {
				char trans = matrix[i-2][j-2] + 1;
				if (source[i-2] != t_j)
					trans++;
				if (s_i != target[j-2])
					trans++;
				if (cell > trans)
					cell = trans;
			}

			mincolval = MIN(mincolval, cell);
			matrix[(int)i][(int)j] = cell;
		}

		if (mincolval >= maxdist)
			break;

	}

	if (i == n + 1)
		return (int) matrix[(int)n][(int)m];
	else
		return maxdist;
}

static int
count_hits_for_word (Indexer *indexer, const gchar *str) {
        
        gint tsiz, hits = 0;

        tsiz = count_hit_size_for_word (indexer, str);

        if (tsiz == -1 || tsiz % sizeof (WordDetails) != 0) {
                return -1;
        }

        hits = tsiz / sizeof (WordDetails);

        return hits;
}

char *
tracker_indexer_get_suggestion (Indexer *indexer, const gchar *term, gint maxdist)
{

	gchar		*str;
	gint		dist; 
	gchar		*winner_str;
	gint		winner_dist;
	gint		hits;
	GTimeVal	start, current;

	winner_str = g_strdup (term);
        winner_dist = G_MAXINT;  /* Initialize to the worst case */

        dpiterinit (indexer->word_index);

	g_get_current_time (&start);

	str = dpiternext (indexer->word_index, NULL);

	while (str != NULL) {

		dist = levenshtein (term, str, 0);

		if (dist != -1 && dist < maxdist && dist < winner_dist) {

                        hits = count_hits_for_word (indexer, str);

                        if (hits < 0) {

                                g_free (winner_str);
                                g_free (str);
                                return NULL;

			} else if (hits > 0) {

                                g_free (winner_str);
                                winner_str = g_strdup (str);
                                winner_dist = dist;

                        } else {
				tracker_log ("No hits for %s!", str);
			}
		}

		g_free (str);

		g_get_current_time (&current);

		if (current.tv_sec - start.tv_sec >= 2) { /* 2 second time out */
			tracker_log ("Timeout in tracker_dbus_method_search_suggest");
                        break;
		}

		str = dpiternext (indexer->word_index, NULL);
	}

        return winner_str;
}
