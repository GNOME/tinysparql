/*
 * Copyright (C) 2010, Nokia (urho.konttori@nokia.com)
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

TrackerXmpData EXAMPLE_EXPECTED = {
        /* NS_DC */
        "Title of the content",
        "CC share alike",
        "The ultimate creator",
        "Description of the content",
        "2010-03-18T15:17:04Z",
        "test, data, xmp",
        "Subject of the content",

        "A honest developer",     /* publisher */
        "A honest contributor",
        NULL,                     /* type ? */
        "application/pdf",
        "12345",
        "My dirty mind",
        "Spanglish",
        "Single",
        "Pretty high after this test",

        /* NS_CC */
        NULL,                     /* license */
          
        /* NS_PDF */
        NULL,                     /* pdf_title */
        NULL,                     /* pdf_keywords */

        /* NS_EXIF*/
        "Title in exif",
        "2010-03-18T15:17:04Z",   
        "Artist in exif",
        "Make in exif",
        "Model in exif",
        "nfo:orientation-top",
        "nmm:flash-off",
        "nmm:metering-mode-spot",
        "1000",                  /* exposure time */
        "12",                    /* fnumber */
        "50",                    /* focal length */
          
        "400",                   /* iso speed rating */
        "nmm:white-balance-manual",
        "Copyright in exif",

        /* NS_XAP */
        NULL,

        /* NS_IPTC4XMP */
        /* NS_PHOTOSHOP */
        NULL,                    /* address */
        NULL,                    /* country */
        NULL,                    /* state */
        NULL                    /* city */
};


typedef struct {
        const gchar *exif_value;
        const gchar *nepomuk_translation;
} ExifNepomuk;


ExifNepomuk METERING_MODES [] = {
        {"0",  "nmm:metering-mode-other"},
        {"1", "nmm:metering-mode-average"},
        {"2", "nmm:metering-mode-center-weighted-average"},
        {"3", "nmm:metering-mode-spot"},
        {"4", "nmm:metering-mode-multispot"},
        {"5", "nmm:metering-mode-pattern"},
        {"6", "nmm:metering-mode-partial"},
        {NULL, NULL}
};

ExifNepomuk ORIENTATIONS [] = {
        {"top - right", "nfo:orientation-top-mirror"},
        {"bottom - right", "nfo:orientation-bottom-mirror"},
        {"bottom - left", "nfo:orientation-bottom"},
        {"left - top", "nfo:orientation-left-mirror"},
        {"right - top", "nfo:orientation-right"},
        {"right - bottom", "nfo:orientation-right-mirror"},
        {"left - bottom", "nfo:orientation-left"},
        {"invalid value", "nfo:orientation-top"}
};

static void
test_parsing_xmp ()
{
        TrackerXmpData data;
        gboolean       result;

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
                result = tracker_xmp_read (BROKEN_XMP, strlen (BROKEN_XMP), "test://file", &data);
                /* Catch io and check error message ("XML parsing failure") */
        }
        g_test_trap_assert_stderr ("*parsing failure*");

        result = tracker_xmp_read (EXAMPLE_XMP, strlen (EXAMPLE_XMP), "test://file", &data);
        /* NS_DC */
        g_assert_cmpstr (data.format, ==, EXAMPLE_EXPECTED.format);
        g_assert_cmpstr (data.title, ==, EXAMPLE_EXPECTED.title);
        g_assert_cmpstr (data.rights, ==, EXAMPLE_EXPECTED.rights);
        g_assert_cmpstr (data.description, ==, EXAMPLE_EXPECTED.description);
        g_assert_cmpstr (data.date, ==, EXAMPLE_EXPECTED.date);
        g_assert_cmpstr (data.keywords, ==, EXAMPLE_EXPECTED.keywords);
        g_assert_cmpstr (data.subject, ==, EXAMPLE_EXPECTED.subject); 
        g_assert_cmpstr (data.publisher, ==, EXAMPLE_EXPECTED.publisher);
        g_assert_cmpstr (data.contributor, ==, EXAMPLE_EXPECTED.contributor);
        g_assert_cmpstr (data.identifier, ==, EXAMPLE_EXPECTED.identifier);
        g_assert_cmpstr (data.source, ==, EXAMPLE_EXPECTED.source);
        g_assert_cmpstr (data.language, ==, EXAMPLE_EXPECTED.language);
        g_assert_cmpstr (data.relation, ==, EXAMPLE_EXPECTED.relation);
        g_assert_cmpstr (data.coverage, ==, EXAMPLE_EXPECTED.coverage);
        g_assert_cmpstr (data.creator, ==, EXAMPLE_EXPECTED.creator);

        /* NS_EXIF*/
        g_assert_cmpstr (data.title2, ==, EXAMPLE_EXPECTED.title2);
	g_assert_cmpstr (data.time_original, ==, EXAMPLE_EXPECTED.time_original);
	g_assert_cmpstr (data.artist, ==, EXAMPLE_EXPECTED.artist);
	g_assert_cmpstr (data.make, ==, EXAMPLE_EXPECTED.make);
	g_assert_cmpstr (data.model, ==, EXAMPLE_EXPECTED.model);
	g_assert_cmpstr (data.orientation, ==, EXAMPLE_EXPECTED.orientation);
	g_assert_cmpstr (data.flash, ==, EXAMPLE_EXPECTED.flash);
	g_assert_cmpstr (data.metering_mode, ==, EXAMPLE_EXPECTED.metering_mode);
	g_assert_cmpstr (data.exposure_time, ==, EXAMPLE_EXPECTED.exposure_time);
	g_assert_cmpstr (data.fnumber, ==, EXAMPLE_EXPECTED.fnumber);
	g_assert_cmpstr (data.focal_length, ==, EXAMPLE_EXPECTED.focal_length);

	g_assert_cmpstr (data.iso_speed_ratings, ==, EXAMPLE_EXPECTED.iso_speed_ratings);
	g_assert_cmpstr (data.white_balance, ==, EXAMPLE_EXPECTED.white_balance);
	g_assert_cmpstr (data.copyright, ==, EXAMPLE_EXPECTED.copyright);

        g_assert (result);
}

