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

#define CREATE_INDEX "CREATE TABLE HitIndex (Word Text not null unique, HitCount Integer, HitArraySize Integer, HitArray Blob);"
#define MAX_HIT_BUFFER 480000

#include <string.h>
#include <math.h>
#include <sqlite3.h>
#include "tracker-indexer.h"


extern Tracker *tracker;
static gboolean tracker_shutdown;


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

	if (!hit_list) return;

	for (lst = hit_list; lst; lst = lst->next) {
                SearchHit *hit;
		hit = lst->data;
		g_slice_free (SearchHit, hit);
	}

	g_slist_free (hit_list);
}



DBConnection *
tracker_indexer_open (const char *name)
{

	char	     *dbname;
	DBConnection *db_con;

	gboolean create_table = FALSE;

	tracker_shutdown = FALSE;

	dbname = g_build_filename (tracker->data_dir, name, NULL);

	if (!g_file_test (dbname, G_FILE_TEST_IS_REGULAR)) {
		tracker_log ("database index file %s is not present - will create", dbname);
		create_table = TRUE;
	} 


	db_con = g_new (DBConnection, 1);

	if (sqlite3_open (dbname, &db_con->db) != SQLITE_OK) {
		tracker_error ("FATAL ERROR: can't open database at %s: %s", dbname, sqlite3_errmsg (db_con->db));
		exit (1);
	}

	sqlite3_extended_result_codes (db_con->db, 0);

	g_free (dbname);

	db_con->db_type = DB_DATA;

	sqlite3_busy_timeout (db_con->db, 10000);
	
	db_con->cache = NULL;
	db_con->emails = NULL;
	db_con->others = NULL;
	db_con->blob = NULL;


	db_con->statements = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	tracker_db_exec_no_reply (db_con, "PRAGMA count_changes = 0");

	tracker_db_exec_no_reply (db_con, "PRAGMA synchronous = 0");

	if (strcmp (name, "email-index.db") == 0) {
		tracker_db_exec_no_reply (db_con, "PRAGMA page_size = 4096");
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA page_size = 4096");
	}

	if (tracker->use_extra_memory) {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 1024");
	} else {
		tracker_db_exec_no_reply (db_con, "PRAGMA cache_size = 256");
	}

	tracker_db_exec_no_reply (db_con, "PRAGMA encoding = \"UTF-8\"");

	if (create_table) {
		tracker_db_exec_no_reply (db_con, CREATE_INDEX);
	}

	db_con->thread = NULL;

	return db_con;

}



void
tracker_indexer_close (DBConnection *db_con)
{
	
	tracker_db_close (db_con);
}


void
tracker_indexer_sync (DBConnection *db_con)
{
      
                return;
      
}


gboolean
tracker_indexer_optimize (DBConnection *db_con)
{

	return TRUE;
}


static inline int
get_padding (int total_hits, int sz)
{

	if (total_hits < 25) {
		return (1 * sz);
	} 
	else  if (total_hits < 100) {
		return (2 * sz);
	} 
	else  if (total_hits < 500) {
		return (4 * sz);
	} 
	else  if (total_hits < 1000) {
		return (8 * sz);
	} 
	else  if (total_hits < 5000) {
		return (16 * sz);
	} 
	else  if (total_hits < 10000) {
		return (32 * sz);
	} else {
		return (64 * sz);
	}

	return 1; 

}


static gboolean
get_word_details (DBConnection *db_con, const char *word, sqlite_int64 *id, int *hit_count, int *hit_array_size)
{
	gchar ***res;
		
	res = tracker_exec_proc (db_con, "GetHitDetails", 1, word);

	if (!res) {
		return FALSE;
	}

	char **row = tracker_db_get_row (res, 0);

	if (!(row && row[0] && row[1] && row[2])) {
		tracker_db_free_result (res);	
		return FALSE;
	} else {
		*id = atoi (row[0]);
		*hit_count = atoi (row[1]);
		*hit_array_size = atoi (row[2]);
	}

	tracker_db_free_result (res);	

	return TRUE;

}

