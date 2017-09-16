/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#include <png.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-date-time.h>
#include <libtracker-extract/tracker-extract.h>

#define RFC1123_DATE_FORMAT "%d %B %Y %H:%M:%S %z"
#define CM_TO_INCH          0.393700787

typedef struct {
	const gchar *title;
	const gchar *copyright;
	const gchar *creator;
	const gchar *description;
	const gchar *date;
	const gchar *license;
	const gchar *artist;
	const gchar *make;
	const gchar *model;
	const gchar *orientation;
	const gchar *white_balance;
	const gchar *fnumber;
	const gchar *flash;
	const gchar *focal_length;
	const gchar *exposure_time;
	const gchar *iso_speed_ratings;
	const gchar *metering_mode;
	const gchar *comment;
	const gchar *city;
	const gchar *state;
	const gchar *address;
	const gchar *country;
	const gchar *gps_direction;
} MergeData;

typedef struct {
	const gchar *author;
	const gchar *creator;
	const gchar *description;
	const gchar *comment;
	const gchar *copyright;
	gchar *creation_time;
	const gchar *title;
	const gchar *disclaimer;
	const gchar *software;
} PngData;

static gchar *
rfc1123_to_iso8601_date (const gchar *date)
{
	/* From: ex. RFC1123 date: "22 May 1997 18:07:10 -0600"
	 * To  : ex. ISO8601 date: "2007-05-22T18:07:10-0600"
	 */
	return tracker_date_format_to_iso8601 (date, RFC1123_DATE_FORMAT);
}

#if defined(PNG_iTXt_SUPPORTED) && (defined(HAVE_EXEMPI) || defined(HAVE_LIBEXIF))

/* Handle raw profiles by Imagemagick (at least). Hex encoded with
 * line-changes and other (undocumented/unofficial) twists.
 */
static gchar *
raw_profile_new (const gchar *input,
                 const guint  input_length,
                 guint       *output_length)
{
	static const gchar* const lut = "0123456789abcdef";
	gchar *output;
	const gchar *ptr;
	const gchar *length_ptr;
	gsize size;
	gchar *length_str;
	guint length;

	size_t len;
	size_t i;
	size_t o;
	char *p;
	char *q;

	ptr = input;

	if (*ptr != '\n') {
		return NULL;
	}

	ptr++;

	if (!g_ascii_isalpha (*ptr)) {
		return NULL;
	}

	/* Skip the type string */
	do {
		ptr++;
	} while (g_ascii_isalpha (*ptr));

	if (*ptr != '\n') {
		return NULL;
	}

	/* Hop over whitespaces */
	do {
		ptr++;
	} while (*ptr == ' ');

	if (!g_ascii_isdigit (*ptr)) {
		return NULL;
	}

	/* Get the length string */
	length_ptr = ptr;
	size = 1;

	do {
		ptr++;
		size++;
	} while (g_ascii_isdigit (*ptr));

	length_str = g_strndup (length_ptr, size - 1);

	if (*ptr != '\n') {
		return NULL;
	}

	ptr++;

	length = atoi (length_str);
	g_free (length_str);

	len = length;
	i = 0;
	o = 0;

	output = malloc (length + 1); /* A bit less with non-valid */

	o = 0;
	while (o < len) {
		do {
			gchar a = ptr[i];
			p = strchr (lut, a);
			i++;
		} while (p == 0);

		do {
			gchar b = ptr[i];
			q = strchr (lut, b);
			i++;
		} while (q == 0);

		output[o] = (((p - lut) << 4) | (q - lut));
		o++;
	}

	output[o] = '\0';
	*output_length = o;

	return output;
}

#endif /* defined(PNG_iTXt_SUPPORTED) && (defined(HAVE_EXEMPI) || defined(HAVE_LIBEXIF)) */

