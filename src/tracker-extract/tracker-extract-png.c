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

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <png.h>


static gchar *
rfc1123_to_iso8160_date (const gchar *rfc_date)
{
        /* ex. RFC1123 date: "22 May 1997 18:07:10 -0600"
           ex. ISO8160 date: "2007-05-22T18:07:10-0600"
        */
        gchar buffer[20];
        gchar timezone_buffer[6];
        gchar **date_parts, **part;
        size_t count;

        g_return_val_if_fail (rfc_date, NULL);

        typedef enum {DAY_STR = 0, DAY, MONTH, YEAR, TIME, TIMEZONE, LAST_STEP} steps;
        steps step;

        struct tm tm;
        memset (&tm, 0, sizeof (struct tm));

        date_parts = g_strsplit (rfc_date, " ", 0);

        for (part = date_parts, step = DAY_STR; *part && step != LAST_STEP; part++, step++) {
                switch (step) {
                        case DAY_STR: {
                                /* We do not care about monday, tuesday, etc. */
                                break;
                        }
                        case DAY: {
                                guint64 val = g_ascii_strtoull (*part, NULL, 10);
                                tm.tm_mday = CLAMP (val, 1, 31);
                                break;
                        }
                        case MONTH: {
                                gchar *months[] = {
                                        "Jan", "Fe", "Mar", "Av", "Ma", "Jun",
                                        "Jul", "Au", "Se", "Oc", "No", "De",
                                        NULL };
                                gchar **tmp;
                                gint i;

                                for (tmp = months, i = 0; *tmp; tmp++, i++) {
                                        if (g_str_has_prefix (*part, *tmp)) {
                                                tm.tm_mon = i;
                                                break;
                                        }
                                }
                                break;
                        }
                        case YEAR : {
                                guint64 val = g_ascii_strtoull (*part, NULL, 10);
                                tm.tm_year = CLAMP (val, 0, G_MAXINT) - 1900;
                                break;
                        }
                        case TIME: {
                                gchar *n = *part;

                                #define READ_PAIR(ret, min, max)                        \
                                {                                                       \
                                        gchar buff[3];                                  \
                                        guint64 val;                                    \
                                        buff[0] = n[0];                                 \
                                        buff[1] = n[1];                                 \
                                        buff[2] = '\0';                                 \
                                                                                        \
                                        val = g_ascii_strtoull (buff, NULL, 10);        \
                                        ret = CLAMP (val, min, max);                    \
                                        n += 2;                                         \
                                }

                                READ_PAIR (tm.tm_hour, 0, 24);
                                if (*n++ != ':') {
                                        goto error;
                                }
                                READ_PAIR (tm.tm_min, 0, 99);
                                if (*n++ != ':') {
                                        goto error;
                                }
                                READ_PAIR (tm.tm_sec, 0, 99);

                                break;
                        }
                        case TIMEZONE: {
                                gchar *timezone_part = *part;

                                /* checks that we are not reading word "GMT" instead of timezone */
                                if (timezone_part[0] && g_ascii_isdigit (timezone_part[1])) {
                                        gchar *n = timezone_part + 1;
                                        gint hours, minutes;

                                        if (strlen (n) < 4) {
                                                goto error;
                                        }

                                        READ_PAIR (hours, 0, 24);
                                        /* that should not happen, but he... */
                                        if (*n == ':') {
                                                n++;
                                        }
                                        READ_PAIR (minutes, 0, 99);

                                        g_sprintf (timezone_buffer, "%c%.2d%.2d",
                                                   (timezone_part[0] == '-' ? '-' : '+'),
                                                   hours, minutes);
                                }

                                #undef READ_PAIR

                                break;
                        }
                        default:
                                /* that cannot happen! */
                                g_strfreev (date_parts);
                                g_return_val_if_reached (NULL);
                }
        }

        count = strftime (buffer, sizeof (buffer), "%FT%T", &tm);

        g_strfreev (date_parts);

        if (count > 0) {
                return g_strconcat (buffer, timezone_buffer, NULL);
        } else {
                return NULL;
        }

 error:
        g_strfreev (date_parts);
        return NULL;
}


typedef gchar * (*PostProcessor) (const gchar *);


static struct {
	gchar *name;
	gchar *type;
        PostProcessor  post;
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
	FILE *png;
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	gint num_text;
	png_textp text_ptr;

	gint bit_depth, color_type;
	gint interlace_type, compression_type, filter_type;

	if ((png = g_fopen(filename, "r"))) {
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
        }
}