static sqlite_int64
insert_word_details (DBConnection *db_con, const char *word, int hit_count, int hit_array_size)
{
	gboolean success = FALSE;

	char *hit_count_str = tracker_int_to_str (hit_count);
	char *hit_array_size_str = tracker_int_to_str (hit_array_size);
		
	success = tracker_exec_proc_no_reply (db_con, "InsertHitDetails", 4, word, hit_count_str, hit_array_size_str, hit_array_size_str);

	g_free (hit_count_str);
	g_free (hit_array_size_str);

	if (success) {
		return sqlite3_last_insert_rowid (db_con->db);
	} else {
		return 0;
	}

	
}

static gboolean
update_word_details (DBConnection *db_con, sqlite_int64 id, int hit_count, int hit_array_size)
{
	gboolean success = FALSE;

	char *id_str = tracker_int_to_str (id);
	char *hit_count_str = tracker_int_to_str (hit_count);
	char *hit_array_size_str = tracker_int_to_str (hit_array_size);
		
	success = tracker_exec_proc_no_reply (db_con, "UpdateHitDetails", 3, hit_count_str, hit_array_size_str, id_str);

	g_free (hit_count_str);
	g_free (hit_array_size_str);
	g_free (id_str);

	return success;

}

static gboolean
resize_word_details (DBConnection *db_con, sqlite_int64 id, int hit_count, int hit_array_size)
{
	gboolean success = FALSE;

	char *id_str = tracker_int_to_str (id);
	char *hit_count_str = tracker_int_to_str (hit_count);
	char *hit_array_size_str = tracker_int_to_str (hit_array_size);
		
	success = tracker_exec_proc_no_reply (db_con, "ResizeHitDetails", 4, hit_count_str, hit_array_size_str, hit_array_size_str, id_str);

	g_free (hit_count_str);
	g_free (hit_array_size_str);
	g_free (id_str);

	return success;

	
}

static sqlite3_blob *
get_blob (DBConnection *db_con, sqlite_int64 id, gboolean write_blob)
{

	sqlite3_blob *blob;

	if (sqlite3_blob_open (db_con->db, NULL, "HitIndex", "HitArray", id, write_blob, &blob) != SQLITE_OK) {
		tracker_error ("could not open blob from index");
		return NULL;
	}

	return blob;

}



static inline gboolean
read_blob (sqlite3_blob *blob, void *buffer, int length, int offset)
{
	if (sqlite3_blob_read (blob, buffer, length, offset) != SQLITE_OK) {
		tracker_error ("could not read blob from index");
		return FALSE;
	}

	return TRUE;
}


static inline gboolean
write_blob (DBConnection *db_con, sqlite3_blob *blob, const void *buffer, int length, int offset)
{
	int rc;

	rc = sqlite3_blob_write (blob, buffer, length, offset);

	if (rc != SQLITE_OK) {
		tracker_error ("could not write blob to index due to %s with error code %d", sqlite3_errmsg (db_con->db), rc);
		return FALSE;
	}

	return TRUE;
}

static inline void
close_blob (sqlite3_blob *blob)
{
	sqlite3_blob_close (blob);
}







/* indexing api */

/* use for fast insertion of a word for multiple documents at a time */

