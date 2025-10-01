/*
 * Copyright (C) 2022, Red Hat Ltd.
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <tinysparql.h>

typedef struct _TestFixture TestFixture;

typedef struct {
	const gchar *test_name;
	void (*func) (TestFixture   *test_fixture,
	              gconstpointer  context);
} TestInfo;

struct _TestFixture {
	TestInfo *test;
	TrackerSparqlConnection *conn;
};

typedef struct {
	TrackerSparqlConnection *sparql_conn;
	GDBusConnection *dbus_conn;
	GMainLoop *thread_loop;
	const char *object_path;
	gboolean started;
} StartupData;

typedef struct {
	GThread *thread;
	GMainLoop *loop;
} EndpointData;

static const gchar *bus_name = NULL;

#define PHOTO_INSERT_SPARQL \
	"INSERT DATA { " \
	"  <http://example.com/a> a nmm:Photo ;" \
	"    nmm:exposureTime 0.12345678901 ; " \
	"    nfo:horizontalResolution 123 ; " \
	"    nfo:codec 'png' ; " \
	"    nfo:interlaceMode false ; " \
	"    nie:contentCreated '2022-12-04T01:01:01Z' ;" \
	"}"

#define PHOTO_TURTLE \
	"@prefix nmm: <" TRACKER_PREFIX_NMM "> ." \
	"@prefix nfo: <" TRACKER_PREFIX_NFO "> ." \
	"@prefix nie: <" TRACKER_PREFIX_NIE "> ." \
	"<http://example.com/turtle> a nmm:Photo ;" \
	"  nmm:exposureTime 3.456789012 ; " \
	"  nfo:horizontalResolution 321 ; " \
	"  nfo:codec 'png' ; " \
	"  nfo:interlaceMode true ; " \
	"  nie:contentCreated '2023-07-07T01:01:01Z' ."

#define PHOTO_TRIG \
	"@prefix nmm: <" TRACKER_PREFIX_NMM "> ." \
	"@prefix nfo: <" TRACKER_PREFIX_NFO "> ." \
	"@prefix nie: <" TRACKER_PREFIX_NIE "> ." \
	"GRAPH <http://example.com/test> { " \
	"  <http://example.com/trig> a nmm:Photo ;" \
	"    nmm:exposureTime 3.456789012 ; " \
	"    nfo:horizontalResolution 321 ; " \
	"    nfo:codec 'png' ; " \
	"    nfo:interlaceMode true ; " \
	"    nie:contentCreated '2023-07-07T01:01:01Z' ." \
	"}"

#define PHOTO_JSONLD \
	"{" \
	"  \"@context\" : {" \
	"    \"nmm\" : \"" TRACKER_PREFIX_NMM "\"," \
	"    \"nfo\" : \"" TRACKER_PREFIX_NFO "\"," \
	"    \"nie\" : \"" TRACKER_PREFIX_NIE "\"" \
	"  }," \
	"  \"@graph\" : [" \
	"    {" \
	"      \"@id\": \"http://example.com/jsonld\"," \
	"      \"@type\": \"nmm:Photo\"," \
	"      \"nmm:exposureTime\": 3.456789012," \
	"      \"nfo:horizontalResolution\": 321," \
	"      \"nfo:codec\": \"png\"," \
	"      \"nfo:interlaceMode\": true," \
	"      \"nie:contentCreated\": \"2023-07-07T01:01:01Z\"" \
	"    }" \
	"  ]" \
	"}"

#define PHOTO_DELETE_SPARQL \
	"DELETE DATA { <http://example.com/a> a rdfs:Resource }"

static TrackerSparqlCursor *
get_cursor (TestFixture *test_fixture,
            const gchar *iri)
{
	g_autoptr (TrackerSparqlStatement) stmt = NULL;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	stmt = tracker_sparql_connection_query_statement (test_fixture->conn,
	                                                  "SELECT ?p ?o { ~iri ?p ?o } order by ?p ?o",
	                                                  NULL, &error);
	g_assert_no_error (error);

	tracker_sparql_statement_bind_string (stmt, "iri", iri);
	cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
	g_assert_no_error (error);

	return cursor;
}

static void
assert_in_graph (TestFixture *test_fixture,
		 const gchar *graph,
		 const gchar *iri)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gchar *query;
	gboolean retval;

	query = g_strdup_printf ("ASK { GRAPH <%s> { <%s> a rdfs:Resource } }",
				 graph, iri);
	cursor = tracker_sparql_connection_query (test_fixture->conn, query, NULL, &error);
	g_assert_no_error (error);
	g_free (query);

	retval = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (retval);

	g_assert_true (tracker_sparql_cursor_get_boolean (cursor, 0));

	g_object_unref (cursor);
}

static void
assert_no_match (TestFixture *test_fixture,
                 const gchar *iri)
{
	g_autoptr (TrackerSparqlCursor) cursor = NULL;
	GError *error = NULL;

	cursor = get_cursor (test_fixture, iri);
	g_assert_false (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);
}

static TrackerSparqlStatement *
create_photo_stmt (TestFixture *test_fixture)
{
	TrackerSparqlStatement *stmt;
	GError *error = NULL;

	stmt = tracker_sparql_connection_update_statement (test_fixture->conn,
	                                                   "DELETE WHERE {"
	                                                   "  ~iri a rdfs:Resource ."
	                                                   "};"
	                                                   "INSERT DATA {"
	                                                   "  ~iri a nmm:Photo ; "
	                                                   "    nmm:exposureTime ~exposure ; "
	                                                   "    nfo:horizontalResolution ~resolution ; "
	                                                   "    nfo:codec ~codec ; "
	                                                   "    nfo:interlaceMode ~interlaced ; "
	                                                   "    nie:contentCreated ~created . "
	                                                   "}",
	                                                   NULL, &error);
	g_assert_no_error (error);

	return stmt;
}

static TrackerSparqlStatement *
create_photo_del_stmt (TestFixture *test_fixture)
{
	TrackerSparqlStatement *stmt;
	GError *error = NULL;

	stmt = tracker_sparql_connection_update_statement (test_fixture->conn,
	                                                   "DELETE WHERE {"
	                                                   "  ~iri a rdfs:Resource ."
	                                                   "}",
	                                                   NULL, &error);
	g_assert_no_error (error);

	return stmt;
}

static TrackerResource *
create_photo_resource (TestFixture *test_fixture,
                       const gchar *iri,
                       const gchar *codec,
                       GDateTime   *date,
                       gboolean     interlaced,
                       gint         horizontal_res,
                       gdouble      exposure_time)
{
	TrackerResource *resource;

	resource = tracker_resource_new (iri);
	tracker_resource_set_uri (resource, "rdf:type", "nmm:Photo");
	tracker_resource_set_double (resource, "nmm:exposureTime", exposure_time);
	tracker_resource_set_int64 (resource, "nfo:horizontalResolution", horizontal_res);
	tracker_resource_set_string (resource, "nfo:codec", codec);
	tracker_resource_set_boolean (resource, "nfo:interlaceMode", interlaced);
	tracker_resource_set_datetime (resource, "nie:contentCreated", date);

	return resource;
}

static void
assert_photo (TestFixture *test_fixture,
              const gchar *iri,
              const gchar *codec,
              GDateTime   *date,
              gboolean     interlaced,
              gint         horizontal_res,
              gdouble      exposure_time)
{
	TrackerSparqlCursor *cursor;
	GDateTime *cursor_date;
	GError *error = NULL;

	cursor = get_cursor (test_fixture, iri);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, TRACKER_PREFIX_DC "date");
	cursor_date = tracker_sparql_cursor_get_datetime (cursor, 1);
	g_assert_true (g_date_time_compare (cursor_date, date) == 0);
	g_date_time_unref (cursor_date);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, TRACKER_PREFIX_NFO "codec");
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 1, NULL), ==, codec);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, TRACKER_PREFIX_NFO "horizontalResolution");
	g_assert_cmpint (tracker_sparql_cursor_get_integer (cursor, 1), ==, horizontal_res);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, TRACKER_PREFIX_NFO "interlaceMode");
	g_assert_cmpint (tracker_sparql_cursor_get_boolean (cursor, 1), ==, interlaced);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, TRACKER_PREFIX_NIE "contentCreated");
	cursor_date = tracker_sparql_cursor_get_datetime (cursor, 1);
	g_assert_true (g_date_time_compare (cursor_date, date) == 0);
	g_date_time_unref (cursor_date);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, TRACKER_PREFIX_NIE "informationElementDate");
	cursor_date = tracker_sparql_cursor_get_datetime (cursor, 1);
	g_assert_true (g_date_time_compare (cursor_date, date) == 0);
	g_date_time_unref (cursor_date);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, TRACKER_PREFIX_NMM "exposureTime");
	g_assert_cmpfloat_with_epsilon (tracker_sparql_cursor_get_double (cursor, 1), exposure_time, DBL_EPSILON);

	/* Skip over nrl:added, nrl:modified, rdf:type */

	g_assert_no_error (error);
	g_object_unref (cursor);
}

