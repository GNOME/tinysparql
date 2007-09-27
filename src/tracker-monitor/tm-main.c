#include <stdlib.h>

#include <gtk/gtk.h>

#include <libnotify/notify.h>

#include "tm-common.h"
#include "tm-tray-icon.h"
#include "tm-trackerd-connection.h"

static TrayIcon *icon;
static TrackerDaemonConnection *trackerd;


static void
set_tooltip(void)
{
   gchar *version = NULL;

   if (trackerd_connection_available(trackerd)) {
      version = trackerd_connection_version(trackerd);

      tray_icon_set_tooltip(icon, "tracker search and indexing monitor");

      g_free(version);
   } else
      tray_icon_set_tooltip(icon, "tracker search and indexing monitor");
}

static void
indexing_started(void)
{
   set_tooltip();
   tray_icon_show_message (icon, "Indexing has started.");
}

static void
indexing_stopped(void)
{
   set_tooltip();
   tray_icon_show_message (icon, "Indexing has stopped.");
}

int
main(int argc, char *argv[])
{
   gtk_init(&argc, &argv);

   if (!notify_is_initted() && !notify_init(PROGRAM_NAME)) {
      g_warning("failed: notify_init()\n");
      return EXIT_FAILURE;
   }

   trackerd = g_object_new(TYPE_TRACKERD_CONNECTION, NULL);
   icon = g_object_new(TYPE_TRAY_ICON, "connection", trackerd, NULL);

   g_signal_connect(trackerd, "started", G_CALLBACK(indexing_started), NULL);
   g_signal_connect(trackerd, "stopped", G_CALLBACK(indexing_stopped), NULL);

   set_tooltip();
	
   gtk_main();

   notify_uninit();
   return EXIT_SUCCESS;
}
