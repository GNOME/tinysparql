/*
 * Copyright (C) 2016, Sam Thursfield <sam@afuera.me.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <locale.h>

#include <tinysparql.h>

static void
test_resource_get_empty (void)
{
	TrackerResource *resource;

	resource = tracker_resource_new ("http://example.com/resource");

	g_assert_true (tracker_resource_get_values (resource, "http://example.com/0") == NULL);

	g_assert_true (tracker_resource_get_first_double (resource, "http://example.com/0") == 0.0);
	g_assert_true (tracker_resource_get_first_int (resource, "http://example.com/0") == 0);
	g_assert_true (tracker_resource_get_first_int64 (resource, "http://example.com/0") == 0);
	g_assert_true (tracker_resource_get_first_string (resource, "http://example.com/0") == NULL);
	g_assert_true (tracker_resource_get_first_uri (resource, "http://example.com/0") == NULL);
	g_assert_true (tracker_resource_get_first_datetime (resource, "http://example.com/0") == NULL);

	g_object_unref (resource);
}

static void
test_resource_get_set_simple (void)
{
	TrackerResource *resource;
	GDateTime *date_time;

	resource = tracker_resource_new ("http://example.com/resource");
	date_time = g_date_time_new_now_utc ();

	tracker_resource_set_double (resource, "http://example.com/1", 0.6);
	tracker_resource_set_int (resource, "http://example.com/2", 60);
	tracker_resource_set_int64 (resource, "http://example.com/3", 123456789);
	tracker_resource_set_string (resource, "http://example.com/4", "Hello");
	tracker_resource_set_uri (resource, "http://example.com/5", "http://example.com/");
	tracker_resource_set_datetime (resource, "http://example.com/6", date_time);

	g_assert_cmpfloat_with_epsilon (tracker_resource_get_first_double (resource, "http://example.com/1"), 0.6, DBL_EPSILON);
	g_assert_cmpint (tracker_resource_get_first_int (resource, "http://example.com/2"), ==, 60);
	g_assert_true (tracker_resource_get_first_int64 (resource, "http://example.com/3") == 123456789);
	g_assert_cmpstr (tracker_resource_get_first_string (resource, "http://example.com/4"), ==, "Hello");
	g_assert_cmpstr (tracker_resource_get_first_uri (resource, "http://example.com/5"), ==, "http://example.com/");
	g_assert_true (g_date_time_compare (tracker_resource_get_first_datetime (resource, "http://example.com/6"), date_time) == 0);

	g_date_time_unref (date_time);
	g_object_unref (resource);
}

static void
test_resource_get_set_gvalue (void)
{
	TrackerResource *resource;
	GValue value = G_VALUE_INIT;
	GList *list;

	resource = tracker_resource_new ("http://example.com/resource");

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_static_string (&value, "xyzzy");
	tracker_resource_set_gvalue (resource, "http://example.com/0", &value);

	list = tracker_resource_get_values (resource, "http://example.com/0");
	g_assert_cmpint (g_list_length (list), ==, 1);

	g_object_unref (resource);
}

#define RANDOM_GVALUE_TYPE (G_TYPE_HASH_TABLE)

static void
init_gvalue_with_random_type (GValue *value)
{
	/* Hash table is used here as an "unexpected" value type. It makes no sense
	 * to do this in real code, but the code shouldn't crash or assert if
	 * someone does do it.
	 */
	g_value_init (value, G_TYPE_HASH_TABLE);
	g_value_take_boxed (value, g_hash_table_new (NULL, NULL));
}

