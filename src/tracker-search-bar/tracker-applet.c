/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <panel-applet.h>

#include "tracker-applet.h"
#include "tracker-results-window.h"

static void applet_about_cb (GtkAction     *action,
                             TrackerApplet *applet);

static const GtkActionEntry applet_menu_actions[] = {
	{ "About",
	  GTK_STOCK_ABOUT,
	  N_("_About"),
	  NULL,
	  NULL,
	  G_CALLBACK (applet_about_cb)
	}
};

static void
applet_about_cb (GtkAction     *action,
                 TrackerApplet *applet)
{
	GObject *object;
	GtkWidget *dialog;

	object = gtk_builder_get_object (applet->builder, "dialog_about");
	g_return_if_fail (object != NULL);

	dialog = GTK_WIDGET (object);

	g_signal_connect_swapped (object,
	                          "response",
	                          G_CALLBACK (gtk_widget_destroy),
	                          dialog);

	gtk_widget_show_all (dialog);
}

static gboolean
applet_event_box_button_press_event_cb (GtkWidget      *widget,
                                        GdkEventButton *event,
                                        TrackerApplet  *applet)
{
	if (applet->results) {
		gtk_widget_destroy (applet->results);
		applet->results = NULL;
		return TRUE;
	}

	return FALSE;
}

static void
applet_entry_start_search (TrackerApplet *applet)
{
	const gchar *text;

	text = gtk_entry_get_text (GTK_ENTRY (applet->entry));

        if (!text || !*text) {
                gtk_widget_hide (applet->results);
                return;
        }

	g_print ("Searching for: '%s'\n", text);

	if (!applet->results) {
		applet->results = tracker_results_window_new (applet->parent, text);
	} else {
		g_object_set (applet->results, "query", text, NULL);
	}

	if (!GTK_WIDGET_VISIBLE (applet->results)) {
		tracker_results_window_popup (TRACKER_RESULTS_WINDOW (applet->results));
	}
}

static gboolean
applet_entry_start_search_cb (gpointer user_data)
{
	TrackerApplet *applet;

	applet = user_data;

	applet->new_search_id = 0;

	applet_entry_start_search (applet);

	return FALSE;
}

static void
applet_entry_activate_cb (GtkEntry      *entry,
                          TrackerApplet *applet)
{
	if (applet->new_search_id) {
		g_source_remove (applet->new_search_id);
		applet->new_search_id = 0;
	}

	applet_entry_start_search (applet);
}

static gboolean
applet_entry_button_press_event_cb (GtkWidget      *widget,
                                    GdkEventButton *event,
                                    TrackerApplet  *applet)
{
	panel_applet_request_focus (PANEL_APPLET (applet->parent), event->time);

	return FALSE;
}

static void
applet_entry_editable_changed_cb (GtkWidget     *widget,
                                  TrackerApplet *applet)
{
        if (applet->new_search_id) {
                g_source_remove (applet->new_search_id);
        }

        applet->new_search_id =
                g_timeout_add (300,
                               applet_entry_start_search_cb,
                               applet);
}

static gboolean
applet_entry_key_press_event_cb (GtkWidget     *widget,
                                 GdkEventKey   *event,
                                 TrackerApplet *applet)
{
	if (event->keyval == GDK_Escape) {
		if (!applet->results) {
			return FALSE;
		}

		gtk_widget_destroy (applet->results);
		applet->results = NULL;
	} else if (event->keyval == GDK_Down) {
		if (!applet->results) {
			return FALSE;
		}

		gtk_widget_grab_focus (applet->results);
	}

	return FALSE;
}

