/* Tracker 
 * utility routines
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

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <glib/gprintf.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include "tracker-utils.h"
#include "xdgmime.h"

GMutex 	*log_access_mutex;
char 	*log_file; 

/* configuration options table */
static 	GHashTable	*config_table;

static int info_allocated = 0;
static int info_deallocated = 0;

/* global config variables */
GSList 		*watch_directory_roots_list;
GSList 		*no_watch_directory_list;
gboolean	index_text_files;
gboolean	index_documents;
gboolean	index_source_code;
gboolean	index_scripts;
gboolean	index_html;
gboolean	index_pdf;
gboolean	index_application_help_files;
gboolean	index_desktop_files;
gboolean	index_epiphany_bookmarks;
gboolean	index_epiphany_history;
gboolean	index_firefox_bookmarks;
gboolean	index_firefox_history;
gboolean	store_text_file_contents_in_db;
char		*db_buffer_memory_limit;

void
tracker_print_object_allocations ()
{
	tracker_log ("total allocations = %d , total deallocations = %d", info_allocated, info_deallocated);
}

gboolean	
tracker_file_info_is_valid (FileInfo *info) {

	if (!info || !info->uri) {

		tracker_log ("************** Warning Invalid Info struct detected *****************");

		return FALSE;

	} else {
	
		if ( !g_utf8_validate (info->uri, -1, NULL) || info->action == TRACKER_ACTION_IGNORE) {
			
			if (info->action != TRACKER_ACTION_IGNORE) {
				tracker_log ("************** Warning UTF8 Validation of FileInfo URI has failed (possible corruption) *****************");
			}
			
			info = tracker_free_file_info (info);

			return FALSE;
		}
	}
	
	return TRUE;
} 

FileInfo *
tracker_create_file_info (const char *uri, TrackerChangeAction action, int counter, WatchTypes watch)
{
	FileInfo *info;

	info = g_new (FileInfo, 1);

	info->action = action;
	info->uri = g_strdup (uri);

	info->counter = counter;
	info->file_id = -1;

	info->file_type = FILE_ORIDNARY;

	info->watch_type = watch;
	info->is_directory = FALSE;

	info->is_link = FALSE;
	info->link_id = -1;
	info->link_path = NULL;
	info->link_name = NULL;

	info->is_moved = FALSE;
	info->move_path = NULL;
	info->move_name = NULL;

	info->mime = NULL;
	info->file_size = 0;
	info->permissions = g_strdup ("-r--r--r--");;
	info->mtime = 0;
	info->atime = 0;
	info->indextime = 0;

	info->ref_count = 1;
	info_allocated ++;
	return info;
}


FileInfo * 
tracker_free_file_info (FileInfo *info)
{

	if (!info) {
		return NULL;
	}

	if (info->uri) {
		g_free (info->uri);
	}

	if (info->link_path) {
		g_free (info->link_path);
	}

	if (info->link_name) {
		g_free (info->link_name);
	}

	if (info->mime) {
		g_free (info->mime);
	}

	if (info->move_path) {
		g_free (info->move_path);
	}

	if (info->move_name) {
		g_free (info->move_name);
	}

	if (info->permissions) {
		g_free (info->permissions);
	}

	g_free (info);

	info_deallocated ++;

	return NULL;

}

/* ref count FileInfo instances */
FileInfo *
tracker_inc_info_ref (FileInfo *info)
{
	if (info) {
		g_atomic_int_inc (&info->ref_count);
	}
	return info;
}

FileInfo *
tracker_dec_info_ref (FileInfo *info)
{
	if (!info) {
		return NULL;
	}

	if g_atomic_int_dec_and_test (&info->ref_count) {
		tracker_free_file_info (info);
		return NULL;
	}

	return info;
}


