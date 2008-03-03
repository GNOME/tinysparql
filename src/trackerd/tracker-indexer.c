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

#define SCORE_MULTIPLIER 100000
#define INDEXFBP        32               /* size of free block pool of inverted index */
#define CREATE_INDEX "CREATE TABLE HitIndex (Word Text not null unique, HitCount Integer, HitArraySize Integer, HitArray Blob);"
#define MAX_HIT_BUFFER 480000

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <sqlite3.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "tracker-indexer.h"
#include "tracker-cache.h"
#include "tracker-dbus.h"

extern Tracker *tracker;

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


static inline SearchHit *
word_details_to_search_hit (WordDetails *details)
{
	SearchHit *hit = g_slice_new (SearchHit);

	hit->service_id = details->id;
	hit->service_type_id = get_service_type (details);
	hit->score = get_score (details);

	return hit;
}


static inline SearchHit *
copy_search_hit (SearchHit *src)
{
	SearchHit *hit = g_slice_new (SearchHit);

	hit->service_id = src->service_id;
	hit->service_type_id = src->service_type_id;
	hit->score = src->score;

	return hit;
}


static gint 
compare_words (const void *a, const void *b)
{
	WordDetails *ap, *bp;

	ap = (WordDetails *)a;
	bp = (WordDetails *)b;

	return (get_score(bp) - get_score(ap));
}


static gint 
compare_search_hits (const void *a, const void *b)
{
	SearchHit *ap, *bp;

	ap = (SearchHit *)a;
	bp = (SearchHit *)b;

	return (bp->score - ap->score);
}


static gint 
compare_search_words (const void *a, const void *b)
{
	SearchWord *ap, *bp;

	ap = (SearchWord *)a;
	bp = (SearchWord *)b;

	return (ap->hit_count - bp->hit_count);	
}


void		
tracker_index_free_hit_list (GSList *hit_list)
{
	GSList *lst;

	if (!hit_list) {
                return;
        }

	for (lst = hit_list; lst; lst = lst->next) {
                SearchHit *hit = lst->data;
		g_slice_free (SearchHit, hit);
	}

	g_slist_free (hit_list);
}