static gboolean
applet_draw (gpointer user_data)
{
	TrackerApplet *applet = user_data;

	if (applet->box) {
		gtk_widget_destroy (applet->box);
	}

	switch (applet->orient) {
	case GTK_ORIENTATION_VERTICAL:
		applet->box = gtk_vbox_new (FALSE, 0);
		break;
	case GTK_ORIENTATION_HORIZONTAL:
		applet->box = gtk_hbox_new (FALSE, 0);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	gtk_container_add (GTK_CONTAINER (PANEL_APPLET (applet->parent)), applet->box);
	gtk_widget_show (applet->box);

	applet->event_box = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (applet->event_box), FALSE);
	gtk_widget_show (applet->event_box);
	gtk_box_pack_start (GTK_BOX (applet->box), applet->event_box, FALSE, FALSE, 0);

	g_signal_connect (applet->event_box,
	                  "button_press_event",
	                  G_CALLBACK (applet_event_box_button_press_event_cb), applet);

	applet->image = gtk_image_new ();
	gtk_container_add (GTK_CONTAINER (applet->event_box), applet->image);
	gtk_image_set_from_stock (GTK_IMAGE (applet->image),
	                          GTK_STOCK_FIND,
	                          GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_widget_show (applet->image);

	applet->entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (applet->box), applet->entry, TRUE, TRUE, 0);
	gtk_entry_set_width_chars (GTK_ENTRY (applet->entry), 12);
	gtk_widget_show (applet->entry);

	g_signal_connect (applet->entry,
	                  "activate",
	                  G_CALLBACK (applet_entry_activate_cb), applet);
	g_signal_connect (applet->entry,
	                  "button_press_event",
	                  G_CALLBACK (applet_entry_button_press_event_cb), applet);
        g_signal_connect (applet->entry,
                          "changed",
                          G_CALLBACK (applet_entry_editable_changed_cb), applet);
	g_signal_connect (applet->entry,
	                  "key_press_event",
	                  G_CALLBACK (applet_entry_key_press_event_cb), applet);

	applet->idle_draw_id = 0;

	return FALSE;
}

static void
applet_queue_draw (TrackerApplet *applet)
{
	if (!applet->idle_draw_id)
		applet->idle_draw_id = g_idle_add (applet_draw, applet);
}

static void
applet_change_orient_cb (GtkWidget         *widget,
                         PanelAppletOrient  orient,
                         gpointer           user_data)
{
	TrackerApplet *applet;
	guint new_size;

	applet = user_data;
        new_size = applet->size;

	switch (orient) {
	case PANEL_APPLET_ORIENT_LEFT:
	case PANEL_APPLET_ORIENT_RIGHT:
		applet->orient = GTK_ORIENTATION_VERTICAL;
		new_size = GTK_WIDGET (applet->parent)->allocation.width;
		break;

	case PANEL_APPLET_ORIENT_UP:
	case PANEL_APPLET_ORIENT_DOWN:
		applet->orient = GTK_ORIENTATION_HORIZONTAL;
		new_size = GTK_WIDGET (applet->parent)->allocation.height;
		break;
	}

	if (new_size != applet->size) {
		applet->size = new_size;
	}

	applet_queue_draw (applet);
}

static void
applet_size_allocate_cb (GtkWidget     *widget,
                         GtkAllocation *allocation,
                         gpointer       user_data)
{
	TrackerApplet *applet;
	PanelAppletOrient orient;
	gint new_size;

	applet = user_data;

	orient = panel_applet_get_orient (PANEL_APPLET (widget));
	if (orient == PANEL_APPLET_ORIENT_LEFT ||
	    orient == PANEL_APPLET_ORIENT_RIGHT) {
		new_size = allocation->width;
	} else {
		new_size = allocation->height;
	}

	if (applet->size != new_size) {
		applet->size = new_size;

		gtk_image_set_pixel_size (GTK_IMAGE (applet->image), applet->size - 2);

		/* re-scale the icon, if it was found */
		if (applet->icon)       {
			GdkPixbuf *scaled;

			scaled = gdk_pixbuf_scale_simple (applet->icon,
			                                  applet->size - 5,
			                                  applet->size - 5,
			                                  GDK_INTERP_BILINEAR);

			gtk_image_set_from_pixbuf (GTK_IMAGE (applet->image), scaled);
			g_object_unref (scaled);
		}
	}

}

