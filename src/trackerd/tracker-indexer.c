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

	word_index = dpopen (word_index_name, DP_OWRITER | DP_OCREAT | DP_ONOLCK, INDEXBNUM);

	if (!word_index) {
		tracker_log ("word index was not closed properly - attempting repair");
		if (dprepair (word_index_name)) {
			word_index = dpopen (word_index_name, DP_OWRITER | DP_OCREAT | DP_ONOLCK, INDEXBNUM);
		} else {
			g_assert ("indexer is dead");
		}
	}

	blob_index = vlopen (blob_index_name, VL_OWRITER  | VL_OCREAT | VL_ONOLCK, VL_CMPINT);

	g_free (base_dir);
	g_free (word_index_name);
	g_free (blob_index_name);

	result = g_new (Indexer, 1);

	result->word_index = word_index;
	result->blob_index = blob_index;

	result->mutex = g_mutex_new ();
	result->search_waiting_mutex = g_mutex_new ();

	dpsetalign (word_index , INDEXALIGN);
	//dpsetfbpsiz (word_index, OD_INDEXFBP);

	return result;
}


void
tracker_indexer_close (Indexer *indexer)
{
	g_return_if_fail (indexer);

	g_mutex_lock (indexer->mutex);

	vlclose (indexer->blob_index);
	dpclose (indexer->word_index);

	g_mutex_unlock (indexer->mutex);
	g_mutex_free (indexer->mutex);
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

	g_mutex_lock (indexer->mutex);
}


static void
UNLOCK (Indexer *indexer)
{
	g_return_if_fail (indexer);

	g_mutex_unlock (indexer->mutex);
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


/* removes dud hits */
void
tracker_indexer_sweep (Indexer *indexer)
{
	int rnum, tnum;

	g_return_if_fail (indexer && indexer->db_con);

	LOCK (indexer);

	if ((rnum = dprnum (indexer->word_index)) < 1) {
		return;
	}

	if (!dpiterinit (indexer->word_index)) {
		return;
	}

	UNLOCK (indexer);

	tnum = 0;

	while (TRUE){
		SearchHit *pairs;
		char	 *kbuf, *vbuf;
		int	 i, ksiz, vsiz, pnum, wi;

		/* give priority to other threads waiting */
		RELOCK (indexer);

		if (!(kbuf = dpiternext(indexer->word_index, &ksiz))){
			UNLOCK (indexer);
			return;
		}

		if (!(vbuf = dpget (indexer->word_index, kbuf, ksiz, 0, -1, &vsiz))){
			g_free (kbuf);
			UNLOCK (indexer);
			return;
		}

		UNLOCK (indexer);

		pairs = (SearchHit *) vbuf;
		pnum = vsiz / sizeof (SearchHit);

		wi = 0;

		for (i = 0; i < pnum; i++){

			if (tracker_db_index_id_exists (indexer->db_con, pairs[i].id) ) {
				pairs[wi++] = pairs[i];
			}
		}

		/* give priority to other threads waiting */
		RELOCK (indexer);

		if (wi > 0) {
			if (!dpput (indexer->word_index, kbuf, ksiz, vbuf, wi * sizeof (SearchHit), DP_DOVER)){
				g_free (vbuf);
				g_free (kbuf);
				UNLOCK (indexer);
				return;
			}
		} else {
			if (!dpout (indexer->word_index, kbuf, ksiz)){
				g_free (vbuf);
				g_free (kbuf);
				UNLOCK (indexer);
				return;
			}
		}

		UNLOCK (indexer);

		g_free (vbuf);
		g_free (kbuf);
		tnum++;

		/* send thread to sleep for a short while so we are not using too much cpu or hogging this resource */
		g_usleep (1000) ;
	}
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
gboolean
tracker_indexer_insert_word (Indexer *indexer, unsigned int id, const char *word, int score)
{
	int  tsiz;
	char *tmp;

	g_return_val_if_fail ((indexer && word), FALSE);

	tracker_log ("inserting word %s with score %d into ID %d", word, score, id);

	/* give priority to other threads waiting */
	RELOCK (indexer);

	/* check if existing record is there and append and sort word/score pairs if necessary */
	if ((tmp = dpget (indexer->word_index, word, -1, 0, -1, &tsiz)) != NULL) {

		UNLOCK (indexer);

		if (tsiz >= (int) sizeof (SearchHit)) {
			SearchHit *current_pairs, *pairs;
			int	 pnum;

			current_pairs = (SearchHit *) tmp;

			pnum = tsiz / sizeof (SearchHit);

			pairs = g_new (SearchHit, pnum +1);

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

			if (!dpput (indexer->word_index, word, -1, (char *) pairs, (tsiz + sizeof (SearchHit)), DP_DOVER)) {
				g_free (tmp);
				g_free (pairs);
				UNLOCK (indexer);
				return FALSE;
			}

			g_free (tmp);
			g_free (pairs);
		}

	} else {
		SearchHit pair;

		pair.id = id;
		pair.score =score;

		if (!dpput (indexer->word_index, word, -1, (char *) &pair, sizeof (pair), DP_DOVER)) {
			UNLOCK (indexer);
			return FALSE;
		}
	}

	UNLOCK (indexer);

	return TRUE;
}


SearchHit *
tracker_indexer_get_hits (Indexer *indexer, const char *word, int offset, int limit, int *count)
{
	int  tsiz;
	char *tmp;

	g_return_val_if_fail ((indexer && word), NULL);

	if (!g_mutex_trylock (indexer->mutex)) {
		g_mutex_lock (indexer->search_waiting_mutex);
		g_mutex_lock (indexer->mutex);
		g_mutex_unlock (indexer->search_waiting_mutex);
	}

	if (!(tmp = dpget (indexer->word_index, word, -1, (offset * sizeof (SearchHit)), limit, &tsiz))) {
		UNLOCK (indexer);
		return NULL;
	}

	UNLOCK (indexer);

	*count = tsiz / sizeof (SearchHit);

	return (SearchHit *)tmp;
}
