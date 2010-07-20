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

#include <glib/gstdio.h>

#include <tiffio.h>

#include <libtracker-client/tracker-client.h>
#include <libtracker-extract/tracker-extract.h>

#define CM_TO_INCH          0.393700787

typedef enum {
	TAG_TYPE_UNDEFINED = 0,
	TAG_TYPE_STRING,
	TAG_TYPE_UINT16,
	TAG_TYPE_UINT32,
	TAG_TYPE_DOUBLE,
	TAG_TYPE_C16_UINT16
} TagType;

typedef struct {
	gchar *camera;
	const gchar *title;
	const gchar *orientation;
	const gchar *copyright;
	const gchar *white_balance;
	const gchar *fnumber;
	const gchar *flash;
	const gchar *focal_length;
	const gchar *artist;
	const gchar *exposure_time;
	const gchar *iso_speed_ratings;
	const gchar *date;
	const gchar *description;
	const gchar *metering_mode;
	const gchar *creator;
	const gchar *x_dimension;
	const gchar *y_dimension;
	const gchar *city;
	const gchar *state;
	const gchar *address;
	const gchar *country;
} MergeData;

typedef struct {
	gchar *artist;
	gchar *copyright;
	gchar *date;
	gchar *title;
	gchar *description;
	gchar *width;
	gchar *length;
	gchar *make;
	gchar *model;
	gchar *orientation;
} TiffData;

static void extract_tiff (const gchar          *filename,
                          TrackerSparqlBuilder *preupdate,
                          TrackerSparqlBuilder *metadata);

static TrackerExtractData extract_data[] = {
	{ "image/tiff", extract_tiff },
	{ NULL, NULL }
};

static void
tiff_data_free (TiffData *tags)
{
	g_free (tags->artist);
	g_free (tags->copyright);
	g_free (tags->date);
	g_free (tags->title);
	g_free (tags->description);
	g_free (tags->width);
	g_free (tags->length);
	g_free (tags->make);
	g_free (tags->model);
	g_free (tags->orientation);
}

static gchar *
get_flash (TIFF *image)
{
	guint16 data = 0;

	if (TIFFGetField (image, EXIFTAG_FLASH, &data)) {
		switch (data) {
		case 0x0001:
		case 0x0009:
		case 0x000D:
		case 0x000F:
		case 0x0019:
		case 0x001D:
		case 0x001F:
		case 0x0041:
		case 0x0045:
		case 0x0047:
		case 0x0049:
		case 0x004D:
		case 0x004F:
		case 0x0059:
		case 0x005F:
		case 0x005D:
			return g_strdup ("nmm:flash-on");
		default:
			return g_strdup ("nmm:flash-off");
		}
	}

	return NULL;
}

static gchar *
get_orientation (TIFF *image)
{
	guint16 data = 0;

	if (TIFFGetField (image, TIFFTAG_ORIENTATION, &data)) {
		switch (data) {
		case 0: return g_strdup ("nfo:orientation-top");
		case 1:	return g_strdup ("nfo:orientation-top-mirror");
		case 2:	return g_strdup ("nfo:orientation-bottom");
		case 3:	return g_strdup ("nfo:orientation-bottom-mirror");
		case 4:	return g_strdup ("nfo:orientation-left-mirror");
		case 5:	return g_strdup ("nfo:orientation-right");
		case 6:	return g_strdup ("nfo:orientation-right-mirror");
		case 7:	return g_strdup ("nfo:orientation-left");
		}
	}

	return NULL;
}

static gchar *
get_metering_mode (TIFF *image)
{
	guint16 data = 0;

	if (TIFFGetField (image, EXIFTAG_METERINGMODE, &data)) {
		switch (data) {
		case 1: return g_strdup ("nmm:metering-mode-average");
		case 2: return g_strdup ("nmm:metering-mode-center-weighted-average");
		case 3: return g_strdup ("nmm:metering-mode-spot");
		case 4: return g_strdup ("nmm:metering-mode-multispot");
		case 5: return g_strdup ("nmm:metering-mode-pattern");
		case 6: return g_strdup ("nmm:metering-mode-partial");
		default: 
			return g_strdup ("nmm:metering-mode-other");
		}
	}

	return NULL;
}

static gchar *
get_white_balance (TIFF *image)
{
	guint16 data = 0;

	if (TIFFGetField (image, EXIFTAG_WHITEBALANCE, &data)) {
		if (data == 0) {
			return g_strdup ("nmm:white-balance-auto");
		} else {
			return g_strdup ("nmm:white-balance-manual");
		}
	}

	return NULL;
}

