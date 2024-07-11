#include "config.h"

#include <string.h>
#include <locale.h>

#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include <tinysparql.h>

static gchar *insert_query_replace = "\
DELETE { ?r nao:hasTag ?tag . }\
WHERE {\
	?r a nco:PhoneNumber;\
	   nco:phoneNumber \"02141730585%d\";\
	   nao:hasTag ?tag .\
}\
\
DELETE {\
	?r a nco:CarPhoneNumber, nco:BbsNumber, nco:PagerNumber,\
	     nco:VideoTelephoneNumber, nco:MessagingNumber, nco:VoicePhoneNumber,\
	     nco:CellPhoneNumber, nco:FaxNumber, nco:ModemNumber .\
} WHERE {\
	?r a nco:PhoneNumber;\
	   nco:phoneNumber \"02141730585%d\" .\
}\
\
INSERT {\
	_:tel a nco:PhoneNumber ;\
	      nco:phoneNumber \"02141730585%d\" .\
} WHERE {\
	OPTIONAL {\
		?r a nco:PhoneNumber;\
		   nco:phoneNumber \"02141730585%d\" .\
	}\
	FILTER(!bound(?r)) .\
}\
\
INSERT OR REPLACE { <mailto:rhome0@example.com%d> a nco:EmailAddress ;\
		nco:emailAddress \"rhome0@example.com%d\" . }\
\
DELETE { <contact:r:%d> nco:hasAffiliation ?e . ?e a rdfs:Resource }\
WHERE { <contact:r:%d> a nco:PersonContact ; nco:hasAffiliation ?e }\
\
INSERT OR REPLACE {\
	_:af1 a nco:Affiliation ;\
	      rdfs:label \"Work\" ;\
	      nco:hasEmailAddress <mailto:rhome0@example.com%d> .\
\
	_:af2 a nco:Affiliation ;\
	      rdfs:label \"Other\" ;\
	      nco:hasPhoneNumber ?tel .\
\
	<contact:r:%d> a nco:PersonContact ;\
	               nco:nameGiven \"First %d\" ;\
	               nco:nameFamily \"Last %d\" ;\
	               nco:hasAffiliation _:af1 ;\
	               nco:hasAffiliation _:af2 ;\
	               nie:contentCreated \"2011-03-14T13:47:25\" ;\
	               nco:contactUID \"c1f1b12d-bc75-4d45-9a1f-b1efe934409f\" .\
} WHERE {\
	?tel nco:phoneNumber \"02141730585%d\"\
}";

static gchar *insert_query_original = "\
DELETE { ?r nao:tag ?tag . }\
WHERE {\
	?r a nco:PhoneNumber; nco:phoneNumber \"2141730585%d\";\
	   nao:hasTag ?tag .\
}\
\
DELETE {\
	?r a nco:CarPhoneNumber, nco:BbsNumber, nco:PagerNumber,\
	     nco:VideoTelephoneNumber, nco:MessagingNumber, nco:VoicePhoneNumber,\
	     nco:CellPhoneNumber, nco:FaxNumber, nco:ModemNumber .\
} WHERE {\
	?r a nco:PhoneNumber;\
	   nco:phoneNumber \"2141730585%d\" .\
}\
\
INSERT {\
	_:tel a nco:PhoneNumber ;\
	      nco:phoneNumber \"2141730585%d\" .\
} WHERE {\
	OPTIONAL {\
		?r a nco:PhoneNumber;\
		   nco:phoneNumber \"2141730585%d\" .\
	}\
	FILTER(!bound(?r)) .\
}\
\
INSERT { <mailto:home0@example.com%d> a nco:EmailAddress ;\
			nco:emailAddress \"home0@example.com%d\" . }\
\
DELETE { <contact:o:%d> nco:hasAffiliation ?e . ?e a rdfs:Resource }\
WHERE { <contact:o:%d> a nco:PersonContact ; nco:hasAffiliation ?e }\
\
DELETE { GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9> {\
    <contact:o:%d> ?predicate ?object .\
} } WHERE { GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9> {\
    <contact:o:%d> ?predicate ?object .\
    FILTER(?predicate NOT IN (nco:contactLocalUID,nco:contactUID,rdf:type)) .\
} }\
\
INSERT { GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9> {\
	_:af1 a nco:Affiliation ;\
	      rdfs:label \"Work\" ;\
	      nco:hasEmailAddress <mailto:home0@example.com%d> .\
\
	_:af2 a nco:Affiliation ;\
	      rdfs:label \"Other\" ;\
	      nco:hasPhoneNumber ?tel .\
\
	<contact:o:%d> a nco:PersonContact ;\
	               nco:nameGiven \"First %d\" ;\
	               nco:nameFamily \"Last %d\" ;\
	               nco:hasAffiliation _:af1 ;\
	               nco:hasAffiliation _:af2 ;\
	               nie:contentCreated \"2011-03-14T13:47:25\" ;\
	               nco:contactUID \"c1f1b12d-bc75-4d45-9a1f-b1efe934409f\" .\
} } WHERE {\
	?tel nco:phoneNumber \"2141730585%d\"\
}";

