#include <stdlib.h>

#include <gtk/gtk.h>

#include "tracker-preferences.h"

gboolean
terminate (gpointer data)
{
	return TRUE;
}

int
main (int argc, char *argv[])
{
	GMainLoop *loop = NULL;
	TrackerPreferences *preferences = NULL;

	gtk_init (&argc, &argv);
	preferences = tracker_preferences_new ();
	gtk_main ();

	g_object_unref (preferences);

	return EXIT_SUCCESS;
}
