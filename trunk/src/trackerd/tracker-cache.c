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
#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "tracker-utils.h"
#include "tracker-dbus.h"
#include "tracker-cache.h"


#define USE_SLICE

extern Tracker *tracker;

typedef struct
{
	Indexer		*file_index;
	Indexer		*file_update_index;
	Indexer		*email_index;
	

} IndexConnection;


static Indexer *
create_merge_index (const char *name)
{
	Indexer *indexer;
	char *temp_file_name;
	int  i;

	for (i=1; i < 1000; i++) {
		temp_file_name = g_strdup_printf ("%s%d", name, i);

		char *tmp = g_build_filename (tracker->data_dir, temp_file_name, NULL);	

		if (g_file_test (tmp , G_FILE_TEST_EXISTS)) {
			g_free (temp_file_name);
			g_free (tmp);
			continue;
		}
		g_free (tmp);
		break;
	}

	indexer = tracker_indexer_open (temp_file_name);

	g_free (temp_file_name);

	return indexer;
}



static gint
flush_all_file_words (  gpointer         key,
	   	 	gpointer         value,
	   	 	gpointer         data)
{
	IndexConnection *index_con = data;

	GByteArray *array = value;

	if (array) {

		tracker_indexer_append_word_chunk (index_con->file_index, key, (WordDetails *) array->data, (array->len / sizeof (WordDetails)));

		g_byte_array_free  (array, TRUE);
	}

	g_free (key);

  	return 1;
}


static gint
flush_all_file_update_words (   gpointer         key,
	   	 		gpointer         value,
	   	 		gpointer         data)
{
	IndexConnection *index_con = data;

	GByteArray *array = value;

	if (array) {

		tracker_indexer_update_word_chunk (index_con->file_update_index, key, (WordDetails *) array->data, (array->len / sizeof (WordDetails)));
  
		g_byte_array_free  (array, TRUE);
	}

	g_free (key);

  	return 1;
}


static gint
flush_all_email_words ( gpointer         key,
	   	 	gpointer         value,
	   	 	gpointer         data)
{
	IndexConnection *index_con = data;
	
	GByteArray *array = value;

	if (array) {

		tracker_indexer_append_word_chunk (index_con->email_index, key, (WordDetails *) array->data, (array->len / sizeof (WordDetails)));

		g_byte_array_free  (array, TRUE);
	}

	g_free (key);
	
  	return 1;
}

void
tracker_cache_flush_all ()
{
	IndexConnection index_con;
	gboolean using_file_tmp = FALSE, using_email_tmp = FALSE;

	if (tracker->word_count == 0 && tracker->word_update_count == 0) {
		return;
	}

	tracker_log ("Flushing all words - total hits in cache is %d, total words %d", tracker->word_detail_count, tracker->word_count);

	/* if word count is small then flush to main index rather than a new temp index */
	if (tracker->word_count < 5000) {
	
		index_con.file_index = tracker->file_index;
		index_con.email_index = tracker->email_index;

	} else {

		/* determine is index has been written to significantly before and create new ones if so */
		if (tracker_indexer_size (tracker->file_index) > 4000000) {
			index_con.file_index = create_merge_index ("file-index.tmp.");
			tracker_log ("flushing to %s", dpname (index_con.file_index->word_index));
			using_file_tmp = TRUE;
		} else {
			index_con.file_index = tracker->file_index;
		}
		
		if (tracker_indexer_size (tracker->email_index) > 4000000) {
			index_con.email_index = create_merge_index ("email-index.tmp.");
			tracker_log ("flushing to %s", dpname (index_con.email_index->word_index));
			using_email_tmp = TRUE;
		} else {
			index_con.email_index = tracker->email_index;
		}
	}

	if (!tracker_indexer_has_merge_files (INDEX_TYPE_FILES) && tracker->word_update_count < 10000) {
		index_con.file_update_index = tracker->file_index;
	} else {
		index_con.file_update_index = tracker->file_update_index;
	}

	g_hash_table_foreach (tracker->file_word_table, (GHFunc) flush_all_file_words, &index_con);
	g_hash_table_destroy (tracker->file_word_table);

	g_hash_table_foreach (tracker->email_word_table, (GHFunc) flush_all_email_words, &index_con);
	g_hash_table_destroy (tracker->email_word_table);

	g_hash_table_foreach (tracker->file_update_word_table, (GHFunc) flush_all_file_update_words, &index_con);
	g_hash_table_destroy (tracker->file_update_word_table);

	if (using_file_tmp) {
		tracker_indexer_close (index_con.file_index);
	}

	if (using_email_tmp) {
		tracker_indexer_close (index_con.email_index);
	}

	tracker->file_word_table = g_hash_table_new (g_str_hash, g_str_equal);
	tracker->file_update_word_table = g_hash_table_new (g_str_hash, g_str_equal);
	tracker->email_word_table = g_hash_table_new (g_str_hash, g_str_equal);

	tracker->word_detail_count = 0;
	tracker->word_count = 0;
	tracker->flush_count = 0;
	tracker->word_update_count = 0;
}


