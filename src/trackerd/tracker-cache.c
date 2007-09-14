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
#include "tracker-db-sqlite.h"
#include "tracker-utils.h"
#include "tracker-cache.h"
#include "tracker-indexer.h"


#define USE_SLICE

extern Tracker *tracker;


static inline Cache *
cache_new (void)
{
	tracker->word_count++;

	if (tracker->use_extra_memory) {
		return g_slice_new0 (Cache);
	} else {
		return g_new0 (Cache, 1);
	}
}


static inline WordDetails *
word_details_new (void)
{
	tracker->word_detail_count++;

	if (tracker->use_extra_memory) {							
		return g_slice_new (WordDetails);
	} else {
		return g_new (WordDetails, 1);
	}
}


static void
free_hits (WordDetails *word_details, gpointer data) 
{
	tracker->word_detail_count--;

	if (tracker->use_extra_memory) {							
		g_slice_free (WordDetails, word_details);
	} else {
		g_free (word_details);
	}
}


static inline void
cache_free (Cache *cache) 
{
	g_slist_foreach (cache->new_file_list, (GFunc) free_hits, NULL);
	g_slist_foreach (cache->new_email_list, (GFunc) free_hits, NULL);
	g_slist_foreach (cache->update_file_list, (GFunc) free_hits, NULL);

	g_slist_free (cache->new_file_list);
	g_slist_free (cache->new_email_list);
	g_slist_free (cache->update_file_list);

	if (tracker->use_extra_memory) {						
		g_slice_free (Cache, cache);
	} else {
		g_free (cache);
	}

	tracker->word_count--;
}


static DBConnection *
create_merge_index (const gchar *name, gboolean update)
{
	gchar *dbname;
	DBConnection *db_con;

	char *new_name = g_strconcat ("tmp-", name, NULL);
	char *new_update_name = g_strconcat ("tmp-update-", name, NULL);


	if (!update) {
		dbname = g_build_filename (tracker->data_dir, new_name, NULL);
	} else {
		dbname = g_build_filename (tracker->data_dir, new_update_name, NULL);
	}
	
	g_free (new_name);
	g_free (new_update_name);

	if (g_file_test (dbname, G_FILE_TEST_IS_REGULAR)) {
		tracker_log ("database index file %s is already present", dbname);
		db_con = tracker_indexer_open (dbname);
		g_free (dbname);

		return db_con;
	}

	db_con = tracker_indexer_open (dbname);

	tracker_db_exec_no_reply (db_con, "update HitIndex set HitCount = 0");
	
	g_free (dbname);

	return db_con;
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
sort_func (gchar *a, gchar *b)
{
	Cache *ca, *cb;

	ca = g_hash_table_lookup (tracker->cached_table, a);
	cb = g_hash_table_lookup (tracker->cached_table, b);
	
	return (ca->hit_count - cb->hit_count); 
}


static inline gboolean
needs_merge (DBConnection *db_con)
{
	if  (tracker_indexer_size (db_con) > tracker->merge_limit) {
		
		if (!db_con->merge_index) {
			db_con->merge_index = create_merge_index (db_con->name, FALSE);
			db_con->merge_update_index = create_merge_index (db_con->name, TRUE);
		}
		return TRUE;
	}
	
	return FALSE;
	
}


static void
flush_list (DBConnection *db_con, GSList *list1, GSList *list2, const gchar *word)
{
	if (needs_merge (db_con)) {
		tracker_indexer_append_word_lists (db_con->merge_index, word, list1, list2);
	} else {
		tracker_indexer_append_word_lists (db_con, word, list1, list2);
	}
}

static GSList *
flush_update_list (DBConnection *db_con, GSList *list, const gchar *word)
{
        if (!list) {
                return NULL;
        }
	
	if (needs_merge (db_con)) {
		GSList *ret_list = tracker_indexer_update_word_list (db_con->merge_update_index, word, list);
		if (ret_list) {
			tracker_indexer_append_word_lists (db_con->merge_update_index, word, ret_list, NULL);
			g_slist_free (ret_list);
		}		

	} else {
		GSList *ret_list = tracker_indexer_update_word_list (db_con, word, list);
		return ret_list;
	}

	return NULL;
}





static void
flush_cache (DBConnection *db_con, Cache *cache, const gchar *word)
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
			g_slist_free (new_update_list);
		}
	}

	if (cache->new_email_list) {
		flush_list (emails->word_index, cache->new_email_list, NULL, word);
	}

	cache_free (cache);
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

	tracker_log ("flushing rare words - total memory used by tracker %d kb, total hits in cache is %d, total words %d", tracker_get_memory_usage(), tracker->word_detail_count, tracker->word_count);

	list = g_hash_table_key_slist (tracker->cached_table);

	list = g_slist_sort (list, (GCompareFunc) sort_func);

	DBConnection *emails = db_con->emails;

	tracker_db_start_transaction (db_con->word_index);
	tracker_db_start_transaction (emails->word_index);

	for (lst = list; (lst && !is_min_flush_done ()); lst = lst->next) {

		gchar *word = lst->data;

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

	/* clear any memory as well by closing/reopening */
	tracker_indexer_free (db_con->word_index, FALSE);
	tracker_indexer_free (emails->word_index, FALSE);

	db_con->word_index = tracker_indexer_open ("file-index.db");
	emails->word_index = tracker_indexer_open ("email-index.db");

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

	tracker_log ("Flushing all words - total memory used by tracker %d, total hits in cache is %d, total words %d", tracker_get_memory_usage(), tracker->word_detail_count, tracker->word_count);

	DBConnection *emails = db_con->emails;

	tracker_db_start_transaction (db_con->word_index);
	tracker_db_start_transaction (emails->word_index);

	g_hash_table_foreach (tracker->cached_table, (GHFunc) flush_all, db_con);

	tracker_db_end_transaction (db_con->word_index);
	tracker_db_end_transaction (emails->word_index);

	/* clear any memory as well by closing/reopening */

	tracker_indexer_free (db_con->word_index, FALSE);
	tracker_indexer_free (emails->word_index, FALSE);

	db_con->word_index = tracker_indexer_open ("file-index.db");
	emails->word_index = tracker_indexer_open ("email-index.db");

	//tracker_db_refresh_all (db_con);

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


static inline gboolean
is_email (gint service_type) 
{
	return (service_type >= tracker->email_service_min && service_type <= tracker->email_service_max);
}


void
tracker_cache_add (const gchar *word, guint32 service_id, gint service_type, gint score, gboolean is_new)
{
	Cache *cache;
	WordDetails *word_details;
	gboolean new_cache = FALSE;

	word_details = word_details_new ();

	word_details->id = service_id;
	word_details->amalgamated = tracker_indexer_calc_amalgamated (service_type, score);

	cache = g_hash_table_lookup (tracker->cached_table, word);

	if (!cache) {
		cache = cache_new ();
		new_cache = TRUE;
	}

	if (is_new) {

		if (!is_email (service_type)) {
			cache->new_file_list = g_slist_prepend (cache->new_file_list, word_details);			
		} else {
			cache->new_email_list = g_slist_prepend (cache->new_email_list, word_details);			
		}

	} else {
		cache->update_file_list = g_slist_prepend (cache->update_file_list, word_details);			
	}


	if (new_cache) {
		g_hash_table_insert (tracker->cached_table, g_strdup (word), cache);
	} else {
		g_hash_table_insert (tracker->cached_table, (gchar *) word, cache);
	}

	cache->hit_count++;
}
