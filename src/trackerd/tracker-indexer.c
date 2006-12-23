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

#include "tracker-indexer.h"

extern Tracker *tracker;

#define INDEXFBP        32               /* size of free block pool of inverted index */

typedef struct {                        
	char 	*word;    
	int 	word_hits;
} SearchWord;



static gboolean shutdown;

static inline guint8
get_score (WordDetails *details)
{
	unsigned char a[2];

	a[0] = (details->amalgamated >> 16) & 0xFF;
	a[1] = (details->amalgamated >> 8) & 0xFF;

	return (a[0] << 8) | (a[1]);
	
}

static inline guint8
get_service_type (WordDetails *details)
{
	return (details->amalgamated >> 24) & 0xFF;
}



guint32
tracker_indexer_calc_amalgamated (int service, int score)
{

	unsigned char a[4];
	guint16 score16;
	guint8 service_type;


	if (score > 65535) {
		score16 = 65535;
	} else {
		score16 = (guint16) score;
	}

	service_type = (guint8) service;


	/* amalgamate and combine score and service_type into a single 32-bit int for compact storage */	

	a[0] = service_type;
	a[1] = (score16 >> 8 ) & 0xFF ;
	a[2] = score16 & 0xFF ;
	a[3] = 0;
	
	return  (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
	
}


static inline SearchHit *
word_details_to_search_hit (WordDetails *details)
{
	SearchHit *hit;

	hit = g_slice_new (SearchHit);

	hit->service_id = details->id;
	hit->score = get_score (details);

	return hit;
}

static inline SearchHit *
copy_search_hit (SearchHit *src)
{
	SearchHit *hit;

	hit = g_slice_new (SearchHit);

	hit->service_id = src->service_id;
	hit->score = src->score;

	return hit;
}


static int 
compare_words (const void *a, const void *b) {

	WordDetails *ap, *bp;

	ap = (WordDetails *)a;
	bp = (WordDetails *)b;
  
	return (get_score(bp) - get_score(ap));
}

static int 
compare_search_hits (const void *a, const void *b) {

	SearchHit *ap, *bp;

	ap = (SearchHit *)a;
	bp = (SearchHit *)b;
  
	return (bp->score - ap->score);
}


static int 
compare_search_words (const void *a, const void *b) {

	SearchWord *ap, *bp;

	ap = (SearchWord *)a;
	bp = (SearchWord *)b;

	return (bp->word_hits - ap->word_hits);
	
}

void		
tracker_index_free_hit_list (GSList *hit_list)
{

	GSList *l;
	SearchHit *hit;

	for (l=hit_list; l; l=l->next) {
		hit = l->data;		
		g_slice_free (SearchHit, hit);
	}  

	g_slist_free (hit_list);

}


static int
get_preferred_bucket_count (Indexer *indexer)
{
	int result;

	if (tracker->index_bucket_ratio < 1) {

		result = (crrnum (indexer->word_index)/2);

	} else if (tracker->index_bucket_ratio > 3) {

		result = (crrnum (indexer->word_index) * 4);

	} else {
		result = (tracker->index_bucket_ratio * crrnum (indexer->word_index));
	}

	tracker_log ("preferred bucket count is %d", result);
	return  result;

}



Indexer *
tracker_indexer_open (const char *name)
{
	char *base_dir, *word_dir;
	CURIA *word_index;
	Indexer *result;

	shutdown = FALSE;

	base_dir = g_build_filename (g_get_home_dir(), ".Tracker", "databases",  NULL);
	word_dir = g_build_filename (base_dir, name, NULL);

	tracker_log ("Opening index %s", word_dir);

	if (!tracker_file_is_valid (base_dir)) {
		g_mkdir_with_parents (base_dir, 00755);
	}

	word_index = cropen (word_dir, CR_OWRITER | CR_OCREAT | CR_ONOLCK, tracker->min_index_bucket_count, tracker->index_divisions);

	if (!word_index) {
		tracker_log ("%s index was not closed properly - attempting repair", word_dir);
		if (crrepair (word_dir)) {
			word_index = cropen (word_dir, CR_OWRITER | CR_OCREAT | CR_ONOLCK, tracker->min_index_bucket_count, tracker->index_divisions);
		} else {
			g_assert ("Fatal : index file is dead (suggest delete index file and restart trackerd)");
		}
	}


	g_free (base_dir);
	g_free (word_dir);

	result = g_new0 (Indexer, 1);

	result->word_index = word_index;

	result->word_mutex = g_mutex_new ();

	crsetalign (word_index , -2);

	/* re optimize database if bucket count < rec count */

	int bucket_count, rec_count;

	bucket_count = crbnum (result->word_index);
	rec_count = crrnum (result->word_index);

	tracker_log ("Bucket count (max is %d) is %d and Record Count is %d", tracker->max_index_bucket_count, bucket_count, rec_count);

	if ((bucket_count < get_preferred_bucket_count (result)) && (bucket_count < tracker->max_index_bucket_count)) {

		if (bucket_count < ((rec_count)/2)) {
			tracker_log ("Optimizing word index - this may take a while...");
			tracker_indexer_optimize (result);
		}
	}

	return result;
}

void
tracker_indexer_close (Indexer *indexer)
{
	g_return_if_fail (indexer);

	shutdown = TRUE;

	g_mutex_lock (indexer->word_mutex);
	crclose (indexer->word_index);

	g_mutex_unlock (indexer->word_mutex);
	g_mutex_free (indexer->word_mutex);

	g_free (indexer);
}


void
tracker_indexer_sync (Indexer *indexer)
{
	if (shutdown) return;

	g_mutex_lock (indexer->word_mutex);
	crsync (indexer->word_index);
	g_mutex_unlock (indexer->word_mutex);
}

gboolean
tracker_indexer_optimize (Indexer *indexer)
{

	if (shutdown) return FALSE;

	int num, b_count;

	/* set bucket count to bucket_ratio times no. of recs divided by no. of divisions */
	num =  (get_preferred_bucket_count (indexer));

	if (num > tracker->max_index_bucket_count) {
		num = tracker->max_index_bucket_count;
	}

	if (num < tracker->min_index_bucket_count) {
		num = tracker->min_index_bucket_count;
	}

	b_count = (num / tracker->index_divisions);
	tracker_log ("no of buckets per division is %d", b_count);

	tracker_log ("Please wait while optimization of indexes takes place...");
	tracker_log ("Index has file size %10.0f and bucket count of %d of which %d are used...", crfsizd (indexer->word_index), crbnum (indexer->word_index), crbusenum (indexer->word_index));
	
	g_mutex_lock (indexer->word_mutex);
	if (!croptimize (indexer->word_index, b_count)) {

		g_mutex_unlock (indexer->word_mutex);
		tracker_log ("Optimization has failed!");
		return FALSE;
	}
	g_mutex_unlock (indexer->word_mutex);
	tracker_log ("Index has been successfully optimized to file size %10.0f and with bucket count of %d of which %d are used...", crfsizd (indexer->word_index), crbnum (indexer->word_index), crbusenum (indexer->word_index));
	
	
	return TRUE;
}


/* indexing api */

/* use for fast insertion of a word for multiple documents at a time */

gboolean
tracker_indexer_append_word_chunk (Indexer *indexer, const char *word, WordDetails *details, int word_detail_count)
{
	if (shutdown) return FALSE;

	g_return_val_if_fail ((indexer && word && details && (word_detail_count > 0)), FALSE);

	if (word_detail_count == 1) {
		return tracker_indexer_append_word (indexer, word, details[0].id, get_service_type (&details[0]), get_score (&details[0]));
	} 

	g_mutex_lock (indexer->word_mutex);
	if (!crput (indexer->word_index, word, -1, (char *) details, (word_detail_count * sizeof (WordDetails)), CR_DCAT)) {
		g_mutex_unlock (indexer->word_mutex);
		return FALSE;
	}
	g_mutex_unlock (indexer->word_mutex);

	return TRUE;	
	
}


/* append individual word for a document */

gboolean
tracker_indexer_append_word (Indexer *indexer, const char *word, guint32 id, int service, int score)
{

	if (shutdown) return FALSE;

	g_return_val_if_fail ((indexer && word), FALSE);

	WordDetails pair;

	pair.id = id;
	pair.amalgamated = tracker_indexer_calc_amalgamated (service, score);

	g_mutex_lock (indexer->word_mutex);
	if (!crput (indexer->word_index, word, -1, (char *) &pair, sizeof (WordDetails), CR_DCAT)) {
		g_mutex_unlock (indexer->word_mutex);
		return FALSE;
	}
	g_mutex_unlock (indexer->word_mutex);

	return TRUE;
}






/* use for deletes or updates when doc is not new */
gboolean
tracker_indexer_update_word (Indexer *indexer, const char *word, guint32 id, int service, int score, gboolean remove_word)
{
	int  tsiz;
	char *tmp;

	if (shutdown) return FALSE;

	g_return_val_if_fail ((indexer && word), FALSE);

	g_mutex_lock (indexer->word_mutex);

	/* check if existing record is there  */
	if ((tmp = crget (indexer->word_index, word, -1, 0, -1, &tsiz)) != NULL) {

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *details;
			int wi, i, pnum;

			details = (WordDetails *) tmp;
			pnum = tsiz / sizeof (WordDetails);
			wi = 0;	
		
			for (i=0; i<pnum; i++) {

				if (details[i].id == id) {
									
					/* NB the paramter score can be negative */
					score += get_score (&details[i]);
									
					if (score < 1 || remove_word) {
						
						int k;

						/* shift all subsequent records in array down one place */
						for (k=i+1; k<pnum; k++) {
							details[k-1] = details[k];
						}

						/* make size of array one size smaller */
						tsiz -= sizeof (WordDetails); 

					} else {
						details[i].amalgamated = tracker_indexer_calc_amalgamated (service, score);
					}

					crput (indexer->word_index, word, -1, (char *) details, tsiz, CR_DOVER);
					g_mutex_unlock (indexer->word_mutex);	
										
					g_free (tmp);

					return TRUE;
				
				}
			}
		} 

		g_free (tmp);
 
	} 
	
	g_mutex_unlock (indexer->word_mutex);
	
	return (tracker_indexer_append_word (indexer, word, id, service, score));

}