static void
assert_count_bnodes (TestFixture *test_fixture,
                     gint         count)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	cursor = tracker_sparql_connection_query (test_fixture->conn,
	                                          "SELECT COUNT (?u) { ?u a nmm:Photo . FILTER (isBlank(?u)) }",
	                                          NULL, &error);
	g_assert_no_error (error);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);

	g_assert_cmpint (tracker_sparql_cursor_get_integer (cursor, 0), ==, count);

	g_object_unref (cursor);
}

static void
batch_sparql_insert (TestFixture   *test_fixture,
                     gconstpointer  context)
{
	TrackerBatch *batch;
	TrackerSparqlConnection *c;
	GError *error = NULL;
	GDateTime *date;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	g_assert_true (test_fixture->conn == tracker_batch_get_connection (batch));
	g_object_get (batch, "connection", &c, NULL);
	g_assert_true (test_fixture->conn == c);
	g_object_unref (c);

	tracker_batch_add_sparql (batch, PHOTO_INSERT_SPARQL);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);

	assert_photo (test_fixture, "http://example.com/a", "png", date, FALSE, 123, 0.12345678901);

	g_object_unref (batch);
	g_date_time_unref (date);
}

static void
batch_sparql_delete (TestFixture   *test_fixture,
                     gconstpointer  context)
{
	TrackerBatch *batch;
	GError *error = NULL;
	GDateTime *date;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);

	/* Insert item */
	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_sparql (batch, PHOTO_INSERT_SPARQL);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);
	assert_photo (test_fixture, "http://example.com/a", "png", date, FALSE, 123, 0.12345678901);

	/* Delete item in a separate batch */
	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_sparql (batch, PHOTO_DELETE_SPARQL);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);
	assert_no_match (test_fixture, "http://example.com/a");

	g_date_time_unref (date);
}

static void
batch_sparql_delete_same_batch (TestFixture   *test_fixture,
                                gconstpointer  context)
{
	TrackerBatch *batch;
	GError *error = NULL;

	/* Insert item and delete item in the same batch */
	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_sparql (batch, PHOTO_INSERT_SPARQL);
	tracker_batch_add_sparql (batch, PHOTO_DELETE_SPARQL);

	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);
	assert_no_match (test_fixture, "http://example.com/a");
}

static void
batch_sparql_bnodes (TestFixture   *test_fixture,
                     gconstpointer  context)
{
	TrackerBatch *batch;
	GError *error = NULL;

	/* Insert a bnode with the same label in separate
	 *  batches, 2 blank nodes are expected
	 */
	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_sparql (batch, "INSERT { _:bnode a nmm:Photo }");
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_sparql (batch, "INSERT { _:bnode a nmm:Photo }");
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_count_bnodes (test_fixture, 2);
}

