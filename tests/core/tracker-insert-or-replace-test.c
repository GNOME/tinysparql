#include "config.h"

#include <string.h>
#include <locale.h>

#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include <libtracker-sparql/tracker-sparql.h>

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

int
main (int argc, char *argv[])
{
	TrackerSparqlConnection *conn;
	GError *error = NULL;
	GFile *cache, *ontology;
	gchar *test_data_dir;
	gchar *query;
	gint i;

	g_print ("1..1\n");
	test_data_dir = g_build_filename (g_get_tmp_dir (),
	                                  "insert-or-replace-test-data-XXXXXX",
	                                  NULL);
	g_mkdtemp (test_data_dir);

	cache = g_file_new_for_path (test_data_dir);
	ontology = g_file_new_for_path (TEST_ONTOLOGIES_DIR);

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

	g_print ("ok 1 /core/insert-or-replace\n");

	return 0;
}