static void
test_resource_get_set_many (void)
{
	TrackerResource *resource;
	GValue value = G_VALUE_INIT;
	GList *list;
	GDateTime *date_time;

	resource = tracker_resource_new ("http://example.com/resource");
	date_time = g_date_time_new_now_utc ();

	/* All the add_* functions except for add_gvalue are generated using the
	 * same macro, so we only need to test one or two of them here.
	 */
	tracker_resource_add_int (resource, "http://example.com/0", 60);
	tracker_resource_add_string (resource, "http://example.com/0", "Hello");
	tracker_resource_add_datetime (resource, "http://example.com/0", date_time);

	init_gvalue_with_random_type (&value);
	tracker_resource_add_gvalue (resource, "http://example.com/0", &value);
	g_value_unset (&value);

	list = tracker_resource_get_values (resource, "http://example.com/0");
	g_assert_cmpint (g_list_length (list), ==, 4);

	g_assert_cmpint (g_value_get_int (list->data), ==, 60);
	g_assert_cmpstr (g_value_get_string (list->next->data), ==, "Hello");
	g_assert_cmpint (g_date_time_compare (g_value_get_boxed (list->next->next->data), date_time), ==, 0);
	g_assert_true (G_VALUE_HOLDS (list->next->next->next->data, RANDOM_GVALUE_TYPE));

	g_list_free_full (list, (GDestroyNotify) g_value_unset);

	g_date_time_unref (date_time);
	g_object_unref (resource);
}

static void
test_resource_get_set_pointer_validation (void)
{
	if (g_test_subprocess ()) {
		TrackerResource *resource;

		resource = tracker_resource_new ("http://example.com/resource");

		/* This should trigger a g_warning(), and abort. */
		tracker_resource_set_string (resource, "http://example.com/1", NULL);

		return;
	}

	g_test_trap_subprocess (NULL, 0, 0);
	g_test_trap_assert_failed ();
	g_test_trap_assert_stderr ("*tracker_resource_set_string: NULL is not a valid value.*");
}

static void
test_resource_serialization (void)
{
	TrackerResource *res, *child, *child2, *child3, *copy;
	GVariant *variant, *variant2;
	GValue value = G_VALUE_INIT;

	/* Create sample data */
	res = tracker_resource_new (NULL);
	tracker_resource_set_string (res, "nie:title", "foo");
	tracker_resource_set_double (res, "nfo:duration", 25.4);
	tracker_resource_set_boolean (res, "nfo:isBootable", TRUE);
	tracker_resource_set_int (res, "nie:usageCounter", 4);
	tracker_resource_set_int64 (res, "nie:contentSize", 42);
	tracker_resource_set_uri (res, "dc:relation", "tracker:");

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, 1);
	tracker_resource_add_gvalue (res, "nie:usageCounter", &value);
	g_value_unset (&value);

	tracker_resource_add_uri (res, "rdf:type", "nfo:Audio");
	tracker_resource_add_uri (res, "rdf:type", "nfo:Media");

	child = tracker_resource_new ("uriuri");
	tracker_resource_set_string (child, "nfo:fileName", "bar");
	tracker_resource_set_take_relation (res, "nie:isStoredAs", child);

	child2 = tracker_resource_new (NULL);
	tracker_resource_set_string (child2, "nie:title", "baz");
	tracker_resource_set_take_relation (child, "nfo:belongsToContainer", child2);

	child3 = tracker_resource_new ("_:a");
	tracker_resource_add_string (child3, "nie:title", "blah");
	tracker_resource_add_double (child3, "nfo:duration", 12.9);
	tracker_resource_add_boolean (res, "nfo:isBootable", FALSE);
	tracker_resource_add_int (res, "nie:usageCounter", 5);
	tracker_resource_add_int64 (res, "nie:contentSize", 420);

	tracker_resource_add_relation (child, "nie:interpretedAs", g_object_ref (child2));
	tracker_resource_add_relation (child, "nie:interpretedAs", child3);

	/* Ensure multiple serialize calls produce the same variant */
	variant = tracker_resource_serialize (res);
	g_assert_true (variant != NULL);
	variant2 = tracker_resource_serialize (res);
	g_assert_true (variant != NULL);
	g_assert_true (g_variant_equal (variant, variant2));
	g_variant_unref (variant2);

	/* Deserialize to a copy and check its content */
	copy = tracker_resource_deserialize (variant);
	g_assert_true (copy != NULL);

	g_assert_true (tracker_resource_get_first_boolean (copy, "nfo:isBootable"));
	g_assert_true (strncmp (tracker_resource_get_identifier (copy), "_:", 2) == 0);
	g_assert_cmpstr (tracker_resource_get_first_string (copy, "nie:title"), ==, "foo");
	g_assert_cmpint (tracker_resource_get_first_int (copy, "nie:usageCounter"), ==, 4);
	g_assert_cmpint (tracker_resource_get_first_int64 (copy, "nie:contentSize"), ==, 42);
	g_assert_cmpstr (tracker_resource_get_first_uri (copy, "dc:relation"), ==, "tracker:");

	child = tracker_resource_get_first_relation (copy, "nie:isStoredAs");
	g_assert_true (child != NULL);
	g_assert_cmpstr (tracker_resource_get_identifier (child), ==, "uriuri");
	g_assert_cmpstr (tracker_resource_get_first_string (child, "nfo:fileName"), ==, "bar");

	child2 = tracker_resource_get_first_relation (child, "nfo:belongsToContainer");
	g_assert_true (child2 != NULL);
	g_assert_true (strncmp (tracker_resource_get_identifier (child2), "_:", 2) == 0);
	g_assert_cmpstr (tracker_resource_get_first_string (child2, "nie:title"), ==, "baz");

	/* Also ensure the copy serializes the same */
	variant2 = tracker_resource_serialize (copy);
	g_assert_true (variant2 != NULL);

	g_assert_true (g_variant_equal (variant, variant2));
	g_variant_unref (variant);
	g_variant_unref (variant2);

	g_object_unref (res);
	g_object_unref (copy);
}

