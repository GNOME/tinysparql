/*
 * Copyright (C) 2007, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008-2009, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-extract/tracker-extract.h>

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

static TrackerResource *
extract_ps_from_filestream (FILE *f)
{
	TrackerResource *metadata;
	gchar *line;
	gsize length;
	gssize read_char;
	gsize accum;
	gsize max_bytes;

	line = NULL;
	length = 0;

	metadata = tracker_resource_new (NULL);
	tracker_resource_add_uri (metadata, "rdf:type", "nfo:PaginatedTextDocument");

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
			tracker_resource_set_string (metadata, "nie:copyright", line + 13);
		} else if (!header_finished && strncmp (line, "%%Title:", 8) == 0) {
			tracker_resource_set_string (metadata, "nie:title", line + 9);
		} else if (!header_finished && strncmp (line, "%%Creator:", 10) == 0) {
			TrackerResource *creator = tracker_extract_new_contact (line + 11);
			tracker_resource_set_relation (metadata, "nco:creator", creator);
			g_object_unref (creator);
		} else if (!header_finished && strncmp (line, "%%CreationDate:", 15) == 0) {
			gchar *date;

			date = date_to_iso8601 (line + 16);
			if (date) {
				tracker_resource_set_string (metadata, "nie:contentCreated", date);
				g_free (date);
			}
		} else if (strncmp (line, "%%Pages:", 8) == 0) {
			if (strcmp (line + 9, "(atend)") == 0) {
				pageno_atend = TRUE;
			} else {
				gint64 page_count;

				page_count = g_ascii_strtoll (line + 9, NULL, 10);
				tracker_resource_set_int (metadata, "nfo:pageCount", page_count);
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

	return metadata;
}



static TrackerResource *
extract_ps (const gchar          *uri)
{
	TrackerResource *metadata;
	FILE *f;
	gchar *filename;

	filename = g_filename_from_uri (uri, NULL, NULL);
	f = tracker_file_open (filename);
	g_free (filename);

	if (!f) {
		return NULL;
	}

	/* Extract from filestream! */
	g_debug ("Extracting PS '%s'...", uri);
	metadata = extract_ps_from_filestream (f);

	tracker_file_close (f, FALSE);

	return metadata;
}

#ifdef USING_UNZIPPSFILES

#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>

static void
spawn_child_func (gpointer user_data)
{
	struct rlimit cpu_limit;
	gint timeout = GPOINTER_TO_INT (user_data);

	if (timeout > 0) {
		/* set cpu limit */
		getrlimit (RLIMIT_CPU, &cpu_limit);
		cpu_limit.rlim_cur = timeout;
		cpu_limit.rlim_max = timeout + 1;

		if (setrlimit (RLIMIT_CPU, &cpu_limit) != 0) {
			g_critical ("Failed to set resource limit for CPU");
		}

		/* Have this as a precaution in cases where cpu limit has not
		 * been reached due to spawned app sleeping.
		 */
		alarm (timeout + 2);
	}

	/* Set child's niceness to 19 */
	errno = 0;

	/* nice() uses attribute "warn_unused_result" and so complains
	 * if we do not check its returned value. But it seems that
	 * since glibc 2.2.4, nice() can return -1 on a successful call
	 * so we have to check value of errno too. Stupid...
	 */
	if (nice (19) == -1 && errno) {
		g_warning ("Failed to set nice value");
	}
}

static TrackerResource *
extract_ps_gz (const gchar          *uri)
{
	TrackerResource *metadata = NULL;
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
	                               spawn_child_func,
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
		metadata = extract_ps_from_filestream (fz);
#ifdef HAVE_POSIX_FADVISE
		if (posix_fadvise (fdz, 0, 0, POSIX_FADV_DONTNEED) != 0)
			g_warning ("posix_fadvise() call failed: %m");
#endif /* HAVE_POSIX_FADVISE */
		fclose (fz);
	}

	g_free (filename);

	return metadata;
}

#endif /* USING_UNZIPPSFILES */

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerResource *metadata;
	GFile *file;
	gchar *uri;
	const char *mimetype;

	file = tracker_extract_info_get_file (info);
	mimetype = tracker_extract_info_get_mimetype (info);
	uri = g_file_get_uri (file);

#ifdef USING_UNZIPPSFILES
	if (strcmp (mimetype, "application/x-gzpostscript") == 0) {
		metadata = extract_ps_gz (uri);
	} else
#endif /* USING_UNZIPPSFILES */
	{
		metadata = extract_ps (uri);
	}

	g_free (uri);

	if (metadata) {
		tracker_extract_info_set_resource (info, metadata);
		g_object_unref (metadata);
	}

	return TRUE;
}