static gchar *
tag_to_string (TIFF    *image, 
               guint    tag,
               TagType  type)
{
	switch (type) {
	case TAG_TYPE_STRING: {
		gchar *data = NULL;

		if (TIFFGetField (image, tag, &data)) {
			return g_strdup (data);
		}
		break;
	}

	case TAG_TYPE_UINT16: {
		guint16 data = 0;

		if (TIFFGetField (image, tag, &data)) {
			return g_strdup_printf ("%i", data);
		}
		break;
	}

	case TAG_TYPE_UINT32: {
		guint32 data = 0;

		if (TIFFGetField (image, tag, &data)) {
			return g_strdup_printf ("%i", data);
		}
		break;
	}

	case TAG_TYPE_DOUBLE: {
		gfloat data = 0;

		if (TIFFGetField (image, tag, &data)) {
			return g_strdup_printf ("%f", data);
		}
		break;
	}

	case TAG_TYPE_C16_UINT16: {
		void *data = NULL;
		guint16 count = 0;

		if (TIFFGetField (image, tag, &count, &data)) {
			return g_strdup_printf ("%i", * (guint16*) data);
		}
		break;
	}

	default:
		break;
	}

	return NULL;
}

static void
extract_tiff (const gchar          *uri,
              TrackerSparqlBuilder *preupdate,
              TrackerSparqlBuilder *metadata)
{
	TIFF *image;
	TrackerXmpData *xd = NULL;
	TrackerIptcData *id = NULL;
	TrackerExifData *ed = NULL;
	MergeData md = { 0 };
	TiffData td = { 0 };
	gchar *filename;
	gchar *date;
	glong exif_offset;
	GPtrArray *keywords;
	guint i;

#ifdef HAVE_LIBIPTCDATA
	gchar *iptc_offset;
	guint32 iptc_size;
#endif

#ifdef HAVE_EXEMPI
	gchar *xmp_offset;
	guint32 size;
#endif /* HAVE_EXEMPI */

	filename = g_filename_from_uri (uri, NULL, NULL);

	if ((image = TIFFOpen (filename, "r")) == NULL){
		g_warning ("Could not open image:'%s'\n", filename);
		g_free (filename);
		return;
	}

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:Image");
	tracker_sparql_builder_object (metadata, "nmm:Photo");

#ifdef HAVE_LIBIPTCDATA
	if (TIFFGetField (image, 
	                  TIFFTAG_RICHTIFFIPTC, 
	                  &iptc_size, 
	                  &iptc_offset)) {
		if (TIFFIsByteSwapped(image) != 0) {
			TIFFSwabArrayOfLong((uint32*) iptc_offset, 
			                    (unsigned long) iptc_size);
		}
		id = tracker_iptc_new (iptc_offset, 4 * iptc_size, uri);
	}
#endif /* HAVE_LIBIPTCDATA */

	if (!id) {
		id = g_new0 (TrackerIptcData, 1);
	}

	/* FIXME There are problems between XMP data embedded with different tools
	   due to bugs in the original spec (type) */
#ifdef HAVE_EXEMPI
	if (TIFFGetField (image, TIFFTAG_XMLPACKET, &size, &xmp_offset)) {
		xd = tracker_xmp_new (xmp_offset, size, uri);
	}
#endif /* HAVE_EXEMPI */

	if (!xd) {
		xd = g_new0 (TrackerXmpData, 1);
	}

	ed = g_new0 (TrackerExifData, 1);

	/* Get Tiff specifics */
	td.width = tag_to_string (image, TIFFTAG_IMAGEWIDTH, TAG_TYPE_UINT32);
	td.length = tag_to_string (image, TIFFTAG_IMAGELENGTH, TAG_TYPE_UINT32);
	td.artist = tag_to_string (image, TIFFTAG_ARTIST, TAG_TYPE_STRING);
	td.copyright = tag_to_string (image, TIFFTAG_COPYRIGHT, TAG_TYPE_STRING);

	date = tag_to_string (image, TIFFTAG_DATETIME, TAG_TYPE_STRING);
	td.date = tracker_date_guess (date);
	g_free (date);

	td.title = tag_to_string (image, TIFFTAG_DOCUMENTNAME, TAG_TYPE_STRING);
	td.description = tag_to_string (image, TIFFTAG_IMAGEDESCRIPTION, TAG_TYPE_STRING);
	td.make = tag_to_string (image, TIFFTAG_MAKE, TAG_TYPE_STRING);
	td.model = tag_to_string (image, TIFFTAG_MODEL, TAG_TYPE_STRING);
	td.orientation = get_orientation (image);

	/* Get Exif specifics */
	if (TIFFGetField (image, TIFFTAG_EXIFIFD, &exif_offset)) {
		if (TIFFReadEXIFDirectory (image, exif_offset)) {
			ed->exposure_time = tag_to_string (image, EXIFTAG_EXPOSURETIME, TAG_TYPE_DOUBLE);
			ed->fnumber = tag_to_string (image, EXIFTAG_FNUMBER, TAG_TYPE_DOUBLE);
			ed->iso_speed_ratings = tag_to_string (image, EXIFTAG_ISOSPEEDRATINGS, TAG_TYPE_C16_UINT16);
			date = tag_to_string (image, EXIFTAG_DATETIMEORIGINAL, TAG_TYPE_STRING);
			ed->time_original = tracker_date_guess (date);
			g_free (date);

			ed->metering_mode = get_metering_mode (image);
			ed->flash = get_flash (image);
			ed->focal_length = tag_to_string (image, EXIFTAG_DATETIMEORIGINAL, TAG_TYPE_DOUBLE);
			ed->white_balance = get_white_balance (image);
			/* ed->software = tag_to_string (image, EXIFTAG_SOFTWARE, TAG_TYPE_STRING); */
		}
	}

	TIFFClose (image);
	g_free (filename);

	md.camera = tracker_merge_const (" ", 2, xd->make, xd->model);

	if (!md.camera) {
		md.camera = tracker_merge_const (" ", 2, td.make, td.model);

		if (!md.camera) {
			md.camera = tracker_merge_const (" ", 2, ed->make, ed->model);
		}
	}

	md.title = tracker_coalesce_strip (5, xd->title, xd->pdf_title, td.title, ed->document_name, xd->title2);
	md.orientation = tracker_coalesce_strip (4, xd->orientation, td.orientation, ed->orientation, id->image_orientation);
	md.copyright = tracker_coalesce_strip (4, xd->rights, td.copyright, ed->copyright, id->copyright_notice);
	md.white_balance = tracker_coalesce_strip (2, xd->white_balance, ed->white_balance);
	md.fnumber = tracker_coalesce_strip (2, xd->fnumber, ed->fnumber);
	md.flash = tracker_coalesce_strip (2, xd->flash, ed->flash);
	md.focal_length = tracker_coalesce_strip (2, xd->focal_length, ed->focal_length);
	md.artist = tracker_coalesce_strip (4, xd->artist, td.artist, ed->artist, xd->contributor);
	md.exposure_time = tracker_coalesce_strip (2, xd->exposure_time, ed->exposure_time);
	md.iso_speed_ratings = tracker_coalesce_strip (2, xd->iso_speed_ratings, ed->iso_speed_ratings);
	md.date = tracker_coalesce_strip (6, xd->date, xd->time_original, td.date, ed->time, id->date_created, ed->time_original);
	md.description = tracker_coalesce_strip (3, xd->description, td.description, ed->description);
	md.metering_mode = tracker_coalesce_strip (2, xd->metering_mode, ed->metering_mode);
	md.city = tracker_coalesce_strip (2, xd->city, id->city);
	md.state = tracker_coalesce_strip (2, xd->state, id->state);
	md.address = tracker_coalesce_strip (2, xd->address, id->sublocation);
	md.country = tracker_coalesce_strip (2, xd->country, id->country_name);
	md.creator = tracker_coalesce_strip (3, xd->creator, id->byline, id->credit);
	md.x_dimension = tracker_coalesce_strip (2, td.width, ed->x_dimension);
	md.y_dimension = tracker_coalesce_strip (2, td.length, ed->y_dimension);

	keywords = g_ptr_array_new ();

	if (ed->user_comment) {
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, ed->user_comment);
	}

	if (md.x_dimension) {
		tracker_sparql_builder_predicate (metadata, "nfo:width");
		tracker_sparql_builder_object_unvalidated (metadata, md.x_dimension);
	}

	if (md.y_dimension) {
		tracker_sparql_builder_predicate (metadata, "nfo:height");
		tracker_sparql_builder_object_unvalidated (metadata, md.y_dimension);
	}

	if (xd->keywords) {
		tracker_keywords_parse (keywords, xd->keywords);
	}

	if (xd->pdf_keywords) {
		tracker_keywords_parse (keywords, xd->pdf_keywords);
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

		tracker_sparql_builder_predicate (metadata, "nco:publisher");
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

	if (xd->rating) {
		tracker_sparql_builder_predicate (metadata, "nao:numericRating");
		tracker_sparql_builder_object_unvalidated (metadata, xd->rating);
	}

	if (xd->license) {
		tracker_sparql_builder_predicate (metadata, "nie:license");
		tracker_sparql_builder_object_unvalidated (metadata, xd->license);
	}

	if (md.city || md.state || md.address || md.country) {
		tracker_sparql_builder_predicate (metadata, "mlo:location");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "mlo:GeoPoint");

		if (md.address) {
			tracker_sparql_builder_predicate (metadata, "mlo:address");
			tracker_sparql_builder_object_unvalidated (metadata, md.address);
		}

		if (md.state) {
			tracker_sparql_builder_predicate (metadata, "mlo:state");
			tracker_sparql_builder_object_unvalidated (metadata, md.state);
		}

		if (md.city) {
			tracker_sparql_builder_predicate (metadata, "mlo:city");
			tracker_sparql_builder_object_unvalidated (metadata, md.city);
		}

		if (md.country) {
			tracker_sparql_builder_predicate (metadata, "mlo:country");
			tracker_sparql_builder_object_unvalidated (metadata, md.country);
		}

		tracker_sparql_builder_object_blank_close (metadata);
	}

	if (id->contact) {
		gchar *uri = tracker_sparql_escape_uri_printf ("urn:contact:%s", id->contact);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, id->contact);
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "nco:representative");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (id->keywords) {
		tracker_keywords_parse (keywords, id->keywords);
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

	if (md.camera) {
		tracker_sparql_builder_predicate (metadata, "nfo:device");
		tracker_sparql_builder_object_unvalidated (metadata, md.camera);
	}

	if (md.title) {
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, md.title);
	}

	if (md.orientation) {
		tracker_sparql_builder_predicate (metadata, "nfo:orientation");
		tracker_sparql_builder_object_unvalidated (metadata, md.orientation);
	}

	if (md.copyright) {
		tracker_sparql_builder_predicate (metadata, "nie:copyright");
		tracker_sparql_builder_object_unvalidated (metadata, md.copyright);
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

	if (md.exposure_time) {
		tracker_sparql_builder_predicate (metadata, "nmm:exposureTime");
		tracker_sparql_builder_object_unvalidated (metadata, md.exposure_time);
	}

	if (md.iso_speed_ratings) {
		tracker_sparql_builder_predicate (metadata, "nmm:isoSpeed");
		tracker_sparql_builder_object_unvalidated (metadata, md.iso_speed_ratings);
	}

	if (md.date) {
		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, md.date);
	}

	if (md.description) {
		tracker_sparql_builder_predicate (metadata, "nie:description");
		tracker_sparql_builder_object_unvalidated (metadata, md.description);
	}

	if (md.metering_mode) {
		tracker_sparql_builder_predicate (metadata, "nmm:meteringMode");
		tracker_sparql_builder_object_unvalidated (metadata, md.metering_mode);
	}

	if (md.creator) {
		gchar *uri = tracker_sparql_escape_uri_printf ("urn:contact:%s", md.creator);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, md.creator);
		tracker_sparql_builder_insert_close (preupdate);

		/* NOTE: We only have affiliation with
		 * nco:PersonContact and we are using
		 * nco:Contact here.
		 */

		/* if (id->byline_title) { */
		/* 	tracker_sparql_builder_insert_open (preupdate, NULL); */

		/* 	tracker_sparql_builder_subject (preupdate, "_:affiliation_by_line"); */
		/* 	tracker_sparql_builder_predicate (preupdate, "a"); */
		/* 	tracker_sparql_builder_object (preupdate, "nco:Affiliation"); */

		/* 	tracker_sparql_builder_predicate (preupdate, "nco:title"); */
		/* 	tracker_sparql_builder_object_unvalidated (preupdate, id->byline_title); */

		/* 	tracker_sparql_builder_insert_close (preupdate); */

		/* 	tracker_sparql_builder_predicate (metadata, "a"); */
		/* 	tracker_sparql_builder_object (metadata, "nco:PersonContact"); */
		/* 	tracker_sparql_builder_predicate (metadata, "nco:hasAffiliation"); */
		/* 	tracker_sparql_builder_object (metadata, "_:affiliation_by_line"); */
		/* } */

		tracker_sparql_builder_predicate (metadata, "nco:creator");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
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

	g_free (md.camera);
	tiff_data_free (&td);
	tracker_exif_free (ed);
	tracker_xmp_free (xd);
	tracker_iptc_free (id);
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return extract_data;
}