static void
test_resource_iri_valid_chars (void)
{
	TrackerResource *resource;

	resource = tracker_resource_new ("http://example.com/resource");
	tracker_resource_set_uri (resource, "rdf:type", "http://example.com/resource");
	g_assert_cmpstr (tracker_resource_get_identifier (resource), ==, "http://example.com/resource");
	g_assert_cmpstr (tracker_resource_get_first_uri (resource, "rdf:type"), ==, "http://example.com/resource");
	g_object_unref (resource);

	resource = tracker_resource_new ("http://example.com/A B");
	tracker_resource_set_uri (resource, "rdf:type", "http://example.com/A B");
	g_assert_cmpstr (tracker_resource_get_identifier (resource), ==, "http://example.com/A%20B");
	g_assert_cmpstr (tracker_resource_get_first_uri (resource, "rdf:type"), ==, "http://example.com/A%20B");
	g_object_unref (resource);

	resource = tracker_resource_new ("http://example.com/♥️");
	tracker_resource_set_uri (resource, "rdf:type", "http://example.com/♥️");
	g_assert_cmpstr (tracker_resource_get_identifier (resource), ==, "http://example.com/♥️");
	g_assert_cmpstr (tracker_resource_get_first_uri (resource, "rdf:type"), ==, "http://example.com/♥️");
	g_object_unref (resource);

	resource = tracker_resource_new ("http://example.com/{}\\`\"^|");
	tracker_resource_set_uri (resource, "rdf:type", "http://example.com/{}\\`\"^|");
	g_assert_cmpstr (tracker_resource_get_identifier (resource), ==, "http://example.com/%7B%7D%5C%60%22%5E%7C");
	g_assert_cmpstr (tracker_resource_get_first_uri (resource, "rdf:type"), ==, "http://example.com/%7B%7D%5C%60%22%5E%7C");
	g_object_unref (resource);

	resource = tracker_resource_new ("http://example.com/\x1f");
	tracker_resource_set_uri (resource, "rdf:type", "http://example.com/\x1f");
	g_assert_cmpstr (tracker_resource_get_identifier (resource), ==, "http://example.com/%1F");
	g_assert_cmpstr (tracker_resource_get_first_uri (resource, "rdf:type"), ==, "http://example.com/%1F");
	g_object_unref (resource);
}

