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
	gchar *camera;
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
read_metadata (TrackerSparqlBuilder *preupdate,
               TrackerSparqlBuilder *metadata,
               png_structp           png_ptr,
               png_infop             info_ptr,
               const gchar          *uri)
{
	MergeData md = { 0 };
	PngData pd = { 0 };
	TrackerExifData *ed = NULL;
	TrackerXmpData *xd = NULL;
	png_textp text_ptr;
	gint num_text;
	gint i;
	gint found;
	GPtrArray *keywords;

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
			xd = tracker_xmp_new (text_ptr[i].text,
			                      text_ptr[i].itxt_length,
			                      uri);
			continue;
		}
#endif

#if defined(HAVE_LIBEXIF) && defined(PNG_iTXt_SUPPORTED)
		/* I'm not certain this is the key for EXIF. Using key according to
		 * this document about exiftool:
		 * http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/PNG.html#TextualData 
		 */
		if (g_strcmp0 ("Raw profile type exif", text_ptr[i].key) == 0) {
			ed = tracker_exif_new (text_ptr[i].text,
			                       text_ptr[i].itxt_length,
			                       uri);
			continue;
		}
#endif /* HAVE_LIBEXIF */

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
	}

	if (!ed) {
		ed = g_new0 (TrackerExifData, 1);
	}

	if (!xd) {
		xd = g_new0 (TrackerXmpData, 1);
	}

	/* Don't merge if the make is in the model */
	if ((xd->make == NULL || xd->model == NULL) ||
	    (xd->make && xd->model && strstr (xd->model, xd->make) == NULL)) {
		md.camera = tracker_merge_const (" ", 2, xd->make, xd->model);
	} else {
		md.camera = g_strdup (xd->model);
	}

	if (!md.camera) {
		if ((ed->make == NULL || ed->model == NULL) ||
		    (ed->make && ed->model && strstr (ed->model, ed->make) == NULL)) {
			md.camera = tracker_merge_const (" ", 2, ed->make, ed->model);
		} else {
			md.camera = g_strdup (ed->model);
		}
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

	keywords = g_ptr_array_new ();

	if (md.comment) {
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, md.comment);
	}

	if (md.license) {
		tracker_sparql_builder_predicate (metadata, "nie:license");
		tracker_sparql_builder_object_unvalidated (metadata, md.license);
	}

	/* TODO: add ontology and store this ed->software */
	g_free (ed->software);

	if (md.creator) {
		gchar *uri = tracker_sparql_escape_uri_printf ("urn:contact:%s", md.creator);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, md.creator);
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "nco:creator");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (md.date) {
		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, md.date);
	}

	if (md.description) {
		tracker_sparql_builder_predicate (metadata, "nie:description");
		tracker_sparql_builder_object_unvalidated (metadata, md.description);
	}

	if (md.copyright) {
		tracker_sparql_builder_predicate (metadata, "nie:copyright");
		tracker_sparql_builder_object_unvalidated (metadata, md.copyright);
	}

	if (md.title) {
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, md.title);
	}

	if (md.camera) {
		tracker_sparql_builder_predicate (metadata, "nfo:device");
		tracker_sparql_builder_object_unvalidated (metadata, md.camera);
	}

	if (md.artist) {
		gchar *uri = tracker_sparql_escape_uri_printf ("urn:contact:%s", md.artist);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, md.artist);
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "nco:contributor");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (md.orientation) {
		tracker_sparql_builder_predicate (metadata, "nfo:orientation");
		tracker_sparql_builder_object_unvalidated (metadata, md.orientation);
	}

	if (md.exposure_time) {
		tracker_sparql_builder_predicate (metadata, "nmm:exposureTime");
		tracker_sparql_builder_object_unvalidated (metadata, md.exposure_time);
	}

	if (md.iso_speed_ratings) {
		tracker_sparql_builder_predicate (metadata, "nmm:isoSpeed");
		tracker_sparql_builder_object_unvalidated (metadata, md.iso_speed_ratings);
	}

	if (md.white_balance) {
		tracker_sparql_builder_predicate (metadata, "nmm:whiteBalance");
		tracker_sparql_builder_object_unvalidated (metadata, md.white_balance);
	}

	if (md.fnumber) {
		tracker_sparql_builder_predicate (metadata, "nmm:fnumber");
		tracker_sparql_builder_object_unvalidated (metadata, md.fnumber);
	}

	if (md.flash) {
		tracker_sparql_builder_predicate (metadata, "nmm:flash");
		tracker_sparql_builder_object_unvalidated (metadata, md.flash);
	}

	if (md.focal_length) {
		tracker_sparql_builder_predicate (metadata, "nmm:focalLength");
		tracker_sparql_builder_object_unvalidated (metadata, md.focal_length);
	}

	if (md.metering_mode) {
		tracker_sparql_builder_predicate (metadata, "nmm:meteringMode");
		tracker_sparql_builder_object_unvalidated (metadata, md.metering_mode);
	}


	if (xd->keywords) {
		tracker_keywords_parse (keywords, xd->keywords);
	}

	if (xd->pdf_keywords) {
		tracker_keywords_parse (keywords, xd->pdf_keywords);
	}

	if (xd->rating) {
		tracker_sparql_builder_predicate (metadata, "nao:numericRating");
		tracker_sparql_builder_object_unvalidated (metadata, xd->rating);
	}

	if (xd->subject) {
		tracker_keywords_parse (keywords, xd->subject);
	}

	if (xd->publisher) {
		gchar *uri = tracker_sparql_escape_uri_printf ("urn:contact:%s", xd->publisher);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, xd->publisher);
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "nco:creator");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (xd->type) {
		tracker_sparql_builder_predicate (metadata, "dc:type");
		tracker_sparql_builder_object_unvalidated (metadata, xd->type);
	}

	if (xd->format) {
		tracker_sparql_builder_predicate (metadata, "dc:format");
		tracker_sparql_builder_object_unvalidated (metadata, xd->format);
	}

	if (xd->identifier) {
		tracker_sparql_builder_predicate (metadata, "dc:identifier");
		tracker_sparql_builder_object_unvalidated (metadata, xd->identifier);
	}

	if (xd->source) {
		tracker_sparql_builder_predicate (metadata, "dc:source");
		tracker_sparql_builder_object_unvalidated (metadata, xd->source);
	}

	if (xd->language) {
		tracker_sparql_builder_predicate (metadata, "dc:language");
		tracker_sparql_builder_object_unvalidated (metadata, xd->language);
	}

	if (xd->relation) {
		tracker_sparql_builder_predicate (metadata, "dc:relation");
		tracker_sparql_builder_object_unvalidated (metadata, xd->relation);
	}

	if (xd->coverage) {
		tracker_sparql_builder_predicate (metadata, "dc:coverage");
		tracker_sparql_builder_object_unvalidated (metadata, xd->coverage);
	}

	if (xd->address || xd->country || xd->city) {
		tracker_sparql_builder_predicate (metadata, "mlo:location");
	
		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "mlo:GeoPoint");

		if (xd->address) {
			tracker_sparql_builder_predicate (metadata, "mlo:address");
			tracker_sparql_builder_object_unvalidated (metadata, xd->address);
		}

		if (xd->state) {
			tracker_sparql_builder_predicate (metadata, "mlo:state");
			tracker_sparql_builder_object_unvalidated (metadata, xd->state);
		}

		if (xd->city) {
			tracker_sparql_builder_predicate (metadata, "mlo:city");
			tracker_sparql_builder_object_unvalidated (metadata, xd->city);
		}

		if (xd->country) {
			tracker_sparql_builder_predicate (metadata, "mlo:country");
			tracker_sparql_builder_object_unvalidated (metadata, xd->country);
		}

		tracker_sparql_builder_object_blank_close (metadata);
	}

	if (ed->x_resolution) {
		gdouble value;

		value = ed->resolution_unit == 1 ? g_strtod (ed->x_resolution, NULL) : g_strtod (ed->x_resolution, NULL) * CM_TO_INCH;
		tracker_sparql_builder_predicate (metadata, "nfo:horizontalResolution");
		tracker_sparql_builder_object_double (metadata, value);
	}

	if (ed->y_resolution) {
		gdouble value;

		value = ed->resolution_unit == 1 ? g_strtod (ed->y_resolution, NULL) : g_strtod (ed->y_resolution, NULL) * CM_TO_INCH;
		tracker_sparql_builder_predicate (metadata, "nfo:verticalResolution");
		tracker_sparql_builder_object_double (metadata, value);
	}

	for (i = 0; i < keywords->len; i++) {
		gchar *p;

		p = g_ptr_array_index (keywords, i);

		tracker_sparql_builder_predicate (metadata, "nao:hasTag");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nao:Tag");
		tracker_sparql_builder_predicate (metadata, "nao:prefLabel");
		tracker_sparql_builder_object_unvalidated (metadata, p);
		tracker_sparql_builder_object_blank_close (metadata);
		g_free (p);
	}
	g_ptr_array_free (keywords, TRUE);

	tracker_exif_free (ed);
	tracker_xmp_free (xd);
	g_free (pd.creation_time);
	g_free (md.camera);
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
