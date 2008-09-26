/* Tracker Extract - extracts embedded metadata from files
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include "tracker-extract.h"
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <libexif/exif-data.h>

#define EXIF_DATE_FORMAT "%Y:%m:%d %H:%M:%S"

static gchar *
date_to_iso8601 (gchar *exif_date)
{
	/* ex; date "2007:04:15 15:35:58"
	   To
	   ex. "2007-04-15T17:35:58+0200 where +0200 is localtime
	*/
	return tracker_generic_date_to_iso8601 (exif_date, EXIF_DATE_FORMAT);
}


static gchar *
fix_focal_length (gchar *fl)
{
	return g_strndup (fl, (strstr (fl, "mm") - fl));
}


static gchar *
fix_flash (gchar *flash)
{
	if (g_str_has_prefix (flash, "No")) {
		return g_strdup ("0");
	} else {
		return g_strdup ("1");
	}
}


static gchar *
fix_fnumber (gchar *fn)
{
	if (fn && fn[0] == 'F') {
		fn[0] = ' ';

	} else if (fn && fn[0] == 'f' && fn[1] == '/') {
		fn[0] = ' ', fn[1] = ' ';
	}

	return fn;
}


static gchar *
fix_exposure_time (gchar *et)
{
	gchar *sep = strchr (et, '/');

	if (sep) {
		gdouble fraction = g_ascii_strtod (sep+1, NULL);

		if (fraction > 0) {
			gdouble val = 1.0f / fraction;
			char str_value[30];

			g_ascii_dtostr (str_value, 30, val);
			return g_strdup (str_value);
		}
	}

	return et;
}


typedef gchar * (*PostProcessor) (gchar *);


typedef struct {
	ExifTag       tag;
	gchar	      *name;
	PostProcessor post;
} TagType;


TagType tags[] = {
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


static void
tracker_extract_exif (const gchar *filename, GHashTable *metadata)
{
	ExifData *exif;
	TagType  *p;

	exif = exif_data_new_from_file (filename);

	for (p = tags; p->name; ++p) {
		ExifEntry *entry = exif_data_get_entry (exif, p->tag);

		if (entry) {
			gchar buffer[1024];

			exif_entry_get_value (entry, buffer, 1024);

			if (p->post) {
				g_hash_table_insert (metadata, g_strdup (p->name),
						     g_strdup ((*p->post) (buffer)));
			} else {
				g_hash_table_insert (metadata, g_strdup (p->name),
						     g_strdup (buffer));
			}
		}
	}
}


TrackerExtractorData data[] = {
	{ "image/jpeg", tracker_extract_exif },
	{ NULL, NULL }
};


TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}