FileInfo *
tracker_get_pending_file_info (long file_id, const char *uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory)
{
	FileInfo *info;

	info = g_new (FileInfo, 1);

	info->action = action;
	info->uri = g_strdup (uri);

	info->counter = counter;
	info->file_id = file_id;

	info->file_type = FILE_ORIDNARY;

	info->is_directory = is_directory;

	info->is_link = FALSE;
	info->link_id = -1;
	info->link_path = NULL;
	info->link_name = NULL;

	info->is_moved = FALSE;
	info->move_path = NULL;
	info->move_name = NULL;

	if (mime) {
		info->mime = g_strdup (mime);
	} else {
		info->mime = NULL;
	}

	info->file_size = 0;
	info->permissions = g_strdup ("-r--r--r--");;
	info->mtime = 0;
	info->atime = 0;
	info->indextime = 0;

	info->ref_count = 1;
	info_allocated ++;
	return info;
}


FileInfo *
tracker_get_file_info (FileInfo *info)
{
	struct stat     finfo;
	char   		*str = NULL, *link_uri;
	int    		n, bit;


	if (!info || !info->uri) {
		return info;
	}

	if (lstat (info->uri, &finfo) == -1){
		return info;
	}

	info->is_directory = S_ISDIR (finfo.st_mode);	 
	info->is_link = S_ISLNK (finfo.st_mode);
	
	if (info->is_link && !info->link_name) {
		str = g_file_read_link (info->uri, NULL);
		if (str) {
			link_uri = g_filename_to_utf8 (str, -1, NULL, NULL, NULL);
			info->link_name = g_path_get_basename (link_uri);
			info->link_path = g_path_get_dirname (link_uri);
			g_free (link_uri);
			g_free (str); 
		}
	}

	if (!info->is_directory) {
		info->file_size =  (long)finfo.st_size;
	} else {
		if (info->watch_type == WATCH_OTHER) {
			info->watch_type = WATCH_SUBFOLDER;
		}
	}

	
	
	/* create permissions string */
	str = g_strdup ("?rwxrwxrwx");
  	
	switch (finfo.st_mode & S_IFMT) {
		case S_IFSOCK: str[0] = 's'; break;
		case S_IFIFO: str[0] = 'p'; break;
		case S_IFLNK: str[0] = 'l'; break;
		case S_IFCHR: str[0] = 'c'; break;
		case S_IFBLK: str[0] = 'b'; break;
		case S_IFDIR: str[0] = 'd'; break;
		case S_IFREG: str[0] = '-'; break;
	}

	for (bit = 0400, n = 1 ; bit ; bit >>= 1, ++n) {
		if (!(finfo.st_mode & bit)) {
			str[n] = '-'; 
		}
	}

	if (finfo.st_mode & S_ISUID) {
		str[3] = (finfo.st_mode & S_IXUSR) ? 's' : 'S';
	}

	if (finfo.st_mode & S_ISGID) {
		str[6] = (finfo.st_mode & S_IXGRP) ? 's' : 'S';
	}

	if (finfo.st_mode & S_ISVTX) {
		str[9] = (finfo.st_mode & S_IXOTH) ? 't' : 'T'; 
	}

	g_free (info->permissions);
	info->permissions = str;

	info->mtime =  finfo.st_mtime;
	info->atime =  finfo.st_atime;

	return info;
}



static gboolean
is_text_file  (const char* uri)
{
	char buffer[32];
	FILE* file = NULL;
	int bytes_read;

	file = g_fopen (uri, "r");
	
	if (file != NULL) {
	
		bytes_read = fread (buffer, 1, 32, file);
		fclose (file);
		if (bytes_read < 0) {
			return FALSE;
		}
		return g_utf8_validate ( (gchar *)buffer, 32, NULL);
		
	}

	return FALSE;
}
 

gboolean 
tracker_file_is_valid (const char *uri)
{
	
	return g_file_test (uri, G_FILE_TEST_EXISTS);

}

char *
tracker_get_mime_type (const char* uri)
{
	const char *result;

	if (!tracker_file_is_valid (uri)) {
		return g_strdup("unknown");
	}

	result = xdg_mime_get_mime_type_for_file (uri);

	if (result != NULL && result != XDG_MIME_TYPE_UNKNOWN) {
		return g_strdup (result);
	} else {
		if (is_text_file (uri)) {
			return g_strdup ("text/plain");
		}
	}
	return g_strdup("unknown");
}