static void
test_xmp_metering_mode (void) 
{
        gint i;
        gchar *xmp;
        TrackerXmpData data;

        const gchar *xmp_template = "" \
                "   <x:xmpmeta   "                    \
                "      xmlns:x=\'adobe:ns:meta/\'"    \
                "      xmlns:exif=\"http://ns.adobe.com/exif/1.0/\">"   \
                "     <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">" \
                "        <rdf:Description rdf:about=\"\">"              \
                "         <exif:MeteringMode>%d</exif:MeteringMode>"     \
                "        </rdf:Description>" \
                "     </rdf:RDF></x:xmpmeta> " ;

        for (i = 0; METERING_MODES[i].exif_value != NULL; i++) {
                xmp = g_strdup_printf (xmp_template, i);
                tracker_xmp_read (xmp, strlen (xmp), "local://file", &data);
                
                g_assert_cmpstr (data.metering_mode, ==, METERING_MODES[i].nepomuk_translation);
                
                g_free (xmp);
        }
}

static void
test_xmp_orientation (void) 
{
        gint i;
        gchar *xmp;
        TrackerXmpData data;

        const gchar *xmp_template = "" \
                "   <x:xmpmeta   "                    \
                "      xmlns:x=\'adobe:ns:meta/\'"    \
                "      xmlns:exif=\"http://ns.adobe.com/exif/1.0/\">"   \
                "     <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">" \
                "        <rdf:Description rdf:about=\"\">"              \
                "         <exif:Orientation>%s</exif:Orientation>"     \
                "        </rdf:Description>" \
                "     </rdf:RDF></x:xmpmeta> " ;

        for (i = 0; ORIENTATIONS[i].exif_value != NULL; i++) {
                xmp = g_strdup_printf (xmp_template, ORIENTATIONS[i].exif_value);
                tracker_xmp_read (xmp, strlen (xmp), "local://file", &data);
                
                g_assert_cmpstr (data.orientation, ==, ORIENTATIONS[i].nepomuk_translation);
                
                g_free (xmp);
        }
}

static void
test_xmp_apply ()
{
        TrackerSparqlBuilder *metadata;
        TrackerXmpData data;

        metadata = tracker_sparql_builder_new_update ();;

        g_assert (tracker_xmp_read (EXAMPLE_XMP, strlen (EXAMPLE_XMP), "urn:uuid:test", &data));

        tracker_sparql_builder_insert_open (metadata, NULL);
        tracker_sparql_builder_subject_iri (metadata, "urn:uuid:test");

        g_assert (tracker_xmp_apply (metadata, "urn:uuid:test", &data));

        tracker_sparql_builder_insert_close (metadata);

        /* This is the only way to check the sparql is kinda correct */
        g_assert_cmpint (tracker_sparql_builder_get_length (metadata), ==, 50);
}

static void
test_xmp_apply_location ()
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
