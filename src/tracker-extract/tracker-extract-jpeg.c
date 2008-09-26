/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <jpeglib.h>

#include "tracker-extract.h"
#include "tracker-xmp.h"

static void extract_jpeg (const gchar *filename,
			  GHashTable  *metadata);

static TrackerExtractorData data[] = {
	{ "image/jpeg", extract_jpeg },
	{ NULL, NULL }
};

#ifdef HAVE_EXEMPI
#define XMP_NAMESPACE	     "http://ns.adobe.com/xap/1.0/\x00"
#define XMP_NAMESPACE_LENGTH 29
#endif /* HAVE_EXEMPI */

#ifdef HAVE_LIBEXIF

#include <libexif/exif-data.h>

#define EXIF_DATE_FORMAT "%Y:%m:%d %H:%M:%S"

typedef gchar * (*PostProcessor) (const gchar *);

typedef struct {
	ExifTag       tag;
	gchar	     *name;
	PostProcessor post;
} TagType;

static gchar *date_to_iso8601	(const gchar *exif_date);
static gchar *fix_focal_length	(const gchar *fl);
static gchar *fix_flash		(const gchar *flash);
static gchar *fix_fnumber	(const gchar *fn);
static gchar *fix_exposure_time (const gchar *et);

static TagType tags[] = {
	{ EXIF_TAG_PIXEL_Y_DIMENSION, "Image:Height", NULL },
	{ EXIF_TAG_PIXEL_X_DIMENSION, "Image:Width", NULL },
	{ EXIF_TAG_RELATED_IMAGE_WIDTH, "Image:Width", NULL },
	{ EXIF_TAG_DOCUMENT_NAME, "Image:Title", NULL },
	/* { -1, "Image:Album", NULL }, */
	{ EXIF_TAG_DATE_TIME, "Image:Date", date_to_iso8601 },
	/* { -1, "Image:Keywords", NULL }, */
	{ EXIF_TAG_ARTIST, "Image:Creator", NULL },
	{ EXIF_TAG_USER_COMMENT, "Image:Comments", NULL },
	{ EXIF_TAG_IMAGE_DESCRIPTION, "Image:Description", NULL },
	{ EXIF_TAG_SOFTWARE, "Image:Software", NULL },
	{ EXIF_TAG_MAKE, "Image:CameraMake", NULL },
	{ EXIF_TAG_MODEL, "Image:CameraModel", NULL },
	{ EXIF_TAG_ORIENTATION, "Image:Orientation", NULL },
	{ EXIF_TAG_EXPOSURE_PROGRAM, "Image:ExposureProgram", NULL },
	{ EXIF_TAG_EXPOSURE_TIME, "Image:ExposureTime", fix_exposure_time },
	{ EXIF_TAG_FNUMBER, "Image:FNumber", fix_fnumber },
	{ EXIF_TAG_FLASH, "Image:Flash", fix_flash },
	{ EXIF_TAG_FOCAL_LENGTH, "Image:FocalLength", fix_focal_length },
	{ EXIF_TAG_ISO_SPEED_RATINGS, "Image:ISOSpeed", NULL },
	{ EXIF_TAG_METERING_MODE, "Image:MeteringMode", NULL },
	{ EXIF_TAG_WHITE_BALANCE, "Image:WhiteBalance", NULL },
	{ EXIF_TAG_COPYRIGHT, "File:Copyright", NULL },
	{ -1, NULL, NULL }
};

static gchar *
date_to_iso8601 (const gchar *exif_date)
{
	/* From: ex; date "2007:04:15 15:35:58"
	 * To  : ex. "2007-04-15T17:35:58+0200 where +0200 is localtime
	 */
	return tracker_generic_date_to_iso8601 (exif_date, EXIF_DATE_FORMAT);
}

static gchar *
fix_focal_length (const gchar *fl)
{
	return g_strndup (fl, strstr (fl, "mm") - fl);
}

static gchar *
fix_flash (const gchar *flash)
{
	if (g_str_has_prefix (flash, "No")) {
		return g_strdup ("0");
	} else {
		return g_strdup ("1");
	}
}

static gchar *
fix_fnumber (const gchar *fn)
{
	gchar *new_fn;

	if (!fn) {
		return NULL;
	}

	new_fn = g_strdup (fn);

	if (new_fn[0] == 'F') {
		new_fn[0] = ' ';
	} else if (fn[0] == 'f' && new_fn[1] == '/') {
		new_fn[0] = new_fn[1] = ' ';
	}

	return new_fn;
}

