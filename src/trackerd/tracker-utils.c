/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Michal Pryc (Michal.Pryc@Sun.Com)
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include "config.h"

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <zlib.h>
#include <math.h>

#ifdef OS_WIN32
#include <conio.h>
#include "mingw-compat.h"
#else
#include <sys/resource.h>
#endif

#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <glib/gpattern.h>

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-config.h>
#include "../xdgmime/xdgmime.h"

#include "tracker-dbus.h"
#include "tracker-utils.h"
#include "tracker-indexer.h"
#include "tracker-process-files.h"
#include "tracker-os-dependant.h"

extern Tracker	*tracker;

char *tracker_actions[] = {
		"TRACKER_ACTION_IGNORE", "TRACKER_ACTION_CHECK", "TRACKER_ACTION_DELETE", "TRACKER_ACTION_DELETE_SELF", "TRACKER_ACTION_CREATE","TRACKER_ACTION_MOVED_FROM",
		"TRACKER_ACTION_MOVED_TO","TRACKER_ACTION_FILE_CHECK", "TRACKER_ACTION_FILE_CHANGED","TRACKER_ACTION_FILE_DELETED", "TRACKER_ACTION_FILE_CREATED",
		"TRACKER_ACTION_FILE_MOVED_FROM", "TRACKER_ACTION_FILE_MOVED_TO", "TRACKER_ACTION_WRITABLE_FILE_CLOSED","TRACKER_ACTION_DIRECTORY_CHECK",
		"TRACKER_ACTION_DIRECTORY_CREATED","TRACKER_ACTION_DIRECTORY_DELETED","TRACKER_ACTION_DIRECTORY_UNMOUNTED", "TRACKER_ACTION_DIRECTORY_MOVED_FROM",
		"TRACKER_ACTION_DIRECTORY_MOVED_TO", "TRACKER_ACTION_DIRECTORY_REFRESH", "TRACKER_ACTION_EXTRACT_METADATA",
		NULL};


#define ZLIBBUFSIZ 8192
#define TEXT_SNIFF_SIZE 4096
#define MAX_INDEX_FILE_SIZE 2000000000

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

	/* check for year only dates (EG ID3 music tags might have Audio.ReleaseDate as 4 digit year) */

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

			if (len < 22 || len > 25) {
				return FALSE;
			}

			if ( (timestamp[19] != '+') && (timestamp[19] != '-') ) {
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

        g_return_val_if_fail (timestamp, -1);

	/* we should have a valid iso 8601 date in format YYYY-MM-DDThh:mm:ss with optional TZ*/
        if (!is_valid_8601_datetime (timestamp)) {
		return  -1;
	}

        memset (&tm, 0, sizeof (struct tm));

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
		tracker_error ("ERROR: date validation failed for %s st %c", timestamp, *timestamp);
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
        /* mktime() always assumes that "tm" is in locale time but
           we want to keep control on time, so we go to UTC */
        tt -= timezone;

	if (*timestamp == '+' || *timestamp == '-') {
		int sign;

		sign = (*timestamp++ == '+') ? -1 : 1;

                /* we have format hh:mm or hhmm */

                /* now, we are reading hours */
                if (timestamp[0] && timestamp[1]) {
                        if (g_ascii_isdigit (timestamp[0]) && g_ascii_isdigit (timestamp[1])) {
                                gchar buff[3];

                                buff[0] = timestamp[0];
                                buff[1] = timestamp[1];
                                buff[2] = '\0';

                                val = strtoul (buff, NULL, 10);
                                tt += sign * (3600 * val);
                                timestamp += 2;
                        }

                        if (*timestamp == ':' || *timestamp == '\'') {
                                timestamp++;
                        }
                }

                /* now, we are reading minutes */
                if (timestamp[0] && timestamp[1]) {
                        if (g_ascii_isdigit (timestamp[0]) && g_ascii_isdigit (timestamp[1])) {
                                gchar buff[3];

                                buff[0] = timestamp[0];
                                buff[1] = timestamp[1];
                                buff[2] = '\0';

                                val = strtoul (buff, NULL, 10);
                                tt += sign * (60 * val);
                                timestamp += 2;
                        }
                }
	} else {
                /*
		if (*timestamp == 'Z') {
			// no need to do anything if already utc
		}
                */
	}

	return tt;
}


char *
tracker_date_to_str (time_t date_time)
{
	char  		buffer[30];
	struct tm 	loctime;
	size_t		count;

	memset (buffer, '\0', sizeof (buffer));
	memset (&loctime, 0, sizeof (struct tm));

	localtime_r (&date_time, &loctime);

	/* output is ISO 8160 format : "YYYY-MM-DDThh:mm:ss+zz:zz" */
	count = strftime (buffer, sizeof (buffer), "%FT%T%z", &loctime);

        return (count > 0) ? g_strdup (buffer) : NULL;
}


inline gboolean
tracker_is_empty_string (const char *s)
{
	return s == NULL || s[0] == '\0';
}


gchar *
tracker_long_to_str (glong i)
{
        return g_strdup_printf ("%ld", i);
}


