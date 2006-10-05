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

#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <gtk/gtk.h> 
#include <glade/glade.h>
#include <gnome.h>

#include "sexy-icon-entry.h"
#include "../libtracker/tracker.h"

#define MAX_SEARCH_RESULTS 30

static void
fill_services_combo_box (GtkComboBox *);

static void
on_search (GtkWidget *, gpointer);

static void
on_search_results (gchar **, GError *, gpointer);

static void
on_search_results_changed (GtkIconView *, gpointer);

static void
load_thumbnail_thread (gpointer);

typedef struct {
   gchar        *service;
   gchar        *icon_name;
   ServiceType   service_type;
} service_info_t;

typedef struct {
   GtkListStore *store;
   GtkTreeIter  *iter;
} thumbnail_thread_data_t;

static service_info_t services[13] = {
   { "Files",             "system-file-manager",      SERVICE_FILES         },
   { "Folders",           "folder",                   SERVICE_FOLDERS       },
   { "Applications",      "application-x-executable", SERVICE_APPLICATIONS  },
   { "Conversations",     "face-smile-big",           SERVICE_CONVERSATIONS },
   { "Contacts",          "x-office-address-book",    SERVICE_CONTACTS      },
   { "Development Files", "applications-development", SERVICE_DEVELOPMENT_FILES },
   { "Documents",         "x-office-document",        SERVICE_DOCUMENTS     },
   { "Emails",            "email",                    SERVICE_EMAILS        },
   { "Images",            "image-x-generic",          SERVICE_IMAGES        },
   { "Music",             "audio-x-generic",          SERVICE_MUSIC         },
   { "Text Files",        "text-x-generic",           SERVICE_TEXT_FILES    },
   { "Videos",            "video-x-generic",          SERVICE_VIDEOS        },
   { NULL,                NULL,                       -1                    }
};

int main (int argc, char *argv[])
{
   GladeXML         *glade;
   GtkWidget        *main_window;
   GtkWidget        *details_text_view;
  	GtkWidget        *search_entry;
  	GtkWidget        *search_button;
  	GtkWidget        *iview;
   GtkListStore     *results_store;
  	GtkTextBuffer    *details_text_buffer;
  	GtkComboBox      *service_combo;

	gtk_init (&argc, &argv);

#if 0
	if (!client) {
       		g_print ("Could not initialise Tracker - exiting...\n");
		return 1;
    	}
#endif
    
    	/*Glade stuff*/
 
	char *glade_file;

	glade_file = "tracker.glade";

	if (!g_file_test (glade_file, G_FILE_TEST_EXISTS)) {
		glade_file = DATADIR "/tracker/tracker.glade";
	}

    	glade = glade_xml_new (glade_file, NULL, NULL);
    	glade_xml_signal_autoconnect (glade);

    	main_window = glade_xml_get_widget (glade, "main_window");

    	search_entry = glade_xml_get_widget (glade, "search_entry");
    	search_button = glade_xml_get_widget (glade, "search_button");
  	iview = glade_xml_get_widget (glade, "iconview_results");
    
	details_text_view = glade_xml_get_widget (glade,"details_text_view");
	details_text_buffer = gtk_text_buffer_new (NULL);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW (details_text_view), details_text_buffer);
    
	sexy_icon_entry_add_clear_button (SEXY_ICON_ENTRY (search_entry));

    	service_combo = GTK_COMBO_BOX (glade_xml_get_widget (glade, "combobox_services"));
   fill_services_combo_box (service_combo);

   results_store = gtk_list_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
   gtk_icon_view_set_column_spacing (iview, 48);
   gtk_icon_view_set_model (iview, results_store);
   gtk_icon_view_set_pixbuf_column (iview, 0);
   gtk_icon_view_set_text_column (iview, 1);

   g_object_set_data (G_OBJECT (iview), "prev_signal", -1);
   g_object_set_data (G_OBJECT (iview), "next_signal", -1);
    	/*Signals*/
    	g_signal_connect (G_OBJECT (main_window), "destroy", G_CALLBACK (gtk_main_quit), NULL);
    	g_signal_connect (G_OBJECT (search_button), "clicked", G_CALLBACK (on_search), glade);
    	g_signal_connect (G_OBJECT (iview), "selection-changed", G_CALLBACK (on_search_results_changed), glade);

    	/*Test*/
    	gtk_text_buffer_set_text(details_text_buffer, "Hello world!", -1);
    
       	gtk_widget_show_all (main_window);

    	gtk_main();

    	return 0;
}

