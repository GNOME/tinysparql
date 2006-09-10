/* Tracker
 * utility routines
 * Copyright (C) 2005, Mr Jamie McCracken
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <glib/gprintf.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include "tracker-dbus.h"
#include "tracker-utils.h"
#include "xdgmime.h"


extern Tracker	*tracker;

char *implemented_services[] = {"Files", "Folders", "Documents", "Images", "Music", "Videos", "Text Files", "Development Files", "Other Files",
				"VFS Files", "VFS Folders", "VFS Documents", "VFS Images", "VFS Music", "VFS Videos", "VFS Text Files", "VFS Development Files", "VFS Other Files",
				NULL};

char *file_service_array[] =   {"Files", "Folders", "Documents", "Images", "Music", "Videos", "Text Files", "Development Files", "Other Files",
				"VFS Files", "VFS Folders", "VFS Documents", "VFS Images", "VFS Music", "VFS Videos", "VFS Text Files", "VFS Development Files", "VFS Other Files",
				NULL};

char *serice_index_array[] = {	"Files", "Folders", "Documents", "Images", "Music", "Videos", "Text Files", "Development Files", "Other Files",
				"VFS Files", "VFS Folders", "VFS Documents", "VFS Images", "VFS Music", "VFS Videos", "VFS Text Files", "VFS Development Files", "VFS Other Files",
				"Conversations", "Playlists", "Applications", "Contacts", "Emails", "EmailAttachments", "Notes", "Appointments",
				"Tasks", "Bookmarks", "History", "Projects", NULL};


char *tracker_actions[] = {
		"TRACKER_ACTION_IGNORE", "TRACKER_ACTION_CHECK", "TRACKER_ACTION_DELETE", "TRACKER_ACTION_DELETE_SELF", "TRACKER_ACTION_CREATE","TRACKER_ACTION_MOVED_FROM",
		"TRACKER_ACTION_MOVED_TO","TRACKER_ACTION_FILE_CHECK", "TRACKER_ACTION_FILE_CHANGED","TRACKER_ACTION_FILE_DELETED", "TRACKER_ACTION_FILE_CREATED",
		"TRACKER_ACTION_FILE_MOVED_FROM", "TRACKER_ACTION_FILE_MOVED_TO", "TRACKER_ACTION_WRITABLE_FILE_CLOSED","TRACKER_ACTION_DIRECTORY_CHECK",
		"TRACKER_ACTION_DIRECTORY_CREATED","TRACKER_ACTION_DIRECTORY_DELETED","TRACKER_ACTION_DIRECTORY_MOVED_FROM","TRACKER_ACTION_DIRECTORY_MOVED_TO",
		"TRACKER_ACTION_DIRECTORY_REFRESH", "TRACKER_ACTION_EXTRACT_METADATA",
		NULL};


static int info_allocated = 0;
static int info_deallocated = 0;


static const char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char imonths[] = {
	'1', '2', '3', '4', '5',
	'6', '7', '8', '9', '0', '1', '2'
};


char **
tracker_make_array_null_terminated (char **array, int length)
{
	char **res = NULL;
	int  i;

	res = g_new (char *, length +1);

	for (i = 0; i < length; i++) {
		res[i] = array[i];
	}

	res[length] = NULL;

	return res;
}


static gboolean
is_int (const char *in)
{
	int i, len;

	if (!in) {
		return FALSE;
	}

	len = strlen (in);

	if (len < 1) {
		return FALSE;
	}

	for (i = 0; i < len; i++) {

		if ( !g_ascii_isdigit (in[i]) ) {
			return FALSE;
		}
	}

	return TRUE;
}


static int
parse_month (const char *month)
{
	int i;

	for (i = 0; i < 12; i++) {
		if (!strncmp (month, months[i], 3))
			return i;
	}
	return -1;
}


/* determine date format and convert to ISO 8601 format*/

