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

#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-file-utils.h>

#include "tracker-main.h"
#include "tracker-xmp.h"
#include "tracker-escape.h"

#define RFC1123_DATE_FORMAT "%d %B %Y %H:%M:%S %z"

typedef gchar * (*PostProcessor) (gchar *);

typedef struct {
	const gchar   *name;
	const gchar   *type;
	PostProcessor  post;
} TagProcessors;

static gchar *rfc1123_to_iso8601_date (gchar	   *rfc_date);
static void   extract_png	      (const gchar *filename,
				       GHashTable  *metadata);

static TagProcessors tag_processors[] = {
	{ "Author",		"Image:Creator",      NULL},
	{ "Creator",		"Image:Creator",      NULL},
	{ "Description",	"Image:Description",  NULL},
	{ "Comment",		"Image:Comments",     NULL},
	{ "Copyright",		"File:Copyright",     NULL},
	{ "Creation Time",	"Image:Date",	      rfc1123_to_iso8601_date},
	{ "Title",		"Image:Title",	      NULL},
	{ "Software",		"Image:Software",     NULL},
	{ "Disclaimer",		"File:License",       NULL},
	{ NULL,			NULL,		      NULL},
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
read_metadata (png_structp png_ptr, png_infop info_ptr, GHashTable *metadata)
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
							g_hash_table_insert (metadata,
									     g_strdup (tag_processors[j].type),
									     tracker_escape_metadata (str));
							g_free (str);
						}
					} else {
						g_hash_table_insert (metadata,
								     g_strdup (tag_processors[j].type),
								     tracker_escape_metadata (text_ptr[i].text));
					}
					
					break;
				}
			}
		}
	}
}

static void
extract_png (const gchar *filename,
	     GHashTable  *metadata)
{
	struct stat  fstatbuf;
	size_t	     size;

	gint	     fd_png;
	FILE	    *png;
	png_structp  png_ptr;
	png_infop    info_ptr;
	png_infop    end_ptr;
	png_bytepp   row_pointers;
	guint        row;

	png_uint_32  width, height;
	gint	     bit_depth, color_type;
	gint	     interlace_type, compression_type, filter_type;

#if defined(__linux__)
	if (((fd_png = g_open (filename, (O_RDONLY | O_NOATIME))) == -1) &&
	    ((fd_png = g_open (filename, (O_RDONLY))) == -1 ) ) {
#else
	if ((fd_png = g_open (filename, O_RDONLY)) == -1) {
#endif
		return;
	}

	if (stat (filename, &fstatbuf) == -1) {
		close(fd_png);
		return;
	}

	/* Check for minimum header size */
	size = fstatbuf.st_size;
	if (size < 64) {
		close (fd_png);
		return;
	}

	if ((png = fdopen (fd_png, "r"))) {
		png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
						  NULL,
						  NULL,
						  NULL);
		if (!png_ptr) {
			fclose (png);
			return;
		}

		info_ptr = png_create_info_struct (png_ptr);
		if (!info_ptr) {
			png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
			fclose (png);
			return;
		}

		end_ptr = png_create_info_struct (png_ptr);
		if (!info_ptr) {
			png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
			png_destroy_read_struct (&png_ptr, &end_ptr, NULL);
			fclose (png);
			return;
		}

		if (setjmp(png_jmpbuf(png_ptr))) {
			png_destroy_read_struct (&png_ptr, &info_ptr,
						 (png_infopp)NULL);
			png_destroy_read_struct (&png_ptr, &end_ptr,
						 (png_infopp)NULL);
			fclose (png);
			return;
		}

		png_init_io (png_ptr, png);
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
			png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
			png_destroy_read_struct (&png_ptr, &end_ptr, NULL);
			fclose (png);
			return;
		}
		
		/* Read the image. FIXME We should be able to skip this step and
		 * just get the info from the end. This causes some errors atm.
		 */
		row_pointers = (png_bytepp) malloc (height * sizeof (png_bytep));		
		for (row = 0; row < height; row++) {
			row_pointers[row] = png_malloc (png_ptr,
							png_get_rowbytes(png_ptr,info_ptr));
		}

		png_read_image (png_ptr, row_pointers);

		for (row = 0; row < height; row++) {
			png_free (png_ptr, row_pointers[row]);
		}
 		g_free (row_pointers);

		png_read_end (png_ptr, end_ptr);

 		read_metadata (png_ptr, info_ptr, metadata);
 		read_metadata (png_ptr, end_ptr, metadata);
		
		/* We want native have higher priority than XMP etc.
		 */
		g_hash_table_insert (metadata,
				     g_strdup ("Image:Width"),
				     tracker_escape_metadata_printf ("%ld", width));
		g_hash_table_insert (metadata,
				     g_strdup ("Image:Height"),
				     tracker_escape_metadata_printf ("%ld", height));
		
		/* Check that we have the minimum data. FIXME We should not need to do this */

		if (!g_hash_table_lookup (metadata, "Image:Date")) {
			gchar *date;
			guint64 mtime;

			mtime = tracker_file_get_mtime (filename);
			date = tracker_date_to_string ((time_t) mtime);
			
			g_hash_table_insert (metadata,
					     g_strdup ("Image:Date"),
					     tracker_escape_metadata (date));
			g_free (date);
		}

		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		fclose (png);
	} else {
		close (fd_png);
	}
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