static gboolean
cache_needs_flush ()
{

	int estimate_cache;

	estimate_cache = tracker->word_detail_count * 8;

	estimate_cache += (tracker->word_count * 75) + (tracker->word_update_count * 75);

	if (estimate_cache > tracker->memory_limit) {
		return TRUE;
	}

	return FALSE;

	
}


static inline gboolean
is_email (gint service_type) 
{
	return (service_type >= tracker->email_service_min && service_type <= tracker->email_service_max);
}


static gboolean
update_word_table (GHashTable *table, const char *word, WordDetails *word_details)
{
	gboolean new_word = FALSE;

	int sz = sizeof (WordDetails);

	tracker->word_detail_count++;
	
	GByteArray *array = g_hash_table_lookup (table, word);

	if (!array) {

		if (tracker->use_extra_memory) {
			array = g_byte_array_sized_new (sz * 2);
		} else {
			array = g_byte_array_sized_new (sz);
		}
		
		new_word = TRUE;
	} 

	array = g_byte_array_append (array, (guint8 *) word_details, sz);

	if (new_word) {
		g_hash_table_insert (table, g_strdup (word), array);
	} else {
		g_hash_table_insert (table, (gchar *) word, array);
	}

	return new_word;

}


void
tracker_cache_add (const gchar *word, guint32 service_id, gint service_type, gint score, gboolean is_new)
{
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
	WordDetails word_details;

	word_details.id = service_id;
	word_details.amalgamated = tracker_indexer_calc_amalgamated (service_type, score);

	if (is_new) {

		/* no need to mutex new stuff as only one thread is processing them */
		if (!is_email (service_type)) {
			if (update_word_table (tracker->file_word_table, word, &word_details)) tracker->word_count++;
		} else {
			if (update_word_table (tracker->email_word_table, word, &word_details)) tracker->word_count++;
		}

	} else {
		/* we need to mutex this to prevent corruption on multi cpu machines as both index process thread and user request thread (when setting tags/metadata) can call this */
		g_static_mutex_lock (&mutex);
		
		if (update_word_table (tracker->file_update_word_table, word, &word_details)) tracker->word_update_count++;

		g_static_mutex_unlock (&mutex);
	}

}

gboolean
tracker_cache_process_events (DBConnection *db_con, gboolean check_flush) 
{
	gboolean stopped_trans = FALSE;
	
	while (TRUE) {

		gboolean sleep = FALSE;

		if (!tracker->is_running || !tracker->enable_indexing) {
			if (check_flush) tracker_cache_flush_all ();
			sleep = TRUE;
		}

		if (tracker->index_status > INDEX_APPLICATIONS && tracker_pause ()) {
			if (db_con) {
				stopped_trans = TRUE;
			}

			sleep = TRUE;
		}

		if (sleep) {

			if (db_con) tracker_db_end_index_transaction (db_con);	

			tracker_dbus_send_index_status_change_signal ();

			/* set mutex to indicate we are in "check" state to prevent race conditions from other threads resetting gloabl vars */
			g_mutex_lock (tracker->files_check_mutex);		

			if ((tracker_pause () || !tracker->is_running || !tracker->enable_indexing) && (!tracker->shutdown))  {
				g_cond_wait (tracker->file_thread_signal, tracker->files_signal_mutex);
			}

			g_mutex_unlock (tracker->files_check_mutex);

			/* determine if wake up call is a shutdown signal */
			if (tracker->shutdown) {
				if (check_flush) tracker_cache_flush_all ();
				return FALSE;				
			} else {
				tracker_dbus_send_index_status_change_signal ();
				continue;
			}
				
		}
		
		if (tracker->grace_period > 1) {

			tracker_log ("pausing indexing while client requests or external disk I/O are taking place");

			tracker->request_waiting = FALSE;

			if (db_con) {
				tracker_db_end_index_transaction (db_con);
				stopped_trans = TRUE;
			}

			tracker->pause_io = TRUE;

			tracker_dbus_send_index_status_change_signal ();
		
			g_usleep (1000 * 1000);
		
			tracker->grace_period--;

			if (tracker->grace_period > 2) tracker->grace_period = 2;

			continue;

		} else {
			if (tracker->pause_io) {
				tracker->pause_io = FALSE;
				tracker_dbus_send_index_status_change_signal ();
			}
		}


		if (check_flush && cache_needs_flush ()) {

			if (db_con) {
				tracker_db_end_index_transaction (db_con);
				tracker_db_refresh_all (db_con->data);
				stopped_trans = TRUE;
			}

			tracker_cache_flush_all ();

		}

		
		if (stopped_trans && db_con && !db_con->in_transaction) tracker_db_start_index_transaction (db_con);

		tracker_throttle (5000);

		return TRUE;

	}	


}

