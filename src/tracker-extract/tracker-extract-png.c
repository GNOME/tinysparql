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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "config.h"
#include "tracker-extract.h"

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <png.h>


static gchar *
rfc1123_to_iso8160_date (gchar *rfc_date)
{
        /* ex. RFC1123 date: "22 May 1997 18:07:10 -0600"
           To
           ex. ISO8160 date: "2007-05-22T18:07:10-0600"
        */

        steps steps_to_do[] = {
                DAY_STR, DAY, MONTH, YEAR, TIME, TIMEZONE, LAST_STEP
        };

        return tracker_generic_date_extractor (rfc_date, steps_to_do);
}


typedef gchar * (*PostProcessor) (gchar *);


static struct {
	gchar         *name;
	gchar         *type;
        PostProcessor post;

} tagmap[] = {
  { "Author",             "Image:Creator",      NULL},
  { "Creator",            "Image:Creator",      NULL},
  { "Description",        "Image:Description",  NULL},
  { "Comment",            "Image:Comments",     NULL},
  { "Copyright",          "File:Copyright",     NULL},
  { "Creation Time",      "Image:Date",         rfc1123_to_iso8160_date},
  { "Title",              "Image:Title",        NULL},
  { "Software",           "Image:Software",     NULL},
  { "Disclaimer",         "File:License",       NULL},
  { NULL,                 NULL,                 NULL},
};


void
tracker_extract_png (gchar *filename, GHashTable *metadata)
{
        gint        fd_png;
	FILE        *png;
	png_structp png_ptr;
	png_infop   info_ptr;
	png_uint_32 width, height;
	gint        num_text;
	png_textp   text_ptr;

	gint bit_depth, color_type;
	gint interlace_type, compression_type, filter_type;

#if defined(__linux__)
        if ((fd_png = g_open (filename, (O_RDONLY | O_NOATIME))) == -1) {
#else
        if ((fd_png = g_open (filename, O_RDONLY)) == -1) {
#endif
                return;
        }

	if ((png = fdopen(fd_png, "r"))) {
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
                        gint i;
			for (i = 0; i < num_text; i++) {
				if (text_ptr[i].key) {
                                        gint j;
					#if defined(HAVE_EXEMPI) && defined(PNG_iTXt_SUPPORTED)
					if (strcmp("XML:com.adobe.xmp", text_ptr[i].key) == 0) {
						tracker_read_xmp (text_ptr[i].text,
                                                                  text_ptr[i].itxt_length,
                                                                  metadata);
						continue;
					}
					#endif
	
					for (j = 0; tagmap[j].type; j++) {
						if (strcasecmp (tagmap[j].name, text_ptr[i].key) == 0) {
							if (text_ptr[i].text && text_ptr[i].text[0] != '\0') {
                                                                if (tagmap[j].post) {
                                                                        g_hash_table_insert (metadata,
                                                                                             g_strdup (tagmap[j].type),
                                                                                             (*tagmap[j].post) (text_ptr[i].text));
                                                                } else {
                                                                        g_hash_table_insert (metadata,
                                                                                             g_strdup (tagmap[j].type),
                                                                                             g_strdup (text_ptr[i].text));
                                                                }
                                                                break;
                                                        }
                                                }
					}
				}
			}
		}

		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		
		fclose (png);

        } else {
                close (fd_png);
        }
}
