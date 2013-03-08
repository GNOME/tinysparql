/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib-object.h>

#include <libtracker-extract/tracker-extract.h>

static void
test_exif_parse (void)
{
        TrackerExifData *exif;
        gchar *blob;
        gsize  length;


        g_assert (g_file_get_contents (TOP_SRCDIR "/tests/libtracker-extract/exif-img.jpg", &blob, &length, NULL));

        exif = tracker_exif_new (blob, length, "test://file");

        /* Ignored on purpose on the code (?) */
        //g_assert_cmpstr (exif->x_dimension, ==, );
        //g_assert_cmpstr (exif->y_dimenstion, ==, );
        //g_assert_cmpstr (exif->image_width, ==, );

        g_assert_cmpstr (exif->document_name, ==, "test-documentname");
        //g_assert_cmpstr (exif->time, ==, "test-documentname");
        g_assert (exif->time_original);
        g_assert_cmpstr (exif->artist, ==, "EXIFspec"); // -Exif:Artist
        g_assert_cmpstr (exif->user_comment, ==, "libexif demonstration image");
        g_assert_cmpstr (exif->description, ==, "Justfortest"); //-Exif:ImageDescription
        g_assert_cmpstr (exif->make, ==, "Nikon"); //-Exif:Make
        g_assert_cmpstr (exif->model, ==, "SD3000"); //-Exif:Model
        g_assert_cmpstr (exif->orientation, ==, "nfo:orientation-left-mirror"); //-n -Exif:Orientation=5
        g_assert_cmpstr (exif->exposure_time, ==, "0.002"); // -Exif:ExposureTime=1/500
        g_assert_cmpstr (exif->fnumber, ==, "5.6"); // -Exif:FNumber
        g_assert_cmpstr (exif->flash, ==, "nmm:flash-off"); // -n -Exif:Flash=88
        g_assert_cmpstr (exif->focal_length, ==, "35.0"); // -n -Exif:FocalLength=35
        g_assert_cmpstr (exif->iso_speed_ratings, ==, "400"); // -n -Exif:ISO=400
        g_assert_cmpstr (exif->metering_mode, ==, "nmm:metering-mode-multispot"); // -n -Exif:MeteringMode=4
        g_assert_cmpstr (exif->white_balance, ==, "nmm:white-balance-auto"); // -n -Exif:WhiteBalance=0
        g_assert_cmpstr (exif->copyright, ==, "From the exif demo with exiftool metadata"); // -Exif:Copyright
        g_assert_cmpstr (exif->software, ==, "bunchof1s"); // -Exif:Software
        g_assert_cmpstr (exif->x_resolution, ==, "72");
        g_assert_cmpstr (exif->y_resolution, ==, "72");
        g_assert_cmpint (exif->resolution_unit, ==, 2);

        g_assert_cmpstr (exif->gps_altitude, ==, "237.000000"); // -n -exif:gpsaltitude=237 
        g_assert_cmpstr (exif->gps_latitude, ==, "-42.500000"); // -exif:gpslatitude="42 30 0.00" -exif:gpslatituderef=S
        g_assert_cmpstr (exif->gps_longitude, ==, "-10.166675"); // -exif:gpslongitude="10 10 0.03" -exif:gpslongituderef=W
        g_assert_cmpstr (exif->gps_direction, ==, "12.3"); // -n -Exif:GPSImgDirection=12.3
        
        tracker_exif_free (exif);
}

static void
test_exif_parse_empty (void)
{
        TrackerExifData *exif;
        gchar *blob;
        gsize  length;

        g_assert (g_file_get_contents (TOP_SRCDIR "/tests/libtracker-extract/exif-free-img.jpg", &blob, &length, NULL));

        exif = tracker_exif_new (blob, length, "test://file");

        tracker_exif_free (exif);
}

int
main (int argc, char **argv) 
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/libtracker-extract/exif/parse",
                         test_exif_parse);
        g_test_add_func ("/libtracker-extract/exif/parse_empty",
                         test_exif_parse_empty);

        return g_test_run ();
}
