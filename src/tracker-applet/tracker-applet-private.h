#ifndef __TRACKER_MONITOR_TRAY_ICON_PRIVATE_H__
#define __TRACKER_MONITOR_TRAY_ICON_PRIVATE_H__

#include <libnotify/notify.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>

#include "tracker.h"


#define TRAY_ICON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TYPE_TRAY_ICON, TrayIconPrivate))

typedef enum _Properties
{
   PROP_TRACKERD_CONNECTION = 1
} Properties;

typedef struct _TrayIconPrivate
{
	/* main window */
   	GtkStatusIcon 		*icon;
   	GtkMenu 		*menu;
	GtkWindow		*window;
	GtkWidget 		*search_entry;	
	GtkWidget 		*status_label;
	GtkWidget 		*progress_bar;
	GtkWidget 		*email_progress_bar;
	GtkWidget 		*count_label;
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
_set_tooltip(TrayIcon *icon, const gchar *tooltip);

static void
tray_icon_clicked (GtkStatusIcon *icon, guint button, guint timestamp, gpointer data);

static void
preferences_menu_activated (GtkMenuItem *item, gpointer data);

static void
statistics_menu_activated (GtkMenuItem *item, gpointer data);

static void
quit_menu_activated (GtkMenuItem *item, gpointer data);

#endif
