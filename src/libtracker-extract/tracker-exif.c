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

static gchar *
get_date (ExifData *exif,
          ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		gchar buf[1024];

		exif_entry_get_value (entry, buf, 1024);
		/* From: ex; date "2007:04:15 15:35:58"
		 * To  : ex. "2007-04-15T17:35:58+0200 where +0200 is offset w.r.t gmt */
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
		ExifByteOrder order;
		gushort flash;

		order = exif_data_get_byte_order (exif);
		flash = exif_get_short (entry->data, order);

		switch (flash) {
		case 0x0000: /* No flash */
		case 0x0005: /* Without strobe */
		case 0x0008: /* Flash did not fire */
		case 0x0010: /* Flash in compulsory mode, did not fire */
		case 0x0018: /* Flash in auto mode, did not fire */
		case 0x0058: /* Only red-eye reduction mode */
			return g_strdup ("nmm:flash-off");
		default:
			return g_strdup ("nmm:flash-on");
		}
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
		ExifByteOrder order;
		gushort orientation;

		order = exif_data_get_byte_order (exif);
		orientation = exif_get_short (entry->data, order);

		switch (orientation) {
		case 1:
			return g_strdup ("nfo:orientation-top");
		case 2:
			return g_strdup ("nfo:orientation-top-mirror");
		case 3:
			return g_strdup ("nfo:orientation-bottom");
		case 4:
			return g_strdup ("nfo:orientation-bottom-mirror");
		case 5:
			return g_strdup ("nfo:orientation-left-mirror");
		case 6:
			return g_strdup ("nfo:orientation-right");
		case 7:
			return g_strdup ("nfo:orientation-right-mirror");
		case 8:
			return g_strdup ("nfo:orientation-left");
		default:
			return g_strdup ("nfo:orientation-top");
		}
	}

	return NULL;
}

static gchar *
get_metering_mode (ExifData *exif,
                   ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		ExifByteOrder order;
		gushort metering;

		order = exif_data_get_byte_order (exif);
		metering = exif_get_short (entry->data, order);

		switch (metering) {
		case 1:
			return g_strdup ("nmm:metering-mode-average");
		case 2:
			return g_strdup ("nmm:metering-mode-center-weighted-average");
		case 3:
			return g_strdup ("nmm:metering-mode-spot");
		case 4:
			return g_strdup ("nmm:metering-mode-multispot");
		case 5:
			return g_strdup ("nmm:metering-mode-pattern");
		case 6:
			return g_strdup ("nmm:metering-mode-partial");
		default:
			return g_strdup ("nmm:metering-mode-other");
		}
	}

	return NULL;
}

static gchar *
get_white_balance (ExifData *exif,
                   ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		ExifByteOrder order;
		gushort white_balance;

		order = exif_data_get_byte_order (exif);
		white_balance = exif_get_short (entry->data, order);

		if (white_balance == 0)
			return g_strdup ("nmm:white-balance-auto");

		/* Found in the field: sunny, fluorescent, incandescent, cloudy.
		 * These will this way also yield as manual. */
		return g_strdup ("nmm:white-balance-manual");
	}

	return NULL;
}

static gchar *
get_gps_coordinate (ExifData *exif,
		    ExifTag   tag,
		    ExifTag   reftag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);
	ExifEntry *refentry = exif_data_get_entry (exif, reftag);

	if (entry && refentry) {
		ExifByteOrder order;
		ExifRational c1,c2,c3;
		gfloat f;
		gchar ref;

		order = exif_data_get_byte_order (exif);
		c1 = exif_get_rational (entry->data, order);
		c2 = exif_get_rational (entry->data+8, order);
		c3 = exif_get_rational (entry->data+16, order);
		ref = exif_get_short (refentry->data, order);
		
		f = (double)c1.numerator/c1.denominator+
		  (double)c2.numerator/(c2.denominator*60)+
		  (double)c3.numerator/(c3.denominator*60*60);

		if (ref == 'S' || ref == 'W') {
		      f = -1 * f;
		}
		return g_strdup_printf ("%f", f);
	}

	return NULL;
}

static gchar *
get_gps_altitude (ExifData *exif,
		  ExifTag   tag,
		  ExifTag   reftag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);
	ExifEntry *refentry = exif_data_get_entry (exif, reftag);

	if (entry) {
		ExifByteOrder order;
		ExifRational c;
		gfloat f;
		gboolean ref;

		order = exif_data_get_byte_order (exif);
		c = exif_get_rational (entry->data, order);
		f = (double)c.numerator/c.denominator;

		/* Strictly speaking it is invalid not to have this
		   but.. let's try to cope here */
		if (refentry) {
		        ref = exif_get_short (refentry->data, order);
		
			if (ref == 1) {
			        f = -1 * f;
			}
		}
		return g_strdup_printf ("%f", f);
	}

	return NULL;
}