static inline int
count_hit_size_for_word (Indexer *indexer, const char *word)
{
	int  tsiz;

	g_mutex_lock (indexer->word_mutex);	
	tsiz = crvsiz (indexer->word_index, word, -1);
	g_mutex_unlock (indexer->word_mutex);	

	return tsiz;
	
}


static GSList *
get_hits_for_single_word (Indexer *indexer, 
		   	  const char *word, 
			  int service_type_min, 
		   	  int service_type_max, 
		   	  int offset,
		   	  int limit,
		   	  gboolean get_count,
		   	  int *hit_count)

{
	int  tsiz, total_count;
	char *tmp;
	GSList *result;
	gboolean single_search_complete;
	
	if (shutdown) return NULL;

	tracker_log ("searching for %s with smin %d and smax %d, offset %d and limit %d", word, service_type_min, service_type_max, offset, limit);

	single_search_complete = FALSE;

	result = NULL;

	total_count = 0;

	g_mutex_lock (indexer->word_mutex);

	if ((tmp = crget (indexer->word_index, word, -1, 0, -1, &tsiz)) != NULL) {

		g_mutex_unlock (indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {
			WordDetails *details;
			int count, pnum;

			details = (WordDetails *) tmp;
			
			pnum = tsiz / sizeof (WordDetails);

			g_debug ("total hit count (excluding service divisions) is %d", pnum);

			if (offset >= pnum) {
				*hit_count = pnum;
				g_free (tmp);
				return NULL;
			} else {
				total_count = offset;
			}

			qsort (details, pnum, sizeof (WordDetails), compare_words);
				
			/* if we want the full count then we have to loop through all results */
			if (!get_count && ((limit + offset) < pnum)) { 
				count = limit + offset;
			} else {
				count = pnum;
			}

			int i;
			for (i=0; i<pnum; i++) {
				int service;

				service = get_service_type (&details[i]);

				if ((service >= service_type_min) && (service <= service_type_max)) {

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

			g_debug ("total hit count for service is %d", total_count);

		}

	} else {
		g_mutex_unlock (indexer->word_mutex);
	}

	*hit_count = total_count;

	g_free (tmp);

	return g_slist_reverse (result);


}




static GHashTable *
get_intermediate_hits (Indexer *indexer, 
	   	      	GHashTable *match_table, 
	   		const char *word, 
	   		int service_type_min, 
	   		int service_type_max)

{
	int  tsiz;
	char *tmp;
	GHashTable *result;

	if (shutdown) return NULL;

	result = g_hash_table_new (NULL, NULL);

	g_mutex_lock (indexer->word_mutex);

	if ((tmp = crget (indexer->word_index, word, -1, 0, -1, &tsiz)) != NULL) {

		g_mutex_unlock (indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *details;
			int count;

			details = (WordDetails *) tmp;
			
			count = tsiz / sizeof (WordDetails);
				
			int i;
			for (i=0; i<count; i++) {

				int service;
				service = get_service_type (&details[i]);

				if ((service >= service_type_min) && (service <= service_type_max)) {

					if (match_table) {
						gpointer pscore;
						int score;
					
						/* we only add hits if in existing match_list where we add to exisitng hit score */
						pscore = g_hash_table_lookup (match_table, GUINT_TO_POINTER (details[i].id));

						if (!pscore) {
							continue;
						}

						score = GPOINTER_TO_INT (pscore) + get_score (&details[i]);

						g_hash_table_insert (result, GUINT_TO_POINTER (details[i].id), GINT_TO_POINTER (score));   	

					} else {
						SearchHit *hit;

						hit = word_details_to_search_hit (&details[i]);
			
						g_hash_table_insert (result, GUINT_TO_POINTER (details[i].id), GINT_TO_POINTER ((int) get_score (&details[i]))); 						
						
					}
				}
			}
		}

		g_free (tmp);

	} else {
		g_mutex_unlock (indexer->word_mutex);
	}

	if (match_table) {
		g_hash_table_destroy (match_table);
	}

	return result;
}


static GSList *
get_final_hits (Indexer *indexer, 
	        GHashTable *match_table, 
	   	const char *word, 
	   	int service_type_min, 
	   	int service_type_max,
		int offset,
		int limit,
		int *hit_count)

{
	int  tsiz, rnum;
	char *tmp;
	SearchHit *result;
	GSList *list;

	rnum = 0;
	*hit_count = 0;
	list = NULL;

	if (shutdown) return NULL;

	if (!match_table || g_hash_table_size (match_table) < 1) {
		return NULL;
	}

	g_mutex_lock (indexer->word_mutex);

	if ((tmp = crget (indexer->word_index, word, -1, 0, -1, &tsiz)) != NULL) {

		g_mutex_unlock (indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *details;
			int size, count;

			details = (WordDetails *) tmp;
			
			count = tsiz / sizeof (WordDetails);
	
			size = g_hash_table_size (match_table);

			result = g_malloc0 ((sizeof (SearchHit) * size) +1);
				
			int i;
			for (i=0; i<count; i++) {

				int service;
				service = get_service_type (&details[i]);

				if ((service >= service_type_min) && (service <= service_type_max)) {

					gpointer pscore;
					int score;
				
					/* we only add hits if in existing match_list where we add to exisitng hit score */
					pscore = g_hash_table_lookup (match_table, GUINT_TO_POINTER (details[i].id));

					if (!pscore) {
						continue;
					}

					score = GPOINTER_TO_INT (pscore) + get_score (&details[i]);

					result[rnum].service_id = details[i].id;
    					result[rnum].score = score;
				    
					rnum++;
				}
			}

			qsort (result, rnum, sizeof (SearchHit), compare_search_hits);

			if (offset >= rnum) {
				tracker_log ("WARNING: offset is too big - no results will be returned for search!");
				g_free (tmp);
				g_free (result);
				return NULL;
			}

			*hit_count = rnum;

			if ((limit + offset) < rnum) { 
				count = limit + offset;
			} else {
				count = rnum;
			}

			for (i=offset; i<count; i++) {
				list = g_slist_prepend (list, copy_search_hit (&result[i]));
			}	

			g_free (result);

		}

		g_free (tmp);

	} else {
		g_mutex_unlock (indexer->word_mutex);
	}

	return g_slist_reverse (list);
}





GSList *
tracker_indexer_get_hits (Indexer *indexer, char **words, int service_type_min, int service_type_max, int offset, int limit, gboolean get_count, int *total_count)
{
	int hit_count, word_count, i;
	GHashTable *table;
	GSList *result;
	SearchWord search_word[6];

	
	if (shutdown) return NULL;
	
	*total_count = 0;

	g_return_val_if_fail ((indexer && (limit > 0)), NULL);

	if (!words || !words[0]) {
		return NULL;
	}

	/* do simple case of only one search word fast */
	if (!words[1]) {

		result = get_hits_for_single_word (indexer,
						   words[0], 
						   service_type_min,
						   service_type_max,
						   offset, 
						   limit,
						   get_count,
						   &hit_count);

		*total_count = hit_count;	
		return result;
	}

	/* limit multiple word searches to 6 words max */
	word_count = 6;

	for (i=0; i<6; i++) {
		if (!words[i]) {
			word_count = i;
			break;
		}
		search_word[i].word = words[i];
		search_word[i].word_hits = count_hit_size_for_word (indexer, words[i]);

		if (search_word[i].word_hits < 8) {
			return NULL;
		}
	}

	/* do multiple word searches - start with words with fewest hits first */
	
	qsort (search_word, word_count, sizeof (SearchWord), compare_search_words);	

	result = NULL;
	table = NULL;

	/* qsort stores results in desc order so we must go from end value to start value to start from word with fewest hits*/
	for (i=word_count-1; i>-1; i--) {

		if (i != 0) {
			table = get_intermediate_hits (indexer, table, search_word[i].word, service_type_min, service_type_max);

			if (g_hash_table_size (table) == 0) {
				*total_count = 0;
				return NULL;
			}

		} else {
			result = get_final_hits (indexer, table, search_word[i].word, service_type_min, service_type_max, offset, limit, &hit_count);
			if (table) {
				g_hash_table_destroy (table);
			}
			*total_count = hit_count;	
			return result;
		}

	}

	return NULL;

}
