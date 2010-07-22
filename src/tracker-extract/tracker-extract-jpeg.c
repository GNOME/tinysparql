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

#include <stdio.h>
#include <setjmp.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include <jpeglib.h>

#include <libtracker-common/tracker-common.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-main.h"

#define CM_TO_INCH              0.393700787

#ifdef HAVE_LIBEXIF
#define EXIF_NAMESPACE          "Exif"
#define EXIF_NAMESPACE_LENGTH   4
#endif /* HAVE_LIBEXIF */

#ifdef HAVE_EXEMPI
#define XMP_NAMESPACE           "http://ns.adobe.com/xap/1.0/\x00"
#define XMP_NAMESPACE_LENGTH    29
#endif /* HAVE_EXEMPI */

#ifdef HAVE_LIBIPTCDATA
#define PS3_NAMESPACE           "Photoshop 3.0\0"
#define PS3_NAMESPACE_LENGTH    14
#include <libiptcdata/iptc-jpeg.h>
#endif /* HAVE_LIBIPTCDATA */

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
	const gchar *comment;
	const gchar *city;
	const gchar *state;
	const gchar *address;
	const gchar *country; 
} MergeData;

static void extract_jpeg (const gchar          *filename,
                          TrackerSparqlBuilder *preupdate,
                          TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "image/jpeg", extract_jpeg },
	{ NULL, NULL }
};

struct tej_error_mgr {
	struct jpeg_error_mgr jpeg;
	jmp_buf setjmp_buffer;
};

static void
extract_jpeg_error_exit (j_common_ptr cinfo)
{
	struct tej_error_mgr *h = (struct tej_error_mgr *) cinfo->err;
	(*cinfo->err->output_message)(cinfo);
	longjmp (h->setjmp_buffer, 1);
}

