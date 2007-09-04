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

#include <sqlite3.h>
#include "tracker-utils.h"
#include "tracker-cache.h"
#include "tracker-indexer.h"

extern Tracker *tracker;

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
sort_func (char *a, char *b)
{
	Cache *lista, *listb;

	lista = g_hash_table_lookup (tracker->cached_table, a);
	listb = g_hash_table_lookup (tracker->cached_table, b);

	return ((lista->new_file_count + lista->new_email_count) - (listb->new_file_count + listb->new_email_count)); 
}


static GSList *
flush_update_list (DBConnection *db_con, GSList *list, const char *word)
{
	if (!list) return NULL;

	return tracker_indexer_update_word_list (db_con, word, list);
	
}



static void
flush_list (DBConnection *db_con, GSList *list, GSList *list2, const char *word)
{
	WordDetails word_details[MAX_HITS_FOR_WORD], *wd;
	int i, count;
	GSList *lst;

	i = 0;

	for (lst = list; (lst && i < MAX_HITS_FOR_WORD); lst = lst->next) {

		wd = lst->data;
		word_details[i].id = wd->id;
		word_details[i].amalgamated = wd->amalgamated;
		i++;
#ifdef USE_SLICE							
		g_slice_free (WordDetails, wd);
#else
		g_free (wd);
#endif

	}

	count = i;

	if (i >= MAX_HITS_FOR_WORD) {


		while (lst) {
			wd = lst->data;
#ifdef USE_SLICE							
			g_slice_free (WordDetails, wd);
#else
			g_free (wd);
#endif

			lst = lst->next;
			i++;
		}

	} else if (list2) {
	
		for (lst = list2; (lst && i < MAX_HITS_FOR_WORD); lst = lst->next) {

			wd = lst->data;
			word_details[i].id = wd->id;
			word_details[i].amalgamated = wd->amalgamated;
			i++;
#ifdef USE_SLICE							
			g_slice_free (WordDetails, wd);
#else
			g_free (wd);
#endif


		}
		count = i;

		if (i >= MAX_HITS_FOR_WORD) {


			while (lst) {
				wd = lst->data;
#ifdef USE_SLICE							
				g_slice_free (WordDetails, wd);
#else
				g_free (wd);
#endif

				lst = lst->next;
				i++;
			}
		}
	}
	
	if (count == 0) {
		return;
	}

	tracker->word_detail_count -= i;

	tracker_indexer_append_word_chunk (db_con, word, word_details, count);
	
}


static void
flush_cache (DBConnection *db_con, Cache *cache, const char *word)
{
	DBConnection *emails = db_con->emails;
	GSList *new_update_list = NULL;


	if (cache->update_file_list) {
		new_update_list = flush_update_list (db_con->word_index, cache->update_file_list, word);
	}

	if (cache->new_file_list) {
		flush_list (db_con->word_index, cache->new_file_list, new_update_list, word);
	} else {
		if (new_update_list) {
			flush_list (db_con->word_index, new_update_list, NULL, word);
		}

	}

	if (cache->new_email_list) {
		flush_list (emails->word_index, cache->new_email_list, NULL, word);
	}

	g_slist_free (cache->new_file_list);
	g_slist_free (cache->new_email_list);
	g_slist_free (cache->update_file_list);

#ifdef USE_SLICE							
	g_slice_free (Cache, cache);
#else
	g_free (cache);
#endif



	tracker->word_count--;
	tracker->update_count++;
}


static inline gboolean
is_min_flush_done (void)
{
	return (tracker->word_detail_count <= tracker->word_detail_min) && (tracker->word_count <= tracker->word_count_min);
}


