#include <tinysparql.h>

enum {
	NAMESPACE_DEFAULT,
	NAMESPACE_DIRECT,
	NAMESPACE_DBUS,
	NAMESPACE_HTTP,
	N_NAMESPACES
};

char *names[] = {
	"default",
	"direct",
	"dbus",
	"http",
};

G_STATIC_ASSERT (G_N_ELEMENTS (names) == N_NAMESPACES);

static GDBusConnection *dbus_conn = NULL;
static TrackerSparqlConnection *dbus = NULL;
static TrackerSparqlConnection *direct = NULL;
static TrackerSparqlConnection *remote = NULL;
static gboolean started = FALSE;

static TrackerNamespaceManager *
get_namespace (int namespace)
{
	TrackerNamespaceManager *manager;

	switch (namespace) {
	case NAMESPACE_DEFAULT:
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		manager = tracker_namespace_manager_get_default ();
		G_GNUC_END_IGNORE_DEPRECATIONS
		break;
	case NAMESPACE_DIRECT:
		manager = tracker_sparql_connection_get_namespace_manager (direct);
		break;
	case NAMESPACE_DBUS:
		manager = tracker_sparql_connection_get_namespace_manager (dbus);
		break;
	case NAMESPACE_HTTP:
		manager = tracker_sparql_connection_get_namespace_manager (remote);
		break;
	default:
		g_assert_not_reached ();
	}

	return manager;
}

TrackerSparqlConnection *
create_local_connection (void)
{
	TrackerSparqlConnection *conn;
	GFile *ontology;
	GError *error = NULL;

	ontology = tracker_sparql_get_ontology_nepomuk ();

	conn = tracker_sparql_connection_new (0, NULL, ontology, NULL, &error);
	g_object_unref (ontology);
	g_assert_no_error (error);

	return conn;
}

static gpointer
thread_func (gpointer user_data)
{
	TrackerEndpointDBus *endpoint;
	TrackerEndpointHttp *endpoint_http;
	GMainContext *context;
	GMainLoop *main_loop;

	context = g_main_context_new ();
	g_main_context_push_thread_default (context);

	main_loop = g_main_loop_new (context, FALSE);

	endpoint = tracker_endpoint_dbus_new (direct, dbus_conn, NULL, NULL, NULL);
	if (!endpoint)
		return NULL;

	endpoint_http = tracker_endpoint_http_new (direct, 54323, NULL, NULL, NULL);
	if (!endpoint_http)
		return NULL;

	started = TRUE;
	g_main_loop_run (main_loop);

	g_main_loop_unref (main_loop);
	g_main_context_pop_thread_default (context);
	g_main_context_unref (context);

	return NULL;
}

static void
create_connections (void)
{
	GThread *thread;
	GError *error = NULL;
	const char *bus_name;

	direct = create_local_connection ();

	dbus_conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	g_assert_no_error (error);

	thread = g_thread_new (NULL, thread_func, NULL);

	while (!started)
		g_usleep (100);

	bus_name = g_dbus_connection_get_unique_name (dbus_conn);
	dbus = tracker_sparql_connection_bus_new (bus_name,
	                                          NULL, dbus_conn,
	                                          &error);
	g_assert_no_error (error);

	remote = tracker_sparql_connection_remote_new ("http://127.0.0.1:54323/sparql");
	g_thread_unref (thread);
}

static void
namespace_check_foreach (gpointer key,
                         gpointer value,
                         gpointer user_data)
{
	TrackerNamespaceManager *spec = user_data;

	g_assert_true (tracker_namespace_manager_has_prefix (spec, key));
	g_assert_cmpstr (value, ==, tracker_namespace_manager_lookup_prefix (spec, key));
}

static void
compare_namespaces (TrackerNamespaceManager *manager,
		    TrackerNamespaceManager *spec)
{
	tracker_namespace_manager_foreach (manager,
	                                   namespace_check_foreach,
	                                   spec);
	tracker_namespace_manager_foreach (spec,
	                                   namespace_check_foreach,
	                                   manager);
}

static void
test_check_nepomuk (TrackerNamespaceManager *manager)
{
	/* Compare everything to a direct connection with nepomuk ontology */
	TrackerNamespaceManager *spec = get_namespace (NAMESPACE_DIRECT);

	compare_namespaces (manager, spec);
}

static void
test_expand_compress (TrackerNamespaceManager *manager)
{
	char *compressed, *expanded;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	manager = tracker_namespace_manager_get_default ();
	G_GNUC_END_IGNORE_DEPRECATIONS

	expanded = tracker_namespace_manager_expand_uri (manager, "rdfs:Resource");
	compressed = tracker_namespace_manager_compress_uri (manager, expanded);
	g_assert_cmpstr (compressed, ==, "rdfs:Resource");
	g_assert_cmpstr (expanded, ==, "http://www.w3.org/2000/01/rdf-schema#Resource");
	g_free (compressed);
	g_free (expanded);
}

static void
add_test (const gchar             *prefix,
          const gchar             *domain,
          TrackerNamespaceManager *manager,
          GTestDataFunc            func)
{
	char *name;

	name = g_strconcat (prefix, "/", domain, NULL);
	g_test_add_data_func (name, manager, func);
	g_free (name);
}

int
main (int    argc,
      char **argv)
{
	int i;

	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing Tracker namespaces manager");

	create_connections ();

	for (i = NAMESPACE_DEFAULT; i < N_NAMESPACES; i++) {
		TrackerNamespaceManager *namespace = get_namespace (i);

		add_test ("/libtracker-sparql/tracker-namespaces/check",
		          names[i],
		          namespace,
		          (GTestDataFunc) test_check_nepomuk);
		add_test ("/libtracker-sparql/tracker-namespaces/expand-compress",
		          names[i],
		          namespace,
		          (GTestDataFunc) test_expand_compress);
	}

	return g_test_run ();
}