static void
read_metadata (TrackerResource      *metadata,
               png_structp           png_ptr,
               png_infop             info_ptr,
               png_infop             end_ptr,
               const gchar          *uri)
{
	MergeData md = { 0 };
	PngData pd = { 0 };
	TrackerExifData *ed = NULL;
	TrackerXmpData *xd = NULL;
	png_infop info_ptrs[2];
	png_textp text_ptr;
	gint info_index;
	gint num_text;
	gint i;
	gint found;
	GPtrArray *keywords;

	info_ptrs[0] = info_ptr;
	info_ptrs[1] = end_ptr;

	for (info_index = 0; info_index < 2; info_index++) {
		if ((found = png_get_text (png_ptr, info_ptrs[info_index], &text_ptr, &num_text)) < 1) {
			g_debug ("Calling png_get_text() returned %d (< 1)", found);
			continue;
		}

		for (i = 0; i < num_text; i++) {
			if (!text_ptr[i].key || !text_ptr[i].text || text_ptr[i].text[0] == '\0') {
				continue;
			}

#if defined(HAVE_EXEMPI) && defined(PNG_iTXt_SUPPORTED)
			if (g_strcmp0 ("XML:com.adobe.xmp", text_ptr[i].key) == 0) {
				/* ATM tracker_extract_xmp_read supports setting xd
				 * multiple times, keep it that way as here it's
				 * theoretically possible that the function gets
				 * called multiple times
				 */
				xd = tracker_xmp_new (text_ptr[i].text,
				                      text_ptr[i].itxt_length,
				                      uri);

				continue;
			}

			if (g_strcmp0 ("Raw profile type xmp", text_ptr[i].key) == 0) {
				gchar *xmp_buffer;
				guint xmp_buffer_length = 0;
				guint input_len;

				if (text_ptr[i].text_length) {
					input_len = text_ptr[i].text_length;
				} else {
					input_len = text_ptr[i].itxt_length;
				}

				xmp_buffer = raw_profile_new (text_ptr[i].text,
				                              input_len,
				                              &xmp_buffer_length);

				if (xmp_buffer) {
					xd = tracker_xmp_new (xmp_buffer,
					                      xmp_buffer_length,
					                      uri);
				}

				g_free (xmp_buffer);

				continue;
			}
#endif /*HAVE_EXEMPI && PNG_iTXt_SUPPORTED */

#if defined(HAVE_LIBEXIF) && defined(PNG_iTXt_SUPPORTED)
			if (g_strcmp0 ("Raw profile type exif", text_ptr[i].key) == 0) {
				gchar *exif_buffer;
				guint exif_buffer_length = 0;
				guint input_len;

				if (text_ptr[i].text_length) {
					input_len = text_ptr[i].text_length;
				} else {
					input_len = text_ptr[i].itxt_length;
				}

				exif_buffer = raw_profile_new (text_ptr[i].text,
				                               input_len,
				                               &exif_buffer_length);

				if (exif_buffer) {
					ed = tracker_exif_new (exif_buffer,
					                       exif_buffer_length,
					                       uri);
				}

				g_free (exif_buffer);

				continue;
			}
#endif /* HAVE_LIBEXIF && PNG_iTXt_SUPPORTED */

			if (g_strcmp0 (text_ptr[i].key, "Author") == 0) {
				pd.author = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Creator") == 0) {
				pd.creator = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Description") == 0) {
				pd.description = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Comment") == 0) {
				pd.comment = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Copyright") == 0) {
				pd.copyright = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Creation Time") == 0) {
				pd.creation_time = rfc1123_to_iso8601_date (text_ptr[i].text);
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Title") == 0) {
				pd.title = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Disclaimer") == 0) {
				pd.disclaimer = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0(text_ptr[i].key, "Software") == 0) {
				pd.software = text_ptr[i].text;
				continue;
			}
		}
	}

	if (!ed) {
		ed = g_new0 (TrackerExifData, 1);
	}

	if (!xd) {
		xd = g_new0 (TrackerXmpData, 1);
	}

	md.creator = tracker_coalesce_strip (3, xd->creator, pd.creator, pd.author);
	md.title = tracker_coalesce_strip (5, xd->title, pd.title, ed->document_name, xd->title2, xd->pdf_title);
	md.copyright = tracker_coalesce_strip (3, xd->rights, pd.copyright, ed->copyright);
	md.license = tracker_coalesce_strip (2, xd->license, pd.disclaimer);
	md.description = tracker_coalesce_strip (3, xd->description, pd.description, ed->description);
	md.date = tracker_coalesce_strip (5, xd->date, xd->time_original, pd.creation_time, ed->time, ed->time_original);
	md.comment = tracker_coalesce_strip (2, pd.comment, ed->user_comment);
	md.artist = tracker_coalesce_strip (3, xd->artist, ed->artist, xd->contributor);
	md.orientation = tracker_coalesce_strip (2, xd->orientation, ed->orientation);
	md.exposure_time = tracker_coalesce_strip (2, xd->exposure_time, ed->exposure_time);
	md.iso_speed_ratings = tracker_coalesce_strip (2, xd->iso_speed_ratings, ed->iso_speed_ratings);
	md.fnumber = tracker_coalesce_strip (2, xd->fnumber, ed->fnumber);
	md.flash = tracker_coalesce_strip (2, xd->flash, ed->flash);
	md.focal_length = tracker_coalesce_strip (2, xd->focal_length, ed->focal_length);
	md.metering_mode = tracker_coalesce_strip (2, xd->metering_mode, ed->metering_mode);
	md.white_balance = tracker_coalesce_strip (2, xd->white_balance, ed->white_balance);
	md.make = tracker_coalesce_strip (2, xd->make, ed->make);
	md.model = tracker_coalesce_strip (2, xd->model, ed->model);

	keywords = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

	if (md.comment) {
		tracker_resource_set_string (metadata, "nie:comment", md.comment);
	}

	if (md.license) {
		tracker_resource_set_string (metadata, "nie:license", md.license);
	}

	/* TODO: add ontology and store this ed->software */

	if (md.creator) {
		TrackerResource *creator = tracker_extract_new_contact (md.creator);

		tracker_resource_set_relation (metadata, "nco:creator", creator);

		g_object_unref (creator);
	}

	tracker_guarantee_resource_date_from_file_mtime (metadata,
	                                                 "nie:contentCreated",
	                                                 md.date,
	                                                 uri);

	if (md.description) {
		tracker_resource_set_string (metadata, "nie:description", md.description);
	}

	if (md.copyright) {
		tracker_resource_set_string (metadata, "nie:copyright", md.copyright);
	}

	tracker_guarantee_resource_title_from_file (metadata,
	                                            "nie:title",
	                                            md.title,
	                                            uri,
	                                            NULL);

	if (md.make || md.model) {
		TrackerResource *equipment = tracker_extract_new_equipment (md.make, md.model);

		tracker_resource_set_relation (metadata, "nfo:equipment", equipment);

		g_object_unref (equipment);
	}

	if (md.artist) {
		TrackerResource *artist = tracker_extract_new_contact (md.artist);

		tracker_resource_set_relation (metadata, "nco:contributor", artist);

		g_object_unref (artist);
	}

	if (md.orientation) {
		TrackerResource *orientation;

		orientation = tracker_resource_new (md.orientation);
		tracker_resource_set_relation (metadata, "nfo:orientation", orientation);
		g_object_unref (orientation);
	}

	if (md.exposure_time) {
		tracker_resource_set_string (metadata, "nmm:exposureTime", md.exposure_time);
	}

	if (md.iso_speed_ratings) {
		tracker_resource_set_string (metadata, "nmm:isoSpeed", md.iso_speed_ratings);
	}

	if (md.white_balance) {
		TrackerResource *white_balance;

		white_balance = tracker_resource_new (md.white_balance);
		tracker_resource_set_relation (metadata, "nmm:whiteBalance", white_balance);
		g_object_unref (white_balance);
	}

	if (md.fnumber) {
		tracker_resource_set_string (metadata, "nmm:fnumber", md.fnumber);
	}

	if (md.flash) {
		TrackerResource *flash;

		flash = tracker_resource_new (md.flash);
		tracker_resource_set_relation (metadata, "nmm:flash", flash);
		g_object_unref (flash);
	}

	if (md.focal_length) {
		tracker_resource_set_string (metadata, "nmm:focalLength", md.focal_length);
	}

	if (md.metering_mode) {
		TrackerResource *metering;

		metering = tracker_resource_new (md.metering_mode);
		tracker_resource_set_relation (metadata, "nmm:meteringMode", metering);
		g_object_unref (metering);
	}

	if (xd->keywords) {
		tracker_keywords_parse (keywords, xd->keywords);
	}

	if (xd->pdf_keywords) {
		tracker_keywords_parse (keywords, xd->pdf_keywords);
	}

	if (xd->rating) {
		tracker_resource_set_string (metadata, "nao:numericRating", xd->rating);
	}

	if (xd->subject) {
		tracker_keywords_parse (keywords, xd->subject);
	}

	if (xd->publisher) {
		TrackerResource *publisher = tracker_extract_new_contact (xd->publisher);

		tracker_resource_set_relation (metadata, "nco:creator", publisher);

		g_object_unref (publisher);
	}

	if (xd->type) {
		tracker_resource_set_string (metadata, "dc:type", xd->type);
	}

	if (xd->format) {
		tracker_resource_set_string (metadata, "dc:format", xd->format);
	}

	if (xd->identifier) {
		tracker_resource_set_string (metadata, "dc:identifier", xd->identifier);
	}

	if (xd->source) {
		tracker_resource_set_string (metadata, "dc:source", xd->source);
	}

	if (xd->language) {
		tracker_resource_set_string (metadata, "dc:language", xd->language);
	}

	if (xd->relation) {
		tracker_resource_set_string (metadata, "dc:relation", xd->relation);
	}

	if (xd->coverage) {
		tracker_resource_set_string (metadata, "dc:coverage", xd->coverage);
	}

	if (xd->address || xd->state || xd->country || xd->city ||
	    xd->gps_altitude || xd->gps_latitude || xd-> gps_longitude) {

		TrackerResource *location = tracker_extract_new_location (xd->address,
		        xd->state, xd->city, xd->country, xd->gps_altitude,
		        xd->gps_latitude, xd-> gps_longitude);
		
		tracker_resource_set_relation (metadata, "slo:location", location);

		g_object_unref (location);
	}

	if (xd->gps_direction) {
		tracker_resource_set_string(metadata, "nfo:heading", xd->gps_direction);
	}

	if (ed->x_resolution) {
		gdouble value;

		value = ed->resolution_unit != 3 ? g_strtod (ed->x_resolution, NULL) : g_strtod (ed->x_resolution, NULL) * CM_TO_INCH;
		tracker_resource_set_double (metadata, "nfo:horizontalResolution", value);
	}

	if (ed->y_resolution) {
		gdouble value;

		value = ed->resolution_unit != 3 ? g_strtod (ed->y_resolution, NULL) : g_strtod (ed->y_resolution, NULL) * CM_TO_INCH;
		tracker_resource_set_double (metadata, "nfo:verticalResolution", value);
	}

	if (xd->regions) {
		tracker_xmp_apply_regions_to_resource (metadata, xd);
	}

	for (i = 0; i < keywords->len; i++) {
		TrackerResource *tag;
		const gchar *p;

		p = g_ptr_array_index (keywords, i);

		tag = tracker_extract_new_tag (p);

		tracker_resource_set_relation (metadata, "nao:hasTag", tag);

		g_object_unref (tag);
	}
	g_ptr_array_free (keywords, TRUE);

	if (g_strcmp0(pd.software, "gnome-screenshot") == 0) {
		tracker_resource_add_uri (metadata, "nie:isPartOf", "nfo:image-category-screenshot");
	}

	tracker_exif_free (ed);
	tracker_xmp_free (xd);
	g_free (pd.creation_time);
}

