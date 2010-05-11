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

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-iochannel.h"

/* Size of the buffer to use when reading from the GIOChannel, in bytes */
#define BUFFER_SIZE 65535

/* Maximum number of retries if the GIOChannel is G_IO_STATUS_AGAIN,
 *  to avoid infinite loops */
#define MAX_RETRIES 5


static GString *
get_string_in_locale (GString *s)
{
	GError *error = NULL;
	gchar  *str;
	gsize   bytes_read;
	gsize   bytes_written;

	str = g_locale_to_utf8 (s->str,
	                        s->len,
	                        &bytes_read,
	                        &bytes_written,
	                        &error);
	if (error) {
		g_debug ("  Conversion to UTF-8 read %d bytes, wrote %d bytes",
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

/**
 * tracker_iochannel_read_text:
 * @channel: input channel to read from
 * @max_bytes: max number of bytes to read from @channel
 * @try_locale_if_not_utf8: if the the text read is not valid UTF-8, try to
 *   convert from locale-encoding to UTF-8
 * @close_channel: if %TRUE, @channel will will be destroyed
 *
 * Reads up to @max_bytes from @channel, and validates the read text as proper
 *  UTF-8.
 *
 * Returns: newly-allocated NIL-terminated UTF-8 string with the read text.
 **/
gchar *
tracker_iochannel_read_text (GIOChannel *channel,
                             gsize       max_bytes,
                             gboolean    try_locale_if_not_utf8,
                             gboolean    close_channel)
{
	GString *s = NULL;
	gsize    n_bytes_remaining = max_bytes;
	guint    n_retries = MAX_RETRIES;

	g_return_val_if_fail (channel, NULL);
	g_return_val_if_fail (max_bytes > 0, NULL);

	/* We don't want to assume that the input data is in UTF-8, as it
	 *  may be in locale's encoding */
	g_io_channel_set_encoding (channel, NULL, NULL);

	/* Reading in chunks of BUFFER_SIZE
	 *   Loop is halted whenever one of this conditions is met:
	 *     a) Read bytes reached the maximum allowed (max_bytes)
	 *     b) No more bytes to read
	 *     c) Error reading
	 *     d) Stream has less than 3 bytes
	 *     e) Stream has a single line of BUFFER_SIZE bytes with no EOL
	 *     f) Max reading retries arrived
	 */
	while (n_bytes_remaining > 0 &&
	       n_retries > 0) {
		gchar      buf[BUFFER_SIZE];
		GError    *error = NULL;
		gssize     bytes_read;
		GIOStatus  status;

		/* Try to read from channel */
		status = g_io_channel_read_chars (channel,
		                                  buf,
		                                  MIN (BUFFER_SIZE, n_bytes_remaining),
		                                  &bytes_read,
		                                  &error);

		/* If any error reading, halt the loop */
		if (error) {
			g_message ("Error reading from iochannel: '%s'",
			           error->message);
			g_error_free (error);
			break;
		}

		/* If no more bytes to read, halt loop */
		if (bytes_read == 0 || status == G_IO_STATUS_EOF) {
			break;
		}

		/* If we are requested to retry, the retry */
		if (status == G_IO_STATUS_AGAIN) {
			n_retries--;
			continue;
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
		if (s == NULL) {
			if (bytes_read == BUFFER_SIZE &&
			    g_strstr_len (buf, bytes_read, "\n") == NULL) {
				g_debug ("  No '\\n' in the first %" G_GSSIZE_FORMAT " bytes, "
				         "not indexing file",
				         bytes_read);
				break;
			} else if (bytes_read <= 2) {
				g_debug ("  File has less than 3 characters in it, "
				         "not indexing file");
				break;
			}
		}

		/* Update remaining bytes */
		n_bytes_remaining -= bytes_read;

		g_debug ("  Read "
		         "%" G_GSSIZE_FORMAT " bytes this time, "
		         "%" G_GSIZE_FORMAT " bytes remaining",
		         bytes_read,
		         n_bytes_remaining);

		/* Append non-NIL terminated bytes */
		s = (s ?
		     g_string_append_len (s, buf, bytes_read) :
		     g_string_new_len (buf, bytes_read));
	}

	/* Validate UTF-8 if something was read */
	if (s) {
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
			s = NULL;
		}
	}

	/* Properly close channel if requested to do so */
	if (close_channel) {
		GError *error = NULL;

		g_io_channel_shutdown (channel, TRUE, &error);
		if (error) {
			g_message ("Couldn't properly shutdown channel: '%s'",
			           error->message);
			g_error_free (error);
		}
		g_io_channel_unref (channel);
	}

	return s ? g_string_free (s, FALSE) : NULL;
}