char *
tracker_format_date (const char *timestamp)
{
	char tmp_buf[30];
	int  len;

	if (!timestamp) {
		return NULL;
	}

	len = strlen (timestamp);

	/* we cannot format a date without at least a four digit year */
	if (len < 4) {
		return NULL;
	}

	/* check for year only dates (EG ID3 music tags might have Auido.ReleaseDate as 4 digit year) */

	if (len == 4) {
		if (is_int (timestamp)) {

			tmp_buf[0] = timestamp[0];
			tmp_buf[1] = timestamp[1];
			tmp_buf[2] = timestamp[2];
			tmp_buf[3] = timestamp[3];
			tmp_buf[4] = '-';
			tmp_buf[5] = '0';
			tmp_buf[6] = '1';
			tmp_buf[7] = '-';
			tmp_buf[8] = '0';
			tmp_buf[9] = '1';
			tmp_buf[10] = 'T';
			tmp_buf[11] = '0';
			tmp_buf[12] = '0';
			tmp_buf[13] = ':';
			tmp_buf[14] = '0';
			tmp_buf[15] = '0';
			tmp_buf[16] = ':';
			tmp_buf[17] = '0';
			tmp_buf[18] = '0';
			tmp_buf[19] = '\0';

			return g_strdup (tmp_buf);

		} else {
			return NULL;
		}

	/* check for date part only YYYY-MM-DD*/

	} else if (len == 10)  {

			tmp_buf[0] = timestamp[0];
			tmp_buf[1] = timestamp[1];
			tmp_buf[2] = timestamp[2];
			tmp_buf[3] = timestamp[3];
			tmp_buf[4] = '-';
			tmp_buf[5] = timestamp[5];
			tmp_buf[6] = timestamp[6];
			tmp_buf[7] = '-';
			tmp_buf[8] = timestamp[8];
			tmp_buf[9] = timestamp[9];
			tmp_buf[10] = 'T';
			tmp_buf[11] = '0';
			tmp_buf[12] = '0';
			tmp_buf[13] = ':';
			tmp_buf[14] = '0';
			tmp_buf[15] = '0';
			tmp_buf[16] = ':';
			tmp_buf[17] = '0';
			tmp_buf[18] = '0';
			tmp_buf[19] = '\0';

			return g_strdup (tmp_buf);

	/* check for pdf format EG 20050315113224-08'00' or 20050216111533Z  */

	} else if (len == 14) {

			tmp_buf[0] = timestamp[0];
			tmp_buf[1] = timestamp[1];
			tmp_buf[2] = timestamp[2];
			tmp_buf[3] = timestamp[3];
			tmp_buf[4] = '-';
			tmp_buf[5] = timestamp[4];
			tmp_buf[6] = timestamp[5];
			tmp_buf[7] = '-';
			tmp_buf[8] = timestamp[6];
			tmp_buf[9] = timestamp[7];
			tmp_buf[10] = 'T';
			tmp_buf[11] = timestamp[8];
			tmp_buf[12] = timestamp[9];
			tmp_buf[13] = ':';
			tmp_buf[14] = timestamp[10];
			tmp_buf[15] = timestamp[11];
			tmp_buf[16] = ':';
			tmp_buf[17] = timestamp[12];
			tmp_buf[18] = timestamp[13];
			tmp_buf[19] = '\0';

			return g_strdup (tmp_buf);


	} else if (len == 15 && timestamp[14] == 'Z') {

			tmp_buf[0] = timestamp[0];
			tmp_buf[1] = timestamp[1];
			tmp_buf[2] = timestamp[2];
			tmp_buf[3] = timestamp[3];
			tmp_buf[4] = '-';
			tmp_buf[5] = timestamp[4];
			tmp_buf[6] = timestamp[5];
			tmp_buf[7] = '-';
			tmp_buf[8] = timestamp[6];
			tmp_buf[9] = timestamp[7];
			tmp_buf[10] = 'T';
			tmp_buf[11] = timestamp[8];
			tmp_buf[12] = timestamp[9];
			tmp_buf[13] = ':';
			tmp_buf[14] = timestamp[10];
			tmp_buf[15] = timestamp[11];
			tmp_buf[16] = ':';
			tmp_buf[17] = timestamp[12];
			tmp_buf[18] = timestamp[13];
			tmp_buf[19] = 'Z';
			tmp_buf[20] = '\0';

			return g_strdup (tmp_buf);


	} else if (len == 21 && (timestamp[14] == '-' || timestamp[14] == '+' )) {

			tmp_buf[0] = timestamp[0];
			tmp_buf[1] = timestamp[1];
			tmp_buf[2] = timestamp[2];
			tmp_buf[3] = timestamp[3];
			tmp_buf[4] = '-';
			tmp_buf[5] = timestamp[4];
			tmp_buf[6] = timestamp[5];
			tmp_buf[7] = '-';
			tmp_buf[8] = timestamp[6];
			tmp_buf[9] = timestamp[7];
			tmp_buf[10] = 'T';
			tmp_buf[11] = timestamp[8];
			tmp_buf[12] = timestamp[9];
			tmp_buf[13] = ':';
			tmp_buf[14] = timestamp[10];
			tmp_buf[15] = timestamp[11];
			tmp_buf[16] = ':';
			tmp_buf[17] = timestamp[12];
			tmp_buf[18] = timestamp[13];
			tmp_buf[19] = timestamp[14];
			tmp_buf[20] = timestamp[15];
			tmp_buf[21] =  timestamp[16];
			tmp_buf[22] =  ':';
			tmp_buf[23] =  timestamp[18];
			tmp_buf[24] = timestamp[19];
			tmp_buf[25] = '\0';

			return g_strdup (tmp_buf);

	/* check for msoffice date format "Mon Feb  9 10:10:00 2004" */

	} else if ((len == 24) && (timestamp[3] == ' ')) {
			int  num_month;
			char mon1;
			char day1;

			num_month = parse_month (timestamp + 4);

			mon1 = imonths[num_month];

			if (timestamp[8] == ' ') {
				day1 = '0';
			} else {
				day1 = timestamp[8];
			}

			tmp_buf[0] = timestamp[20];
			tmp_buf[1] = timestamp[21];
			tmp_buf[2] = timestamp[22];
			tmp_buf[3] = timestamp[23];
			tmp_buf[4] = '-';

			if (num_month < 10) {
				tmp_buf[5] = '0';
				tmp_buf[6] = mon1;
			} else {
				tmp_buf[5] = '1';
				tmp_buf[6] = mon1;
			}

			tmp_buf[7] = '-';
			tmp_buf[8] = day1;
			tmp_buf[9] = timestamp[9];
			tmp_buf[10] = 'T';
			tmp_buf[11] = timestamp[11];
			tmp_buf[12] = timestamp[12];
			tmp_buf[13] = ':';
			tmp_buf[14] = timestamp[14];
			tmp_buf[15] = timestamp[15];
			tmp_buf[16] = ':';
			tmp_buf[17] = timestamp[17];
			tmp_buf[18] = timestamp[18];
			tmp_buf[19] = '\0';

			return g_strdup (tmp_buf);

	/* check for Exif date format "2005:04:29 14:56:54" */

	} else if ((len == 19) && (timestamp[4] == ':') && (timestamp[7] == ':')) {

			tmp_buf[0] = timestamp[0];
			tmp_buf[1] = timestamp[1];
			tmp_buf[2] = timestamp[2];
			tmp_buf[3] = timestamp[3];
			tmp_buf[4] = '-';
			tmp_buf[5] = timestamp[5];
			tmp_buf[6] = timestamp[6];
			tmp_buf[7] = '-';
			tmp_buf[8] = timestamp[8];
			tmp_buf[9] = timestamp[9];
			tmp_buf[10] = 'T';
			tmp_buf[11] = timestamp[11];
			tmp_buf[12] = timestamp[12];
			tmp_buf[13] = ':';
			tmp_buf[14] = timestamp[14];
			tmp_buf[15] = timestamp[15];
			tmp_buf[16] = ':';
			tmp_buf[17] = timestamp[17];
			tmp_buf[18] = timestamp[18];
			tmp_buf[19] = '\0';

			return g_strdup (tmp_buf);
	}

	return g_strdup (timestamp);
}


