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

#include "tracker-indexer.h"
#include "tracker-utils.h"


#define INDEXBNUM       262144            /* initial bucket number of word index */
#define INDEXALIGN      -2                /* alignment of inverted index */
#define INDEXFBP        32                /* size of free block pool of inverted index */
#define COMPACT_AT_SIZE 1750000000	  /* size above which we try to automatically compact */


typedef struct {                         /* type of structure for an element of search result */
	guint32 	id;              /* Service ID number of the document */
	int 		amalgamated;     /* amalgamation of service_type and score of the word in the document's metadata */
} WordDetails;


typedef struct {                        
	char 	*word;    
	int 	word_hits;
} SearchWord;



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



static inline guint32
calc_amalgamated (int service, int score)
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



Indexer *
tracker_indexer_open (const char *name)
{
	char *base_dir, *word_dir;
	DEPOT *word_index;
	Indexer *result;

	base_dir = g_build_filename (g_get_home_dir(), ".Tracker", "Indexes",  NULL);
	word_dir = g_build_filename (base_dir, name, NULL);

	tracker_log ("Word index is %s", word_dir);

	if (!tracker_file_is_valid (base_dir)) {
		g_mkdir_with_parents (base_dir, 00755);
	}

	word_index = dpopen (word_dir, DP_OWRITER | DP_OCREAT | DP_ONOLCK, INDEXBNUM);

	if (!word_index) {
		tracker_log ("%s index was not closed properly - attempting repair", word_dir);
		if (dprepair (word_dir)) {
			word_index = dpopen (word_dir, DP_OWRITER | DP_OCREAT | DP_ONOLCK, INDEXBNUM);
		} else {
			g_assert ("Fatal : index file is dead (suggest delete index file and restart trackerd)");
		}
	}


	g_free (base_dir);
	g_free (word_dir);

	result = g_new (Indexer, 1);

	result->word_index = word_index;

	result->word_mutex = g_mutex_new ();

	dpsetalign (word_index , INDEXALIGN);

	/* re optimize database if bucket count < (2 * rec count) */
	int buckets, records;
	
	buckets = dpbnum (result->word_index);
	records = dprnum (result->word_index);

	if ((buckets < (2 * records)) || (dpfsiz (result->word_index) > COMPACT_AT_SIZE))  {
		tracker_log ("Optimizing word index - this may take a while...");
		tracker_indexer_optimize (result);
	}

	return result;
}

void
tracker_indexer_close (Indexer *indexer)
{
	g_return_if_fail (indexer);

	g_mutex_lock (indexer->word_mutex);
	dpclose (indexer->word_index);
	g_mutex_unlock (indexer->word_mutex);
	g_mutex_free (indexer->word_mutex);

	g_free (indexer);
}

gboolean
tracker_indexer_optimize (Indexer *indexer)
{

	g_mutex_lock (indexer->word_mutex);
	if (!dpoptimize (indexer->word_index, INDEXBNUM)) {
		g_mutex_unlock (indexer->word_mutex);
		return FALSE;
	}
	g_mutex_unlock (indexer->word_mutex);
	return TRUE;
}


/* indexing api */

/* use for fast word insertion when doc is new */
gboolean
tracker_indexer_append_word (Indexer *indexer, const char *word, guint32 id, int service, int score)
{

	g_return_val_if_fail ((indexer && word), FALSE);

	/* tracker_log ("inserting word %s with score %d into Service ID %d and service type %d",
		 	word, 
			score, 
			id, 	
			service);
	*/
		
	WordDetails pair;

	pair.id = id;
	pair.amalgamated = calc_amalgamated (service, score);

	g_mutex_lock (indexer->word_mutex);
	if (!dpput (indexer->word_index, word, -1, (char *) &pair, sizeof (pair), DP_DCAT)) {
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
	
	g_return_val_if_fail ((indexer && word), FALSE);

	g_mutex_lock (indexer->word_mutex);

	/* check if existing record is there  */
	if ((tmp = dpget (indexer->word_index, word, -1, 0, -1, &tsiz)) != NULL) {

		g_mutex_unlock (indexer->word_mutex);

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
						GString *result;

						result = g_string_new ("");

						if (i > 0) {
							result = g_string_append_len (result, tmp,  sizeof (WordDetails) * i);			
						}

						char *s;

						s = (char *) (tmp + (sizeof (WordDetails) * (i+1)));

						g_free (tmp);

						result = g_string_append (result, s);

						tmp = g_string_free (result, FALSE);

						details = (WordDetails *) tmp;
						
					} else {
						details[i].amalgamated = calc_amalgamated (service, score);
					}

					g_mutex_lock (indexer->word_mutex);
					dpput (indexer->word_index, word, -1, (char *) details, tsiz, DP_DOVER);
					g_mutex_unlock (indexer->word_mutex);	
										
					g_free (tmp);

					return TRUE;
				
				}
			}
		}
 
	}  else {
		g_mutex_unlock (indexer->word_mutex);
	}

	g_free (tmp);

	return (tracker_indexer_append_word (indexer, word, id, service, score));

}