static gchar *
fix_exposure_time (const gchar *et)
{
	gchar *sep;

	sep = strchr (et, '/');

	if (sep) {
		gdouble fraction;

		fraction = g_ascii_strtod (sep + 1, NULL);

		if (fraction > 0.0) {
			gdouble val;
			gchar	buf[G_ASCII_DTOSTR_BUF_SIZE];

			val = 1.0f / fraction;
			g_ascii_dtostr (buf, sizeof(buf), val);

			return g_strdup (buf);
		}
	}

	return g_strdup (et);
}

#endif /* HAVE_LIBEXIF */

static void
read_exif (const unsigned char *buffer,
	   size_t		len,
	   GHashTable	       *metadata)
{
#ifdef HAVE_LIBEXIF
	ExifData *exif;
	TagType  *p;

	exif = exif_data_new_from_data ((unsigned char *) buffer, len);

	for (p = tags; p->name; ++p) {
		ExifEntry *entry;

		entry = exif_data_get_entry (exif, p->tag);

		if (entry) {
			gchar buffer[1024];

			exif_entry_get_value (entry, buffer, 1024);

			if (p->post) {
				g_hash_table_insert (metadata,
						     g_strdup (p->name),
						     (*p->post) (buffer));
			} else {
				g_hash_table_insert (metadata,
						     g_strdup (p->name),
						     g_strdup (buffer));
			}
		}
	}
#endif /* HAVE_LIBEXIF */
}

static void
extract_jpeg (const gchar *filename,
	      GHashTable  *metadata)
{
	struct jpeg_decompress_struct  cinfo;
	struct jpeg_error_mgr	       jerr;
	struct jpeg_marker_struct     *marker;
	FILE			      *jpeg;
	gint			       fd_jpeg;

	if ((fd_jpeg = g_open (filename, O_RDONLY)) == -1) {
		return;
	}

	if ((jpeg = fdopen (fd_jpeg, "rb"))) {
		gchar *str;
		gsize  len;

		cinfo.err = jpeg_std_error (&jerr);
		jpeg_create_decompress (&cinfo);

		jpeg_save_markers (&cinfo, JPEG_COM, 0xFFFF);
		jpeg_save_markers (&cinfo, JPEG_APP0 + 1, 0xFFFF);

		jpeg_stdio_src (&cinfo, jpeg);

		jpeg_read_header (&cinfo, TRUE);

		/* FIXME? It is possible that there are markers after SOS,
		 * but there shouldn't be. Should we decompress the whole file?
		 *
		 * jpeg_start_decompress(&cinfo);
		 * jpeg_finish_decompress(&cinfo);
		 *
		 * jpeg_calc_output_dimensions(&cinfo);
		*/

		marker = (struct jpeg_marker_struct *) &cinfo.marker_list;

		while (marker) {
			switch (marker->marker) {
			case JPEG_COM:
				str = (gchar*) marker->data;
				len = marker->data_length;

				g_hash_table_insert (metadata,
						     g_strdup ("Image:Comments"),
						     g_strndup (str, len));
				break;

			case JPEG_APP0+1:
#if defined(HAVE_LIBEXIF)
				if (strncmp ("Exif", (gchar*) (marker->data), 5) == 0) {
					read_exif ((unsigned char*) marker->data,
						   marker->data_length,
						   metadata);
				}
#endif /* HAVE_LIBEXIF */

#if defined(HAVE_EXEMPI)
				str = (gchar*) marker->data;
				len = marker->data_length;

				if (strncmp (XMP_NAMESPACE, str, XMP_NAMESPACE_LENGTH) == 0) {
					tracker_read_xmp (str + XMP_NAMESPACE_LENGTH,
							  len - XMP_NAMESPACE_LENGTH,
							  metadata);
				}
#endif /* HAVE_EXEMPI */
				break;

			default:
				marker = marker->next;
				continue;
			}

			marker = marker->next;
		}

		/* We want native size to have priority over EXIF, XMP etc */
		g_hash_table_insert (metadata,
				     g_strdup ("Image:Width"),
				     g_strdup_printf ("%u", cinfo.image_width));
		g_hash_table_insert (metadata,
				     g_strdup ("Image:Height"),
				     g_strdup_printf ("%u", cinfo.image_height));

		jpeg_destroy_decompress (&cinfo);

		fclose (jpeg);
	} else {
		close (fd_jpeg);
	}
}

TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}
