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
extract_ps_from_filestream (FILE *f,
                            TrackerSparqlBuilder *preupdate,
                            TrackerSparqlBuilder *metadata)
{
	gchar *line;
	gsize length;
	gssize read_char;
	gsize accum;
	gsize max_bytes;

	line = NULL;
	length = 0;

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	/* 20 MiB should be enough! (original safe limit) */
	accum = 0;
	max_bytes = 20u << 20;

	/* Reuse the same buffer for all lines. Must be dynamically allocated with
	 * malloc family methods as getline() may re-size it with realloc() */
	length = 1024;
	line = g_malloc (length);

	/* Halt the whole when one of these conditions is met:
	 *  a) Reached max bytes to read
	 *  b) No more lines to read
	 */
	while ((accum < max_bytes) &&
	       (read_char = tracker_getline (&line, &length, f)) != -1) {
		gboolean pageno_atend = FALSE;
		gboolean header_finished = FALSE;

		/* Update accumulated bytes read */
		accum += read_char;

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
	}

	/* Deallocate the buffer */
	if (line) {
		g_free (line);
	}
}



static void
extract_ps (const gchar          *uri,
            TrackerSparqlBuilder *preupdate,
            TrackerSparqlBuilder *metadata)
{
	FILE *f;
	gchar *filename;

	filename = g_filename_from_uri (uri, NULL, NULL);
	f = tracker_file_open (filename, "r", TRUE);
	g_free (filename);

	if (!f) {
		return;
	}

	/* Extract from filestream! */
	g_debug ("Extracting PS '%s'...", uri);
	extract_ps_from_filestream (f, preupdate, metadata);

	tracker_file_close (f, FALSE);
}

#ifdef USING_UNZIPPSFILES

static void
extract_ps_gz (const gchar          *uri,
               TrackerSparqlBuilder *preupdate,
               TrackerSparqlBuilder *metadata)
{
	FILE *fz;
	gint fdz;
	const gchar *argv[4];
	gchar *filename;
	GError *error = NULL;

	filename = g_filename_from_uri (uri, NULL, NULL);

	/* TODO: we should be using libz for this instead */

	argv[0] = "gunzip";
	argv[1] = "-c";
	argv[2] = filename;
	argv[3] = NULL;

	/* Fork & spawn to gunzip the file */
	if (!g_spawn_async_with_pipes (g_get_tmp_dir (),
	                               (gchar **) argv,
	                               NULL,
	                               G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
	                               tracker_spawn_child_func,
	                               GINT_TO_POINTER (10),
	                               NULL,
	                               NULL,
	                               &fdz,
	                               NULL,
	                               &error)) {
		g_warning ("Couldn't fork & spawn to gunzip '%s': %s",
		           uri, error ? error->message : NULL);
		g_clear_error (&error);
	}
	/* Get FILE from FD */
	else if ((fz = fdopen (fdz, "r")) == NULL) {
		g_warning ("Couldn't open FILE from FD (%s)...", uri);
		close (fdz);
	}
	/* Extract from filestream! */
	else
	{
		g_debug ("Extracting compressed PS '%s'...", uri);
		extract_ps_from_filestream (fz, preupdate, metadata);
#ifdef HAVE_POSIX_FADVISE
		posix_fadvise (fdz, 0, 0, POSIX_FADV_DONTNEED);
#endif /* HAVE_POSIX_FADVISE */
		fclose (fz);
	}

	g_free (filename);
}

#endif /* USING_UNZIPPSFILES */

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