static gboolean
is_valid_8601_datetime (const char *timestamp)
{
	int len;

	len = strlen (timestamp);

	if (len < 19) {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[0]) ) {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[1]) ) {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[2]) ) {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[3]) ) {
		return FALSE;
	}

	if (timestamp[4] != '-') {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[5]) ) {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[6]) ) {
		return FALSE;
	}

	if (timestamp[7] != '-') {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[8]) ) {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[9]) ) {
		return FALSE;
	}

	if ( (timestamp[10] != 'T') ) {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[11]) ) {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[12]) ) {
		return FALSE;
	}

	if (timestamp[13] != ':') {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[14]) ) {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[15]) ) {
		return FALSE;
	}

	if (timestamp[16] != ':'){
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[17]) ) {
		return FALSE;
	}

	if ( !g_ascii_isdigit (timestamp[18]) ){
		return FALSE;
	}

	if (len == 20) {
		if (timestamp[19] != 'Z') {
			return FALSE;
		}
	} else {

		if (len > 20) {

			/* format must be YYYY-MM-DDThh:mm:ss+xx  or YYYY-MM-DDThh:mm:ss+xx:yy */

			if (len != 22 || len != 25) {
				return FALSE;
			}

			if ( (timestamp[19] != '+') || (timestamp[19] != '-') ) {
				return FALSE;
			}

			if ( !g_ascii_isdigit (timestamp[20]) ) {
				return FALSE;
			}

			if ( !g_ascii_isdigit (timestamp[21]) ) {
				return FALSE;
			}
		}
	}

	return TRUE;
}