static int
get_preferred_bucket_count (Indexer *indexer)
{
	int result;

	if (tracker->index_bucket_ratio < 1) {

		result = (dprnum (indexer->word_index)/2);

	} else if (tracker->index_bucket_ratio > 3) {

		result = (dprnum (indexer->word_index) * 4);

	} else {

		result = (tracker->index_bucket_ratio * dprnum (indexer->word_index));
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
		word_index = dpopen (name, DP_OWRITER | DP_OCREAT | DP_ONOLCK, tracker->min_index_bucket_count);
	} else {
		word_index = dpopen (name, DP_OWRITER | DP_OCREAT | DP_ONOLCK, tracker->max_index_bucket_count);
	}

	if (!word_index) {
		tracker_error ("%s index was not closed properly and caused error %s- attempting repair", name, dperrmsg (dpecode));
		if (dprepair (name)) {
			word_index = dpopen (name, DP_OWRITER | DP_OCREAT | DP_ONOLCK, tracker->min_index_bucket_count);
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
tracker_indexer_open (const gchar *name)
{
	char *word_dir;
	DEPOT *word_index;
	Indexer *result;

	if (!name) return NULL;

	word_dir = get_index_file (name);

	word_index = open_index (word_dir);
	
	g_free (word_dir);

	result = g_new0 (Indexer, 1);

	result->main_index = FALSE;
	
	result->needs_merge = FALSE;

	result->name = g_strdup (name);

	result->word_index = word_index;

	result->word_mutex = g_mutex_new ();

	dpsetalign (word_index , 8);

	/* re optimize database if bucket count < rec count */

	int bucket_count, rec_count;

	bucket_count = dpbnum (result->word_index);
	rec_count = dprnum (result->word_index);

	tracker_log ("Bucket count (max is %d) is %d and Record Count is %d", tracker->max_index_bucket_count, bucket_count, rec_count);

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
	num = (get_preferred_bucket_count (indexer));

	if (num > tracker->max_index_bucket_count) {
		num = tracker->max_index_bucket_count;
	}

	if (num < tracker->min_index_bucket_count) {
		num = tracker->min_index_bucket_count;
	}

	b_count = (num / tracker->index_divisions);
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

	/* halve the interval value as notebook hard drives are smaller */
	if (tracker->battery_udi) interval = interval / 2;

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
			if (!tracker->fast_merges) dpsync (dest->word_index);
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
		tracker->file_update_index = tracker_indexer_open ("file-update-index.db");
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
		files =  tracker_get_files_with_prefix (tracker->data_dir, "file-index.tmp.");
	} else {
		files =  tracker_get_files_with_prefix (tracker->data_dir, "email-index.tmp.");
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
		files =  tracker_get_files_with_prefix (tracker->data_dir, "file-index.tmp.");
		final = g_build_filename(tracker->data_dir, "file-index-final", NULL);
	} else {
		files =  tracker_get_files_with_prefix (tracker->data_dir, "email-index.tmp.");
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
	
	file_list = tracker_get_files_with_prefix (tracker->data_dir, prefix);

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

                                                Indexer *tmp_index = tracker_indexer_open (name);
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
		final_index = tracker_indexer_open ("file-index-final");
	} else {
		final_index = tracker_indexer_open ("email-index-final");
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
						tracker->status = STATUS_SHUTDOWN;
						tracker_dbus_send_index_status_change_signal ();
						return;	
					}
				}

				if (i > interval && (i % interval == 0)) {

					if (!tracker->fast_merges) {

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

						/* halve the interval value as notebook hard drives are smaller */
						if (tracker->battery_udi) interval = interval / 2;
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



/* use to delete dud hits for a word - dud_list is a list of SearchHit structs */
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

					SearchHit *hit = lst->data;

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


static gint
get_idf_score (WordDetails *details, float idf)
{
	guint32 score = get_score (details);
	float f = idf * score * SCORE_MULTIPLIER;

        return (f > 1.0) ? lrintf (f) : 1;
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


static inline gboolean
in_array (gint *array, gint count, gint value)
{
	gint i;

	for (i = 0; i < count; i++) {
		if (array[i] == value) {
			return TRUE;
		}
	}

	return FALSE;
}


SearchQuery *
tracker_create_query (Indexer *indexer, gint *service_array, gint service_array_count, gint offset, gint limit)
{
	SearchQuery *result = g_slice_new0 (SearchQuery);

	result->indexer = indexer;
	result->service_array = service_array;
	result->service_array_count = service_array_count;
	result->offset = offset;
	result->limit = limit;	

	return result;
}


void
tracker_add_query_word (SearchQuery *query, const gchar *word, WordType word_type)
{
	if (!word || word[0] == 0 || (word[0] == ' ' && word[1] == 0)) {
		return;
	}

	SearchWord *result = g_slice_new0 (SearchWord);

	result->word = g_strdup (word);
	result->hit_count = 0;
	result->idf = 0;
	result->word_type = word_type;

	query->words = g_slist_prepend (query->words, result);
}


static void
free_word (SearchWord *result)
{
	g_free (result->word);
	g_slice_free (SearchWord, result);
}


void
tracker_free_query (SearchQuery *query)
{
	tracker_index_free_hit_list (query->hits);

	/* Do not free individual dud hits - dud SearchHit structs are always part
           of the hit list so will already be freed when hit list is freed above */
	g_slist_free (query->duds);

	g_slist_foreach (query->words, (GFunc) free_word, NULL);
	g_slist_free (query->words);

	g_slice_free (SearchQuery, query);
}


static GSList *
get_hits_for_single_word (SearchQuery *query, SearchWord *search_word, gint *return_hit_count)
{
	int  tsiz, total_count = 0;
	char *tmp = NULL;
	GSList *result = NULL;

	int offset = query->offset;

	/* some results might be dud so get an extra 50 to compensate */
	int limit = query->limit + 50;
	
	if (tracker->shutdown) {
                return NULL;
        }

	g_mutex_lock (query->indexer->word_mutex);

	if ((tmp = dpget (query->indexer->word_index, search_word->word, -1, 0, MAX_HIT_BUFFER, &tsiz)) != NULL) {

		g_mutex_unlock (query->indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {
			WordDetails *details;
			int pnum;

			details = (WordDetails *) tmp;
			
			pnum = tsiz / sizeof (WordDetails);

			tracker_debug ("total hit count (excluding service divisions) is %d", pnum);
		
			qsort (details, pnum, sizeof (WordDetails), compare_words);

			int i;
			for (i = 0; i < pnum; i++) {
				int service;

				service = get_service_type (&details[i]);

				if (!query->service_array || in_array (query->service_array, query->service_array_count, service)) {

					total_count++;

					if (offset != 0) {
						offset--;
						continue;
					}

					if (limit > 0) {
						SearchHit *hit;

						hit = word_details_to_search_hit (&details[i]);

						result = g_slist_prepend (result, hit);
		
						limit--;

					} else {
						continue;
					}
				}
			}

                        tracker_debug ("total hit count for service is %d", total_count);
		}

	} else {
		g_mutex_unlock (query->indexer->word_mutex);
	}

	*return_hit_count = total_count;

	g_free (tmp);

	return g_slist_reverse (result);
}


static GHashTable *
get_intermediate_hits (SearchQuery *query, GHashTable *match_table, SearchWord *search_word, BoolOp bool_op)
{
	int  tsiz;
	char *tmp;
	GHashTable *result;

	if (tracker->shutdown) {
                return NULL;
        }

	result = g_hash_table_new (NULL, NULL);

	g_mutex_lock (query->indexer->word_mutex);

	if ((tmp = dpget (query->indexer->word_index, search_word->word, -1, 0, MAX_HIT_BUFFER, &tsiz)) != NULL) {

		g_mutex_unlock (query->indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *details;
			int count;

			details = (WordDetails *) tmp;
			
			count = tsiz / sizeof (WordDetails);
				
			int i;
			for (i = 0; i < count; i++) {

				int service;
				service = get_service_type (&details[i]);

				if (!query->service_array || in_array (query->service_array, query->service_array_count, service)) {

					if (match_table) {
						gpointer pscore;
						guint32 score;
					
						pscore = g_hash_table_lookup (match_table, GUINT_TO_POINTER (details[i].id));

						if (bool_op == BoolAnd && !pscore) {
							continue;
						}

						score = GPOINTER_TO_UINT (pscore) +  get_idf_score (&details[i], search_word->idf);

						g_hash_table_insert (result, GUINT_TO_POINTER (details[i].id), GUINT_TO_POINTER (score));   	

					} else {
						int idf_score = get_idf_score (&details[i], search_word->idf);

						g_hash_table_insert (result, GUINT_TO_POINTER (details[i].id), GUINT_TO_POINTER (idf_score));
					}
				}
			}
		}

		g_free (tmp);

	} else {
		g_mutex_unlock (query->indexer->word_mutex);
	}

	if (match_table) {
		g_hash_table_destroy (match_table);
	}

	return result;
}


static GSList *
get_final_hits (SearchQuery *query, GHashTable *match_table, SearchWord *search_word, BoolOp bool_op, gint *return_hit_count)
{
	int  tsiz, rnum;
	char *tmp;
	SearchHit *result;
	GSList *list;

	int offset = query->offset;

	/* some results might be dud so get an extra 50 to compensate */
	int limit = query->limit + 50;

	*return_hit_count = 0;

	rnum = 0;
	list = NULL;

	if (tracker->shutdown) {
                return NULL;
        }

	if (!match_table || g_hash_table_size (match_table) < 1) {
		return NULL;
	}
				
	g_mutex_lock (query->indexer->word_mutex);

	if ((tmp = dpget (query->indexer->word_index, search_word->word, -1, 0, MAX_HIT_BUFFER, &tsiz)) != NULL) {

		g_mutex_unlock (query->indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *details;
			int size, count;

			details = (WordDetails *) tmp;
			
			count = tsiz / sizeof (WordDetails);
	
			size = g_hash_table_size (match_table);

			if (bool_op == BoolAnd) {
				result = g_malloc0 (sizeof (SearchHit) * size);
			} else {
				result = g_malloc0 (sizeof (SearchHit) * (size + count));
			}
				
			int i;
			for (i = 0; i < count; i++) {

				int service;
				service = get_service_type (&details[i]);

				if (!query->service_array || in_array (query->service_array, query->service_array_count, service)) {

					gpointer pscore;
					int score;
				
					pscore = g_hash_table_lookup (match_table, GUINT_TO_POINTER (details[i].id));

					if (bool_op == BoolAnd && !pscore) {
						continue;
					}

					/* implements IDF * TF here  */
					score = GPOINTER_TO_INT (pscore) +  get_idf_score (&details[i], search_word->idf);

					result[rnum].service_id = details[i].id;
					result[rnum].service_type_id = service;
    					result[rnum].score = score;
				    
					rnum++;
				}
			}

			qsort (result, rnum, sizeof (SearchHit), compare_search_hits);

			*return_hit_count = rnum;

			if (offset > rnum) {
				tracker_log ("WARNING: offset is too big - no results will be returned for search!");
				g_free (tmp);
				g_free (result);
				return NULL;
			}

			if ((limit + offset) < rnum) { 
				count = limit + offset;
			} else {
				count = rnum;
			}

			for (i = offset; i < count; i++) {
				list = g_slist_prepend (list, copy_search_hit (&result[i]));
			}

			g_free (result);
		}

		g_free (tmp);

	} else {
		g_mutex_unlock (query->indexer->word_mutex);
	}

	return g_slist_reverse (list);
}


gboolean
tracker_indexer_get_hits (SearchQuery *query)
{
	GHashTable *table;
	GSList *lst;

	if (tracker->shutdown) {
                return FALSE;
	}

	g_return_val_if_fail (query->indexer, FALSE);
	g_return_val_if_fail (query->limit > 0, FALSE);

	if (!query->words) {
		return TRUE;
	}

	/* do simple case of only one search word fast */
	if (!query->words->next) {
		SearchWord *word = query->words->data;

		if (!word) {
                        return FALSE;
                }

		query->hits = get_hits_for_single_word (query, word, &query->hit_count);

		return TRUE;
	}

	/* calc stats for each word */
	for (lst = query->words; lst; lst = lst->next) {
		SearchWord *word = lst->data;

		if (!word) {
                        return FALSE;
                }

		word->hit_count = count_hit_size_for_word (query->indexer, word->word);
		word->idf = 1.0/word->hit_count;

		if (word->hit_count < 1) {
			return FALSE;
		}
	}

	/* do multiple word searches - start with words with fewest hits first */

	query->words = g_slist_sort (query->words, (GCompareFunc) compare_search_words);

	table = NULL;

	for (lst = query->words; lst; lst = lst->next) {
		SearchWord *word = lst->data;

		if (!word) {
                        return FALSE;
                }

		if (lst->next) {
			table = get_intermediate_hits (query, table, word, BoolAnd);

			if (g_hash_table_size (table) == 0) {
				query->hit_count = 0;
				g_hash_table_destroy (table);
				return FALSE;
			}

		} else {
			query->hits = get_final_hits (query, table, word, BoolAnd, &query->hit_count);

			if (table) {
				g_hash_table_destroy (table);
			}
				
			return TRUE;
		}
        }

	return FALSE;
}


static gint
prepend_key_pointer (gpointer key, gpointer value, gpointer data)
{
  	GSList **plist = data;
  	*plist = g_slist_prepend (*plist, key);
  	return 1;
}


static GSList * 
g_hash_table_key_slist (GHashTable *table)
{
  	GSList *rv = NULL;
  	g_hash_table_foreach (table, (GHFunc) prepend_key_pointer, &rv);
  	return rv;
}


static gint
sort_func (gpointer a, gpointer b)
{
	return (GPOINTER_TO_UINT (a) - GPOINTER_TO_UINT (b));
}


gchar ***
tracker_get_hit_counts (SearchQuery *query)
{
	GHashTable *table = g_hash_table_new (NULL, NULL);

	if (tracker->shutdown) {
                return NULL;
	}

	g_return_val_if_fail (query, NULL);
        g_return_val_if_fail (query->words, NULL);

	query->service_array = NULL;
	query->service_array_count = 0;

	query->hits = NULL;

	if (tracker_indexer_get_hits (query)) {
		GSList *tmp;

		for (tmp = query->hits; tmp; tmp=tmp->next) {
			SearchHit *hit = tmp->data;
			guint32 count = GPOINTER_TO_UINT (g_hash_table_lookup (table, GUINT_TO_POINTER (hit->service_type_id))) + 1;

			g_hash_table_insert (table, GUINT_TO_POINTER (hit->service_type_id), GUINT_TO_POINTER (count));


			/* update service's parent count too (if it has a parent) */
			gint parent_id = tracker_get_parent_id_for_service_id (hit->service_type_id);

			if (parent_id != -1) {
				count = GPOINTER_TO_UINT (g_hash_table_lookup (table, GUINT_TO_POINTER (parent_id))) + 1;
	
				g_hash_table_insert (table, GUINT_TO_POINTER (parent_id), GUINT_TO_POINTER (count));
			}
        	}
		tracker_index_free_hit_list (query->hits);
		query->hits = NULL;
	}


	/* search emails */

	query->indexer = tracker->email_index;

	if (tracker_indexer_get_hits (query)) {
		GSList *tmp;
	
		for (tmp = query->hits; tmp; tmp=tmp->next) {
			SearchHit *hit = tmp->data;
			guint32 count = GPOINTER_TO_UINT (g_hash_table_lookup (table, GUINT_TO_POINTER (hit->service_type_id))) + 1;

			g_hash_table_insert (table, GUINT_TO_POINTER (hit->service_type_id), GUINT_TO_POINTER (count));

			/* update service's parent count too (if it has a parent) */
			gint parent_id = tracker_get_parent_id_for_service_id (hit->service_type_id);

			if (parent_id != -1) {
				count = GPOINTER_TO_UINT (g_hash_table_lookup (table, GUINT_TO_POINTER (parent_id))) + 1;

				g_hash_table_insert (table, GUINT_TO_POINTER (parent_id), GUINT_TO_POINTER (count));
			}
	        }

		tracker_index_free_hit_list (query->hits);
		query->hits = NULL;
	}

	query->indexer = tracker->file_index;

	GSList *list, *lst;

	list = g_hash_table_key_slist (table);

	list = g_slist_sort (list, (GCompareFunc) sort_func);

	gint len, i;
	len = g_slist_length (list);
	
	gchar **res = g_new0 (gchar *, len + 1);

	res[len] = NULL;

	i = 0;

	for (lst = list; i < len && lst; lst = lst->next) {

		if (!lst || !lst->data) {
			tracker_error ("ERROR: in get hit counts");
			res[i] = NULL;
			continue;
		}

		guint32 service = GPOINTER_TO_UINT (lst->data);
		guint32 count = GPOINTER_TO_UINT (g_hash_table_lookup (table, GUINT_TO_POINTER (service)));

		gchar **row = g_new0 (gchar *, 3);
		row[0] = tracker_get_service_by_id ((int) service);
		row[1] = tracker_uint_to_str (count);
		row[2] = NULL;

		res[i] = (gchar *)row;

		i++;
	}

	g_slist_free (list);

	g_hash_table_destroy (table);

	return (gchar ***) res;
}


gint
tracker_get_hit_count (SearchQuery *query)
{
	if (tracker->shutdown) {
                return 0;
	}

	g_return_val_if_fail (query, 0);
        g_return_val_if_fail (query->words, 0);

	if (!tracker_indexer_get_hits (query)) {
		return 0;
	} else {
		return query->hit_count;
	}
}
