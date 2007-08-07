/* Tracker Extract - extracts embedded metadata from files
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#define _GNU_SOURCE

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "tracker-extract.h"

#if !HAVE_GETLINE
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#undef getdelim
#undef getline

static ssize_t
igetdelim (gchar **linebuf, size_t *linebufsz, gint delimiter, FILE *file)
{
	static const gint GROWBY = 80; /* how large we will grow strings by */
	gint ch;
	gint idx;

	if ((file == NULL || linebuf == NULL || *linebuf == NULL || *linebufsz == 0)
            && !(*linebuf == NULL && *linebufsz == 0)) {
                errno = EINVAL;
		return -1;
	}

	if (*linebuf == NULL && *linebufsz == 0){
		*linebuf = g_malloc (GROWBY);
		if (!*linebuf) {
			errno = ENOMEM;
			return -1;
		}
		*linebufsz += GROWBY;
	}

        idx = 0;

        while ((ch = fgetc (file)) != EOF) {
		/* grow the line buffer as necessary */
		while (idx > *linebufsz - 2) {
			*linebuf = g_realloc (*linebuf, *linebufsz += GROWBY);
			if (!*linebuf) {
				errno = ENOMEM;
				return -1;
			}
		}
		(*linebuf)[idx++] = (char)ch;
		if ((char)ch == delimiter) {
			break;
                }
	}

	if (idx != 0) {
		(*linebuf)[idx] = 0;
	} else if ( ch == EOF ) {
		return -1;
        }

	return idx;
}


gint
getline (gchar **s, guint *lim, FILE *stream)
{
	return igetdelim (s, lim, '\n', stream);
}
#endif


static gchar
*hour_day_str_day (gchar *date)
{
        /* ex. date: "(18:07 Tuesday 22 May 2007)"
           To
           ex. ISO8160 date: "2007-05-22T18:07:10-0600"
        */

        steps steps_to_do[] = {
                TIME, DAY_STR, DAY, MONTH, YEAR, LAST_STEP
        };

        return tracker_generic_date_extractor (date + 1, steps_to_do);
}


static gchar *
day_str_month_day (gchar *date)
{
        /* ex. date: "Tue May 22 18:07:10 2007"
           To
           ex. ISO8160 date: "2007-05-22T18:07:10-0600"
        */

        steps steps_to_do[] = {
                DAY_STR, MONTH, DAY, TIME, YEAR, LAST_STEP
        };

        return tracker_generic_date_extractor (date, steps_to_do);
}


static gchar *
day_month_year_date (gchar *date)
{
        /* ex. date: "22 May 1997 18:07:10 -0600"
           To
           ex. ISO8160 date: "2007-05-22T18:07:10-0600"
        */

        steps steps_to_do[] = {
                DAY_STR, MONTH, DAY, TIME, YEAR, LAST_STEP
        };

        return tracker_generic_date_extractor (date, steps_to_do);
}


static gchar *
hour_month_day_date (gchar *date)
{
        /* ex. date: "6:07 PM May 22, 2007"
           To
           ex. ISO8160 date: "2007-05-22T18:07:10-0600"
        */

        steps steps_to_do[] = {
                TIME, DAY_PART, MONTH, DAY, YEAR, LAST_STEP
        };

        return tracker_generic_date_extractor (date, steps_to_do);
}


static gchar *
date_to_iso8160 (gchar *date)
{
        if (date && date[1] && date[2]) {
                if (date[0] == '(') {
                        /* we have probably a date like
                           "(18:07 Tuesday 22 May 2007)" */
                        return hour_day_str_day (date);
                } else if (g_ascii_isalpha (date[0])) {
                        /* we have probably a date like
                           "Tue May 22 18:07:10 2007" */
                        return day_str_month_day (date);

                } else if (date[1] == ' ' || date[2] == ' ') {
                        /* we have probably a date like
                           "22 May 1997 18:07:10 -0600" */
                        return day_month_year_date (date);

                } else if (date[1] == ':' || date[2] == ':') {
                        /* we have probably a date like
                           "6:07 PM May 22, 2007" */
                        return hour_month_day_date (date);

                }
        }

        return NULL;
}


void
tracker_extract_ps (gchar *filename, GHashTable *metadata)
{
	FILE *f;

	if ((f = g_fopen (filename, "r"))) {
                gchar  *line;
                gsize  length;
                gssize read_char;

		line = NULL;
                length = 0;

                while ((read_char = getline (&line, &length, f)) != -1) {
                        gboolean pageno_atend    = FALSE;
                        gboolean header_finished = FALSE;

			line[read_char - 1] = '\0';  /* overwrite '\n' char */

			if (!header_finished && strncmp (line, "%%Copyright:", 12) == 0) {
                                g_hash_table_insert (metadata,
                                                     g_strdup ("File:Other"), g_strdup (line + 13));

			} else if (!header_finished && strncmp (line, "%%Title:", 8) == 0) {
				g_hash_table_insert (metadata,
                                                     g_strdup ("Doc:Title"), g_strdup (line + 9));

			} else if (!header_finished && strncmp (line, "%%Creator:", 10) == 0) {
				g_hash_table_insert (metadata,
                                                     g_strdup ("Doc:Author"), g_strdup (line + 11));

			} else if (!header_finished && strncmp (line, "%%CreationDate:", 15) == 0) {
                                gchar *date = date_to_iso8160 (line + 16);
                                if (date) {
                                        g_hash_table_insert (metadata, g_strdup ("Doc:Created"), date);
                                }

			} else if (strncmp (line, "%%Pages:", 8) == 0) {
                                if (strcmp (line + 9, "(atend)") == 0) {
					pageno_atend = TRUE;
				} else {
					g_hash_table_insert (metadata,
                                                             g_strdup ("Doc:PageCount"), g_strdup (line + 9));
                                }

			} else if (strncmp (line, "%%EndComments", 14) == 0) {
				header_finished = TRUE;
				if (!pageno_atend) {
					break;
                                }
			}

			g_free (line);
			line = NULL;
                        length = 0;
		}

                if (line) {
                        g_free (line);
                }

                fclose (f);
	}
}
