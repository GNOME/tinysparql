/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <utime.h>
#include <time.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "tracker-file-utils.h"
#include "tracker-date-time.h"
#include "tracker-albumart.h"

static gboolean
albumart_strip_find_next_block (const gchar    *original,
                                const gunichar  open_char,
                                const gunichar  close_char,
                                gint           *open_pos,
                                gint           *close_pos)
{
	const gchar *p1, *p2;

	if (open_pos) {
		*open_pos = -1;
	}

	if (close_pos) {
		*close_pos = -1;
	}

	p1 = g_utf8_strchr (original, -1, open_char);
	if (p1) {
		if (open_pos) {
			*open_pos = p1 - original;
		}

		p2 = g_utf8_strchr (g_utf8_next_char (p1), -1, close_char);
		if (p2) {
			if (close_pos) {
				*close_pos = p2 - original;
			}

			return TRUE;
		}
	}

	return FALSE;
}

/**
 * tracker_albumart_strip_invalid_entities:
 * @original: original string
 *
 * Strip a albumname or artistname string to prepare it for calculating the
 * albumart path with it. Certain characters and charactersets will be stripped
 * and a newly allocated string returned which you must free with g_free.
 *
 * Returns: copy of original but then stripped
 *
 * Since: 0.10.4
 */
gchar *
tracker_albumart_strip_invalid_entities (const gchar *original)
{
	GString *str_no_blocks;
	gchar **strv;
	gchar *str;
	gboolean blocks_done = FALSE;
	const gchar *p;
	const gchar *invalid_chars = "()[]<>{}_!@#$^&*+=|\\/\"'?~";
	const gchar *invalid_chars_delimiter = "*";
	const gchar *convert_chars = "\t";
	const gchar *convert_chars_delimiter = " ";
	const gunichar blocks[5][2] = {
		{ '(', ')' },
		{ '{', '}' },
		{ '[', ']' },
		{ '<', '>' },
		{  0,   0  }
	};

	str_no_blocks = g_string_new ("");

	p = original;

	while (!blocks_done) {
		gint pos1, pos2, i;

		pos1 = -1;
		pos2 = -1;

		for (i = 0; blocks[i][0] != 0; i++) {
			gint start, end;

			/* Go through blocks, find the earliest block we can */
			if (albumart_strip_find_next_block (p, blocks[i][0], blocks[i][1], &start, &end)) {
				if (pos1 == -1 || start < pos1) {
					pos1 = start;
					pos2 = end;
				}
			}
		}

		/* If either are -1 we didn't find any */
		if (pos1 == -1) {
			/* This means no blocks were found */
			g_string_append (str_no_blocks, p);
			blocks_done = TRUE;
		} else {
			/* Append the test BEFORE the block */
			if (pos1 > 0) {
				g_string_append_len (str_no_blocks, p, pos1);
			}

			p = g_utf8_next_char (p + pos2);

			/* Do same again for position AFTER block */
			if (*p == '\0') {
				blocks_done = TRUE;
			}
		}
	}

	/* Now convert chars to lower case */
	str = g_utf8_strdown (str_no_blocks->str, -1);
	g_string_free (str_no_blocks, TRUE);

	/* Now strip invalid chars */
	g_strdelimit (str, invalid_chars, *invalid_chars_delimiter);
	strv = g_strsplit (str, invalid_chars_delimiter, -1);
	g_free (str);
	str = g_strjoinv (NULL, strv);
	g_strfreev (strv);

	/* Now convert chars */
	g_strdelimit (str, convert_chars, *convert_chars_delimiter);
	strv = g_strsplit (str, convert_chars_delimiter, -1);
	g_free (str);
	str = g_strjoinv (convert_chars_delimiter, strv);
	g_strfreev (strv);

	while (g_strrstr (str, "  ") != NULL) {
		/* Now remove double spaces */
		strv = g_strsplit (str, "  ", -1);
		g_free (str);
		str = g_strjoinv (" ", strv);
		g_strfreev (strv);
	}

	/* Now strip leading/trailing white space */
	g_strstrip (str);

	return str;
}

static gchar *
albumart_checksum_for_data (GChecksumType  checksum_type,
                            const guchar  *data,
                            gsize          length)
{
	GChecksum *checksum;
	gchar *retval;

	checksum = g_checksum_new (checksum_type);
	if (!checksum) {
		return NULL;
	}

	g_checksum_update (checksum, data, length);
	retval = g_strdup (g_checksum_get_string (checksum));
	g_checksum_free (checksum);

	return retval;
}

/**
 * tracker_albumart_get_path:
 * @artist: the artist
 * @album: the album
 * @prefix: For example "album"
 * @uri: NULL or the uri of the file
 * @path: the location to store the local path
 * @local_uri: the location to store the local uri or NULL
 *
 * Get the path to albumart for a given album or song. Newly allocated data in
 * @path and @local_uri must be freed with g_free.
 *
 * Since: 0.10.4
 */
void
tracker_albumart_get_path (const gchar  *artist,
                           const gchar  *album,
                           const gchar  *prefix,
                           const gchar  *uri,
                           gchar       **path,
                           gchar       **local_uri)
{
	gchar *art_filename;
	gchar *dir;
	gchar *artist_down, *album_down;
	gchar *artist_stripped, *album_stripped;
	gchar *artist_checksum, *album_checksum;

	/* http://live.gnome.org/MediaArtStorageSpec */

	if (path) {
		*path = NULL;
	}

	if (local_uri) {
		*local_uri = NULL;
	}

	if (!artist && !album) {
		return;
	}

	if (!artist) {
		artist_stripped = g_strdup (" ");
	} else {
		artist_stripped = tracker_albumart_strip_invalid_entities (artist);
	}

	if (!album) {
		album_stripped = g_strdup (" ");
	} else {
		album_stripped = tracker_albumart_strip_invalid_entities (album);
	}

	artist_down = g_utf8_strdown (artist_stripped, -1);
	album_down = g_utf8_strdown (album_stripped, -1);

	/* g_print ("[%s] [%s]\n", artist_down, album_down); */

	g_free (artist_stripped);
	g_free (album_stripped);

	dir = g_build_filename (g_get_user_cache_dir (),
	                        "media-art",
	                        NULL);

	if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (dir, 0770);
	}

	artist_checksum = albumart_checksum_for_data (G_CHECKSUM_MD5,
	                                              (const guchar *) artist_down,
	                                              strlen (artist_down));
	album_checksum = albumart_checksum_for_data (G_CHECKSUM_MD5,
	                                             (const guchar *) album_down,
	                                             strlen (album_down));

	g_free (artist_down);
	g_free (album_down);

	art_filename = g_strdup_printf ("%s-%s-%s.jpeg",
	                                prefix ? prefix : "album",
	                                artist_checksum,
	                                album_checksum);

	if (path) {
		*path = g_build_filename (dir, art_filename, NULL);
	}

	if (local_uri) {
		gchar *local_dir;
		GFile *file, *parent;

		if (strstr (uri, "://")) {
			file = g_file_new_for_uri (uri);
		} else {
			file = g_file_new_for_path (uri);
		}

		parent = g_file_get_parent (file);
		if (parent) {
			local_dir = g_file_get_uri (parent);

			/* This is a URI, don't use g_build_filename here */
			*local_uri = g_strdup_printf ("%s/.mediaartlocal/%s", local_dir, art_filename);

			g_free (local_dir);
			g_object_unref (parent);
		}
		g_object_unref (file);
	}

	g_free (dir);
	g_free (art_filename);
	g_free (artist_checksum);
	g_free (album_checksum);
}
