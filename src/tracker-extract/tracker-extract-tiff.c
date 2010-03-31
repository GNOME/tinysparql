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
	gchar *title;
	gchar *orientation;
	gchar *copyright;
	gchar *white_balance;
	gchar *fnumber;
	gchar *flash;
	gchar *focal_length;
	gchar *artist;
	gchar *exposure_time;
	gchar *iso_speed_ratings;
	gchar *date;
	gchar *description;
	gchar *metering_mode;
	gchar *creator;
	gchar *x_dimension;
	gchar *y_dimension;
	gchar *city;
	gchar *state;
	gchar *address;
	gchar *country;
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
	if (keywords[len - 1] == '"')
		keywords[len - 1] = '\0';

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
extract_tiff (const gchar          *uri,
              TrackerSparqlBuilder *preupdate,
	      TrackerSparqlBuilder *metadata)
{
	TIFF *image;
	TrackerXmpData xd = { 0 };
	TrackerIptcData id = { 0 };
	TrackerExifData ed = { 0 };
	MergeData md = { 0 };
	TiffData td = { 0 };
	gchar *filename;
	gchar *date;
	glong exif_offset;

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
		tracker_iptc_read (iptc_offset, 4 * iptc_size, uri, &id);
	}
#endif /* HAVE_LIBIPTCDATA */

	/* FIXME There are problems between XMP data embedded with different tools
	   due to bugs in the original spec (type) */
#ifdef HAVE_EXEMPI
	if (TIFFGetField (image, TIFFTAG_XMLPACKET, &size, &xmp_offset)) {
		tracker_xmp_read (xmp_offset, size, uri, &xd);
	}
