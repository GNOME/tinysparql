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


typedef struct {                         /* type of structure for an element of search result */
	guint32 	id;              /* Service ID number of the document */
	int 		amalgamated;      /* amalgamation of metadata_is, service_type and score of the word in the document's metadata */
} WordDetails;



static inline guint8
get_score (WordDetails *details)
{
	return (details->amalgamated >> 24) & 0xFF;
}

static inline guint8
get_service_type (WordDetails *details)
{
	return details->amalgamated & 0xFF;
}

static inline guint16
get_metadata_type (WordDetails *details)
{
	unsigned char a[2];

	a[0] = (details->amalgamated >> 16) & 0xFF;
	a[1] = (details->amalgamated >> 8) & 0xFF;

	return (a[0] << 8) | (a[1]);

}


static inline guint32
calc_amalgamated (IndexWord *word)
{

	unsigned char a[4];
	guint8 score8, service_type;
	guint16 metadata_id;

	if (word->score > 255) {
		score8 = 255;
	} else {
		score8 = (guint8) word->score;
	}

	service_type = (guint8) word->service_type;
	metadata_id = (guint16) word->metadata_id;

	/* amalgamate and combine score, service_type and metadata_type into a single 32-bit int for compact storage */	

	a[0] = score8;
	a[1] = (metadata_id >> 8 ) & 0xFF ;
	a[2] = metadata_id & 0xFF ;
	a[3] = service_type;
	
	return  (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
	
}




static SearchHit *
word_details_to_search_hit (WordDetails *details)
{
	SearchHit *hit;

	hit = g_slice_new (SearchHit, hit);

	hit->service_id = details->service_id;
	hit->metadata_id = get_metadata_type (details);
	hit->service_type = get_service_type (details);
	hit->score = get_score (details);

	return hit;
}

void
tracker_index_free_search_hit (SearchHit *hit)
{
	g_slice_free (SearchHit, hit);

}


IndexWord *
tracker_indexer_create_index_word (const char *word, guint32 service_id, guint32 metadata_id, guint32 service_type, guint32 score)
{
	IndexWord *word;

	word = g_slice_new (IndexWord, word);

	word->word = g_strdup (word);
	word->service_id = service_id;
	word->metadata_id = metadata_id;
	word->service_type = service_type;
	word->score = score;

	return word;

}


void
tracker_indexer_free_index_word (IndexWord *word)
{
	if (word->word) {
		g_free (word->word);
	}

	g_slice_free (IndexWord, word);

}


Indexer *
tracker_indexer_open (const char *name)
{
	char *base_dir, *word_index_name, *blob_index_name;
	DEPOT *word_index;
	VILLA *blob_index;
	Indexer *result;

	base_dir = g_build_filename (g_get_home_dir(), ".Tracker", "Indexes", NULL);
	word_index_name = g_strconcat (base_dir, "/words/", name, NULL);
	blob_index_name = g_strconcat (base_dir, "/blobs/", name, NULL);

	tracker_log ("Word index is %s and blob index is %s", word_index_name, blob_index_name);

	if (!tracker_file_is_valid (word_index_name)) {
		g_mkdir_with_parents (word_index_name, 00755);
	}

	if (!tracker_file_is_valid (blob_index_name)) {
		g_mkdir_with_parents (blob_index_name, 00755);
	}

	word_index = cropen (word_index_name, CR_OWRITER | CR_OCREAT | CR_ONOLCK, INDEXBNUM, 8);

	if (!word_index) {
		tracker_log ("word index was not closed properly - attempting repair");
		if (crrepair (word_index_name)) {
			word_index = cropen (word_index_name, CR_OWRITER | CR_OCREAT | CR_ONOLCK, INDEXBNUM);
		} else {
			g_assert ("Fatal : indexer is dead");
		}
	}

	blob_index = vlopen (blob_index_name, VL_OWRITER  | VL_OCREAT | VL_ONOLCK, VL_CMPINT);

	g_free (base_dir);
	g_free (word_index_name);
	g_free (blob_index_name);

	result = g_new (Indexer, 1);

	result->word_index = word_index;
	result->blob_index = blob_index;

	result->word_mutex = g_mutex_new ();
	result->blob_mutex = g_mutex_new ();

	crsetalign (word_index , INDEXALIGN);

	/* re optimize database if bucket count < (2 * rec count) */
	int buckets, records;
	
	buckets = crbnum (result->word_index);
	records = crrnum (result->word_index);

	if (buckets < (2 * records)) {
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
	crclose (indexer->word_index);

	g_mutex_lock (indexer->blob_mutex);
	vlclose (indexer->blob_index);
		
	g_mutex_free (indexer->word_mutex);
	g_mutex_free (indexer->search_waiting_mutex);

	g_free (indexer);
}

gboolean
tracker_indexer_optimize (Indexer *indexer)
{
	if (!croptimize (indexer->word_index, INDEXBNUM)) {
		return FALSE;
	}

	return TRUE;
}


/* indexing api */
gboolean
tracker_indexer_add_word (Indexer *indexer, IndexWord *word)
{
	int  tsiz;
	char *tmp;

	g_return_val_if_fail ((indexer && word && word->word), FALSE);

	tracker_log ("inserting word %s with score %d into Service ID %d and Metadata ID %d and service type %d",
		 	word->word, 
			word->score, 
			word->service_id, 	
			word->metadata_id,
			word->service_type);

	g_mutex_lock (indexer->word_mutex);

	/* check if existing record is there and append and sort word/score pairs if necessary */
	if ((tmp = crget (indexer->word_index, word->word, -1, 0, -1, &tsiz)) != NULL) {

		g_mutex_unlock (indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *current_pairs, *pairs;
			int	 pnum;

			current_pairs = (WordDetails *) tmp;

			/* ensure we alocate for existing size + 1 to make room for the stuff we are inserting */
			pnum = tsiz / sizeof (WordDetails);
			pairs = g_new (WordDetails, pnum +1);

			if ((pnum > 0) && (word->score > get_score (current_pairs[pnum -1]))) {
				div_t	 t;
				gboolean unsorted;
				int	 i, j;

				/* need to insert into right place - use a binary sort to do it quickly by starting in the middle */
				t = div (pnum, 2);

				i = t.quot;

				unsorted = TRUE;

				while (unsorted) {

					if (word->score == get_score (current_pairs[i])) {
						unsorted = FALSE;

					} else if (word->score > get_score (current_pairs[i])) {

						if ((i == 0) || ((i > 0) && (word->score < get_score (current_pairs[i-1])))) {
							unsorted = FALSE;
						}

						i--;

					} else {

						if ((i == pnum-1) || ((i < pnum-1) && (word->score > get_score (current_pairs[i+1])))) {
							unsorted = FALSE;
						} else {
							i++;
						}
					}
				}

				for (j = pnum-1; j > i; j--) {
					pairs[j+1] = current_pairs[j];
				}

				pairs[i+1].id = id;
				pairs[i+1].almagamated = calc_amalgamated (word);

				for (j = 0; j < i+1; j++) {
					pairs[j] = current_pairs[j];
				}

			} else {
				pairs[pnum].id = id;
				pairs[pnum].almagamated = calc_amalgamated (word);
			}

			g_mutex_lock (indexer->word_mutex);

			if (!crput (indexer->word_index, word->word, -1, (char *) pairs, (tsiz + sizeof (WordDetails)), CR_DOVER)) {
				g_mutex_unlock (indexer->word_mutex);
				g_free (tmp);
				g_free (pairs);
				return FALSE;
			}
			g_mutex_unlock (indexer->word_mutex);
			g_free (tmp);
			g_free (pairs);
		}

	} else {
		WordDetails pair;

		pair.id = id;
		pair.almagamated = calc_amalgamated (word);

		if (!crput (indexer->word_index, word->word, -1, (char *) &pair, sizeof (pair), CR_DOVER)) {
			g_mutex_unlock (indexer->word_mutex);
			return FALSE;
		}
		g_mutex_unlock (indexer->word_mutex);
		
	}

	

	return TRUE;
}


gboolean
tracker_indexer_update_word_score (Indexer *indexer, IndexWord *word, gboolean replace_score)
{
	int  tsiz;
	char *tmp;

	g_return_val_if_fail ((indexer && word && word->word), FALSE);

	g_mutex_lock (indexer->word_mutex);

	/* check if existing record is there  */
	if ((tmp = crget (indexer->word_index, word->word, -1, 0, -1, &tsiz)) != NULL) {

		g_mutex_unlock (indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *details;
			int i, pnum;

			details = (WordDetails *) tmp;
			pnum = tsiz / sizeof (WordDetails);

			for (i=0; i<pnum; i++) {

				if (details[i].id == word->service_id) {
					
					if (get_metadata_type (details[i]) == word->metadata_id) {
						
						if (!replace_score) {
							word->score += get_score (details[i]);
						}
						
						details[i].amalgamated = calc_amalgamated (word);

						g_mutex_lock (indexer->word_mutex);
						crput (indexer->word_index, word->word, -1, (char *) details, tsiz, CR_DOVER);
						g_mutex_unlock (indexer->word_mutex);	
					
						g_free (tmp);

						return TRUE;
					}
				}
			}
		}
 
	}  else {
		g_mutex_unlock (indexer->word_mutex);
	}

	g_free (tmp);

	return FALSE;
}

gboolean
tracker_indexer_remove_word (Indexer *indexer, IndexWord *word, gboolean remove_all)
{
	int  tsiz;
	char *tmp;

	g_return_val_if_fail ((indexer && word && word->word), FALSE);

	g_mutex_lock (indexer->word_mutex);

	/* check if existing record is there  */
	if ((tmp = crget (indexer->word_index, word->word, -1, 0, -1, &tsiz)) != NULL) {

		g_mutex_unlock (indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *details;
			int wi, i, pnum;
			gboolean delete_this;

			details = (WordDetails *) tmp;
			pnum = tsiz / sizeof (WordDetails);

			wi =0;
	
			for (i=0; i<pnum; i++) {

				delete_this = FALSE;
			
				if (details[i].id == word->service_id) {	

					if (remove_all) {
						delete_this = TRUE;
					} else {
						delete_this = (get_metadata_type (details[i]) == word->metadata_id);
					}

				} 
				
				if (G_LIKELY (!delete_this)) {
					details[wi] = details[i];
					wi++;
				} 
			}
			
			if (wi > 0) {
	
				g_mutex_lock (indexer->word_mutex);
				crput (indexer->word_index, word->word, -1, (char *) details, wi * sizeof (WordDetails), CR_DOVER);
				g_mutex_unlock (indexer->word_mutex);	

			} else {
				g_mutex_lock (indexer->word_mutex);
				crout (indexer->word_index, word->word, -1);
				g_mutex_unlock (indexer->word_mutex);	
			}
		}				
		g_free (tmp);

		return TRUE;
		

	} else {
		g_mutex_unlock (indexer->word_mutex);
	}

	g_free (tmp);

	return FALSE;
}

gboolean
tracker_indexer_remove_word_by_meta_ids (Indexer *indexer, IndexWord *word, int *meta_ids)
{
	int  tsiz;
	char *tmp;

	g_return_val_if_fail ((indexer && word && word->word), FALSE);

	g_mutex_lock (indexer->word_mutex);

	/* check if existing record is there  */
	if ((tmp = crget (indexer->word_index, word->word, -1, 0, -1, &tsiz)) != NULL) {

		g_mutex_unlock (indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *details;
			int wi, i, pnum, j;
			gboolean delete_this;

			details = (WordDetails *) tmp;
	
			pnum = tsiz / sizeof (WordDetails);

			wi =0;
				
			for (i=0; i<pnum; i++) {

				delete_this = FALSE;
			
				if (details[i].id == word->service_id) {	

					j = 0;
					while (*meta_ids[j] != -1) {
						if (get_metadata_type (details[i]) == *meta_ids[j]) {
							delete_this = TRUE;	
							break;
						} else {
							j++ ;
						}
					}
				} 
				
				if (G_LIKELY (!delete_this)) {
					details[wi] = details[i];
					wi++;
				} 
			}
			
			if (wi > 0) {
	
				g_mutex_lock (indexer->word_mutex);
				crput (indexer->word_index, word->word, -1, (char *) details, wi * sizeof (WordDetails), CR_DOVER);
				g_mutex_unlock (indexer->word_mutex);	

			} else {
				g_mutex_lock (indexer->word_mutex);
				crout (indexer->word_index, word->word, -1);
				g_mutex_unlock (indexer->word_mutex);	
			}
		}				
		g_free (tmp);

		return TRUE;
		

	} else {
		g_mutex_unlock (indexer->word_mutex);
	}

	g_free (tmp);

	return FALSE;
}



static inline int
count_hits_for_word (Indexer *indexer, const char *word)
{
	int  tsiz;

	g_mutex_lock (indexer->word_mutex);	
	tsiz = crcrvsiz (indexer->word_index, word, -1);
	g_mutex_unlock (indexer->word_mutex);	

	return tsiz;
	
}


static GSList *
get_hits_for_word (Indexer *indexer, const char *word, int service_type_min, int service_type_max, int metadata_type, int offset, int limit, int *total_count)
{
	int  tsiz;
	char *tmp;
	GSList *result;
	
	g_mutex_lock (indexer->word_mutex);

	if ((tmp = crget (indexer->word_index, word->word, -1, 0, -1, &tsiz)) != NULL) {

		g_mutex_unlock (indexer->word_mutex);

		if (tsiz >= (int) sizeof (WordDetails)) {

			WordDetails *details;
			int wi, i, pnum, j;
			gboolean delete_this;

			details = (WordDetails *) tmp;
	
			*total_count = tsiz / sizeof (WordDetails);

			wi =0;
				
			for (i=0; i<pnum; i++) {



	} else {

		g_mutex_unlock (indexer->word_mutex);
		return NULL;
	}

}




GSList *
tracker_indexer_get_hits (Indexer *indexer, char **words, int service_type_min, int service_type_max, int metadata_type, int offset, int limit, int *total_count)
{
	int  tsiz;
	char *tmp;
	GSList *result;
	
	

	g_return_val_if_fail ((indexer && words && words[0]), NULL);

	/* do simple case - one search word */

	if (!words[1]) {
				
		
	}

	g_mutex_lock (indexer->word_mutex);

	if (!(tmp = crget (indexer->word_index, word, -1, (offset * sizeof (WordDetails)), limit, &tsiz))) {
		g_mutex_unlock (indexer->word_mutex);
		return NULL;
	}

	g_mutex_unlock (indexer->word_mutex);

	*count = tsiz / sizeof (WordDetails);

	return (WordDetails *)tmp;
}