gchar *
tracker_int_to_str (gint i)
{
	return g_strdup_printf ("%d", i);
}


gchar *
tracker_uint_to_str (guint i)
{
	return g_strdup_printf ("%u", i);
}


gchar *
tracker_gint32_to_str (gint32 i)
{
        return g_strdup_printf ("%" G_GINT32_FORMAT, i);
}


gchar *
tracker_guint32_to_str (guint32 i)
{
        return g_strdup_printf ("%" G_GUINT32_FORMAT, i);
}


gboolean
tracker_str_to_uint (const char *s, guint *ret)
{
	unsigned long int n;

	g_return_val_if_fail (s, FALSE);

	n = strtoul (s, NULL, 10);

	if (n > G_MAXUINT) {
		*ret = 0;
		return FALSE;

	} else {
		*ret = (guint) n;
		return TRUE;
	}
}


int
tracker_str_in_array (const char *str, char **array)
{
	int  i;
	char **st;

	i = 0;

	for (st = array; *st; st++) {
		if (strcasecmp (*st, str) == 0) {
			return i;
		}
		i++;
	}

	return -1;
}


char *
tracker_get_radix_by_suffix (const char *str, const char *suffix)
{
	g_return_val_if_fail (str, NULL);
	g_return_val_if_fail (suffix, NULL);

	if (g_str_has_suffix (str, suffix)) {
		return g_strndup (str, g_strrstr (str, suffix) - str);
	} else {
		return NULL;
	}
}


char *
tracker_escape_metadata (const char *in)
{
	if (!in) {
		return NULL;
	}

	GString *gs = g_string_new ("");

	for(; *in; in++) {
		if (*in == '|') {
			g_string_append_c (gs, 30);
		} else {
			g_string_append_c (gs, *in);
		}
	}

	return g_string_free (gs, FALSE);
}



char *
tracker_unescape_metadata (const char *in)
{
	if (!in) {
		return NULL;
	}

	GString *gs = g_string_new ("");

	for(; *in; in++) {
		if (*in == 30) {
			g_string_append_c (gs, '|');
		} else {
			g_string_append_c (gs, *in);
		}
	}

	return g_string_free (gs, FALSE);
}



