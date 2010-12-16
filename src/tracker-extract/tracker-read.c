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

#include "config.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-read.h"

/* Size of the buffer to use when reading, in bytes */
#define BUFFER_SIZE 65535

static gchar *
get_string_in_locale (const gchar *locale_str,
                      gsize        locale_str_len,
                      gsize       *utf8_len)
{
	GError *error = NULL;
	gchar  *utf8_str;
	gsize   bytes_read = 0;
	gsize   bytes_written = 0;

	utf8_str = g_locale_to_utf8 (locale_str,
	                             locale_str_len,
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
		g_free (utf8_str);
		return NULL;
	}

	*utf8_len = bytes_written;
	return utf8_str;
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
	 *
	 * NOTE: We may have non-UTF8 content read (say,
	 * UTF-16LE), so we can't rely on methods which assume
	 * NUL-terminated strings, as g_strstr_len().
	 */
	if (*s == NULL) {
		if (read_size <= 3) {
			g_debug ("  File has less than 3 characters in it, "
			         "not indexing file");
			return FALSE;
		}

		if (read_size == buffer_size) {
			const gchar *i;
			gboolean eol_found = FALSE;

			i = read_bytes;
			while (i != &read_bytes[read_size - 1]) {
				if (*i == '\n') {
					eol_found = TRUE;
					break;
				}
				i++;
			}

			if (!eol_found) {
				g_debug ("  No '\\n' in the first %" G_GSSIZE_FORMAT " bytes, "
				         "not indexing file",
				         read_size);
				return FALSE;
			}
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
	gchar *utf8 = NULL;
	gsize  utf8_len = 0;
	gsize n_valid_utf8_bytes = 0;

	/* Support also UTF-16 encoded text files, as the ones generated in
	 * Windows OS. We will only accept text files in UTF-16 which come
	 * with a proper BOM. */
	if (s->len > 2) {
		GError *error = NULL;

		if (memcmp (s->str, "\xFF\xFE", 2) == 0) {
			g_debug ("String comes in UTF-16LE, converting");
			utf8 = g_convert (&(s->str[2]),
			                  s->len - 2,
			                  "UTF-8",
			                  "UTF-16LE",
			                  NULL,
			                  &utf8_len,
			                  &error);

		} else if (memcmp (s->str, "\xFE\xFF", 2) == 0) {
			g_debug ("String comes in UTF-16BE, converting");
			utf8 = g_convert (&(s->str[2]),
			                  s->len - 2,
			                  "UTF-8",
			                  "UTF-16BE",
			                  NULL,
			                  &utf8_len,
			                  &error);
		}

		if (error) {
			g_warning ("Couldn't convert string from UTF-16 to UTF-8...: %s",
			           error->message);
			g_error_free (error);
			g_string_free (s, TRUE);
			return NULL;
		}
	}

	if (!utf8) {
		utf8_len = s->len;
		utf8 = g_string_free (s, FALSE);
	}

	/* Get number of valid UTF-8 bytes found */
	tracker_text_validate_utf8 (utf8,
	                            utf8_len,
	                            NULL,
	                            &n_valid_utf8_bytes);

	/* A valid UTF-8 file will be that where all read bytes are valid,
	 *  with a margin of 3 bytes for the last UTF-8 character which might
	 *  have been cut. */
	if (try_locale_if_not_utf8 &&
	    utf8_len - n_valid_utf8_bytes > 3) {
		gchar *from_locale_str;
		gsize  from_locale_str_len;

		/* If not UTF-8, try to get contents in locale encoding
		 *  (returns valid UTF-8) */
		from_locale_str = get_string_in_locale (utf8,
		                                        utf8_len,
		                                        &from_locale_str_len);
		g_free (utf8);
		if (!from_locale_str)
			return NULL;
		utf8 = from_locale_str;
		utf8_len = from_locale_str_len;
	} else if (n_valid_utf8_bytes < utf8_len) {
		g_debug ("  Truncating to last valid UTF-8 character "
		         "(%" G_GSSIZE_FORMAT "/%" G_GSSIZE_FORMAT " bytes)",
		         n_valid_utf8_bytes,
		         utf8_len);
		utf8[n_valid_utf8_bytes] = '\0';
		utf8_len = n_valid_utf8_bytes;
	}

	if (utf8_len < 1) {
		g_free (utf8);
		return NULL;
	}

	return utf8;
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
#ifdef HAVE_POSIX_FADVISE
	posix_fadvise (fd, 0, 0, POSIX_FADV_DONTNEED);
#endif /* HAVE_POSIX_FADVISE */
	fclose (fz);

	/* Validate UTF-8 if something was read, and return it */
	return s ? process_whole_string (s, try_locale_if_not_utf8) : NULL;
}