gboolean
tracker_indexer_append_word_chunk (DBConnection *db_con, const char *word, WordDetails *details, int word_detail_count)
{
        if (tracker_shutdown) {
                return FALSE;
        }

	g_return_val_if_fail ((db_con && word && details && (word_detail_count > 0)), FALSE);


	sqlite_int64 id=0;
	int hit_count=0, hit_array_size=0, new_count=word_detail_count;
	sqlite3_blob *blob;
	int sz = sizeof (WordDetails);
	

	if (get_word_details (db_con, word, &id, &hit_count, &hit_array_size)) {

		/* do not add further hits if hit count for that word is already huge */
		if (hit_count >= MAX_HITS_FOR_WORD) return FALSE;

		if ((hit_count + new_count) > MAX_HITS_FOR_WORD) {
			new_count = MAX_HITS_FOR_WORD - hit_count;
		}

		int new_space = new_count * sz;
		int used_hit_space = hit_count * sz;
		int free_hit_space = hit_array_size - used_hit_space;
		int total_hits = hit_count + new_count;


		blob = get_blob (db_con, id, TRUE);
		if (!blob) {
			return FALSE;
		}


		/* check to see if there is enough free space in the blob to accommadate without resizing */
		if ((free_hit_space > 0) && (new_space <= free_hit_space)) {

			
			if (!write_blob (db_con, blob, (const char *) details, new_space, used_hit_space)) {
				close_blob (blob);
				return FALSE;
			}

			close_blob (blob);

			update_word_details (db_con, id, total_hits, hit_array_size);

		} else {

			/* need to resize blob */
			char buffer[MAX_HIT_BUFFER];

			if (!read_blob (blob, buffer, used_hit_space, 0)) {
				close_blob (blob);
				return FALSE;
			}

			int padding = get_padding (total_hits, sz);
			int new_size = (new_space - free_hit_space) + padding + hit_array_size;
		
			/* ToDo : should stream in smaller chunks to be more memory efficient */

			if (!resize_word_details (db_con, id, total_hits, new_size)) {
				close_blob (blob);
				return FALSE;
			}
			close_blob (blob);


			blob = get_blob (db_con, id, TRUE);

			if (!blob) {
				return FALSE;
			}
			
			if (!write_blob (db_con, blob, (const char *) buffer, used_hit_space, 0)) {
				close_blob (blob);
				return FALSE;
			}

			if (!write_blob (db_con, blob, (const char *) details, new_space, used_hit_space)) {
				close_blob (blob);
				return FALSE;
			}

			close_blob (blob);

			
		}

	} else {
		/* its a new word */

		if (new_count > MAX_HITS_FOR_WORD) {
			new_count = MAX_HITS_FOR_WORD;
		}

		int new_space = new_count * sz;
		int padding = get_padding (new_count, sz);

		id = insert_word_details (db_con, word, new_count, (new_space + padding));

		if (id > 0) {
			blob = get_blob (db_con, id, TRUE);
			if (!blob) {
				return FALSE;
			}

			if (!write_blob (db_con, blob, (const char *) details, new_space, 0)) {
				close_blob (blob);
				return FALSE;
			}

			close_blob (blob);
			
		}

	}

	return TRUE;	
}


/* append individual word for a document */

gboolean
tracker_indexer_append_word (DBConnection *db_con, const char *word, guint32 id, int service, int score)
{
        if (tracker_shutdown || score < 1) {
                return FALSE;
        }

	g_return_val_if_fail ((db_con && word), FALSE);

	WordDetails pair;

	pair.id = id;
	pair.amalgamated = tracker_indexer_calc_amalgamated (service, score);


	return tracker_indexer_append_word_chunk (db_con, word, &pair, 1);

}


/* use for deletes or updates when doc is not new */
gboolean
tracker_indexer_update_word (DBConnection *db_con, const char *word, guint32 id, int service, int score, gboolean remove_word)
{
	int  tsiz;

        if (tracker_shutdown) {
                return FALSE;
        }

	g_return_val_if_fail ((db_con && word), FALSE);
	
	/* check if existing record is there  */
	sqlite_int64 row_id=0;
	int hit_count=0, hit_array_size=0;
	sqlite3_blob *blob;
	int sz = sizeof (WordDetails);

	if (get_word_details (db_con, word, &row_id, &hit_count, &hit_array_size)) {

		blob = get_blob (db_con, row_id, TRUE);
		if (!blob) {
			return FALSE;
		}	

		char buffer[MAX_HIT_BUFFER];

		tsiz = (hit_count * sz);

		if (!read_blob (blob, &buffer, tsiz, 0)) {
			close_blob (blob);
			return FALSE;
		}

		WordDetails *details;
		int i;

		details = (WordDetails *) buffer;
		
		for (i = 0; i < hit_count; i++) {

			if (details[i].id == id) {
									
				/* NB the paramter score can be negative */
				score += get_score (&details[i]);
									
				if (score < 1 || remove_word) {
						
					int k, mod_count=0;

					/* shift all subsequent records in array down one place */
					for (k = i + 1; k < hit_count; k++) {
						details[k - 1] = details[k];
						mod_count++;
					}
					
					/* write back only the parts that have changed */
					if (!write_blob (db_con, blob, &buffer[i * sz], mod_count * sz, i * sz)) {
						close_blob (blob);
						return FALSE;
					}

					/* update hit count to 1 less */
					update_word_details (db_con, row_id, hit_count-1, hit_array_size);
					

				} else {
					details[i].amalgamated = tracker_indexer_calc_amalgamated (service, score);

					/* write back only the part that has changed */
					if (!write_blob (db_con, blob, &buffer[i * sz], sz, i * sz)) {
						close_blob (blob);
						return FALSE;
					}
				}

				close_blob (blob);
				return TRUE;
			}
		}

		close_blob (blob);

	}

	return (tracker_indexer_append_word (db_con, word, id, service, score));
}


