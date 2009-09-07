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

#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-main.h"
#include "tracker-xmp.h"

#define RFC1123_DATE_FORMAT "%d %B %Y %H:%M:%S %z"

#define NMM_PREFIX TRACKER_NMM_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX

#define RDF_PREFIX TRACKER_RDF_PREFIX

typedef struct {
	gchar *title, *copyright, *creator, *description, *date;
} PngNeedsMergeData;

typedef struct {
	gchar *author, *creator, *description, *comment, *copyright, 
	      *creation_time, *title, *disclaimer;
} PngData;

static gchar *rfc1123_to_iso8601_date (gchar	   *rfc_date);
static void   extract_png	      (const gchar *filename,
				       TrackerSparqlBuilder   *metadata);

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
		tracker_statement_list_insert (metadata, uri, 
		                               NIE_PREFIX "keyword", 
		                               (const gchar*) keyw);
	}
}

static void
read_metadata (png_structp png_ptr, png_infop info_ptr, const gchar *uri, TrackerSparqlBuilder *metadata)
{
	gint	     num_text;
	png_textp    text_ptr;
	PngNeedsMergeData merge_data = { 0 };
	PngData png_data = { 0 };
	TrackerXmpData xmp_data = { 0 };

	if (png_get_text (png_ptr, info_ptr, &text_ptr, &num_text) > 0) {
		gint i;

		for (i = 0; i < num_text; i++) {

			if (!text_ptr[i].key || !text_ptr[i].text || text_ptr[i].text[0] == '\0') {
				continue;
			}

#if defined(HAVE_EXEMPI) && defined(PNG_iTXt_SUPPORTED)

			if (g_strcmp0 ("XML:com.adobe.xmp", text_ptr[i].key) == 0) {

				/* ATM tracker_read_xmp supports setting xmp_data 
				 * multiple times, keep it that way as here it's
				 * theoretically possible that the function gets
				 * called multiple times */

				tracker_read_xmp (text_ptr[i].text,
						  text_ptr[i].itxt_length,
						  uri, &xmp_data);

				continue;
			}
#endif

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

		merge_data.creator = tracker_coalesce (3, png_data.creator,
		                                       png_data.author,
		                                       xmp_data.creator);

		merge_data.title = tracker_coalesce (2, png_data.title,
		                                     xmp_data.title);

		merge_data.copyright = tracker_coalesce (2, png_data.copyright,
		                                         xmp_data.rights);

		merge_data.description = tracker_coalesce (2, png_data.description,
		                                           xmp_data.description);

		merge_data.date = tracker_coalesce (3, png_data.creation_time,
		                                    xmp_data.date, 
		                                    xmp_data.DateTimeOriginal);

		if (merge_data.creator) {
			tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", merge_data.creator);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "creator", ":");
			g_free (merge_data.creator);
		}

		if (merge_data.date) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "contentCreated", merge_data.date);
			g_free (merge_data.date);
		}

		if (merge_data.description) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "description", merge_data.description);
			g_free (merge_data.description);
		}

		if (merge_data.copyright) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "copyright", merge_data.copyright);
			g_free (merge_data.copyright);
		}

		if (merge_data.title) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "title", merge_data.title);
			g_free (merge_data.title);
		}


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

		if (xmp_data.Make || xmp_data.Model) {
			gchar *final_camera = tracker_coalesce (2, xmp_data.Make, xmp_data.Model); 
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "camera", final_camera);
			g_free (final_camera);
		}

		if (xmp_data.Orientation) {
			tracker_statement_list_insert (metadata, uri, NFO_PREFIX "orientation", xmp_data.Orientation);
			g_free (xmp_data.Orientation);
		}

		if (xmp_data.WhiteBalance) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "whiteBalance", xmp_data.WhiteBalance);
			g_free (xmp_data.WhiteBalance);
		}

		if (xmp_data.FNumber) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "fnumber", xmp_data.FNumber);
			g_free (xmp_data.FNumber);
		}

		if (xmp_data.Flash) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "flash", xmp_data.Flash);
			g_free (xmp_data.Flash);
		}

		if (xmp_data.FocalLength) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "focalLength", xmp_data.FocalLength);
			g_free (xmp_data.FocalLength);
		}

		if (xmp_data.Artist || xmp_data.contributor) {
			gchar *final_artist =  tracker_coalesce (2, xmp_data.Artist, xmp_data.contributor);
			tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", final_artist);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "contributor", ":");
			g_free (final_artist);
		}

		if (xmp_data.ExposureTime) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "exposureTime", xmp_data.ExposureTime);
			g_free (xmp_data.ExposureTime);
		}

		if (xmp_data.ISOSpeedRatings) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "isoSpeed", xmp_data.ISOSpeedRatings);
			g_free (xmp_data.ISOSpeedRatings);
		}

		if (xmp_data.MeteringMode) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "meteringMode", xmp_data.MeteringMode);
			g_free (xmp_data.MeteringMode);
		}
	}
}

static void
extract_png (const gchar *uri,
	     TrackerSparqlBuilder   *metadata)
{
	goffset      size;
	FILE	    *f;
	png_structp  png_ptr;
	png_infop    info_ptr;
	png_infop    end_ptr;
	png_bytepp   row_pointers;
	guint        row;
	png_uint_32  width, height;
	gint	     bit_depth, color_type;
	gint	     interlace_type, compression_type, filter_type;
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

		tracker_statement_list_insert (metadata, uri, 
		                               RDF_PREFIX "type", 
		                               NFO_PREFIX "Image");

		tracker_statement_list_insert (metadata, uri,
		                               RDF_PREFIX "type",
		                               NMM_PREFIX "Photo");

		read_metadata (png_ptr, info_ptr, uri, metadata);
		read_metadata (png_ptr, end_ptr, uri, metadata);

		tracker_statement_list_insert_with_int (metadata, uri,
		                                        NFO_PREFIX "width",
		                                        width);

		tracker_statement_list_insert_with_int (metadata, uri,
		                                        NFO_PREFIX "height",
		                                        height);

		png_destroy_read_struct (&png_ptr, &info_ptr, &end_ptr);
		tracker_file_close (f, FALSE);
	}

	g_free (filename);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
