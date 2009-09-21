/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2006-2007 Imendio AB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <bonobo/bonobo-ui-component.h>
#include <panel-applet-gconf.h>

typedef struct {
	GtkBuilder *builder;

	GtkWidget *widget;

	GtkWidget *image;
	GtkWidget *entry;
} Applet;

static void applet_about_cb (BonoboUIComponent *uic,
			     Applet            *applet,
			     const gchar       *verb_name);

static const BonoboUIVerb applet_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("about", applet_about_cb),
	BONOBO_UI_VERB_END
};

static void
applet_about_cb (BonoboUIComponent *uic, 
		 Applet            *applet, 
		 const gchar       *verb_name)
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

static void
applet_entry_activate_cb (GtkEntry *entry,
			  Applet   *applet)
{
	const gchar *text;

	text = gtk_entry_get_text (entry);
	if (strlen (text) < 1) {
		return;
	}

	g_print ("Searching for: '%s'\n", text);

	/* Do something */

	gtk_entry_set_text (entry, "");
}

static gboolean
applet_entry_button_press_event_cb (GtkWidget      *widget, 
				    GdkEventButton *event, 
				    Applet         *applet)
{
	panel_applet_request_focus (PANEL_APPLET (applet->widget), event->time);

	return FALSE;
}

static void
applet_size_allocate_cb (GtkWidget     *widget,
			 GtkAllocation *allocation,
			 Applet        *applet)
{
	PanelAppletOrient orient;
        gint size;
	
	orient = panel_applet_get_orient (PANEL_APPLET (widget));
        if (orient == PANEL_APPLET_ORIENT_LEFT ||
            orient == PANEL_APPLET_ORIENT_RIGHT) {
                size = allocation->width;
        } else {
                size = allocation->height;
        }

        gtk_image_set_pixel_size (GTK_IMAGE (applet->image), size - 2);
}

static void
applet_destroy_cb (BonoboObject *object, 
		   Applet       *applet)
{
	if (applet->builder) {
		g_object_unref (applet->builder);
		applet->builder = NULL;
	}

	g_free (applet);
}

static gboolean
applet_new (PanelApplet *parent_applet)
{
	Applet *applet;
	GError *error = NULL;
	GtkBuilder *builder;
	GtkWidget *hbox;
	const gchar *filename;

	builder = gtk_builder_new ();
	filename = PKGDATADIR "/tracker-search-bar.ui";

	if (gtk_builder_add_from_file (builder, filename, &error) == 0) {
		g_printerr ("Could not load builder file, %s", error->message);
		g_error_free (error);
		g_object_unref (builder);

		return FALSE;
	}

	g_print ("Added builder file:'%s'\n", filename);
  
	applet = g_new0 (Applet, 1);

	applet->widget = GTK_WIDGET (parent_applet);
	applet->builder = builder;

	hbox = gtk_hbox_new (FALSE, 0);  
	gtk_container_add (GTK_CONTAINER (parent_applet), hbox);
	gtk_widget_show (hbox);

	applet->image = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (hbox), applet->image, FALSE, FALSE, 0);
	gtk_image_set_from_stock (GTK_IMAGE (applet->image),
				  GTK_STOCK_FIND,
				  GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_widget_show (applet->image);

	applet->entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), applet->entry, TRUE, TRUE, 0);
	gtk_entry_set_width_chars (GTK_ENTRY (applet->entry), 10);
 	gtk_widget_show (applet->entry); 

	g_signal_connect (applet->entry, 
			  "activate",
			  G_CALLBACK (applet_entry_activate_cb), applet);
	g_signal_connect (applet->entry, 
			  "button_press_event", 
			  G_CALLBACK (applet_entry_button_press_event_cb), applet);

	panel_applet_set_flags (PANEL_APPLET (applet->widget), 
				PANEL_APPLET_EXPAND_MINOR);
	panel_applet_set_background_widget (PANEL_APPLET (applet->widget),
					    GTK_WIDGET (applet->widget));

  	panel_applet_setup_menu_from_file (PANEL_APPLET (applet->widget),
					   NULL,
					   PKGDATADIR "/GNOME_Search_Bar_Applet.xml",
					   NULL,
					   applet_menu_verbs,
					   applet);                               

	gtk_widget_show (applet->widget);

	g_signal_connect (applet->widget,
			  "size_allocate",
			  G_CALLBACK (applet_size_allocate_cb), applet);
	g_signal_connect (panel_applet_get_control (PANEL_APPLET (applet->widget)), 
			  "destroy",
			  G_CALLBACK (applet_destroy_cb), applet);

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
	if (!strcmp (iid, "OAFIID:GNOME_Search_Bar_Applet")) {
		g_print ("Creating applet\n");
		return applet_new (applet);
	}

	return FALSE;
}

/*
 * Generate the boilerplate to hook into GObject/Bonobo.
 */
PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_Search_Bar_Applet_Factory",
                             PANEL_TYPE_APPLET,
                             "GNOME_Search_Bar_Applet", PACKAGE_VERSION,
                             applet_factory,
                             NULL);
