/*
 * Copyright (C) 2013-2014 Jolla Ltd. <andrew.den.exter@jollamobile.com>
 * Author: Philip Van Hoof <philip@codeminded.be>
 * Author: Mingxiang Lin <paralmx@gmail.com>
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

#include <libtracker-common/tracker-common.h>
#include <libtracker-extract/tracker-extract.h>

static gboolean
get_img_resolution (const GFile *file,
                    gint64      *width,
                    gint64      *height)
{
	GFileInputStream *stream;
	GInputStream *inputstream;
	GError *error = NULL;
	char bfType[2] = { 0 };
	uint w, h;

	if (width) {
		*width = 0;
	}

	if (height) {
		*height = 0;
	}

	w = h = 0;

	stream = g_file_read ((GFile *)file, NULL, &error);
	if (error) {
		g_message ("Could not read BMP file, %s", error->message);
		g_clear_error (&error);
		return FALSE;
	}

	inputstream = G_INPUT_STREAM (stream);

	if (!g_input_stream_read (inputstream, bfType, 2, NULL, &error)) {
		g_message ("Could not read BMP header from stream, %s", error ? error->message : "No error given");
		g_clear_error (&error);
		g_object_unref (stream);
		return FALSE;
	}

	if (bfType[0] != 'B' || bfType[1] != 'M') {
		g_message ("Expected BMP header to read 'B' or 'M', can not continue");
		g_object_unref (stream);
		return FALSE;
	}

	if (!g_input_stream_skip (inputstream, 16, NULL, &error)) {
		g_message ("Could not read 16 bytes from BMP header, %s", error ? error->message : "No error given");
		g_clear_error (&error);
		g_object_unref (stream);
		return FALSE;
	}

	if (!g_input_stream_read (inputstream, &w, sizeof (uint), NULL, &error)) {
		g_message ("Could not read width from BMP header, %s", error ? error->message : "No error given");
		g_clear_error (&error);
		g_object_unref (stream);
		return FALSE;
	}

	if (!g_input_stream_read (inputstream, &h, sizeof (uint), NULL, &error)) {
		g_message ("Could not read height from BMP header, %s", error ? error->message : "No error given");
		g_clear_error (&error);
		g_object_unref (stream);
		return FALSE;
	}

	if (width) {
		*width = w;
	}

	if (height) {
		*height = h;
	}

	g_input_stream_close (inputstream, NULL, NULL);
	g_object_unref (stream);

	return TRUE;
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerResource *image;
	goffset size;
	gchar *filename;
	GFile *file;
	gint64 width = 0, height = 0;

	file = tracker_extract_info_get_file (info);
	if (!file) {
		return FALSE;
	}

	filename = g_file_get_path (file);
	size = tracker_file_get_size (filename);
	g_free (filename);

	if (size < 14) {
		/* Smaller than BMP header, can't be a real BMP file */
		return FALSE;
	}

	image = tracker_resource_new (NULL);
	tracker_resource_add_uri (image, "rdf:type", "nfo:Image");
	tracker_resource_add_uri (image, "rdf:type", "nmm:Photo");

	if (get_img_resolution (file, &width, &height)) {
		if (width > 0) {
			tracker_resource_set_int64 (image, "nfo:width", width);
		}

		if (height > 0) {
			tracker_resource_set_int64 (image, "nfo:height", height);
		}
	}

	tracker_extract_info_set_resource (info, image);

	return TRUE;
}
