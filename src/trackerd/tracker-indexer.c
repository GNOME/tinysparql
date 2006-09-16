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
	int 		amalgamated;      /* amalgamation of metadata_is, service_type_id and score of the word in the document's metadata */
} WordDetails;

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

	word_index = cropen (word_index_name, CR_OWRITER | CR_OCREAT | CR_ONOLCK, INDEXBNUM, 2);

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
	result->search_waiting_mutex = g_mutex_new ();

	crsetalign (word_index , INDEXALIGN);
	//crsetfbpsiz (word_index, OD_INDEXFBP);

	return result;
}


void
tracker_indexer_close (Indexer *indexer)
{
	g_return_if_fail (indexer);

	g_mutex_lock (indexer->word_mutex);

	vlclose (indexer->blob_index);
	crclose (indexer->word_index);

	g_mutex_unlock (indexer->word_mutex);
	g_mutex_free (indexer->word_mutex);
	g_mutex_free (indexer->search_waiting_mutex);

	g_free (indexer);
}


static gboolean
WAITING (Indexer *indexer)
{
	gboolean result;

	g_return_val_if_fail (indexer, FALSE);

	result = g_mutex_trylock (indexer->search_waiting_mutex);

	if (result) {
		g_mutex_unlock (indexer->search_waiting_mutex);
	}

	return !result;
}


static void
LOCK (Indexer *indexer)
{
	g_return_if_fail (indexer);

	g_mutex_lock (indexer->word_mutex);
}


static void
UNLOCK (Indexer *indexer)
{
	g_return_if_fail (indexer);

	g_mutex_unlock (indexer->word_mutex);
}


static void
RELOCK (Indexer *indexer)
{
	g_return_if_fail (indexer);

	/* give priority to other threads waiting */
	g_mutex_lock (indexer->search_waiting_mutex);
	g_mutex_unlock (indexer->search_waiting_mutex);

	LOCK (indexer);
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
tracker_indexer_insert_word (Indexer *indexer, guint32 service_id, guint16 metadata_id, guint8 type_id, guint8 score, const char *word)
{
	int  tsiz;
	char *tmp;

	g_return_val_if_fail ((indexer && word), FALSE);

	tracker_log ("inserting word %s with score %d into ID %d", word, score, id);

	/* give priority to other threads waiting */
	RELOCK (indexer);


	guint8 score = 200,  StypeID = 3;
	guint16 MID = 1024;

	guint32 i;
	unsigned char a[4];
	
	a[0] = score;
	a[1] = (MID >> 8 ) & 0xFF ;
	a[2] = MID & 0xFF ;
	a[3] = StypeID;

	i = (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3] ;




	score = (i >> 24) & 0xFF ;
	MID = (a[1] << 8) | a[2] ;
  	StypeID = i & 0xFF ;


	/* check if existing record is there and append and sort word/score pairs if necessary */
	if ((tmp = crget (indexer->word_index, word, -1, 0, -1, &tsiz)) != NULL) {

		UNLOCK (indexer);

		if (tsiz >= (int) sizeof (WordDetails)) {
			WordDetails *current_pairs, *pairs;
			int	 pnum;

			current_pairs = (WordDetails *) tmp;

			pnum = tsiz / sizeof (WordDetails);

			pairs = g_new (WordDetails, pnum +1);

			if ((pnum > 0) && (score > current_pairs[pnum -1].score)) {
				div_t	 t;
				gboolean unsorted;
				int	 i, j;

				/* need to insert into right place - use a binary sort to do it quickly by starting in the middle */
				t = div (pnum, 2);

				i = t.quot;

				unsorted = TRUE;

				while (unsorted) {

					if (score == current_pairs[i].score) {
						unsorted = FALSE;

					} else if (score > current_pairs[i].score) {

						if ((i == 0) || ((i > 0) && (score < current_pairs[i-1].score))) {
							unsorted = FALSE;
						}

						i--;

					} else {

						if ((i == pnum-1) || ((i < pnum-1) && (score > current_pairs[i+1].score))) {
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
				pairs[i+1].score = score;

				for (j = 0; j < i+1; j++) {
					pairs[j] = current_pairs[j];
				}

			} else {
				pairs[pnum].id = id;
				pairs[pnum].score = score;
			}

			RELOCK (indexer);

			if (!crput (indexer->word_index, word, -1, (char *) pairs, (tsiz + sizeof (WordDetails)), CR_DOVER)) {
				g_free (tmp);
				g_free (pairs);
				UNLOCK (indexer);
				return FALSE;
			}

			g_free (tmp);
			g_free (pairs);
		}

	} else {
		WordDetails pair;

		pair.id = id;
		pair.score =score;

		if (!crput (indexer->word_index, word, -1, (char *) &pair, sizeof (pair), CR_DOVER)) {
			UNLOCK (indexer);
			return FALSE;
		}
	}

	UNLOCK (indexer);

	return TRUE;
}


WordDetails *
tracker_indexer_get_hits (Indexer *indexer, const char *word, int offset, int limit, int *count)
{
	int  tsiz;
	char *tmp;

	g_return_val_if_fail ((indexer && word), NULL);

	if (!g_mutex_trylock (indexer->word_mutex)) {
		g_mutex_lock (indexer->search_waiting_mutex);
		g_mutex_lock (indexer->word_mutex);
		g_mutex_unlock (indexer->search_waiting_mutex);
	}

	if (!(tmp = crget (indexer->word_index, word, -1, (offset * sizeof (WordDetails)), limit, &tsiz))) {
		UNLOCK (indexer);
		return NULL;
	}

	UNLOCK (indexer);

	*count = tsiz / sizeof (WordDetails);

	return (WordDetails *)tmp;
}
