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

#include <glib-object.h>

#include <libtracker-extract/tracker-extract.h>

#define BROKEN_XMP "This is not even XML"
#define EXAMPLE_XMP	  \
"   <x:xmpmeta   " \
"      xmlns:x=\'adobe:ns:meta/\'" \
"      xmlns:dc=\"http://purl.org/dc/elements/1.1/\"" \
"      xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\"" \
"      xmlns:exif=\"http://ns.adobe.com/exif/1.0/\"" \
"      xmlns:tiff=\"http://ns.adobe.com/tiff/1.0/\">" \
"     <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">" \
"        <rdf:Description rdf:about=\"\">" \
"         <dc:format>application/pdf</dc:format>" \
"         <dc:title>Title of the content</dc:title>" \
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
"         <exif:Flash>0</exif:Flash>" \
"         <exif:MeteringMode>3</exif:MeteringMode>" \
"         <exif:ExposureTime>1000</exif:ExposureTime>" \
"         <exif:FNumber>12</exif:FNumber>" \
"         <exif:FocalLength>50</exif:FocalLength>" \
"         <exif:ISOSpeedRatings>400</exif:ISOSpeedRatings>" \
"         <exif:WhiteBalance>1</exif:WhiteBalance>" \
"         <exif:Copyright>Copyright in exif</exif:Copyright>" \
"         <tiff:Orientation>1</tiff:Orientation>" \
"         <xmp:CreateDate>2002-08-15T17:10:04Z</xmp:CreateDate>" \
"        </rdf:Description> " \
"     </rdf:RDF> " \
"   </x:xmpmeta>"

#define METERING_MODE_XMP \
"   <x:xmpmeta   " \
"      xmlns:x=\'adobe:ns:meta/\'" \
"      xmlns:exif=\"http://ns.adobe.com/exif/1.0/\">" \
"     <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">" \
"        <rdf:Description rdf:about=\"\">" \
"         <exif:MeteringMode>%d</exif:MeteringMode>" \
"        </rdf:Description>" \
"     </rdf:RDF></x:xmpmeta> "

#define ORIENTATION_XMP	  \
"   <x:xmpmeta   " \
"      xmlns:x=\'adobe:ns:meta/\'" \
"      xmlns:tiff=\"http://ns.adobe.com/tiff/1.0/\">" \
"     <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">" \
"        <rdf:Description rdf:about=\"\">" \
"         <tiff:Orientation>%s</tiff:Orientation>" \
"        </rdf:Description>" \
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
	{"2", "nfo:orientation-top-mirror"},
	{"3", "nfo:orientation-bottom"},
	{"4", "nfo:orientation-bottom-mirror"},
	{"5", "nfo:orientation-left-mirror"},
	{"6", "nfo:orientation-right"},
	{"7", "nfo:orientation-right-mirror"},
	{"8", "nfo:orientation-left"},
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
test_parsing_xmp_invalid_file_subprocess (void)
{
	TrackerXmpData *data;

	data = tracker_xmp_new (BROKEN_XMP, strlen (BROKEN_XMP), "test://file");
	g_assert (data != NULL);

	tracker_xmp_free (data);
}

static void
test_parsing_xmp_invalid_file (void)
{
	g_test_trap_subprocess ("/libtracker-extract/tracker-xmp/parsing_xmp_invalid_file/subprocess", 0, 0);
	g_test_trap_assert_passed ();
	g_test_trap_assert_stderr ("*parsing failure*");
}

static void
test_parsing_xmp (void)
{
	TrackerXmpData *data;
	TrackerXmpData *expected;

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
	guint i;

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
	TrackerResource *resource;
	TrackerResource *artist;
	TrackerXmpData *data;

	resource = tracker_resource_new ("urn:uuid:test");

	data = tracker_xmp_new (EXAMPLE_XMP, strlen (EXAMPLE_XMP), "urn:uuid:test");
	g_assert (data != NULL);

	g_assert (tracker_xmp_apply_to_resource (resource, data));

	/* We just check a few of the properties at random. */
	g_assert_cmpstr (tracker_resource_get_first_string (resource, "nie:description"), ==,
	                 "Description of the content");

	artist = tracker_resource_get_first_relation (resource, "nco:contributor");
	g_assert_cmpstr (tracker_resource_get_first_string(artist, "nco:fullname"), ==,
	                 "Artist in exif");

	tracker_xmp_free (data);
}

static void
test_xmp_apply_location (void)
{
	TrackerXmpData data = { 0, };
	TrackerResource *resource, *location, *address;

	data.address = g_strdup ("Itamerenkatu 11-13");
	data.city = g_strdup ("Helsinki");
	data.state = g_strdup ("N/A");
	data.country = g_strdup ("Findland");

	resource = tracker_resource_new ("urn:uuid:test");

	g_assert (tracker_xmp_apply_to_resource (resource, &data));

	location = tracker_resource_get_first_relation (resource, "slo:location");
	address = tracker_resource_get_first_relation (location, "slo:postalAddress");

	g_assert_cmpstr (tracker_resource_get_first_string (address, "nco:streetAddress"), ==, data.address);
	g_assert_cmpstr (tracker_resource_get_first_string (address, "nco:region"), ==, data.state);
	g_assert_cmpstr (tracker_resource_get_first_string (address, "nco:locality"), ==, data.city);
	g_assert_cmpstr (tracker_resource_get_first_string (address, "nco:country"), ==, data.country);
}