static gint
get_int (ExifData *exif,
         ExifTag   tag)
{
	ExifEntry *entry = exif_data_get_entry (exif, tag);

	if (entry) {
		ExifByteOrder order;

		order = exif_data_get_byte_order (exif);
		return (gint) exif_get_short (entry->data, order);
	}

	return -1;
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

static gboolean
parse_exif (const unsigned char *buffer,
            size_t               len,
            const gchar         *uri,
            TrackerExifData     *data)
{
#ifdef HAVE_LIBEXIF
	ExifData *exif;
#endif

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
	if (!data->copyright) {
		gchar *strip_off;
		data->copyright = get_value (exif, EXIF_TAG_COPYRIGHT);
		/* exiftool catenates this to the string, so we don't need it */
		if (data->copyright) {
			strip_off = strstr (data->copyright, " (Photographer) -  (Editor)");
			if (strip_off) {
				*strip_off = '\0';
			}
		}
	}
	if (!data->software)
		data->software = get_value (exif, EXIF_TAG_SOFTWARE);

	if (!data->resolution_unit)
		data->resolution_unit = get_int (exif, EXIF_TAG_RESOLUTION_UNIT);
	if (!data->x_resolution)
		data->x_resolution = get_value (exif, EXIF_TAG_X_RESOLUTION);
	if (!data->y_resolution)
		data->y_resolution = get_value (exif, EXIF_TAG_Y_RESOLUTION);

	if(!data->gps_altitude)
		data->gps_altitude = get_gps_altitude (exif, EXIF_TAG_GPS_ALTITUDE, EXIF_TAG_GPS_ALTITUDE_REF);
	if(!data->gps_latitude)
		data->gps_latitude = get_gps_coordinate (exif, EXIF_TAG_GPS_LATITUDE, EXIF_TAG_GPS_LATITUDE_REF);
	if(!data->gps_longitude)
		data->gps_longitude = get_gps_coordinate (exif, EXIF_TAG_GPS_LONGITUDE, EXIF_TAG_GPS_LONGITUDE_REF);
	if(!data->gps_direction)
		data->gps_direction = get_value (exif, EXIF_TAG_GPS_IMG_DIRECTION);

	exif_data_free (exif);
#endif /* HAVE_LIBEXIF */

	return TRUE;
}

#ifndef TRACKER_DISABLE_DEPRECATED

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
 *
 * Deprecated: 0.9. Use tracker_exif_new() instead.
 **/
gboolean
tracker_exif_read (const unsigned char *buffer,
                   size_t               len,
                   const gchar         *uri,
                   TrackerExifData     *data)
{
	g_return_val_if_fail (buffer != NULL, FALSE);
	g_return_val_if_fail (len > 0, FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	return parse_exif (buffer, len, uri, data);
}

#endif /* TRACKER_DISABLE_DEPRECATED */

/**
 * tracker_exif_new:
 * @buffer: a chunk of data with exif data in it.
 * @len: the size of @buffer.
 * @uri: the URI this is related to.
 *
 * This function takes @len bytes of @buffer and runs it through the
 * EXIF library.
 *
 * Returns: a newly allocated #TrackerExifData struct if EXIF data was
 * found, %NULL otherwise. Free the returned struct with tracker_exif_free().
 *
 * Since: 0.10
 **/
TrackerExifData *
tracker_exif_new (const guchar *buffer,
                  size_t        len,
                  const gchar  *uri)
{
	TrackerExifData *data;

	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (len > 0, NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	data = g_new0 (TrackerExifData, 1);

	if (!parse_exif (buffer, len, uri, data)) {
		tracker_exif_free (data);
		return NULL;
	}

	return data;
}

/**
 * tracker_exif_free:
 * @data: a #TrackerExifData
 *
 * Frees @data and all #TrackerExifData members. %NULL will produce a
 * a warning.
 *
 * Since: 0.10
 **/
void
tracker_exif_free (TrackerExifData *data)
{
	g_return_if_fail (data != NULL);

	g_free (data->y_dimension);
	g_free (data->x_dimension);
	g_free (data->image_width);
	g_free (data->document_name);
	g_free (data->time);
	g_free (data->time_original);
	g_free (data->artist);
	g_free (data->user_comment);
	g_free (data->description);
	g_free (data->make);
	g_free (data->model);
	g_free (data->orientation);
	g_free (data->exposure_time);
	g_free (data->fnumber);
	g_free (data->flash);
	g_free (data->focal_length);
	g_free (data->iso_speed_ratings);
	g_free (data->metering_mode);
	g_free (data->white_balance);
	g_free (data->copyright);
	g_free (data->software);
	g_free (data->x_resolution);
	g_free (data->y_resolution);
	g_free (data->gps_altitude);
	g_free (data->gps_latitude);
	g_free (data->gps_longitude);
	g_free (data->gps_direction);

	g_free (data);
}
