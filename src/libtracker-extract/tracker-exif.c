/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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
#include <ctype.h>

#include "tracker-exif.h"
#include "tracker-utils.h"

#ifdef HAVE_LIBEXIF

#include <libexif/exif-data.h>

#define EXIF_DATE_FORMAT "%Y:%m:%d %H:%M:%S"

/**
 * SECTION:tracker-exif
 * @title: Exif
 * @short_description: Exchangeable Image File Format (EXIF)
 * @stability: Stable
 * @include: libtracker-extract/tracker-extract.h
 *
 * Exchangeable Image File Format (EXIF) is a specification for the
 * image file format used by digital cameras. The specification uses
 * the existing JPEG, TIFF Rev. 6.0, and RIFF WAV file formats, with
 * the addition of specific metadata tags. It is not supported in JPEG
 * 2000, PNG, or GIF.
 *
 * This API is provided to remove code duplication between extractors
 * using these standards.
 **/

#ifndef HAVE_STRCASESTR

static gchar *
strcasestr (const gchar *haystack,
            const gchar *needle)
{
	gchar *p;
	gchar *startn = NULL;
	gchar *np = NULL;

	for (p = (gchar *) haystack; *p; p++) {
		if (np) {
			if (toupper (*p) == toupper (*np)) {
				if (!*++np) {
					return startn;
				}
			} else {
				np = 0;
			}
		} else if (toupper (*p) == toupper (*needle)) {
			np = (gchar *) needle + 1;
			startn = p;
		}
	}

	return NULL;
}

#endif /* HAVE_STRCASESTR */

static gchar *
get_date (ExifData *exif, 
          ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		gchar buf[1024];

		exif_entry_get_value (entry, buf, 1024);
		/* From: ex; date "2007:04:15 15:35:58"
		 * To  : ex. "2007-04-15T17:35:58+0200 where +0200 is localtime */
		return tracker_date_format_to_iso8601 (buf, EXIF_DATE_FORMAT);
	}

	return NULL;
}

static gchar *
get_focal_length (ExifData *exif, 
                  ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		gchar buf[1024];
		const gchar *end;
		exif_entry_get_value (entry, buf, 1024);
		end = g_strstr_len (buf, 1024, " mm");
		if (end) {
			return g_strndup (buf, end - buf);
		} else {
			return NULL;
		}
	}

	return NULL;
}

static gchar *
get_flash (ExifData *exif, 
           ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		gchar buf[1024];

		exif_entry_get_value (entry, buf, 1024);

		if (strcasestr (buf, "flash fired")) {
			return g_strdup ("nmm:flash-on");
		}

		return g_strdup ("nmm:flash-off");
	}

	return NULL;
}

static gchar *
get_fnumber (ExifData *exif, 
             ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		gchar buf[1024];
		gchar *new_fn;

		exif_entry_get_value (entry, buf, 1024);

		if (strlen (buf) <= 0) {
			return NULL;
		}

		new_fn = g_strdup (buf);

		if (new_fn[0] == 'F') {
			new_fn[0] = ' ';
		} else if (buf[0] == 'f' && new_fn[1] == '/') {
			new_fn[0] = new_fn[1] = ' ';
		}

		return g_strstrip (new_fn);
	}

	return NULL;
}

static gchar *
get_exposure_time (ExifData *exif, 
                   ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		gchar buf[1024];
		gchar *sep;

		exif_entry_get_value (entry, buf, 1024);

		sep = strchr (buf, '/');

		if (sep) {
			gdouble fraction;

			fraction = g_ascii_strtod (sep + 1, NULL);

			if (fraction > 0.0) {
				gdouble val;
				gchar   bufr[G_ASCII_DTOSTR_BUF_SIZE];

				val = 1.0f / fraction;
				g_ascii_dtostr (bufr, sizeof(bufr), val);

				return g_strdup (bufr);
			}
		}

		return g_strdup (buf);
	}

	return NULL;
}

static gchar *
get_orientation (ExifData *exif, 
                 ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		gchar buf[1024];

		exif_entry_get_value (entry, buf, 1024);

		if (g_ascii_strcasecmp (buf, "top - left") == 0)
			return g_strdup ("nfo:orientation-top");
		else if (g_ascii_strcasecmp (buf, "top - right") == 0)
			return g_strdup ("nfo:orientation-top-mirror");
		else if (g_ascii_strcasecmp (buf, "bottom - right") == 0)
			return g_strdup ("nfo:orientation-bottom");
		else if (g_ascii_strcasecmp (buf, "bottom - left") == 0)
			return g_strdup ("nfo:orientation-bottom-mirror");
		else if (g_ascii_strcasecmp (buf, "left - top") == 0)
			return g_strdup ("nfo:orientation-left-mirror");
		else if (g_ascii_strcasecmp (buf, "right - top") == 0)
			return g_strdup ("nfo:orientation-right");
		else if (g_ascii_strcasecmp (buf, "right - bottom") == 0)
			return g_strdup ("nfo:orientation-right-mirror");
		else if (g_ascii_strcasecmp (buf, "left - bottom") == 0)
			return g_strdup ("nfo:orientation-left");

		return g_strdup ("nfo:orientation-top");
	}

	return NULL;
}

