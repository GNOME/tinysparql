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



#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <zlib.h>
#include "tracker-dbus.h"
#include "tracker-utils.h"
#include "tracker-indexer.h"
#include "tracker-stemmer.h"
#include "../xdgmime/xdgmime.h"

#include "tracker-os-dependant.h"

#ifdef OS_WIN32
#include <conio.h>
#include "mingw-compat.h"
#else
#include <sys/resource.h>
#endif

extern Tracker	*tracker;

char *tracker_actions[] = {
		"TRACKER_ACTION_IGNORE", "TRACKER_ACTION_CHECK", "TRACKER_ACTION_DELETE", "TRACKER_ACTION_DELETE_SELF", "TRACKER_ACTION_CREATE","TRACKER_ACTION_MOVED_FROM",
		"TRACKER_ACTION_MOVED_TO","TRACKER_ACTION_FILE_CHECK", "TRACKER_ACTION_FILE_CHANGED","TRACKER_ACTION_FILE_DELETED", "TRACKER_ACTION_FILE_CREATED",
		"TRACKER_ACTION_FILE_MOVED_FROM", "TRACKER_ACTION_FILE_MOVED_TO", "TRACKER_ACTION_WRITABLE_FILE_CLOSED","TRACKER_ACTION_DIRECTORY_CHECK",
		"TRACKER_ACTION_DIRECTORY_CREATED","TRACKER_ACTION_DIRECTORY_DELETED","TRACKER_ACTION_DIRECTORY_MOVED_FROM","TRACKER_ACTION_DIRECTORY_MOVED_TO",
		"TRACKER_ACTION_DIRECTORY_REFRESH", "TRACKER_ACTION_EXTRACT_METADATA",
		NULL};

char *ignore_suffix[] = {"~", ".o", ".la", ".lo", ".loT", ".in", ".csproj", ".m4", ".rej", ".gmo", ".orig", ".pc", ".omf", ".aux", ".tmp", ".po", NULL};
char *ignore_prefix[] = {"autom4te", "conftest.", "confstat", "config.", NULL};
char *ignore_name[] = {"po", "CVS", "aclocal", "Makefile", "CVS", "SCCS", "ltmain.sh","libtool", "config.status", "conftest", "confdefs.h", NULL};

#define ZLIBBUFSIZ 8192
#define TEXT_SNIFF_SIZE 4096


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


static Matches tmap[] = {
		{"da", "danish"},
		{"nl", "dutch"},
		{"en", "english"},
 		{"fi", "finnish"},
		{"fr", "french"},
		{"de", "german"},
		{"it", "italian"},
		{"nb", "norwegian"},
		{"pt", "portuguese"},
		{"ru", "russian"},
		{"es", "spanish"},
		{"sv", "swedish"},
		{NULL, NULL},
};


char *
tracker_get_service_by_id (int service_type_id)
{
	char *str_id;
	ServiceDef *def;

	str_id  = tracker_int_to_str (service_type_id);
	def = g_hash_table_lookup (tracker->service_id_table, str_id);
	g_free (str_id);

	if (!def) {
		tracker_log ("WARNING: no service found for id %d", service_type_id);
		return NULL;
	}

	return g_strdup (def->name);
}


char *
tracker_get_parent_service_by_id (int service_type_id)
{
	char *str_id = tracker_int_to_str (service_type_id);
	ServiceDef *def = g_hash_table_lookup (tracker->service_id_table, str_id);

	g_free (str_id);

	if (!def) {
		return NULL;
	}

	return g_strdup (def->parent);
}


int
tracker_get_parent_id_for_service_id (int service_type_id)
{
	char *str_id = tracker_int_to_str (service_type_id);
	ServiceDef *def = g_hash_table_lookup (tracker->service_id_table, str_id);

	g_free (str_id);

	if (!def) {
		return -1;
	}

	char *name = g_utf8_strdown (def->parent, -1);

	ServiceDef *def2 = g_hash_table_lookup (tracker->service_table, name);

	g_free (name);

	if (!def2) {
		return -1;
	}

	return  def2->id;
}


int
tracker_get_id_for_service (const char *service)
{
	char *name = g_utf8_strdown (service, -1);

	ServiceDef *def = g_hash_table_lookup (tracker->service_table, name);

	g_free (name);

	if (!def) {
		return -1;
	}

	return def->id;
}


int
tracker_get_id_for_parent_service (const char *service)
{
	char *name = g_utf8_strdown (service, -1);

	ServiceDef *def = g_hash_table_lookup (tracker->service_table, name);

	g_free (name);

	if (!def) {
		return -1;
	}

	return tracker_get_id_for_service (def->parent);
}


