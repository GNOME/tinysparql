/* Tracker GUI - A graphical front-end for Tracker
 *
 * Copyright (C) 2006, Jaime Frutos Morales (acidborg@gmail.com)    
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA. 
 */

#include <string.h>
#include <gtk/gtk.h> 
#include <glade/glade.h>
#include "sexy-icon-entry.h"
#include "../libtracker/tracker.h"

static void
fill_service_combobox_cb (gpointer, gpointer, gpointer);

static gchar *service_icons[12][2] = {
   { "Applications",      "application-x-executable" },
   { "Conversations",     "face-smile-big" },
   { "Contacts",          "x-office-address-book" },
   { "Development Files", "applications-development" },
   { "Documents",         "x-office-document" },
   { "Emails",            "email" },
   { "Files",             "system-file-manager" },
   { "Images",            "image-x-generic" },
   { "Music",             "audio-x-generic" },
   { "Text Files",        "text-x-generic" },
   { "Videos",            "video-x-generic" },
   { NULL,                NULL }
};

int main (int argc, char *argv[])
{
	GladeXML         *glade;
	GtkWidget        *main_window;
	GtkWidget        *details_text_view;
    	GtkWidget        *search_entry;
    	GtkTextBuffer    *details_text_buffer;
    	GtkComboBox      *service_combo;
    	GtkListStore     *service_store;
    	GtkCellRenderer  *cell;
    	GError           *error;

	TrackerClient    *client;
	GHashTable       *services;


	gtk_init (&argc, &argv);

	client = tracker_connect (FALSE);
	if (!client) {
       		g_print ("Could not initialise Tracker - exiting...\n");
		return 1;
    	}

    
    	/*Glade stuff*/
    	glade = glade_xml_new ("tracker.glade", NULL, NULL);
    	glade_xml_signal_autoconnect (glade);

    	main_window = glade_xml_get_widget (glade, "main_window");

    	search_entry = glade_xml_get_widget (glade, "search_entry");
    
	details_text_view = glade_xml_get_widget (glade,"details_text_view");
	details_text_buffer = gtk_text_buffer_new (NULL);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW (details_text_view), details_text_buffer);
    
	sexy_icon_entry_add_clear_button (SEXY_ICON_ENTRY (search_entry));

    	service_combo = GTK_COMBO_BOX (glade_xml_get_widget (glade, "combobox_services"));
    	service_store = gtk_list_store_new (2, GDK_TYPE_PIXBUF, G_TYPE_STRING);

    	gtk_combo_box_set_model (service_combo, GTK_TREE_MODEL (service_store));

    	cell = gtk_cell_renderer_pixbuf_new ();
    	gtk_cell_layout_pack_start ( GTK_CELL_LAYOUT (service_combo), cell, FALSE);
    	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (service_combo), cell, "pixbuf", 0);

    	cell = gtk_cell_renderer_text_new ();
    	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (service_combo), cell, TRUE);
    	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (service_combo), cell, "text", 1);
    	
	error = NULL;
    	services = tracker_get_services (client, TRUE, &error);
    	printf("%d services\n", g_hash_table_size (services));
    	g_hash_table_foreach (services, fill_service_combobox_cb, service_combo);


    	/*Signals*/
    	g_signal_connect (G_OBJECT (main_window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

    	/*Test*/
    	gtk_text_buffer_set_text(details_text_buffer, "Hello world!", -1);
    
       	gtk_widget_show_all (main_window);

    	gtk_main();

    	return 0;
}

void fill_service_combobox_cb (gpointer key, gpointer value, gpointer user_data)
{
	GtkComboBox   *combo;
	GtkListStore  *store;
	GdkPixbuf     *pixbuf = NULL;
	GtkTreeIter   iter;
	GtkIconTheme  *theme;
	guint         i;
	GError        *error = NULL;

	theme = gtk_icon_theme_get_default ();

	if (strcmp (key, "Other Files") == 0) {
	       	return;
	}

	for(i = 0; i < 11; i++) {
	      	if (strcmp (key, service_icons[i][0]) == 0) {
         		pixbuf = gtk_icon_theme_load_icon (theme, service_icons[i][1],
                                            GTK_ICON_SIZE_MENU,
                                            GTK_ICON_LOOKUP_USE_BUILTIN,
                                            &error);
         		break;
      		}
   	}
   
	combo = GTK_COMBO_BOX (user_data);
	store = gtk_combo_box_get_model (combo);
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, pixbuf, 1, key, -1);

	if (strcmp (key, "Files") == 0) {
		gtk_combo_box_set_active_iter (combo, &iter);
	}
}
