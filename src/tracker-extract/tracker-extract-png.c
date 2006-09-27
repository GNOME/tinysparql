
#include "config.h"

#ifdef HAVE_LIBPNG

#include <stdio.h>
#include <glib.h>
#include <png.h>

void
tracker_extract_png (gchar *filename, GHashTable *metadata)
{
	FILE *png;
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;

	int bit_depth, color_type;
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
		if (png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
		                 &color_type, &interlace_type, &compression_type, &filter_type)) {
			g_hash_table_insert (metadata, g_strdup ("Image.Width"),
			                     g_strdup_printf ("%ld", width));
			g_hash_table_insert (metadata, g_strdup ("Image.Height"),
			                     g_strdup_printf ("%ld", height));
		}

		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		fclose (png);
	}
}

#else
#warning "Not building PNG metadata extractor."
#endif  /* HAVE_LIBPNG */
