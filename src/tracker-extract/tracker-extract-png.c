
#include "config.h"

#ifdef HAVE_LIBPNG

#include <stdio.h>
#include <glib.h>
#include <png.h>

static struct {
	char *name;
	char *type;
} tagmap[] = {
	{ "Author" , "Image.Creator"},
	{ "Creator" , "Image.Creator"},
   	{ "Description" , "Image.Description"},
   	{ "Comment", "Image.Comments"},
   	{ "Copyright", "Image.Copyright"},
   	{ "Creation Time", "Image.Date"},
   	{ "Title", "Image.Title"},
   	{ "Software", "Image.Software"},
   	{ "Disclaimer", "Image.License"},
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
			g_hash_table_insert (metadata, g_strdup ("Image.Width"),
			                     g_strdup_printf ("%ld", width));
			g_hash_table_insert (metadata, g_strdup ("Image.Height"),
			                     g_strdup_printf ("%ld", height));
		}


		if (png_get_text (png_ptr, info_ptr, &text_ptr, &num_text) > 0) {
		    	for (i = 0; i < num_text; i++) {
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

		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		
		fclose (png);
	}
}

#else
#warning "Not building PNG metadata extractor."
#endif  /* HAVE_LIBPNG */
