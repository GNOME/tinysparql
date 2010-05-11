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

#include "tracker-istream.h"

#define BUFFER_SIZE 65535    /* bytes */

GString *
tracker_istream_read_text (GInputStream  *stream,
                           gsize          max_bytes)
{
	GString *s = NULL;
	guchar   buf[BUFFER_SIZE];
	gsize    n_bytes_remaining;
	GError  *error = NULL;

	g_return_val_if_fail (stream, NULL);
	g_return_val_if_fail (max_bytes > 0, NULL);

	/* Reading in chunks of BUFFER_SIZE
	 *   Loop is halted whenever one of this conditions is met:
	 *     a) Read bytes reached the maximum allowed (max_bytes)
	 *     b) No more bytes to read
	 *     c) Error reading
	 *     d) File has less than 3 bytes
	 *     e) File has a single line of BUFFER_SIZE bytes with no EOL
	 */
	n_bytes_remaining = max_bytes;
	while (n_bytes_remaining > 0) {
		gssize bytes_read;

		/* Read n_bytes_remaining or BUFFER_SIZE bytes */
		bytes_read = g_input_stream_read (stream,
		                                  buf,
		                                  MIN (BUFFER_SIZE, n_bytes_remaining),
		                                  NULL,
		                                  &error);

		/* If any error reading, halt the loop */
		if (error) {
			g_message ("Error reading from stream: '%s'",
			           error->message);
			g_error_free (error);
			break;
		}

		/* If no more bytes to read, halt loop */
		if(bytes_read == 0) {
			break;
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
		s = (s == NULL ?
		     g_string_new_len (buf, bytes_read) :
		     g_string_append_len (s, buf, bytes_read));
	}

	/* Return whatever we got... */
	return s;
}
