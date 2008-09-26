
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <png.h>

#include <glib.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#include <Windows.h>

#define _fullpath_internal(res,path,size) \
  (GetFullPathName ((path), (size), (res), NULL) ? (res) : NULL)
#define realpath(path,resolved_path) _fullpath_internal(resolved_path, path, MAX_PATH)
#endif

#ifndef LIBDIR
#define LIBDIR "/usr/lib"
#endif

static gboolean
create_thumbnails_dir (const gchar * const subdir)
{

	gchar *thumbnails_dir = NULL;
	const gchar *home_dir;

	home_dir = g_get_home_dir ();

	thumbnails_dir = g_build_filename (home_dir, ".thumbnails", NULL);

	/* Ensure that ~/.thumbnails is not a file if it exists */
	if (g_file_test (thumbnails_dir, G_FILE_TEST_EXISTS) &&
			!g_file_test (thumbnails_dir, G_FILE_TEST_IS_DIR)) {
		g_printerr ("%s exists but is not a directory.\n", thumbnails_dir);
		g_free (thumbnails_dir);
		return FALSE;
	}

	g_free (thumbnails_dir);

	thumbnails_dir = g_build_filename (home_dir, ".thumbnails", subdir, NULL);

	if (g_mkdir_with_parents (thumbnails_dir, 00775) == -1) {
		g_printerr ("failed: g_mkdir_with_parents (%s)\n", thumbnails_dir);
		g_free (thumbnails_dir);
		return FALSE;
	}

	g_free (thumbnails_dir);
	return TRUE;

}

static gboolean
valid_thumbnail_exists (const gchar *thumbnail_filename, const gchar *uri, const gchar *mtime)
{
	FILE	    *fp;
	png_structp  png_ptr;
	png_infop    info_ptr;
	gint	     i, count, tests_passed;
	png_textp    pngtext;

	if (g_file_test (thumbnail_filename, G_FILE_TEST_EXISTS)) {

		g_printerr ("%s exists ", thumbnail_filename);

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

		png_destroy_info_struct (png_ptr, &info_ptr);
		png_destroy_read_struct (&png_ptr, NULL, NULL);


		if (tests_passed == 2) {
			g_printerr ("and it is valid\n");
			return TRUE;
		}
		g_printerr ("but it is not valid. Recreating thumbnail...\n");
	}

	return FALSE;
}

static gboolean
generate_thumbnail (const gchar *filename, const gchar *thumbnail_filename, const gchar *mime, gint size)
{

	gchar	 *thumbnailer;
	gchar	 *args[5];
	gint	  i;
	gboolean  error = FALSE;

	/* do we have a thumbnailer for this mime type? */
	thumbnailer = g_strconcat (LIBDIR "/tracker/thumbnailers/",
				   mime, "_thumbnailer", NULL);

	if (!g_file_test (thumbnailer, G_FILE_TEST_EXISTS)) {
		g_printerr ("Thumbnailer '%s' for mime %s not found\n", thumbnailer, mime);
		g_free (thumbnailer);
		return FALSE;
	}

	/* execute the thumbnailer */
	args[0] = g_strdup (thumbnailer);
	args[1] = g_filename_from_utf8 (filename, -1, NULL, NULL, NULL);
	args[2] = g_strdup (thumbnail_filename);
	args[3] = g_strdup_printf ("%d", size);
	args[4] = NULL;

	if (!g_spawn_sync (NULL, args, NULL,
			   G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
			   NULL, NULL, NULL, NULL, NULL, NULL)) {

		g_printerr ("%s failed to create %s\n", thumbnailer, thumbnail_filename);
		g_remove (thumbnail_filename);
		error = TRUE;
	}

	for (i = 0; i < 4; i++) {
		g_free (args[i]);
	}
	g_free (thumbnailer);

	return !error;
}


