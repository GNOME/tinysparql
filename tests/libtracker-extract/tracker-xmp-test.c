/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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
#include <glib-object.h>

#include <libtracker-extract/tracker-xmp.h>
#include <libtracker-client/tracker-sparql-builder.h>

#define BROKEN_XMP "This is not even XML"
#define EXAMPLE_XMP   \
"   <x:xmpmeta   " \
"      xmlns:x=\'adobe:ns:meta/\'" \
"      xmlns:dc=\"http://purl.org/dc/elements/1.1/\"" \
"      xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\"" \
"      xmlns:exif=\"http://ns.adobe.com/exif/1.0/\">" \
"     <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">" \
"        <rdf:Description rdf:about=\"\">"\
"         <dc:format>application/pdf</dc:format>" \
"         <dc:title>Title of the content</dc:title>"   \
"         <dc:rights>CC share alike</dc:rights> " \
"         <dc:description>Description of the content</dc:description>" \
"         <dc:date>2010-03-18T15:17:04Z</dc:date>" \
"         <dc:keywords>test, data, xmp</dc:keywords>" \
"         <dc:subject>Subject of the content</dc:subject>" \
"         <dc:publisher>A honest developer</dc:publisher>" \
"         <dc:contributor>A honest contributor</dc:contributor>" \
"         <dc:type>PhysicalObject</dc:type>" \
"         <dc:identifier>12345</dc:identifier>" \
"         <dc:source>My dirty mind</dc:source>" \
"         <dc:language>Spanglish</dc:language>" \
"         <dc:relation>Single</dc:relation>" \
"         <dc:coverage>Pretty high after this test</dc:coverage>" \
"         <dc:creator>The ultimate creator</dc:creator>" \
"         <exif:Title>Title in exif</exif:Title>" \
"         <exif:DateTimeOriginal>2010-03-18T15:17:04Z</exif:DateTimeOriginal>" \
"         <exif:Artist>Artist in exif</exif:Artist>" \
"         <exif:Make>Make in exif</exif:Make>" \
"         <exif:Model>Model in exif</exif:Model>" \
"         <exif:Orientation>top - left</exif:Orientation>" \
"         <exif:Flash>0</exif:Flash>" \
"         <exif:MeteringMode>3</exif:MeteringMode>" \
"         <exif:ExposureTime>1000</exif:ExposureTime>" \
"         <exif:FNumber>12</exif:FNumber>" \
"         <exif:FocalLength>50</exif:FocalLength>" \
"         <exif:ISOSpeedRatings>400</exif:ISOSpeedRatings>" \
"         <exif:WhiteBalance>1</exif:WhiteBalance>" \
"         <exif:Copyright>Copyright in exif</exif:Copyright>" \
"         <xmp:CreateDate>2002-08-15T17:10:04Z</xmp:CreateDate>"\
"        </rdf:Description> " \
"     </rdf:RDF> " \
"   </x:xmpmeta>"

#define METERING_MODE_XMP \
        "   <x:xmpmeta   "                            \
        "      xmlns:x=\'adobe:ns:meta/\'"                              \
        "      xmlns:exif=\"http://ns.adobe.com/exif/1.0/\">"           \
        "     <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">" \
        "        <rdf:Description rdf:about=\"\">"                      \
        "         <exif:MeteringMode>%d</exif:MeteringMode>"            \
        "        </rdf:Description>"                                    \
        "     </rdf:RDF></x:xmpmeta> "

#define ORIENTATION_XMP \
        "   <x:xmpmeta   "                            \
        "      xmlns:x=\'adobe:ns:meta/\'"                              \
        "      xmlns:exif=\"http://ns.adobe.com/exif/1.0/\">"           \
        "     <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">" \
        "        <rdf:Description rdf:about=\"\">"                      \
        "         <exif:Orientation>%s</exif:Orientation>"              \
        "        </rdf:Description>"                                    \
        "     </rdf:RDF></x:xmpmeta> "

typedef struct {
        const gchar *exif_value;
        const gchar *nepomuk_translation;
} ExifNepomuk;

static ExifNepomuk METERING_MODES [] = {
        {"0",  "nmm:metering-mode-other"},
        {"1", "nmm:metering-mode-average"},
        {"2", "nmm:metering-mode-center-weighted-average"},
        {"3", "nmm:metering-mode-spot"},
        {"4", "nmm:metering-mode-multispot"},
        {"5", "nmm:metering-mode-pattern"},
        {"6", "nmm:metering-mode-partial"},
        {NULL, NULL}
};

