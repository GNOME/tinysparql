/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
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

#include <glib-object.h>

#include <libtracker-common/tracker-common.h>

struct {
        const gchar *input;
        const gchar *expected_output;
} strip_test_cases [] = {
        { "nothing to strip here", "nothing to strip here" },
        { "Upper Case gOEs dOwN", "upper case goes down"},
        { "o", "o"},
        { "A", "a"},
        { "cool album (CD1)", "cool album"},
        { "cool album [CD1]", "cool album"},
        { "cool album {CD1}", "cool album"},
        { "cool album <CD1>", "cool album"},
        { " ", ""},
        { "     a     ", "a"},
        { "messy #title & stuff?", "messy title stuff"},
        { "Unbalanced [brackets", "unbalanced brackets" },
        { "Unbalanced (brackets", "unbalanced brackets" },
        { "Unbalanced <brackets", "unbalanced brackets" },
        { "Unbalanced brackets)", "unbalanced brackets" },
        { "Unbalanced brackets]", "unbalanced brackets" },
        { "Unbalanced brackets>", "unbalanced brackets" },
        { "Live at *WEMBLEY* dude!", "live at wembley dude" },
        { NULL, NULL}
};

static void
test_albumart_stripping (void)
{
        gint i;
        gchar *result;

        for (i = 0; strip_test_cases[i].input != NULL; i++) {
                result = tracker_media_art_strip_invalid_entities (strip_test_cases[i].input);
                g_assert_cmpstr (result, ==, strip_test_cases[i].expected_output);
                g_free (result);
        }

        g_print ("(%d test cases) ", i);
}

static void
test_albumart_stripping_null (void)
{
        // FIXME: Decide what is the expected behaviour here...
        //   a. Return NULL
        //   b. Return ""
        //g_assert (!tracker_albumart_strip_invalid_entities (NULL));
}

struct {
        const gchar *artist;
        const gchar *album;
        const gchar *filename;
} albumart_test_cases [] = {
        {"Beatles", "Sgt. Pepper", 
         "album-2a9ea35253dbec60e76166ec8420fbda-cfba4326a32b44b8760b3a2fc827a634.jpeg"},

        { "", "sgt. pepper",
          "album-d41d8cd98f00b204e9800998ecf8427e-cfba4326a32b44b8760b3a2fc827a634.jpeg"},

        { " ", "sgt. pepper",
          "album-d41d8cd98f00b204e9800998ecf8427e-cfba4326a32b44b8760b3a2fc827a634.jpeg"},

        { NULL, "sgt. pepper",
          "album-cfba4326a32b44b8760b3a2fc827a634-7215ee9c7d9dc229d2921a40e899ec5f.jpeg"}, 

        { "Beatles", NULL,
          "album-2a9ea35253dbec60e76166ec8420fbda-7215ee9c7d9dc229d2921a40e899ec5f.jpeg"},

        { NULL, NULL, NULL }
};

static void
test_albumart_location (void)
{
        gchar *path = NULL, *local_uri = NULL;
        gchar *expected;
        gint i;
     
        for (i = 0; albumart_test_cases[i].filename != NULL; i++) {
                tracker_media_art_get_path (albumart_test_cases[i].artist,
                                            albumart_test_cases[i].album,
                                            "album",
                                            "file:///home/test/a.mp3",
                                            &path,
                                            &local_uri);
                expected = g_build_path (G_DIR_SEPARATOR_S, 
                                         g_get_user_cache_dir (),
                                         "media-art",
                                         albumart_test_cases[i].filename, 
                                         NULL);
                g_assert_cmpstr (path, ==, expected);
                
                g_free (expected);
                g_free (path);
                g_free (local_uri);
        }
        g_print ("(%d test cases) ", i);


}

static void
test_albumart_location_null (void)
{
        gchar *path = NULL, *local_uri = NULL;

        /* NULL parameters */
        tracker_media_art_get_path (NULL, NULL, "album", "file:///a/b/c.mp3", &path, &local_uri);
        g_assert (!path && !local_uri);
}

static void
test_albumart_location_path (void)
{
        gchar *path = NULL, *local_uri = NULL;
        gchar *expected;

        /* Use path instead of URI */
        tracker_media_art_get_path (albumart_test_cases[0].artist,
                                    albumart_test_cases[0].album,
                                    "album",
                                    "/home/test/a.mp3",
                                    &path,
                                    &local_uri);
        expected = g_build_path (G_DIR_SEPARATOR_S, 
                                 g_get_user_cache_dir (),
                                 "media-art",
                                 albumart_test_cases[0].filename, 
                                 NULL);
        g_assert_cmpstr (path, ==, expected);
                
        g_free (expected);
        g_free (path);
        g_free (local_uri);
}

gint
main (gint argc, gchar **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/libtracker-common/albumart/stripping",
                         test_albumart_stripping);
        g_test_add_func ("/libtracker-common/albumart/stripping_null",
                         test_albumart_stripping_null);
        g_test_add_func ("/libtracker-common/albumart/location",
                         test_albumart_location);
        g_test_add_func ("/libtracker-common/albumart/location_null",
                         test_albumart_location_null);
        g_test_add_func ("/libtracker_common/albumart/location_path",
                         test_albumart_location_path);

        return g_test_run ();
}