static void
batch_sparql_bnodes_same_batch (TestFixture   *test_fixture,
                                gconstpointer  context)
{
	TrackerBatch *batch;
	GError *error = NULL;

	/* Insert a bnode with the same label twice in the same
	 *  batch, 1 blank node is expected
	 */
	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_sparql (batch, "INSERT { _:bnode a nmm:Photo }");
	tracker_batch_add_sparql (batch, "INSERT { _:bnode a nmm:Photo }");

	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_count_bnodes (test_fixture, 1);
}

static void
batch_rdf_turtle (TestFixture   *test_fixture,
                  gconstpointer  context)
{
	TrackerBatch *batch;
	GInputStream *istream;
	GError *error = NULL;
	GDateTime *date;

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	istream = g_memory_input_stream_new_from_data (PHOTO_TURTLE, -1, NULL);
	g_assert_no_error (error);
	g_assert_nonnull (istream);

	tracker_batch_add_rdf (batch,
	                       TRACKER_DESERIALIZE_FLAGS_NONE,
	                       TRACKER_RDF_FORMAT_TURTLE,
	                       NULL,
	                       istream);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	date = g_date_time_new_from_iso8601 ("2023-07-07T01:01:01Z", NULL);
	assert_photo (test_fixture, "http://example.com/turtle", "png", date, TRUE, 321, 3.456789012);
	g_date_time_unref (date);
}

static void
batch_rdf_turtle_graph (TestFixture   *test_fixture,
                        gconstpointer  context)
{
	TrackerBatch *batch;
	GInputStream *istream;
	GError *error = NULL;
	GDateTime *date;

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	istream = g_memory_input_stream_new_from_data (PHOTO_TURTLE, -1, NULL);
	g_assert_no_error (error);
	g_assert_nonnull (istream);

	/* Specify a graph in the batch call, and check that Turtle data gets added there */
	tracker_batch_add_rdf (batch,
	                       TRACKER_DESERIALIZE_FLAGS_NONE,
	                       TRACKER_RDF_FORMAT_TURTLE,
	                       TRACKER_PREFIX_NRL "TestGraph",
	                       istream);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	date = g_date_time_new_from_iso8601 ("2023-07-07T01:01:01Z", NULL);
	assert_photo (test_fixture, "http://example.com/turtle", "png", date, TRUE, 321, 3.456789012);
	assert_in_graph (test_fixture, TRACKER_PREFIX_NRL "TestGraph", "http://example.com/turtle");
	g_date_time_unref (date);
}

static void
batch_rdf_trig (TestFixture   *test_fixture,
                gconstpointer  context)
{
	TrackerBatch *batch;
	GInputStream *istream;
	GError *error = NULL;
	GDateTime *date;

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	istream = g_memory_input_stream_new_from_data (PHOTO_TRIG, -1, NULL);
	g_assert_no_error (error);
	g_assert_nonnull (istream);

	tracker_batch_add_rdf (batch,
	                       TRACKER_DESERIALIZE_FLAGS_NONE,
	                       TRACKER_RDF_FORMAT_TRIG,
	                       NULL,
	                       istream);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	date = g_date_time_new_from_iso8601 ("2023-07-07T01:01:01Z", NULL);
	assert_photo (test_fixture, "http://example.com/trig", "png", date, TRUE, 321, 3.456789012);
	assert_in_graph (test_fixture, "http://example.com/test", "http://example.com/trig");
	g_date_time_unref (date);
}

static void
batch_rdf_trig_graph (TestFixture   *test_fixture,
                      gconstpointer  context)
{
	TrackerBatch *batch;
	GInputStream *istream;
	GError *error = NULL;
	GDateTime *date;

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	istream = g_memory_input_stream_new_from_data (PHOTO_TRIG, -1, NULL);
	g_assert_no_error (error);
	g_assert_nonnull (istream);

	/* Specify a graph in the batch call, and check that Trig graph shall
	 * get honored anyways. This is our documented behavior.
	 */
	tracker_batch_add_rdf (batch,
	                       TRACKER_DESERIALIZE_FLAGS_NONE,
	                       TRACKER_RDF_FORMAT_TRIG,
	                       TRACKER_PREFIX_NRL "TestGraph",
	                       istream);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	date = g_date_time_new_from_iso8601 ("2023-07-07T01:01:01Z", NULL);
	assert_photo (test_fixture, "http://example.com/trig", "png", date, TRUE, 321, 3.456789012);
	assert_in_graph (test_fixture, "http://example.com/test", "http://example.com/trig");
	g_date_time_unref (date);
}

static void
batch_rdf_jsonld (TestFixture   *test_fixture,
                  gconstpointer  context)
{
	TrackerBatch *batch;
	GInputStream *istream;
	GError *error = NULL;
	GDateTime *date;

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	istream = g_memory_input_stream_new_from_data (PHOTO_JSONLD, -1, NULL);
	g_assert_no_error (error);
	g_assert_nonnull (istream);

	tracker_batch_add_rdf (batch,
	                       TRACKER_DESERIALIZE_FLAGS_NONE,
	                       TRACKER_RDF_FORMAT_JSON_LD,
	                       NULL,
	                       istream);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	date = g_date_time_new_from_iso8601 ("2023-07-07T01:01:01Z", NULL);
	assert_photo (test_fixture, "http://example.com/jsonld", "png", date, TRUE, 321, 3.456789012);
	g_date_time_unref (date);
}

static void
batch_rdf_jsonld_graph (TestFixture   *test_fixture,
                        gconstpointer  context)
{
	TrackerBatch *batch;
	GInputStream *istream;
	GError *error = NULL;
	GDateTime *date;

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	istream = g_memory_input_stream_new_from_data (PHOTO_JSONLD, -1, NULL);
	g_assert_no_error (error);
	g_assert_nonnull (istream);

	/* Specify a graph in the batch call, and check that Turtle data gets added there */
	tracker_batch_add_rdf (batch,
	                       TRACKER_DESERIALIZE_FLAGS_NONE,
	                       TRACKER_RDF_FORMAT_JSON_LD,
	                       TRACKER_PREFIX_NRL "TestGraph",
	                       istream);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	date = g_date_time_new_from_iso8601 ("2023-07-07T01:01:01Z", NULL);
	assert_photo (test_fixture, "http://example.com/jsonld", "png", date, TRUE, 321, 3.456789012);
	assert_in_graph (test_fixture, TRACKER_PREFIX_NRL "TestGraph", "http://example.com/jsonld");
	g_date_time_unref (date);
}

