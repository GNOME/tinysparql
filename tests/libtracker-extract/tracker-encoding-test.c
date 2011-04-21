/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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
 *
 */

#include "config.h"

#include <glib-object.h>
#include <libtracker-extract/tracker-encoding.h>

static void
test_encoding_guessing ()
{
	gchar *output;
	GMappedFile *file = NULL;
	gchar *prefix, *filen;

	prefix = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "libtracker-extract", NULL);
	filen = g_build_filename (prefix, "encoding-detect.bin", NULL);

	file = g_mapped_file_new (filen, FALSE, NULL);

	output = tracker_encoding_guess (g_mapped_file_get_contents (file),
	                                 g_mapped_file_get_length (file));

	g_assert_cmpstr (output, ==, "UTF-8");

#if GLIB_CHECK_VERSION(2,22,0)
	g_mapped_file_unref (file);
#else
	g_mapped_file_free (file);
#endif

	g_free (prefix);
	g_free (filen);
	g_free (output);
}


int
main (int argc, char **argv)
{
	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/libtracker-extract/tracker-encoding/encoding_guessing",
	                 test_encoding_guessing);

	return g_test_run ();
}
