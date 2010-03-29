/*
 * Copyright (C) 2006, Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <png.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-file-utils.h>

#include <libtracker-extract/tracker-extract.h>

#define RFC1123_DATE_FORMAT "%d %B %Y %H:%M:%S %z"

typedef struct {
	gchar *title, *copyright, *creator, *description, *date, *license;
	gchar *artist;
	gchar *camera;
	gchar *orientation;
	gchar *white_balance;
	gchar *fnumber;
	gchar *flash;
	gchar *focal_length;
	gchar *exposure_time;
	gchar *iso_speed_ratings;
	gchar *metering_mode;
	gchar *comment;
	gchar *city;
	gchar *state;
	gchar *address;
	gchar *country;
} PngNeedsMergeData;

typedef struct {
	gchar *author, *creator, *description, *comment, *copyright,
		*creation_time, *title, *disclaimer;
} PngData;

static gchar *rfc1123_to_iso8601_date (gchar                *rfc_date);
static void   extract_png             (const gchar          *filename,
                                       TrackerSparqlBuilder *preupdate,
                                       TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "image/png", extract_png },
	{ "sketch/png", extract_png },
	{ NULL, NULL }
};

static gchar *
rfc1123_to_iso8601_date (gchar *date)
{
	/* From: ex. RFC1123 date: "22 May 1997 18:07:10 -0600"
	 * To  : ex. ISO8601 date: "2007-05-22T18:07:10-0600"
	 */
	return tracker_date_format_to_iso8601 (date, RFC1123_DATE_FORMAT);
}

static void
insert_keywords (TrackerSparqlBuilder *metadata, const gchar *uri, gchar *keywords)
{
	char *lasts, *keyw;
	size_t len;

	keyw = keywords;
	keywords = strchr (keywords, '"');
	if (keywords)
		keywords++;
	else
		keywords = keyw;

	len = strlen (keywords);
	if (keywords[len - 1] == '"')
		keywords[len - 1] = '\0';

	for (keyw = strtok_r (keywords, ",; ", &lasts); keyw;
	     keyw = strtok_r (NULL, ",; ", &lasts)) {
		tracker_sparql_builder_predicate (metadata, "nao:hasTag");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nao:Tag");

		tracker_sparql_builder_predicate (metadata, "nao:prefLabel");
		tracker_sparql_builder_object_unvalidated (metadata, keyw);

		tracker_sparql_builder_object_blank_close (metadata);
	}
}

