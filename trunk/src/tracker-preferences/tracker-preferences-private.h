/* Tracker - indexer and metadata database engine
 * Copyright (C) 2007, Saleem Abdulrasool (compnerd@gentoo.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __TRACKER_PREFERENCES_PRIVATE_H__
#define __TRACKER_PREFERENCES_PRIVATE_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include "tracker-configuration.h"

#define TRACKER_PREFERENCES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TRACKER_TYPE_PREFERENCES, TrackerPreferencesPrivate))

typedef struct _TrackerPreferencesPrivate {
	GladeXML *gxml;
	TrackerConfiguration *prefs;
	DBusGConnection *connection;
	DBusGProxy *dbus_proxy;
	DBusGProxy *tracker_proxy;
} TrackerPreferencesPrivate;

static void
tracker_preferences_class_init (TrackerPreferencesClass * klass);

static void
tracker_preferences_init (GTypeInstance * instance, gpointer g_class);

static void
tracker_preferences_finalize (GObject * object);

static void
setup_page_general (TrackerPreferences * preferences);

static void
setup_page_files (TrackerPreferences * preferences);

static void
setup_page_emails (TrackerPreferences * preferences);


static void
setup_page_ignored_files (TrackerPreferences * preferences);

static void
setup_page_performance (TrackerPreferences * preferences);

static void
dlgPreferences_Quit (GtkWidget * widget, GdkEvent * event, gpointer data);

static void
cmdHelp_Clicked (GtkWidget * widget, gpointer data);

static void
cmdClose_Clicked (GtkWidget * widget, gpointer data);

static void
cmdAddIndexPath_Clicked (GtkWidget * widget, gpointer data);

static void
cmdRemoveIndexPath_Clicked (GtkWidget * widget, gpointer data);

static void
cmdAddCrawledPath_Clicked (GtkWidget * widget, gpointer data);

static void
cmdRemoveCrawledPath_Clicked (GtkWidget * widget, gpointer data);

static void
cmdAddIndexMailbox_Clicked (GtkWidget * widget, gpointer data);

static void
cmdRemoveIndexMailbox_Clicked (GtkWidget * widget, gpointer data);

static void
cmdAddIgnorePath_Clicked (GtkWidget * widget, gpointer data);

static void
cmdRemoveIgnorePath_Clicked (GtkWidget * widget, gpointer data);

static void
cmdAddIgnorePattern_Clicked (GtkWidget * widget, gpointer data);

static void
cmdRemoveIgnorePattern_Clicked (GtkWidget * widget, gpointer data);


static void
append_item_to_list (TrackerPreferences * dialog, const gchar * const item,
		     const gchar * const widget);

static void
remove_selection_from_list (TrackerPreferences * dialog,
			    const gchar * const widget);

static GSList *
treeview_get_values (GtkTreeView * treeview);

static gint
_strcmp (gconstpointer a, gconstpointer b);

static void
initialize_listview (GtkWidget * treeview);

static void
populate_list (GtkWidget * treeview, GSList * list);

#endif
