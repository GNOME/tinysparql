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

#include <config.h>

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

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


static gchar *date_to_iso8160 (gchar *date)
{
        /* ex. date: "22 May 1997 18:07:10 -0600"
           To
           ex. ISO8160 date: "2007-05-22T18:07:10-0600"
        */
        gchar buffer[20];
        gchar **date_parts, **part;
        size_t count;

        g_return_val_if_fail (date, NULL);

        typedef enum {DAY_STR = 0, MONTH, DAY, TIME, YEAR, LAST_STEP} steps;
        steps step;

        struct tm tm;
        memset (&tm, 0, sizeof (struct tm));

        date_parts = g_strsplit (date, " ", 0);

        for (part = date_parts, step = DAY_STR; *part && step != LAST_STEP; part++, step++) {
                switch (step) {
                        case DAY_STR: {
                                /* We do not care about monday, tuesday, etc. */
                                break;
                        }
                        case DAY: {
                                guint64 val = g_ascii_strtoull (*part, NULL, 10);
                                tm.tm_mday = CLAMP (val, 1, 31);
                                break;
                        }
                        case MONTH: {
                                gchar *months[] = {
                                        "Jan", "Fe", "Mar", "Av", "Ma", "Jun",
                                        "Jul", "Au", "Se", "Oc", "No", "De",
                                        NULL };
                                gchar **tmp;
                                gint i;

                                for (tmp = months, i = 0; *tmp; tmp++, i++) {
                                        if (g_str_has_prefix (*part, *tmp)) {
                                                tm.tm_mon = i;
                                                break;
                                        }
                                }
                                break;
                        }
                        case YEAR : {
                                guint64 val = g_ascii_strtoull (*part, NULL, 10);
                                tm.tm_year = CLAMP (val, 0, G_MAXINT) - 1900;
                                break;
                        }
                        case TIME: {
                                gchar *n = *part;

                                #define READ_PAIR(ret, min, max)                        \
                                {                                                       \
                                        gchar buff[3];                                  \
                                        guint64 val;                                    \
                                        buff[0] = n[0];                                 \
                                        buff[1] = n[1];                                 \
                                        buff[2] = '\0';                                 \
                                                                                        \
                                        val = g_ascii_strtoull (buff, NULL, 10);        \
                                        ret = CLAMP (val, min, max);                    \
                                        n += 2;                                         \
                                }

                                READ_PAIR (tm.tm_hour, 0, 24);
                                if (*n++ != ':') {
                                        goto error;
                                }
                                READ_PAIR (tm.tm_min, 0, 99);
                                if (*n++ != ':') {
                                        goto error;
                                }
                                READ_PAIR (tm.tm_sec, 0, 99);

                                #undef READ_PAIR

                                break;
                        }
                        default:
                                /* that cannot happen! */
                                g_strfreev (date_parts);
                                g_return_val_if_reached (NULL);
                }
        }

        count = strftime (buffer, sizeof (buffer), "%FT%T", &tm);

        g_strfreev (date_parts);

        return (count > 0) ? g_strdup (buffer) : NULL;

 error:
        g_strfreev (date_parts);
        return NULL;
}


void tracker_extract_ps (gchar *filename, GHashTable *metadata)
{
	FILE *f;

	if ((f = g_fopen (filename, "r"))) {
		gchar *line = NULL;
                gsize length = 0;

		getline (&line, &length, f);

		while (!feof (f)) {
                        gboolean pageno_atend = FALSE;
                        gboolean header_finished = FALSE;

			line[strlen (line) - 1] = '\0';  /* overwrite '\n' char */

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
				g_hash_table_insert (metadata,
                                                     g_strdup ("Doc:Created"), date_to_iso8160 (line + 16));

			} else if (strncmp (line, "%%Pages:", 8) == 0) {
                                if (strcmp (line + 9, "(atend)") == 0) {
					pageno_atend = TRUE;
				} else {
					g_hash_table_insert (metadata,
                                                             g_strdup ("Doc:PageCount"), g_strdup (line + 9));
                                }

			} else if (strncmp (line, "%%EndComments", 14) == 0) {
				header_finished = TRUE;
				if (!pageno_atend)
					break;
			}

			g_free (line);

			line = NULL;
			length = 0;
			getline (&line, &length, f);
		}

		g_free (line);
	}

	fclose (f);
}
