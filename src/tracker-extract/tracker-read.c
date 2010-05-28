/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-read.h"

/* Size of the buffer to use when reading, in bytes */
#define BUFFER_SIZE 65535

static GString *
get_string_in_locale (GString *s)
{
	GError *error = NULL;
	gchar *str;
	gsize bytes_read;
	gsize bytes_written;

	str = g_locale_to_utf8 (s->str,
	                        s->len,
	                        &bytes_read,
	                        &bytes_written,
	                        &error);
	if (error) {
		g_debug ("  Conversion to UTF-8 read %" G_GSIZE_FORMAT " bytes, wrote %" G_GSIZE_FORMAT " bytes",
		         bytes_read,
		         bytes_written);
		g_message ("Could not convert string from locale to UTF-8, %s",
		           error->message);
		g_error_free (error);
		g_free (str);
	} else {
		g_string_assign (s, str);
		g_free (str);
	}

	return s;
}


/* Returns %TRUE if read operation should continue, %FALSE otherwise */
static gboolean
process_chunk (const gchar  *read_bytes,
               gsize         read_size,
               gsize         buffer_size,
               gsize        *remaining_size,
               GString     **s)
{
	/* If no more bytes to read, halt loop */
	if (read_size == 0) {
		return FALSE;
	}

	/* First of all, check if this is the first time we
	 * have tried to read the stream up to the BUFFER_SIZE
	 * limit. Then make sure that we read the maximum size
	 * of the buffer. If we don't do this, there is the
	 * case where we read 10 bytes in and it is just one
	 * line with no '\n'. Once we have confirmed this we
	 * check that the buffer has a '\n' to make sure the
	 * file is worth indexing. Similarly if the file has
	 * <= 3 bytes then we drop it.
	 */
	if (*s == NULL) {
		if (read_size == buffer_size &&
		    g_strstr_len (read_bytes, read_size, "\n") == NULL) {
			g_debug ("  No '\\n' in the first %" G_GSSIZE_FORMAT " bytes, "
			         "not indexing file",
			         read_size);
			return FALSE;
		} else if (read_size <= 2) {
			g_debug ("  File has less than 3 characters in it, "
			         "not indexing file");
			return FALSE;
		}
	}

	/* Update remaining bytes */
	*remaining_size -= read_size;

	g_debug ("  Read "
	         "%" G_GSSIZE_FORMAT " bytes from file, %" G_GSIZE_FORMAT " "
	         "bytes remaining until configured threshold is reached",
	         read_size,
	         *remaining_size);

	/* Append non-NIL terminated bytes */
	*s = (*s ?
	      g_string_append_len (*s, read_bytes, read_size) :
	      g_string_new_len (read_bytes, read_size));

	return TRUE;
}

static gchar *
process_whole_string (GString  *s,
                      gboolean  try_locale_if_not_utf8)
{
	gsize n_valid_utf8_bytes = 0;

	/* Get number of valid UTF-8 bytes found */
	tracker_text_validate_utf8 (s->str,
	                            s->len,
	                            NULL,
	                            &n_valid_utf8_bytes);

	/* A valid UTF-8 file will be that where all read bytes are valid,
	 *  with a margin of 3 bytes for the last UTF-8 character which might
	 *  have been cut. */
	if (try_locale_if_not_utf8 &&
	    s->len - n_valid_utf8_bytes > 3) {
		/* If not UTF-8, try to get contents in locale encoding
		 *  (returns valid UTF-8) */
		s = get_string_in_locale (s);
	} else if (n_valid_utf8_bytes < s->len) {
		g_debug ("  Truncating to last valid UTF-8 character "
		         "(%" G_GSSIZE_FORMAT "/%" G_GSSIZE_FORMAT " bytes)",
		         n_valid_utf8_bytes,
		         s->len);
		s = g_string_truncate (s, n_valid_utf8_bytes);
	}

	if (s->len < 1) {
		g_string_free (s, TRUE);
		return NULL;
	}

	return g_string_free (s, FALSE);
}

