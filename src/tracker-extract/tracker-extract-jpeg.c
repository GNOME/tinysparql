/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

/*
 * FIXME: We should try to get raw data (from libexif) to avoid processing.
 */

#include "config.h"

#include <stdio.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include <ctype.h>
#include <string.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <setjmp.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <jpeglib.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-main.h"
#include "tracker-xmp.h"
#include "tracker-iptc.h"
#include "tracker-exif.h"

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define NMM_PREFIX TRACKER_NMM_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX

#ifdef HAVE_LIBEXIF
#define EXIF_NAMESPACE		"Exif"
#define EXIF_NAMESPACE_LENGTH	4
#endif /* HAVE_LIBEXIF */

#ifdef HAVE_EXEMPI
#define XMP_NAMESPACE		"http://ns.adobe.com/xap/1.0/\x00"
#define XMP_NAMESPACE_LENGTH	29
#endif /* HAVE_EXEMPI */

#ifdef HAVE_LIBIPTCDATA
#define PS3_NAMESPACE		"Photoshop 3.0\0"
#define PS3_NAMESPACE_LENGTH	14
#include <libiptcdata/iptc-jpeg.h>
#endif /* HAVE_LIBIPTCDATA */

typedef struct {
	gchar *camera, *title, *orientation, *copyright, *white_balance, 
	      *fnumber, *flash, *focal_length, *artist, 
	      *exposure_time, *iso_speed_ratings, *date, *description,
	      *metering_mode, *creator;
} JpegNeedsMergeData;

static void extract_jpeg (const gchar *filename,
			  TrackerSparqlBuilder   *metadata);

static TrackerExtractData data[] = {
	{ "image/jpeg", extract_jpeg },
	{ NULL, NULL }
};

struct tej_error_mgr 
{
	struct jpeg_error_mgr jpeg;
	jmp_buf setjmp_buffer;
};

static void tracker_extract_jpeg_error_exit (j_common_ptr cinfo)
{
    struct tej_error_mgr *h = (struct tej_error_mgr *)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(h->setjmp_buffer, 1);
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
		tracker_statement_list_insert (metadata, uri, 
		                               NIE_PREFIX "keyword", 
		                               (const gchar*) keyw);
	}
}