static void
test_resource_compare (void)
{
	TrackerResource *res1, *res2, *res3;

	res1 = tracker_resource_new ("http://example.com/a");
	res2 = tracker_resource_new ("http://example.com/b");
	res3 = tracker_resource_new ("http://example.com/c");

	g_assert_cmpint (tracker_resource_identifier_compare_func (res1, tracker_resource_get_identifier (res1)), ==, 0);
	g_assert_cmpint (tracker_resource_identifier_compare_func (res1, tracker_resource_get_identifier (res2)), <, 0);
	g_assert_cmpint (tracker_resource_identifier_compare_func (res1, tracker_resource_get_identifier (res3)), <, 0);
	g_assert_cmpint (tracker_resource_identifier_compare_func (res2, tracker_resource_get_identifier (res1)), >, 0);
	g_assert_cmpint (tracker_resource_identifier_compare_func (res2, tracker_resource_get_identifier (res2)), ==, 0);
	g_assert_cmpint (tracker_resource_identifier_compare_func (res2, tracker_resource_get_identifier (res3)), <, 0);
	g_assert_cmpint (tracker_resource_identifier_compare_func (res3, tracker_resource_get_identifier (res1)), >, 0);
	g_assert_cmpint (tracker_resource_identifier_compare_func (res3, tracker_resource_get_identifier (res2)), >, 0);
	g_assert_cmpint (tracker_resource_identifier_compare_func (res3, tracker_resource_get_identifier (res3)), ==, 0);

	g_object_unref (res1);
	g_object_unref (res2);
	g_object_unref (res3);
}

static void
deserialize_cb (GObject      *source,
                GAsyncResult *res,
                gpointer      user_data)
{
	GError *error = NULL;
	gboolean retval;

	retval = tracker_sparql_connection_deserialize_finish (TRACKER_SPARQL_CONNECTION (source),
	                                                       res, &error);
	g_assert_true (retval);
	g_assert_no_error (error);

	g_main_loop_quit (user_data);
}

static void
deserialize (TrackerSparqlConnection  *connection,
             const gchar              *rdf,
             TrackerRdfFormat          rdf_format,
             const gchar              *default_graph)
{
	GMainLoop *loop;
	GInputStream *stream;

	stream = g_memory_input_stream_new_from_data (rdf, strlen (rdf), NULL);

	loop = g_main_loop_new (NULL, FALSE);
	tracker_sparql_connection_deserialize_async (connection,
	                                             0,
	                                             rdf_format,
	                                             default_graph,
	                                             stream,
	                                             NULL,
	                                             deserialize_cb,
	                                             loop);

	g_main_loop_run (loop);
	g_main_loop_unref (loop);
	g_object_unref (stream);
}

