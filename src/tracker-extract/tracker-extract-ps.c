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

#include <stdio.h>
#include <string.h>
#include <glib.h>

#if !HAVE_GETLINE
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

static ssize_t getdelim(char **linebuf, size_t *linebufsz, int delimiter, FILE *file)
{
	static const int GROWBY = 80; /* how large we will grow strings by */
	int ch;
	int idx = 0;

	if ((file == NULL || linebuf==NULL || *linebuf == NULL || *linebufsz == 0)
			&& !(*linebuf == NULL && *linebufsz ==0 )) {
		errno=EINVAL;
		return -1;
	}

	if (*linebuf == NULL && *linebufsz == 0){
		*linebuf = malloc(GROWBY);
		if (!*linebuf) {
			errno=ENOMEM;
			return -1;
		}
		*linebufsz += GROWBY;
	}

	while (1) {
		ch = fgetc(file);
		if (ch == EOF)
			break;
		/* grow the line buffer as necessary */
		while (idx > *linebufsz-2) {
			*linebuf = realloc(*linebuf, *linebufsz += GROWBY);
			if (!*linebuf) {
				errno=ENOMEM;
				return -1;
			}
		}
		(*linebuf)[idx++] = (char)ch;
		if ((char)ch == delimiter)
			break;
	}

	if (idx != 0)
		(*linebuf)[idx] = 0;
	else if ( ch == EOF )
		return -1;
	return idx;
}





int getline(char **s, unsigned int *lim, FILE *stream)
{
	return getdelim(s, lim, '\n', stream);
}
#endif

void tracker_extract_ps (gchar *filename, GHashTable *metadata)
{
	FILE        *f;
	gchar       *line;
	gsize        length = 0;
	gboolean     pageno_atend = FALSE;
	gboolean     header_finished = FALSE;
   
	if((f = fopen (filename, "r"))) {
		line = NULL;
		getline (&line, &length, f);
		while (!feof (f)) {
			line[strlen(line) - 1] = '\0';  /* overwrite \n char */
			if (!header_finished
					&& strncmp (line, "%%Copyright:", 12) == 0) {
				g_hash_table_insert (metadata,
					g_strdup ("File:Other"), g_strdup (line+13));
			}
			else if (!header_finished
					&& strncmp (line, "%%Title:", 8) == 0) {
				g_hash_table_insert (metadata,
					g_strdup ("Doc:Title"), g_strdup (line+9));
			}
			else if (!header_finished
					&& strncmp (line, "%%Creator:", 10) == 0) {
				g_hash_table_insert (metadata,
					g_strdup ("Doc:Author"), g_strdup (line+11));
			}
			else if (!header_finished
					&& strncmp (line, "%%CreationDate:", 15) == 0) {
				g_hash_table_insert (metadata,
					g_strdup ("Doc:Created"), g_strdup (line+16));
			}
			else if (strncmp (line, "%%Pages:", 8) == 0) {
				if (strcmp (line+9, "(atend)") == 0)
					pageno_atend = TRUE;
				else
					g_hash_table_insert (metadata,
						g_strdup ("Doc:PageCount"), g_strdup (line+9));
			}
			else if (strncmp (line, "%%EndComments", 14) == 0) {
				header_finished = TRUE;
				if (!pageno_atend)
					break;
			}
			g_free (line);
			line = NULL;
			getline (&line, &length, f);
		}
		g_free (line);
	}
	fclose (f);
}
