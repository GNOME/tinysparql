/*
 * Copyright (C) 2007, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008-2009, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-os-dependant.h>
#include <libtracker-common/tracker-file-utils.h>

#include <libtracker-extract/tracker-extract.h>

#ifndef HAVE_GETLINE

#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#undef getdelim
#undef getline

#define GROW_BY 80

#endif /* HAVE_GETLINE */

#ifdef USING_UNZIPPSFILES
static void extract_ps_gz (const gchar          *uri,
                           TrackerSparqlBuilder *preupdate,
                           TrackerSparqlBuilder *metadata);
#endif
static void extract_ps    (const gchar          *uri,
                           TrackerSparqlBuilder *preupdate,
                           TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
#ifdef USING_UNZIPPSFILES
	{ "application/x-gzpostscript", extract_ps_gz },
#endif /* USING_UNZIPPSFILES */
	{ "application/postscript",     extract_ps    },
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
		*linebuf = g_malloc (GROW_BY);

		if (!*linebuf) {
			errno = ENOMEM;
			return -1;
		}

		*linebufsz += GROW_BY;
	}

	idx = 0;

	while ((ch = fgetc (file)) != EOF) {
		/* Grow the line buffer as necessary */
		while (idx > *linebufsz - 2) {
			*linebuf = g_realloc (*linebuf, *linebufsz += GROW_BY);

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
         guint  *lim,
         FILE   *stream)
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
			 * "(18:07 Tuesday 22 May 2007)"
			 */
			return hour_day_str_day (date);
		} else if (g_ascii_isalpha (date[0])) {
			/* we have probably a date like
			 * "Tue May 22 18:07:10 2007"
			 */
			return day_str_month_day (date);

		} else if (date[1] == ' ' || date[2] == ' ') {
			/* we have probably a date like
			 * "22 May 1997 18:07:10 -0600"
			 */
			return day_month_year_date (date);

		} else if (date[1] == ':' || date[2] == ':') {
			/* we have probably a date like
			 * "6:07 PM May 22, 2007"
			 */
			return hour_month_day_date (date);
		}
	}

	return NULL;
}

static void
extract_ps (const gchar          *uri,
            TrackerSparqlBuilder *preupdate,
            TrackerSparqlBuilder *metadata)
{
	FILE *f;
	gchar *filename;
	gchar *line;
	gsize length;
	gssize read_char;

	filename = g_filename_from_uri (uri, NULL, NULL);
	f = tracker_file_open (filename, "r", TRUE);
	g_free (filename);

	if (!f) {
		return;
	}

	line = NULL;
	length = 0;

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	while ((read_char = getline (&line, &length, f)) != -1) {
		gboolean pageno_atend = FALSE;
		gboolean header_finished = FALSE;

		line[read_char - 1] = '\0';  /* overwrite '\n' char */

		if (!header_finished && strncmp (line, "%%Copyright:", 12) == 0) {
			tracker_sparql_builder_predicate (metadata, "nie:copyright");
			tracker_sparql_builder_object_unvalidated (metadata, line + 13);
		} else if (!header_finished && strncmp (line, "%%Title:", 8) == 0) {
			tracker_sparql_builder_predicate (metadata, "nie:title");
			tracker_sparql_builder_object_unvalidated (metadata, line + 9);
		} else if (!header_finished && strncmp (line, "%%Creator:", 10) == 0) {
			tracker_sparql_builder_predicate (metadata, "nco:creator");
			tracker_sparql_builder_object_blank_open (metadata);
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "nco:Contact");
			tracker_sparql_builder_predicate (metadata, "nco:fullname");
			tracker_sparql_builder_object_unvalidated (metadata, line + 11);
			tracker_sparql_builder_object_blank_close (metadata);
		} else if (!header_finished && strncmp (line, "%%CreationDate:", 15) == 0) {
			gchar *date;

			date = date_to_iso8601 (line + 16);
			if (date) {
				tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
				tracker_sparql_builder_object_unvalidated (metadata, date);
				g_free (date);
			}
		} else if (strncmp (line, "%%Pages:", 8) == 0) {
			if (strcmp (line + 9, "(atend)") == 0) {
				pageno_atend = TRUE;
			} else {
				gint64 page_count;

				page_count = g_ascii_strtoll (line + 9, NULL, 10);
				tracker_sparql_builder_predicate (metadata, "nfo:pageCount");
				tracker_sparql_builder_object_int64 (metadata, page_count);
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

	tracker_file_close (f, FALSE);
}

#ifdef USING_UNZIPPSFILES

static void
extract_ps_gz (const gchar          *uri,
               TrackerSparqlBuilder *preupdate,
               TrackerSparqlBuilder *metadata)
{
	FILE *fz, *f;
	GError *error = NULL;
	gchar *gunzipped;
	gint fdz;
	gint fd;
	gboolean ptat;
	const gchar *argv[4];
	gchar *filename;

	fd = g_file_open_tmp ("tracker-extract-ps-gunzipped.XXXXXX",
	                      &gunzipped,
	                      &error);

	if (error) {
		g_error_free (error);
		return;
	}

	filename = g_filename_from_uri (uri, NULL, NULL);

	/* TODO: we should be using libz for this instead */

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
		g_free (filename);
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

	extract_ps (gunzipped, preupdate, metadata);
	g_unlink (gunzipped);
	g_free (filename);
}

#endif /* USING_UNZIPPSFILES */

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
