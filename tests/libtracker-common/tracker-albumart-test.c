/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#include "config.h"

#include <glib.h>

#include <libtracker-common/tracker-albumart.h>
#include <tracker-test-helpers.h>

#define TEST_DIR "/temp/test"

static void
test_path_album_ok ()
{
	gchar *thumbnail = NULL;
	gchar *relative_art = NULL;

	const gchar *expected_thumb = "/temp/test/media-art/album-73f7b9fa0472cec82ca937f085fd2363-1611986091762e6273abda742ba8a7ee.jpeg";
	const gchar *expected_relative = "file:///home/user/test/.mediaartlocal/album-73f7b9fa0472cec82ca937f085fd2363-1611986091762e6273abda742ba8a7ee.jpeg";

        tracker_albumart_get_path ("Pink Floyd", "The Wall", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));
	
	g_free (thumbnail);
	g_free (relative_art);

	/* lower case */
        tracker_albumart_get_path ("pink floyd", "the wall", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));
	
	g_free (thumbnail);
	g_free (relative_art);

	/* upper case */
        tracker_albumart_get_path ("PINK FLOYD", "THE WALL", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));
	
	g_free (thumbnail);
	g_free (relative_art);

	/* Whitespaces */
        tracker_albumart_get_path ("PINK  FLOYD", "THE  WALL", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));
	
	g_free (thumbnail);
	g_free (relative_art);
}

static void
test_path_album_strip_paired_brackets ()
{
	gchar *expected_thumb = NULL;
	gchar *expected_relative = NULL;
	gchar *thumbnail = NULL;
	gchar *relative_art = NULL;

	/*
	 * Any combination of two (), {}, [], <> with anything written 
	 * inside of the two. For example (01), [1], [10], <etc>, {f (bar) oo} 
	 *must be stripped away 
	 */

	/* Ask the default paths */
        tracker_albumart_get_path ("evil artist", "evil album", "album",
				   "/home/user/test/song_1.mp3", 
				   &expected_thumb, &expected_relative);

        tracker_albumart_get_path ("evil artist[12]", "evil album(cb)", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));

	g_free (thumbnail);
	g_free (relative_art);


        tracker_albumart_get_path ("evil artist{1}", "evil album<with a>", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));

	g_free (thumbnail);
	g_free (relative_art);

	/* Nested symbols */
        tracker_albumart_get_path ("evil artist{here some <1>}", "evil album", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));
	
	g_free (thumbnail);
	g_free (relative_art);

	/* Nested repeated symbols */
        tracker_albumart_get_path ("evil artist", "evil album<aodif<ifj>>", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));
	
	g_free (thumbnail);
	g_free (relative_art);
}

static void
test_path_album_unwanted_chars ()
{
	gchar *expected_thumb = NULL;
	gchar *expected_relative = NULL;
	gchar *thumbnail = NULL;
	gchar *relative_art = NULL;

	/*
	 * The list of unwanted characters is as follows: ()_{}[]!@#$^&*+=|\/"'?<>~` 
	 */

	/* Ask the default paths */
        tracker_albumart_get_path ("evil artist", "evil album", "album",
				   "/home/user/test/song_1.mp3", 
				   &expected_thumb, &expected_relative);

        tracker_albumart_get_path ("evil artist!", "evil _album_", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));

	g_free (thumbnail);
	g_free (relative_art);

	/* Unpaired brackets */
        tracker_albumart_get_path ("[evil) }(artist!", "evil ]album{_", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));

	g_free (thumbnail);
	g_free (relative_art);

	/* Some symbols */
        tracker_albumart_get_path ("@@@evil@@@ |artist$", "^evil& >album<", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));

	g_free (thumbnail);
	g_free (relative_art);

	/* More Symbols */
        tracker_albumart_get_path ("~*evil*~ ++\"artist\"++", "=#evil 'album??", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));

	g_free (thumbnail);
	g_free (relative_art);

	/* Slash backslash -- ????*/
        tracker_albumart_get_path ("/evil/ \\artist\\", "\\_/evil /album\\", "album",
				   "/home/user/test/song_1.mp3", 
				   &thumbnail, &relative_art);

	g_assert (tracker_test_helpers_cmpstr_equal (thumbnail, expected_thumb));
	g_assert (tracker_test_helpers_cmpstr_equal (relative_art, expected_relative));

	g_free (thumbnail);
	g_free (relative_art);


}


static void
test_path_album_white_spaces ()
{

	/* FIXME complete */


}


gint
main (gint argc, gchar **argv)
{
        const gchar *previous_cache_dir;
        gint result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

        previous_cache_dir = g_getenv ("XDG_CACHE_HOME");
        g_setenv ("XDG_CACHE_HOME", TEST_DIR, TRUE);

        g_test_add_func ("/libtracker-common/tracker-albumart/path_album_ok",
                         test_path_album_ok);
	g_test_add_func ("/libtracker-common/tracker-albumart/path_album_strip_paired_brackets",
			 test_path_album_strip_paired_brackets);
	g_test_add_func ("/libtracker-common/tracker-albumart/path_album_unwanted_chars",
			 test_path_album_unwanted_chars);

	g_test_add_func ("/libtracker-common/tracker-albumart/path_album_white_",
			 test_path_album_white_spaces);

        result = g_test_run ();
        
        g_setenv ("XDG_CACHE_HOME", previous_cache_dir, TRUE);
        return result;
}