static void
batch_resource_insert (TestFixture   *test_fixture,
                       gconstpointer  context)
{
	TrackerBatch *batch;
	TrackerResource *resource;
	GError *error = NULL;
	GDateTime *date;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	resource = create_photo_resource (test_fixture, "http://example.com/b", "png", date, FALSE, 123, 0.12345678901);

	tracker_batch_add_resource (batch, NULL, resource);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);

	assert_photo (test_fixture, "http://example.com/b", "png", date, FALSE, 123, 0.12345678901);

	g_object_unref (batch);
	g_date_time_unref (date);
	g_object_unref (resource);
}

static void
batch_resource_update (TestFixture   *test_fixture,
                       gconstpointer  context)
{
	TrackerBatch *batch;
	TrackerResource *resource;
	GError *error = NULL;
	GDateTime *date;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);

	/* Insert photo */
	resource = create_photo_resource (test_fixture, "http://example.com/c", "png", date, TRUE, 234, 1.23456789012);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_resource (batch, NULL, resource);
	g_object_unref (resource);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_photo (test_fixture, "http://example.com/c", "png", date, TRUE, 234, 1.23456789012);

	/* Modify photo in another batch */
	resource = create_photo_resource (test_fixture, "http://example.com/c", "png", date, FALSE, 123, 0.12345678901);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_resource (batch, NULL, resource);
	g_object_unref (resource);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_photo (test_fixture, "http://example.com/c", "png", date, FALSE, 123, 0.12345678901);

	g_date_time_unref (date);
}

static void
batch_resource_update_same_batch (TestFixture   *test_fixture,
                                  gconstpointer  context)
{
	TrackerBatch *batch;
	TrackerResource *resource;
	GError *error = NULL;
	GDateTime *date;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	resource = create_photo_resource (test_fixture, "http://example.com/d", "png", date, TRUE, 234, 1.23456789012);
	tracker_batch_add_resource (batch, NULL, resource);
	g_object_unref (resource);

	resource = create_photo_resource (test_fixture, "http://example.com/d", "png", date, FALSE, 123, 0.12345678901);
	tracker_batch_add_resource (batch, NULL, resource);
	g_object_unref (resource);

	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);

	assert_photo (test_fixture, "http://example.com/d", "png", date, FALSE, 123, 0.12345678901);

	g_object_unref (batch);
	g_date_time_unref (date);
}

static void
batch_resource_bnodes (TestFixture   *test_fixture,
                       gconstpointer  context)
{
	TrackerBatch *batch;
	GError *error = NULL;
	TrackerResource *resource;

	/* Insert a bnode with the same label in separate
	 *  batches, 2 blank nodes are expected
	 */
	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	resource = tracker_resource_new ("_:bnode");
	tracker_resource_set_uri (resource, "rdf:type", "nmm:Photo");
	tracker_batch_add_resource (batch, NULL, resource);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);
	g_object_unref (resource);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	resource = tracker_resource_new ("_:bnode");
	tracker_resource_set_uri (resource, "rdf:type", "nmm:Photo");
	tracker_batch_add_resource (batch, NULL, resource);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);
	g_object_unref (resource);

	assert_count_bnodes (test_fixture, 2);
}

static void
batch_resource_bnodes_same_batch (TestFixture   *test_fixture,
                                  gconstpointer  context)
{
	TrackerBatch *batch;
	GError *error = NULL;
	TrackerResource *resource;

	/* Insert a bnode with the same label twice in the same
	 *  batch, 1 blank node is expected
	 */
	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	resource = tracker_resource_new ("_:bnode");
	tracker_resource_set_uri (resource, "rdf:type", "nmm:Photo");
	tracker_batch_add_resource (batch, NULL, resource);
	g_object_unref (resource);

	resource = tracker_resource_new ("_:bnode");
	tracker_resource_set_uri (resource, "rdf:type", "nmm:Photo");
	tracker_batch_add_resource (batch, NULL, resource);
	g_object_unref (resource);

	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_count_bnodes (test_fixture, 1);
}

static void
batch_resource_graph (TestFixture   *test_fixture,
		      gconstpointer  context)
{
	TrackerBatch *batch;
	TrackerResource *resource;
	GError *error = NULL;
	GDateTime *date;

	date = g_date_time_new_from_iso8601 ("2023-12-04T01:01:01Z", NULL);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	resource = create_photo_resource (test_fixture, "http://example.com/resource_graph", "png", date, FALSE, 123, 0.12345678901);

	tracker_batch_add_resource (batch, TRACKER_PREFIX_NRL "TestGraph", resource);

	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);

	assert_photo (test_fixture, "http://example.com/resource_graph", "png", date, FALSE, 123, 0.12345678901);
	assert_in_graph (test_fixture, TRACKER_PREFIX_NRL "TestGraph", "http://example.com/resource_graph");

	g_object_unref (batch);
	g_date_time_unref (date);
	g_object_unref (resource);
}

static void
batch_statement_insert (TestFixture   *test_fixture,
                        gconstpointer  context)
{
	TrackerBatch *batch;
	g_autoptr (TrackerSparqlStatement) stmt = NULL;
	GError *error = NULL;
	GDateTime *date;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	stmt = create_photo_stmt (test_fixture);

	tracker_batch_add_statement (batch, stmt,
	                             "iri", G_TYPE_STRING, "http://example.com/e",
	                             "codec", G_TYPE_STRING, "png",
	                             "interlaced", G_TYPE_BOOLEAN, FALSE,
	                             "exposure", G_TYPE_DOUBLE, 0.12345678901,
	                             "resolution", G_TYPE_INT64, (gint64) 123,
	                             "created", G_TYPE_DATE_TIME, date,
	                             NULL);

	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_photo (test_fixture, "http://example.com/e", "png", date, FALSE, 123, 0.12345678901);

	g_date_time_unref (date);
}