time_t
tracker_str_to_date (const char *timestamp)
{
	struct tm	tm;
	long		val;
	time_t		tt;
	gboolean	has_time_zone;

	has_time_zone = FALSE;

	if (!timestamp) {
		return -1;
	}

	/* we should have a valid iso 8601 date in format YYYY-MM-DDThh:mm:ss with optional TZ*/

	if (!is_valid_8601_datetime (timestamp)) {
		return -1;
	}

	val = strtoul (timestamp, (char **)&timestamp, 10);

	if (*timestamp == '-') {
		// YYYY-MM-DD
		tm.tm_year = val - 1900;
		timestamp++;
		tm.tm_mon = strtoul (timestamp, (char **)&timestamp, 10) -1;

		if (*timestamp++ != '-') {
			return -1;
		}

		tm.tm_mday = strtoul (timestamp, (char **)&timestamp, 10);
	}

	if (*timestamp++ != 'T') {
		tracker_log ("date validation failed for %s st %c", timestamp, *timestamp);
		return -1;
	}

	val = strtoul (timestamp, (char **)&timestamp, 10);

	if (*timestamp == ':') {
		// hh:mm:ss
		tm.tm_hour = val;
		timestamp++;
		tm.tm_min = strtoul (timestamp, (char **)&timestamp, 10);
		if (*timestamp++ != ':') {

			return -1;
		}
		tm.tm_sec = strtoul (timestamp, (char **)&timestamp, 10);
	}

	tt = mktime (&tm);

	if (*timestamp == '+' || *timestamp == '-') {
		int sign, num_length;

		has_time_zone = TRUE;

		sign = (*timestamp == '+') ? -1 : 1;

		num_length = (int) timestamp + 1;

		val = strtoul (timestamp +1, (char **)&timestamp, 10);

		num_length = (int) (timestamp - num_length);

		if (*timestamp == ':' || *timestamp == '\'') {
			val = 3600 * val + (60 * strtoul (timestamp + 1, NULL, 10));
		} else {
			if (num_length == 4) {
				val = (3600 * (val / 100)) + (60 * (val % 100));
			} else if (num_length == 2) {
				val = 3600 * val;
			}
		}
		tt += sign * val;
	} else {
		if (*timestamp == 'Z') {
			/* no need to do anything if utc */
			has_time_zone = TRUE;
		}
	}

	/* make datetime reflect user's timezone if no explicit timezone present */
	if (!has_time_zone) {
		tt += timezone;
	}

	return tt;
}


char *
tracker_date_to_str (long date_time)
{
	char  		buffer[30];
	time_t 		time_stamp;
	struct tm 	loctime;
	size_t		count;

	memset (buffer, '\0', sizeof (buffer));
	memset (&loctime, 0, sizeof (struct tm));

	time_stamp = (time_t) date_time;

	localtime_r (&time_stamp, &loctime);

	/* output is ISO 8160 format : "YYYY-MM-DDThh:mm:ss+zz:zz" */
	count = strftime (buffer, sizeof (buffer), "%FT%T%z", &loctime);

	if (count > 0) {
		return g_strdup (buffer);
	} else {
		return NULL;
	}
}


char *
tracker_int_to_str (int i)
{
	return g_strdup_printf ("%d", i);
}


char *
tracker_long_to_str (long i)
{
	return g_strdup_printf ("%ld", i);
}


int
tracker_str_in_array (const char *str, char **array)
{
	int  i;
	char *st;

	i = 0;

	for (st = (char *) *array; *st; st++) {
		if (strcmp (st, str) == 0) {
			return TRUE;
		}
		i++;
	}

	return -1;
}


char *
tracker_format_search_terms (const char *str, gboolean *do_bool_search)
{
	char *def_prefix;
	char **terms;

	*do_bool_search = FALSE;

	def_prefix = "+";

	if (strlen (str) < 3) {
		return g_strdup (str);
	}

	/* if already has quotes then do nothing */
	if (strchr (str, '"') || strchr (str, '*')) {
		*do_bool_search = TRUE;
		return g_strdup (str);
	}

	if (strstr (str, " or ")) {
		def_prefix = " ";
	}

	terms = g_strsplit (str, " ", -1);

	if (terms) {
		GString *search_term;
		char	**st;
		char	*prefix;

		search_term = g_string_new (" ");

		for (st = terms; *st; st++) {

			if (*st[0] == '-') {
				prefix = " ";
			} else {
				prefix = def_prefix;
			}

			if ((*st[0] != '-') && strchr (*st, '-')) {
				char *s;

				*do_bool_search = TRUE;

				s = g_strconcat ("\"", *st, "\"", NULL);

				g_string_append (search_term, s);

				g_free (s);

			} else {
				g_string_append_printf (search_term, " %s%s ", prefix, *st);
			}
		}

		g_strfreev (terms);

		return g_string_free (search_term, FALSE);
	}

	return g_strdup (str);
}


void
tracker_print_object_allocations ()
{
	tracker_log ("total allocations = %d, total deallocations = %d", info_allocated, info_deallocated);
}