static gboolean
guess_dlna_profile (gint          depth,
                    gint          width,
                    gint          height,
                    const gchar **dlna_profile,
                    const gchar **dlna_mimetype)
{
	const gchar *profile = NULL;

	if (dlna_profile) {
		*dlna_profile = NULL;
	}

	if (dlna_mimetype) {
		*dlna_mimetype = NULL;
	}

	if (width == 120 && height == 120) {
		profile = "PNG_LRG_ICO";
	} else if (width == 48 && height == 48) {
		profile = "PNG_SM_ICO";
	} else if (width <= 160 && height <= 160) {
		profile = "PNG_TN";
	} else if (depth <= 32 && width <= 4096 && height <= 4096) {
		profile = "PNG_LRG";
	}

	if (profile) {
		if (dlna_profile) {
			*dlna_profile = profile;
		}

		if (dlna_mimetype) {
			*dlna_mimetype = "image/png";
		}

		return TRUE;
	}

	return FALSE;
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerResource *metadata;
	goffset size;
	FILE *f;
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_ptr;
	png_bytep row_data;
	guint row;
	png_uint_32 width, height;
	gint bit_depth, color_type;
	gint interlace_type, compression_type, filter_type;
	const gchar *dlna_profile, *dlna_mimetype;
	gchar *filename, *uri;
	GFile *file;

	file = tracker_extract_info_get_file (info);
	filename = g_file_get_path (file);
	size = tracker_file_get_size (filename);

	if (size < 64) {
		return FALSE;
	}

	f = tracker_file_open (filename);
	g_free (filename);

	if (!f) {
		return FALSE;
	}

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
	                                  NULL,
	                                  NULL,
	                                  NULL);
	if (!png_ptr) {
		tracker_file_close (f, FALSE);
		return FALSE;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct (&png_ptr, NULL, NULL);
		tracker_file_close (f, FALSE);
		return FALSE;
	}

	end_ptr = png_create_info_struct (png_ptr);
	if (!end_ptr) {
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		tracker_file_close (f, FALSE);
		return FALSE;
	}

	if (setjmp (png_jmpbuf (png_ptr))) {
		png_destroy_read_struct (&png_ptr, &info_ptr, &end_ptr);
		tracker_file_close (f, FALSE);
		return FALSE;
	}

	png_init_io (png_ptr, f);
	png_read_info (png_ptr, info_ptr);

	if (!png_get_IHDR (png_ptr,
	                   info_ptr,
	                   &width,
	                   &height,
	                   &bit_depth,
	                   &color_type,
	                   &interlace_type,
	                   &compression_type,
	                   &filter_type)) {
		png_destroy_read_struct (&png_ptr, &info_ptr, &end_ptr);
		tracker_file_close (f, FALSE);
		return FALSE;
	}

	/* Read the image. FIXME We should be able to skip this step and
	 * just get the info from the end. This causes some errors atm.
	 */
	row_data = png_malloc (png_ptr, png_get_rowbytes (png_ptr, info_ptr));
	for (row = 0; row < height; row++)
		png_read_row (png_ptr, row_data, NULL);
	png_free (png_ptr, row_data);

	png_read_end (png_ptr, end_ptr);

	metadata = tracker_resource_new (NULL);

	tracker_resource_add_uri (metadata, "rdf:type", "nfo:Image");
	tracker_resource_add_uri (metadata, "rdf:type", "nmm:Photo");

	uri = g_file_get_uri (file);

	read_metadata (metadata, png_ptr, info_ptr, end_ptr, uri);
	g_free (uri);

	tracker_resource_set_int64 (metadata, "nfo:width", width);
	tracker_resource_set_int64 (metadata, "nfo:height", height);

	if (guess_dlna_profile (bit_depth, width, height, &dlna_profile, &dlna_mimetype)) {
		tracker_resource_set_string (metadata, "nmm:dlnaProfile", dlna_profile);
		tracker_resource_set_string (metadata, "nmm:dlnaMime", dlna_mimetype);
	}

	png_destroy_read_struct (&png_ptr, &info_ptr, &end_ptr);
	tracker_file_close (f, FALSE);

	tracker_extract_info_set_resource (info, metadata);
	g_object_unref (metadata);

	return TRUE;
}