static void
read_metadata (png_structp png_ptr, png_infop info_ptr, const gchar *uri, TrackerSparqlBuilder *metadata)
{
	gint         num_text;
	png_textp    text_ptr;
	PngNeedsMergeData merge_data = { 0 };
	PngData png_data = { 0 };
	TrackerExifData exif_data = { 0 };
	TrackerXmpData xmp_data = { 0 };

	if (png_get_text (png_ptr, info_ptr, &text_ptr, &num_text) > 0) {
		gint i;

		for (i = 0; i < num_text; i++) {

			if (!text_ptr[i].key || !text_ptr[i].text || text_ptr[i].text[0] == '\0') {
				continue;
			}

#if defined(HAVE_EXEMPI) && defined(PNG_iTXt_SUPPORTED)

			if (g_strcmp0 ("XML:com.adobe.xmp", text_ptr[i].key) == 0) {

				/* ATM tracker_extract_xmp_read supports setting xmp_data
				 * multiple times, keep it that way as here it's
				 * theoretically possible that the function gets
				 * called multiple times */

				tracker_extract_xmp_read (text_ptr[i].text,
				                          text_ptr[i].itxt_length,
				                          uri, &xmp_data);

				continue;
			}
#endif

#if defined(HAVE_LIBEXIF) && defined(PNG_iTXt_SUPPORTED)

			/* I'm not certain this is the key for EXIF. Using key according to
			 * this document about exiftool:
			 * http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/PNG.html#TextualData */

			if (g_strcmp0 ("Raw profile type exif", text_ptr[i].key) == 0) {
				tracker_extract_exif_read (text_ptr[i].text,
				                           text_ptr[i].itxt_length, 
				                           uri, &exif_data);
				continue;
			}
#endif /* HAVE_LIBEXIF */

			if (g_strcmp0 (text_ptr[i].key, "Author") == 0) {
				png_data.author = g_strdup (text_ptr[i].text);
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Creator") == 0) {
				png_data.creator = g_strdup (text_ptr[i].text);
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Description") == 0) {
				png_data.description = g_strdup (text_ptr[i].text);
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Comment") == 0) {
				png_data.comment = g_strdup (text_ptr[i].text);
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Copyright") == 0) {
				png_data.copyright = g_strdup (text_ptr[i].text);
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Creation Time") == 0) {
				png_data.creation_time = rfc1123_to_iso8601_date (text_ptr[i].text);
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Title") == 0) {
				png_data.title = g_strdup (text_ptr[i].text);
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Disclaimer") == 0) {
				png_data.disclaimer = g_strdup (text_ptr[i].text);
				continue;
			}
		}

		/* Don't merge if the make is in the model */
		if ((xmp_data.make == NULL || xmp_data.model == NULL) ||
		    (xmp_data.make && xmp_data.model && strstr (xmp_data.model, xmp_data.make) == NULL)) {
			merge_data.camera = tracker_merge (" ", 2, xmp_data.make, xmp_data.model);
		} else {
			merge_data.camera = g_strdup (xmp_data.model);
			g_free (xmp_data.model);
			g_free (xmp_data.make);
		}

		if (!merge_data.camera) {
			if ((exif_data.make == NULL || exif_data.model == NULL) ||
			    (exif_data.make && exif_data.model && strstr (exif_data.model, exif_data.make) == NULL)) {
				merge_data.camera = tracker_merge (" ", 2, exif_data.make, exif_data.model);
			} else {
				merge_data.camera = g_strdup (exif_data.model);
				g_free (exif_data.model);
				g_free (exif_data.make);
			}
		} else {
			g_free (exif_data.model);
			g_free (exif_data.make);
		}

		merge_data.creator = tracker_coalesce (3, xmp_data.creator, 
		                                       png_data.creator,
		                                       png_data.author);

		merge_data.title = tracker_coalesce (5, xmp_data.title, 
		                                     png_data.title,
		                                     exif_data.document_name,
		                                     xmp_data.title2,
		                                     xmp_data.pdf_title);

		merge_data.copyright = tracker_coalesce (3, xmp_data.rights, 
		                                         png_data.copyright,
		                                         exif_data.copyright);

		merge_data.license = tracker_coalesce (2, xmp_data.license, 
		                                       png_data.disclaimer);

		merge_data.description = tracker_coalesce (3, xmp_data.description,
		                                           png_data.description,
		                                           exif_data.description);

		merge_data.date = tracker_coalesce (5, xmp_data.date,
		                                    xmp_data.time_original,
		                                    png_data.creation_time,
		                                    exif_data.time,
		                                    exif_data.time_original);

		merge_data.comment = tracker_coalesce (2, png_data.comment,
		                                       exif_data.user_comment);

		merge_data.artist = tracker_coalesce (3, xmp_data.artist,
		                                      exif_data.artist,
		                                      xmp_data.contributor);

		merge_data.orientation = tracker_coalesce (2, xmp_data.orientation, 
		                                           exif_data.orientation);

		merge_data.exposure_time = tracker_coalesce (2, xmp_data.exposure_time, 
		                                             exif_data.exposure_time);

		merge_data.iso_speed_ratings = tracker_coalesce (2, xmp_data.iso_speed_ratings, 
		                                                 exif_data.iso_speed_ratings);

		merge_data.fnumber = tracker_coalesce (2, xmp_data.fnumber, 
		                                       exif_data.fnumber);

		merge_data.flash = tracker_coalesce (2, xmp_data.flash, 
		                                     exif_data.flash);

		merge_data.focal_length = tracker_coalesce (2, xmp_data.focal_length, 
		                                            exif_data.focal_length);

		merge_data.metering_mode = tracker_coalesce (2, xmp_data.metering_mode, 
		                                             exif_data.metering_mode);

		merge_data.white_balance = tracker_coalesce (2, xmp_data.white_balance, 
		                                             exif_data.white_balance);

		if (merge_data.comment) {
			tracker_sparql_builder_predicate (metadata, "nie:comment");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.comment);
			g_free (merge_data.comment);
		}

		if (merge_data.license) {
			tracker_sparql_builder_predicate (metadata, "nie:license");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.license);
			g_free (merge_data.license);
		}

		/* TODO: add ontology and store this */
		g_free (exif_data.software);

		g_free (exif_data.x_dimension);
		g_free (exif_data.y_dimension);
		g_free (exif_data.image_width);

		if (merge_data.creator) {
			tracker_sparql_builder_predicate (metadata, "nco:creator");

			tracker_sparql_builder_object_blank_open (metadata);
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "nco:Contact");

			tracker_sparql_builder_predicate (metadata, "nco:fullname");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.creator);
			tracker_sparql_builder_object_blank_close (metadata);
			g_free (merge_data.creator);
		}

		if (merge_data.date) {
			tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.date);
			g_free (merge_data.date);
		}

		if (merge_data.description) {
			tracker_sparql_builder_predicate (metadata, "nie:description");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.description);
			g_free (merge_data.description);
		}

		if (merge_data.copyright) {
			tracker_sparql_builder_predicate (metadata, "nie:copyright");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.copyright);
			g_free (merge_data.copyright);
		}

		if (merge_data.title) {
			tracker_sparql_builder_predicate (metadata, "nie:title");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.title);
			g_free (merge_data.title);
		}

		if (merge_data.camera) {
			tracker_sparql_builder_predicate (metadata, "nmm:camera");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.camera);
			g_free (merge_data.camera);
		}

		if (merge_data.artist) {
			tracker_sparql_builder_predicate (metadata, "nco:contributor");

			tracker_sparql_builder_object_blank_open (metadata);
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "nco:Contact");

			tracker_sparql_builder_predicate (metadata, "nco:fullname");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.artist);
			tracker_sparql_builder_object_blank_close (metadata);
			g_free (merge_data.artist);
		}

		if (merge_data.orientation) {
			tracker_sparql_builder_predicate (metadata, "nfo:orientation");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.orientation);
			g_free (merge_data.orientation);
		}

		if (merge_data.exposure_time) {
			tracker_sparql_builder_predicate (metadata, "nmm:exposureTime");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.exposure_time);
			g_free (merge_data.exposure_time);
		}

		if (merge_data.iso_speed_ratings) {
			tracker_sparql_builder_predicate (metadata, "nmm:isoSpeed");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.iso_speed_ratings);
			g_free (merge_data.iso_speed_ratings);
		}

		if (merge_data.white_balance) {
			tracker_sparql_builder_predicate (metadata, "nmm:whiteBalance");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.white_balance);
			g_free (merge_data.white_balance);
		}

		if (merge_data.fnumber) {
			tracker_sparql_builder_predicate (metadata, "nmm:fnumber");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.fnumber);
			g_free (merge_data.fnumber);
		}

		if (merge_data.flash) {
			tracker_sparql_builder_predicate (metadata, "nmm:flash");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.flash);
			g_free (merge_data.flash);
		}

		if (merge_data.focal_length) {
			tracker_sparql_builder_predicate (metadata, "nmm:focalLength");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.focal_length);
			g_free (merge_data.focal_length);
		}

		if (merge_data.metering_mode) {
			tracker_sparql_builder_predicate (metadata, "nmm:meteringMode");
			tracker_sparql_builder_object_unvalidated (metadata, merge_data.metering_mode);
			g_free (merge_data.metering_mode);
		}


		if (xmp_data.keywords) {
			insert_keywords (metadata, uri, xmp_data.keywords);
			g_free (xmp_data.keywords);
		}

		if (xmp_data.pdf_keywords) {
			insert_keywords (metadata, uri, xmp_data.pdf_keywords);
			g_free (xmp_data.pdf_keywords);
		}

		if (xmp_data.rating) {
			tracker_sparql_builder_predicate (metadata, "nao:numericRating");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data.rating);
			g_free (xmp_data.rating);
		}

		if (xmp_data.subject) {
			insert_keywords (metadata, uri, xmp_data.subject);
			g_free (xmp_data.subject);
		}

		if (xmp_data.publisher) {
			tracker_sparql_builder_predicate (metadata, "nco:publisher");

			tracker_sparql_builder_object_blank_open (metadata);
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "nco:Contact");

			tracker_sparql_builder_predicate (metadata, "nco:fullname");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data.publisher);
			tracker_sparql_builder_object_blank_close (metadata);
			g_free (xmp_data.publisher);
		}

		if (xmp_data.type) {
			tracker_sparql_builder_predicate (metadata, "dc:type");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data.type);
			g_free (xmp_data.type);
		}

		if (xmp_data.format) {
			tracker_sparql_builder_predicate (metadata, "dc:format");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data.format);
			g_free (xmp_data.format);
		}

		if (xmp_data.identifier) {
			tracker_sparql_builder_predicate (metadata, "dc:identifier");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data.identifier);
			g_free (xmp_data.identifier);
		}

		if (xmp_data.source) {
			tracker_sparql_builder_predicate (metadata, "dc:source");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data.source);
			g_free (xmp_data.source);
		}

		if (xmp_data.language) {
			tracker_sparql_builder_predicate (metadata, "dc:language");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data.language);
			g_free (xmp_data.language);
		}

		if (xmp_data.relation) {
			tracker_sparql_builder_predicate (metadata, "dc:relation");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data.relation);
			g_free (xmp_data.relation);
		}

		if (xmp_data.coverage) {
			tracker_sparql_builder_predicate (metadata, "dc:coverage");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data.coverage);
			g_free (xmp_data.coverage);
		}

		if (xmp_data.address || xmp_data.country || xmp_data.city) {
			tracker_sparql_builder_predicate (metadata, "mlo:location");
	
			tracker_sparql_builder_object_blank_open (metadata);
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "mlo:GeoPoint");
	
			if (xmp_data.address) {
				tracker_sparql_builder_predicate (metadata, "mlo:address");
				tracker_sparql_builder_object_unvalidated (metadata, xmp_data.address);
				g_free (xmp_data.address);
			}
	
			if (xmp_data.state) {
				tracker_sparql_builder_predicate (metadata, "mlo:state");
				tracker_sparql_builder_object_unvalidated (metadata, xmp_data.state);
				g_free (xmp_data.state);
			}
	
			if (xmp_data.city) {
				tracker_sparql_builder_predicate (metadata, "mlo:city");
				tracker_sparql_builder_object_unvalidated (metadata, xmp_data.city);
				g_free (xmp_data.city);
			}
	
			if (xmp_data.country) {
				tracker_sparql_builder_predicate (metadata, "mlo:country");
				tracker_sparql_builder_object_unvalidated (metadata, xmp_data.country);
				g_free (xmp_data.country);
			}
		
			tracker_sparql_builder_object_blank_close (metadata);
		}
	}
}

