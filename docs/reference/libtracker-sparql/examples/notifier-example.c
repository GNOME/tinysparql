#include <tinysparql.h>

#define REMOTE_NAME "org.freedesktop.Tracker3.Miner.Files"

void on_events (TrackerNotifier *notifier,
                const char      *service,
                const char      *graph,
                GPtrArray       *events,
                gpointer         user_data)
{
  int i;

  for (i = 0; i < events->len; i++) {
    TrackerNotifierEvent *event = g_ptr_array_index (events, i);

    g_print ("Event %d on %s\n",
             tracker_notifier_event_get_event_type (event),
             tracker_notifier_event_get_urn (event));
  }
}

int main (int   argc,
          char *argv[])
{
  g_autoptr (GError) error = NULL;
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  g_autoptr (TrackerNotifier) notifier = NULL;
  g_autoptr (GMainLoop) main_loop = NULL;

  /* Create connection to remote service */
  connection = tracker_sparql_connection_bus_new (REMOTE_NAME, NULL, NULL, &error);
  if (!connection) {
    g_printerr ("Couldn't obtain a connection to the Tracker store: %s",
                error ? error->message : "unknown error");
    return 1;
  }

  /* Create notifier, and connect to its events signal */
  notifier = tracker_sparql_connection_create_notifier (connection);
  g_signal_connect (notifier, "events", G_CALLBACK (on_events), NULL);

  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);

  tracker_sparql_connection_close (connection);

  return 0;
}
