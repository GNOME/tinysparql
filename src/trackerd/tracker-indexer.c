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

#define SCORE_MULTIPLIER 10000000

#include <math.h>
#include "tracker-indexer.h"

extern Tracker *tracker;

#define INDEXFBP        32               /* size of free block pool of inverted index */

static gboolean shutdown;


static inline guint16
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
	hit->service_type_id = get_service_type (details);
	hit->score = get_score (details);

	return hit;
}


static inline SearchHit *
copy_search_hit (SearchHit *src)
{
	SearchHit *hit;

	hit = g_slice_new (SearchHit);

	hit->service_id = src->service_id;
	hit->service_type_id = src->service_type_id;
	hit->score = src->score;

	return hit;
}


static int 
compare_words (const void *a, const void *b)
{
	WordDetails *ap, *bp;

	ap = (WordDetails *)a;
	bp = (WordDetails *)b;
  
	return (get_score(bp) - get_score(ap));
}


static int 
compare_search_hits (const void *a, const void *b)
{
	SearchHit *ap, *bp;

	ap = (SearchHit *)a;
	bp = (SearchHit *)b;
  
	return (bp->score - ap->score);
}


static int 
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

	for (lst = hit_list; lst; lst = lst->next) {
                SearchHit *hit;
		hit = lst->data;
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

	tracker_log ("Preferred bucket count is %d", result);

	return  result;
}


Indexer *
tracker_indexer_open (const char *name)
{
	char *word_dir;
	CURIA *word_index;
	Indexer *result;

	shutdown = FALSE;

	word_dir = g_build_filename (tracker->data_dir, name, NULL);

	tracker_log ("Opening index %s", word_dir);

	word_index = cropen (word_dir, CR_OWRITER | CR_OCREAT | CR_ONOLCK, tracker->min_index_bucket_count, tracker->index_divisions);

	if (!word_index) {
		tracker_log ("%s index was not closed properly and caused error %s- attempting repair", word_dir, dperrmsg (dpecode));
		if (crrepair (word_dir)) {
			word_index = cropen (word_dir, CR_OWRITER | CR_OCREAT | CR_ONOLCK, tracker->min_index_bucket_count, tracker->index_divisions);
		} else {
			g_assert ("FATAL: index file is dead (suggest delete index file and restart trackerd)");
		}
	}

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

	if (!crclose (indexer->word_index)) {
		tracker_log ("Index closure has failed due to %s", dperrmsg (dpecode));
	}

	g_mutex_unlock (indexer->word_mutex);
	g_mutex_free (indexer->word_mutex);

	g_free (indexer);
}


void
tracker_indexer_sync (Indexer *indexer)
{
        if (shutdown) {
                return;
        }

	g_mutex_lock (indexer->word_mutex);
	crsync (indexer->word_index);
	g_mutex_unlock (indexer->word_mutex);
}