char *
tracker_get_parent_service (const char *service)
{
	char *name = g_utf8_strdown (service, -1);

	ServiceDef *def = g_hash_table_lookup (tracker->service_table, name);

	g_free (name);

	if (!def) {
		return NULL;
	}

	if (def->parent) {
		return g_strdup (def->parent);
	}

	return NULL;
}


ServiceDef *
tracker_get_service (const char *service)
{
	char *name = g_utf8_strdown (service, -1);

	ServiceDef *def = g_hash_table_lookup (tracker->service_table, name);

	g_free (name);

	return def;
}


DBTypes
tracker_get_db_for_service (const char *service)
{
	char *name = g_utf8_strdown (service, -1);

	if (g_str_has_prefix (name, "emails") || g_str_has_prefix (name, "attachments")) {
		g_free (name);
		return DB_EMAIL;
	}

	int id = tracker_get_id_for_parent_service (name);
	char *str_id = tracker_int_to_str (id);

//	ServiceDef *def = g_hash_table_lookup (tracker->service_id_table, str_id);
	g_free (str_id);

	g_free (name);

//	if (!def) {
		return DB_DATA;

//	return def->database;
}


gboolean
tracker_is_service_embedded (const char *service)
{
	char *name = g_utf8_strdown (service, -1);

	ServiceDef *def = g_hash_table_lookup (tracker->service_table, name);

	g_free (name);

	if (!def) {
		return FALSE;
	}

	return def->embedded;
}


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


gboolean
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


static inline gboolean
is_in_path (const char *uri, const char *path)
{
	char *tmp = g_strconcat (path, G_DIR_SEPARATOR_S, NULL);

	gboolean result = g_str_has_prefix (uri, tmp);

	g_free (tmp);

	return result;
}


gboolean
tracker_file_is_no_watched (const char* uri)
{
	GSList *lst;

	if (!tracker_check_uri (uri)) {
		return TRUE;
	}

	if (is_in_path (uri, g_get_tmp_dir ())) {
		return TRUE;
	}

	if (is_in_path (uri, "/proc")) {
		return TRUE;
	}

	if (is_in_path (uri, "/dev")) {
		return TRUE;
	}

	if (is_in_path (uri, "/tmp")) {
		return TRUE;
	}

	if (!tracker->no_watch_directory_list) {
		return FALSE;
	}

	for (lst = tracker->no_watch_directory_list; lst; lst = lst->next) {

                char *compare_uri = lst->data;

		/* check if equal or a prefix with an appended '/' */
		if (strcmp (uri, compare_uri) == 0) {
			tracker_debug ("blocking watch of %s", uri);
			return TRUE;
		}

		if (is_in_path (uri, compare_uri)) {
			tracker_debug ("blocking watch of %s", uri);
			return TRUE;
		}
	}

	return FALSE;
}


gboolean
tracker_file_is_crawled (const char* uri)
{
	GSList *lst;

	if (!tracker->crawl_directory_list) {
		return FALSE;
	}

	if (!uri || uri[0] != '/') {
		return FALSE;
	}

	for (lst = tracker->crawl_directory_list; lst; lst = lst->next) {

		char *compare_uri = lst->data;

		/* check if equal or a prefix with an appended '/' */
		if (strcmp (uri, compare_uri) == 0) {
			tracker_debug ("blocking watch of %s", uri);
			return TRUE;
		}

		if (is_in_path (uri, compare_uri)) {
			tracker_debug ("blocking watch of %s", uri);
			return TRUE;
		}
	}

	return FALSE;
}


void
tracker_add_root_dir (const char *uri)
{
	struct stat st;

	if (! tracker->skip_mount_points) {
		return;
	}

	if (!uri || uri[0] != '/') {
		return;
	}

	if (g_stat (uri, &st) == 0) {
		GSList * cur = NULL;
		dev_t * elem = NULL;

		if (! S_ISDIR (st.st_mode)) {
			return;
		}

		/* FIXME: too costly? */
		for (cur = tracker->root_directory_devices; cur; cur = g_slist_next (cur)) {
			if (cur->data && *((dev_t *) cur->data) == st.st_dev) {
				return;
			}
		}

		elem = g_new (dev_t, 1);
		* elem = st.st_dev;
		tracker->root_directory_devices = g_slist_prepend (tracker->root_directory_devices, elem);
	} else {
		tracker_log ("Could not stat `%s'", uri);
	}
}


void
tracker_add_root_directories (GSList * uri_list)
{
	GSList * cur = NULL;

	if (! tracker->skip_mount_points) {
		return;
	}

	for (cur = uri_list; cur; cur = g_slist_next (cur)) {
		tracker_add_root_dir ((const char *) cur->data);
	}
}