void
tracker_remove_dirs (const char *root_dir)
{
	GQueue *dirs;
	GSList *dirs_to_remove;

	dirs = g_queue_new ();

	g_queue_push_tail (dirs, g_strdup (root_dir));

	dirs_to_remove = NULL;

	while (!g_queue_is_empty (dirs)) {
		char *dir;
		GDir *dirp;

		dir = g_queue_pop_head (dirs);

		dirs_to_remove = g_slist_prepend (dirs_to_remove, dir);

		if ((dirp = g_dir_open (dir, 0, NULL))) {
			const char *file;

			while ((file = g_dir_read_name (dirp))) {
				char *full_filename;

				full_filename = g_build_filename (dir, file, NULL);

				if (g_file_test (full_filename, G_FILE_TEST_IS_DIR)) {
					g_queue_push_tail (dirs, full_filename);
				} else {
					g_unlink (full_filename);
					g_free (full_filename);
				}
			}

			g_dir_close (dirp);
		}
	}

	g_queue_free (dirs);

	/* remove directories (now they are empty) */
	g_slist_foreach (dirs_to_remove, (GFunc) g_rmdir, NULL);

	g_slist_foreach (dirs_to_remove, (GFunc) g_free, NULL);

	g_slist_free (dirs_to_remove);
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
tracker_print_object_allocations (void)
{
	tracker_log ("Total allocations = %d, total deallocations = %d", info_allocated, info_deallocated);
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

	info = g_slice_new0 (FileInfo);

	info->action = action;
	info->uri = g_strdup (uri);

	info->counter = counter;
	info->file_id = 0;

	info->file_type = FILE_ORDINARY;

	info->watch_type = watch;
	info->is_directory = FALSE;

	info->is_link = FALSE;
	info->link_id = 0;
	info->link_path = NULL;
	info->link_name = NULL;

	info->mime = NULL;
	info->file_size = 0;
	info->permissions = g_strdup ("-r--r--r--");
	info->mtime = 0;
	info->atime = 0;
	info->indextime = 0;
	info->offset = 0;
	info->aux_id = -1;

	info->is_hidden = FALSE;

	info->is_new = TRUE;
	info->service_type_id = -1;

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

	if (info->moved_to_uri) {
		g_free (info->moved_to_uri);
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
tracker_get_pending_file_info (guint32 file_id, const char *uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory)
{
	FileInfo *info;

	info = g_slice_new0 (FileInfo);

	info->action = action;
	info->uri = g_strdup (uri);

	info->counter = counter;
	info->file_id = file_id;

	info->file_type = FILE_ORDINARY;

	info->is_directory = is_directory;

	info->is_link = FALSE;
	info->link_id = 0;
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
	info->offset = 0;

	info->service_type_id = -1;
	info->is_new = TRUE;

	info->ref_count = 1;
	info_allocated ++;

	return info;
}


gint32
tracker_get_file_mtime (const char *uri)
{
	struct stat  	finfo;
	char 		*uri_in_locale;

	uri_in_locale = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (uri_in_locale) {
		if (g_lstat (uri_in_locale, &finfo) == -1) {
			g_free (uri_in_locale);

			return 0;
		}

	} else {
		tracker_error ("ERROR: uri could not be converted to locale format");

		return 0;
	}

	g_free (uri_in_locale);

	return (gint32) finfo.st_mtime;
}


FileInfo *
tracker_get_file_info (FileInfo *info)
{
	struct stat     finfo;
	char   		*str, *uri_in_locale;

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
		tracker_error ("ERROR: info->uri could not be converted to locale format");

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
		info->file_size = (guint32) finfo.st_size;
	} else {
		if (info->watch_type == WATCH_OTHER) {
			info->watch_type = WATCH_SUBFOLDER;
		}
	}

	g_free (info->permissions);
	info->permissions = tracker_create_permission_string (finfo);

	info->mtime =  finfo.st_mtime;
	info->atime =  finfo.st_atime;

	return info;
}


GSList *
tracker_get_service_dirs (const char *service)
{
	GSList *list = NULL, *tmp;

	if (!service) {
		return NULL;
	}

	for (tmp = tracker->service_directory_list; tmp; tmp = tmp->next) {

		char *path = (char *) tmp->data;
		char *path_service = g_hash_table_lookup (tracker->service_directory_table, path);

		if (strcasecmp (service, path_service) ==0) {
			list = g_slist_prepend (list, path);
		}
	}

	return list;
}


void
tracker_add_service_path (const char *service,  const char *path)
{
	if (!service || !path || !tracker_file_is_valid (path)) {
		return;
	}

	char *dir_path = g_strdup (path);
	char *service_type = g_strdup (service);

	tracker->service_directory_list = g_slist_prepend (tracker->service_directory_list, g_strdup (path));
	g_hash_table_insert (tracker->service_directory_table, dir_path, service_type);
}


void
tracker_del_service_path (const char *service,  const char *path)
{
	if (!service || !path) {
		return;
	}
	GSList *found = g_slist_find_custom (tracker->service_directory_list, path, (GCompareFunc) strcmp);
	if (found) {
		g_free (found->data);
		tracker->service_directory_list = g_slist_remove_link (tracker->service_directory_list, found);
		g_slist_free (found);
	}
	g_hash_table_remove (tracker->service_directory_table, path);
}

char *
tracker_get_service_for_uri (const char *uri)
{
	GSList *tmp;

	/* check service dir list to see if a prefix */
	for (tmp = tracker->service_directory_list; tmp; tmp = tmp->next) {
		char *prefix;

		prefix = (char *) tmp->data;
		if (prefix && g_str_has_prefix (uri, prefix)) {
			return g_strdup (g_hash_table_lookup (tracker->service_directory_table, prefix));
		}
	}

	return g_strdup ("Files");
}


gboolean
tracker_is_service_file (const char *uri)
{
	char *service;
	gboolean result;

	service = tracker_get_service_for_uri (uri);

	result =  (service && strcmp (service, "Files") != 0);

	g_free (service);

	return result;
}


gboolean
tracker_file_is_valid (const char *uri)
{
	gboolean convert_ok;
	char	 *uri_in_locale;

	uri_in_locale = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (!uri_in_locale) {
		tracker_error ("ERROR: uri could not be converted to locale format");
		g_free (uri_in_locale);
		return FALSE;
	}

	/* g_file_test(file,G_FILE_TEST_EXISTS) uses the access() system call and so needs locale filenames. */
	convert_ok = (tracker_check_uri (uri) && g_file_test (uri_in_locale, G_FILE_TEST_IS_REGULAR|G_FILE_TEST_IS_DIR|G_FILE_TEST_IS_SYMLINK));

	g_free (uri_in_locale);

	return convert_ok;
}


gboolean
tracker_file_is_indexable (const char *uri)
{
	char	 *uri_in_locale;
	struct stat finfo;
	gboolean convert_ok;

	uri_in_locale = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (!uri_in_locale) {
		tracker_error ("ERROR: uri could not be converted to locale format");
		return FALSE;
	}

	g_lstat (uri_in_locale, &finfo);

	g_free (uri_in_locale);

	convert_ok = (!S_ISDIR (finfo.st_mode) && S_ISREG (finfo.st_mode));

	if (convert_ok) {
		 tracker_debug ("file %s is indexable", uri);
	} else {
		 tracker_debug ("file %s is *not* indexable", uri);
	}

	return convert_ok;
}


static inline gboolean
is_utf8 (const gchar *buffer, gint buffer_length)
{
	gchar *end;

	/* code in this function modified from gnome-vfs */

	if (g_utf8_validate ((gchar *)buffer, buffer_length, (const gchar**)&end)) {
		return TRUE;
	} else {
		/* Check whether the string was truncated in the middle of
		 * a valid UTF8 char, or if we really have an invalid
		 * UTF8 string
     		 */
		gint remaining_bytes = buffer_length;

		remaining_bytes -= (end-((gchar *)buffer));

		if (remaining_bytes > 4) {
			return FALSE;
		}

 		if (g_utf8_get_char_validated (end, (gsize) remaining_bytes) == (gunichar) -2) {
			return TRUE;
		}
	}

	return FALSE;
}


static gboolean
is_text_file (const gchar *uri)
{
	char 	buffer[TEXT_SNIFF_SIZE];
	int 	buffer_length = 0;
	GError	*err = NULL;
	int 	fd;

	fd = tracker_file_open (uri, FALSE);

	buffer_length = read (fd, buffer, TEXT_SNIFF_SIZE);

	if (buffer_length < 3) {
		goto return_false;
	}

	/* Don't allow embedded zeros in textfiles. */
	if (memchr (buffer, 0, buffer_length) != NULL) {
		goto return_false;
	}

	if (is_utf8 (buffer, buffer_length)) {
		 goto return_true;
	} else {
		gchar *tmp = g_locale_to_utf8 (buffer, buffer_length, NULL, NULL, &err);
		g_free (tmp);

		if (err) {
			gboolean result = FALSE;

			if (err->code != G_CONVERT_ERROR_ILLEGAL_SEQUENCE && err->code !=  G_CONVERT_ERROR_FAILED && err->code != G_CONVERT_ERROR_NO_CONVERSION) {
				result = TRUE;
			}

			g_error_free (err);

			if (result) goto return_true;

		}
	}

return_false:
	tracker_file_close (fd, TRUE);
	return FALSE;

return_true:
	tracker_file_close (fd, FALSE);
	return TRUE;
}


char *
tracker_get_mime_type (const char *uri)
{
	struct stat finfo;
	char	    *uri_in_locale;
	const char  *result;
	char *mime;

	if (!tracker_file_is_valid (uri)) {
		tracker_log ("WARNING: file %s is no longer valid", uri);
		return g_strdup ("unknown");
	}

	uri_in_locale = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (!uri_in_locale) {
		tracker_error ("ERROR: uri could not be converted to locale format");
		return g_strdup ("unknown");
	}

	g_lstat (uri_in_locale, &finfo);

	if (S_ISLNK (finfo.st_mode) && S_ISDIR (finfo.st_mode)) {
	        g_free (uri_in_locale);
		return g_strdup ("symlink");
	}


	/* handle iso files as they can be mistaken for video files */
	
	if (g_str_has_suffix (uri, ".iso")) {
		return g_strdup ("application/x-cd-image");
	}

	result = xdg_mime_get_mime_type_for_file (uri, NULL);

	if (!result || (result == XDG_MIME_TYPE_UNKNOWN)) {

		if (is_text_file (uri_in_locale)) {
			mime = g_strdup ("text/plain");
		} else {
			mime = g_strdup ("unknown");
		}
	} else {
		mime = g_strdup (result);
	}

	g_free (uri_in_locale);

	return mime;
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
		char *p, *res, *tmp, *result;
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
			result =  g_strdup (res);
			g_free (tmp);

			return result;
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
		tracker_error ("ERROR: dir could not be converted to locale format");
	}

	return FALSE;
}


/*
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
*/


GSList *
tracker_string_list_to_gslist (const gchar **array)
{
	GSList *list = NULL;
 	gint	i;

 	for (i = 0; array[i]; i++) {
		if (tracker_is_empty_string (array[i])) {
			continue;
		}

		list = g_slist_prepend (list, g_strdup (array[i]));
	}

	return g_slist_reverse (list);
}

gchar **
tracker_gslist_to_string_list (GSList *list)
{
	GSList  *l;
	gchar  **string_list;
	gint     i;

	string_list = g_new0 (gchar *, g_slist_length (list) + 1);

	for (l = list, i = 0; l; l = l->next) {
 		if (!l->data) {
			continue;
  		}

		string_list[i++] = g_strdup (l->data);
	}

	string_list[i] = NULL;

	return string_list;
}

char *
tracker_array_to_str (char **array, int length, char sep)
{
	GString *gstr = g_string_new ("");
	int i;

	for (i=0; i<length; i++) {

		

		if (array[i]) {
			if (i > 0) g_string_append_c (gstr, sep);

			gstr = g_string_append (gstr, array[i]);
		} else {
			break;
		}
	}

	return g_string_free (gstr, FALSE);

}


void
tracker_throttle (int multiplier)
{
	gint throttle;

	throttle = tracker_config_get_throttle (tracker->config);

	if (throttle < 1) {
		return;
	}

 	throttle *= multiplier;

	if (throttle > 0) {
  		g_usleep (throttle);
	}
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
	if (g_async_queue_length (tracker->file_process_queue) > 1) {
		return;
	}

	/* if file thread not in check phase then we need do nothing */
	if (g_mutex_trylock (tracker->files_check_mutex)) {
		g_mutex_unlock (tracker->files_check_mutex);
		return;
	}

	int revs = 0;

	/* we are in check phase - we need to wait until either check_mutex is unlocked or file thread is asleep then awaken it */
	while (revs < 100000) {

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
		revs++;
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
	if (g_async_queue_length (tracker->file_metadata_queue) > 1) {
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
		tracker_debug ("in check phase");
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


GTimeVal *
tracker_timer_start (void)
{
	GTimeVal  *before;

	before = g_new0 (GTimeVal, 1);

	g_get_current_time (before);

	return before;
}


void
tracker_timer_end (GTimeVal *before, const char *str)
{
	GTimeVal  after;
	double	  elapsed;

	g_get_current_time (&after);

	elapsed = (1000 * (after.tv_sec - before->tv_sec))  +  ((after.tv_usec - before->tv_usec) / 1000.0);

	g_free (before);

	tracker_log ("%s %f ms", str, elapsed);
}


char *
tracker_compress (const char *ptr, int size, int *compressed_size)
{
	z_stream zs;
	char *buf, *swap;
	unsigned char obuf[ZLIBBUFSIZ];
	int rv, asiz, bsiz, osiz;
	if (size < 0) size = strlen (ptr);
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;

	if (deflateInit2 (&zs, 6, Z_DEFLATED, 15, 6, Z_DEFAULT_STRATEGY) != Z_OK) {
		return NULL;
	}

	asiz = size + 16;
	if (asiz < ZLIBBUFSIZ) asiz = ZLIBBUFSIZ;
	if (!(buf = malloc (asiz))) {
		deflateEnd (&zs);
		return NULL;
	}
	bsiz = 0;
	zs.next_in = (unsigned char *)ptr;
	zs.avail_in = size;
	zs.next_out = obuf;
	zs.avail_out = ZLIBBUFSIZ;
	while ( (rv = deflate (&zs, Z_FINISH)) == Z_OK) {
		osiz = ZLIBBUFSIZ - zs.avail_out;
		if (bsiz + osiz > asiz) {
			asiz = asiz * 2 + osiz;
			if (!(swap = realloc (buf, asiz))) {
				free (buf);
				deflateEnd (&zs);
				return NULL;
			}
			buf = swap;
		}
		memcpy (buf + bsiz, obuf, osiz);
		bsiz += osiz;
		zs.next_out = obuf;
		zs.avail_out = ZLIBBUFSIZ;
	}
	if (rv != Z_STREAM_END) {
		free (buf);
		deflateEnd (&zs);
		return NULL;
	}

	osiz = ZLIBBUFSIZ - zs.avail_out;

	if (bsiz + osiz + 1 > asiz) {
		asiz = asiz * 2 + osiz;

		if (!(swap = realloc (buf, asiz))) {
			free (buf);
			deflateEnd (&zs);
			return NULL;
		}
		buf = swap;
	}

	memcpy (buf + bsiz, obuf, osiz);
	bsiz += osiz;
	buf[bsiz] = '\0';

	*compressed_size = bsiz;

	deflateEnd (&zs);

	return buf;
}


char *
tracker_uncompress (const char *ptr, int size, int *uncompressed_size)
{
	z_stream zs;
	char *buf, *swap;
	unsigned char obuf[ZLIBBUFSIZ];
	int rv, asiz, bsiz, osiz;
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;

	if (inflateInit2 (&zs, 15) != Z_OK) return NULL;

	asiz = size * 2 + 16;
	if (asiz < ZLIBBUFSIZ) asiz = ZLIBBUFSIZ;
	if (!(buf = malloc (asiz))) {
		inflateEnd (&zs);
		return NULL;
	}
	bsiz = 0;
	zs.next_in = (unsigned char *)ptr;
	zs.avail_in = size;
	zs.next_out = obuf;
	zs.avail_out = ZLIBBUFSIZ;
	while ( (rv = inflate (&zs, Z_NO_FLUSH)) == Z_OK) {
		osiz = ZLIBBUFSIZ - zs.avail_out;
		if (bsiz + osiz >= asiz) {
			asiz = asiz * 2 + osiz;
			if (!(swap = realloc (buf, asiz))) {
				free (buf);
				inflateEnd (&zs);
				return NULL;
			}
			buf = swap;
		}
		memcpy (buf + bsiz, obuf, osiz);
		bsiz += osiz;
		zs.next_out = obuf;
		zs.avail_out = ZLIBBUFSIZ;
	}
	if (rv != Z_STREAM_END) {
		free (buf);
		inflateEnd (&zs);
		return NULL;
	}
	osiz = ZLIBBUFSIZ - zs.avail_out;
	if (bsiz + osiz >= asiz) {
		asiz = asiz * 2 + osiz;
		if (!(swap = realloc (buf, asiz))) {
			free (buf);
			inflateEnd (&zs);
			return NULL;
		}
		buf = swap;
	}
	memcpy (buf + bsiz, obuf, osiz);
	bsiz += osiz;
	buf[bsiz] = '\0';
	*uncompressed_size = bsiz;
	inflateEnd (&zs);
	return buf;
}


static inline gboolean
is_match (const char *a, const char *b)
{
	int len = strlen (b);

	char *str1 = g_utf8_casefold (a, len);
        char *str2 = g_utf8_casefold (b, len);

	char *normal1 = g_utf8_normalize (str1, -1, G_NORMALIZE_NFC);
	char *normal2 = g_utf8_normalize (str2, -1, G_NORMALIZE_NFC);

	gboolean result = (strcmp (normal1, normal2) == 0);

	g_free (str1);
	g_free (str2);
	g_free (normal1);
	g_free (normal2);

	return result;
}


static const gchar *
pointer_from_offset_skipping_decomp (const gchar *str, gint offset)
{
	gchar *casefold, *normal;
	const gchar *p, *q;

	g_return_val_if_fail (str != NULL, NULL);

	p = str;
	while (offset > 0)
	{
		q = g_utf8_next_char (p);
		casefold = g_utf8_casefold (p, q - p);
		normal = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFC);
		offset -= g_utf8_strlen (normal, -1);
		g_free (casefold);
		g_free (normal);
		p = q;
	}
	return p;
}


static const char *
g_utf8_strcasestr_array (const gchar *haystack, gchar **needles)
{
	gsize needle_len;
	gsize haystack_len;
	const char *ret = NULL, *needle;
	char **array;
	char *p;
	char *casefold;
	char *caseless_haystack;
	int i;

	g_return_val_if_fail (haystack != NULL, NULL);

	casefold = g_utf8_casefold (haystack, -1);
	caseless_haystack = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFC);
	g_free (casefold);

	if (!caseless_haystack) {
		return NULL;
	}

	haystack_len = g_utf8_strlen (caseless_haystack, -1);

	for (array=needles; *array; array++)
	{
		needle = *array;
		needle_len = g_utf8_strlen (needle, -1);

		if (needle_len == 0) {
			continue;
		}

		if (haystack_len < needle_len) {
			continue;
		}

		p = (gchar *) caseless_haystack;
		needle_len = strlen (needle);
		i = 0;

		while (*p) {

			if ((strncmp (p, needle, needle_len) == 0)) {
				ret = pointer_from_offset_skipping_decomp (haystack, i);
				goto finally_1;
			}

			p = g_utf8_next_char (p);
			i++;
		}
	}

finally_1:
	g_free (caseless_haystack);

	return ret;
}


const char *
substring_utf8 (const char *a, const char *b)
{
        const char  *ptr, *found_ptr;
	gunichar c, lower, upper;
	int len;
	gboolean got_match = FALSE;

	len = strlen (b);

	c = g_utf8_get_char (b);

	lower = g_unichar_tolower (c);
	upper = g_unichar_toupper (c);

	ptr = a;
	found_ptr = a;

	/* check lowercase first */
	while (found_ptr) {

		found_ptr = g_utf8_strchr (ptr, -1, lower);

		if (found_ptr) {
			ptr = g_utf8_find_next_char (found_ptr, NULL);
			if (is_match (found_ptr, b)) {
				got_match = TRUE;
				break;
			}
		} else {
			break;
		}
	}

	if (!got_match) {
		ptr = a;
		found_ptr = a;
		while (found_ptr) {

			found_ptr = g_utf8_strchr (ptr, -1, upper);

			if (found_ptr) {
				ptr = g_utf8_find_next_char (found_ptr, NULL);
				if (is_match (found_ptr, b)) {
					break;
				}
			} else {

			}
		}
	}

	return found_ptr;
}


static int
get_word_break (const char *a)
{
	char **words = g_strsplit_set (a, "\t\n\v\f\r !\"#$%&'()*/<=>?[\\]^`{|}~+,.:;@\"[]" , -1);

	if (!words) return 0;

	int ret = strlen (words[0]);

	g_strfreev  (words);

	return ret;
}


static gboolean
is_word_break (const char a)
{
	const char *breaks = "\t\n\v\f\r !\"#$%&'()*/<=>?[\\]^`{|}~+,.:;@\"[]";
	int i;

	for (i = 0; breaks[i]; i++) {
		if (a == breaks[i]) {
			return TRUE;
		}
	}

	return FALSE;
}


static char *
highlight_terms (const char *str, char **terms)
{
	const char *ptr;
	char *txt;
	GString *st;
	int term_length;

	if (!str || !terms) {
		return NULL;
	}

	char **array;
	txt = g_strdup (str);

	for (array = terms; *array; array++) {
		char **single_term;

		single_term = g_new( char *, 2);
		single_term[0] = g_strdup (*array);
		single_term[1] = NULL;

		st = g_string_new ("");

		const char *ptxt = txt;

		while ((ptr = g_utf8_strcasestr_array  (ptxt, single_term))) {
			char *pre_snip, *term;

			pre_snip = g_strndup (ptxt, (ptr - ptxt));

			term_length = get_word_break (ptr);

			term = g_strndup (ptr, term_length);

			ptxt = ptr + term_length;

			g_string_append_printf (st, "%s<b>%s</b>", pre_snip, term);

			g_free (pre_snip);
			g_free (term);
		}

		if (ptxt) {
			g_string_append (st, ptxt);

		}

		g_strfreev  (single_term);
		g_free (txt);

		txt = g_string_free (st, FALSE);
	}

	return txt;
}


char *
tracker_get_snippet (const char *txt, char **terms, int length)
{
	const char *ptr = NULL, *end_ptr,  *tmp;
	int i, txt_len;

	if (!txt || !terms) {
		return NULL;
	}

	txt_len = strlen (txt);

	ptr = g_utf8_strcasestr_array (txt, terms);

	if (ptr) {
		tmp = ptr;

		i = 0;

		/* get snippet before  the matching term */
		while ((ptr = g_utf8_prev_char (ptr)) && (ptr >= txt) && (i < length)) {

			if (*ptr == '\n') {
				break;
			}
			i++;
		}

		/* try to start beginning of snippet on a word break */
		if ((*ptr != '\n') && (ptr > txt)) {
			i=0;
			while (!is_word_break (*ptr) && (i<(length/2))) {
				ptr = g_utf8_next_char (ptr);
				i++;
			}
		}

		ptr = g_utf8_next_char (ptr);

		if (!ptr || ptr < txt) {
			return NULL;
		}

		end_ptr = tmp;
		i = 0;

		/* get snippet after match */
		while ((end_ptr = g_utf8_next_char (end_ptr)) && (end_ptr <= txt_len + txt) && (i < length)) {
			i++;
			if (*end_ptr == '\n') {
				break;
			}
		}

		while (end_ptr > txt_len + txt) {
			end_ptr = g_utf8_prev_char (end_ptr);
		}

		/* try to end snippet on a word break */
		if ((*end_ptr != '\n') && (end_ptr < txt_len + txt)) {
			i=0;
			while (!is_word_break (*end_ptr) && (i<(length/2))) {
				end_ptr = g_utf8_prev_char (end_ptr);
				i++;
			}
		}

		if (!end_ptr || !ptr) {
			return NULL;
		}

		char *snip, *esc_snip,  *highlight_snip;

		snip = g_strndup (ptr,  end_ptr - ptr);

		i = strlen (snip);

		esc_snip = g_markup_escape_text (snip, i);

		g_free (snip);

		highlight_snip = highlight_terms (esc_snip, terms);

		g_free (esc_snip);

		return highlight_snip;
	}

	ptr = txt;
	i = 0;
	while ((ptr = g_utf8_next_char (ptr)) && (ptr <= txt_len + txt) && (i < length)) {
		i++;
		if (*ptr == '\n') {
			break;
		}
	}

	if (ptr > txt_len + txt) {
		ptr = g_utf8_prev_char (ptr);
	}

	if (ptr) {
		char *snippet = g_strndup (txt, ptr - txt);
		char *esc_snippet = g_markup_escape_text (snippet, ptr - txt);
		char *highlight_snip = highlight_terms (esc_snippet, terms);

		g_free (snippet);
		g_free (esc_snippet);

		return highlight_snip;
	} else {
		return NULL;
	}
}

gchar *
tracker_string_replace (const gchar *haystack, gchar *needle, gchar *replacement)
{
        GString *str;
        gint pos, needle_len;

	g_return_val_if_fail (haystack && needle, NULL);

	needle_len = strlen (needle);

        str = g_string_new ("");

        for (pos = 0; haystack[pos]; pos++)
        {
                if (strncmp (&haystack[pos], needle, needle_len) == 0)
                {
			if (replacement) {
	                        str = g_string_append (str, replacement);
			}

                        pos += needle_len - 1;

                } else {
                        str = g_string_append_c (str, haystack[pos]);
		}
        }

        return g_string_free (str, FALSE);
}


void
tracker_add_metadata_to_table (GHashTable *meta_table, const gchar *key, const gchar *value)
{
	GSList *list = g_hash_table_lookup (meta_table, (gchar *) key);

	list = g_slist_prepend (list, (gchar *) value);

	g_hash_table_steal (meta_table, key);

	g_hash_table_insert (meta_table, (gchar *) key, list);
}


void
tracker_free_metadata_field (FieldData *field_data)
{
	g_return_if_fail (field_data);

	if (field_data->alias) {
		g_free (field_data->alias);
	}

	if (field_data->where_field) {
		g_free (field_data->where_field);
	}

	if (field_data->field_name) {
		g_free (field_data->field_name);
	}

	if (field_data->select_field) {
		g_free (field_data->select_field);
	}

	if (field_data->table_name) {
		g_free (field_data->table_name);
	}

	if (field_data->id_field) {
		g_free (field_data->id_field);
	}

	g_free (field_data);
}


gboolean
tracker_unlink (const gchar *uri)
{
	gchar *locale_uri = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (!g_file_test (locale_uri, G_FILE_TEST_EXISTS)) {						
		g_free (locale_uri);
		return FALSE;
	}

	g_unlink (locale_uri);

	g_free (locale_uri);

	return TRUE;
}


int 
tracker_get_memory_usage (void)
{


#if defined(__linux__)
	int  fd, length, mem = 0;
	char buffer[8192];

	char *stat_file = g_strdup_printf ("/proc/%d/stat", tracker->pid);

	fd = open (stat_file, O_RDONLY); 

	g_free (stat_file);

	if (fd ==-1) {
		return 0;
	}

	
	length = read (fd, buffer, 8192);

	buffer[length] = 0;

	close (fd);

	char **terms = g_strsplit (buffer, " ", -1);

	
	if (terms) {

		int i;
		for (i=0; i < 24; i++) {
			if (!terms[i]) {
				break;
			}		

			if (i==23) mem = 4 * atoi (terms[23]);
		}
	}


	g_strfreev (terms);

	return mem;	
	
#endif
	return 0;
}

guint32
tracker_file_size (const gchar *name)
{
	struct stat finfo;
	
	if (g_lstat (name, &finfo) == -1) {
		return 0;
	} else {
                return (guint32) finfo.st_size;
        }
}

int
tracker_file_open (const gchar *file_name, gboolean readahead)
{
	int fd;

#if defined(__linux__)
	fd = open (file_name, O_RDONLY|O_NOATIME);

	if (fd == -1) {
		fd = open (file_name, O_RDONLY); 
	}
#else
	fd = open (file_name, O_RDONLY); 
#endif

	if (fd == -1) return -1;
	
#ifdef HAVE_POSIX_FADVISE
	if (readahead) {
		posix_fadvise (fd, 0, 0, POSIX_FADV_SEQUENTIAL);
	} else {
		posix_fadvise (fd, 0, 0, POSIX_FADV_RANDOM);
	}
#endif

	return fd;
}


void
tracker_file_close (int fd, gboolean no_longer_needed)
{

#ifdef HAVE_POSIX_FADVISE
	if (no_longer_needed) {
		posix_fadvise (fd, 0, 0, POSIX_FADV_DONTNEED);
	}
#endif
	close (fd);
}

void
tracker_add_io_grace (const gchar *uri)
{
	if (g_str_has_prefix (uri, tracker->xesam_dir)) {
		return;
	}

	tracker_log ("file changes to %s is pausing tracker", uri);

	tracker->grace_period++;
}


gchar *
tracker_get_status (void)
{
	if (tracker->status < 7) {
                gchar *tracker_status[] = {"Initializing", "Watching", "Indexing", "Pending", "Optimizing", "Idle", "Shutdown"};
                return g_strdup (tracker_status[tracker->status]);
        } else {
                return g_strdup ("Idle");
        }
}


gboolean
tracker_pause_on_battery (void)
{
        if (!tracker->pause_battery) {
                return FALSE;
        }

	if (tracker->first_time_index) {
		return tracker_config_get_disable_indexing_on_battery_init (tracker->config);
	}

        return tracker_config_get_disable_indexing_on_battery (tracker->config);
}


gboolean
tracker_low_diskspace (void)
{
	struct statvfs st;
        gint           low_disk_space_limit;

        low_disk_space_limit = tracker_config_get_low_disk_space_limit (tracker->config);

	if (low_disk_space_limit < 1)
		return FALSE;

	if (statvfs (tracker->data_dir, &st) == -1) {
		static gboolean reported = 0;
		if (! reported) {
			reported = 1;
			tracker_error ("Could not statvfs %s", tracker->data_dir);
		}
		return FALSE;
	}

	if (((long long) st.f_bavail * 100 / st.f_blocks) <= low_disk_space_limit) {
		tracker_error ("Disk space is low!");
		return TRUE;
	}

	return FALSE;
}



gboolean
tracker_index_too_big ()
{

	
	char *file_index = g_build_filename (tracker->data_dir, "file-index.db", NULL);
	if (tracker_file_size (file_index) > MAX_INDEX_FILE_SIZE) {
		tracker_error ("file index is too big - discontinuing index");
		g_free (file_index);
		return TRUE;	
	}
	g_free (file_index);


	char *email_index = g_build_filename (tracker->data_dir, "email-index.db", NULL);
	if (tracker_file_size (email_index) > MAX_INDEX_FILE_SIZE) {
		tracker_error ("email index is too big - discontinuing index");
		g_free (email_index);
		return TRUE;	
	}
	g_free (email_index);


	char *file_meta = g_build_filename (tracker->data_dir, "file-meta.db", NULL);
	if (tracker_file_size (file_meta) > MAX_INDEX_FILE_SIZE) {
		tracker_error ("file metadata is too big - discontinuing index");
		g_free (file_meta);
		return TRUE;	
	}
	g_free (file_meta);


	char *email_meta = g_build_filename (tracker->data_dir, "email-meta.db", NULL);
	if (tracker_file_size (email_meta) > MAX_INDEX_FILE_SIZE) {
		tracker_error ("email metadata is too big - discontinuing index");
		g_free (email_meta);
		return TRUE;	
	}
	g_free (email_meta);

	return FALSE;

}

void
tracker_set_status (Tracker *tracker, TrackerStatus status, gdouble percentage, gboolean signal)
{
	TrackerStatus old = tracker->status;

	tracker->status = status;

	if (signal && old != status)
		tracker_dbus_send_index_status_change_signal ();
}

gboolean
tracker_pause (void)
{
	return tracker->pause_manual || tracker_pause_on_battery () || tracker_low_diskspace () || tracker_index_too_big ();
}
