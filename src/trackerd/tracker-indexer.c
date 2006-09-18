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


#define INDEXBNUM       262144            /* initial bucket number of inverted index */
#define INDEXALIGN      -2                /* alignment of inverted index */
#define INDEXFBP        32                /* size of free block pool of inverted index */
#define COMPACT_AT_SIZE 1750000000	  /* size above which we try to automatically compact */


typedef struct {                         /* type of structure for an element of search result */
	guint32 	id;              /* Service ID number of the document */
	int 		amalgamated;      /* amalgamation of service_type and score of the word in the document's metadata */
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


	if (score > 65565) {
		score16 = 65565;
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

	hit = g_slice_new (SearchHit, hit);

	hit->service_id = details->service_id;
	hit->score = get_score (details);

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
	char *base_dir, *word_index_name;
	DEPOT *word_index;
	Indexer *result;

	base_dir = g_build_filename (g_get_home_dir(), ".Tracker", "Indexes", NULL);
	word_index_name = g_strconcat (base_dir, "/words/", name, NULL);

	tracker_log ("Word index is %s", word_index_name);

	if (!tracker_file_is_valid (word_index_name)) {
		g_mkdir_with_parents (word_index_name, 00755);
	}

	word_index = dpopen (word_index_name, DP_OWRITER | DP_OCREAT | DP_ONOLCK, INDEXBNUM);

	if (!word_index) {
		tracker_log ("word index was not closed properly - attempting repair");
		if (dprepair (word_index_name)) {
			word_index = dpopen (word_index_name, DP_OWRITER | DP_OCREAT | DP_ONOLCK, INDEXBNUM);
		} else {
			g_assert ("Fatal : indexer is dead");
		}
	}


	g_free (base_dir);
	g_free (word_index_name);

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
		tracker_indexer_optimize (result->word_index);
	}

	return result;
}

void
tracker_indexer_close (Indexer *indexer)
{
	g_return_if_fail (indexer);

	g_mutex_lock (indexer->word_mutex);
	dpclose (indexer->word_index);

	g_mutex_lock (indexer->blob_mutex);
	vlclose (indexer->blob_index);
		
	g_mutex_free (indexer->word_mutex);
	g_mutex_free (indexer->search_waiting_mutex);

	g_free (indexer);
}

gboolean
tracker_indexer_optimize (Indexer *indexer)
{
	if (!dpoptimize (indexer->word_index, INDEXBNUM)) {
		return FALSE;
	}

	return TRUE;
}


/* indexing api */

