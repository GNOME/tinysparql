#include "config.h"

#include <tinysparql.h>

#define N_ITERS 10

static TrackerSparqlConnection *
open_database (const char *data_dir)
{
	TrackerSparqlConnection *conn;
	GFile *data_location, *ontology;
	GError *error = NULL;

	data_location = g_file_new_for_path (data_dir);

	ontology = tracker_sparql_get_ontology_nepomuk ();
	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      data_location,
	                                      ontology,
	                                      NULL, &error);
	g_assert_no_error (error);

	g_object_unref (ontology);
	g_object_unref (data_location);

	return conn;
}

static void
close_database (TrackerSparqlConnection *conn,
		gboolean                 delete)
{
	GFile *cache;

	g_object_get (conn, "store-location", &cache, NULL);

	tracker_sparql_connection_close (conn);
	g_object_unref (conn);

	if (delete) {
		const char *children[] = { "meta.db", "meta.db-wal", "meta.db-shm" };
		guint i;

		for (i = 0; i < G_N_ELEMENTS (children); i++) {
			GFile *child;

			child = g_file_get_child (cache, children[i]);
			g_file_delete (child, NULL, NULL);
			g_object_unref (child);
		}

		g_assert_true (g_file_delete (cache, NULL, NULL));
	}

	g_object_unref (cache);
}

static gboolean
resource_exists (TrackerSparqlConnection *conn,
		 const char              *urn)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	char *query;
	gint64 id;

	query = g_strdup_printf ("SELECT (tracker:id(<%s>) AS ?id) {}", urn);
	cursor = tracker_sparql_connection_query (conn, query, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);

	id = tracker_sparql_cursor_get_integer (cursor, 0);
	g_object_unref (cursor);

	return id != 0;
}

void
test_rdfs_resource_refcount (void)
{
	TrackerSparqlConnection *conn;
	GError *error = NULL;
	gchar *data_dir;
	int i;

	data_dir = g_build_filename (g_get_tmp_dir (), "tracker-refcount-test-XXXXXX", NULL);
	data_dir = g_mkdtemp_full (data_dir, 0700);

	conn = open_database (data_dir);

	for (i = 0; i < N_ITERS; i++) {
		tracker_sparql_connection_update (conn, "INSERT DATA { <foo:a> a nie:InformationElement }",
						  NULL, &error);
		g_assert_no_error (error);
	}

	g_assert_true (resource_exists (conn, "foo:a"));

	/* Delete nie:InformationElement subclass, still should be a rdfs:Resource */
	tracker_sparql_connection_update (conn, "DELETE DATA { <foo:a> a nie:InformationElement }",
					  NULL, &error);
	g_assert_no_error (error);
	g_assert_true (resource_exists (conn, "foo:a"));

	close_database (conn, FALSE);
	conn = open_database (data_dir);

	/* Delete resource completely */
	tracker_sparql_connection_update (conn, "DELETE DATA { <foo:a> a rdfs:Resource }",
					  NULL, &error);
	g_assert_no_error (error);

	/* Reopen database to ensure resource cleanup,
	 * expect that resource IRI does not exist after it
	 */
	close_database (conn, FALSE);
	conn = open_database (data_dir);
	g_assert_false (resource_exists (conn, "foo:a"));

	close_database (conn, TRUE);
	g_free (data_dir);
}

