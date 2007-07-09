/* Tracker Extract - extracts embedded metadata from files
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include "tracker-extract.h"

#include <stdio.h>
#include <glib.h>
#include <png.h>

static struct {
	char *name;
	char *type;
} tagmap[] = {
	{ "Author" , "Image:Creator"},
	{ "Creator" , "Image:Creator"},
	{ "Description" , "Image:Description"},
	{ "Comment", "Image:Comments"},
	{ "Copyright", "File:Copyright"},
	{ "Creation Time", "Image:Date"},
	{ "Title", "Image:Title"},
	{ "Software", "Image:Software"},
	{ "Disclaimer", "File:License"},
	{ NULL, NULL},
};


void
tracker_extract_png (gchar *filename, GHashTable *metadata)
{
	FILE *png;
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	int num_text;
	png_textp text_ptr;

	int bit_depth, color_type, i, j;
	int interlace_type, compression_type, filter_type;

	if ((png = fopen(filename, "r"))) {
		png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
		                                  NULL, NULL, NULL);
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
		png_init_io (png_ptr, png);
		png_read_info (png_ptr, info_ptr);

		/* read header bits */
		if (png_get_IHDR (png_ptr, info_ptr, &width, &height, &bit_depth,
		                 &color_type, &interlace_type, &compression_type, &filter_type)) {
			g_hash_table_insert (metadata, g_strdup ("Image:Width"),
			                     g_strdup_printf ("%ld", width));
			g_hash_table_insert (metadata, g_strdup ("Image:Height"),
			                     g_strdup_printf ("%ld", height));
		}

		if (png_get_text (png_ptr, info_ptr, &text_ptr, &num_text) > 0) {
			for (i = 0; i < num_text; i++) {
				if ( text_ptr[i].key != NULL ) {
					#if defined(HAVE_EXEMPI) && defined(PNG_iTXt_SUPPORTED)
					if (strcmp("XML:com.adobe.xmp",text_ptr[i].key) == 0) {
						tracker_read_xmp(text_ptr[i].text,text_ptr[i].itxt_length,metadata);
						continue;
					}
					#endif
	
					for (j=0; tagmap[j].type; j++) {
						if (strcasecmp (tagmap[j].name,  text_ptr[i].key) == 0) {
							if (text_ptr[i].text && strlen (text_ptr[i].text) > 0) {
								g_hash_table_insert (metadata, g_strdup (tagmap[j].type), g_strdup (text_ptr[i].text));
							}
							break;
						}
					}
				}
			}
		}

		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		
		fclose (png);
	}
}
