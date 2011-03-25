/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-extract/tracker-extract.h>

#define ICON_HEADER_SIZE_16 3
#define ICON_IMAGE_METADATA_SIZE_8 16

static gboolean
find_max_width_and_height (const gchar *uri,
                           guint       *width,
                           guint       *height)
{
	GError *error = NULL;
	GFile *file;
	GFileInputStream *stream;
	guint n_images;
	guint i;
	guint16 header [ICON_HEADER_SIZE_16];

	*width = 0;
	*height = 0;

	file = g_file_new_for_uri (uri);
	stream = g_file_read (file, NULL, &error);
	if (error) {
		g_message ("Could not read file '%s': %s",
		           uri,
		           error->message);
		g_error_free (error);
		g_object_unref (file);

		return FALSE;
	}

	/* Header consists of:
	 *  - 2bytes, reserved, must be 0
	 *  - 2bytes, image type (1:icon, 2:cursor, other values invalid)
	 *  - 2bytes, number of images in the file.
	 *
	 * Right now we just need the number of images in the file.
	 */
	if (!g_input_stream_read_all (G_INPUT_STREAM (stream),
	                              header,
	                              ICON_HEADER_SIZE_16 * 2,
	                              NULL,
	                              NULL,
	                              &error)) {
		g_message ("Error reading icon header from stream: '%s'",
		           error->message);
		g_error_free (error);
		g_object_unref (stream);
		g_object_unref (file);
		return FALSE;
	}

	n_images = GUINT16_FROM_LE (header[2]);
	g_debug ("Found '%u' images in the icon file...", n_images);

	/* Loop images looking for the biggest one... */
	for (i = 0; i < n_images; i++) {
		guint8 image_metadata [ICON_IMAGE_METADATA_SIZE_8];

		/* Image metadata chunk consists of:
		 *  - 1 byte, width in pixels, 0 means 256
		 *  - 1 byte, height in pixels, 0 means 256
		 *  - Plus some other stuff we don't care about...
		 */
		if (!g_input_stream_read_all (G_INPUT_STREAM (stream),
		                              image_metadata,
		                              ICON_IMAGE_METADATA_SIZE_8,
		                              NULL,
		                              NULL,
		                              &error)) {
			g_message ("Error reading icon image metadata '%u' from stream: '%s'",
			           i,
			           error->message);
			g_error_free (error);
			break;
		}

		g_debug ("  Image '%u'; width:%u height:%u",
		         i,
		         image_metadata[0],
		         image_metadata[1]);

		/* Width... */
		if (image_metadata[0] == 0) {
			*width = 256;
		} else if (image_metadata[0] > *width) {
			*width = image_metadata[0];
		}

		/* Height... */
		if (image_metadata[1] == 0) {
			*height = 256;
		} else if (image_metadata[1] > *width) {
			*height = image_metadata[0];
		}
	}

	g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);
	g_object_unref (stream);
	g_object_unref (file);
	return TRUE;
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (const gchar          *uri,
                              const gchar          *mimetype,
                              TrackerSparqlBuilder *preupdate,
                              TrackerSparqlBuilder *metadata,
                              GString              *where)
{
	guint max_width;
	guint max_height;

	/* The Windows Icon file format may contain the same icon with different
	 * sizes inside, so there's no clear way of setting single width and
	 * height values. Thus, we set maximum sizes found. */
	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:Image");
	tracker_sparql_builder_object (metadata, "nfo:Icon");

	if (find_max_width_and_height (uri, &max_width, &max_height)) {
		if (max_width > 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:width");
			tracker_sparql_builder_object_int64 (metadata, (gint64) max_width);
		}
		if (max_height > 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:height");
			tracker_sparql_builder_object_int64 (metadata, (gint64) max_height);
		}
	}

	return TRUE;
}
