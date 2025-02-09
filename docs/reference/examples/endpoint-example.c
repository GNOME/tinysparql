#include <tinysparql.h>

int main (int argc, const char **argv)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  g_autoptr (GFile) ontology = NULL;
  g_autoptr (GMainLoop) main_loop = NULL;

  /* Create a SPARQL store */
  ontology = tracker_sparql_get_ontology_nepomuk ();
  connection = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
					      NULL, /* Database location, NULL creates it in-memory */
					      ontology, /* Ontology location */
					      NULL,
					      &error);
  if (connection) {
    g_printerr ("Couldn't create a Tracker store: %s",
                error->message);
    return 1;
  }

  /* Make the connection available via D-Bus, other applications would be
   * able to make connections to this application's bus name
   */
  endpoint = tracker_endpoint_dbus_new (connection, NULL, NULL, NULL, &error);
  if (!endpoint) {
    g_printerr ("Couldn't create D-Bus endpoint: %s",
                error->message);
    return 1;
  }

  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);

  return 0;
}