static inline int
count_hit_size_for_word (Indexer *indexer, const char *word)
{
	int  tsiz;

	g_mutex_lock (indexer->word_mutex);	
	tsiz = dpvsiz (indexer->word_index, word, -1);
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
	
	single_search_complete = FALSE;

	result = NULL;

	total_count = 0;

	g_mutex_lock (indexer->word_mutex);

	if ((tmp = dpget (indexer->word_index, word, -1, 0, -1, &tsiz)) != NULL) {

		g_mutex_unlock (indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {
			WordDetails *details;
			int count, pnum;

			details = (WordDetails *) tmp;
			
			pnum = tsiz / sizeof (WordDetails);

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
			for (i=offset; i<count; i++) {
				int service;

				service = get_service_type (&details[i]);

				if ((service >= service_type_min) && (service <= service_type_max)) {
					SearchHit *hit;
					
					total_count++;
					
					/* check if we have reached limit but want to continue to get full count */
					if (single_search_complete) {
						continue;
					}

					hit = word_details_to_search_hit (&details[i]);

					result = g_slist_prepend (result, hit);

					single_search_complete = (total_count >= (limit + offset));		

					if (single_search_complete && !get_count) {					
						break;
					}
				}
			}
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

	result = g_hash_table_new (NULL, NULL);

	g_mutex_lock (indexer->word_mutex);

	if ((tmp = dpget (indexer->word_index, word, -1, 0, -1, &tsiz)) != NULL) {

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

					
						/*GSList *l;
						for (l = match_list; l; l=l->next) {
						
							hit = l->data;
							if (hit->service_id  ==  details[i].id) {
									
								hit->score += get_score (&details[i]);
								
								match_list = g_slist_delete_link (match_list, l);

								result = g_slist_prepend (result, hit);
								
								//tracker_log ("found matching hit for sid %d with new score of %d", hit->service_id, hit->score);
								//tracker_log ("result list is now %d and match list is %d", g_slist_length (result),g_slist_length (match_list));
								break;
							}
						}*/

					} else {
						SearchHit *hit;

						hit = word_details_to_search_hit (&details[i]);
			
						g_hash_table_insert (result, GUINT_TO_POINTER (details[i].id), GINT_TO_POINTER ((int) get_score (&details[i]))); 						
						//result = g_slist_prepend (result, hit);
						
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

	g_mutex_lock (indexer->word_mutex);

	if ((tmp = dpget (indexer->word_index, word, -1, 0, -1, &tsiz)) != NULL) {

		g_mutex_unlock (indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *details;
			int count;

			details = (WordDetails *) tmp;
			
			count = tsiz / sizeof (WordDetails);

			result = g_malloc (sizeof(SearchHit) * g_hash_table_size (match_table) + 1);
				
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

			g_hash_table_destroy (match_table);

			qsort (result, rnum, sizeof (SearchHit), compare_search_hits);

			if (offset >= rnum) {
				tracker_log ("offset is too big - no results will be returned");
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
	
	*total_count = 0;

	g_return_val_if_fail ((indexer && words && words[0] && (limit > 0)), NULL);

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

		tracker_log ("getting hits for %s with hits %ld", search_word[i].word, search_word[i].word_hits);

		if (i != 0) {
			table = get_intermediate_hits (indexer, table, search_word[i].word, service_type_min, service_type_max);

			if (g_hash_table_size (table) == 0) {
				*total_count = 0;
				return NULL;
			}

		} else {
			result = get_final_hits (indexer, table, search_word[i].word, service_type_min, service_type_max, offset, limit, &hit_count);
			*total_count = hit_count;	
			return result;
		}

	}

	return NULL;

}
