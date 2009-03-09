/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "config.h"

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-os-dependant.h>

#include "tracker-main.h"
#include "tracker-escape.h"

#ifndef HAVE_GETLINE

#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#undef getdelim
#undef getline

#define GROWBY 80

#endif /* HAVE_GETLINE */

#ifdef USING_UNZIPPSFILES
static void extract_ps_gz (const gchar *filename,
			   GHashTable  *metadata);
#endif

static void extract_ps	  (const gchar *filename,
			   GHashTable  *metadata);

static TrackerExtractData data[] = {
#ifdef USING_UNZIPPSFILES
	{ "application/x-gzpostscript",	extract_ps_gz },
#endif
	{ "application/postscript",	extract_ps    },
	{ NULL, NULL }
};

#ifndef HAVE_GETLINE

static ssize_t
igetdelim (gchar  **linebuf,
	   size_t  *linebufsz,
	   gint     delimiter,
	   FILE    *file)
{
	gint ch;
	gint idx;

	if ((file == NULL || linebuf == NULL || *linebuf == NULL || *linebufsz == 0) &&
	    !(*linebuf == NULL && *linebufsz == 0)) {
		errno = EINVAL;
		return -1;
	}

	if (*linebuf == NULL && *linebufsz == 0) {
		*linebuf = g_malloc (GROWBY);

		if (!*linebuf) {
			errno = ENOMEM;
			return -1;
		}

		*linebufsz += GROWBY;
	}

	idx = 0;

	while ((ch = fgetc (file)) != EOF) {
		/* Grow the line buffer as necessary */
		while (idx > *linebufsz - 2) {
			*linebuf = g_realloc (*linebuf, *linebufsz += GROWBY);

			if (!*linebuf) {
				errno = ENOMEM;
				return -1;
			}
		}
		(*linebuf)[idx++] = (gchar) ch;

		if ((gchar) ch == delimiter) {
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

static gint
getline (gchar **s,
	 guint	*lim,
	 FILE	*stream)
{
	return igetdelim (s, lim, '\n', stream);
}

#endif /* HAVE_GETLINE */

static gchar *
hour_day_str_day (const gchar *date)
{
	/* From: ex. date: "(18:07 Tuesday 22 May 2007)"
	 * To  : ex. ISO8601 date: "2007-05-22T18:07:10-0600"
	 */
	return tracker_date_format_to_iso8601 (date, "(%H:%M %A %d %B %Y)");
}

static gchar *
day_str_month_day (const gchar *date)
{
	/* From: ex. date: "Tue May 22 18:07:10 2007"
	 * To  : ex. ISO8601 date: "2007-05-22T18:07:10-0600"
	 */
	return tracker_date_format_to_iso8601 (date, "%A %B %d %H:%M:%S %Y");
}

static gchar *
day_month_year_date (const gchar *date)
{
	/* From: ex. date: "22 May 1997 18:07:10 -0600"
	 * To  : ex. ISO8601 date: "2007-05-22T18:07:10-0600"
	 */
	return tracker_date_format_to_iso8601 (date, "%d %B %Y %H:%M:%S %z");
}

static gchar *
hour_month_day_date (const gchar *date)
{
	/* From: ex. date: "6:07 PM May 22, 2007"
	 * To  : ex. ISO8601 date: "2007-05-22T18:07:10-0600"
	 */
	return tracker_date_format_to_iso8601 (date, "%I:%M %p %B %d, %Y");
}

static gchar *
date_to_iso8601 (const gchar *date)
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

static void
extract_ps (const gchar *filename,
	    GHashTable	*metadata)
{
	gint fd;
	FILE *f;

#if defined(__linux__)
	if ((fd = g_open (filename, (O_RDONLY | O_NOATIME))) == -1) {
#else
	if ((fd = g_open (filename, O_RDONLY)) == -1) {
#endif
		return;
	}

	if ((f = fdopen (fd, "r"))) {
		gchar  *line;
		gsize	length;
		gssize	read_char;

		line = NULL;
		length = 0;

		while ((read_char = getline (&line, &length, f)) != -1) {
			gboolean pageno_atend	 = FALSE;
			gboolean header_finished = FALSE;

			line[read_char - 1] = '\0';  /* overwrite '\n' char */

			if (!header_finished && strncmp (line, "%%Copyright:", 12) == 0) {
				g_hash_table_insert (metadata,
						     g_strdup ("File:Other"),
						     tracker_escape_metadata (line + 13));

			} else if (!header_finished && strncmp (line, "%%Title:", 8) == 0) {
				g_hash_table_insert (metadata,
						     g_strdup ("Doc:Title"),
						     tracker_escape_metadata (line + 9));

			} else if (!header_finished && strncmp (line, "%%Creator:", 10) == 0) {
				g_hash_table_insert (metadata,
						     g_strdup ("Doc:Author"),
						     tracker_escape_metadata (line + 11));

			} else if (!header_finished && strncmp (line, "%%CreationDate:", 15) == 0) {
				gchar *date;

				date = date_to_iso8601 (line + 16);

				if (date) {
					g_hash_table_insert (metadata,
							     g_strdup ("Doc:Created"),
							     tracker_escape_metadata (date));

					g_free (date);
				}

			} else if (strncmp (line, "%%Pages:", 8) == 0) {
				if (strcmp (line + 9, "(atend)") == 0) {
					pageno_atend = TRUE;
				} else {
					g_hash_table_insert (metadata,
							     g_strdup ("Doc:PageCount"),
							     tracker_escape_metadata (line + 9));
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
	} else {
		close (fd);
	}
}

#ifdef USING_UNZIPPSFILES
static void
extract_ps_gz (const gchar *filename,
	       GHashTable  *metadata)
{
	FILE	    *fz, *f;
	GError	    *error = NULL;
	gchar	    *gunzipped;
	gint	     fdz;
	gint	     fd;
	gboolean     ptat;
	const gchar *argv[4];

	fd = g_file_open_tmp ("tracker-extract-ps-gunzipped.XXXXXX",
			      &gunzipped,
			      &error);

	if (error) {
		g_error_free (error);
		return;
	}

	argv[0] = "gunzip";
	argv[1] = "-c";
	argv[2] = filename;
	argv[3] = NULL;

	ptat = g_spawn_async_with_pipes (g_get_tmp_dir (),
					 (gchar **) argv,
					 NULL,
					 G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
					 tracker_spawn_child_func,
					 GINT_TO_POINTER (10),
					 NULL,
					 NULL,
					 &fdz,
					 NULL,
					 &error);

	if (!ptat) {
		g_unlink (gunzipped);
		g_clear_error (&error);
		close (fd);
		return;
	}

	fz = fdopen (fdz, "r");

	if (!fz) {
		g_unlink (gunzipped);
		close (fdz);
		close (fd);
		return;
	}

	f = fdopen (fd, "w");

	if (!f) {
		g_unlink (gunzipped);
		fclose (fz);
		close (fd);
		return;
	}

	if (f && fz) {
		unsigned char buf[8192];
		size_t w, b, accum;
		size_t max;

		/* 20 MiB should be enough! */
		accum = 0;
		max = 20u << 20;

		while ((b = fread (buf, 1, 8192, fz)) && accum <= max) {
			accum += b;
			w = 0;

			while (w < b) {
				w += fwrite (buf, 1, b, f);
			}
		}

		fclose (fz);
		fclose (f);
	}

	extract_ps (gunzipped, metadata);
	g_unlink (gunzipped);
}
#endif

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
