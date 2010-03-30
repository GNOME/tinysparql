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

#undef  TRY_LOCALE_TO_UTF8_CONVERSION

#define TEXT_MAX_SIZE   1048576  /* bytes */
#define TEXT_CHECK_SIZE 65535    /* bytes */

static void extract_text (const gchar          *uri,
                          TrackerSparqlBuilder *preupdate,
                          TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "text/plain",       extract_text },
	{ "text/x-authors",   extract_text },
	{ "text/x-changelog", extract_text },
	{ "text/x-copying",   extract_text },
	{ "text/x-credits",   extract_text },
	{ "text/x-install",   extract_text },
	{ "text/x-readme",    extract_text },
	{ NULL, NULL }
};

static gboolean
get_file_is_utf8 (GString *s,
                  gssize  *bytes_valid)
{
	const gchar *end;

	/* Check for UTF-8 validity, since we may
	 * have cut off the end.
	 */
	if (g_utf8_validate (s->str, s->len, &end)) {
		*bytes_valid = (gssize) s->len;
		return TRUE;
	}

	*bytes_valid = end - s->str;

	/* 4 is the maximum bytes for a UTF-8 character. */
	if (*bytes_valid > 4) {
		return FALSE;
	}

	if (g_utf8_get_char_validated (end, *bytes_valid) == (gunichar) -1) {
		return FALSE;
	}

	return TRUE;
}

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
get_file_content (const gchar *uri)
{
	GFile            *file;
	GFileInputStream *stream;
	GError           *error = NULL;
	GString                  *s;
	gssize            bytes;
	gssize            bytes_valid;
	gssize            bytes_read_total;
	gssize            buf_size;
	gchar             buf[TEXT_CHECK_SIZE];
	gboolean          has_more_data;
	gboolean          has_reached_max;
	gboolean          is_utf8;

	file = g_file_new_for_uri (uri);
	stream = g_file_read (file, NULL, &error);

	if (error) {
		g_message ("Could not get read file:'%s', %s",
		           uri,
		           error->message);
		g_error_free (error);
		g_object_unref (file);

		return NULL;
	}

	s = g_string_new ("");
	has_reached_max = FALSE;
	has_more_data = TRUE;
	bytes_read_total = 0;
	buf_size = TEXT_CHECK_SIZE - 1;

	g_debug ("  Starting read...");

	while (has_more_data && !has_reached_max && !error) {
		gssize bytes_read;
		gssize bytes_remaining;

		/* Leave space for NULL termination and make sure we
		 * add it at the end now.
		 */
		bytes_remaining = buf_size;
		bytes_read = 0;

		/* Loop until we hit the maximum */
		for (bytes = -1; bytes != 0 && !error; ) {
			bytes = g_input_stream_read (G_INPUT_STREAM (stream),
			                             buf,
			                             bytes_remaining,
			                             NULL,
			                             &error);

			bytes_read += bytes;
			bytes_remaining -= bytes;

			g_debug ("  Read %" G_GSSIZE_FORMAT " bytes", bytes);
		}

		/* Set the NULL termination after the last byte read */
		buf[buf_size - bytes_remaining] = '\0';

		/* First of all, check if this is the first time we
		 * have tried to read the file up to the TEXT_CHECK_SIZE
		 * limit. Then make sure that we read the maximum size
		 * of the buffer. If we don't do this, there is the
		 * case where we read 10 bytes in and it is just one
		 * line with no '\n'. Once we have confirmed this we
		 * check that the buffer has a '\n' to make sure the
		 * file is worth indexing. Similarly if the file has
		 * <= 3 bytes then we drop it.
		 */
		if (bytes_read_total == 0) {
			if (bytes_read == buf_size &&
			    strchr (buf, '\n') == NULL) {
				g_debug ("  No '\\n' in the first %" G_GSSIZE_FORMAT " bytes, not indexing file",
				         buf_size);
				break;
			} else if (bytes_read <= 2) {
				g_debug ("  File has less than 3 characters in it, not indexing file");
				break;
			}
		}

		/* Here we increment the bytes read total to evaluate
		 * the next states. We don't do this before the
		 * previous condition so we can know when we have
		 * iterated > 1.
		 */
		bytes_read_total += bytes_read;

		if (bytes_read != buf_size || bytes_read == 0) {
			has_more_data = FALSE;
		}

		if (bytes_read_total >= TEXT_MAX_SIZE) {
			has_reached_max = TRUE;
		}

		g_debug ("  Read "
		         "%" G_GSSIZE_FORMAT " bytes total, "
		         "%" G_GSSIZE_FORMAT " bytes this time, "
		         "more data:%s, reached max:%s",
		         bytes_read_total,
		         bytes_read,
		         has_more_data ? "yes" : "no",
		         has_reached_max ? "yes" : "no");

		/* The + 1 is for the NULL terminating byte */
		s = g_string_append_len (s, buf, bytes_read + 1);
	}

	if (has_reached_max) {
		g_debug ("  Maximum indexable limit reached");
	}

	if (error) {
		g_message ("Could not read input stream for:'%s', %s",
		           uri,
		           error->message);
		g_error_free (error);
		g_string_free (s, TRUE);
		g_object_unref (stream);
		g_object_unref (file);

		return NULL;
	}

	/* Check for UTF-8 Validity, if not try to convert it to the
	 * locale we are in.
	 */
	is_utf8 = get_file_is_utf8 (s, &bytes_valid);

	/* Make sure the string is NULL terminated and in the case
	 * where the string is valid UTF-8 up to the last character
	 * which was cut off, NULL terminate to the last most valid
	 * character.
	 */
#ifdef TRY_LOCALE_TO_UTF8_CONVERSION
	if (!is_utf8) {
		s = get_file_in_locale (s);
	} else {
		g_debug ("  Truncating to last valid UTF-8 character (%d/%d bytes)",
		         bytes_valid,
		         s->len);
		s = g_string_truncate (s, bytes_valid);
	}
#else   /* TRY_LOCALE_TO_UTF8_CONVERSION */
	g_debug ("  Truncating to last valid UTF-8 character (%" G_GSSIZE_FORMAT "/%" G_GSSIZE_FORMAT " bytes)",
	         bytes_valid,
	         s->len);
	s = g_string_truncate (s, bytes_valid);
#endif  /* TRY_LOCALE_TO_UTF8_CONVERSION */

	g_object_unref (stream);
	g_object_unref (file);

	if (s->len < 1) {
		g_string_free (s, TRUE);
		s = NULL;
	}

	return s ? g_string_free (s, FALSE) : NULL;
}

static void
extract_text (const gchar          *uri,
              TrackerSparqlBuilder *preupdate,
              TrackerSparqlBuilder *metadata)
{
	gchar *content;

	g_type_init ();

	content = get_file_content (uri);

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
