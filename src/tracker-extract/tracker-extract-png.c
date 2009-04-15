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

#include "tracker-main.h"
#include "tracker-xmp.h"

#define RFC1123_DATE_FORMAT "%d %B %Y %H:%M:%S %z"

#define NMM_PREFIX TRACKER_NMM_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

typedef gchar * (*PostProcessor) (gchar *);

typedef struct {
	const gchar   *name;
	const gchar   *type;
	PostProcessor  post;
	gboolean       anon;
} TagProcessors;

static gchar *rfc1123_to_iso8601_date (gchar	   *rfc_date);
static void   extract_png	      (const gchar *filename,
				       GPtrArray   *metadata);

static TagProcessors tag_processors[] = {
	{ "Author",		NCO_PREFIX "creator",      NULL, TRUE},
	{ "Creator",		NCO_PREFIX "creator",      NULL, TRUE},
	{ "Description",	NIE_PREFIX "description",  NULL, FALSE},
	{ "Comment",		NIE_PREFIX "comment",      NULL, FALSE},
	{ "Copyright",		NIE_PREFIX "copyright",    NULL, FALSE},
	{ "Creation Time",	NIE_PREFIX "contentCreated", rfc1123_to_iso8601_date, FALSE},
	{ "Title",		NIE_PREFIX "title",	    NULL, FALSE},
	{ "Software",		"Image:Software",           NULL, FALSE},
	{ "Disclaimer",		NIE_PREFIX "license",       NULL, FALSE},
	{ NULL,			NULL,		            NULL, FALSE},
};

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
read_metadata (png_structp png_ptr, png_infop info_ptr, const gchar *uri, GPtrArray *metadata)
{
	gint	     num_text;
	png_textp    text_ptr;

	if (png_get_text (png_ptr, info_ptr, &text_ptr, &num_text) > 0) {
		gint i;
		gint j;
		
		for (i = 0; i < num_text; i++) {
			if (!text_ptr[i].key) {
				continue;
			}
			
#if defined(HAVE_EXEMPI) && defined(PNG_iTXt_SUPPORTED)
			if (strcmp ("XML:com.adobe.xmp", text_ptr[i].key) == 0) {
				tracker_read_xmp (text_ptr[i].text,
						  text_ptr[i].itxt_length,
						  uri,
						  metadata);
				continue;
			}
#endif
			
			for (j = 0; tag_processors[j].type; j++) {
				if (strcasecmp (tag_processors[j].name, text_ptr[i].key) != 0) {
					continue;
				}
				
				if (text_ptr[i].text && text_ptr[i].text[0] != '\0') {
					if (tag_processors[j].post) {
						gchar *str;
						
						str = (*tag_processors[j].post) (text_ptr[i].text);
						if (str) {
							if (tag_processors[j].anon) {
								tracker_statement_list_insert (metadata, ":", RDF_TYPE, NCO_PREFIX "Contact");
								tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", str);
								tracker_statement_list_insert (metadata, uri, tag_processors[j].type, ":");
							} else {
								tracker_statement_list_insert (metadata, uri,
											  tag_processors[j].type,
											  str);
							}
							g_free (str);
						}
					} else {
						if (tag_processors[j].anon) {
							tracker_statement_list_insert (metadata, ":", RDF_TYPE, NCO_PREFIX "Contact");
							tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", text_ptr[i].text);
							tracker_statement_list_insert (metadata, uri, tag_processors[j].type, ":");
						} else {
							tracker_statement_list_insert (metadata, uri,
										  tag_processors[j].type,
										  text_ptr[i].text);
						}
					}
					
					break;
				}
			}
		}
	}
}

static void
extract_png (const gchar *uri,
	     GPtrArray   *metadata)
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

 		read_metadata (png_ptr, info_ptr, uri, metadata);
 		read_metadata (png_ptr, end_ptr, uri, metadata);

		tracker_statement_list_insert (metadata, uri, 
		                          RDF_TYPE, 
		                          NFO_PREFIX "Image");

		/* We want native have higher priority than XMP etc.
		 */
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