static void
test_xmp_regions (void)
{
	TrackerXmpData *data;
	TrackerXmpRegion *region;

	GFile *f;
	gchar *contents;
	gsize  size;
	gchar *filepath;

	filepath = g_build_filename (TOP_SRCDIR, "tests", "libtracker-extract", "areas.xmp", NULL);
	f = g_file_new_for_path (filepath);
	g_assert (g_file_load_contents (f, NULL, &contents, &size, NULL, NULL));
	g_object_unref (f);
	g_free (filepath);

	data = tracker_xmp_new (contents, size, "test://file");

        g_free (contents);

	g_assert_cmpint (2, ==, g_slist_length (data->regions));

	region = g_slist_nth_data (data->regions, 1);
	g_assert_cmpstr (region->x, ==, "0.51");
	g_assert_cmpstr (region->y, ==, "0.51");
	g_assert_cmpstr (region->width, ==, "0.01");
	g_assert_cmpstr (region->height, ==, "0.09");
	g_assert_cmpstr (region->type, ==, "Pet");
	g_assert_cmpstr (region->title, ==, "Fido");
	g_assert_cmpstr (region->description, ==, "Fido looks happy!");

	region = g_slist_nth_data (data->regions, 0);
	g_assert_cmpstr (region->x, ==, "0.5");
	g_assert_cmpstr (region->y, ==, "0.5");
	g_assert_cmpstr (region->width, ==, "0.06");
	g_assert_cmpstr (region->height, ==, "0.09");
	g_assert_cmpstr (region->type, ==, "Face");
	g_assert_cmpstr (region->title, ==, "John Doe");

	tracker_xmp_free (data);
}

static void
test_xmp_regions_quill (void)
{
	TrackerXmpData *data;
	TrackerXmpRegion *region;

	GFile *f;
	gchar *contents;
	gsize  size;
	gchar *filepath;

	filepath = g_build_filename (TOP_SRCDIR, "tests", "libtracker-extract", "areas-with-contacts.xmp", NULL);
	f = g_file_new_for_path (filepath);
	g_assert (g_file_load_contents (f, NULL, &contents, &size, NULL, NULL));
	g_object_unref (f);
	g_free (filepath);

	data = tracker_xmp_new (contents, size, "test://file");

        g_free (contents);

	g_assert_cmpint (2, ==, g_slist_length (data->regions));

	region = g_slist_nth_data (data->regions, 1);
	g_assert_cmpstr (region->x, ==, "0.4");
	g_assert_cmpstr (region->y, ==, "0.3");
	g_assert_cmpstr (region->width, ==, "0.17");
	g_assert_cmpstr (region->height, ==, "0.15");
	g_assert_cmpstr (region->type, ==, "Face");
	g_assert_cmpstr (region->title, ==, "Dilbert");
	g_assert_cmpstr (region->link_class, ==, "nco:PersonContact");
	g_assert_cmpstr (region->link_uri, ==, "urn:uuid:2");

	region = g_slist_nth_data (data->regions, 0);
	g_assert_cmpstr (region->x, ==, "0.3");
	g_assert_cmpstr (region->y, ==, "0.4");
	g_assert_cmpstr (region->width, ==, "0.15");
	g_assert_cmpstr (region->height, ==, "0.17");
	g_assert_cmpstr (region->type, ==, "Face");
	g_assert_cmpstr (region->title, ==, "Albert Einstein");
	g_assert_cmpstr (region->link_class, ==, "nco:PersonContact");
	g_assert_cmpstr (region->link_uri, ==, "urn:uuid:1");

	tracker_xmp_free (data);
}

static void
test_xmp_regions_ns_prefix (void)
{
	TrackerXmpData *data;
	TrackerXmpRegion *region;

	GFile *f;
	gchar *contents;
	gsize  size;
	gchar *filepath;

	filepath = g_build_filename (TOP_SRCDIR, "tests", "libtracker-extract", "areas-ns.xmp", NULL);
	f = g_file_new_for_path (filepath);
	g_assert(g_file_load_contents (f, NULL, &contents, &size, NULL, NULL));
	g_object_unref (f);
	g_free (filepath);

	data = tracker_xmp_new (contents, size, "test://file");

        g_free (contents);

	g_assert_cmpint (2, ==, g_slist_length (data->regions));

	region = g_slist_nth_data (data->regions, 1);
	g_assert_cmpstr (region->x, ==, "0.51");
	g_assert_cmpstr (region->y, ==, "0.51");
	g_assert_cmpstr (region->width, ==, "0.01");
	g_assert_cmpstr (region->height, ==, "0.09");
	g_assert_cmpstr (region->type, ==, "Pet");
	g_assert_cmpstr (region->title, ==, "Fidoz");
	g_assert_cmpstr (region->description, ==, "Fido looks happy!");

	region = g_slist_nth_data (data->regions, 0);
	g_assert_cmpstr (region->x, ==, "0.5");
	g_assert_cmpstr (region->y, ==, "0.5");
	g_assert_cmpstr (region->width, ==, "0.06");
	g_assert_cmpstr (region->height, ==, "0.09");
	g_assert_cmpstr (region->type, ==, "Face");
	g_assert_cmpstr (region->title, ==, "Average Joe");

	tracker_xmp_free (data);
}

