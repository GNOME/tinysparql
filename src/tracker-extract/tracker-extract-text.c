/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-main.h"

#undef  TRY_LOCALE_TO_UTF8_CONVERSION

#define TEXT_BUFFER_SIZE 65535    /* bytes */

static void extract_text (const gchar          *uri,
                          TrackerSparqlBuilder *preupdate,
                          TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "text/*", extract_text },
	{ NULL, NULL }
};

#ifdef TRY_LOCALE_TO_UTF8_CONVERSION

static GString *
get_file_in_locale (GString *s)
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
		g_message ("Could not convert file from locale to UTF-8, %s",
		           error->message);
		g_error_free (error);
		g_free (str);
	} else {
		g_string_assign (s, str);
		g_free (str);
	}

	return s;
}

#endif /* TRY_LOCALE_TO_UTF8_CONVERSION */

static gchar *
get_file_content (const gchar *uri,
                  gsize        n_bytes)
{
	GFile            *file;
	GFileInputStream *stream;
	GError           *error = NULL;
	GString          *s = NULL;
	gchar             buf[TEXT_BUFFER_SIZE];
	gsize             n_bytes_remaining;
	gsize             n_valid_utf8_bytes;

	file = g_file_new_for_uri (uri);
	stream = g_file_read (file, NULL, &error);

	if (error) {
		g_message ("Could not read file:'%s', %s",
		           uri,
		           error->message);
		g_error_free (error);
		g_object_unref (file);

		return NULL;
	}

	g_debug ("  Starting to read '%s' up to %" G_GSIZE_FORMAT " bytes...",
	         uri, n_bytes);

	/* Reading in chunks of TEXT_BUFFER_SIZE (8192)
	 *   Loop is halted whenever one of this conditions is met:
	 *     a) Read bytes reached the maximum allowed (n_bytes)
	 *     b) No more bytes to read
	 *     c) Error reading
	 *     d) File has less than 3 bytes
	 *     e) File has a single line of TEXT_BUFFER_SIZE bytes with
	 *          no EOL
	 */
	n_bytes_remaining = n_bytes;
	while (n_bytes_remaining > 0) {
		gssize bytes_read;

		/* Read n_bytes_remaining or TEXT_BUFFER_SIZE bytes */
		bytes_read = g_input_stream_read (G_INPUT_STREAM (stream),
		                                  buf,
		                                  MIN (TEXT_BUFFER_SIZE, n_bytes_remaining),
		                                  NULL,
		                                  &error);

		/* If any error reading, halt the loop */
		if (error) {
			g_message ("Error reading from '%s': '%s'",
			           uri,
			           error->message);
			g_error_free (error);
			break;
		}

		/* If no more bytes to read, halt loop */
		if(bytes_read == 0) {
			break;
		}

		/* First of all, check if this is the first time we
		 * have tried to read the file up to the TEXT_BUFFER_SIZE
		 * limit. Then make sure that we read the maximum size
		 * of the buffer. If we don't do this, there is the
		 * case where we read 10 bytes in and it is just one
		 * line with no '\n'. Once we have confirmed this we
		 * check that the buffer has a '\n' to make sure the
		 * file is worth indexing. Similarly if the file has
		 * <= 3 bytes then we drop it.
		 */
		if (s == NULL) {
			if (bytes_read == TEXT_BUFFER_SIZE &&
			    g_strstr_len (buf, bytes_read, "\n") == NULL) {
				g_debug ("  No '\\n' in the first %" G_GSSIZE_FORMAT " bytes, not indexing file",
				         bytes_read);
				break;
			} else if (bytes_read <= 2) {
				g_debug ("  File has less than 3 characters in it, not indexing file");
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

	/* If nothing really read, return here */
	if (!s) {
		g_object_unref (stream);
		g_object_unref (file);
		return NULL;
	}

	/* Get number of valid UTF-8 bytes found */
	tracker_text_validate_utf8 (s->str,
	                            s->len,
	                            NULL,
	                            &n_valid_utf8_bytes);

#ifdef TRY_LOCALE_TO_UTF8_CONVERSION
	/* A valid UTF-8 file will be that where all read bytes are valid,
	 *  with a margin of 3 bytes for the last UTF-8 character which might
	 *  have been cut. */
	if (s->len - n_valid_utf8_bytes > 3) {
		/* If not UTF-8, try to get contents in locale encoding
		 *  (returns valid UTF-8) */
		s = get_file_in_locale (s);
	} else
#endif  /* TRY_LOCALE_TO_UTF8_CONVERSION */
	if (n_valid_utf8_bytes < s->len) {
		g_debug ("  Truncating to last valid UTF-8 character "
		         "(%" G_GSSIZE_FORMAT "/%" G_GSSIZE_FORMAT " bytes)",
		         n_valid_utf8_bytes,
		         s->len);
		s = g_string_truncate (s, n_valid_utf8_bytes);
	}

	g_object_unref (stream);
	g_object_unref (file);

	if (s->len < 1) {
		g_string_free (s, TRUE);
		return NULL;
	}

	return g_string_free (s, FALSE);
}

static void
extract_text (const gchar          *uri,
              TrackerSparqlBuilder *preupdate,
              TrackerSparqlBuilder *metadata)
{
	TrackerConfig *config;
	gchar *content;

	config = tracker_main_get_config ();

	content = get_file_content (uri,
	                            tracker_config_get_max_bytes (config));

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PlainTextDocument");

	if (content) {
		tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, content);
		g_free (content);
	}
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