gboolean
tracker_file_is_in_root_dir (const char *uri)
{
	struct stat  st;
	GSList *     cur = NULL;
	dev_t        uri_dev = 0;

	if (! tracker->skip_mount_points) {
		return TRUE;
	}

	if (!uri || uri[0] != '/') {
		return FALSE;
	}

	if (g_stat (uri, &st) == 0) {
		uri_dev = st.st_dev;
	} else {
		tracker_log ("Could not stat `%s'", uri);
		return TRUE; /* the caller should take care of skipping this one */
	}

	if (! S_ISDIR (st.st_mode)) { /* only directories are mount points and therefore checked */
		return TRUE;
	}

	for (cur = tracker->root_directory_devices; cur; cur = g_slist_next (cur)) {
		if (cur->data && *((dev_t *) cur->data) == uri_dev) {
			return TRUE;
		}
	}

	return FALSE;
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

	tracker->service_directory_list = g_slist_prepend (tracker->service_directory_list, dir_path);

	g_hash_table_insert (tracker->service_directory_table, dir_path, service_type);
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
	convert_ok = g_file_test (uri_in_locale, G_FILE_TEST_EXISTS);

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

#if defined(__linux__)
	fd = open (uri, O_RDONLY|O_NOATIME);
#else
	fd = open (uri, O_RDONLY); 
#endif

	if (fd ==-1) {
		return FALSE;
	}

	buffer_length = read (fd, buffer, TEXT_SNIFF_SIZE);

	close (fd);

	if (buffer_length < 3) {
		return FALSE;
	}

	/* Don't allow embedded zeros in textfiles. */
	if (memchr (buffer, 0, buffer_length) != NULL) {
		return FALSE;
	}

	if (is_utf8 (buffer, buffer_length)) {
		return TRUE;
	} else {
		gchar *tmp = g_locale_to_utf8 (buffer, buffer_length, NULL, NULL, &err);
		g_free (tmp);

		if (err) {
			gboolean result = FALSE;

			if (err->code != G_CONVERT_ERROR_ILLEGAL_SEQUENCE && err->code !=  G_CONVERT_ERROR_FAILED && err->code != G_CONVERT_ERROR_NO_CONVERSION) {
				result = TRUE;
			}

			g_error_free (err);

			return result;
		}
	}

	return FALSE;
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


static GSList *
get_files (const char *dir, gboolean dir_only, gboolean skip_ignored_files)
{
	GDir	*dirp;
	GSList	*file_list;
	char	*dir_in_locale;

	dir_in_locale = g_filename_from_utf8 (dir, -1, NULL, NULL, NULL);

	if (!dir_in_locale) {
		tracker_error ("ERROR: dir could not be converted to utf8 format");
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

			if (skip_ignored_files && tracker_ignore_file (str)) {
				g_free (str);
				continue;
			}

			mystr = g_build_filename (dir, str, NULL);
			g_free (str);

 			if (!tracker_file_is_valid (mystr)) {
				g_free (mystr);
				continue;
			}

			if (!tracker_file_is_in_root_dir (mystr)) {
				tracker_log ("Skipping mount point %s", mystr);
				g_free (mystr);
				continue;
			}

			if (!dir_only || tracker_is_directory (mystr)) {

				if (!tracker_file_is_no_watched (mystr)) {
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


GSList *
tracker_get_all_files (const char *dir, gboolean dir_only)
{
	return get_files (dir, dir_only, FALSE);
}


GSList *
tracker_get_files (const char *dir, gboolean dir_only)
{
	return get_files (dir, dir_only, TRUE);
}


static void
get_dirs (const char *dir, GSList **file_list, gboolean skip_ignored_files)
{
	GSList *tmp_list;

	g_return_if_fail (dir);

	tmp_list = (skip_ignored_files ?
		    tracker_get_files (dir, TRUE) :
		    tracker_get_all_files (dir, TRUE));

	if (tmp_list) {			/* check list is not empty */
		if (*file_list) {	/* check this list too */
			*file_list = g_slist_concat (*file_list, tmp_list);
		} else {
			*file_list = tmp_list;
		}
	}
}


void
tracker_get_dirs (const char *dir, GSList **file_list)
{
	get_dirs (dir, file_list, TRUE);
}


void
tracker_get_all_dirs (const char *dir, GSList **file_list)
{
	get_dirs (dir, file_list, FALSE);
}


static GSList *
check_dir_name (GSList* list, char *input_name)
{
	tracker_debug ("checking dir names : %s", input_name);

	int is_converted = FALSE;

	if (input_name[0] == '~') {

		const char* home_dir = g_get_home_dir ();

		if ((home_dir != NULL) && (home_dir[0] != '\0')) {

			int is_separator = FALSE;
			char *new_name;

			if (input_name[1] == G_DIR_SEPARATOR) {
				is_separator = TRUE;
			} else {
				if (home_dir[strlen (home_dir)-1] == G_DIR_SEPARATOR) {
					is_separator = TRUE;
				}
			}

			new_name = g_strdup_printf ("%s%s%s", home_dir, (is_separator? "" : G_DIR_SEPARATOR_S), &input_name[1]);

			if ((new_name != NULL) && (strlen (new_name) <= PATH_MAX)) {

				char resolved_name[PATH_MAX+2];

				if (realpath (new_name, resolved_name) != NULL) {
					list = g_slist_prepend (list, g_strdup (resolved_name));
				} else {
					list = g_slist_prepend (list, g_strdup (new_name));
				}

				is_converted = TRUE;
			}

			if (new_name != NULL) {
				g_free (new_name);
			}
		}
	}

	if (!is_converted) {

		char resolved_name[PATH_MAX+2];

		if( realpath (input_name, resolved_name) != NULL ) {
			list = g_slist_prepend (list, g_strdup (resolved_name));
		} else {
			list = g_slist_prepend (list, g_strdup (input_name));
		}
	}

	tracker_debug ("resolved to %s\n", list->data);

	return list;
}


GSList *
tracker_filename_array_to_list (char **array)
{
	GSList *list;
	int    i;

	list = NULL;

	for (i = 0; array[i] != NULL; i++) {
                if (!tracker_is_empty_string (array[i])) {
			list = check_dir_name (list, array[i]);
		}
	}

	g_strfreev (array);

	return list;
}


static GSList *
array_to_list (char **array)
{
	GSList  *list;
	int	i;

	list = NULL;

	for (i = 0; array[i] != NULL; i++) {
                if (!tracker_is_empty_string (array[i])) {
			list = g_slist_prepend (list, g_strdup (array[i]));
		}
	}

	g_strfreev (array);

	return list;
}


GSList *
tracker_array_to_list (char **array)
{
	return array_to_list (array);
}


char **
tracker_list_to_array (GSList *list)
{
	GSList *tmp;
	char **array;
	int i;

	array = g_new0 (char *, g_slist_length (list) + 1);

	i = 0;

	for (tmp = list; tmp; tmp = tmp->next) {
		if (tmp->data) {
			array[i] = g_strdup (tmp->data);
			i++;
		}
	}

	array[i] = NULL;

	return array;
}


void
tracker_free_strs_in_array (char **array)
{
	char **a;

	for (a = array; *a; a++) {
		g_free (*a);
	}
}


gboolean
tracker_ignore_file (const char *uri)
{
	char *name;
	char **st;

	if (tracker_is_empty_string (uri)) {
		return TRUE;
	}

	name = g_path_get_basename (uri);

	if (!name || name[0] == '.') {
		g_free (name);
		return TRUE;
	}


	/* test suffixes */
	for (st = ignore_suffix; *st; st++) {
		if (g_str_has_suffix (name, *st)) {
			g_free (name);
			return TRUE;
		}
	}

	/* test prefixes */
	for (st = ignore_prefix; *st; st++) {
		if (g_str_has_prefix (name, *st)) {
			g_free (name);
			return TRUE;
		}
	}

	/* test exact names */
	for (st = ignore_name; *st; st++) {
		if (strcmp (name, *st) == 0) {
			g_free (name);
			return TRUE;
		}
	}


	g_free (name);
	return FALSE;
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

char *
tracker_get_english_lang_code (void)
{
        return g_strdup (tmap[2].lang);
}


gboolean
tracker_is_supported_lang (const char *lang)
{
	int i;

	for (i = 0; tmap[i].lang; i++) {
		if (g_str_has_prefix (lang, tmap[i].lang)) {
			return TRUE;
		}
	}

	return FALSE;
}


static char *
get_default_language_code (void)
{
	char **langs, **plangs;

	/* get langauges for user's locale */
	langs = (char**) g_get_language_names ();

	for (plangs = langs; *plangs; plangs++) {
		if (strlen (*plangs) > 1) {
                        int i;
			for (i = 0; tmap[i].lang; i++) {
				if (g_str_has_prefix (*plangs, tmap[i].lang)) {
					return g_strndup (*plangs, 2);
				}
			}
		}
	}

	return g_strdup ("en");
}


void
tracker_set_language (const char *language, gboolean create_stemmer)
{
	if (!language || strlen (language) < 2) {
                if (tracker->language) {
                        g_free (tracker->language);
                }
		tracker->language = get_default_language_code ();
		tracker_log ("Setting default language code to %s based on user's locale", language);

	} else {
		int i;
		for (i = 0; tmap[i].lang; i++) {
			if (g_str_has_prefix (language, tmap[i].lang)) {
                                if (tracker->language) {
                                        g_free (tracker->language);
                                }
                                tracker->language = g_strdup (tmap[i].lang);
				break;
			}
		}
	}

	/* set stopwords list and create stemmer for language */
	tracker_log ("setting stopword list for language code %s", language);

	char *stopword_path, *stopword_file, *stopword_en_file = NULL;
	char *stopwords = NULL;

	stopword_path = g_build_filename (TRACKER_DATADIR, "tracker", "languages", "stopwords", NULL);
	stopword_file = g_strconcat (stopword_path, ".", language, NULL);

	if (strcmp (language, "en") != 0) {
		stopword_en_file = g_strconcat (stopword_path, ".en", NULL);
	}


	if (!g_file_get_contents (stopword_file, &stopwords, NULL, NULL)) {
		tracker_log ("Warning : Tracker cannot read stopword file %s", stopword_file);
	} else {

		tracker->stop_words = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

		char **words = g_strsplit_set (stopwords, "\n" , -1);
		char **pwords;

		for (pwords = words; *pwords; pwords++) {
			g_hash_table_insert (tracker->stop_words, g_strdup (g_strstrip (*pwords)), GINT_TO_POINTER (1));
		}

		g_strfreev (words);
	}


	if (stopword_en_file) {
		g_free (stopwords);
		if (!g_file_get_contents (stopword_en_file, &stopwords, NULL, NULL)) {
			tracker_log ("Warning : Tracker cannot read stopword file %s", stopword_file);
		} else {

			tracker->stop_words = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

			char **words = g_strsplit_set (stopwords, "\n" , -1);
			char **pwords;

			for (pwords = words; *pwords; pwords++) {
				g_hash_table_insert (tracker->stop_words, g_strdup (g_strstrip (*pwords)), GINT_TO_POINTER (1));
			}

			g_strfreev (words);
		}
		g_free (stopword_en_file);
	}

	g_free (stopword_path);
	g_free (stopwords);
	g_free (stopword_file);

	if (!tracker->use_stemmer || !create_stemmer) {
		return;
	}

	char *stem_language;

	/* set default language */
	stem_language = "english";

	if (language) {
		int i;

		for (i = 0; tmap[i].lang; i++) {
			if ((strcasecmp (tmap[i].lang, language) == 0)) {
				stem_language = tmap[i].name;
				break;
			}
		}
	}

	tracker->stemmer = sb_stemmer_new (stem_language, NULL);

	if (!tracker->stemmer) {
		tracker_log ("Warning : No stemmer could be found for language %s", stem_language);
	} else {
		tracker_log ("Using stemmer for language %s\n", stem_language);
	}
}


void
tracker_load_config_file (void)
{
	GKeyFile *key_file;
	char	 *filename;
	char	 **values;

	key_file = g_key_file_new ();

	filename = g_build_filename (tracker->config_dir, "/tracker/tracker.cfg", NULL);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
                char *tracker_dir = g_build_filename (tracker->config_dir,"/tracker",NULL);

                if (!g_file_test (tracker_dir,G_FILE_TEST_EXISTS)) {
                    g_mkdir_with_parents (tracker_dir,0700);
                }

		g_free (tracker_dir);

		char *contents, *language;

		language = get_default_language_code ();

		contents  = g_strconcat (
					 "[General]\n",
					 "# Log Verbosity - Valid values are 0 (displays/logs only errors), 1 (minimal), 2 (detailed), and 3 (debug)\n",
					 "Verbosity=0\n\n",
					 "# Set the initial sleeping time, in seconds\n",
					 "InitialSleep=45\n\n",
					 "# Minimizes the use of memory but may slow indexing down\n", 
					 "LowMemoryMode=false\n\n",
					 "[Watches]\n",
					 "# List of directory roots to index and watch seperated by semicolons\n",
					 "WatchDirectoryRoots=", g_get_home_dir (), ";\n",
					 "# List of directory roots to index but not watch (no live updates but are refreshed when trackerd is next restarted) seperated by semicolons\n",
					 "CrawlDirectory=\n",
					 "# List of directory roots to not index and not watch seperated by semicolons\n",
					 "NoWatchDirectory=\n",
					 "# Set to false to prevent watching of any kind\n",
					 "EnableWatching=true\n\n",
					 "[Indexing]\n",
					 "# Throttles the indexing process. Allowable values are 0-20. higher values decrease indexing speed\n",
					 "Throttle=0\n",
					 "# Disables the indexing process\n",
					 "EnableIndexing=true\n",
					 "# Enables indexing of a file's text contents\n",
					 "EnableFileContentIndexing=true\n",
					 "# Enables generation of thumbnails\n",
					 "EnableThumbnails=false\n",
					 "# List of partial file patterns (glob) seperated by semicolons that specify files to not index (basic stat info is only indexed for files that match these patterns)\n",
					 "NoIndexFileTypes=;\n\n",
					 "# Sets minimum length of words to index\n",
					 "MinWordLength=3\n",
					 "# Sets maximum length of words to index (words are cropped if bigger than this)\n",
					 "MaxWordLength=30\n",
					 "# Sets the language specific stemmer and stopword list to use \n",
					 "# Valid values are 'en' (english), 'da' (danish), 'nl' (dutch), 'fi' (finnish), 'fr' (french), 'de' (german), 'it' (italien), 'nb' (norwegian), 'pt' (portugese), 'ru' (russian), 'es' (spanish), 'sv' (swedish)\n",
					 "Language=", language, "\n",
					 "# Enables use of language-specific stemmer\n",
					 "EnableStemmer=true\n",
					 "# Set to true prevents tracker from descending into mounted directory trees\n",
					 "SkipMountPoints=false\n\n",
					 "[Emails]\n",
					 "IndexEvolutionEmails=true\n",
					 "[Performance]\n",
					 "# Maximum size of text in bytes to index from a file's text contents\n",
					 "MaxTextToIndex=1048576\n",
					 "# Maximum number of unique words to index from a file's text contents\n",
					 "MaxWordsToIndex=10000\n",
					 "# Specifies the no of entities to index before determining whether to perform index optimization\n",
					 "OptimizationSweepCount=10000\n",
					 "# Sets the maximum bucket count for the indexer\n",
					 "MaxBucketCount=524288\n",
					 "# Sets the minimum bucket count\n",
					 "MinBucketCount=65536\n",
					 "# Sets no. of divisions of the index file\n",
					 "Dvisions=4\n",
					 "# Selects the desired ratio of used records to buckets to be used when optimizing index (should be a value between 0 and 4) \n",
					 "BucketRatio=1\n",
					 "# Alters how much padding is used to prevent index relocations. Higher values improve indexing speed but waste more disk space. Value should be in range (1..8)\n",
					 "Padding=2\n",
					 NULL);

		g_file_set_contents (filename, contents, strlen (contents), NULL);
		g_free (contents);
	}

	/* load all options into tracker struct */
	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL);

	/* general options */

	if (g_key_file_has_key (key_file, "General", "Verbosity", NULL)) {
		tracker->verbosity = g_key_file_get_integer (key_file, "General", "Verbosity", NULL);
	}

	if (g_key_file_has_key (key_file, "General", "LowMemoryMode", NULL)) {
		tracker->use_extra_memory = !g_key_file_get_boolean (key_file, "General", "LowMemoryMode", NULL);
	}

	if (g_key_file_has_key (key_file, "General", "InitialSleep", NULL)) {
		tracker->initial_sleep = g_key_file_get_integer (key_file, "General", "InitialSleep", NULL);
	}

	/* Watch options */

	values =  g_key_file_get_string_list (key_file,
					      "Watches",
					      "WatchDirectoryRoots",
					      NULL,
					      NULL);

	if (values) {
		tracker->watch_directory_roots_list = tracker_filename_array_to_list (values);
	} else {
		tracker->watch_directory_roots_list = g_slist_prepend (tracker->watch_directory_roots_list, g_strdup (g_get_home_dir ()));
	}



	values =  g_key_file_get_string_list (key_file,
			       	     	      "Watches",
				              "CrawlDirectory",
				              NULL,
				              NULL);

	if (values) {
		tracker->crawl_directory_list = tracker_filename_array_to_list (values);

	} else {
		tracker->crawl_directory_list = NULL;
	}


	values =  g_key_file_get_string_list (key_file,
			       	     	      "Watches",
				              "NoWatchDirectory",
				              NULL,
				              NULL);

	if (values) {
		tracker->no_watch_directory_list = tracker_filename_array_to_list (values);

	} else {
		tracker->no_watch_directory_list = NULL;
	}


	if (g_key_file_has_key (key_file, "Watches", "EnableWatching", NULL)) {
		tracker->enable_watching = g_key_file_get_boolean (key_file, "Watches", "EnableWatching", NULL);
	}


	/* Indexing options */

	if (g_key_file_has_key (key_file, "Indexing", "Throttle", NULL)) {
		tracker->throttle = g_key_file_get_integer (key_file, "Indexing", "Throttle", NULL);
	} else {
		tracker->throttle = 0;
	}

	if (g_key_file_has_key (key_file, "Indexing", "BatteryThrottle", NULL)) {
		tracker->throttle = g_key_file_get_integer (key_file, "Indexing", "BatteryThrottle", NULL);
	} else {
		tracker->battery_throttle = 10;
	}

	if (g_key_file_has_key (key_file, "Indexing", "EnableIndexing", NULL)) {
		tracker->enable_indexing = g_key_file_get_boolean (key_file, "Indexing", "EnableIndexing", NULL);
	} else {
		tracker->enable_indexing = TRUE;
	}

	if (g_key_file_has_key (key_file, "Indexing", "EnableFileContentIndexing", NULL)) {
		tracker->enable_content_indexing = g_key_file_get_boolean (key_file, "Indexing", "EnableFileContentIndexing", NULL);
	} else {
		tracker->enable_content_indexing = TRUE;
	}

	if (g_key_file_has_key (key_file, "Indexing", "EnableThumbnails", NULL)) {
		tracker->enable_thumbnails = g_key_file_get_boolean (key_file, "Indexing", "EnableThumbnails", NULL);
	} else {
		tracker->enable_thumbnails = FALSE;
	}

	values =  g_key_file_get_string_list (key_file,
			       	     	      "Indexing",
				              "NoIndexFileTypes",
				              NULL,
				              NULL);

	if (values) {
		tracker->no_index_file_types = array_to_list (values);
	} else {
		tracker->no_index_file_types = NULL;
	}


	if (g_key_file_has_key (key_file, "Indexing", "MinWordLength", NULL)) {
		tracker->min_word_length = g_key_file_get_integer (key_file, "Indexing", "MinWordLength", NULL);
	} else {
		tracker->min_word_length = 3;
	}

	if (g_key_file_has_key (key_file, "Indexing", "MaxWordLength", NULL)) {
		tracker->max_word_length = g_key_file_get_integer (key_file, "Indexing", "MaxWordLength", NULL);
	} else {
		tracker->max_word_length = 40;
	}

	if (g_key_file_has_key (key_file, "Indexing", "EnableStemmer", NULL)) {
		tracker->use_stemmer = g_key_file_get_boolean (key_file, "Indexing", "EnableStemmer", NULL);
	} else {
		tracker->use_stemmer = TRUE;
	}

	if (g_key_file_has_key (key_file, "Indexing", "Language", NULL)) {
		tracker->language = g_key_file_get_string (key_file, "Indexing", "Language", NULL);
	}

	if (g_key_file_has_key (key_file, "Indexing", "SkipMountPoints", NULL)) {
		tracker->skip_mount_points = g_key_file_get_boolean (key_file, "Indexing", "SkipMountPoints", NULL);
	}



	/* Emails config */

	tracker->additional_mboxes_to_index = NULL;

	if (g_key_file_has_key (key_file, "Emails", "IndexEvolutionEmails", NULL)) {
		tracker->index_evolution_emails = g_key_file_get_boolean (key_file, "Emails", "IndexEvolutionEmails", NULL);
	} else {
		tracker->index_evolution_emails = TRUE;
	}

	if (g_key_file_has_key (key_file, "Emails", "IndexKMailEmails", NULL)) {
		tracker->index_kmail_emails = g_key_file_get_boolean (key_file, "Emails", "IndexKMailEmails", NULL);
	} else {
		tracker->index_kmail_emails = FALSE;
	}


	/* Performance options */

	if (g_key_file_has_key (key_file, "Performance", "MaxTextToIndex", NULL)) {
		tracker->max_index_text_length = g_key_file_get_integer (key_file, "Performance", "MaxTextToIndex", NULL);

	}

	if (g_key_file_has_key (key_file, "Performance", "OptimizationSweepCount", NULL)) {
		tracker->optimization_count = g_key_file_get_integer (key_file, "Performance", "OptimizationSweepCount", NULL);

	}

	if (g_key_file_has_key (key_file, "Performance", "MaxBucketCount", NULL)) {
		tracker->max_index_bucket_count = g_key_file_get_integer (key_file, "Performance", "MaxBucketCount", NULL);

	}

	if (g_key_file_has_key (key_file, "Performance", "MinBucketCount", NULL)) {
		tracker->min_index_bucket_count = g_key_file_get_integer (key_file, "Performance", "MinBucketCount", NULL);

	}

	if (g_key_file_has_key (key_file, "Performance", "Dvisions", NULL)) {
		tracker->index_divisions = g_key_file_get_integer (key_file, "Performance", "Dvisions", NULL);

	}

	if (g_key_file_has_key (key_file, "Performance", "BucketRatio", NULL)) {
		tracker->index_bucket_ratio = g_key_file_get_integer (key_file, "Performance", "BucketRatio", NULL);

	}

	if (g_key_file_has_key (key_file, "Performance", "Padding", NULL)) {
		tracker->padding = g_key_file_get_integer (key_file, "Performance", "Padding", NULL);

	}

	if (g_key_file_has_key (key_file, "Performance", "MaxWordsToIndex", NULL)) {
		tracker->max_words_to_index = g_key_file_get_integer (key_file, "Performance", "MaxWordsToIndex", NULL);
	}


	g_free (filename);

	g_key_file_free (key_file);
}


void
tracker_throttle (int multiplier)
{
	if (tracker->throttle == 0) {
		return;
	}

	int throttle;

	throttle = tracker->throttle * multiplier;

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






static inline void
output_log (const char *message)
{
	FILE		*fd;
	time_t		now;
	char		buffer1[64], buffer2[20];
	char		*output;
	struct tm	*loctime;
	GTimeVal	start;

	if (message) {
		g_print ("%s\n", message);
	}

	/* ensure file logging is thread safe */
	g_mutex_lock (tracker->log_access_mutex);

	fd = g_fopen (tracker->log_file, "a");

	if (!fd) {
		g_mutex_unlock (tracker->log_access_mutex);
		g_warning ("could not open %s", tracker->log_file);
		return;
	}

	g_get_current_time (&start);

	now = time ((time_t *) NULL);

	loctime = localtime (&now);

	strftime (buffer1, 64, "%d %b %Y, %H:%M:%S:", loctime);

	g_sprintf (buffer2, "%03ld", start.tv_usec / 1000);

	output = g_strconcat (buffer1, buffer2, " - ", message, NULL);

	g_fprintf (fd, "%s\n", output);

	g_free (output);

	fclose (fd);

	g_mutex_unlock (tracker->log_access_mutex);
}


void
tracker_error (const char *message, ...)
{
	va_list		args;
	char 		*msg;

  	va_start (args, message);
  	msg = g_strdup_vprintf (message, args);
	va_end (args);

	output_log (msg);
	g_free (msg);

	if (tracker->fatal_errors) {
		g_assert (FALSE);
	}
}


void
tracker_log 	(const char *message, ...)
{
	va_list		args;
	char 		*msg;

	if (tracker->verbosity < 1) {
		return;
	}

  	va_start (args, message);
  	msg = g_strdup_vprintf (message, args);
  	va_end (args);

	output_log (msg);
	g_free (msg);
}


void
tracker_info	(const char *message, ...)
{
	va_list		args;
	char 		*msg;

	if (tracker->verbosity < 2) {
		return;
	}

  	va_start (args, message);
  	msg = g_strdup_vprintf (message, args);
  	va_end (args);

	output_log (msg);
	g_free (msg);
}


void
tracker_debug 	(const char *message, ...)
{
	va_list		args;
	char 		*msg;

	if (tracker->verbosity < 3) {
		return;
	}

  	va_start (args, message);
  	msg = g_strdup_vprintf (message, args);
  	va_end (args);

	output_log (msg);
	g_free (msg);
}


char *
tracker_string_replace (const char *haystack, char *needle, char *replacement)
{
        GString *str;
        int pos, needle_len;

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


gboolean
tracker_using_battery (void)
{
	if (!tracker->battery_state_file) {
		return FALSE;
	}

	char *txt;
	gboolean using_battery;

	if (!g_file_get_contents (tracker->battery_state_file, &txt, NULL, NULL)) {
		return FALSE;
	} else {
		using_battery = (strstr (txt, "on-line") == NULL);
	}

	g_free (txt);

	return using_battery;
}


void
tracker_add_metadata_to_table (GHashTable *meta_table, const char *key, const char *value)
{
	GSList *list;
	
	list = g_hash_table_lookup (meta_table, (char *) key);

	list = g_slist_prepend (list, (char *) value);

	g_hash_table_steal (meta_table, key);

	g_hash_table_insert (meta_table, (char *) key, list);

	
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
tracker_unlink (const char *uri)
{
	char *locale_uri = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	if (!g_file_test (locale_uri, G_FILE_TEST_EXISTS)) {						
				g_free (locale_uri);
				return FALSE;
	}

	g_unlink (locale_uri);

	g_free (locale_uri);

	return TRUE;
}