static gboolean
ensure_correct_thumbnail (const gchar *uri, const gchar *thumbnail_filename, const gchar *mtime) {

	FILE	      *fp;
	png_structp    png_ptr;
	png_infop      info_ptr;
	png_textp      pngtext;
	png_uint_32    width, height;
	gint	       bit_depth, color_type, interlace_method;
	gint	       compression_method, filter_method;
	png_colorp     palette = NULL;
	gint	       num_palette;
	png_bytepp     row_pointers;
	guint	       i;

	/* the fd.o spec requires us to set certain PNG keys.  As far as I can
	 * see, libpng doesn't allow you to just set metadata, you have to do
	 * that when you create the file.  Therefore, we need to copy the newly
	 * created thumbnail into a new one while setting the required attributes
	 */
	g_assert ((fp = g_fopen (thumbnail_filename, "r")));
	g_assert ((png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)));
	g_assert ((info_ptr = png_create_info_struct (png_ptr)));

	if (setjmp (png_jmpbuf (png_ptr))) {
		g_printerr ("Error reading thumbnail\n");
		png_destroy_info_struct (png_ptr, &info_ptr);
		png_destroy_read_struct (&png_ptr, NULL, NULL);
		fclose (fp);
		g_remove (thumbnail_filename);
		return FALSE;
	}

	png_init_io (png_ptr, fp);
	png_read_info (png_ptr, info_ptr);

	g_assert ((1 == png_get_IHDR (png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_method, &compression_method, &filter_method)));

	png_get_PLTE (png_ptr, info_ptr, &palette, &num_palette);

	row_pointers = g_new (png_bytep, height);
	for (i = 0; i < height; i++) {
		row_pointers[i] = (png_bytep) g_malloc0 (png_get_rowbytes (png_ptr, info_ptr));
	}
	png_read_image (png_ptr, row_pointers);
	png_read_end (png_ptr, NULL);

	png_destroy_info_struct (png_ptr, &info_ptr);
	png_destroy_read_struct (&png_ptr, NULL, NULL);
	fclose (fp);

	/* write the new thumbnail (overwrites the old) */
	g_assert ((fp = g_fopen (thumbnail_filename, "w")));
	g_assert ((png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)));
	g_assert ((info_ptr = png_create_info_struct (png_ptr)));

	if (setjmp (png_jmpbuf (png_ptr))) {
		g_printerr ("Error creating thumbnail\n");
	} else {

		png_init_io (png_ptr, fp);

		png_set_IHDR (png_ptr, info_ptr, width, height, bit_depth,
			      color_type, interlace_method, compression_method, filter_method);

		if (palette && num_palette > 0)
			png_set_PLTE (png_ptr, info_ptr, palette, num_palette);

		/* set the required fields */
		pngtext = g_new0 (png_text, 3);
		pngtext[0].key = "Thumb::URI";
		pngtext[0].text = g_strdup (uri);
		pngtext[0].text_length = strlen (uri);
		pngtext[0].compression = PNG_TEXT_COMPRESSION_NONE;
		pngtext[1].key = "Thumb::MTime";
		pngtext[1].text = g_strdup (mtime);
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

		g_free (pngtext[0].text);
		g_free (pngtext[1].text);
		g_free (pngtext);
	}
	fclose (fp);

	for (i = 0; i < height; i++) {
		g_free (row_pointers[i]);
	}

	g_free (row_pointers);

	png_destroy_info_struct (png_ptr, &info_ptr);
	png_destroy_write_struct (&png_ptr, NULL);

	return TRUE;
}

static gboolean
validate_args (gint argc, gchar **argv)
{
	if (argc < 4) {
		return FALSE;
	}

	if (!strcmp (argv[3], "normal")
	    || !strcmp (argv[3], "large")
	    || !strcmp (argv[3], "preview")) {
		return TRUE;
	}

	return FALSE;
}


/* argv[1] == full path of file to be nailed
 * argv[2] == mime type of said file
 * argv[3] == requested size: "normal", "large", "preview"
 */

gint
main (gint argc, gchar *argv[])
{
	gchar	      *uri;
	gchar	       realname[MAXPATHLEN];
	struct stat    stat_info;
	gchar	      *mtime;
	gchar	      *uri_hash;
	GChecksum     *checksum;
	gchar	      *thumbnail_filename;

	if (!validate_args (argc, argv)) {
		g_printerr ("Usage: ./tracker-thumbnailer filename mimetype [normal|large|preview]\n");
		return EXIT_FAILURE;
	}

	/* only make normal size thumbnails for now */
	if (strcmp (argv[3], "normal") != 0) {
		g_printerr ("Only normal sized thumbnails are supported\n");
		return EXIT_FAILURE;
	}

	if (!create_thumbnails_dir (argv[3]))
		return EXIT_FAILURE;

	/* make sure the actual file exists */
	if (!g_file_test (argv[1], G_FILE_TEST_EXISTS)) {
		g_printerr ("File '%s' does not exist\n", argv[1]);
		return EXIT_FAILURE;
	}

	/* g_filename_to_uri needs an absolute filename */
	if (!realpath (argv[1], realname)) {
		g_printerr("could not resolve absolute pathname for %s\n", argv[1]);
		return EXIT_FAILURE;
	}

	/* convert file name to URI */
	uri = g_filename_to_uri (realname, NULL, NULL);


	/* create path to thumbnail */
	checksum = g_checksum_new (G_CHECKSUM_MD5);
	g_checksum_update (checksum, (guchar *)uri, strlen (uri));
	uri_hash = g_strconcat (g_checksum_get_string (checksum), ".png", NULL);
	g_checksum_free (checksum);


	thumbnail_filename = g_build_filename (g_get_home_dir (), ".thumbnails", argv[3], uri_hash, NULL);
	g_free (uri_hash);

	/* get stat information on the file */
	if (g_stat (argv[1], &stat_info) == -1) {
		g_printerr ("stat () failed for %s\n", argv[1]);
		g_free (thumbnail_filename);
		g_free (uri);
		return EXIT_FAILURE;
	}
	mtime = g_strdup_printf ("%lu", stat_info.st_mtime);

	/* check to see if the thumbnail already exists */
	if (valid_thumbnail_exists (thumbnail_filename, uri, mtime)) {
		g_print ("%s\n", thumbnail_filename);
		g_free (mtime);
		g_free (thumbnail_filename);
		g_free (uri);
		return EXIT_SUCCESS;
	}

	if (!generate_thumbnail (uri, thumbnail_filename, argv[2], 128)) {
		g_free (mtime);
		g_free (thumbnail_filename);
		g_free (uri);
		return EXIT_FAILURE;
	}

	if (g_file_test (thumbnail_filename, G_FILE_TEST_EXISTS)) {
		ensure_correct_thumbnail (uri, thumbnail_filename, mtime);
	} else {
		g_free (mtime);
		g_free (thumbnail_filename);
		g_free (uri);
		return EXIT_FAILURE;
	}

	/* if we get here, everything must have succeeded */
	g_print ("%s\n", thumbnail_filename);
	g_free (thumbnail_filename);
	g_free (uri);
	g_free (mtime);
	return EXIT_SUCCESS;
}