gboolean 
tracker_is_directory (const char *dir) 
{
	struct stat finfo;

	lstat (dir, &finfo);
	return S_ISDIR (finfo.st_mode); 

}

void 
tracker_log (const char* fmt, ...) 
{
	
	FILE *fd;
	time_t now;
	char buffer[64], buffer2[20];
	char *output;
 	char *msg;
    	va_list args;
	struct tm *loctime;
	GTimeVal start;
  
  	va_start (args, fmt);
  	msg = g_strdup_vprintf (fmt, args);
  	va_end (args);
	
	if (msg) {
		g_print ("%s\n", msg);
	}

	/* ensure file logging is thread safe */
	g_mutex_lock (log_access_mutex);

	fd = fopen (log_file, "a");

	if (!fd) {
		g_mutex_unlock (log_access_mutex);
		g_warning ("could not open %s", log_file);
		g_free (msg);
		return;
	}

        g_get_current_time (&start);
    	now = time((time_t *)NULL);
	loctime = localtime (&now);
	strftime (buffer, 64, "%d %b %Y, %H:%M:%S:", loctime);
	g_sprintf (buffer2, "%ld", start.tv_usec / 1000);
	output = g_strconcat (buffer, buffer2,  " - ", msg, NULL);
	fprintf(fd,"%s\n",output);
	g_free (msg);
	g_free (output);
	fclose (fd);
	g_mutex_unlock (log_access_mutex);

}

GSList *
tracker_get_files (const char *dir, gboolean dir_only) 
{

	DIR *dirp;
	struct dirent *entry;
	GSList *file_list = NULL;

   	if ((dirp = opendir (dir)) != NULL) {
   		while ((entry = readdir (dirp)) != NULL) {

			char  *mystr = NULL, *str = NULL;

     			if (entry->d_name[0] == '.') {
       				continue;
			}

			str = g_filename_to_utf8 (entry->d_name, -1, NULL, NULL, NULL);

			if (!str) {
				continue;
			}

			mystr = g_strconcat (dir,"/", str , NULL);
			g_free (str);
		
     			if (!dir_only || (dir_only && tracker_is_directory (mystr))) {
				file_list = g_slist_prepend (file_list, g_strdup (mystr));
			}
			
			g_free (mystr);

		}
		g_free (entry);
 		closedir (dirp);
	}
	return file_list;
}			

void 
tracker_get_dirs (const char *dir, GSList **file_list) 
{
	GSList *tmp_list;

	if (!dir) {
		return;
	}

	tmp_list = tracker_get_files (dir, TRUE);
	if (g_slist_length (tmp_list) > 0) {
		if (g_slist_length (*file_list) > 0) {
			*file_list = g_slist_concat (*file_list, tmp_list);
		} else {
			*file_list = tmp_list;
		}
	}
}

static GSList *
array_to_list (char **array) 
{
	GSList *list = NULL;
	int i;

	for (i = 0; array[i] != NULL; i++) {
		list = g_slist_prepend	(list, g_strdup (array[i]));
	}
		
	g_strfreev (array);
	return list;

}



