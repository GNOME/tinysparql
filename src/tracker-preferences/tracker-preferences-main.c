#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "tracker-preferences.h"
#include "config.h"

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
        
        bindtextdomain (GETTEXT_PACKAGE, TRACKER_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);
	preferences = tracker_preferences_new ();
	gtk_main ();

	g_object_unref (preferences);

	return EXIT_SUCCESS;
}