#if 0

static void
applet_destroy_cb (BonoboObject  *object,
                   TrackerApplet *applet)
{
	if (applet->builder) {
		g_object_unref (applet->builder);
		applet->builder = NULL;
	}

	if (applet->results) {
		gtk_widget_destroy (applet->results);
		applet->results = NULL;
	}

	if (applet->icon) {
		g_object_unref (applet->icon);
		applet->icon = NULL;
	}

	if (applet->idle_draw_id) {
		g_source_remove (applet->idle_draw_id);
		applet->idle_draw_id = 0;
	}

	if (applet->new_search_id) {
		g_source_remove (applet->new_search_id);
		applet->new_search_id = 0;
	}

	g_free (applet);
}

#endif

static gboolean
applet_new (PanelApplet *parent_applet)
{
	TrackerApplet *applet;
	GError *error = NULL;
	GtkBuilder *builder;
	GtkActionGroup *action_group;
	gchar *ui_path;

	builder = gtk_builder_new ();
	ui_path = g_build_filename (PKGDATADIR,
	                            "tracker-search-bar.ui",
	                            NULL);

	if (gtk_builder_add_from_file (builder, ui_path, &error) == 0) {
		g_printerr ("Could not load builder file, %s", error->message);
		g_error_free (error);
		g_free (ui_path);
		g_object_unref (builder);

		return FALSE;
	}

	g_print ("Added builder file:'%s'\n", ui_path);
	g_free (ui_path);

	applet = g_new0 (TrackerApplet, 1);

	applet->parent = GTK_WIDGET (parent_applet);
	applet->builder = builder;

	applet->icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
	                                         GTK_STOCK_FIND,
	                                         48,
	                                         0,
	                                         NULL);

	applet_queue_draw (applet);

	panel_applet_set_flags (PANEL_APPLET (applet->parent),
	                        PANEL_APPLET_EXPAND_MINOR);
	panel_applet_set_background_widget (PANEL_APPLET (applet->parent),
	                                    GTK_WIDGET (applet->parent));

	action_group = gtk_action_group_new ("Applet Actions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
	                              applet_menu_actions,
	                              G_N_ELEMENTS (applet_menu_actions),
	                              applet);
	ui_path = g_build_filename (PKGDATADIR,
	                            "tracker-search-bar-menu.xml",
	                            NULL);
	panel_applet_setup_menu_from_file (PANEL_APPLET (applet->parent),
	                                   ui_path,
	                                   action_group);
	g_free (ui_path);
	g_object_unref (action_group);

	gtk_widget_show (applet->parent);

	g_signal_connect (applet->parent, "size_allocate",
	                  G_CALLBACK (applet_size_allocate_cb), applet);
	g_signal_connect (applet->parent, "change_orient",
	                  G_CALLBACK (applet_change_orient_cb), applet);

#if 0
	g_signal_connect (panel_applet_get_control (PANEL_APPLET (applet->parent)), "destroy",
	                  G_CALLBACK (applet_destroy_cb), applet);
#endif

	/* Initialise other modules */

	return TRUE;
}

/*
 * The entry point for this factory. If the OAFIID matches, create an instance
 * of the applet.
 */
static gboolean
applet_factory (PanelApplet *applet,
                const gchar *iid,
                gpointer     data)
{
	if (!strcmp (iid, "SearchBar")) {
		g_print ("Creating applet\n");
		return applet_new (applet);
	}

	return FALSE;
}

/*
 * Generate the boilerplate to hook into GObject/Bonobo.
 */
PANEL_APPLET_OUT_PROCESS_FACTORY ("SearchBarFactory",
                                  PANEL_TYPE_APPLET,
                                  "SearchBar",
                                  applet_factory,
                                  NULL)
