#include <tinysparql.h>

#define REMOTE_NAME "org.freedesktop.Tracker3.Miner.Files"

int main (int argc, const char **argv)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  g_autoptr (TrackerSparqlStatement) stmt = NULL;
  const char *query = "SELECT nie:url(?u) WHERE { ?u a nfo:FileDataObject ; nfo:fileName ~name }";
  const char *name = NULL;
  int i = 0;

  connection = tracker_sparql_connection_bus_new (REMOTE_NAME, NULL, NULL, &error);
  if (!connection) {
    g_printerr ("Couldn't obtain a connection to the Tracker store: %s",
                error ? error->message : "unknown error");
    return 1;
  }

  /* Create a prepared statement */
  stmt = tracker_sparql_connection_query_statement (connection,
                                                    query,
                                                    NULL,
                                                    &error);

  if (!stmt) {
    g_printerr ("Couldn't create a prepared statement: '%s'",
                error->message);
    return 1;
  }

  /* Bind a value to the query variables */
  name = argv[1] ? argv[1] : "";
  tracker_sparql_statement_bind_string (stmt, "name", name);

  /* Executes the SPARQL query with the currently bound values and get new cursor */
  cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
  if (!cursor) {
    g_printerr ("Couldn't execute query: '%s'",
                error->message);
    return 1;
  }

  /* Iterate synchronously the results */
  while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
    g_print ("Result [%d]: %s\n",
             i++,
             tracker_sparql_cursor_get_string (cursor, 0, NULL));
  }

  g_print ("A total of '%d' results were found\n", i);

  tracker_sparql_cursor_close (cursor);

  tracker_sparql_connection_close (connection);

  return 0;
}