gboolean
tracker_indexer_optimize (Indexer *indexer)
{
	int num, b_count;

        if (shutdown) {
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
	tracker_log ("Index has file size %10.0f and bucket count of %d of which %d are used...", crfsizd (indexer->word_index), crbnum (indexer->word_index), crbusenum (indexer->word_index));
	
	g_mutex_lock (indexer->word_mutex);

	if (!croptimize (indexer->word_index, b_count)) {

		g_mutex_unlock (indexer->word_mutex);
		tracker_log ("Optimization has failed due to %s", dperrmsg (dpecode));
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
        if (shutdown) {
                return FALSE;
        }

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
        if (shutdown) {
                return FALSE;
        }

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

        if (shutdown) {
                return FALSE;
        }

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
		
			for (i = 0; i < pnum; i++) {

				if (details[i].id == id) {
									
					/* NB the paramter score can be negative */
					score += get_score (&details[i]);
									
					if (score < 1 || remove_word) {
						
						int k;

						/* shift all subsequent records in array down one place */
						for (k = i + 1; k < pnum; k++) {
							details[k - 1] = details[k];
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


/* use to delete dud hits for a word - dud_list is a list of SearchHit structs */
gboolean
tracker_remove_dud_hits (Indexer *indexer, const char *word, GSList *dud_list)
{
	int  tsiz;
	char *tmp;

	if (shutdown) {
                return FALSE;
        }

	g_return_val_if_fail ((indexer && word && dud_list), FALSE);

	g_mutex_lock (indexer->word_mutex);

	/* check if existing record is there  */
	if ((tmp = crget (indexer->word_index, word, -1, 0, -1, &tsiz)) != NULL) {

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

			crput (indexer->word_index, word, -1, (char *) details, tsiz, CR_DOVER);
			
			g_mutex_unlock (indexer->word_mutex);	
	
			g_free (tmp);

			return TRUE;
		}

		g_free (tmp);
	}

	g_mutex_unlock (indexer->word_mutex);

	return FALSE;
}


static int
get_idf_score (WordDetails *details, float idf)
{
	guint32 score = get_score (details);
	float f = idf * score * SCORE_MULTIPLIER;

        return (f > 1.0) ? lrintf (f) : 1;
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


static inline gboolean
in_array (int *array, int count, int value)
{
	int i;

	for (i = 0; i < count; i++) {
		if (array[i] == value) {
			return TRUE;
		}
	}

	return FALSE;
}


SearchQuery *
tracker_create_query (Indexer *indexer, int *service_array, int service_array_count, int offset, int limit)
{
	SearchQuery *result = g_new0 (SearchQuery, 1);

	result->indexer = indexer;
	result->service_array = service_array;
	result->service_array_count = service_array_count;
	result->offset = offset;
	result->limit = limit;	

	return result;
}


void
tracker_add_query_word (SearchQuery *query, const char *word, WordType word_type)
{
	if (!word || word[0] == 0 || (word[0] == ' ' && word[1] == 0)) {
		return;
	}

	SearchWord *result = g_new0 (SearchWord, 1);

	result->word = g_strdup (word);
	result->hit_count = 0;
	result->idf = 0;
	result->word_type = word_type;

	query->words =  g_slist_prepend (query->words, result);
}


static void
free_word (SearchWord *result)
{
	g_free (result->word);
	g_free (result);
}


void
tracker_free_query (SearchQuery *query)
{
	tracker_index_free_hit_list (query->hits);

	/* Do not free individual dud hits - dud SearchHit structs are always part of the hit list so will already be freed when hit list is freed above */
	g_slist_free (query->duds);

	g_slist_foreach (query->words, (GFunc) free_word, NULL);
	g_slist_free (query->words);

	g_free (query);
}


static GSList *
get_hits_for_single_word (SearchQuery *query, SearchWord *search_word, int *hit_count)
{
	int  tsiz, total_count = 0;
	char *tmp = NULL;
	GSList *result = NULL;

	int offset = query->offset;

	/* some results might be dud so get an extra 50 to compensate */
	int limit = query->limit + 50;
	
	if (shutdown) {
                return NULL;
        }

	g_mutex_lock (query->indexer->word_mutex);

	if ((tmp = crget (query->indexer->word_index, search_word->word, -1, 0, -1, &tsiz)) != NULL) {

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

	*hit_count = total_count;

	g_free (tmp);

	return g_slist_reverse (result);
}


static GHashTable *
get_intermediate_hits (SearchQuery *query, GHashTable *match_table, SearchWord *search_word, BoolOp bool_op)
{
	int  tsiz;
	char *tmp;
	GHashTable *result;

	if (shutdown) {
                return NULL;
        }

	result = g_hash_table_new (NULL, NULL);

	g_mutex_lock (query->indexer->word_mutex);

	if ((tmp = crget (query->indexer->word_index, search_word->word, -1, 0, -1, &tsiz)) != NULL) {

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
get_final_hits (SearchQuery *query, GHashTable *match_table, SearchWord *search_word, BoolOp bool_op, int *hit_count)
{
	int  tsiz, rnum;
	char *tmp;
	SearchHit *result;
	GSList *list;

	int offset = query->offset;

	/* some results might be dud so get an extra 50 to compensate */
	int limit = query->limit + 50;

	*hit_count = 0;

	rnum = 0;
	list = NULL;

	if (shutdown) {
                return NULL;
        }

	if (!match_table || g_hash_table_size (match_table) < 1) {
		return NULL;
	}
				
	g_mutex_lock (query->indexer->word_mutex);

	if ((tmp = crget (query->indexer->word_index, search_word->word, -1, 0, -1, &tsiz)) != NULL) {

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

			*hit_count = rnum;

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
	int word_count;
	GHashTable *table;
	SearchWord *word;
	GSList *lst;

	if (shutdown) {
                return FALSE;
	}

	g_return_val_if_fail ((query->indexer && query->words && (query->limit > 0)), FALSE);

	word_count = g_slist_length (query->words);

	if (word_count == 0) {
                return FALSE;
        }

	/* do simple case of only one search word fast */
	if (word_count == 1) {
		word = query->words->data;

		if (!word) {
                        return FALSE;
                }

		query->hits = get_hits_for_single_word (query, word, &query->hit_count);

		return TRUE;
	}

	/* calc stats for each word */
	for (lst = query->words; lst; lst = lst->next) {
		word = lst->data;

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

	query->words = g_slist_sort (query->words, (GCompareFunc)  compare_search_words);
	
	table = NULL;

	for (lst = query->words; lst; lst = lst->next) {
		word = lst->data;

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
prepend_key_pointer (gpointer         key,
		     gpointer         value,
		     gpointer         data)
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


char ***
tracker_get_hit_counts (SearchQuery *query)
{
	GSList *result;

	GHashTable *table = g_hash_table_new (NULL, NULL);

	if (shutdown) {
                return NULL;
	}

	g_return_val_if_fail ((query && query->words), NULL);

	int word_count = g_slist_length (query->words);

	if (word_count == 0) {
                return NULL;
        }

	query->service_array = NULL;
	query->service_array_count = 0;

	if (!tracker_indexer_get_hits (query)) {
		return NULL;
	}

	result = query->hits;

	GSList *tmp;

	for (tmp = result; tmp; tmp=tmp->next) {

		SearchHit *hit = tmp->data;

		guint32 count;
				
		count = GPOINTER_TO_UINT (g_hash_table_lookup (table, GUINT_TO_POINTER (hit->service_type_id))) + 1;

		g_hash_table_insert (table, GUINT_TO_POINTER (hit->service_type_id), GUINT_TO_POINTER (count));


		/* update service's parent count too (if it has a parent) */
		int parent_id =  tracker_get_parent_id_for_service_id (hit->service_type_id);

		if (parent_id != -1) {
			count = GPOINTER_TO_UINT (g_hash_table_lookup (table, GUINT_TO_POINTER (parent_id))) + 1;

			g_hash_table_insert (table, GUINT_TO_POINTER (parent_id), GUINT_TO_POINTER (count));
		}
        }

	GSList *list, *lst;

	list = g_hash_table_key_slist (table);

	list = g_slist_sort (list, (GCompareFunc) sort_func);

	int len, i;
	len = g_slist_length (list);
	
	char **res = g_new0 (char *, len + 1);

	res[len] = NULL;

	i = 0;

	for (lst = list; i < len; lst = lst->next) {

		if (!lst->data) {
			tracker_error ("ERROR: in get hit counts");
			res[i] = NULL;
			continue;
		}

		guint32 service = GPOINTER_TO_UINT (lst->data);
		guint32 count = GPOINTER_TO_UINT (g_hash_table_lookup (table, GUINT_TO_POINTER (service)));

		char **row = g_new0 (char *, 3);
		row[0] = tracker_get_service_by_id ((int) service);
		row[1] = tracker_uint_to_str (count);
		row[2] = NULL;

		res[i] = (char *)row;

		i++;
	}

	g_slist_free (list);

	g_hash_table_destroy (table);

	return (char ***) res;
}