static void
batch_statement_update (TestFixture   *test_fixture,
                        gconstpointer  context)
{
	TrackerBatch *batch;
	g_autoptr (TrackerSparqlStatement) stmt = NULL;
	GError *error = NULL;
	GDateTime *date;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);
	stmt = create_photo_stmt (test_fixture);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_statement (batch, stmt,
	                             "iri", G_TYPE_STRING, "http://example.com/f",
	                             "codec", G_TYPE_STRING, "jpeg",
	                             "interlaced", G_TYPE_BOOLEAN, TRUE,
	                             "exposure", G_TYPE_DOUBLE, 1.23456789012,
	                             "resolution", G_TYPE_INT64, (gint64) 234,
	                             "created", G_TYPE_DATE_TIME, date,
	                             NULL);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_photo (test_fixture, "http://example.com/f", "jpeg", date, TRUE, 234, 1.23456789012);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_statement (batch, stmt,
	                             "iri", G_TYPE_STRING, "http://example.com/f",
	                             "codec", G_TYPE_STRING, "png",
	                             "interlaced", G_TYPE_BOOLEAN, FALSE,
	                             "exposure", G_TYPE_DOUBLE, 0.12345678901,
	                             "resolution", G_TYPE_INT64, (gint64) 123,
	                             "created", G_TYPE_DATE_TIME, date,
	                             NULL);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_photo (test_fixture, "http://example.com/f", "png", date, FALSE, 123, 0.12345678901);

	g_date_time_unref (date);
}

static void
batch_statement_update_same_batch (TestFixture   *test_fixture,
                                   gconstpointer  context)
{
	TrackerBatch *batch;
	g_autoptr (TrackerSparqlStatement) stmt = NULL;
	GError *error = NULL;
	GDateTime *date;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);
	stmt = create_photo_stmt (test_fixture);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_statement (batch, stmt,
	                             "iri", G_TYPE_STRING, "http://example.com/g",
	                             "codec", G_TYPE_STRING, "jpeg",
	                             "interlaced", G_TYPE_BOOLEAN, TRUE,
	                             "exposure", G_TYPE_DOUBLE, 1.23456789012,
	                             "resolution", G_TYPE_INT64, (gint64) 234,
	                             "created", G_TYPE_DATE_TIME, date,
	                             NULL);
	tracker_batch_add_statement (batch, stmt,
	                             "iri", G_TYPE_STRING, "http://example.com/g",
	                             "codec", G_TYPE_STRING, "png",
	                             "interlaced", G_TYPE_BOOLEAN, FALSE,
	                             "exposure", G_TYPE_DOUBLE, 0.12345678901,
	                             "resolution", G_TYPE_INT64, (gint64) 123,
	                             "created", G_TYPE_DATE_TIME, date,
	                             NULL);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_photo (test_fixture, "http://example.com/g", "png", date, FALSE, 123, 0.12345678901);

	g_date_time_unref (date);
}

static void
batch_statement_delete (TestFixture   *test_fixture,
                        gconstpointer  context)
{
	TrackerBatch *batch;
	g_autoptr (TrackerSparqlStatement) stmt = NULL;
	g_autoptr (TrackerSparqlStatement) del_stmt = NULL;
	GError *error = NULL;
	GDateTime *date;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);
	stmt = create_photo_stmt (test_fixture);
	del_stmt = create_photo_del_stmt (test_fixture);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_statement (batch, stmt,
	                             "iri", G_TYPE_STRING, "http://example.com/h",
	                             "codec", G_TYPE_STRING, "png",
	                             "interlaced", G_TYPE_BOOLEAN, FALSE,
	                             "exposure", G_TYPE_DOUBLE, 0.12345678901,
	                             "resolution", G_TYPE_INT64, (gint64) 123,
	                             "created", G_TYPE_DATE_TIME, date,
	                             NULL);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_photo (test_fixture, "http://example.com/h", "png", date, FALSE, 123, 0.12345678901);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_statement (batch, del_stmt,
	                             "iri", G_TYPE_STRING, "http://example.com/h",
	                             NULL);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_no_match (test_fixture, "http://example.com/h");

	g_date_time_unref (date);
}

static void
batch_statement_delete_same_batch (TestFixture   *test_fixture,
                                   gconstpointer  context)
{
	TrackerBatch *batch;
	g_autoptr (TrackerSparqlStatement) stmt = NULL;
	g_autoptr (TrackerSparqlStatement) del_stmt = NULL;
	GError *error = NULL;
	GDateTime *date;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);
	stmt = create_photo_stmt (test_fixture);
	del_stmt = create_photo_del_stmt (test_fixture);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_statement (batch, stmt,
	                             "iri", G_TYPE_STRING, "http://example.com/i",
	                             "codec", G_TYPE_STRING, "png",
	                             "interlaced", G_TYPE_BOOLEAN, FALSE,
	                             "exposure", G_TYPE_DOUBLE, 0.12345678901,
	                             "resolution", G_TYPE_INT64, (gint64) 123,
	                             "created", G_TYPE_DATE_TIME, date,
	                             NULL);
	tracker_batch_add_statement (batch, del_stmt,
	                             "iri", G_TYPE_STRING, "http://example.com/i",
	                             NULL);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_no_match (test_fixture, "http://example.com/i");

	g_date_time_unref (date);
}

static void
batch_statement_bnodes (TestFixture   *test_fixture,
                        gconstpointer  context)
{
	g_autoptr (TrackerSparqlStatement) stmt = NULL;
	TrackerBatch *batch;
	GError *error = NULL;

	stmt = tracker_sparql_connection_update_statement (test_fixture->conn,
	                                                   "INSERT {"
	                                                   "  _:bnode a nmm:Photo . "
	                                                   "}",
	                                                   NULL, &error);

	/* Insert a bnode with the same label in separate
	 *  batches, 2 blank nodes are expected
	 */
	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_statement (batch, stmt, NULL);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_statement (batch, stmt, NULL);
	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_count_bnodes (test_fixture, 2);
}

