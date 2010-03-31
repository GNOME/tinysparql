/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <png.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-client/tracker-client.h>
#include <libtracker-extract/tracker-extract.h>

#define RFC1123_DATE_FORMAT "%d %B %Y %H:%M:%S %z"

typedef struct {
	gchar *title;
	gchar *copyright;
	gchar *creator;
	gchar *description;
	gchar *date;
	gchar *license;
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
} MergeData;

typedef struct {
	gchar *author;
	gchar *creator;
	gchar *description;
	gchar *comment;
	gchar *copyright;
	gchar *creation_time;
	gchar *title;
	gchar *disclaimer;
} PngData;

static void extract_png (const gchar          *filename,
                         TrackerSparqlBuilder *preupdate,
                         TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "image/png", extract_png },
	{ "sketch/png", extract_png },
	{ NULL, NULL }
};

static gchar *
rfc1123_to_iso8601_date (const gchar *date)
{
	/* From: ex. RFC1123 date: "22 May 1997 18:07:10 -0600"
	 * To  : ex. ISO8601 date: "2007-05-22T18:07:10-0600"
	 */
	return tracker_date_format_to_iso8601 (date, RFC1123_DATE_FORMAT);
}

static void
insert_keywords (TrackerSparqlBuilder *metadata, 
                 const gchar          *uri, 
                 gchar                *keywords)
{
	char *lasts, *keyw;
	size_t len;

	keyw = keywords;
	keywords = strchr (keywords, '"');
	if (keywords) {
		keywords++;
	} else {
		keywords = keyw;
	}

	len = strlen (keywords);
	if (keywords[len - 1] == '"') {
		keywords[len - 1] = '\0';
	}

	for (keyw = strtok_r (keywords, ",; ", &lasts); 
	     keyw;
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
read_metadata (TrackerSparqlBuilder *preupdate,
               TrackerSparqlBuilder *metadata,
               png_structp           png_ptr,
               png_infop             info_ptr,
               const gchar          *uri)
{
	MergeData md = { 0 };
	PngData pd = { 0 };
	TrackerExifData ed = { 0 };
	TrackerXmpData xd = { 0 };
	png_textp text_ptr;
	gint num_text;
	gint i;
	gint found;

	if ((found = png_get_text (png_ptr, info_ptr, &text_ptr, &num_text)) < 1) {
		g_debug ("Calling png_get_text() returned %d (< 1)", found);
		return;
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
			tracker_extract_xmp_read (text_ptr[i].text,
			                          text_ptr[i].itxt_length,
			                          uri, 
			                          &xd);

			continue;
		}
#endif

#if defined(HAVE_LIBEXIF) && defined(PNG_iTXt_SUPPORTED)
		/* I'm not certain this is the key for EXIF. Using key according to
		 * this document about exiftool:
		 * http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/PNG.html#TextualData 
		 */
		if (g_strcmp0 ("Raw profile type exif", text_ptr[i].key) == 0) {
			tracker_extract_exif_read (text_ptr[i].text,
			                           text_ptr[i].itxt_length, 
			                           uri, 
			                           &ed);
			continue;
		}
#endif /* HAVE_LIBEXIF */

		if (g_strcmp0 (text_ptr[i].key, "Author") == 0) {
			pd.author = g_strdup (text_ptr[i].text);
			continue;
		}

		if (g_strcmp0 (text_ptr[i].key, "Creator") == 0) {
			pd.creator = g_strdup (text_ptr[i].text);
			continue;
		}

		if (g_strcmp0 (text_ptr[i].key, "Description") == 0) {
			pd.description = g_strdup (text_ptr[i].text);
			continue;
		}

		if (g_strcmp0 (text_ptr[i].key, "Comment") == 0) {
			pd.comment = g_strdup (text_ptr[i].text);
			continue;
		}

		if (g_strcmp0 (text_ptr[i].key, "Copyright") == 0) {
			pd.copyright = g_strdup (text_ptr[i].text);
			continue;
		}

		if (g_strcmp0 (text_ptr[i].key, "Creation Time") == 0) {
			pd.creation_time = rfc1123_to_iso8601_date (text_ptr[i].text);
			continue;
		}

		if (g_strcmp0 (text_ptr[i].key, "Title") == 0) {
			pd.title = g_strdup (text_ptr[i].text);
			continue;
		}

		if (g_strcmp0 (text_ptr[i].key, "Disclaimer") == 0) {
			pd.disclaimer = g_strdup (text_ptr[i].text);
			continue;
		}
	}

	/* Don't merge if the make is in the model */
	if ((xd.make == NULL || xd.model == NULL) ||
	    (xd.make && xd.model && strstr (xd.model, xd.make) == NULL)) {
		md.camera = tracker_merge (" ", 2, xd.make, xd.model);
	} else {
		md.camera = g_strdup (xd.model);
		g_free (xd.model);
		g_free (xd.make);
	}

	if (!md.camera) {
		if ((ed.make == NULL || ed.model == NULL) ||
		    (ed.make && ed.model && strstr (ed.model, ed.make) == NULL)) {
			md.camera = tracker_merge (" ", 2, ed.make, ed.model);
		} else {
			md.camera = g_strdup (ed.model);
			g_free (ed.model);
			g_free (ed.make);
		}
	} else {
		g_free (ed.model);
		g_free (ed.make);
	}

	md.creator = tracker_coalesce (3, xd.creator, pd.creator, pd.author);
	md.title = tracker_coalesce (5, xd.title, pd.title, ed.document_name, xd.title2, xd.pdf_title);
	md.copyright = tracker_coalesce (3, xd.rights, pd.copyright, ed.copyright);
	md.license = tracker_coalesce (2, xd.license, pd.disclaimer);
	md.description = tracker_coalesce (3, xd.description, pd.description, ed.description);
	md.date = tracker_coalesce (5, xd.date, xd.time_original, pd.creation_time, ed.time, ed.time_original);
	md.comment = tracker_coalesce (2, pd.comment, ed.user_comment);
	md.artist = tracker_coalesce (3, xd.artist, ed.artist, xd.contributor);
	md.orientation = tracker_coalesce (2, xd.orientation, ed.orientation);
	md.exposure_time = tracker_coalesce (2, xd.exposure_time, ed.exposure_time);
	md.iso_speed_ratings = tracker_coalesce (2, xd.iso_speed_ratings, ed.iso_speed_ratings);
	md.fnumber = tracker_coalesce (2, xd.fnumber, ed.fnumber);
	md.flash = tracker_coalesce (2, xd.flash, ed.flash);
	md.focal_length = tracker_coalesce (2, xd.focal_length, ed.focal_length);
	md.metering_mode = tracker_coalesce (2, xd.metering_mode, ed.metering_mode);
	md.white_balance = tracker_coalesce (2, xd.white_balance, ed.white_balance);

	if (md.comment) {
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, md.comment);
		g_free (md.comment);
	}

	if (md.license) {
		tracker_sparql_builder_predicate (metadata, "nie:license");
		tracker_sparql_builder_object_unvalidated (metadata, md.license);
		g_free (md.license);
	}

	/* TODO: add ontology and store this */
	g_free (ed.software);

	g_free (ed.x_dimension);
	g_free (ed.y_dimension);
	g_free (ed.image_width);

	if (md.creator) {
		gchar *uri = tracker_uri_printf_escaped ("urn:artist:%s", md.creator);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, md.creator);
		tracker_sparql_builder_insert_close (preupdate);
		g_free (md.creator);

		tracker_sparql_builder_predicate (metadata, "nco:creator");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (md.date) {
		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, md.date);
		g_free (md.date);
	}

	if (md.description) {
		tracker_sparql_builder_predicate (metadata, "nie:description");
		tracker_sparql_builder_object_unvalidated (metadata, md.description);
		g_free (md.description);
	}

	if (md.copyright) {
		tracker_sparql_builder_predicate (metadata, "nie:copyright");
		tracker_sparql_builder_object_unvalidated (metadata, md.copyright);
		g_free (md.copyright);
	}

	if (md.title) {
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, md.title);
		g_free (md.title);
	}

	if (md.camera) {
		tracker_sparql_builder_predicate (metadata, "nmm:camera");
		tracker_sparql_builder_object_unvalidated (metadata, md.camera);
		g_free (md.camera);
	}

	if (md.artist) {
		gchar *uri = tracker_uri_printf_escaped ("urn:artist:%s", md.artist);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, md.artist);
		tracker_sparql_builder_insert_close (preupdate);
		g_free (md.artist);

		tracker_sparql_builder_predicate (metadata, "nco:contributor");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (md.orientation) {
		tracker_sparql_builder_predicate (metadata, "nfo:orientation");
		tracker_sparql_builder_object_unvalidated (metadata, md.orientation);
		g_free (md.orientation);
	}

	if (md.exposure_time) {
		tracker_sparql_builder_predicate (metadata, "nmm:exposureTime");
		tracker_sparql_builder_object_unvalidated (metadata, md.exposure_time);
		g_free (md.exposure_time);
	}

	if (md.iso_speed_ratings) {
		tracker_sparql_builder_predicate (metadata, "nmm:isoSpeed");
		tracker_sparql_builder_object_unvalidated (metadata, md.iso_speed_ratings);
		g_free (md.iso_speed_ratings);
	}

	if (md.white_balance) {
		tracker_sparql_builder_predicate (metadata, "nmm:whiteBalance");
		tracker_sparql_builder_object_unvalidated (metadata, md.white_balance);
		g_free (md.white_balance);
	}

	if (md.fnumber) {
		tracker_sparql_builder_predicate (metadata, "nmm:fnumber");
		tracker_sparql_builder_object_unvalidated (metadata, md.fnumber);
		g_free (md.fnumber);
	}

	if (md.flash) {
		tracker_sparql_builder_predicate (metadata, "nmm:flash");
		tracker_sparql_builder_object_unvalidated (metadata, md.flash);
		g_free (md.flash);
	}

	if (md.focal_length) {
		tracker_sparql_builder_predicate (metadata, "nmm:focalLength");
		tracker_sparql_builder_object_unvalidated (metadata, md.focal_length);
		g_free (md.focal_length);
	}

	if (md.metering_mode) {
		tracker_sparql_builder_predicate (metadata, "nmm:meteringMode");
		tracker_sparql_builder_object_unvalidated (metadata, md.metering_mode);
		g_free (md.metering_mode);
	}


	if (xd.keywords) {
		insert_keywords (metadata, uri, xd.keywords);
		g_free (xd.keywords);
	}

	if (xd.pdf_keywords) {
		insert_keywords (metadata, uri, xd.pdf_keywords);
		g_free (xd.pdf_keywords);
	}

	if (xd.rating) {
		tracker_sparql_builder_predicate (metadata, "nao:numericRating");
		tracker_sparql_builder_object_unvalidated (metadata, xd.rating);
		g_free (xd.rating);
	}

	if (xd.subject) {
		insert_keywords (metadata, uri, xd.subject);
		g_free (xd.subject);
	}

	if (xd.publisher) {
		gchar *uri = tracker_uri_printf_escaped ("urn:artist:%s", xd.publisher);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, xd.publisher);
		tracker_sparql_builder_insert_close (preupdate);
		g_free (xd.publisher);

		tracker_sparql_builder_predicate (metadata, "nco:creator");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (xd.type) {
		tracker_sparql_builder_predicate (metadata, "dc:type");
		tracker_sparql_builder_object_unvalidated (metadata, xd.type);
		g_free (xd.type);
	}

	if (xd.format) {
		tracker_sparql_builder_predicate (metadata, "dc:format");
		tracker_sparql_builder_object_unvalidated (metadata, xd.format);
		g_free (xd.format);
	}

	if (xd.identifier) {
		tracker_sparql_builder_predicate (metadata, "dc:identifier");
		tracker_sparql_builder_object_unvalidated (metadata, xd.identifier);
		g_free (xd.identifier);
	}

	if (xd.source) {
		tracker_sparql_builder_predicate (metadata, "dc:source");
		tracker_sparql_builder_object_unvalidated (metadata, xd.source);
		g_free (xd.source);
	}

	if (xd.language) {
		tracker_sparql_builder_predicate (metadata, "dc:language");
		tracker_sparql_builder_object_unvalidated (metadata, xd.language);
		g_free (xd.language);
	}

	if (xd.relation) {
		tracker_sparql_builder_predicate (metadata, "dc:relation");
		tracker_sparql_builder_object_unvalidated (metadata, xd.relation);
		g_free (xd.relation);
	}

	if (xd.coverage) {
		tracker_sparql_builder_predicate (metadata, "dc:coverage");
		tracker_sparql_builder_object_unvalidated (metadata, xd.coverage);
		g_free (xd.coverage);
	}

	if (xd.address || xd.country || xd.city) {
		tracker_sparql_builder_predicate (metadata, "mlo:location");
	
		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "mlo:GeoPoint");
	
		if (xd.address) {
			tracker_sparql_builder_predicate (metadata, "mlo:address");
			tracker_sparql_builder_object_unvalidated (metadata, xd.address);
			g_free (xd.address);
		}
	
		if (xd.state) {
			tracker_sparql_builder_predicate (metadata, "mlo:state");
			tracker_sparql_builder_object_unvalidated (metadata, xd.state);
			g_free (xd.state);
		}
	
		if (xd.city) {
			tracker_sparql_builder_predicate (metadata, "mlo:city");
			tracker_sparql_builder_object_unvalidated (metadata, xd.city);
			g_free (xd.city);
		}
	
		if (xd.country) {
			tracker_sparql_builder_predicate (metadata, "mlo:country");
			tracker_sparql_builder_object_unvalidated (metadata, xd.country);
			g_free (xd.country);
		}
		
		tracker_sparql_builder_object_blank_close (metadata);
	}
}