void fill_services_combo_box (GtkComboBox *combo)
{
	GtkListStore     *store;
  	GtkCellRenderer  *cell;
	GdkPixbuf        *pixbuf;
	GtkTreeIter       iter;
	GtkIconTheme     *theme;
	GError           *error = NULL;
   service_info_t   *service;

	theme = gtk_icon_theme_get_default ();

   store = gtk_list_store_new (2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
  	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));

  	cell = gtk_cell_renderer_pixbuf_new ();
  	gtk_cell_layout_pack_start ( GTK_CELL_LAYOUT (combo), cell, FALSE);
  	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), cell, "pixbuf", 0);

  	cell = gtk_cell_renderer_text_new ();
  	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
  	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), cell, "text", 1);
    	
   for (service = services; service->service; ++service) {
  	   pixbuf = gtk_icon_theme_load_icon (theme, service->icon_name,
                                         GTK_ICON_SIZE_MENU,
                                         GTK_ICON_LOOKUP_USE_BUILTIN,
                                         &error);
	   gtk_list_store_append (store, &iter);
	   gtk_list_store_set (store, &iter, 0, pixbuf, 1, service->service, -1);
   }
   gtk_combo_box_set_active (combo, 0);
}

void
start_search (GladeXML *glade, gint offset)
{
   GtkWidget *search_entry;
   GtkWidget *service_combo;
   GtkWidget *iview;
   const gchar *query;
   TrackerClient *client;
   gint service_idx;

	client = tracker_connect (FALSE);

	search_entry = glade_xml_get_widget (glade, "search_entry");
  	service_combo = glade_xml_get_widget (glade, "combobox_services");
  	iview = glade_xml_get_widget (glade, "iconview_results");
	assert (iview);

	/* gtk_icon_view_unselect_all (iview); */
   service_idx = gtk_combo_box_get_active (GTK_COMBO_BOX (service_combo));
   query = gtk_entry_get_text (GTK_ENTRY(search_entry));
   g_object_set_data (G_OBJECT (iview), "offset", offset);
   tracker_search_text_async (client, -1, services[service_idx].service_type,
                              query, offset, MAX_SEARCH_RESULTS + offset + 1, on_search_results,
                              glade);

   printf("%s\n", query);
}

void
on_search (GtkWidget *button, gpointer user_data)
{
   GladeXML *glade;
   glade = GLADE_XML (user_data);
   start_search (glade, 0);
}

void
on_search_previous (GtkWidget *button, gpointer user_data)
{
   GladeXML *glade;
   GtkWidget *iview;
   gint    offset;

   glade = GLADE_XML (user_data);
  	iview = glade_xml_get_widget (glade, "iconview_results");
   offset = (gint) g_object_get_data (G_OBJECT (iview), "offset");

   start_search (glade, offset - MAX_SEARCH_RESULTS);
}

void
on_search_next (GtkWidget *button, gpointer user_data)
{
   GladeXML *glade;
   GtkWidget *iview;
   gint    offset;

   glade = GLADE_XML (user_data);
  	iview = glade_xml_get_widget (glade, "iconview_results");
   offset = (gint) g_object_get_data (G_OBJECT (iview), "offset");

   start_search (glade, offset + MAX_SEARCH_RESULTS);
}