static void
batch_statement_bnodes_same_batch (TestFixture   *test_fixture,
                                   gconstpointer  context)
{
	g_autoptr (TrackerSparqlStatement) stmt = NULL;
	TrackerBatch *batch;
	GError *error = NULL;

	stmt = tracker_sparql_connection_update_statement (test_fixture->conn,
	                                                   "INSERT {"
	                                                   "  _:bnode a nmm:Photo . "
	                                                   "}",
	                                                   NULL, &error);

	/* Insert a bnode with the same label twice in the same
	 *  batch, 1 blank node is expected
	 */
	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	tracker_batch_add_statement (batch, stmt, NULL);
	tracker_batch_add_statement (batch, stmt, NULL);

	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_count_bnodes (test_fixture, 1);
}

static void
batch_bnodes (TestFixture   *test_fixture,
              gconstpointer  context)
{
	TrackerBatch *batch;
	TrackerResource *resource;
	TrackerSparqlStatement *stmt;
	GError *error = NULL;
	GDateTime *date;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	tracker_batch_add_sparql (batch,
	                          "INSERT {"
	                          "  _:bnode a nmm:Photo . "
	                          "  [] a nmm:Photo . "
	                          "}");

	resource = tracker_resource_new ("_:bnode");
	tracker_resource_set_uri (resource, "rdf:type", "nmm:Photo");
	tracker_batch_add_resource (batch, NULL, resource);
	g_object_unref (resource);

	resource = tracker_resource_new (NULL);
	tracker_resource_set_uri (resource, "rdf:type", "nmm:Photo");
	tracker_batch_add_resource (batch, NULL, resource);
	g_object_unref (resource);

	stmt = tracker_sparql_connection_update_statement (test_fixture->conn,
	                                                   "INSERT {"
	                                                   "  _:bnode a nmm:Photo . "
	                                                   "  [] a nmm:Photo . "
	                                                   "}",
	                                                   NULL, &error);
	g_assert_no_error (error);

	tracker_batch_add_statement (batch, stmt, NULL);
	tracker_batch_add_statement (batch, stmt, NULL);
	g_object_unref (stmt);

	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	assert_count_bnodes (test_fixture, 5);

	g_date_time_unref (date);
}

static void
batch_mixed_rdf (TestFixture   *test_fixture,
                 gconstpointer  context)
{
	TrackerBatch *batch;
	GInputStream *istream;
	GError *error = NULL;
	GDateTime *date;

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);

	istream = g_memory_input_stream_new_from_data (PHOTO_TURTLE, -1, NULL);
	g_assert_no_error (error);
	g_assert_nonnull (istream);

	tracker_batch_add_rdf (batch,
	                       TRACKER_DESERIALIZE_FLAGS_NONE,
	                       TRACKER_RDF_FORMAT_TURTLE,
	                       NULL,
	                       istream);
	g_object_unref (istream);

	istream = g_memory_input_stream_new_from_data (PHOTO_TRIG, -1, NULL);
	g_assert_no_error (error);
	g_assert_nonnull (istream);

	tracker_batch_add_rdf (batch,
	                       TRACKER_DESERIALIZE_FLAGS_NONE,
	                       TRACKER_RDF_FORMAT_TRIG,
	                       NULL,
	                       istream);
	g_object_unref (istream);

	istream = g_memory_input_stream_new_from_data (PHOTO_JSONLD, -1, NULL);
	g_assert_no_error (error);
	g_assert_nonnull (istream);

	tracker_batch_add_rdf (batch,
	                       TRACKER_DESERIALIZE_FLAGS_NONE,
	                       TRACKER_RDF_FORMAT_JSON_LD,
	                       NULL,
	                       istream);
	g_object_unref (istream);

	tracker_batch_execute (batch, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (batch);

	date = g_date_time_new_from_iso8601 ("2023-07-07T01:01:01Z", NULL);
	assert_photo (test_fixture, "http://example.com/turtle", "png", date, TRUE, 321, 3.456789012);
	assert_photo (test_fixture, "http://example.com/trig", "png", date, TRUE, 321, 3.456789012);
	assert_photo (test_fixture, "http://example.com/jsonld", "png", date, TRUE, 321, 3.456789012);
	g_date_time_unref (date);
}

typedef struct {
	gint count;
	GMainLoop *loop;
} AsyncData;

static void
update_async_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
	AsyncData *data = user_data;
	GError *error = NULL;

	g_assert_true (tracker_batch_execute_finish (TRACKER_BATCH (source), res, &error));
	g_assert_no_error (error);

	data->count--;
	if (data->count == 0)
		g_main_loop_quit (data->loop);
}

static void
batch_async_simultaneous (TestFixture   *test_fixture,
                          gconstpointer  context)
{
	TrackerBatch *batch1, *batch2;
	TrackerResource *resource;
	GDateTime *date;
	AsyncData data;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);

	/* Ensure batches get both executed, despite asynchronously */
	batch1 = tracker_sparql_connection_create_batch (test_fixture->conn);
	resource = create_photo_resource (test_fixture, "http://example.com/j", "png", date, TRUE, 234, 1.23456789012);
	tracker_batch_add_resource (batch1, NULL, resource);
	g_object_unref (resource);

	batch2 = tracker_sparql_connection_create_batch (test_fixture->conn);
	resource = create_photo_resource (test_fixture, "http://example.com/j2", "png", date, FALSE, 123, 0.12345678901);
	tracker_batch_add_resource (batch2, NULL, resource);
	g_object_unref (resource);

	data.count = 2;
	data.loop = g_main_loop_new (NULL, FALSE);

	tracker_batch_execute_async (batch1, NULL, update_async_cb, &data);
	tracker_batch_execute_async (batch2, NULL, update_async_cb, &data);

	g_main_loop_run (data.loop);

	assert_photo (test_fixture, "http://example.com/j", "png", date, TRUE, 234, 1.23456789012);
	assert_photo (test_fixture, "http://example.com/j2", "png", date, FALSE, 123, 0.12345678901);

	g_main_loop_unref (data.loop);
	g_object_unref (batch1);
	g_object_unref (batch2);
	g_date_time_unref (date);
}