#endif /* HAVE_EXEMPI */

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
			ed.exposure_time = tag_to_string (image, EXIFTAG_EXPOSURETIME, TAG_TYPE_DOUBLE);
			ed.fnumber = tag_to_string (image, EXIFTAG_FNUMBER, TAG_TYPE_DOUBLE);
			ed.iso_speed_ratings = tag_to_string (image, EXIFTAG_ISOSPEEDRATINGS, TAG_TYPE_C16_UINT16);
			date = tag_to_string (image, EXIFTAG_DATETIMEORIGINAL, TAG_TYPE_STRING);
			ed.time_original = tracker_date_guess (date);
			g_free (date);

			ed.metering_mode = get_metering_mode (image);
			ed.flash = get_flash (image);
			ed.focal_length = tag_to_string (image, EXIFTAG_DATETIMEORIGINAL, TAG_TYPE_DOUBLE);
			ed.white_balance = get_white_balance (image);
			/* ed.software = tag_to_string (image, EXIFTAG_SOFTWARE, TAG_TYPE_STRING); */
		}
	}

	TIFFClose (image);
	g_free (filename);

	md.camera = tracker_merge (" ", 2, xd.make, xd.model);

	if (!md.camera) {
		md.camera = tracker_merge (" ", 2, td.make, td.model);

		if (!md.camera) {
			md.camera = tracker_merge (" ", 2, ed.make, ed.model);
		} else {
			g_free (ed.model);
			g_free (ed.make);
		}
	} else {
		g_free (td.model);
		g_free (td.make);
		g_free (ed.model);
		g_free (ed.make);
	}

	md.title = tracker_coalesce (5, xd.title, xd.pdf_title, td.title, ed.document_name, xd.title2);
	md.orientation = tracker_coalesce (4, xd.orientation, td.orientation, ed.orientation, id.image_orientation);
	md.copyright = tracker_coalesce (4, xd.rights, td.copyright, ed.copyright, id.copyright_notice);
	md.white_balance = tracker_coalesce (2, xd.white_balance, ed.white_balance);
	md.fnumber = tracker_coalesce (2, xd.fnumber, ed.fnumber);
	md.flash = tracker_coalesce (2, xd.flash, ed.flash);
	md.focal_length = tracker_coalesce (2, xd.focal_length, ed.focal_length);
	md.artist = tracker_coalesce (4, xd.artist, td.artist, ed.artist, xd.contributor);
	md.exposure_time = tracker_coalesce (2, xd.exposure_time, ed.exposure_time);
	md.iso_speed_ratings = tracker_coalesce (2, xd.iso_speed_ratings, ed.iso_speed_ratings);
	md.date = tracker_coalesce (6, xd.date, xd.time_original, td.date, ed.time, id.date_created, ed.time_original);
	md.description = tracker_coalesce (3, xd.description, td.description, ed.description);
	md.metering_mode = tracker_coalesce (2, xd.metering_mode, ed.metering_mode);
	md.city = tracker_coalesce (2, xd.city, id.city);
	md.state = tracker_coalesce (2, xd.state, id.state);
	md.address = tracker_coalesce (2, xd.address, id.sublocation);
	md.country = tracker_coalesce (2, xd.country, id.country_name);
	md.creator = tracker_coalesce (3, xd.creator, id.byline, id.credit);
	md.x_dimension = tracker_coalesce (2, td.width, ed.x_dimension);
	md.y_dimension = tracker_coalesce (2, td.length, ed.y_dimension);

	if (ed.user_comment) {
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, ed.user_comment);
		g_free (ed.user_comment);
	}

	if (md.x_dimension) {
		tracker_sparql_builder_predicate (metadata, "nfo:width");
		tracker_sparql_builder_object_unvalidated (metadata, md.x_dimension);
		g_free (md.x_dimension);
	}

	if (md.y_dimension) {
		tracker_sparql_builder_predicate (metadata, "nfo:height");
		tracker_sparql_builder_object_unvalidated (metadata, md.y_dimension);
		g_free (md.y_dimension);
	}

	if (xd.keywords) {
		insert_keywords (metadata, uri, xd.keywords);
		g_free (xd.keywords);
	}

	if (xd.pdf_keywords) {
		insert_keywords (metadata, uri, xd.pdf_keywords);
		g_free (xd.pdf_keywords);
	}

	if (xd.subject) {
		insert_keywords (metadata, uri, xd.subject);
		g_free (xd.subject);
	}

	if (xd.publisher) {
		gchar *uri = tracker_uri_printf_escaped ("urn:contact:%s", xd.publisher);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, xd.publisher);
		tracker_sparql_builder_insert_close (preupdate);
		g_free (xd.publisher);

		tracker_sparql_builder_predicate (metadata, "nco:publisher");
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

	if (xd.rating) {
		tracker_sparql_builder_predicate (metadata, "nao:numericRating");
		tracker_sparql_builder_object_unvalidated (metadata, xd.rating);
		g_free (xd.rating);
	}

	if (xd.license) {
		tracker_sparql_builder_predicate (metadata, "nie:license");
		tracker_sparql_builder_object_unvalidated (metadata, xd.license);
		g_free (xd.license);
	}

	if (md.city || md.state || md.address || md.country) {
		tracker_sparql_builder_predicate (metadata, "mlo:location");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "mlo:GeoPoint");
	
		if (md.address) {
			tracker_sparql_builder_predicate (metadata, "mlo:address");
			tracker_sparql_builder_object_unvalidated (metadata, md.address);
			g_free (md.address);
		}
	
		if (md.state) {
			tracker_sparql_builder_predicate (metadata, "mlo:state");
			tracker_sparql_builder_object_unvalidated (metadata, md.state);
			g_free (md.state);
		}
	
		if (md.city) {
			tracker_sparql_builder_predicate (metadata, "mlo:city");
			tracker_sparql_builder_object_unvalidated (metadata, md.city);
			g_free (md.city);
		}
	
		if (md.country) {
			tracker_sparql_builder_predicate (metadata, "mlo:country");
			tracker_sparql_builder_object_unvalidated (metadata, md.country);
			g_free (md.country);
		}
		
		tracker_sparql_builder_object_blank_close (metadata);
	}

	if (id.contact) {
		gchar *uri = tracker_uri_printf_escaped ("urn:contact:%s", id.contact);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, id.contact);
		tracker_sparql_builder_insert_close (preupdate);
		g_free (id.contact);

		tracker_sparql_builder_predicate (metadata, "nco:representative");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (id.keywords) {
		insert_keywords (metadata, uri, id.keywords);
		g_free (id.keywords);
	}

	if (md.camera) {
		tracker_sparql_builder_predicate (metadata, "nmm:camera");
		tracker_sparql_builder_object_unvalidated (metadata, md.camera);
		g_free (md.camera);
	}

	if (md.title) {
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, md.title);
		g_free (md.title);
	}

	if (md.orientation) {
		tracker_sparql_builder_predicate (metadata, "nfo:orientation");
		tracker_sparql_builder_object_unvalidated (metadata, md.orientation);
		g_free (md.orientation);
	}

	if (md.copyright) {
		tracker_sparql_builder_predicate (metadata, "nie:copyright");
		tracker_sparql_builder_object_unvalidated (metadata, md.copyright);
		g_free (md.copyright);
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

	if (md.artist) {
		gchar *uri = tracker_uri_printf_escaped ("urn:contact:%s", md.artist);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, uri);
		tracker_sparql_builder_insert_close (preupdate);
		g_free (md.artist);

		tracker_sparql_builder_predicate (metadata, "nco:contributor");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
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

	if (md.metering_mode) {
		tracker_sparql_builder_predicate (metadata, "nmm:meteringMode");
		tracker_sparql_builder_object_unvalidated (metadata, md.metering_mode);
		g_free (md.metering_mode);
	}

	if (md.creator) {
		gchar *uri = tracker_uri_printf_escaped ("urn:contact:%s", md.creator);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, md.creator);
		tracker_sparql_builder_insert_close (preupdate);
		g_free (md.creator);

		/* NOTE: We only have affiliation with
		 * nco:PersonContact and we are using
		 * nco:Contact here.
		 */

		/* if (id.byline_title) { */
		/* 	tracker_sparql_builder_insert_open (preupdate, NULL); */

		/* 	tracker_sparql_builder_subject (preupdate, "_:affiliation_by_line"); */
		/* 	tracker_sparql_builder_predicate (preupdate, "a"); */
		/* 	tracker_sparql_builder_object (preupdate, "nco:Affiliation"); */

		/* 	tracker_sparql_builder_predicate (preupdate, "nco:title"); */
		/* 	tracker_sparql_builder_object_unvalidated (preupdate, id.byline_title); */

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

	g_free (id.byline_title);
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return extract_data;
}
