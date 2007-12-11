#ifndef __TRACKER_MONITOR_TRAY_ICON_PRIVATE_H__
#define __TRACKER_MONITOR_TRAY_ICON_PRIVATE_H__

#include <libnotify/notify.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>

#include "tracker.h"


#define TRAY_ICON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TYPE_TRAY_ICON, TrayIconPrivate))

#define TRACKER_ICON            "tracker-applet-default.png"
#define TRACKER_ICON_PAUSED     "tracker-applet-paused.png"
#define TRACKER_ICON_INDEX1     "tracker-applet-indexing1.png"
#define TRACKER_ICON_INDEX2     "tracker-applet-indexing2.png"


typedef enum {
	ICON_DEFAULT,
	ICON_PAUSED,
	ICON_INDEX1,
	ICON_INDEX2,
} IndexIcon;


static char *index_icons[4] = {TRACKER_ICON, TRACKER_ICON_PAUSED, TRACKER_ICON_INDEX1, TRACKER_ICON_INDEX2};


typedef struct _TrayIconPrivate
{
   	GtkStatusIcon 		*icon;

	/* states */
	gboolean 		indexing;
	gboolean 		paused;
	IndexIcon		index_icon;
	gboolean		animated;

	/* main window */
   	GtkMenu 		*menu;
	GtkWindow		*window;
	GtkWidget 		*search_entry;	
	GtkWidget 		*status_label;
	GtkWidget 		*progress_bar;
	GtkWidget 		*email_progress_bar;
	GtkWidget 		*uri_label;

	gboolean		initial_index_msg_shown;

	/* tracker connection */
	TrackerClient		*tracker;

	/* stats window table shown */
	gboolean		stat_window_active;
	gboolean		stat_request_pending;



} TrayIconPrivate;

static void
tray_icon_class_init (TrayIconClass *klass);

static void
tray_icon_init (GTypeInstance *instance, gpointer g_class);

static void
tray_icon_clicked (GtkStatusIcon *icon, guint button, guint timestamp, gpointer data);

static void
preferences_menu_activated (GtkMenuItem *item, gpointer data);

static void
statistics_menu_activated (GtkMenuItem *item, gpointer data);

static void
quit_menu_activated (GtkMenuItem *item, gpointer data);

#endif