static void
batch_async_same_item (TestFixture   *test_fixture,
                       gconstpointer  context)
{
	g_autoptr (TrackerSparqlCursor) cursor = NULL;
	TrackerBatch *batch1, *batch2;
	TrackerResource *resource;
	GDateTime *date;
	AsyncData data;
	GError *error = NULL;

	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);

	/* Ensure changes from both batches get applied to
	 * the same resource, despite asynchronously */
	batch1 = tracker_sparql_connection_create_batch (test_fixture->conn);
	resource = tracker_resource_new ("http://example.com/j3");
	tracker_resource_add_uri (resource, "rdf:type", "nie:DataObject");
	tracker_batch_add_resource (batch1, NULL, resource);
	g_object_unref (resource);

	batch2 = tracker_sparql_connection_create_batch (test_fixture->conn);
	resource = tracker_resource_new ("http://example.com/j3");
	tracker_resource_add_uri (resource, "rdf:type", "nie:InformationElement");
	tracker_batch_add_resource (batch2, NULL, resource);
	g_object_unref (resource);

	data.count = 2;
	data.loop = g_main_loop_new (NULL, FALSE);

	tracker_batch_execute_async (batch1, NULL, update_async_cb, &data);
	tracker_batch_execute_async (batch2, NULL, update_async_cb, &data);

	g_main_loop_run (data.loop);

	cursor = get_cursor (test_fixture, "http://example.com/j3");
	/* nrl:added */
	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	/* nrl:modified */
	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	/* rdf:type */
	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, TRACKER_PREFIX_RDF "type");
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 1, NULL), ==, TRACKER_PREFIX_NIE "DataObject");
	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, TRACKER_PREFIX_RDF "type");
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 1, NULL), ==, TRACKER_PREFIX_NIE "InformationElement");
	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, TRACKER_PREFIX_RDF "type");
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 1, NULL), ==, TRACKER_PREFIX_RDFS "Resource");
	g_assert_false (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);

	g_main_loop_unref (data.loop);
	g_object_unref (batch1);
	g_object_unref (batch2);
	g_date_time_unref (date);
}

static void
batch_transaction_error (TestFixture   *test_fixture,
                         gconstpointer  context)
{
	TrackerBatch *batch;
	TrackerResource *resource;
	GError *error = NULL;
	GDateTime *date;

	/* Ensure rollback on errors */
	date = g_date_time_new_from_iso8601 ("2022-12-04T01:01:01Z", NULL);

	batch = tracker_sparql_connection_create_batch (test_fixture->conn);
	resource = create_photo_resource (test_fixture, "http://example.com/k", "png", date, TRUE, 234, 1.23456789012);
	tracker_batch_add_resource (batch, NULL, resource);
	g_object_unref (resource);

	tracker_batch_add_sparql (batch, "I am not sparql!");

	resource = create_photo_resource (test_fixture, "http://example.com/l", "png", date, FALSE, 123, 0.12345678901);
	tracker_batch_add_resource (batch, NULL, resource);
	g_object_unref (resource);

	tracker_batch_execute (batch, NULL, &error);
	g_assert_error (error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_PARSE);

	assert_no_match (test_fixture, "http://example.com/k");
	assert_no_match (test_fixture, "http://example.com/l");

	g_object_unref (batch);
	g_date_time_unref (date);
	g_error_free (error);
}


TrackerSparqlConnection *
create_local_connection (GError **error)
{
	TrackerSparqlConnection *conn;
	GFile *ontology;

	ontology = tracker_sparql_get_ontology_nepomuk ();

	conn = tracker_sparql_connection_new (0, NULL, ontology, NULL, error);

	g_object_unref (ontology);

	return conn;
}

static gpointer
thread_func (gpointer user_data)
{
	StartupData *data = user_data;
	TrackerEndpointDBus *endpoint;
	GMainContext *context;
	GMainLoop *main_loop;

	context = g_main_context_new ();
	g_main_context_push_thread_default (context);

	main_loop = g_main_loop_new (context, FALSE);

	endpoint = tracker_endpoint_dbus_new (data->sparql_conn,
					      data->dbus_conn,
					      data->object_path, NULL, NULL);
	g_assert_nonnull (endpoint);

	data->thread_loop = main_loop;
	data->started = TRUE;
	g_main_loop_run (main_loop);

	g_main_loop_unref (main_loop);
	g_main_context_pop_thread_default (context);
	g_main_context_unref (context);

	g_object_unref (endpoint);
	g_object_unref (data->sparql_conn);
	g_free (data);

	return NULL;
}

static void
finish_endpoint (EndpointData *endpoint_data)
{
	g_main_loop_quit (endpoint_data->loop);
	g_thread_join (endpoint_data->thread);
	g_free (endpoint_data);
}

static TrackerSparqlConnection *
create_dbus_connection (GDBusConnection  *dbus_conn,
                        const char       *object_path,
                        GError          **error)
{
	TrackerSparqlConnection *dbus;
	StartupData *data;
	EndpointData *endpoint_data;
	GThread *thread;
	GError *inner_error = NULL;

	data = g_new0 (StartupData, 1);
	data->object_path = object_path;
	data->dbus_conn = dbus_conn;

	data->sparql_conn = create_local_connection (&inner_error);
	g_assert_no_error (inner_error);
	g_assert_nonnull (data->sparql_conn);

	thread = g_thread_new (NULL, thread_func, data);

	while (!data->started)
		g_usleep (100);

	endpoint_data = g_new0 (EndpointData, 1);
	endpoint_data->thread = thread;
	endpoint_data->loop = data->thread_loop;

	bus_name = g_dbus_connection_get_unique_name (data->dbus_conn);
	dbus = tracker_sparql_connection_bus_new (bus_name,
						  data->object_path,
						  data->dbus_conn, error);
	g_assert_nonnull (dbus);

	g_object_set_data_full (G_OBJECT (dbus),
				"endpoint-data",
				endpoint_data,
				(GDestroyNotify) finish_endpoint);

	return dbus;
}