void
test_single_property_refcount (void)
{
	TrackerSparqlConnection *conn;
	GError *error = NULL;
	gchar *data_dir;
	int i;

	data_dir = g_build_filename (g_get_tmp_dir (), "tracker-refcount-test-XXXXXX", NULL);
	data_dir = g_mkdtemp_full (data_dir, 0700);

	conn = open_database (data_dir);

	for (i = 0; i < N_ITERS; i++) {
		tracker_sparql_connection_update (conn,
						  "INSERT DATA { <foo:a> a nie:InformationElement ; nco:publisher <foo:b> ; nie:rootElementOf <foo:c> }",
						  NULL, &error);
		g_assert_no_error (error);
	}

	close_database (conn, FALSE);
	conn = open_database (data_dir);
	g_assert_true (resource_exists (conn, "foo:a"));
	g_assert_true (resource_exists (conn, "foo:b"));
	g_assert_true (resource_exists (conn, "foo:c"));

	for (i = 0; i < N_ITERS; i++) {
		/* Delete property holding ref to foo:b */
		tracker_sparql_connection_update (conn,
						  "DELETE DATA { <foo:a> nco:publisher <foo:b> }",
						  NULL, &error);
		g_assert_no_error (error);
	}

	/* Reopen database to ensure resource cleanup,
	 * expect that resource IRI does not exist after it
	 */
	close_database (conn, FALSE);
	conn = open_database (data_dir);
	g_assert_true (resource_exists (conn, "foo:a"));
	g_assert_false (resource_exists (conn, "foo:b"));
	g_assert_true (resource_exists (conn, "foo:c"));

	/* Delete resource completely, should indirectly drop last ref on foo:c */
	tracker_sparql_connection_update (conn, "DELETE DATA { <foo:a> a rdfs:Resource }",
					  NULL, &error);
	g_assert_no_error (error);

	/* Reopen database to ensure resource cleanup,
	 * expect that resource IRIs do not exist after it
	 */
	close_database (conn, FALSE);
	conn = open_database (data_dir);
	g_assert_false (resource_exists (conn, "foo:a"));
	g_assert_false (resource_exists (conn, "foo:b"));
	g_assert_false (resource_exists (conn, "foo:c"));

	close_database (conn, TRUE);
	g_free (data_dir);
}

void
test_multi_property_refcount (void)
{
	TrackerSparqlConnection *conn;
	GError *error = NULL;
	gchar *data_dir;
	int i;

	data_dir = g_build_filename (g_get_tmp_dir (), "tracker-refcount-test-XXXXXX", NULL);
	data_dir = g_mkdtemp_full (data_dir, 0700);

	conn = open_database (data_dir);

	for (i = 0; i < N_ITERS; i++) {
		/* nie:depends is an interesting property, as it is a subproperty of multi-valued nie:relatedTo */
		tracker_sparql_connection_update (conn,
						  "INSERT DATA { <foo:a> a nie:InformationElement ; nie:depends <foo:b> ; nie:hasLogicalPart <foo:c> }",
						  NULL, &error);
		g_assert_no_error (error);
	}

	close_database (conn, FALSE);
	conn = open_database (data_dir);
	g_assert_true (resource_exists (conn, "foo:a"));
	g_assert_true (resource_exists (conn, "foo:b"));
	g_assert_true (resource_exists (conn, "foo:c"));

	for (i = 0; i < N_ITERS; i++) {
		/* Delete property holding ref to foo:b */
		tracker_sparql_connection_update (conn,
						  "DELETE DATA { <foo:a> nie:depends <foo:b> }",
						  NULL, &error);
		g_assert_no_error (error);
	}

	/* Reopen database to ensure resource cleanup,
	 * expect that resource IRI does not exist after it
	 */
	close_database (conn, FALSE);
	conn = open_database (data_dir);
	g_assert_true (resource_exists (conn, "foo:a"));
	g_assert_false (resource_exists (conn, "foo:b"));
	g_assert_true (resource_exists (conn, "foo:c"));

	/* Delete resource completely, should indirectly drop last ref on foo:c */
	tracker_sparql_connection_update (conn, "DELETE DATA { <foo:a> a rdfs:Resource }",
					  NULL, &error);
	g_assert_no_error (error);

	/* Reopen database to ensure resource cleanup,
	 * expect that resource IRIs do not exist after it
	 */
	close_database (conn, FALSE);
	conn = open_database (data_dir);
	g_assert_false (resource_exists (conn, "foo:a"));
	g_assert_false (resource_exists (conn, "foo:b"));
	g_assert_false (resource_exists (conn, "foo:c"));

	close_database (conn, TRUE);
	g_free (data_dir);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/core/rdfs-resource-refcount", test_rdfs_resource_refcount);
	g_test_add_func ("/core/single-property-refcount", test_single_property_refcount);
	g_test_add_func ("/core/multi-property-refcount", test_multi_property_refcount);

	return g_test_run ();
}