static void
extract_jpeg (const gchar *uri,
	      TrackerSparqlBuilder   *metadata)
{
	struct jpeg_decompress_struct  cinfo;
	struct tej_error_mgr	       tejerr;
	struct jpeg_marker_struct     *marker;
	FILE			      *f;
	goffset                        size;
	gchar                         *filename;

	filename = g_filename_from_uri (uri, NULL, NULL);

	size = tracker_file_get_size (filename);

	if (size < 18) {
		return;
	}

	f = tracker_file_open (filename, "rb", FALSE);

	if (f) {
		TrackerXmpData xmp_data = { 0 };
		TrackerExifData exif_data = { 0 };
		TrackerIptcData iptc_data = { 0 };
		JpegNeedsMergeData merge_data = { 0 };
		gchar *str;
		gsize  len;
#ifdef HAVE_LIBIPTCDATA
		gsize  offset;
		gsize  sublen;
#endif /* HAVE_LIBIPTCDATA */

		tracker_statement_list_insert (metadata, uri, 
		                               RDF_PREFIX "type", 
		                               NFO_PREFIX "Image");

		tracker_statement_list_insert (metadata, uri,
		                               RDF_PREFIX "type",
		                               NMM_PREFIX "Photo");

		cinfo.err = jpeg_std_error (&tejerr.jpeg);
		tejerr.jpeg.error_exit = tracker_extract_jpeg_error_exit;
		if (setjmp(tejerr.setjmp_buffer)) {
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
			switch (marker->marker) {
			case JPEG_COM:
				len = marker->data_length;
				str = g_strndup ((gchar*) marker->data, len);

				tracker_statement_list_insert (metadata, uri,
				                               NIE_PREFIX "comment",
				                               str);

				g_free (str);

				break;

			case JPEG_APP0+1:
				str = (gchar*) marker->data;
				len = marker->data_length;

#ifdef HAVE_LIBEXIF
				if (strncmp (EXIF_NAMESPACE, (gchar*) (marker->data), EXIF_NAMESPACE_LENGTH) == 0) {

					tracker_read_exif ((unsigned char*) marker->data,
					                   marker->data_length, 
					                   uri, &exif_data);
				}
#endif /* HAVE_LIBEXIF */

#ifdef HAVE_EXEMPI
				if (strncmp (XMP_NAMESPACE, str, XMP_NAMESPACE_LENGTH) == 0) {
					TrackerXmpData xmp_data = { 0 };

					tracker_read_xmp (str + XMP_NAMESPACE_LENGTH,
							  len - XMP_NAMESPACE_LENGTH,
							  uri, &xmp_data);

				}
#endif /* HAVE_EXEMPI */

				break;

			case JPEG_APP0+13:
				str = (gchar*) marker->data;
				len = marker->data_length;
#ifdef HAVE_LIBIPTCDATA
				if (strncmp (PS3_NAMESPACE, str, PS3_NAMESPACE_LENGTH) == 0) {
					offset = iptc_jpeg_ps3_find_iptc (str, len, &sublen);
					if (offset>0) {
						tracker_read_iptc (str + offset,
						                   sublen,
						                   uri, &iptc_data);
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

		merge_data.camera = tracker_merge (" ", 2, xmp_data.Make,
		                                   xmp_data.Model);

		if (!merge_data.camera) {
			merge_data.camera = tracker_merge (" ", 2, exif_data.make,
			                                   exif_data.model);
		} else {
			g_free (exif_data.model);
			g_free (exif_data.make);
		}

		merge_data.title = tracker_coalesce (2, xmp_data.title,
		                                     exif_data.document_name);

		merge_data.orientation = tracker_coalesce (3, exif_data.orientation,
		                                           xmp_data.Orientation,
		                                           iptc_data.image_orientation);

		merge_data.copyright = tracker_coalesce (3, exif_data.copyright,
		                                         xmp_data.rights,
		                                         iptc_data.copyright_notice);

		merge_data.white_balance = tracker_coalesce (2, exif_data.white_balance,
		                                             xmp_data.WhiteBalance);
		                                             

		merge_data.fnumber =  tracker_coalesce (2, exif_data.fnumber,
		                                        xmp_data.FNumber);

		merge_data.flash =  tracker_coalesce (2, exif_data.flash,
		                                      xmp_data.Flash);

		merge_data.focal_length =  tracker_coalesce (2, exif_data.focal_length,
		                                             xmp_data.FocalLength);

		merge_data.artist =  tracker_coalesce (3, exif_data.artist,
		                                       xmp_data.Artist,
		                                       xmp_data.contributor);

		merge_data.exposure_time =  tracker_coalesce (2, exif_data.exposure_time,
		                                              xmp_data.ExposureTime);

		merge_data.iso_speed_ratings =  tracker_coalesce (2, exif_data.iso_speed_ratings,
		                                                  xmp_data.ISOSpeedRatings);

		merge_data.date =  tracker_coalesce (5, exif_data.time, 
		                                     xmp_data.date,
		                                     iptc_data.date_created,
		                                     exif_data.time_original,
		                                     xmp_data.DateTimeOriginal);

		merge_data.description = tracker_coalesce (2, exif_data.description,
		                                           xmp_data.description);

		merge_data.metering_mode =  tracker_coalesce (2, exif_data.metering_mode,
		                                              xmp_data.MeteringMode);

		merge_data.creator =  tracker_coalesce (3, iptc_data.byline,
		                                        xmp_data.creator,
		                                        iptc_data.credit);

		if (exif_data.user_comment) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "comment", exif_data.user_comment);
			g_free (exif_data.user_comment);
		}

		/* Prioritize on native dimention in all cases */
		tracker_statement_list_insert_with_int (metadata, uri,
						   NFO_PREFIX "width",
						   cinfo.image_width);
		g_free (exif_data.x_dimension);

		tracker_statement_list_insert_with_int (metadata, uri,
						   NFO_PREFIX "height",
						    cinfo.image_height);
		g_free (exif_data.y_dimension);

		if (xmp_data.keywords) {
			insert_keywords (metadata, uri, xmp_data.keywords);
			g_free (xmp_data.keywords);
		}

		if (xmp_data.subject) {
			insert_keywords (metadata, uri, xmp_data.subject);
			g_free (xmp_data.subject);
		}

		if (xmp_data.publisher) {
			tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", xmp_data.publisher);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "publisher", ":");
			g_free (xmp_data.publisher);
		}

		if (xmp_data.type) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "type", xmp_data.type);
			g_free (xmp_data.type);
		}

		if (xmp_data.format) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "format", xmp_data.format);
			g_free (xmp_data.format);
		}

		if (xmp_data.identifier) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "identifier", xmp_data.identifier);
			g_free (xmp_data.identifier);
		}

		if (xmp_data.source) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "source", xmp_data.source);
			g_free (xmp_data.source);
		}

		if (xmp_data.language) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "language", xmp_data.language);
			g_free (xmp_data.language);
		}

		if (xmp_data.relation) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "relation", xmp_data.relation);
			g_free (xmp_data.relation);
		}

		if (xmp_data.coverage) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "coverage", xmp_data.coverage);
			g_free (xmp_data.coverage);
		}

		if (xmp_data.license) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "license", xmp_data.license);
			g_free (xmp_data.license);
		}

		if (iptc_data.keywords) {
			insert_keywords (metadata, uri, iptc_data.keywords);
			g_free (iptc_data.keywords);
		}

		if (merge_data.camera) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "camera", merge_data.camera);
			g_free (merge_data.camera);
		}

		if (merge_data.title) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "title", merge_data.title);
			g_free (merge_data.title);
		}

		if (merge_data.orientation) {
			tracker_statement_list_insert (metadata, uri, NFO_PREFIX "orientation", merge_data.orientation);
			g_free (merge_data.orientation);
		}

		if (merge_data.copyright) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "copyright", merge_data.copyright);
			g_free (merge_data.copyright);
		}

		if (merge_data.white_balance) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "whiteBalance", merge_data.white_balance);
			g_free (merge_data.white_balance);
		}

		if (merge_data.fnumber) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "fnumber", merge_data.fnumber);
			g_free (merge_data.fnumber);
		}

		if (merge_data.flash) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "flash", merge_data.flash);
			g_free (merge_data.flash);
		}

		if (merge_data.focal_length) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "focalLength", merge_data.focal_length);
			g_free (merge_data.focal_length);
		}

		if (merge_data.artist) {
			tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", merge_data.artist);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "contributor", ":");
			g_free (merge_data.artist);
		}

		if (merge_data.exposure_time) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "exposureTime", merge_data.exposure_time);
			g_free (merge_data.exposure_time);
		}

		if (merge_data.iso_speed_ratings) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "isoSpeed", merge_data.iso_speed_ratings);
			g_free (merge_data.iso_speed_ratings);
		}

		if (merge_data.date) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "contentCreated", merge_data.date);
			g_free (merge_data.date);
		}

		if (merge_data.description) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "description", merge_data.description);
			g_free (merge_data.description);
		}

		if (merge_data.metering_mode) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "meteringMode", merge_data.metering_mode);
			g_free (merge_data.metering_mode);
		}

		if (merge_data.creator) {
			tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", merge_data.creator);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "creator", ":");
			g_free (merge_data.creator);
		}

		jpeg_destroy_decompress (&cinfo);
fail:
		tracker_file_close (f, FALSE);
	}

	g_free (filename);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