static TrackerSparqlConnection *
create_dbus_over_dbus_connection (GDBusConnection  *dbus_conn,
                                  GError          **error)
{
	TrackerSparqlConnection *dbus_over_dbus;
	StartupData *data;
	EndpointData *endpoint_data;
	GThread *thread;
	GError *inner_error = NULL;

	data = g_new0 (StartupData, 1);
	data->object_path = "/org/test/nested2";
	data->dbus_conn = dbus_conn;

	data->sparql_conn = create_dbus_connection (dbus_conn,
						    "/org/test/nested1",
	                                            &inner_error);
	g_assert_no_error (inner_error);
	g_assert_nonnull (data->sparql_conn);

	thread = g_thread_new (NULL, thread_func, data);

	while (!data->started)
		g_usleep (100);

	endpoint_data = g_new0 (EndpointData, 1);
	endpoint_data->thread = thread;
	endpoint_data->loop = data->thread_loop;

	bus_name = g_dbus_connection_get_unique_name (data->dbus_conn);
	dbus_over_dbus = tracker_sparql_connection_bus_new (bus_name,
							    data->object_path,
							    data->dbus_conn, error);

	g_object_set_data_full (G_OBJECT (dbus_over_dbus),
				"endpoint-data",
				endpoint_data,
				(GDestroyNotify) finish_endpoint);

	return dbus_over_dbus;
}

static void
setup (TestFixture   *fixture,
       gconstpointer  context)
{
	const TestFixture *test = context;
	GError *error = NULL;

	*fixture = *test;

	tracker_sparql_connection_update (fixture->conn,
	                                  "DELETE {"
	                                  "  ?u a rdfs:Resource ."
	                                  "} WHERE {"
	                                  "  ?u a nmm:Photo ."
	                                  "};"
	                                  "CLEAR SILENT GRAPH nrl:TestGraph;"
	                                  "CLEAR SILENT GRAPH <http://example.com/test>",
	                                  NULL, &error);
	g_assert_no_error (error);
}

static void
teardown (TestFixture   *fixture,
          gconstpointer  context)
{
	g_object_unref (fixture->conn);
}

TestInfo tests[] = {
	{ "sparql/insert", batch_sparql_insert },
	{ "sparql/delete", batch_sparql_delete },
	{ "sparql/delete-same-batch", batch_sparql_delete_same_batch },
	{ "sparql/bnodes", batch_sparql_bnodes },
	{ "sparql/bnodes-same-batch", batch_sparql_bnodes_same_batch },
	{ "rdf/turtle", batch_rdf_turtle },
	{ "rdf/turtle-graph", batch_rdf_turtle_graph },
	{ "rdf/trig", batch_rdf_trig },
	{ "rdf/trig-graph", batch_rdf_trig_graph },
	{ "rdf/json-ld", batch_rdf_jsonld },
	{ "rdf/json-ld-graph", batch_rdf_jsonld_graph },
	{ "resource/insert", batch_resource_insert },
	{ "resource/update", batch_resource_update },
	{ "resource/update-same-batch", batch_resource_update_same_batch },
	{ "resource/bnodes", batch_resource_bnodes },
	{ "resource/bnodes-same-batch", batch_resource_bnodes_same_batch },
	{ "resource/graph", batch_resource_graph },
	{ "statement/insert", batch_statement_insert },
	{ "statement/update", batch_statement_update },
	{ "statement/update-same-batch", batch_statement_update_same_batch },
	{ "statement/delete", batch_statement_delete },
	{ "statement/delete-same-batch", batch_statement_delete_same_batch },
	{ "statement/bnodes", batch_statement_bnodes },
	{ "statement/bnodes-same-batch", batch_statement_bnodes_same_batch },
	{ "mixed/bnodes", batch_bnodes },
	{ "mixed/rdf", batch_mixed_rdf },
	{ "async/simultaneous", batch_async_simultaneous },
	{ "async/same-item", batch_async_same_item },
	{ "error/transaction", batch_transaction_error },
};

static void
add_tests (TrackerSparqlConnection *conn,
           const gchar             *name,
           gboolean                 run_service_tests)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		TestFixture *fixture;
		gchar *testpath;

		fixture = g_new0 (TestFixture, 1);
		fixture->conn = g_object_ref (conn);
		fixture->test = &tests[i];
		testpath = g_strconcat ("/libtracker-sparql/batch/", name, "/", tests[i].test_name, NULL);
		g_test_add (testpath, TestFixture, fixture, setup, tests[i].func, teardown);
		g_free (testpath);
	}
}

gint
main (gint argc, gchar **argv)
{
	TrackerSparqlConnection *dbus_over_dbus = NULL, *dbus = NULL, *direct = NULL;
	GDBusConnection *dbus_conn;
	GError *error = NULL;

	g_test_init (&argc, &argv, NULL);

	dbus_conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	g_assert_no_error (error);

	direct = create_local_connection (&error);
	g_assert_no_error (error);
	dbus = create_dbus_connection (dbus_conn, "/org/test/object3", &error);
	g_assert_no_error (error);
	dbus_over_dbus = create_dbus_over_dbus_connection (dbus_conn, &error);
	g_assert_no_error (error);

	add_tests (direct, "direct", TRUE);
	add_tests (dbus, "dbus", FALSE);
	add_tests (dbus_over_dbus, "dbus-over-dbus", FALSE);

	g_clear_object (&dbus_over_dbus);
	g_clear_object (&dbus);
	g_clear_object (&direct);

	return g_test_run ();
}