/* use for deletes or updates of multiple entities when they are not new */
GSList *
tracker_indexer_update_word_list (DBConnection *db_con, const char *word, GSList *update_list)
{
	int  tsiz, score;
	GSList *list = NULL;
	WordDetails *word_details;
	gboolean write_back = FALSE;

        if (tracker_shutdown) {
                return list;
        }

	g_return_val_if_fail ((db_con && word && update_list), NULL);
	
	/* check if existing record is there  */
	sqlite_int64 row_id=0;
	int hit_count=0, hit_array_size=0;
	sqlite3_blob *blob;
	int sz = sizeof (WordDetails);

	if (get_word_details (db_con, word, &row_id, &hit_count, &hit_array_size)) {

		blob = get_blob (db_con, row_id, TRUE);
		if (!blob) {
			return list;
		}	

		char buffer[MAX_HIT_BUFFER];

		tsiz = (hit_count * sz);

		if (!read_blob (blob, &buffer, tsiz, 0)) {
			close_blob (blob);
			return list;
		}

		WordDetails *details;
		int i;
		GSList *l;

		details = (WordDetails *) buffer;

		for (l=update_list; l; l=l->next) {
			
			if (!l->data) {
				tracker_error ("possible corrupted data in cache");
				continue;
			}

			word_details = l->data;

			gboolean edited = FALSE;

			for (i = 0; i < hit_count; i++) {

				if (details[i].id == word_details->id) {

					write_back = TRUE;
	
					/* NB the paramter score can be negative */
					score = get_score (&details[i]) + get_score (word_details);
							
					/* check for deletion */		
					if (score < 1) {
						
						int k;
					
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

			/* add hits that could not be updated directly here so they can be appended later - otherwise free them */
			if (!edited) {
				list = g_slist_prepend (list, word_details);
				tracker_debug ("could not update word hit %s - appending", word);
			} else {
#ifdef USE_SLICE							
				g_slice_free (WordDetails, word_details);
#else
				g_free (word_details);
#endif
			}

		}

	
		/* write back if we have modded anything */
		if (!write_back || !write_blob (db_con, blob, &buffer, hit_count * sz, 0)) {
			close_blob (blob);
			return list;
		}

		update_word_details (db_con, row_id, hit_count, hit_array_size);

		close_blob (blob);
	
		return list;
	}

	/* none of the updates can be applied if word does not exist so return them all to be appended later */
	tracker_debug ("none of the updated hits for word %s could be applied - appending...", word);
	return update_list;
}



/* use to delete dud hits for a word - dud_list is a list of SearchHit structs */
gboolean
tracker_remove_dud_hits (DBConnection *db_con, const char *word, GSList *dud_list)
{
	int  tsiz;
	gboolean made_changes = FALSE;

	if (tracker_shutdown) {
                return FALSE;
        }

	g_return_val_if_fail ((db_con && word && dud_list), FALSE);

	
	sqlite_int64 id=0;
	int hit_count=0, hit_array_size=0;
	sqlite3_blob *blob;
	int sz = sizeof (WordDetails);

	if (get_word_details (db_con, word, &id, &hit_count, &hit_array_size)) {

		blob = get_blob (db_con, id, TRUE);
		if (!blob) {
			return FALSE;
		}	

		char buffer[MAX_HIT_BUFFER];

		tsiz = (hit_count * sz);

		if (!read_blob (blob, &buffer, tsiz, 0)) {
			close_blob (blob);
			return FALSE;
		}

		WordDetails *details;
		int i;

		details = (WordDetails *) buffer;

		for (i = 0; i < hit_count; i++) {

			GSList *lst;

			for (lst = dud_list; lst; lst = lst->next) {

				SearchHit *hit = lst->data;

				if (hit) {
					if (details[i].id == hit->service_id) {
						int k;

						made_changes = TRUE;
						
						/* shift all subsequent records in array down one place */
						for (k = i + 1; k < hit_count; k++) {
							details[k - 1] = details[k];
						}

						/* make size of array one size smaller */
						tsiz -= sizeof (WordDetails); 
						hit_count--;

						break;
					}
				}
			}
		}

		if (made_changes) {
			if (!write_blob (db_con, blob, buffer, hit_count, 0)) {
				close_blob (blob);
				return FALSE;
			}
		}
		close_blob (blob);

		return TRUE;
	}


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
count_hit_size_for_word (DBConnection *db_con, const char *word)
{
	sqlite_int64 id=0;
	int hit_count=0, hit_array_size=0;
	
	get_word_details (db_con, word, &id, &hit_count, &hit_array_size);
	
	return hit_count;

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
tracker_create_query (DBConnection *db_con, int *service_array, int service_array_count, int offset, int limit)
{
	SearchQuery *result = g_new0 (SearchQuery, 1);

	result->db_con = db_con;
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
get_hits_for_single_word (SearchQuery *query, SearchWord *search_word, int *return_hit_count)
{
	int  tsiz, total_count = 0;
	GSList *result = NULL;

	int offset = query->offset;

	/* some results might be dud so get an extra 100 to compensate */
	int limit = query->limit + 100;
	
	if (tracker_shutdown) {
                return NULL;
        }

	*return_hit_count = 0;

	sqlite_int64 id=0;
	int hit_count=0, hit_array_size=0;
	sqlite3_blob *blob;
	int sz = sizeof (WordDetails);

	if (get_word_details (query->db_con, search_word->word, &id, &hit_count, &hit_array_size)) {

		blob = get_blob (query->db_con, id, FALSE);
		if (!blob) {
			return NULL;
		}	

		char buffer[MAX_HIT_BUFFER];

		tsiz = (hit_count * sz);

		if (!read_blob (blob, &buffer, tsiz, 0)) {
			close_blob (blob);
			return NULL;
		}

		close_blob (blob);

		WordDetails *details;

		details = (WordDetails *) buffer;
		
		tracker_debug ("total hit count (excluding service divisions) for %s is %d", search_word->word, hit_count);
		
		qsort (details, hit_count, sizeof (WordDetails), compare_words);

		int i;
		for (i = 0; i < hit_count; i++) {
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
		

	} else {
		return NULL;
	}

	*return_hit_count = total_count;
	
	return g_slist_reverse (result);
}


static GHashTable *
get_intermediate_hits (SearchQuery *query, GHashTable *match_table, SearchWord *search_word, BoolOp bool_op)
{
	int  tsiz;
	GHashTable *result;

	if (tracker_shutdown) {
                return NULL;
        }

	result = g_hash_table_new (NULL, NULL);
	
	sqlite_int64 id=0;
	int hit_count=0, hit_array_size=0;
	sqlite3_blob *blob;
	int sz = sizeof (WordDetails);

	if (get_word_details (query->db_con, search_word->word, &id, &hit_count, &hit_array_size)) {

		blob = get_blob (query->db_con, id, FALSE);
		if (!blob) {
			return result;
		}	

		char buffer[MAX_HIT_BUFFER];

		tsiz = (hit_count * sz);

		if (!read_blob (blob, &buffer, tsiz, 0)) {
			close_blob (blob);
			return result;
		}

		close_blob (blob);

		WordDetails *details;
		details = (WordDetails *) buffer;

		int i;
		for (i = 0; i < hit_count; i++) {

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

	if (match_table) {
		g_hash_table_destroy (match_table);
	}


	tracker_debug ("%d matches for word %s", g_hash_table_size (result), search_word->word);

	return result;
}


static GSList *
get_final_hits (SearchQuery *query, GHashTable *match_table, SearchWord *search_word, BoolOp bool_op, int *return_hit_count)
{
	int  tsiz, rnum;
	SearchHit *result;
	GSList *list;

	int offset = query->offset;

	/* some results might be dud so get an extra 100 to compensate */
	int limit = query->limit + 100;

	*return_hit_count = 0;

	rnum = 0;
	list = NULL;

	if (tracker_shutdown) {
                return NULL;
        }

	if (!match_table || g_hash_table_size (match_table) < 1) {
		return NULL;
	}
				
	sqlite_int64 id=0;
	int hit_count=0, hit_array_size=0;
	sqlite3_blob *blob;
	int sz = sizeof (WordDetails);

	if (get_word_details (query->db_con, search_word->word, &id, &hit_count, &hit_array_size)) {

		blob = get_blob (query->db_con, id, FALSE);
		if (!blob) {
			return NULL;
		}	

		char buffer[MAX_HIT_BUFFER];

		tsiz = (hit_count * sz);

		if (!read_blob (blob, &buffer, tsiz, 0)) {
			close_blob (blob);
			return NULL;
		}
		close_blob (blob);

		WordDetails *details;
		details = (WordDetails *) buffer;
	
		int size = g_hash_table_size (match_table);

		if (bool_op == BoolAnd) {
			result = g_malloc0 (sizeof (SearchHit) * size);
		} else {
			result = g_malloc0 (sizeof (SearchHit) * (size + hit_count));
		}
				
		int i;
		for (i = 0; i < hit_count; i++) {

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
			g_free (result);
			return NULL;
		}

		int count;

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

	return g_slist_reverse (list);
}


gboolean
tracker_indexer_get_hits (SearchQuery *query)
{
	int word_count;
	GHashTable *table;
	SearchWord *word;
	GSList *lst;

	if (tracker_shutdown) {
                return FALSE;
	}

	g_return_val_if_fail ((query->db_con && query->words && (query->limit > 0)), FALSE);

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

		word->hit_count = count_hit_size_for_word (query->db_con, word->word);
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

	if (tracker_shutdown) {
                return NULL;
	}

	g_return_val_if_fail ((query && query->words), NULL);

	int word_count = g_slist_length (query->words);

	if (word_count == 0) {
                return NULL;
        }

	query->service_array = NULL;
	query->service_array_count = 0;

	query->hits = NULL;

	if (tracker_indexer_get_hits (query)) {
	
		GSList *tmp;

		for (tmp = query->hits; tmp; tmp=tmp->next) {

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
		tracker_index_free_hit_list (query->hits);
		query->hits = NULL;

	}


	

	/* search emails */
	DBConnection *tmp_db = query->db_con;

	query->db_con = query->db_con_email;

	if (tracker_indexer_get_hits (query)) {

		GSList *tmp;
	
		for (tmp = query->hits; tmp; tmp=tmp->next) {

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

		tracker_index_free_hit_list (query->hits);
		query->hits = NULL;

	}

	query->db_con = tmp_db;

	GSList *list, *lst;

	list = g_hash_table_key_slist (table);

	list = g_slist_sort (list, (GCompareFunc) sort_func);

	int len, i;
	len = g_slist_length (list);
	
	char **res = g_new0 (char *, len + 1);

	res[len] = NULL;

	i = 0;

	for (lst = list; i < len && lst && lst->next; lst = lst->next) {

		if (!lst || !lst->data) {
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

int
tracker_get_hit_count (SearchQuery *query)
{
	if (tracker_shutdown) {
                return 0;
	}

	g_return_val_if_fail ((query && query->words), 0);

	int word_count = g_slist_length (query->words);

	if (word_count == 0) {
                return 0;
        }

	if (!tracker_indexer_get_hits (query)) {
		return 0;
	} else {
		return query->hit_count;
	}
}