void
on_search_results (gchar **results, GError *error, gpointer user_data)
{
   GladeXML        *glade;
   GtkWidget       *iview;
   GtkWidget       *button;
   GdkPixbuf       *pixbuf;
	GtkIconTheme    *theme;
   GtkListStore    *store;
   GtkTreeIter      iter;

   gchar **p;
   gint    offset;
   gint    idx;
   gulong  signal_id;
   char   *icon_path;
	
   glade = GLADE_XML (user_data);
  	iview = glade_xml_get_widget (glade, "iconview_results");
   offset = (gint) g_object_get_data (G_OBJECT (iview), "offset");
	
   store = gtk_icon_view_get_model (iview);

	theme = gtk_icon_theme_get_default ();
   pixbuf = gtk_icon_theme_load_icon (theme, "image-loading",
                                      32 /*GTK_ICON_SIZE_MENU*/,
                                      GTK_ICON_LOOKUP_USE_BUILTIN,
                                      &error);
   idx = 0;
   gtk_list_store_clear (store);
	for (p = results; *p && idx < MAX_SEARCH_RESULTS; ++p,++idx) {
      pthread_t tid;
      thumbnail_thread_data_t *data;

      gchar *basename = g_path_get_basename (*p);
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, pixbuf, 1, basename, 2, *p, -1);
      g_free (basename);

      data = (thumbnail_thread_data_t *)malloc(sizeof(thumbnail_thread_data_t));
      data->store = store;
      data->iter = gtk_tree_iter_copy (&iter);
      pthread_create(&tid, NULL, load_thumbnail_thread, data);
	}

   button = glade_xml_get_widget (glade, "button_previous");
   signal_id = (gulong) g_object_get_data (G_OBJECT (iview), "prev_signal");
   if (signal_id != -1)
      g_signal_handler_disconnect (G_OBJECT (button), signal_id);
   if (offset > 0) {
      /* initialize the previous results button */
      gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
      signal_id = g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (on_search_previous), user_data);
      g_object_set_data (G_OBJECT (iview), "prev_signal", signal_id);
   }
   else {
      gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
      g_object_set_data (G_OBJECT (iview), "prev_signal", -1);
   }

   button = glade_xml_get_widget (glade, "button_next");
   signal_id = (gulong) g_object_get_data (G_OBJECT (iview), "next_signal");
   if (signal_id != -1)
      g_signal_handler_disconnect (G_OBJECT (button), signal_id);
   if (idx == MAX_SEARCH_RESULTS && *p) {
      /* initialize the next results button */
      gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
      signal_id = g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (on_search_next), user_data);
      g_object_set_data (G_OBJECT (iview), "next_signal", signal_id);
   }
   else {
      gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
      g_object_set_data (G_OBJECT (iview), "next_signal", -1);
   }

}

void
on_search_results_changed (GtkIconView *iview, gpointer user_data)
{
   GladeXML        *glade;
   GtkListStore    *store;
   gchar           *fullpath;
   GList           *selected;
   GtkTreeIter      iter;
   GValue           value = { 0, };
   GtkTreePath     *path;

   glade = GLADE_XML (user_data);

   store = gtk_icon_view_get_model (iview);
   selected = gtk_icon_view_get_selected_items (iview);
   if (selected == NULL)
      return;
   path = selected->data;
   gtk_tree_model_get_iter (store, &iter, path);
   gtk_tree_model_get_value (store, &iter, 2, &value);
   fullpath = g_value_get_string (&value);
   printf("\"%s\" selected\n",fullpath);
}

void
load_thumbnail_thread (gpointer user_data)
{
   thumbnail_thread_data_t  *data;
   GValue                    value = { 0, };
   gchar                    *path;
   gchar                    *uri;
   gchar                    *icon_path;
   GdkPixbuf                *pixbuf;
   GError                   *error;

   data = user_data;

   error = NULL;
   gtk_tree_model_get_value (data->store, data->iter, 2, &value);
   path = g_value_get_string (&value);
   uri = g_filename_to_uri (path, NULL, &error);
   icon_path = gnome_thumbnail_path_for_uri (uri, GNOME_THUMBNAIL_SIZE_NORMAL);
   if (!icon_path)
      return;
   printf ("Icon path for %s is %s\n", uri, icon_path);
   pixbuf = gdk_pixbuf_new_from_file (icon_path, &error);
   if (!pixbuf) printf("thumbnail not found\n");
   if (!pixbuf || error)
      return;
   gtk_list_store_set (data->store, data->iter, 0, pixbuf, -1);
 //  free (user_data);
}