static void
test_resource_load (void)
{
	TrackerResource *resource, *content, *artist, *album;
	gchar *ttl, *trig, *json_ld, *sparql;
	TrackerSparqlConnection *connection;
	TrackerNamespaceManager *namespaces;
	GDateTime *datetime;
	GFile *nepomuk;
	GError *error = NULL;
	guint i, j;
	gchar *graphs[] = {
		"graph:direct",
		"graph:sparql",
		"graph:turtle",
		"graph:trig",
		"graph:json",
	};

	datetime = g_date_time_new_utc (2020, 01, 01, 01, 01, 01);

	/* Create set of resources to insert */
	resource = tracker_resource_new ("file://song.mp3");
	tracker_resource_set_uri (resource, "rdf:type", "nfo:FileDataObject");
	tracker_resource_set_string (resource, "nfo:fileName", "song.mp3");
	tracker_resource_set_datetime (resource, "nfo:fileCreated", datetime);
	tracker_resource_set_int (resource, "nie:byteSize", 123456);

	content = tracker_resource_new ("inode:123");
	tracker_resource_set_uri (content, "rdf:type", "nmm:MusicPiece");
	tracker_resource_add_uri (content, "rdf:type", "nfo:Audio");
	tracker_resource_add_uri (content, "rdf:type", "nfo:Media");
	tracker_resource_set_string (content, "nie:title", "Song");
	tracker_resource_set_string (content, "nfo:codec", "MP3");
	tracker_resource_set_int (content, "nfo:duration", 123);
	tracker_resource_set_int64 (content, "nfo:averageBitrate", 320000);
	tracker_resource_set_double (content, "nfo:sampleRate", 999.99);
	tracker_resource_set_boolean (content, "nmm:uPnPShared", FALSE);

	artist = tracker_resource_new ("urn:artist");
	tracker_resource_set_uri (artist, "rdf:type", "nmm:Artist");
	tracker_resource_set_string (artist, "nmm:artistName", "Artist");

	album = tracker_resource_new ("urn:album");
	tracker_resource_set_uri (album, "rdf:type", "nmm:MusicAlbum");
	tracker_resource_set_string (album, "nie:title", "Album");

	tracker_resource_set_take_relation (resource, "nie:interpretedAs", content);
	tracker_resource_set_relation (content, "nie:isStoredAs", resource);
	tracker_resource_set_take_relation (content, "nmm:musicAlbum", album);
	tracker_resource_add_take_relation (content, "nmm:artist", artist);

	/* Create connection */
	nepomuk = tracker_sparql_get_ontology_nepomuk ();
	connection = tracker_sparql_connection_new (0, NULL, nepomuk, NULL, &error);
	g_assert_no_error (error);
	namespaces = tracker_sparql_connection_get_namespace_manager (connection);

	/* Get RDF/SPARQL output for the resource */
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	ttl = tracker_resource_print_turtle (resource, NULL);
	trig = tracker_resource_print_rdf (resource, namespaces, TRACKER_RDF_FORMAT_TRIG, "graph:trig");
	json_ld = tracker_resource_print_jsonld (resource, NULL);
	sparql = tracker_resource_print_sparql_update (resource, NULL, "graph:sparql");
	G_GNUC_END_IGNORE_DEPRECATIONS

	/* Insert data in separate graphs for comparisons */
	tracker_sparql_connection_update_resource (connection, "graph:direct", resource, NULL, &error);
	g_assert_no_error (error);
	tracker_sparql_connection_update (connection, sparql, NULL, &error);
	g_assert_no_error (error);
	deserialize (connection, ttl, TRACKER_RDF_FORMAT_TURTLE, "graph:turtle");
	deserialize (connection, trig, TRACKER_RDF_FORMAT_TRIG, NULL);
	deserialize (connection, json_ld, TRACKER_RDF_FORMAT_JSON_LD, "graph:json");

	/* Diff every graph with every other */
	for (i = 0; i < G_N_ELEMENTS (graphs); i++) {
		for (j = 0; j < G_N_ELEMENTS (graphs); j++) {
			TrackerSparqlCursor *cursor;
			gchar *query;

			if (i == j)
				continue;

			query = g_strdup_printf ("SELECT ?s ?p ?o {"
			                         "  GRAPH %s {"
			                         "    ?s ?p ?o ."
			                         "    FILTER (?p != nrl:added && ?p != nrl:modified)"
			                         "  } MINUS {"
			                         "    GRAPH %s {"
			                         "      ?s ?p ?o ."
			                         "      FILTER (?p != nrl:added && ?p != nrl:modified)"
			                         "    }"
			                         "  }"
			                         "}",
			                         graphs[i], graphs[j]);

			cursor = tracker_sparql_connection_query (connection, query, NULL, &error);
			g_assert_no_error (error);
			g_free (query);

			/* Cursor should come up empty, all graphs have the same data */
			g_assert_false (tracker_sparql_cursor_next (cursor, NULL, &error));
			g_assert_no_error (error);

			g_clear_object (&cursor);
		}
	}

	g_free (ttl);
	g_free (trig);
	g_free (json_ld);
	g_free (sparql);
	g_date_time_unref (datetime);

	tracker_sparql_connection_close (connection);
	g_object_unref (connection);
}

int
main (int    argc,
      char **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing Tracker resource abstraction");

	g_test_add_func ("/libtracker-sparql/tracker-resource/get_empty",
	                 test_resource_get_empty);
	g_test_add_func ("/libtracker-sparql/tracker-resource/get_set_simple",
	                 test_resource_get_set_simple);
	g_test_add_func ("/libtracker-sparql/tracker-resource/get_set_gvalue",
	                 test_resource_get_set_gvalue);
	g_test_add_func ("/libtracker-sparql/tracker-resource/get_set_many",
	                 test_resource_get_set_many);
	g_test_add_func ("/libtracker-sparql/tracker-resource/get_set_pointer_validation",
	                 test_resource_get_set_pointer_validation);
	g_test_add_func ("/libtracker-sparql/tracker-resource/serialization",
	                 test_resource_serialization);
	g_test_add_func ("/libtracker-sparql/tracker-resource/iri-valid-chars",
	                 test_resource_iri_valid_chars);
	g_test_add_func ("/libtracker-sparql/tracker-resource/compare",
	                 test_resource_compare);
	g_test_add_func ("/libtracker-sparql/tracker-resource/load",
	                 test_resource_load);

	return g_test_run ();
}
