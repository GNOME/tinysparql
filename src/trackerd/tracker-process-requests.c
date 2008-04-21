/* Tracker - indexer and metadata database engine
 * Copyright (C) 2008, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include "config.h"

#include <string.h>
#include <signal.h>

#ifdef OS_WIN32
#include <windows.h>
#include <pthread.h>
#include "mingw-compat.h"
#endif

#include <glib.h>

#include <libtracker-common/tracker-log.h>

#include "tracker-db.h"
#include "tracker-dbus.h"
#include "tracker-dbus-files.h"
#include "tracker-dbus-keywords.h"
#include "tracker-dbus-methods.h"
#include "tracker-dbus-metadata.h"
#include "tracker-dbus-search.h"
#include "tracker-utils.h"

static void
process_block_signals (void)
{
	sigset_t signal_set;

        /* Block all signals in this thread */
        sigfillset (&signal_set);
        
#ifndef OS_WIN32
        pthread_sigmask (SIG_BLOCK, &signal_set, NULL);
#endif
}

/* This is the thread entry function for handing DBus requests by the
 * daemon to any clients connected.
 */
gpointer
tracker_process_requests (gpointer data)
{
	Tracker      *tracker;
	DBConnection *db_con;

        tracker = (Tracker*) data;

        process_block_signals ();

	g_mutex_lock (tracker->request_signal_mutex);
	g_mutex_lock (tracker->request_stopped_mutex);

	/* Set thread safe DB connection */
	tracker_db_thread_init ();

	db_con = tracker_db_connect_all (FALSE);

	tracker_db_prepare_queries (db_con);

	while (TRUE) {
		DBusRec	    *rec;
		DBusMessage *reply;
                gboolean     result;

		/* Make thread sleep if first part of the shutdown
                 * process has been activated.
                 */
		if (!tracker->is_running) {
			g_cond_wait (tracker->request_thread_signal, 
                                     tracker->request_signal_mutex);

			/* Determine if wake up call is new stuff or a
                         * shutdown signal.
                         */
			if (!tracker->shutdown) {
				continue;
			} else {
				break;
			}
		}

		/* Lock check mutex to prevent race condition when a
                 * request is submitted after popping queue but prior
                 * to sleeping.
                 */
		g_mutex_lock (tracker->request_check_mutex);
		rec = g_async_queue_try_pop (tracker->user_request_queue);

		if (!rec) {
			g_cond_wait (tracker->request_thread_signal, 
                                     tracker->request_signal_mutex);
			g_mutex_unlock (tracker->request_check_mutex);

			/* Determine if wake up call is new stuff or a
                         * shutdown signal.
                         */
			if (!tracker->shutdown) {
				continue;
			} else {
				break;
			}
		}

		/* Thread will not sleep without another iteration so
                 * race condition no longer applies.
                 */
		g_mutex_unlock (tracker->request_check_mutex);
	
		rec->user_data = db_con;

		switch (rec->action) {
                case DBUS_ACTION_PING:      
                        result = TRUE;

                        reply = dbus_message_new_method_return (rec->message);
                        dbus_message_append_args (reply,
                                                  DBUS_TYPE_BOOLEAN, &result,
                                                  DBUS_TYPE_INVALID);
                        
                        dbus_connection_send (rec->connection, reply, NULL);
                        dbus_message_unref (reply);
                        break;
                        
                case DBUS_ACTION_GET_STATS:
                        tracker_dbus_method_get_stats (rec);
                        break;
                        
                case DBUS_ACTION_GET_SERVICES:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_get_services (rec);
                        break;
                        
                case DBUS_ACTION_GET_VERSION:
                        tracker_dbus_method_get_version (rec);
                        break;
                        
                case DBUS_ACTION_GET_STATUS:
                        tracker_dbus_method_get_status (rec);
                        break;
                        
                case DBUS_ACTION_SET_BOOL_OPTION:
                        tracker_dbus_method_set_bool_option (rec);
                        break;
                        
                case DBUS_ACTION_SET_INT_OPTION:
                        tracker_dbus_method_set_int_option (rec);
                        break;
                        
                case DBUS_ACTION_SHUTDOWN:
                        tracker_dbus_method_shutdown (rec);
                        break;
                        
                case DBUS_ACTION_PROMPT_INDEX_SIGNALS:
                        tracker_dbus_method_prompt_index_signals (rec);
                        break;
                        
                case DBUS_ACTION_METADATA_GET:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_metadata_get (rec);
                        break;
                        
                case DBUS_ACTION_METADATA_SET:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_metadata_set(rec);
                        break;
                        
                case DBUS_ACTION_METADATA_REGISTER_TYPE:
                        tracker_dbus_method_metadata_register_type (rec);
                        break;
                        
                case DBUS_ACTION_METADATA_GET_TYPE_DETAILS:
                        tracker_dbus_method_metadata_get_type_details (rec);
                        break;
                        
                case DBUS_ACTION_METADATA_GET_REGISTERED_TYPES:
                        tracker_dbus_method_metadata_get_registered_types (rec);
                        break;
                        
                case DBUS_ACTION_METADATA_GET_WRITEABLE_TYPES:
                        tracker_dbus_method_metadata_get_writeable_types (rec);
                        break;
                        
                case DBUS_ACTION_METADATA_GET_REGISTERED_CLASSES:
                        tracker_dbus_method_metadata_get_registered_classes (rec);
                        break;
                        
                case DBUS_ACTION_KEYWORDS_GET_LIST:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_keywords_get_list (rec);
                        break;
                        
                case DBUS_ACTION_KEYWORDS_GET:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_keywords_get (rec);
                        break;
                        
                case DBUS_ACTION_KEYWORDS_ADD:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_keywords_add (rec);
                        break;
                        
                case DBUS_ACTION_KEYWORDS_REMOVE:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_keywords_remove (rec);
                        break;
                        
                case DBUS_ACTION_KEYWORDS_REMOVE_ALL:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_keywords_remove_all (rec);
                        break;
                        
                case DBUS_ACTION_KEYWORDS_SEARCH:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_keywords_search (rec);
                        break;
                        
                case DBUS_ACTION_SEARCH_GET_HIT_COUNT:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_search_get_hit_count (rec);
                        break;
                        
                case DBUS_ACTION_SEARCH_GET_HIT_COUNT_ALL:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_search_get_hit_count_all (rec);
                        break;
                        
                case DBUS_ACTION_SEARCH_TEXT:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_search_text (rec);
                        break;
                        
                case DBUS_ACTION_SEARCH_TEXT_DETAILED:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_search_text_detailed (rec);
                        break;
                        
                case DBUS_ACTION_SEARCH_GET_SNIPPET:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_search_get_snippet (rec);
                        break;
                        
                case DBUS_ACTION_SEARCH_FILES_BY_TEXT:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_search_files_by_text (rec);
                        break;
                        
                case DBUS_ACTION_SEARCH_METADATA:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_search_metadata (rec);
                        break;
                        
                case DBUS_ACTION_SEARCH_MATCHING_FIELDS:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_search_matching_fields (rec);
                        break;
                        
                case DBUS_ACTION_SEARCH_QUERY:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_search_query (rec);
                        break;
                        
                case DBUS_ACTION_SEARCH_SUGGEST:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_search_suggest (rec);
                        break;
                        
                case DBUS_ACTION_FILES_EXISTS:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_files_exists (rec);
                        break;
                        
                case DBUS_ACTION_FILES_CREATE:
                        tracker_dbus_method_files_create (rec);
                        break;
                        
                case DBUS_ACTION_FILES_DELETE:
                        tracker_dbus_method_files_delete (rec);
                        break;
                        
                case DBUS_ACTION_FILES_GET_SERVICE_TYPE:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_files_get_service_type (rec);
                        break;
                        
                case DBUS_ACTION_FILES_GET_TEXT_CONTENTS:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_files_get_text_contents (rec);
                        break;
                        
                case DBUS_ACTION_FILES_SEARCH_TEXT_CONTENTS:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_files_search_text_contents (rec);
                        break;
                        
                case DBUS_ACTION_FILES_GET_BY_SERVICE_TYPE:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_files_get_by_service_type (rec);
                        break;
                        
                case DBUS_ACTION_FILES_GET_BY_MIME_TYPE:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_files_get_by_mime_type (rec);
                        break;
                        
                case DBUS_ACTION_FILES_GET_BY_MIME_TYPE_VFS:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_files_get_by_mime_type_vfs (rec);
                        break;
                        
                case DBUS_ACTION_FILES_GET_MTIME:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_files_get_mtime (rec);
                        break;
                        
                case DBUS_ACTION_FILES_GET_METADATA_FOLDER_FILES:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_files_get_metadata_for_files_in_folder (rec);
                        break;
                        
                case DBUS_ACTION_FILES_SEARCH_BY_TEXT_MIME:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_files_search_by_text_mime (rec);
                        break;
                        
                case DBUS_ACTION_FILES_SEARCH_BY_TEXT_MIME_LOCATION:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_files_search_by_text_mime_location(rec);
                        break;
                        
                case DBUS_ACTION_FILES_SEARCH_BY_TEXT_LOCATION:
                        tracker->request_waiting = TRUE;
                        tracker->grace_period = 2;
                        tracker_dbus_method_files_search_by_text_location (rec);
                        break;
                        
                default:
                        break;
		}

		dbus_message_unref (rec->message);
		g_free (rec);
	}

	tracker_db_close_all (db_con);

	tracker_debug ("Request thread has exited successfully");

	/* Ulock mutex so we know thread has exited */
	g_mutex_unlock (tracker->request_check_mutex);
	g_mutex_unlock (tracker->request_stopped_mutex);

        return NULL;
}