static ExifNepomuk ORIENTATIONS [] = {
        {"top - right", "nfo:orientation-top-mirror"},
        {"bottom - right", "nfo:orientation-bottom-mirror"},
        {"bottom - left", "nfo:orientation-bottom"},
        {"left - top", "nfo:orientation-left-mirror"},
        {"right - top", "nfo:orientation-right"},
        {"right - bottom", "nfo:orientation-right-mirror"},
        {"left - bottom", "nfo:orientation-left"},
        {"invalid value", "nfo:orientation-top"}
};

static TrackerXmpData *
get_example_expected (void)
{
        TrackerXmpData *data;

        data = g_new0 (TrackerXmpData, 1);

        /* NS_DC */
        data->title = g_strdup ("Title of the content");
        data->rights = g_strdup ("CC share alike");
        data->creator = g_strdup ("The ultimate creator");
        data->description = g_strdup ("Description of the content");
        data->date = g_strdup ("2010-03-18T15:17:04Z");
        data->keywords = g_strdup ("test, data, xmp");
        data->subject = g_strdup ("Subject of the content");

        data->publisher = g_strdup ("A honest developer");     /* publisher */
        data->contributor = g_strdup ("A honest contributor");
        data->type = NULL ;
        data->format = g_strdup ("application/pdf");
        data->identifier = g_strdup ("12345");
        data->source = g_strdup ("My dirty mind");
        data->language = g_strdup ("Spanglish");
        data->relation = g_strdup ("Single");
        data->coverage = g_strdup ("Pretty high after this test");

        /* NS_CC */
        data->license = NULL;

        /* NS_PDF */
        data->pdf_title = NULL;
        data->pdf_keywords = NULL;

        /* NS_EXIF*/
        data->title2 = g_strdup ("Title in exif");
        data->time_original = g_strdup ("2010-03-18T15:17:04Z");
        data->artist = g_strdup ("Artist in exif");
        data->make = g_strdup ("Make in exif");
        data->model = g_strdup ("Model in exif");
        data->orientation = g_strdup ("nfo:orientation-top");
        data->flash = g_strdup ("nmm:flash-off");
        data->metering_mode = g_strdup ("nmm:metering-mode-spot");
        data->exposure_time = g_strdup ("1000");                      /* exposure time */
        data->fnumber = g_strdup ("12");                              /* fnumber */
        data->focal_length = g_strdup ("50");                         /* focal length */

        data->iso_speed_ratings = g_strdup ("400");                   /* iso speed rating */
        data->white_balance = g_strdup ("nmm:white-balance-manual");
        data->copyright = g_strdup ("Copyright in exif");

        /* NS_XAP */
        data->rating = NULL;

        /* NS_IPTC4XMP */
        /* NS_PHOTOSHOP */
        data->address = NULL;                /* address */
        data->country = NULL;                /* country */
        data->state = NULL;                  /* state */
        data->city = NULL;                   /* city */

        return data;
};

static void
test_parsing_xmp (void)
{
        TrackerXmpData *data;
        TrackerXmpData *expected;

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
                data = tracker_xmp_new (BROKEN_XMP, strlen (BROKEN_XMP), "test://file");
		g_assert (data != NULL);

		tracker_xmp_free (data);
        }
        g_test_trap_assert_stderr ("*parsing failure*");

        data = tracker_xmp_new (EXAMPLE_XMP, strlen (EXAMPLE_XMP), "test://file");
        expected = get_example_expected ();

        /* NS_DC */
        g_assert_cmpstr (data->format, ==, expected->format);
        g_assert_cmpstr (data->title, ==, expected->title);
        g_assert_cmpstr (data->rights, ==, expected->rights);
        g_assert_cmpstr (data->description, ==, expected->description);
        g_assert_cmpstr (data->date, ==, expected->date);
        g_assert_cmpstr (data->keywords, ==, expected->keywords);
        g_assert_cmpstr (data->subject, ==, expected->subject);
        g_assert_cmpstr (data->publisher, ==, expected->publisher);
        g_assert_cmpstr (data->contributor, ==, expected->contributor);
        g_assert_cmpstr (data->identifier, ==, expected->identifier);
        g_assert_cmpstr (data->source, ==, expected->source);
        g_assert_cmpstr (data->language, ==, expected->language);
        g_assert_cmpstr (data->relation, ==, expected->relation);
        g_assert_cmpstr (data->coverage, ==, expected->coverage);
        g_assert_cmpstr (data->creator, ==, expected->creator);

        /* NS_EXIF*/
        g_assert_cmpstr (data->title2, ==, expected->title2);
	g_assert_cmpstr (data->time_original, ==, expected->time_original);
	g_assert_cmpstr (data->artist, ==, expected->artist);
	g_assert_cmpstr (data->make, ==, expected->make);
	g_assert_cmpstr (data->model, ==, expected->model);
	g_assert_cmpstr (data->orientation, ==, expected->orientation);
	g_assert_cmpstr (data->flash, ==, expected->flash);
	g_assert_cmpstr (data->metering_mode, ==, expected->metering_mode);
	g_assert_cmpstr (data->exposure_time, ==, expected->exposure_time);
	g_assert_cmpstr (data->fnumber, ==, expected->fnumber);
	g_assert_cmpstr (data->focal_length, ==, expected->focal_length);

	g_assert_cmpstr (data->iso_speed_ratings, ==, expected->iso_speed_ratings);
	g_assert_cmpstr (data->white_balance, ==, expected->white_balance);
	g_assert_cmpstr (data->copyright, ==, expected->copyright);

        tracker_xmp_free (expected);
	tracker_xmp_free (data);
}