static void
test_xmp_regions_nb282393 ()
{
	TrackerXmpData *data;
	TrackerXmpRegion *region;

	GFile *f;
	gchar *contents;
	gsize  size;
	gchar *filepath;

	filepath = g_build_filename (TOP_SRCDIR, "tests", "libtracker-extract", "nb282393.xmp", NULL);
	f = g_file_new_for_path (filepath);
	g_assert(g_file_load_contents (f, NULL, &contents, &size, NULL, NULL));
	g_object_unref (f);
	g_free (filepath);

	data = tracker_xmp_new (contents, size, "test://file");

        g_free (contents);

	g_assert_cmpint (1, ==, g_slist_length (data->regions));

	/* Regions are stacked while parsing.*/
	region = g_slist_nth_data (data->regions, 0);
	g_assert_cmpstr (region->x, ==, "0.433333");
	g_assert_cmpstr (region->y, ==, "0.370000");
	g_assert_cmpstr (region->width, ==, "0.586667");
	g_assert_cmpstr (region->height, ==, "0.440000");
	g_assert_cmpstr (region->title, ==, " ");

	tracker_xmp_free (data);
}

static void
test_xmp_regions_nb282393_2 ()
{
	TrackerXmpData *data;
	TrackerXmpRegion *region;

	GFile *f;
	gchar *contents;
	gsize  size;
	gchar *filepath;

	filepath = g_build_filename (TOP_SRCDIR, "tests", "libtracker-extract", "nb282393_simple.xmp", NULL);
	f = g_file_new_for_path (filepath);
	g_assert(g_file_load_contents (f, NULL, &contents, &size, NULL, NULL));
	g_object_unref (f);
	g_free (filepath);

	data = tracker_xmp_new (contents, size, "test://file");

        g_free (contents);

	g_assert_cmpint (1, ==, g_slist_length (data->regions));

	/* Regions are stacked while parsing.*/
	region = g_slist_nth_data (data->regions, 0);
	g_assert_cmpstr (region->x, ==, "0.440000");
	g_assert_cmpstr (region->y, ==, "0.365000");
	g_assert_cmpstr (region->width, ==, "0.586667");
	g_assert_cmpstr (region->height, ==, "0.440000");
	g_assert_cmpstr (region->title, ==, " ");

        g_assert_cmpstr (region->link_class, ==, "nco:PersonContact");
        g_assert_cmpstr (region->link_uri, ==, "urn:uuid:840a3c05-6cc6-48a1-bb56-fc50fae3345a");

	tracker_xmp_free (data);
}

int
main (int    argc,
      char **argv)
{
	gint result;

	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing XMP");

#ifdef HAVE_EXEMPI

	g_test_add_func ("/libtracker-extract/tracker-xmp/parsing_xmp",
	                 test_parsing_xmp);

	g_test_add_func ("/libtracker-extract/tracker-xmp/parsing_xmp_invalid_file",
	                 test_parsing_xmp_invalid_file);
	g_test_add_func ("/libtracker-extract/tracker-xmp/parsing_xmp_invalid_file/subprocess",
	                 test_parsing_xmp_invalid_file_subprocess);

	g_test_add_func ("/libtracker-extract/tracker-xmp/metering-mode",
	                 test_xmp_metering_mode);

	g_test_add_func ("/libtracker-extract/tracker-xmp/orientation",
	                 test_xmp_orientation);

	g_test_add_func ("/libtracker-extract/tracker-xmp/sparql_translation",
	                 test_xmp_apply);

	g_test_add_func ("/libtracker-extract/tracker-xmp/xmp_regions",
	                 test_xmp_regions);

	g_test_add_func ("/libtracker-extract/tracker-xmp/xmp_regions_2",
	                 test_xmp_regions_quill);

        g_test_add_func ("/libtracker-extract/tracker-xmp/xmp_regions_crash_nb282393",
                         test_xmp_regions_nb282393);

        g_test_add_func ("/libtracker-extract/tracker-xmp/xmp_regions_crash_nb282393_2",
                         test_xmp_regions_nb282393_2);

	g_test_add_func ("/libtracker-extract/tracker-xmp/xmp_regions_ns_prefix",
	                 test_xmp_regions_ns_prefix);

#endif
	g_test_add_func ("/libtracker-extract/tracker-xmp/sparql_translation_location",
	                 test_xmp_apply_location);

	result = g_test_run ();

	return result;
}
