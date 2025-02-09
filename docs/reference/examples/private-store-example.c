#include <tinysparql.h>

int main (int argc, const char **argv)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  g_autoptr (TrackerResource) resource = NULL;
  g_autoptr (GFile) ontology = NULL;

  /* Create a private SPARQL store */
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

  /* Create a resource containing RDF data */
  resource = tracker_resource_new (NULL);
  tracker_resource_set_uri (resource, "rdf:type", "nmm:MusicPiece");

  /* Create a batch, and add the resource to it */
  batch = tracker_sparql_connection_create_batch (connection);
  tracker_batch_add_resource (batch, NULL, resource);

  /* Execute the batch to insert the data */
  if (!tracker_batch_execute (batch, NULL, &error)) {
    g_printerr ("Couldn't insert batch of resources: %s",
                error->message);
    return 1;
  }

  tracker_sparql_connection_close (connection);

  return 0;
}