/* use for fast word insertion when doc is new */
gboolean
tracker_indexer_append_word (Indexer *indexer, const char *word, guint32 id, int service, int score)
{
	int  tsiz;
	char *tmp;

	g_return_val_if_fail ((indexer && word), FALSE);

	tracker_log ("inserting word %s with score %d into Service ID %d and service type %d",
		 	word, 
			score, 
			service_id, 	
			service_type);

		
	WordDetails pair;

	pair.id = id;
	pair.almagamated = calc_amalgamated (service, score);

	if (!dpput (indexer->word_index, word->word, -1, (char *) &pair, sizeof (pair), DP_DCAT)) {
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

				if (details[i].id == service_id) {
									
					/* NB the paramter score can be negative */
					score += get_score (details[i]);
									
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

	return (tracker_indexer_append_word ());

}




static inline int
count_hits_for_word (Indexer *indexer, const char *word)
{
	int  tsiz;

	g_mutex_lock (indexer->word_mutex);	
	tsiz = dpcrvsiz (indexer->word_index, word, -1);
	g_mutex_unlock (indexer->word_mutex);	

	return tsiz;
	
}


static GSList *
get_hits_for_word (Indexer *indexer, 
		   GSList *match_list, 
		   const char *word, 
		   int service_type_min, 
		   int service_type_max, 
		   int offset,
		   int limit,
		   gboolean get_count;
		   int *hit_count)
{
	int  tsiz;
	char *tmp;
	GSList *result;
	gboolean single_search, single_search_complete;
	
	/* limit will be set > 0 if we only have one word to search */
	single_search = (limit > 0 && !match_list);

	single_search_complete = FALSE;

	result = match_list;

	*hit_count = 0;

	g_mutex_lock (indexer->word_mutex);

	if ((tmp = dpget (indexer->word_index, word->word, -1, 0, -1, &tsiz)) != NULL) {

		g_mutex_unlock (indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *details;
			int count, j;

			details = (WordDetails *) tmp;
			
			count = tsiz / sizeof (WordDetails);

			
			if (single_search) {
				qsort (details, count, sizeof (WordDetails), compare_words);
			}
				

			for (i=0; i<count; i++) {

				int service;
				gboolean hit_added;

				hit_added = FALSE:

				service = get_service_type (details[i]);

				if ((service >= service_type_min) && (service <= service_type_max)) {

					*hit_count++;

					if ((offset > i) || single_search_complete) {
						continue;
					}

					if (match_list) {

						/* add to hit score if service is already in supplied match_list */
						
						GSList *l;
						SearchHit *hit;

						hit_added = FALSE;

						for (l = match_list; l; l=l->next) {
						
							hit = l->data;
							if (hit->service_id  ==  details[i].id) {
								hit->score += get_score (details[i]);
								hit_added = TRUE;
								*hit_count--;
								break;
							}
						}  
					}
					
					if (!hit_added) {
						hit = word_details_to_search_hit (details[i]);
						result = g_slist_prepend (result, hit);
					
					}

					single_search_complete = (single_search && (*hit_count >= (limit + offset)));		

					if (single_search_complete && !get_count) {
						
							g_free (tmp);
							return result;
						}
					}
				}
			
			}

		}

		g_free (tmp);
		return result;
	}


	} else {
		g_mutex_unlock (indexer->word_mutex);
		return result;
	}

}




GSList *
tracker_indexer_get_hits (Indexer *indexer, char **words, int service_type_min, int service_type_max, int offset, int limit, gboolean get_count, int *total_count)
{
	int  tsiz, hit_count, word_count, i;
	char *tmp;
	GSList *result;
	SearchWord *search_word;
	
	*total_count = 0;

	g_return_val_if_fail ((indexer && words && words[0] && (limit > 0)), NULL);

	/* do simple case of only one search word fast */

	if (!words[1]) {
		result = get_hits_for_word (indexer, NULL, words[0], service_type_min, service_type_max, offset, limit, get_count, &hit_count);
		result = g_slist_reverse (result);	
		*total_count = hit_count;		
		return result;
	}

	/* limit multiple word searches to 6 words max */
	search_word = g_new (SearchWord, 6);
	word_count = 6;
	result = NULL;

	for (i=0; i<6; i++) {
		if (!words[i]) {
			word_count = i+1;
			break;
		}
		search_word[i]->word = words[i];
		search_word[i]->word_hits = count_hits_for_word (words[i]);
	}

	/* do multiple word searches - start with words with fewest hits first */

	qsort (search_word, word_count, sizeof (search_word), compare_search_words);	

	
	/* qsort stores results in desc order so we must go from end value to start value */
	for (i=word_count-1; i>-1; i--) {
		result = get_hits_for_word (indexer, result, search_word[i], service_type_min, service_type_max, 0, 0, get_count, &hit_count);
	}

	g_free (search_word);

	*total_count = g_slist_length (result);
	
	/* sort results so highest rank items are first in list */
	result = g_slist_sort (result, compare_search_hits);

	/* apply offset and limit */	
	GSList *l, *res;
	i = 0;
	res = NULL;

	for (l = result; l; l=l->next) {
		if (offset != 0) {
			offset--;
			continue;
		}	
		
		i++;
		res = g_slist_prepend (res, l->data);
		
		if (i >= limit) {
			break;
		}
		
	}

	tracker_index_free_hit_list (result);

	return g_slist_reverse (res);
}
