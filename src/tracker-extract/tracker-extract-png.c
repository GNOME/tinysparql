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
	gboolean       multi;
	PostProcessor  post;
} TagProcessors;

static gchar *rfc1123_to_iso8601_date (gchar	   *rfc_date);
static void   extract_png	      (const gchar *filename,
				       GHashTable  *metadata);

static TagProcessors tag_processors[] = {
	{ "Author",	   "Image:Creator",     FALSE, NULL },
	{ "Creator",	   "Image:Creator",     FALSE, NULL },
	{ "Description",   "Image:Description", FALSE, NULL },
	{ "Comment",	   "Image:Comments",    FALSE, NULL },
	{ "Copyright",	   "File:Copyright",    FALSE, NULL },
	{ "Creation Time", "Image:Date",	FALSE, rfc1123_to_iso8601_date },
	{ "Title",	   "Image:Title",	FALSE, NULL },
	{ "Disclaimer",	   "File:License",      FALSE, NULL },
#ifdef ENABLE_DETAILED_METADATA
	{ "Software",	   "Image:Software",    FALSE, NULL },
#endif /* ENABLE_DETAILED_METADATA */
	{ NULL,		   NULL,		FALSE, NULL },
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
metadata_append (GHashTable *metadata, gchar *key, gchar *value, gboolean append)
{
	gchar   *new_value;
	gchar   *orig;
	gchar  **list;
	gboolean found = FALSE;
	guint    i;

	if (append && (orig = g_hash_table_lookup (metadata, key))) {
		gchar *escaped;
		
		escaped = tracker_escape_metadata (value);

		list = g_strsplit (orig, "|", -1);			
		for (i=0; list[i]; i++) {
			if (strcmp (list[i], escaped) == 0) {
				found = TRUE;
				break;
			}
		}			
		g_strfreev(list);

		if (!found) {
			new_value = g_strconcat (orig, "|", escaped, NULL);
			g_hash_table_insert (metadata, g_strdup (key), new_value);
		}

		g_free (escaped);		
	} else {
		new_value = tracker_escape_metadata (value);
		g_hash_table_insert (metadata, g_strdup (key), new_value);

		/* FIXME Postprocessing is evil and should be elsewhere */
		if (strcmp (key, "Image:Keywords") == 0) {
			g_hash_table_insert (metadata,
					     g_strdup ("Image:HasKeywords"),
					     tracker_escape_metadata ("1"));			
		}		
	}

	/* Adding certain fields also to keywords FIXME Postprocessing is evil */
	if ((strcmp (key, "Image:Title") == 0) ||
	    (strcmp (key, "Image:Description") == 0) ) {
		metadata_append (metadata, "Image:Keywords", value, TRUE);
		g_hash_table_insert (metadata,
				     g_strdup ("Image:HasKeywords"),
				     tracker_escape_metadata ("1"));
	}
}

static void
read_metadata (png_structp  png_ptr, 
	       png_infop    info_ptr, 
	       GHashTable  *metadata)
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
							metadata_append (metadata,
									 g_strdup (tag_processors[j].type),
									 tracker_escape_metadata (str),
									 tag_processors[j].multi);
							g_free (str);
						}
					} else {
						metadata_append (metadata,
								 g_strdup (tag_processors[j].type),
								 tracker_escape_metadata (text_ptr[i].text),
								 tag_processors[j].multi);
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

	size = tracker_file_get_size (filename);

	if (size < 64) {
		goto fail;
	}

	f = tracker_file_open (filename, "r", FALSE); 

	if (f) {
		png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
						  NULL,
						  NULL,
						  NULL);
		if (!png_ptr) {
			tracker_file_close (f, FALSE);
			goto fail;
		}

		info_ptr = png_create_info_struct (png_ptr);
		if (!info_ptr) {
			png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
			tracker_file_close (f, FALSE);
			goto fail;
		}

		end_ptr = png_create_info_struct (png_ptr);
		if (!end_ptr) {
			png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
			tracker_file_close (f, FALSE);
			goto fail;
		}

		if (setjmp (png_jmpbuf (png_ptr))) {
			png_destroy_read_struct (&png_ptr, &info_ptr, &end_ptr);
			tracker_file_close (f, FALSE);
			goto fail;
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
			goto fail;
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

		png_destroy_read_struct (&png_ptr, &info_ptr, &end_ptr);
		tracker_file_close (f, FALSE);
	}
	
fail:
	/* We fallback to the file's modified time for the
	 * "Image:Date" metadata if it doesn't exist.
	 *
	 * FIXME: This shouldn't be necessary.
	 */
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
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