static gchar *
get_metering_mode (ExifData *exif,
                   ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		gchar buf[1024];

		exif_entry_get_value (entry, buf, 1024);

		if (strcasestr (buf, "center"))
			return g_strdup ("nmm:metering-mode-center-weighted-average");
		else if (strcasestr (buf, "average"))
			return g_strdup ("nmm:metering-mode-average");
		else if (strcasestr (buf, "spot"))
			return g_strdup ("nmm:metering-mode-spot");
		else if (strcasestr (buf, "multispot"))
			return g_strdup ("nmm:metering-mode-multispot");
		else if (strcasestr (buf, "pattern"))
			return g_strdup ("nmm:metering-mode-pattern");
		else if (strcasestr (buf, "partial"))
			return g_strdup ("nmm:metering-mode-partial");
		else
			return g_strdup ("nmm:metering-mode-other");
	}

	return NULL;
}

static gchar *
get_white_balance (ExifData *exif, 
                   ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		gchar buf[1024];

		exif_entry_get_value (entry, buf, 1024);

		if (strcasestr (buf, "auto"))
			return g_strdup ("nmm:white-balance-auto");

		/* Found in the field: sunny, fluorescent, incandescent, cloudy.
		 * These will this way also yield as manual. */

		return g_strdup ("nmm:white-balance-manual");
	}

	return NULL;
}

static gchar *
get_value (ExifData *exif, 
           ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		gchar buf[1024];

		exif_entry_get_value (entry, buf, 1024);

		return g_strdup (buf);
	}

	return NULL;
}

#endif /* HAVE_LIBEXIF */

/**
 * tracker_exif_read:
 * @buffer: a chunk of data with exif data in it.
 * @len: the size of @buffer.
 * @uri: the URI this is related to.
 * @data: a pointer to a TrackerExifData struture to populate.
 *
 * This function takes @len bytes of @buffer and runs it through the
 * EXIF library. The result is that @data is populated with the EXIF
 * data found in @uri.
 *
 * Returns: %TRUE if the @data was populated successfully, otherwise
 * %FALSE is returned.
 *
 * Since: 0.8
 **/
gboolean
tracker_exif_read (const unsigned char *buffer,
                   size_t               len,
                   const gchar         *uri,
                   TrackerExifData     *data)
{
#ifdef HAVE_LIBEXIF
	ExifData *exif;
#endif

	g_return_val_if_fail (buffer != NULL, FALSE);
	g_return_val_if_fail (len > 0, FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	
	memset (data, 0, sizeof (TrackerExifData));

#ifdef HAVE_LIBEXIF

	exif = exif_data_new ();

	g_return_val_if_fail (exif != NULL, FALSE);

	exif_data_set_option (exif, EXIF_DATA_OPTION_IGNORE_UNKNOWN_TAGS);
	exif_data_unset_option (exif, EXIF_DATA_OPTION_FOLLOW_SPECIFICATION);
	exif_data_set_option (exif, EXIF_DATA_OPTION_DONT_CHANGE_MAKER_NOTE);

	exif_data_load_data (exif, (unsigned char *) buffer, len);

	/* Unused in the current only user of this code (JPeg extractor)
	   if (!data->y_dimension)
	   data->y_dimension = get_value (exif, EXIF_TAG_PIXEL_Y_DIMENSION);
	   if (!data->x_dimension)
	   data->x_dimension = get_value (exif, EXIF_TAG_PIXEL_X_DIMENSION);
	   if (!data->image_width)
	   data->image_width = get_value (exif, EXIF_TAG_RELATED_IMAGE_WIDTH);
	*/

	if (!data->document_name)
		data->document_name = get_value (exif, EXIF_TAG_DOCUMENT_NAME);
	if (!data->time)
		data->time = get_date (exif, EXIF_TAG_DATE_TIME);
	if (!data->time_original)
		data->time_original = get_date (exif, EXIF_TAG_DATE_TIME_ORIGINAL);
	if (!data->artist)
		data->artist = get_value (exif, EXIF_TAG_ARTIST);
	if (!data->user_comment)
		data->user_comment = get_value (exif, EXIF_TAG_USER_COMMENT);
	if (!data->description)
		data->description = get_value (exif, EXIF_TAG_IMAGE_DESCRIPTION);
	if (!data->make)
		data->make = get_value (exif, EXIF_TAG_MAKE);
	if (!data->model)
		data->model = get_value (exif, EXIF_TAG_MODEL);
	if (!data->orientation)
		data->orientation = get_orientation (exif, EXIF_TAG_ORIENTATION);
	if (!data->exposure_time)
		data->exposure_time = get_exposure_time (exif, EXIF_TAG_EXPOSURE_TIME);
	if (!data->fnumber)
		data->fnumber = get_fnumber (exif, EXIF_TAG_FNUMBER);
	if (!data->flash)
		data->flash = get_flash (exif, EXIF_TAG_FLASH);
	if (!data->focal_length)
		data->focal_length = get_focal_length (exif, EXIF_TAG_FOCAL_LENGTH);
	if (!data->iso_speed_ratings)
		data->iso_speed_ratings = get_value (exif, EXIF_TAG_ISO_SPEED_RATINGS);
	if (!data->metering_mode)
		data->metering_mode = get_metering_mode (exif, EXIF_TAG_METERING_MODE);
	if (!data->white_balance)
		data->white_balance = get_white_balance (exif, EXIF_TAG_WHITE_BALANCE);
	if (!data->copyright)
		data->copyright = get_value (exif, EXIF_TAG_COPYRIGHT);
	if (!data->software)
		data->software = get_value (exif, EXIF_TAG_SOFTWARE);


	exif_data_free (exif);
#endif /* HAVE_LIBEXIF */

	return TRUE;
}

