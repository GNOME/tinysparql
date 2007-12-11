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

#include "tracker-watch.h"

#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <windows.h>

// private stuff

static GHashTable *watch_table;

extern Tracker	    *tracker;
static DBConnection *win_db_con;

static void
free_watch_func (gpointer user_data)
{
  g_free (user_data);
}

static DWORD sleep_thread_id;
static HANDLE sleep_thread;

#define MAX_DIRECTORIES  1024 // should be more than enough
#define MAX_FILENAME_LENGTH  1024 // should be more than enough

static int dir_id = 0;

static char buffer[MAX_DIRECTORIES][256];
static HANDLE monitor_handle[MAX_DIRECTORIES];
static OVERLAPPED overlapped_buffer[MAX_DIRECTORIES];
static char dirs[MAX_DIRECTORIES][256];

unsigned int __stdcall ThreadProc( LPVOID param ) 
{ 
  while (TRUE) {
    SleepEx(INFINITE, TRUE); // 10000 might be better
  }
}

void CALLBACK init_db (DWORD param)
{
  tracker_db_thread_init ();

  win_db_con = tracker_db_connect ();

  win_db_con->blob = tracker_db_connect_full_text ();
  win_db_con->cache = tracker_db_connect_cache ();

  win_db_con->thread = "notify";
}

// public stuff

gboolean 
tracker_start_watching (void)
{
  if (tracker->watch_limit == 0) {
    tracker->watch_limit = 8191;
  }

  watch_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, free_watch_func);

  // create a windows thread to synchonize all metadata changes in one thread
  sleep_thread = _beginthreadex( NULL, 0, ThreadProc, 0, 0, &sleep_thread_id );

  QueueUserAPC(init_db, sleep_thread, (DWORD) 0 );

  return TRUE;
}

void
tracker_end_watching (void)
{
  // kill thread and call these just before it dies

#if 0
  tracker_db_close (win_db_con);
  tracker_db_thread_end ();
#endif

  // release all watched dirs

  g_hash_table_destroy  (watch_table);
}

gboolean monitor_dir(int dir_id);

VOID CALLBACK callback(DWORD error_code,
		       DWORD bytes_transferred,
		       LPOVERLAPPED overlapped)        // I/O information buffer
{
  int callback_dir_id = (int) overlapped->hEvent;

  if (bytes_transferred > 0) {

    FILE_NOTIFY_INFORMATION *p = (FILE_NOTIFY_INFORMATION *) buffer[callback_dir_id];

    TrackerChangeAction event_type = TRACKER_ACTION_IGNORE;
    int counter = 1;

    while (p) {
      TrackerChangeAction event_type;

      switch (p->Action) {
      case FILE_ACTION_MODIFIED:
	event_type = TRACKER_ACTION_CHECK;
	counter = 1;
	break;
      case FILE_ACTION_REMOVED:
	event_type = TRACKER_ACTION_DELETE;
	counter = 0;
	break;
      case FILE_ACTION_ADDED:
	event_type = TRACKER_ACTION_CREATE;
	counter = 1;
	break;
      default:
	break;
      }

      if (event_type != TRACKER_ACTION_IGNORE) {

	FileInfo *info;
	char file_utf8_uri[MAX_FILENAME_LENGTH];

	int status = WideCharToMultiByte(CP_UTF8, 0, p->FileName, p->FileNameLength, 
					 file_utf8_uri, MAX_FILENAME_LENGTH, NULL, NULL);

	file_utf8_uri[p->FileNameLength/sizeof(WCHAR)] = '\0';

	char *file_uri = g_build_filename (dirs[callback_dir_id], file_utf8_uri, NULL);

	info = tracker_create_file_info (file_uri, event_type, counter, WATCH_OTHER);

	if (tracker_file_info_is_valid (info)) {

	  tracker_db_insert_pending_file (win_db_con, info->file_id, info->uri,  NULL, info->mime, 
					  info->counter, info->action, info->is_directory, (event_type == TRACKER_ACTION_CREATE), -1);
	  tracker_free_file_info (info);
	} 

	g_free(file_uri);
      }

      if (p->NextEntryOffset == 0)
	p = NULL;
      else
	p = (FILE_NOTIFY_INFORMATION *) ((char *) p + p->NextEntryOffset);
    }
  }

  // reschedule
  monitor_dir(callback_dir_id);
}

gboolean
monitor_dir(int dir_id) 
{
  return ReadDirectoryChangesW( monitor_handle[dir_id], buffer[dir_id], sizeof(buffer[dir_id]),
				TRUE, 
				//				FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE |
				FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_FILE_NAME,
				NULL, &overlapped_buffer[dir_id], callback);
}


void CALLBACK tracker_add_watch_dir_wrapped (DWORD param)
{
  char *dir = (char *) param;

  char *dir_in_locale;

  dir_in_locale = g_filename_from_utf8 (dir, -1, NULL, NULL, NULL);

  if (!dir_in_locale) {
    free(dir);
    return;
  }

  /* check directory permissions are okay */
  if (g_access (dir_in_locale, F_OK) == 0 && g_access (dir_in_locale, R_OK) == 0) {

    monitor_handle[dir_id] = CreateFile( dir_in_locale, FILE_LIST_DIRECTORY, 
					 FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, 
					 NULL, OPEN_EXISTING, 
					 FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, 
					 NULL );

    if (monitor_handle[dir_id] == INVALID_HANDLE_VALUE) {
      free(dir);
    } else if (monitor_dir(dir_id)) {  
      g_hash_table_insert (watch_table, g_strdup (dir), monitor_handle[dir_id]);
      strcpy(dirs[dir_id], dir);

      g_free (dir_in_locale);
      free(dir);

      ++dir_id;
    }
  } else {

    g_free (dir_in_locale);
    free(dir);
  }
}

gboolean
tracker_add_watch_dir (const char *dir, DBConnection *db_con)
{
  // create a copy so that the string is still available when we are called in the file notify thread
  char *something = g_strdup(dir);
  return QueueUserAPC( tracker_add_watch_dir_wrapped, sleep_thread, (DWORD) something );
}

void
tracker_remove_watch_dir (const char *dir, gboolean delete_subdirs, DBConnection *db_con)
{
#if 0
  char *something = g_strdup(dir);
  QueueUserAPC( tracker_remove_watch_dir_wrapped, sleep_thread, (DWORD) something );
#endif
}

gboolean
tracker_is_directory_watched (const char *dir, DBConnection *db_con)
{
  if (!dir) {
    return FALSE;
  }

  return (g_hash_table_lookup (watch_table, dir) != NULL);
}

int
tracker_count_watch_dirs (void)
{
  return g_hash_table_size (watch_table);
}