static void
extract_png (const gchar          *uri,
             TrackerSparqlBuilder *preupdate,
             TrackerSparqlBuilder *metadata)
{
	goffset      size;
	FILE        *f;
	png_structp  png_ptr;
	png_infop    info_ptr;
	png_infop    end_ptr;
	png_bytepp   row_pointers;
	guint        row;
	png_uint_32  width, height;
	gint         bit_depth, color_type;
	gint         interlace_type, compression_type, filter_type;
	gchar       *filename = g_filename_from_uri (uri, NULL, NULL);

	size = tracker_file_get_size (filename);

	if (size < 64) {
		return;
	}

	f = tracker_file_open (filename, "r", FALSE);

	if (f) {
		png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
		                                  NULL,
		                                  NULL,
		                                  NULL);
		if (!png_ptr) {
			tracker_file_close (f, FALSE);
			g_free (filename);
			return;
		}

		info_ptr = png_create_info_struct (png_ptr);
		if (!info_ptr) {
			png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
			tracker_file_close (f, FALSE);
			g_free (filename);
			return;
		}

		end_ptr = png_create_info_struct (png_ptr);
		if (!end_ptr) {
			png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
			tracker_file_close (f, FALSE);
			g_free (filename);
			return;
		}

		if (setjmp (png_jmpbuf (png_ptr))) {
			png_destroy_read_struct (&png_ptr, &info_ptr, &end_ptr);
			tracker_file_close (f, FALSE);
			return;
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
			g_free (filename);
			return;
		}

		/* Read the image. FIXME We should be able to skip this step and
		 * just get the info from the end. This causes some errors atm.
		 */
		row_pointers = g_new0 (png_bytep, height);

		for (row = 0; row < height; row++) {
			row_pointers[row] = png_malloc (png_ptr,
			                                png_get_rowbytes (png_ptr,info_ptr));
		}

		png_read_image (png_ptr, row_pointers);

		for (row = 0; row < height; row++) {
			png_free (png_ptr, row_pointers[row]);
		}

		g_free (row_pointers);

		png_read_end (png_ptr, end_ptr);

		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nfo:Image");
		tracker_sparql_builder_object (metadata, "nmm:Photo");

		read_metadata (png_ptr, info_ptr, uri, metadata);
		read_metadata (png_ptr, end_ptr, uri, metadata);

		tracker_sparql_builder_predicate (metadata, "nfo:width");
		tracker_sparql_builder_object_int64 (metadata, width);

		tracker_sparql_builder_predicate (metadata, "nfo:height");
		tracker_sparql_builder_object_int64 (metadata, height);

		png_destroy_read_struct (&png_ptr, &info_ptr, &end_ptr);
		tracker_file_close (f, FALSE);
	}

	g_free (filename);
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
