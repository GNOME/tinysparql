
/* tracker-thumbnailer
 * Copyright (C) 2006, Edward Duffy <eduffy@gmail.com>
 *
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



#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <png.h>
#include "md5.h"

#ifndef LIBDIR
#define LIBDIR "/usr/lib"
#endif

/* argv[1] == full path of file to be nailed
 * argv[2] == mime type of said file
 * argv[3] == requested size: "normal", "large", "preview"
 */

int main (int argc, char *argv[])
{
	gchar         *uri;
	struct stat    stat_info;
	gchar         *mtime;
	md5_state_t    hash_state;
	guchar         hash[16];
	gchar          uri_hash[20], *p;
	int	       i;
	guint32        j;
	gchar         *thumbnail_filename;
	FILE          *fp;
	png_structp    png_ptr;
	png_infop      info_ptr;
	png_textp      pngtext;
	int            count, tests_passed;
	gchar         *thumbnailer;
	gchar         *args[5];
	png_uint_32    width, height;
	int            bit_depth, color_type, interlace_method;
	int            compression_method, filter_method;
	png_colorp     palette;
	int            num_palette;
	png_bytepp     row_pointers;

	/* only make normal size thumbnails for now */
	if (strcmp (argv[3], "normal") != 0) {
		g_printerr ("Only normal sized thumbnails are supported\n");
		return EXIT_FAILURE;
	}

	/* make sure the actual file exists */
	if (!g_file_test (argv[1], G_FILE_TEST_EXISTS)) {
		g_printerr ("%s does not exist\n", argv[1]);
		return EXIT_FAILURE;
	}

	/* convert file name to URI */
	uri = g_filename_to_uri (argv[1], NULL, NULL);

	/* get stat information on the file */
	if (g_stat (argv[1], &stat_info) == -1) {
		g_printerr ("stat () failed for %s\n", argv[1]);
		return EXIT_FAILURE;
	}
	mtime = g_strdup_printf ("%lu", stat_info.st_mtime);

	/* create path to thumbnail */
	md5_init (&hash_state);
	md5_append (&hash_state, (guchar *)uri, strlen (uri));
	md5_finish (&hash_state, hash);
	p = uri_hash;
	for (i = 0; i < 16; i++) {
		g_sprintf (p, "%02x", hash[i]);
		p += 2;
	}
	g_sprintf (p, ".png");
	thumbnail_filename = g_build_filename (
		g_get_home_dir (), ".thumbnails", argv[3], uri_hash, NULL);

	/* check to see if the thumbnail already exists */
	if (g_file_test (thumbnail_filename, G_FILE_TEST_EXISTS)) {
		g_printerr ("%s exists\n", thumbnail_filename);

		/* thumbnail exists; but is it valid? */
		g_assert ((fp = g_fopen (thumbnail_filename, "r")));
		g_assert ((png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)));
		g_assert ((info_ptr = png_create_info_struct (png_ptr)));
		png_init_io (png_ptr, fp);
		png_read_info (png_ptr, info_ptr);
		i = png_get_text (png_ptr, info_ptr, &pngtext, &count);
		tests_passed = 0;
		for (i = 0; i < count; i++) {
			if (strcmp (pngtext[i].key, "Thumb::URI") == 0 && strcmp (pngtext[i].text, uri) == 0) {
				++tests_passed;
			}
			else if (strcmp (pngtext[i].key, "Thumb::MTime") == 0 && strcmp (pngtext[i].text, mtime) == 0) {
				++tests_passed;
			}
		}
		fclose (fp);
		if (tests_passed == 2) {
			g_printerr ("Thumbnail valid\n");
			g_print ("%s\n", thumbnail_filename);
			return EXIT_SUCCESS;
		}
		g_printerr ("Not all tests passed.  Recreating thumbnail...\n");
	}
	/* thumbnail either doesn't exist or is invalid; contiue... */

	/* do we have a thumbnailer for this mime type? */
	thumbnailer = g_strconcat (LIBDIR "/tracker/thumbnailers/",
	                           argv[2], "_thumbnailer", NULL);

	if (!g_file_test (thumbnailer, G_FILE_TEST_EXISTS)) {
		g_printerr ("%s not found\n", thumbnailer);
		return EXIT_FAILURE;
	}

	/* execute the thumbnailer */
	args[0] = thumbnailer;
	args[1] = g_filename_from_utf8 (argv[1], -1, NULL, NULL, NULL);
	args[2] = thumbnail_filename;
	if (strcmp (argv[3], "normal") == 0)
		args[3] = g_strdup ("128");
	args[4] = NULL;

	if (!g_spawn_sync (NULL, args, NULL,
	                   G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
	                   NULL, NULL, NULL, NULL, NULL, NULL)) {

		g_printerr ("%s failed to create %s\n", thumbnailer, thumbnail_filename);
		g_remove (thumbnail_filename);
		return EXIT_FAILURE;
	}

	/* the fd.o spec requires us to set certain PNG keys.  As far as I can
	 * see, libpng doesn't allow you to just set metadata, you have to do
	 * that when you create the file.  Therefore, we need to copy the newly
	 * created thumbnail into a new one while setting the required attributes
	 */

	/* read in the thumbnail into a buffer */
	g_assert ((fp = g_fopen (thumbnail_filename, "r")));
	g_assert ((png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)));
	g_assert ((info_ptr = png_create_info_struct (png_ptr)));

	if (setjmp (png_jmpbuf (png_ptr))) {
		g_printerr ("Error reading thumbnail\n");
		fclose (fp);
		g_remove (thumbnail_filename);
		return EXIT_FAILURE;
	}

	png_init_io (png_ptr, fp);
	png_read_info (png_ptr, info_ptr);

	g_assert ((1 == png_get_IHDR (png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_method, &compression_method, &filter_method)));

	png_get_PLTE (png_ptr, info_ptr, &palette, &num_palette);

	row_pointers = g_new (png_bytep, height);
	for (j = 0; j < height; j++) {
		row_pointers[j] = (png_bytep) malloc (png_get_rowbytes (png_ptr, info_ptr));
	}
	png_read_image (png_ptr, row_pointers);
	png_read_end (png_ptr, NULL);
	fclose (fp);

	/* write the new thumbnail (overwrites the old) */
	g_assert ((fp = g_fopen (thumbnail_filename, "w")));
	g_assert ((png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)));
	g_assert ((info_ptr = png_create_info_struct (png_ptr)));

	if (setjmp (png_jmpbuf (png_ptr))) {
		g_printerr ("Error creating thumbnail\n");
		fclose (fp);
		g_remove (thumbnail_filename);
		return EXIT_FAILURE;
	}

	png_init_io (png_ptr, fp);

	png_set_IHDR (png_ptr, info_ptr, width, height, bit_depth, color_type, interlace_method, compression_method, filter_method);

	if (palette && num_palette > 0)
		png_set_PLTE (png_ptr, info_ptr, palette, num_palette);

	/* set the required fields */
	pngtext = g_new0 (png_text, 3);
	pngtext[0].key = "Thumb::URI";
	pngtext[0].text = uri;
	pngtext[0].text_length = strlen (uri);
	pngtext[0].compression = PNG_TEXT_COMPRESSION_NONE;
	pngtext[1].key = "Thumb::MTime";
	pngtext[1].text = mtime;
	pngtext[1].text_length = strlen (mtime);
	pngtext[1].compression = PNG_TEXT_COMPRESSION_NONE;
	/* set some optional fields */
	pngtext[2].key = "Software";
	pngtext[2].text = "Tracker thumbnail factory";
	pngtext[2].compression = PNG_TEXT_COMPRESSION_NONE;
	png_set_text(png_ptr, info_ptr, pngtext, 3);

	png_write_info (png_ptr, info_ptr);
	png_write_image (png_ptr, row_pointers);
	png_write_end (png_ptr, info_ptr);
	fclose (fp);

	/* if we get here, everything must have succeeded */
	g_print ("%s\n", thumbnail_filename);
	return EXIT_SUCCESS;
}