static void
flush_rare (DBConnection *db_con)
{
	GSList *list, *lst;

	tracker_log ("flushing rare words - total hits in cache is %d, total words %d", tracker->word_detail_count, tracker->word_count);

	list = g_hash_table_key_slist (tracker->cached_table);

	list = g_slist_sort (list, (GCompareFunc) sort_func);

	DBConnection *emails = db_con->emails;

	tracker_db_start_transaction (db_con->word_index);
	tracker_db_start_transaction (emails->word_index);

	for (lst = list; (lst && !is_min_flush_done ()); lst = lst->next) {

		char *word = lst->data;

                Cache *cache;
		gpointer key = NULL;
		gpointer value = NULL;

		if (g_hash_table_lookup_extended (tracker->cached_table, word, &key, &value)) {

			cache = value;

			flush_cache (db_con, cache, word);

			g_hash_table_remove (tracker->cached_table, word);
	
		}

		g_free (word);
	}

	tracker_db_end_transaction (db_con->word_index);
	tracker_db_end_transaction (emails->word_index);

	/* clear cache memory as well */
	

	g_slist_free (list);

	tracker_log ("total hits in cache is %d, total words %d", tracker->word_detail_count, tracker->word_count);


}


static gint
flush_all (gpointer         key,
	   gpointer         value,
	   gpointer         data)
{

	DBConnection *db_con = data;

	flush_cache (db_con, value, key);

	g_free (key);

  	return 1;
}


void
tracker_cache_flush_all (DBConnection *db_con)
{

	if (g_hash_table_size (tracker->cached_table) == 0) {
		return;
	}

	tracker_log ("Flushing all words - total hits in cache is %d, total words %d", tracker->word_detail_count, tracker->word_count);

	DBConnection *emails = db_con->emails;

	tracker_db_start_transaction (db_con->word_index);
	tracker_db_start_transaction (emails->word_index);

	g_hash_table_foreach (tracker->cached_table, (GHFunc) flush_all, db_con);

	tracker_db_end_transaction (db_con->word_index);
	tracker_db_end_transaction (emails->word_index);

	g_hash_table_destroy (tracker->cached_table);

	tracker->cached_table = g_hash_table_new (g_str_hash, g_str_equal);

	tracker->word_detail_count = 0;
	tracker->word_count = 0;
	tracker->flush_count = 0;

}


void
tracker_cache_flush (DBConnection *db_con)
{
	if (tracker->word_detail_count > tracker->word_detail_limit || tracker->word_count > tracker->word_count_limit) {
		if (tracker->flush_count < 5) {
			tracker->flush_count++;
			tracker_info ("flushing");
			flush_rare (db_con);
		} else {
			tracker_cache_flush_all (db_con);
		}
	}
}


static inline void			
cache_free (Cache *cache) 
{

	g_slist_free (cache->new_file_list);
	g_slist_free (cache->new_email_list);
	g_slist_free (cache->update_file_list);

#ifdef USE_SLICE							
	g_slice_free (Cache, cache);
#else
	g_free (cache);
#endif

}



static inline gboolean
is_email (int service_type) 
{
	return (service_type >= tracker->email_service_min && service_type <= tracker->email_service_max);
}


void
tracker_cache_add (const char *word, guint32 service_id, int service_type, int score, gboolean is_new)
{
	Cache *cache;
	WordDetails *word_details;
	gboolean new_cache = FALSE;

#ifdef USE_SLICE							
	word_details = g_slice_new (WordDetails);
#else
	word_details = g_new (WordDetails, 1);
#endif


	word_details->id = service_id;
	word_details->amalgamated = tracker_indexer_calc_amalgamated (service_type, score);

	cache = g_hash_table_lookup (tracker->cached_table, word);

	if (!cache) {
#ifdef USE_SLICE							
		cache = g_slice_new0 (Cache);
#else
		cache = g_new0 (Cache, 1);
#endif
		tracker->word_count++;
		new_cache = TRUE;
	}

	if (is_new) {

		if (!is_email (service_type)) {

			if (cache->new_file_count >= MAX_HITS_FOR_WORD) {
				return;
			} else {
				cache->new_file_count++;
			}

			cache->new_file_list = g_slist_prepend (cache->new_file_list, word_details);			

		} else {

			if (cache->new_email_count >= MAX_HITS_FOR_WORD) {
				return;
			} else {
				cache->new_email_count++;
			}

			cache->new_email_list = g_slist_prepend (cache->new_email_list, word_details);			
		}

	} else {
		cache->update_file_list = g_slist_prepend (cache->update_file_list, word_details);			
	}


	if (new_cache) {
		g_hash_table_insert (tracker->cached_table, g_strdup (word), cache);
	} else {
		g_hash_table_insert (tracker->cached_table, (char *) word, cache);
	}

	tracker->word_detail_count++;
}