void
tracker_load_config_file ()
{
	GKeyFile *key_file;
	char *filename;

	key_file = g_key_file_new ();

	filename = g_build_filename (g_get_home_dir (), ".Tracker", "tracker.cfg", NULL);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		char *contents;
		contents  = g_strconcat ("[Watches]\n", 
					 "WatchDirectoryRoots=", g_get_home_dir (), ";\n",
					 "NoWatchDirectory=\n\n\n",
					 "[Indexes]\n"
					 "IndexTextFiles=true\n",
					 "IndexDocuments=true\n",
					 "IndexSourceCode=true\n",
					 "IndexScripts=true\n",
					 "IndexHTML=true\n",
					 "IndexPDF=true\n",
					 "IndexApplicationHelpFiles=true\n",
					 "IndexDesktopFiles=true\n",
					 "IndexEpiphanyBookmarks=true\n"
					 "IndexEpiphanyHistory=true\n",
					 "IndexFirefoxBookmarks=true\n",
					 "IndexFirefoxHistory=true\n\n",
					 "[Database]\n",
					 "StoreTextFileContentsInDB=false\n",					
					 "DBBufferMemoryLimit=1M\n", NULL);
		
		g_file_set_contents (filename, contents, strlen (contents), NULL);
		g_free (contents);
	}	

	/* load all options into hashtable for fast retrieval */
	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL);

	char **values =  g_key_file_get_string_list ( key_file,
				       	     	      "Watches",
					              "WatchDirectoryRoots",
					              NULL,
					              NULL);

	if (values) {
		watch_directory_roots_list = array_to_list (values);
	} else {
		watch_directory_roots_list = g_slist_prepend (watch_directory_roots_list, g_strdup (g_get_home_dir ()));
	}
	

	values =  g_key_file_get_string_list ( key_file,
			       	     	      "Watches",
				              "NoWatchDirectory",
				              NULL,
				              NULL);

	if (values) {
		no_watch_directory_list = array_to_list (values);
	} else {
		no_watch_directory_list = NULL;
	}
	
	if (g_key_file_has_key (key_file, "Indexes", "IndexTextFiles", NULL)) {
		index_text_files = g_key_file_get_boolean (key_file, "Indexes", "IndexTextFiles", NULL);
	}
	
	if (g_key_file_has_key (key_file, "Indexes", "IndexDocuments", NULL)) {
		index_documents = g_key_file_get_boolean (key_file, "Indexes", "IndexDocuments", NULL);
	}

	if (g_key_file_has_key (key_file, "Indexes", "IndexSourceCode", NULL)) {
		index_source_code = g_key_file_get_boolean (key_file, "Indexes", "IndexSourceCode", NULL);
	}

	if (g_key_file_has_key (key_file, "Indexes", "IndexScripts", NULL)) {
		index_scripts = g_key_file_get_boolean (key_file, "Indexes", "IndexScripts", NULL);
	}

	if (g_key_file_has_key (key_file, "Indexes", "IndexHTML", NULL)) {
		index_html = g_key_file_get_boolean (key_file, "Indexes", "IndexHTML", NULL);
	}

	if (g_key_file_has_key (key_file, "Indexes", "IndexPDF", NULL)) {
		index_pdf = g_key_file_get_boolean (key_file, "Indexes", "IndexPDF", NULL);
	}

	if (g_key_file_has_key (key_file, "Indexes", "IndexApplicationHelpFiles", NULL)) {
		index_application_help_files = g_key_file_get_boolean (key_file, "Indexes", "IndexApplicationHelpFiles", NULL);
	}

	if (g_key_file_has_key (key_file, "Indexes", "IndexDesktopFiles", NULL)) {
		index_desktop_files = g_key_file_get_boolean (key_file, "Indexes", "IndexDesktopFiles", NULL);
	}

	if (g_key_file_has_key (key_file, "Indexes", "IndexEpiphanyBookmarks", NULL)) {
		index_epiphany_bookmarks = g_key_file_get_boolean (key_file, "Indexes", "IndexEpiphanyBookmarks", NULL);
	}

	if (g_key_file_has_key (key_file, "Indexes", "IndexEpiphanyHistory", NULL)) {
		index_epiphany_history = g_key_file_get_boolean (key_file, "Indexes", "IndexEpiphanyHistory", NULL);
	}

	if (g_key_file_has_key (key_file, "Indexes", "IndexFirefoxBookmarks", NULL)) {
		index_firefox_bookmarks = g_key_file_get_boolean (key_file, "Indexes", "IndexFirefoxBookmarks", NULL);
	}

	if (g_key_file_has_key (key_file, "Indexes", "IndexFirefoxHistory", NULL)) {
		index_firefox_history = g_key_file_get_boolean (key_file, "Indexes", "IndexFirefoxHistory", NULL);
	}

	if (g_key_file_has_key (key_file, "Database", "StoreTextFileContentsInDB", NULL)) {
		store_text_file_contents_in_db = g_key_file_get_boolean (key_file, "Indexes", "StoreTextFileContentsInDB", NULL);
	}

	db_buffer_memory_limit = g_key_file_get_string ( key_file, "Database", "DBBufferMemoryLimit", NULL);

	g_free (filename);
	g_key_file_free (key_file);
}