gboolean
tracker_file_info_is_valid (FileInfo *info)
{
	if (!info || !info->uri) {

		tracker_log ("************** Warning Invalid Info struct detected *****************");

		return FALSE;

	} else {

		if ( !g_utf8_validate (info->uri, -1, NULL) || info->action == TRACKER_ACTION_IGNORE) {

			if (info->action != TRACKER_ACTION_IGNORE) {
				tracker_log ("************** Warning UTF8 Validation of FileInfo URI has failed (possible corruption) *****************");
			}

			tracker_free_file_info (info);

			return FALSE;
		}
	}

	return TRUE;
}


void
tracker_free_array (char **array, int row_count)
{
	if (array && (row_count > 0)) {
		int i;

		for (i = 0; i < row_count; i++) {
			if (array[i]) {
	        		g_free (array[i]);
			}
		}

		g_free (array);
	}
}


FileInfo *
tracker_create_file_info (const char *uri, TrackerChangeAction action, int counter, WatchTypes watch)
{
	FileInfo *info;

	info = g_slice_new (FileInfo);

	info->action = action;
	info->uri = g_strdup (uri);

	info->counter = counter;
	info->file_id = -1;

	info->file_type = FILE_ORDINARY;

	info->watch_type = watch;
	info->is_directory = FALSE;

	info->is_link = FALSE;
	info->link_id = -1;
	info->link_path = NULL;
	info->link_name = NULL;

	info->mime = NULL;
	info->file_size = 0;
	info->permissions = g_strdup ("-r--r--r--");
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

	if (info->permissions) {
		g_free (info->permissions);
	}

	g_slice_free (FileInfo, info);

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

	info = g_slice_new (FileInfo);

	info->action = action;
	info->uri = g_strdup (uri);

	info->counter = counter;
	info->file_id = file_id;

	info->file_type = FILE_ORDINARY;

	info->is_directory = is_directory;

	info->is_link = FALSE;
	info->link_id = -1;
	info->link_path = NULL;
	info->link_name = NULL;

	if (mime) {
		info->mime = g_strdup (mime);
	} else {
		info->mime = NULL;
	}

	info->file_size = 0;
	info->permissions = g_strdup ("-r--r--r--");
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
	char   		*uri_in_locale, *str;
	int    		n, bit;

	if (!info || !info->uri) {
		return info;
	}

	uri_in_locale = g_filename_from_utf8 (info->uri, -1, NULL, NULL, NULL);

	if (uri_in_locale) {
		if (g_lstat (uri_in_locale, &finfo) == -1) {
			g_free (uri_in_locale);

			return info;
		}

	} else {
		tracker_log ("******ERROR**** info->uri could not be converted to locale format");

		return NULL;
	}

	info->is_directory = S_ISDIR (finfo.st_mode);
	info->is_link = S_ISLNK (finfo.st_mode);

	if (info->is_link && !info->link_name) {
		str = g_file_read_link (uri_in_locale, NULL);

		if (str) {
			char *link_uri;

			link_uri = g_filename_to_utf8 (str, -1, NULL, NULL, NULL);
			info->link_name = g_path_get_basename (link_uri);
			info->link_path = g_path_get_dirname (link_uri);
			g_free (link_uri);
			g_free (str);
		}
	}

	g_free (uri_in_locale);

	if (!info->is_directory) {
		info->file_size = (long) finfo.st_size;
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
is_text_file (const char* uri)
{
	FILE *file;
	char *uri_in_locale;

	uri_in_locale = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (!uri_in_locale) {
		tracker_log ("******ERROR**** uri could not be converted to locale format");
		return FALSE;
	}

	file = g_fopen (uri_in_locale, "r");

	g_free (uri_in_locale);

	if (file) {
		char	 buffer[65566];
		gboolean data_read;

		if (fgets (buffer, 65565, file)) {
			data_read = TRUE;
		}

		fclose (file);

		if (data_read) {
			char *s;

			s = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);

			if (!s) {
				return FALSE;
			} else {
				gboolean ret;

				/* if file contains text then it will be in correct utf8 but
				   it isn't clear whether some binary files will be identified
				   as text files too if they aren't in UTF-8! */
				ret = g_utf8_validate (s, -1, NULL);

				g_free (s);

				return ret;
			}
		}
	}

	return FALSE;
}


gboolean
tracker_file_is_valid (const char *uri)
{
	gboolean convert_ok;
	char	 *uri_in_locale;

	uri_in_locale = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (!uri_in_locale) {
		tracker_log ("******ERROR**** uri could not be converted to locale format");
		return FALSE;
	}

	/* g_file_test(file,G_FILE_TEST_EXISTS) uses the access() system call and so needs locale filenames. */
	convert_ok = g_file_test (uri_in_locale, G_FILE_TEST_EXISTS);

	g_free (uri_in_locale);

	return convert_ok;
}


char *
tracker_get_mime_type (const char* uri)
{
	struct stat finfo;
	char	    *uri_in_locale;
	const char  *result;

	if (!tracker_file_is_valid (uri)) {
		return g_strdup ("unknown");
	}

	uri_in_locale = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (!uri_in_locale) {
		tracker_log ("******ERROR**** uri could not be converted to locale format");
		return FALSE;
	}

	g_lstat (uri_in_locale, &finfo);

	g_free (uri_in_locale);

	if (S_ISLNK (finfo.st_mode) && S_ISDIR (finfo.st_mode)) {
		return g_strdup ("symlink");
	}

	result = xdg_mime_get_mime_type_for_file (uri, NULL);

	if (result != NULL && result != XDG_MIME_TYPE_UNKNOWN) {
		return g_strdup (result);

	} else {
		if (is_text_file (uri)) {
			return g_strdup ("text/plain");
		}
	}

	return g_strdup ("unknown");
}


char *
tracker_get_vfs_path (const char* uri)
{
	if (uri != NULL && strchr (uri, G_DIR_SEPARATOR) != NULL) {
		char *p;
		int  len;

		len = strlen (uri);
		p = (char *) (uri + (len - 1));

		/* Skip trailing slash  */
		if (p != uri && *p == G_DIR_SEPARATOR) {
			p--;
		}

		/* Search backwards to the next slash.  */
		while (p != uri && *p != G_DIR_SEPARATOR) {
			p--;
		}

		if (p[0] != '\0') {

			char *new_uri_text;
			int  length;

			length = p - uri;

			if (length == 0) {
				new_uri_text = g_strdup (G_DIR_SEPARATOR_S);
			} else {
				new_uri_text = g_malloc (length + 1);
				memcpy (new_uri_text, uri, length);
				new_uri_text[length] = '\0';
			}

			return new_uri_text;
		} else {
			return g_strdup (G_DIR_SEPARATOR_S);
		}
	}

	return NULL;
}


char *
tracker_get_vfs_name (const char* uri)
{
	if (uri != NULL && strchr (uri, G_DIR_SEPARATOR) != NULL) {
		char *p, *res, *tmp;
		int  len;

		len = strlen (uri);

		tmp = g_strdup (uri);

		p = (tmp + (len - 1));

		/* Skip trailing slash  */
		if (p != tmp && *p == G_DIR_SEPARATOR) {
			*p = '\0';
		}

		/* Search backwards to the next slash.  */
		while (p != tmp && *p != G_DIR_SEPARATOR) {
			p--;
		}

		res = p+1;

		if (res && res[0] != '\0') {

			g_free (tmp);

			return g_strdup (res);
		}

		g_free (tmp);
	}

	return g_strdup (" ");
}


gboolean
tracker_is_directory (const char *dir)
{
	char *dir_in_locale;

	dir_in_locale = g_filename_from_utf8 (dir, -1, NULL, NULL, NULL);

	if (dir_in_locale) {
		struct stat finfo;

		g_lstat (dir_in_locale, &finfo);

		g_free (dir_in_locale);

		return S_ISDIR (finfo.st_mode);

	} else {
		tracker_log ("******ERROR**** dir could not be converted to locale format");

		return FALSE;
	}
}


void
tracker_log (const char* fmt, ...)
{
	FILE		*fd;
	time_t		now;
	char		buffer1[64], buffer2[20];
	char		*output;
 	char		*msg;
    	va_list		args;
	struct tm	*loctime;
	GTimeVal	start;

  	va_start (args, fmt);
  	msg = g_strdup_vprintf (fmt, args);
  	va_end (args);

	if (msg) {
		g_print ("%s\n", msg);
	}

	/* ensure file logging is thread safe */
	g_mutex_lock (tracker->log_access_mutex);

	fd = g_fopen (tracker->log_file, "a");

	if (!fd) {
		g_mutex_unlock (tracker->log_access_mutex);
		g_warning ("could not open %s", tracker->log_file);
		g_free (msg);
		return;
	}

        g_get_current_time (&start);

    	now = time ((time_t *) NULL);

	loctime = localtime (&now);

	strftime (buffer1, 64, "%d %b %Y, %H:%M:%S:", loctime);

	g_sprintf (buffer2, "%ld", start.tv_usec / 1000);

	output = g_strconcat (buffer1, buffer2, " - ", msg, NULL);
	g_free (msg);

	g_fprintf (fd, "%s\n", output);
	g_free (output);

	fclose (fd);

	g_mutex_unlock (tracker->log_access_mutex);
}


static int
has_prefix (const char *str1, const char *str2)
{
	if (strcmp (str1, str2) == 0) {
		return 0;
	} else {
		char *compare_str;

		compare_str = g_strconcat (str1, G_DIR_SEPARATOR_S, NULL);

		if (g_str_has_prefix (str2, compare_str)) {
			return 0;
		}
		g_free (compare_str);
		return 1;
	}
}


GSList *
tracker_get_files (const char *dir, gboolean dir_only)
{
	GDir	*dirp;
	GSList	*file_list;
	char	*dir_in_locale;

	dir_in_locale = g_filename_from_utf8 (dir, -1, NULL, NULL, NULL);

	if (!dir_in_locale) {
		tracker_log ("******ERROR**** dir could not be converted to utf8 format");
		g_free (dir_in_locale);
		return NULL;
	}

	file_list = NULL;

   	if ((dirp = g_dir_open (dir_in_locale, 0, NULL))) {
		const char *name;

   		while ((name = g_dir_read_name (dirp))) {
			char  *mystr, *str;

			if (!tracker->is_running) {
				g_free (dir_in_locale);
				g_dir_close (dirp);
				return NULL;
			}

			str = g_filename_to_utf8 (name, -1, NULL, NULL, NULL);

			if (!str) {
				continue;
			}

			if (tracker_ignore_file (str)) {
				g_free (str);
				continue;
			}

			mystr = g_build_filename (dir, str, NULL);
			g_free (str);

			if (!dir_only || tracker_is_directory (mystr)) {

				if (g_slist_find_custom (tracker->no_watch_directory_list, mystr, (GCompareFunc) has_prefix) == NULL) {
					file_list = g_slist_prepend (file_list, g_strdup (mystr));
				}
			}

			g_free (mystr);
		}

 		g_dir_close (dirp);
	}

	g_free (dir_in_locale);

	if (!tracker->is_running) {
		if (file_list) {
			g_slist_foreach (file_list, (GFunc) g_free, NULL);
			g_slist_free (file_list);
		}

		return NULL;
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
	GSList  *list;
	int	i;

	list = NULL;

	for (i = 0; array[i] != NULL; i++) {
		list = g_slist_prepend (list, g_strdup (array[i]));
	}

	g_strfreev (array);

	return list;
}


gboolean
tracker_ignore_file (const char *uri)
{
	int  i;
	char *name;

	if (!uri || strlen (uri) == 0) {
		return TRUE;
	}

	name = g_path_get_basename (uri);

	if (name[0] == '.') {
		g_free (name);
		return TRUE;
	}

	/* ignore trash files */
	i = strlen (name);
	i--;
	if (name [i] == '~') {
		g_free (name);
		return TRUE;
	}

	g_free (name);

	return FALSE;
}


static void
display_list_values (const char *uri)
{
	tracker_log ("setting no watch directory %s", uri);
}


void
tracker_load_config_file ()
{
	GKeyFile *key_file;
	char	 *filename;
	char	 **values;

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

	values =  g_key_file_get_string_list (key_file,
					      "Watches",
					      "WatchDirectoryRoots",
					      NULL,
					      NULL);

	if (values) {
		tracker->watch_directory_roots_list = array_to_list (values);
	} else {
		tracker->watch_directory_roots_list = g_slist_prepend (tracker->watch_directory_roots_list, g_strdup (g_get_home_dir ()));
	}

	values =  g_key_file_get_string_list (key_file,
			       	     	      "Watches",
				              "NoWatchDirectory",
				              NULL,
				              NULL);

	if (values) {
		tracker->no_watch_directory_list = array_to_list (values);

		g_slist_foreach (tracker->no_watch_directory_list,(GFunc) display_list_values, NULL);
	} else {
		tracker->no_watch_directory_list = NULL;
	}

/*
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
*/
	//db_buffer_memory_limit = g_key_file_get_string ( key_file, "Database", "DBBufferMemoryLimit", NULL);

	g_free (filename);

	g_key_file_free (key_file);
}


void
tracker_remove_poll_dir (const char *dir)
{
	GSList *tmp;
	char   *str1;

	str1 = g_strconcat (dir, G_DIR_SEPARATOR_S, NULL);

	for (tmp = tracker->poll_list; tmp; tmp = tmp->next) {
		char *str2;

		str2 = tmp->data;

		if (strcmp (dir, str2) == 0) {
			g_mutex_lock (tracker->poll_access_mutex);
			tracker->poll_list = g_slist_remove (tracker->poll_list, tmp->data);
			g_mutex_unlock (tracker->poll_access_mutex);
			g_free (str2);
		}

		/* check if subfolder of existing roots */

		if (str2 && g_str_has_prefix (str2, str1)) {
			g_mutex_lock (tracker->poll_access_mutex);
			tracker->poll_list = g_slist_remove (tracker->poll_list, tmp->data);
			g_mutex_unlock (tracker->poll_access_mutex);
			g_free (str2);
		}
	}

	g_free (str1);
}


void
tracker_add_poll_dir (const char *dir)
{
	g_return_if_fail (dir && tracker_is_directory (dir));

	if (!tracker->is_running) {
		return;
	}

	g_mutex_lock (tracker->poll_access_mutex);
	tracker->poll_list = g_slist_prepend (tracker->poll_list, g_strdup (dir));
	g_mutex_unlock (tracker->poll_access_mutex);
	tracker_log ("adding %s for polling (poll count is %d)", dir, g_slist_length (tracker->poll_list));
}


gboolean
tracker_is_dir_polled (const char *dir)
{
	GSList *tmp;

	for (tmp = tracker->poll_list; tmp; tmp = tmp->next) {
		char *str;

		str = (char *) tmp->data;

		if (strcmp (dir, str) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}


void
tracker_notify_file_data_available (void)
{
	if (!tracker->is_running) {
		return;
	}

	/* if file thread is asleep then we just need to wake it up! */
	if (g_mutex_trylock (tracker->files_signal_mutex)) {
		g_cond_signal (tracker->file_thread_signal);
		g_mutex_unlock (tracker->files_signal_mutex);
		return;
	}

	/* if busy - check if async queue has new stuff as we do not need to notify then */
	if (g_async_queue_length (tracker->file_process_queue) > 0) {
		return;
	}

	/* if file thread not in check phase then we need do nothing */
	if (g_mutex_trylock (tracker->files_check_mutex)) {
		g_mutex_unlock (tracker->files_check_mutex);
		return;
	}

	/* we are in check phase - we need to wait until either check_mutex is unlocked or file thread is asleep then awaken it */
	while (TRUE) {

		if (g_mutex_trylock (tracker->files_check_mutex)) {
			g_mutex_unlock (tracker->files_check_mutex);
			return;
		}

		if (g_mutex_trylock (tracker->files_signal_mutex)) {
			g_cond_signal (tracker->file_thread_signal);
			g_mutex_unlock (tracker->files_signal_mutex);
			return;
		}

		g_thread_yield ();
		g_usleep (10);
	}
}


void
tracker_notify_meta_data_available (void)
{
	if (!tracker->is_running) {
		return;
	}

	/* if metadata thread is asleep then we just need to wake it up! */
	if (g_mutex_trylock (tracker->metadata_signal_mutex)) {
		g_cond_signal (tracker->metadata_thread_signal);
		g_mutex_unlock (tracker->metadata_signal_mutex);
		return;
	}

	/* if busy - check if async queue has new stuff as we do not need to notify then */
	if (g_async_queue_length (tracker->file_metadata_queue) > 0) {
		return;
	}

	/* if metadata thread not in check phase then we need do nothing */
	if (g_mutex_trylock (tracker->metadata_check_mutex)) {
		g_mutex_unlock (tracker->metadata_check_mutex);
		return;
	}

	/* we are in check phase - we need to wait until either check_mutex is unlocked or until metadata thread is asleep then we awaken it */
	while (TRUE) {

		if (g_mutex_trylock (tracker->metadata_check_mutex)) {
			g_mutex_unlock (tracker->metadata_check_mutex);
			return;
		}

		if (g_mutex_trylock (tracker->metadata_signal_mutex)) {
			g_cond_signal (tracker->metadata_thread_signal);
			g_mutex_unlock (tracker->metadata_signal_mutex);
			return;
		}

		g_thread_yield ();
		g_usleep (10);
	}
}


void
tracker_notify_request_data_available (void)
{
	/* if thread is asleep then we just need to wake it up! */
	if (g_mutex_trylock (tracker->request_signal_mutex)) {
		g_cond_signal (tracker->request_thread_signal);
		g_mutex_unlock (tracker->request_signal_mutex);
		return;
	}

	/* if busy - check if async queue has new stuff as we do not need to notify then */
	if (g_async_queue_length (tracker->user_request_queue) > 0) {
		return;
	}

	/* if thread not in check phase then we need do nothing */
	if (g_mutex_trylock (tracker->request_check_mutex)) {
		g_mutex_unlock (tracker->request_check_mutex);
		return;
	}

	/* we are in check phase - we need to wait until either check_mutex is unlocked or thread is asleep then awaken it */
	while (TRUE) {

		if (g_mutex_trylock (tracker->request_check_mutex)) {
			g_mutex_unlock (tracker->request_check_mutex);
			return;
		}

		if (g_mutex_trylock (tracker->request_signal_mutex)) {
			g_cond_signal (tracker->request_thread_signal);
			g_mutex_unlock (tracker->request_signal_mutex);
			return;
		}

		g_thread_yield ();
		g_usleep (10);
	}
}