/**
 * tracker_read_text_from_stream:
 * @stream: input stream to read from
 * @max_bytes: max number of bytes to read from @stream
 * @try_locale_if_not_utf8: if the the text read is not valid UTF-8, try to
 *   convert from locale-encoding to UTF-8
 *
 * Reads up to @max_bytes from @stream, and validates the read text as proper
 *  UTF-8.
 *
 * Returns: newly-allocated NUL-terminated UTF-8 string with the read text.
 **/
gchar *
tracker_read_text_from_stream (GInputStream *stream,
                               gsize       max_bytes,
                               gboolean    try_locale_if_not_utf8)
{
	GString *s = NULL;
	gsize n_bytes_remaining = max_bytes;

	g_return_val_if_fail (stream, NULL);
	g_return_val_if_fail (max_bytes > 0, NULL);

	/* Reading in chunks of BUFFER_SIZE
	 *   Loop is halted whenever one of this conditions is met:
	 *     a) Read bytes reached the maximum allowed (max_bytes)
	 *     b) No more bytes to read
	 *     c) Error reading
	 *     d) Stream has less than 3 bytes
	 *     e) Stream has a single line of BUFFER_SIZE bytes with no EOL
	 */
	while (n_bytes_remaining > 0) {
		gchar buf[BUFFER_SIZE];
		GError *error = NULL;
		gsize n_bytes_read;

		/* Read bytes from stream */
		if (!g_input_stream_read_all (stream,
		                              buf,
		                              MIN (BUFFER_SIZE, n_bytes_remaining),
		                              &n_bytes_read,
		                              NULL,
		                              &error)) {
			g_message ("Error reading from stream: '%s'",
			           error->message);
			g_error_free (error);
			break;
		}

		/* Process read bytes, and halt loop if needed */
		if (!process_chunk (buf,
		                    n_bytes_read,
		                    BUFFER_SIZE,
		                    &n_bytes_remaining,
		                    &s)) {
			break;
		}
	}

	/* Validate UTF-8 if something was read, and return it */
	return s ? process_whole_string (s, try_locale_if_not_utf8) : NULL;
}


/**
 * tracker_read_text_from_fd:
 * @fd: input fd to read from
 * @max_bytes: max number of bytes to read from @fd
 * @try_locale_if_not_utf8: if the the text read is not valid UTF-8, try to
 *   convert from locale-encoding to UTF-8
 *
 * Reads up to @max_bytes from @fd, and validates the read text as proper
 *  UTF-8. Will also properly close the FD when finishes.
 *
 * Returns: newly-allocated NUL-terminated UTF-8 string with the read text.
 **/
gchar *
tracker_read_text_from_fd (gint     fd,
                           gsize    max_bytes,
                           gboolean try_locale_if_not_utf8)
{
	FILE *fz;
	GString *s = NULL;
	gsize n_bytes_remaining = max_bytes;

	g_return_val_if_fail (max_bytes > 0, NULL);

	if ((fz = fdopen (fd, "r")) == NULL) {
		g_warning ("Cannot read from FD... could not extract text");
		close (fd);
		return NULL;
	}

	/* Reading in chunks of BUFFER_SIZE
	 *   Loop is halted whenever one of this conditions is met:
	 *     a) Read bytes reached the maximum allowed (max_bytes)
	 *     b) No more bytes to read
	 *     c) Error reading
	 *     d) Stream has less than 3 bytes
	 *     e) Stream has a single line of BUFFER_SIZE bytes with no EOL
	 */
	while (n_bytes_remaining > 0) {
		gchar buf[BUFFER_SIZE];
		gsize n_bytes_read;

		/* Read bytes */
		n_bytes_read = fread (buf,
		                      1,
		                      MIN (BUFFER_SIZE, n_bytes_remaining),
		                      fz);

		/* Process read bytes, and halt loop if needed */
		if (!process_chunk (buf,
		                    n_bytes_read,
		                    BUFFER_SIZE,
		                    &n_bytes_remaining,
		                    &s)) {
			break;
		}
	}

	/* Close the file here */
	fclose (fz);

	/* Validate UTF-8 if something was read, and return it */
	return s ? process_whole_string (s, try_locale_if_not_utf8) : NULL;
}