#define N_QUERIES 10

void
test_insert_or_replace (void)
{
	TrackerSparqlConnection *conn;
	GError *error = NULL;
	GFile *cache, *ontology;
	gchar *test_data_dir;
	gchar *query;
	gint i;

	test_data_dir = g_build_filename (g_get_tmp_dir (),
	                                  "insert-or-replace-test-data-XXXXXX",
	                                  NULL);
	g_mkdtemp (test_data_dir);

	cache = g_file_new_for_path (test_data_dir);
	ontology = tracker_sparql_get_ontology_nepomuk ();

	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      cache,
	                                      ontology,
	                                      NULL, &error);
	g_assert_no_error (error);

	for (i = 0; i < N_QUERIES; i++) {
		query = g_strdup_printf (insert_query_replace,
		                         i, i, i, i, i, i, i, i, i , i, i, i, i);
		tracker_sparql_connection_update (conn,
		                                  query,
		                                  NULL, &error);
		g_assert_no_error (error);
		g_free (query);
	}

	for (i = 0; i < N_QUERIES; i++) {
		query = g_strdup_printf (insert_query_original,
		                         i, i, i, i, i, i, i, i, i, i, i, i, i, i, i);
		tracker_sparql_connection_update (conn,
		                                  query,
		                                  NULL, &error);
		g_assert_no_error (error);
		g_free (query);
	}

	g_object_unref (conn);
	g_object_unref (cache);
	g_object_unref (ontology);
}

static void
test_insert_or_replace_where (void)
{
	TrackerSparqlConnection *conn;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GFile *ontology;
	gboolean next;

	ontology = tracker_sparql_get_ontology_nepomuk ();

	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      NULL,
	                                      ontology,
	                                      NULL, &error);
	g_assert_no_error (error);

	tracker_sparql_connection_update (conn,
	                                  "INSERT { <playlist:/a> a nmm:Playlist ; nie:title 'foo'; nfo:entryCounter 0 } ;"
	                                  "INSERT OR REPLACE { _:entry a nfo:MediaFileListEntry ; nfo:entryUrl 'file:///' ; nfo:listPosition ?position . ?playlist nfo:entryCounter ?position ; nfo:hasMediaFileListEntry _:entry } "
	                                  "WHERE { SELECT ?playlist ((?counter + 1) AS ?position) { ?playlist a nmm:Playlist ; a nfo:MediaList ; nfo:entryCounter ?counter . FILTER (?playlist = <playlist:/a>) } }",
	                                  NULL, &error);
	g_assert_no_error (error);

	cursor = tracker_sparql_connection_query (conn, "ASK { <playlist:/a> nfo:entryCounter 1 }", NULL, &error);
	g_assert_no_error (error);

	next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_true (next);
	g_assert_no_error (error);

	g_assert_true (tracker_sparql_cursor_get_boolean (cursor, 0));

	g_object_unref (conn);
	g_object_unref (ontology);
	g_object_unref (cursor);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/core/insert-replace", test_insert_or_replace);
	g_test_add_func ("/core/insert-replace-where", test_insert_or_replace_where);

	return g_test_run ();
}