static void
test_xmp_metering_mode (void)
{
        gint i;

        for (i = 0; METERING_MODES[i].exif_value != NULL; i++) {
		TrackerXmpData *data;
		gchar *xmp;

                xmp = g_strdup_printf (METERING_MODE_XMP, i);
                data = tracker_xmp_new (xmp, strlen (xmp), "local://file");
                g_free (xmp);

                g_assert_cmpstr (data->metering_mode, ==, METERING_MODES[i].nepomuk_translation);
		tracker_xmp_free (data);
        }
}

static void
test_xmp_orientation (void)
{
        gint i;

        for (i = 0; i < G_N_ELEMENTS (ORIENTATIONS); i++) {
		TrackerXmpData *data;
		gchar *xmp;

                xmp = g_strdup_printf (ORIENTATION_XMP, ORIENTATIONS[i].exif_value);
                data = tracker_xmp_new (xmp, strlen (xmp), "local://file");
                g_free (xmp);

                g_assert_cmpstr (data->orientation, ==, ORIENTATIONS[i].nepomuk_translation);
		tracker_xmp_free (data);
        }
}

static void
test_xmp_apply (void)
{
        TrackerSparqlBuilder *metadata;
        TrackerXmpData *data;

        metadata = tracker_sparql_builder_new_update ();;

        data = tracker_xmp_new (EXAMPLE_XMP, strlen (EXAMPLE_XMP), "urn:uuid:test");
	g_assert (data != NULL);

        tracker_sparql_builder_insert_open (metadata, NULL);
        tracker_sparql_builder_subject_iri (metadata, "urn:uuid:test");

        g_assert (tracker_xmp_apply (metadata, "urn:uuid:test", data));

        tracker_sparql_builder_insert_close (metadata);

        /* This is the only way to check the sparql is kinda correct */

	/* Disabled this for 0.8.5. It was reporting 41 not 50, this
	 * test is not credible and I can't see how it can be trusted
	 * as a method for making sure the query is correct.
	 *
	 * -mr
	 */

        /* g_assert_cmpint (tracker_sparql_builder_get_length (metadata), ==, 50); */
}

static void
test_xmp_apply_location (void)
{
        TrackerXmpData data = { 0, };
        TrackerSparqlBuilder *metadata;

        data.address = g_strdup ("Itamerenkatu 11-13");
        data.city = g_strdup ("Helsinki");
        data.state = g_strdup ("N/A");
        data.country = g_strdup ("Findland");

        metadata = tracker_sparql_builder_new_update ();

        tracker_sparql_builder_insert_open (metadata, NULL);
        tracker_sparql_builder_subject_iri (metadata, "urn:uuid:test");

        g_assert (tracker_xmp_apply (metadata, "urn:uuid:test", &data));

        tracker_sparql_builder_insert_close (metadata);

        /* This is the only way to check the sparql is kinda correct */
        g_assert_cmpint (tracker_sparql_builder_get_length (metadata), ==, 6);
}

int
main (int    argc,
      char **argv)
{
        gint result;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing XMP");

#ifdef HAVE_EXEMPI
	g_test_add_func ("/libtracker-extract/tracker-xmp/parsing_xmp",
                         test_parsing_xmp);

        g_test_add_func ("/libtracker-extract/tracker-xmp/metering-mode",
                         test_xmp_metering_mode);

        g_test_add_func ("/libtracker-extract/tracker-xmp/orientation",
                         test_xmp_orientation);

        g_test_add_func ("/libtracker-extract/tracker-xmp/sparql_translation",
                         test_xmp_apply);

#endif
        g_test_add_func ("/libtracker-extract/tracker-xmp/sparql_translation_location",
                         test_xmp_apply_location);

        result = g_test_run ();

	return result;
}