static void
extract_jpeg (const gchar          *uri,
              TrackerSparqlBuilder *preupdate,
              TrackerSparqlBuilder *metadata)
{
	struct jpeg_decompress_struct cinfo;
	struct tej_error_mgr tejerr;
	struct jpeg_marker_struct *marker;
	TrackerXmpData *xd = NULL;
	TrackerExifData *ed = NULL;
	TrackerIptcData *id = NULL;
	MergeData md = { 0 };
	FILE *f;
	goffset size;
	gchar *filename;
	gchar *comment = NULL;
	GPtrArray *keywords;
	guint i;

	filename = g_filename_from_uri (uri, NULL, NULL);

	size = tracker_file_get_size (filename);

	if (size < 18) {
		g_free (filename);
		return;
	}

	f = tracker_file_open (filename, "rb", FALSE);
	g_free (filename);

	if (!f) {
		return;
	}

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:Image");
	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nmm:Photo");

	cinfo.err = jpeg_std_error (&tejerr.jpeg);
	tejerr.jpeg.error_exit = extract_jpeg_error_exit;
	if (setjmp (tejerr.setjmp_buffer)) {
		goto fail;
	}

	jpeg_create_decompress (&cinfo);

	jpeg_save_markers (&cinfo, JPEG_COM, 0xFFFF);
	jpeg_save_markers (&cinfo, JPEG_APP0 + 1, 0xFFFF);
	jpeg_save_markers (&cinfo, JPEG_APP0 + 13, 0xFFFF);

	jpeg_stdio_src (&cinfo, f);

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
		gchar *str;
		gsize len;
#ifdef HAVE_LIBIPTCDATA
		gsize offset;
		guint sublen;
#endif /* HAVE_LIBIPTCDATA */

		switch (marker->marker) {
		case JPEG_COM:
			g_free (comment);
			comment = g_strndup ((gchar*) marker->data, marker->data_length);
			break;

		case JPEG_APP0 + 1:
			str = (gchar*) marker->data;
			len = marker->data_length;

#ifdef HAVE_LIBEXIF
			if (strncmp (EXIF_NAMESPACE, str, EXIF_NAMESPACE_LENGTH) == 0) {
				ed = tracker_exif_new ((guchar *) marker->data, len, uri);
			}
#endif /* HAVE_LIBEXIF */

#ifdef HAVE_EXEMPI
			if (strncmp (XMP_NAMESPACE, str, XMP_NAMESPACE_LENGTH) == 0) {
				xd = tracker_xmp_new (str + XMP_NAMESPACE_LENGTH,
				                      len - XMP_NAMESPACE_LENGTH,
				                      uri);
			}
#endif /* HAVE_EXEMPI */

			break;

		case JPEG_APP0 + 13:
			str = (gchar*) marker->data;
			len = marker->data_length;
#ifdef HAVE_LIBIPTCDATA
			if (len > 0 && strncmp (PS3_NAMESPACE, str, PS3_NAMESPACE_LENGTH) == 0) {
				offset = iptc_jpeg_ps3_find_iptc (str, len, &sublen);
				if (offset > 0 && sublen > 0) {
					id = tracker_iptc_new (str + offset, sublen, uri);
				}
			}
#endif /* HAVE_LIBIPTCDATA */

			break;

		default:
			marker = marker->next;
			continue;
		}

		marker = marker->next;
	}

	if (!ed) {
		ed = g_new0 (TrackerExifData, 1);
	}

	if (!xd) {
		xd = g_new0 (TrackerXmpData, 1);
	}

	if (!id) {
		id = g_new0 (TrackerIptcData, 1);
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

	md.title = tracker_coalesce_strip (4, xd->title, ed->document_name, xd->title2, xd->pdf_title);
	md.orientation = tracker_coalesce_strip (3, xd->orientation, ed->orientation, id->image_orientation);
	md.copyright = tracker_coalesce_strip (4, xd->copyright, xd->rights, ed->copyright, id->copyright_notice);
	md.white_balance = tracker_coalesce_strip (2, xd->white_balance, ed->white_balance);
	md.fnumber = tracker_coalesce_strip (2, xd->fnumber, ed->fnumber);
	md.flash = tracker_coalesce_strip (2, xd->flash, ed->flash);
	md.focal_length =  tracker_coalesce_strip (2, xd->focal_length, ed->focal_length);
	md.artist = tracker_coalesce_strip (3, xd->artist, ed->artist, xd->contributor);
	md.exposure_time = tracker_coalesce_strip (2, xd->exposure_time, ed->exposure_time);
	md.iso_speed_ratings = tracker_coalesce_strip (2, xd->iso_speed_ratings, ed->iso_speed_ratings);
	md.date = tracker_coalesce_strip (5, xd->date, xd->time_original, ed->time, id->date_created, ed->time_original);
	md.description = tracker_coalesce_strip (2, xd->description, ed->description);
	md.metering_mode = tracker_coalesce_strip (2, xd->metering_mode, ed->metering_mode);
	md.city = tracker_coalesce_strip (2, xd->city, id->city);
	md.state = tracker_coalesce_strip (2, xd->state, id->state);
	md.address = tracker_coalesce_strip (2, xd->address, id->sublocation);
	md.country = tracker_coalesce_strip (2, xd->country, id->country_name);
	md.creator = tracker_coalesce_strip (3, xd->creator, id->byline, id->credit);
	md.comment = tracker_coalesce_strip (2, comment, ed->user_comment);

	/* Prioritize on native dimention in all cases */
	tracker_sparql_builder_predicate (metadata, "nfo:width");
	tracker_sparql_builder_object_int64 (metadata, cinfo.image_width);

	/* TODO: add ontology and store ed->software */

	tracker_sparql_builder_predicate (metadata, "nfo:height");
	tracker_sparql_builder_object_int64 (metadata, cinfo.image_height);

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

	keywords = g_ptr_array_new ();

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

	if (xd->rating) {
		tracker_sparql_builder_predicate (metadata, "nao:numericRating");
		tracker_sparql_builder_object_unvalidated (metadata, xd->rating);
	}

	if (xd->format) {
		tracker_sparql_builder_predicate (metadata, "dc:format");
		tracker_sparql_builder_object_unvalidated (metadata, xd->format);
	}

	if (xd->identifier) {
		tracker_sparql_builder_predicate (metadata, "dc:indentifier");
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

	if (xd->license) {
		tracker_sparql_builder_predicate (metadata, "nie:license");
		tracker_sparql_builder_object_unvalidated (metadata, xd->license);
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
		tracker_sparql_builder_object (metadata, md.orientation);
	}

	if (md.copyright) {
		tracker_sparql_builder_predicate (metadata, "nie:copyright");
		tracker_sparql_builder_object_unvalidated (metadata, md.copyright);
	}

	if (md.white_balance) {
		tracker_sparql_builder_predicate (metadata, "nmm:whiteBalance");
		tracker_sparql_builder_object (metadata, md.white_balance);
	}

	if (md.fnumber) {
		gdouble value;

		value = g_strtod (md.fnumber, NULL);
		tracker_sparql_builder_predicate (metadata, "nmm:fnumber");
		tracker_sparql_builder_object_double (metadata, value);
	}

	if (md.flash) {
		tracker_sparql_builder_predicate (metadata, "nmm:flash");
		tracker_sparql_builder_object (metadata, md.flash);
	}

	if (md.focal_length) {
		gdouble value;

		value = g_strtod (md.focal_length, NULL);
		tracker_sparql_builder_predicate (metadata, "nmm:focalLength");
		tracker_sparql_builder_object_double (metadata, value);
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
		gdouble value;

		value = g_strtod (md.exposure_time, NULL);
		tracker_sparql_builder_predicate (metadata, "nmm:exposureTime");
		tracker_sparql_builder_object_double (metadata, value);
	}

	if (md.iso_speed_ratings) {
		gdouble value;

		value = g_strtod (md.iso_speed_ratings, NULL);
		tracker_sparql_builder_predicate (metadata, "nmm:isoSpeed");
		tracker_sparql_builder_object_double (metadata, value);
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
		tracker_sparql_builder_object (metadata, md.metering_mode);
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

		/*      tracker_sparql_builder_predicate (preupdate, "a"); */
		/*      tracker_sparql_builder_object (preupdate, "nco:Contact"); */
		/*      tracker_sparql_builder_predicate (preupdate, "nco:hasAffiliation"); */
		/*      tracker_sparql_builder_object (preupdate, "_:affiliation_by_line"); */
		/* } */

		tracker_sparql_builder_predicate (metadata, "nco:creator");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (md.comment) {
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, md.comment);
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

	if (cinfo.density_unit != 0 || ed->x_resolution) {
		gdouble value;

		if (cinfo.density_unit == 0) {
			if (ed->resolution_unit == 1)
				value = g_strtod (ed->x_resolution, NULL);
			else
				value = g_strtod (ed->x_resolution, NULL) * CM_TO_INCH;
		} else {
			if (cinfo.density_unit == 1)
				value = cinfo.X_density;
			else
				value = cinfo.X_density * CM_TO_INCH;
		}

		tracker_sparql_builder_predicate (metadata, "nfo:horizontalResolution");
		tracker_sparql_builder_object_double (metadata, value);
	}

	if (cinfo.density_unit != 0 || ed->y_resolution) {
		gdouble value;

		if (cinfo.density_unit == 0) {
			if (ed->resolution_unit == 1)
				value = g_strtod (ed->y_resolution, NULL);
			else
				value = g_strtod (ed->y_resolution, NULL) * CM_TO_INCH;
		} else {
			if (cinfo.density_unit == 1)
				value = cinfo.Y_density;
			else
				value = cinfo.Y_density * CM_TO_INCH;
		}

		tracker_sparql_builder_predicate (metadata, "nfo:verticalResolution");
		tracker_sparql_builder_object_double (metadata, value);
	}

	jpeg_destroy_decompress (&cinfo);

	g_free (md.camera);

	tracker_exif_free (ed);
	tracker_xmp_free (xd);
	tracker_iptc_free (id);
	g_free (comment);

fail:
	tracker_file_close (f, FALSE);
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