static void
extract_png (const gchar          *uri,
             TrackerSparqlBuilder *preupdate,
             TrackerSparqlBuilder *metadata)
{
	goffset size;
	FILE *f;
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_ptr;
	png_bytepp row_pointers;
	guint row;
	png_uint_32 width, height;
	gint bit_depth, color_type;
	gint interlace_type, compression_type, filter_type;
	gchar *filename;

	filename = g_filename_from_uri (uri, NULL, NULL);
	size = tracker_file_get_size (filename);

	if (size < 64) {
		return;
	}

	f = tracker_file_open (filename, "r", FALSE);
	g_free (filename);

	if (!f) {
		return;
	}

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
	                                  NULL,
	                                  NULL,
	                                  NULL);
	if (!png_ptr) {
		tracker_file_close (f, FALSE);
		return;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		tracker_file_close (f, FALSE);
		return;
	}

	end_ptr = png_create_info_struct (png_ptr);
	if (!end_ptr) {
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		tracker_file_close (f, FALSE);
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

	read_metadata (preupdate, metadata, png_ptr, info_ptr, uri);
	read_metadata (preupdate, metadata, png_ptr, end_ptr, uri);

	tracker_sparql_builder_predicate (metadata, "nfo:width");
	tracker_sparql_builder_object_int64 (metadata, width);

	tracker_sparql_builder_predicate (metadata, "nfo:height");
	tracker_sparql_builder_object_int64 (metadata, height);

	png_destroy_read_struct (&png_ptr, &info_ptr, &end_ptr);
	tracker_file_close (f, FALSE);
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
