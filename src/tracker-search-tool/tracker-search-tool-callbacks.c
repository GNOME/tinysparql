/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * GNOME Search Tool
 *
 *  File:  tracker_search-callbacks.c
 *
 *  (C) 2002 the Free Software Foundation
 *
 *  Authors:	Dennis Cranston  <dennis_cranston@yahoo.com>
 *		George Lebl
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-find-directory.h>
#include <libgnome/gnome-desktop-item.h>

#include <gnome.h>

#include "tracker-search-tool.h"
#include "tracker-search-tool-callbacks.h"
#include "tracker-search-tool-support.h"
#include "../libtracker-gtk/tracker-metadata-tile.h"

#define SILENT_WINDOW_OPEN_LIMIT 5
#define METADATA_IMAGE_WIDTH	128
#define METADATA_IMAGE_HEIGHT	128

#ifdef HAVE_GETPGID
extern pid_t getpgid (pid_t);
#endif

gboolean row_selected_by_button_press_event;

static void
store_window_state_and_geometry (GSearchWindow *gsearch)
{
	tracker_search_gconf_set_int ("/apps/tracker-search-tool/default_window_width",
				      gsearch->window_width);
	tracker_search_gconf_set_int ("/apps/tracker-search-tool/default_window_height",
				      gsearch->window_height);
	tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/default_window_maximized",
					  gsearch->is_window_maximized);
	tracker_set_stored_separator_position (gtk_paned_get_position (GTK_PANED (gsearch->pane)));
}

static void
quit_application (GSearchWindow * gsearch)
{
	//GSearchCommandDetails * command_details = gsearch->command_details;
	store_window_state_and_geometry (gsearch);
	gtk_main_quit ();
}

void
die_cb (GnomeClient * client,
	gpointer data)
{
	quit_application ((GSearchWindow *) data);
}

void
quit_cb (GtkWidget * widget,
	 GdkEvent * event,
	 gpointer data)
{
	quit_application ((GSearchWindow *) data);
}

void
click_close_cb (GtkWidget * widget,
		gpointer data)
{
	quit_application ((GSearchWindow *) data);
}

void
click_stop_cb (GtkWidget * widget,
	       gpointer data)
{
	GSearchWindow * gsearch = data;

	if (gsearch->command_details->command_status == RUNNING) {
		gtk_widget_set_sensitive (gsearch->stop_button, FALSE);
	}
}

void
click_help_cb (GtkWidget * widget,
	       gpointer data)
{
	GtkWidget * window = data;
	GError * error = NULL;

	gnome_help_display_desktop_on_screen (NULL, "tracker-search-tool", "tracker-search-tool",
					      NULL, gtk_widget_get_screen (widget), &error);
	if (error) {
		GtkWidget * dialog;

		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Could not open help document."));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  error->message);

		gtk_window_set_title (GTK_WINDOW (dialog), "");
		gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
		gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

		g_signal_connect (G_OBJECT (dialog),
				  "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);

		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

void
click_expander_cb (GObject * object,
		   GParamSpec * param_spec,
		   gpointer data)
{
	GSearchWindow * gsearch = data;

	if (gtk_expander_get_expanded (GTK_EXPANDER (object)) == TRUE) {
		gtk_widget_show (gsearch->available_options_vbox);
		gtk_window_set_geometry_hints (GTK_WINDOW (gsearch->window),
					       GTK_WIDGET (gsearch->window),
					       &gsearch->window_geometry,
					       GDK_HINT_MIN_SIZE);
	}
	else {
		GdkGeometry default_geometry = {DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT};

		gtk_widget_hide (gsearch->available_options_vbox);
		gtk_window_set_geometry_hints (GTK_WINDOW (gsearch->window),
					       GTK_WIDGET (gsearch->window),
					       &default_geometry,
					       GDK_HINT_MIN_SIZE);
	}
}

void
size_allocate_cb (GtkWidget * widget,
		  GtkAllocation * allocation,
		  gpointer data)
{
	GtkWidget * button = data;

	gtk_widget_set_size_request (button, allocation->width, -1);
}

/*
void
add_constraint_cb (GtkWidget * widget,
		   gpointer data)
{
	GSearchWindow * gsearch = data;
	gint index;

	index = gtk_combo_box_get_active (GTK_COMBO_BOX (gsearch->available_options_combo_box));
	add_constraint (gsearch, index, NULL, FALSE);
}
*/

void
remove_constraint_cb (GtkWidget * widget,
		      gpointer data)
{
	GList * list = data;

	GSearchWindow * gsearch = g_list_first (list)->data;
	GSearchConstraint * constraint = g_list_last (list)->data;

	gsearch->window_geometry.min_height -= WINDOW_HEIGHT_STEP;

	gtk_window_set_geometry_hints (GTK_WINDOW (gsearch->window),
				       GTK_WIDGET (gsearch->window),
				       &gsearch->window_geometry,
				       GDK_HINT_MIN_SIZE);

	gtk_container_remove (GTK_CONTAINER (gsearch->available_options_vbox), widget->parent);

	gsearch->available_options_selected_list =
	    g_list_remove (gsearch->available_options_selected_list, constraint);

	//set_constraint_selected_state (gsearch, constraint->constraint_id, FALSE);
	set_constraint_gconf_boolean (constraint->constraint_id, FALSE);
	g_slice_free (GSearchConstraint, constraint);
	g_list_free (list);
}

void
constraint_activate_cb (GtkWidget * widget,
			gpointer data)
{
	GSearchWindow * gsearch = data;

	if ((GTK_WIDGET_VISIBLE (gsearch->find_button)) &&
	    (GTK_WIDGET_SENSITIVE (gsearch->find_button))) {
		click_find_cb (gsearch->find_button, data);
	}
}

void
constraint_update_info_cb (GtkWidget * widget,
			   gpointer data)
{
	static gchar * string;
	GSearchConstraint * opt = data;

	string = (gchar *) gtk_entry_get_text (GTK_ENTRY (widget));
	update_constraint_info (opt, string);
}

void
name_contains_activate_cb (GtkWidget * widget,
			   gpointer data)
{
	GSearchWindow * gsearch = data;

	if ((GTK_WIDGET_VISIBLE (gsearch->find_button)) &&
	    (GTK_WIDGET_SENSITIVE (gsearch->find_button))) {
		click_find_cb (gsearch->find_button, data);
	}
}

gboolean
text_changed_cb (GtkWidget * widget,
		 gpointer data)
{
	GSearchWindow * gsearch = data;
	const gchar * s = gtk_entry_get_text (GTK_ENTRY (gsearch->search_entry));

	if (!tracker_is_empty_string (s)) {
		gtk_widget_set_sensitive (gsearch->find_button, TRUE);
	} else {
		gtk_widget_set_sensitive (gsearch->find_button, FALSE);
	}

	return FALSE;
}

void
click_find_cb (GtkWidget * widget,
	       gpointer data)
{
	GSearchWindow * gsearch = data;
	gchar	      * command = build_search_command (gsearch, TRUE);

	if (command) {
		start_new_search (gsearch, command);
	}

	g_free (command);
}

void
next_results_cb (GtkWidget * widget,
		 gpointer data)
{
	GSearchWindow * gsearch = data;

	do_search (gsearch, gsearch->search_term, FALSE,
		   gsearch->current_service->offset + MAX_SEARCH_RESULTS);
}


void
prev_results_cb (GtkWidget * widget,
		 gpointer data)
{
	GSearchWindow * gsearch = data;

	do_search (gsearch, gsearch->search_term, FALSE,
		   gsearch->current_service->offset - MAX_SEARCH_RESULTS);
}

void
category_changed_cb (GtkTreeSelection * treeselection,
		     gpointer data)
{
	select_category (treeselection, data);
}

static gint
display_dialog_file_open_limit (GtkWidget * window,
				gint count)
{
	GtkWidget * dialog;
	GtkWidget * button;
	gchar * primary;
	gchar * secondary;
	gint response;

	primary = g_strdup_printf (ngettext ("Are you sure you want to open %d document?",
					     "Are you sure you want to open %d documents?",
					     count),
				   count);

	secondary = g_strdup_printf (ngettext ("This will open %d separate window.",
					       "This will open %d separate windows.",
					       count),
				     count);

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_CANCEL,
					 primary);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  secondary);

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	button = gtk_button_new_from_stock ("gtk-open");
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_show (button);

	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	g_free (primary);
	g_free (secondary);

	return response;
}

static void
display_dialog_could_not_open_file (GtkWidget * window,
				    const gchar * file,
				    const gchar * message)
{
	GtkWidget * dialog;
	gchar * primary;

	primary = g_strdup_printf (_("Could not open document \"%s\"."), file);

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 primary);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  message);

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (dialog);
	g_free (primary);
}

static void
display_dialog_could_not_open_folder (GtkWidget * window,
				      const gchar * folder)
{
	GtkWidget * dialog;
	gchar * primary;

	primary = g_strdup_printf (_("Could not open folder \"%s\"."), folder);

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 primary);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("The nautilus file manager is not running."));

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (dialog);
	g_free (primary);
}

void
select_changed_cb (GtkTreeSelection *treeselection,
		   gpointer user_data)
{

	GSearchWindow *gsearch = user_data;
	tracker_update_metadata_tile (gsearch);

#if 0

	GtkTreeModel * model;
	GtkTreeIter iter;
	char *name, *path;

	if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection)) == 0) {
		return;
	}

	gtk_tree_selection_get_selected (GTK_TREE_SELECTION (gsearch->search_results_selection),
					 &model,
					 &iter);

	gtk_tree_model_get (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
			    COLUMN_NAME, &name,
			    COLUMN_PATH, &path,
			    -1);

	//g_print ("selected uri is %s/%s\n", path, name);

	g_free (path);

	g_free (name);

#endif
}

static GdkPixbuf *
get_large_icon (const gchar * local_uri,
		gboolean is_local_file,
		GSearchWindow * gsearch)
{
	gchar	  * thumb_name = NULL;
	GdkPixbuf * temp = NULL;

	if (is_local_file) {
		gchar *uri = gnome_vfs_get_uri_from_local_path (local_uri);

		thumb_name = gnome_thumbnail_path_for_uri (uri, GNOME_THUMBNAIL_SIZE_NORMAL);

		g_free (uri);
	}

	if (!thumb_name) {

		gchar * icon_name = gnome_icon_lookup_sync  (gtk_icon_theme_get_default (),
							     gsearch->thumbnail_factory,
							     local_uri,
							     NULL,
							     GNOME_ICON_LOOKUP_FLAGS_SHOW_SMALL_IMAGES_AS_THEMSELVES | GNOME_ICON_LOOKUP_FLAGS_ALLOW_SVG_AS_THEMSELVES,
							     0);

		temp = gtk_icon_theme_load_icon (gtk_icon_theme_get_default(),
					 icon_name,
					 METADATA_IMAGE_HEIGHT,
					 GTK_ICON_LOOKUP_FORCE_SVG,
					 NULL);

		g_free (icon_name);

		return temp;
	}

	temp = gdk_pixbuf_new_from_file_at_scale (thumb_name, METADATA_IMAGE_WIDTH, METADATA_IMAGE_HEIGHT, TRUE, NULL);

	g_free (thumb_name);

	return temp;
}


void
tracker_update_metadata_tile (GSearchWindow *gsearch)
{
	GtkTreeModel * model;
	GList * list, * tmp;
	ServiceType type;

	if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection)) != 1) {
		tracker_metadata_tile_set_uri (TRACKER_METADATA_TILE (gsearch->metatile), NULL, 0, NULL, NULL);
		//gtk_widget_hide (gsearch->metatile);
		return;
	}

	list = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection),
						     &model);
	for (tmp = list; tmp; tmp = tmp->next) {

		gboolean no_files_found = FALSE;
		gchar *uri = NULL;
		gchar *mime = NULL;
		GdkPixbuf *pixbuf = NULL;
		GtkTreeIter iter;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter, tmp->data);

		gtk_tree_model_get (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
				    COLUMN_ICON, &pixbuf,
				    COLUMN_URI, &uri,
				    COLUMN_MIME, &mime,
				    COLUMN_TYPE, &type,
				    COLUMN_NO_FILES_FOUND, &no_files_found,
				    -1);

		/* get large icon for documents, images and videos only */
		GdkPixbuf *large_icon = NULL;
		if (type == SERVICE_DOCUMENTS || type == SERVICE_IMAGES || type == SERVICE_VIDEOS) {
			large_icon = get_large_icon (uri, (gsearch->type < 10), gsearch);
		}


		if (large_icon) {
			tracker_metadata_tile_set_uri (TRACKER_METADATA_TILE (gsearch->metatile), uri, type, mime, large_icon);
			g_object_unref (G_OBJECT (large_icon));
		} else {
			tracker_metadata_tile_set_uri (TRACKER_METADATA_TILE (gsearch->metatile), uri, type, mime, pixbuf);
		}


		g_free (uri);
		g_free (mime);
	}

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

void
open_file_cb (GtkAction * action,
	      gpointer data)
{
	GSearchWindow * gsearch = data;
	GtkTreeModel * model;
	GList * list, * tmp;
	char *exec = NULL;

	if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection)) == 0) {
		return;
	}

	list = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection),
						     &model);

	if (g_list_length (list) > SILENT_WINDOW_OPEN_LIMIT) {
		gint response;

		response = display_dialog_file_open_limit (gsearch->window, g_list_length (list));

		if (response == GTK_RESPONSE_CANCEL) {
			g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
			g_list_free (list);
			return;
		}
	}

	for (tmp = list; tmp; tmp = tmp->next) {

		gboolean no_files_found = FALSE;
		gchar * uri;
		gchar * mime;
		GtkTreeIter iter;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter, tmp->data);

		gtk_tree_model_get (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
				    COLUMN_URI, &uri,
				    COLUMN_MIME, &mime,
				    COLUMN_EXEC, &exec,
				    COLUMN_NO_FILES_FOUND, &no_files_found,
				    -1);


		if (gsearch->type == SERVICE_EMAILS) {
			if (strstr (mime, "Evolution")) {
				exec = g_strdup_printf ("evolution \"%s\"", uri);
			} else if (strstr (mime, "Modest")) {
				exec = g_strdup_printf ("modest-open \"%s\"", uri);
			} else if (strstr (mime, "KMail")) {
				exec = g_strdup_printf ("kmail --view \"%s\"", uri);
			} else if (strstr (mime, "Thunderbird")) {
				exec = g_strdup_printf ("thunderbird -viewtracker \"%s\"", uri);
			} else {
				exec = NULL;
			}
			if (exec) {
				g_spawn_command_line_async (exec, NULL);
				g_free (exec);
			}

		} else if (gsearch->type == SERVICE_APPLICATIONS) {
			if (exec) {
				gchar *my_exec = tracker_string_replace (exec, "%U", NULL);
				g_spawn_command_line_async (my_exec, NULL);
				g_free (my_exec);
				g_free (exec);

			} else {
				display_dialog_could_not_open_file (gsearch->window, uri, _("Application could not be opened"));
			}

		} else {

			gchar * file;
			gchar * locale_file;

			file = uri;
			locale_file = g_locale_from_utf8 (file, -1, NULL, NULL, NULL);

			if (!g_file_test (locale_file, G_FILE_TEST_EXISTS)) {
				gtk_tree_selection_unselect_iter (GTK_TREE_SELECTION (gsearch->search_results_selection),
								  &iter);
				display_dialog_could_not_open_file (gsearch->window, uri,
								    _("The document does not exist."));

			} else if (open_file_with_xdg_open (gsearch->window, locale_file) == FALSE) {

				if (open_file_with_application (gsearch->window, locale_file) == FALSE) {

					if (launch_file (locale_file) == FALSE) {

						if (g_file_test (locale_file, G_FILE_TEST_IS_DIR)) {

							if (open_file_with_nautilus (gsearch->window, locale_file) == FALSE) {
								display_dialog_could_not_open_folder (gsearch->window, uri);
							}

						} else {
							display_dialog_could_not_open_file (gsearch->window, uri,
										    _("There is no installed viewer capable "
										      "of displaying the document."));
						}
					}
				}
			}
			g_free (locale_file);
		}
		g_free (uri);

	}
	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

static gint
display_dialog_folder_open_limit (GtkWidget * window,
				  gint count)
{
	GtkWidget * dialog;
	GtkWidget * button;
	gchar * primary;
	gchar * secondary;
	gint response;

	primary = g_strdup_printf (ngettext ("Are you sure you want to open %d folder?",
					     "Are you sure you want to open %d folders?",
					     count),
				   count);

	secondary = g_strdup_printf (ngettext ("This will open %d separate window.",
					       "This will open %d separate windows.",
					       count),
				     count);

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_CANCEL,
					 primary);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  secondary);

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	button = gtk_button_new_from_stock ("gtk-open");
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_show (button);

	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	g_free (primary);
	g_free (secondary);

	return response;
}

void
open_folder_cb (GtkAction * action,
		gpointer data)
{
	GSearchWindow * gsearch = data;
	GtkTreeModel * model;
	GList * list, * tmp;

	if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection)) == 0) {
		return;
	}

	list = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection),
						     &model);

	if (g_list_length (list) > SILENT_WINDOW_OPEN_LIMIT) {
		gint response;

		response = display_dialog_folder_open_limit (gsearch->window, g_list_length (list));

		if (response == GTK_RESPONSE_CANCEL) {
			g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
			g_list_free (list);
			return;
		}
	}

	for (tmp = list; tmp; tmp = tmp->next) {

		gchar * folder_locale;
		gchar * folder_utf8;
		GtkTreeIter iter;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter, tmp->data);

		gtk_tree_model_get (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
				    COLUMN_PATH, &folder_utf8,
				    -1);

		folder_locale = g_filename_from_utf8 (folder_utf8, -1, NULL, NULL, NULL);

		if (open_file_with_xdg_open (gsearch->window, folder_locale) == FALSE) {
			if (open_file_with_nautilus (gsearch->window, folder_locale) == FALSE) {

				display_dialog_could_not_open_folder (gsearch->window, folder_utf8);
			}
			g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
			g_list_free (list);
			g_free (folder_locale);
			g_free (folder_utf8);
			return;
		}
		g_free (folder_locale);
		g_free (folder_utf8);
	}
	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

static void
display_dialog_could_not_move_to_trash (GtkWidget * window,
					const gchar * file,
					const gchar * message)
{
	GtkWidget * dialog;
	gchar * primary;

	primary = g_strdup_printf (_("Could not move \"%s\" to trash."), file);

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 primary);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  message);

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	gtk_widget_show (dialog);
	g_free (primary);
}

static gint
display_dialog_delete_permanently (GtkWidget * window,
				   const gchar * file)
{
	GtkWidget * dialog;
	GtkWidget * button;
	gchar * primary;
	gchar * secondary;
	gint response;

	primary = g_strdup_printf (_("Do you want to delete \"%s\" permanently?"),
				   g_path_get_basename (file));

	secondary = g_strdup_printf (_("Trash is unavailable.  Could not move \"%s\" to the trash."),
				     file);

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_CANCEL,
					 primary);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  secondary);

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	button = gtk_button_new_from_stock ("gtk-delete");
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_show (button);

	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (GTK_WIDGET(dialog));
	g_free (primary);
	g_free (secondary);

	return response;
}

static void
display_dialog_could_not_delete (GtkWidget * window,
				 const gchar * file,
				 const gchar * message)
{
	GtkWidget * dialog;
	gchar	  * primary;

	primary = g_strdup_printf (_("Could not delete \"%s\"."), file);

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 primary);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  message);

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	gtk_widget_show (dialog);
	g_free (primary);
}

static char *
get_trash_path (const gchar * file)
{
	GnomeVFSURI * trash_uri;
	GnomeVFSURI * uri;
	gchar	    * filename;

	filename = gnome_vfs_escape_path_string (file);
	uri = gnome_vfs_uri_new (filename);
	g_free (filename);

	gnome_vfs_find_directory (uri,
				  GNOME_VFS_DIRECTORY_KIND_TRASH,
				  &trash_uri,
				  TRUE,
				  TRUE,
				  0777);
	gnome_vfs_uri_unref (uri);

	if (trash_uri == NULL) {
		return NULL;
	}
	else {
		gchar * trash_path;
		trash_path = gnome_vfs_uri_to_string (trash_uri, GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD);
		gnome_vfs_uri_unref (trash_uri);
		return trash_path;
	}
}

void
move_to_trash_cb (GtkAction * action,
		  gpointer data)
{
	GSearchWindow * gsearch = data;
	gint total;
	gint index;

	if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection)) == 0) {
		return;
	}

	total = gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection));

	for (index = 0; index < total; index++) {
		gboolean no_files_found = FALSE;
		GtkTreeModel * model;
		GtkTreeIter iter;
		GList * list;
		gchar * utf8_basename;
		gchar * utf8_basepath;
		gchar * utf8_filename;
		gchar * locale_filename;
		gchar * trash_path;

		list = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection),
							     &model);

		gtk_tree_model_get_iter (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
					 g_list_nth (list, 0)->data);

		gtk_tree_model_get (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
				    COLUMN_NAME, &utf8_basename,
				    COLUMN_PATH, &utf8_basepath,
				    COLUMN_NO_FILES_FOUND, &no_files_found,
				    -1);

		if (no_files_found) {
			g_free (utf8_basename);
			g_free (utf8_basepath);
			return;
		}

		utf8_filename = g_build_filename (utf8_basepath, utf8_basename, NULL);
		locale_filename = g_locale_from_utf8 (utf8_filename, -1, NULL, NULL, NULL);
		trash_path = get_trash_path (locale_filename);

		if ((!g_file_test (locale_filename, G_FILE_TEST_EXISTS)) &&
		    (!g_file_test (locale_filename, G_FILE_TEST_IS_SYMLINK))) {
			gtk_tree_selection_unselect_iter (GTK_TREE_SELECTION (gsearch->search_results_selection), &iter);
			display_dialog_could_not_move_to_trash (gsearch->window, utf8_basename,
								_("The document does not exist."));
		}
		else if (trash_path != NULL) {
			GnomeVFSResult result;
			gchar * destination;
			gchar * basename;
			gchar * source_uri;;

			source_uri = g_filename_to_uri (locale_filename, NULL, NULL);
			basename = g_locale_from_utf8 (utf8_basename, -1, NULL, NULL, NULL);
			destination = g_build_filename (trash_path, basename, NULL);

			result = gnome_vfs_move (source_uri, destination, TRUE);
			gtk_tree_selection_unselect_iter (GTK_TREE_SELECTION (gsearch->search_results_selection), &iter);

			if (result == GNOME_VFS_OK) {

				gtk_list_store_remove (GTK_LIST_STORE (gsearch->search_results_list_store), &iter);
			}
			else {
				gchar * message;

				message = g_strdup_printf (_("Moving \"%s\" failed: %s."),
							   utf8_filename,
							   gnome_vfs_result_to_string (result));
				display_dialog_could_not_move_to_trash (gsearch->window, utf8_basename,
									message);
				g_free (message);
			}
			g_free (source_uri);
			g_free (basename);
			g_free (destination);
		}
		else {
			gint response;

			gtk_tree_selection_unselect_iter (GTK_TREE_SELECTION (gsearch->search_results_selection), &iter);
			response = display_dialog_delete_permanently (gsearch->window, utf8_filename);

			if (response == GTK_RESPONSE_OK) {
				GnomeVFSResult result;

				if (!g_file_test (locale_filename, G_FILE_TEST_IS_DIR)) {
					result = gnome_vfs_unlink (locale_filename);
				}
				else {
					result = gnome_vfs_remove_directory (locale_filename);
				}

				if (result == GNOME_VFS_OK) {

					gtk_list_store_remove (GTK_LIST_STORE (gsearch->search_results_list_store), &iter);
				}
				else {
					gchar * message;

					message = g_strdup_printf (_("Deleting \"%s\" failed: %s."),
								     utf8_filename, gnome_vfs_result_to_string (result));

					display_dialog_could_not_delete (gsearch->window, utf8_basename, message);

					g_free (message);
				}
			}
		}
		g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (list);
		g_free (locale_filename);
		g_free (utf8_filename);
		g_free (utf8_basename);
		g_free (utf8_basepath);
		g_free (trash_path);
	}

	if (gsearch->command_details->command_status != RUNNING) {
		update_search_counts (gsearch);
	}
}

gboolean
file_button_press_event_cb (GtkWidget * widget,
			    GdkEventButton * event,
			    gpointer data)
{
	GtkTreeView * tree = data;
	GtkTreePath * path;

	row_selected_by_button_press_event = TRUE;

	if (event->window != gtk_tree_view_get_bin_window (tree)) {
		return FALSE;
	}

	if (gtk_tree_view_get_path_at_pos (tree, event->x, event->y,
		&path, NULL, NULL, NULL)) {

		if ((event->button == 1 || event->button == 2 || event->button == 3)
			&& gtk_tree_selection_path_is_selected (gtk_tree_view_get_selection (tree), path)) {
			row_selected_by_button_press_event = FALSE;
		}
		gtk_tree_path_free (path);
	}
	else {
		gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (tree));
	}

	return !(row_selected_by_button_press_event);
}

gboolean
file_key_press_event_cb (GtkWidget * widget,
			 GdkEventKey * event,
			 gpointer data)
{
	if (event->keyval == GDK_space || event->keyval == GDK_Return) {
		if (event->state != GDK_CONTROL_MASK) {
			open_file_cb ((GtkAction *) NULL, data);
			return TRUE;
		}
	}
	else if (event->keyval == GDK_Delete) {
		move_to_trash_cb ((GtkAction *) NULL, data);
		return TRUE;
	}
	return FALSE;
}

gboolean
file_button_release_event_cb (GtkWidget * widget,
			      GdkEventButton * event,
			      gpointer data)
{
	GSearchWindow * gsearch = data;
	gboolean no_files_found = FALSE;
	GtkTreeIter iter;

	if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (gsearch->search_results_tree_view))) {
		return FALSE;
	}

	if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection)) == 0) {
		return FALSE;
	}

	if (event->button == 3) {
		GtkTreeModel * model;
		GList * list;

		list = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection),
							     &model);

		gtk_tree_model_get_iter (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
					 g_list_first (list)->data);

		gtk_tree_model_get (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
				    COLUMN_NO_FILES_FOUND, &no_files_found,
				    -1);

		if (!no_files_found) {
			gtk_menu_popup (GTK_MENU (gsearch->search_results_popup_menu), NULL, NULL, NULL, NULL,
					event->button, event->time);
		}
		g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (list);
	}
	else if (event->button == 1 || event->button == 2) {
		if (gsearch->is_search_results_single_click_to_activate == TRUE) {
			if (!(event->state & GDK_CONTROL_MASK) && !(event->state & GDK_SHIFT_MASK)) {
				open_file_cb ((GtkAction *) NULL, data);
			}
		}
	}

	return FALSE;
}

gboolean
file_event_after_cb (GtkWidget * widget,
		     GdkEventButton * event,
		     gpointer data)
{
	GSearchWindow * gsearch = data;

	if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (gsearch->search_results_tree_view))) {
		return FALSE;
	}

	if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection)) == 0) {
		return FALSE;
	}

	if (!(event->state & GDK_CONTROL_MASK) && !(event->state & GDK_SHIFT_MASK)) {
		if (gsearch->is_search_results_single_click_to_activate == FALSE) {
			if (event->type == GDK_2BUTTON_PRESS) {
				open_file_cb ((GtkAction *) NULL, data);
				return TRUE;
			}
		}
	}
	return FALSE;
}

gboolean
file_motion_notify_cb (GtkWidget *widget,
		       GdkEventMotion *event,
		       gpointer user_data)
{
	GSearchWindow * gsearch = user_data;
	GdkCursor * cursor;
	GtkTreePath * last_hover_path;
	GtkTreeIter iter;

	if (gsearch->is_search_results_single_click_to_activate == FALSE) {
		gdk_window_set_cursor (event->window, NULL);
		return FALSE;
	}

	if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (gsearch->search_results_tree_view))) {
		gdk_window_set_cursor (event->window, NULL);
		return FALSE;
	}

	last_hover_path = gsearch->search_results_hover_path;

	gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
				       event->x, event->y,
				       &gsearch->search_results_hover_path,
				       NULL, NULL, NULL);

	if (gsearch->search_results_hover_path != NULL) {
		cursor = gdk_cursor_new (GDK_HAND2);
	}
	else {
		cursor = NULL;
	}

	gdk_window_set_cursor (event->window, cursor);

	/* Redraw if the hover row has changed */
	if (!(last_hover_path == NULL && gsearch->search_results_hover_path == NULL) &&
	    (!(last_hover_path != NULL && gsearch->search_results_hover_path != NULL) ||
	     gtk_tree_path_compare (last_hover_path, gsearch->search_results_hover_path))) {
		if (last_hover_path) {
			gtk_tree_model_get_iter (GTK_TREE_MODEL (gsearch->search_results_list_store),
						 &iter, last_hover_path);
			gtk_tree_model_row_changed (GTK_TREE_MODEL (gsearch->search_results_list_store),
						    last_hover_path, &iter);
		}

		if (gsearch->search_results_hover_path) {
			gtk_tree_model_get_iter (GTK_TREE_MODEL (gsearch->search_results_list_store),
						 &iter, gsearch->search_results_hover_path);
			gtk_tree_model_row_changed (GTK_TREE_MODEL (gsearch->search_results_list_store),
						    gsearch->search_results_hover_path, &iter);
		}
	}

	gtk_tree_path_free (last_hover_path);

	return FALSE;
}

gboolean
file_leave_notify_cb (GtkWidget *widget,
		      GdkEventCrossing *event,
		      gpointer user_data)
{
	GSearchWindow * gsearch = user_data;
	GtkTreeIter iter;

	if (gsearch->is_search_results_single_click_to_activate && (gsearch->search_results_hover_path != NULL)) {
		gtk_tree_model_get_iter (GTK_TREE_MODEL (gsearch->search_results_list_store),
					 &iter,
					 gsearch->search_results_hover_path);
		gtk_tree_model_row_changed (GTK_TREE_MODEL (gsearch->search_results_list_store),
					    gsearch->search_results_hover_path,
					    &iter);

		gtk_tree_path_free (gsearch->search_results_hover_path);
		gsearch->search_results_hover_path = NULL;

		return TRUE;
	}

	return FALSE;
}

void
drag_begin_file_cb (GtkWidget * widget,
		    GdkDragContext * context,
		    gpointer data)
{
	GSearchWindow * gsearch = data;
	gint number_of_selected_rows;

	number_of_selected_rows = gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection));

	if (number_of_selected_rows > 1) {
		gtk_drag_set_icon_stock (context, GTK_STOCK_DND_MULTIPLE, 0, 0);
	}
	else if (number_of_selected_rows == 1) {
		GdkPixbuf * pixbuf;
		GtkTreeModel * model;
		GtkTreeIter iter;
		GList * list;

		list = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection),
							     &model);

		gtk_tree_model_get_iter (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
					 g_list_first (list)->data);

		gtk_tree_model_get (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
				    COLUMN_ICON, &pixbuf,
				    -1);
		g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (list);

		if (pixbuf) {
			gtk_drag_set_icon_pixbuf (context, pixbuf, 0, 0);
		}
		else {
			gtk_drag_set_icon_stock (context, GTK_STOCK_DND, 0, 0);
		}
	}
}

/* Make a desktop file in /tmp witch points to this email */
static gchar*
make_email_desktop_file (const gchar *utf8_uri, const gchar *utf8_name)
{
	GnomeDesktopItem *item = NULL;
	gchar *exec_string = NULL;
	gchar *save_uri = NULL;
	time_t seconds;

	item = gnome_desktop_item_new ();

	exec_string = g_strdup_printf ("evolution \"%s\"",utf8_uri);

	gnome_desktop_item_set_string (item, GNOME_DESKTOP_ITEM_ENCODING, "UTF-8");
	gnome_desktop_item_set_string (item, GNOME_DESKTOP_ITEM_NAME, utf8_name);
	gnome_desktop_item_set_string (item, GNOME_DESKTOP_ITEM_COMMENT, _("Activate to view this email"));
	gnome_desktop_item_set_string (item, GNOME_DESKTOP_ITEM_EXEC, exec_string);
	gnome_desktop_item_set_string (item, GNOME_DESKTOP_ITEM_ICON, "email");
	gnome_desktop_item_set_string (item, GNOME_DESKTOP_ITEM_TERMINAL, "false");
	gnome_desktop_item_set_string (item, GNOME_DESKTOP_ITEM_TYPE, "Application");

	seconds = time (NULL);
	save_uri = g_strdup_printf ("/tmp/tracker-email-shortcut-file%d.desktop", (int)seconds);
	gnome_desktop_item_save (item,
				 save_uri,
				 TRUE,
				 NULL);

	g_free (exec_string);

	return save_uri;
}

void
drag_file_cb (GtkWidget * widget,
	      GdkDragContext * context,
	      GtkSelectionData * selection_data,
	      guint info,
	      guint time,
	      gpointer data)
{
	GSearchWindow * gsearch = data;
	gchar * uri_list = NULL;
	GList * list, * tmp;
	GtkTreeModel * model;
	GtkTreeIter iter;

	if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection)) == 0) {
		return;
	}

	list = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection),
						     &model);

	for (tmp = list; tmp; tmp = tmp->next) {

		gboolean no_files_found = FALSE;
		gchar * utf8_name;
		gchar * utf8_path;
		gchar * utf8_uri;
		gchar * file;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter, tmp->data);

		gtk_tree_model_get (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
				    COLUMN_NAME, &utf8_name,
				    COLUMN_PATH, &utf8_path,
				    COLUMN_URI, &utf8_uri,
				    COLUMN_NO_FILES_FOUND, &no_files_found,
				    -1);

		file = g_build_filename (utf8_path, utf8_name, NULL);

		if (!no_files_found) {
			gchar * tmp_uri = NULL;
			tmp_uri = g_filename_to_uri (file, NULL, NULL);

			if (uri_list == NULL) {
				uri_list = g_strdup (tmp_uri);
			}
			else {
				uri_list = g_strconcat (uri_list, "\n", tmp_uri, NULL);
			}
			if (gsearch->type < 10) {
				gtk_selection_data_set (selection_data,
							selection_data->target,
							8,
							(guchar *) uri_list,
							strlen (uri_list));
			} else {
				gchar *desktop_uri;
				desktop_uri = make_email_desktop_file (utf8_uri, utf8_name);
				gtk_selection_data_set (selection_data,
						selection_data->target,
						8,
						(guchar *) desktop_uri,
						strlen (desktop_uri));
					g_free (desktop_uri);
			}
			g_free (tmp_uri);
		}
		else {
			gtk_selection_data_set_text (selection_data, utf8_name, -1);
		}
		g_free (utf8_name);
		g_free (utf8_path);
		g_free (utf8_uri);
		g_free (file);
	}
	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
	g_free (uri_list);
}

void
drag_data_animation_cb (GtkWidget * widget,
			GdkDragContext * context,
			GtkSelectionData * selection_data,
			guint info,
			guint time,
			gpointer data)
{
	GSearchWindow * gsearch = data;
	GnomeDesktopItem * ditem;
	GString * command = g_string_new ("");
	gchar ** argv;
	gchar * desktop_item_name = NULL;
	gchar * uri;
	gchar * path;
	gchar * disk;
	gchar * scheme;
	gint argc;
	gint index;

	set_clone_command (gsearch, &argc, &argv, "tracker-search-tool", TRUE);

	if (argc == 0) {
		return;
	}

	for (index = 0; index < argc; index++) {
		command = g_string_append (command, argv[index]);
		command = g_string_append_c (command, ' ');
	}
	command = g_string_append (command, "--start");

	disk = g_locale_from_utf8 (command->str, -1, NULL, NULL, NULL);
	uri = gnome_vfs_make_uri_from_input_with_dirs (disk, GNOME_VFS_MAKE_URI_DIR_HOMEDIR);
	scheme = gnome_vfs_get_uri_scheme (uri);

	ditem = gnome_desktop_item_new ();

	gnome_desktop_item_set_entry_type (ditem, GNOME_DESKTOP_ITEM_TYPE_APPLICATION);
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_EXEC, command->str);

	desktop_item_name = get_desktop_item_name (gsearch);

	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_NAME, desktop_item_name);
	gnome_desktop_item_set_boolean (ditem, GNOME_DESKTOP_ITEM_TERMINAL, FALSE);
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_ICON, TRACKER_SEARCH_TOOL_ICON);
	gnome_desktop_item_set_boolean (ditem, "StartupNotify", TRUE);

	g_string_free (command, TRUE);
	g_free (desktop_item_name);
	g_free (uri);

	path = tracker_search_get_unique_filename (g_get_tmp_dir (), ".desktop");
	gnome_desktop_item_set_location (ditem, path);

	uri = gnome_vfs_get_uri_from_local_path (path);

	if (gnome_desktop_item_save (ditem, NULL, FALSE, NULL)) {
		gtk_selection_data_set (selection_data,
					selection_data->target, 8,
					(guchar *) uri, strlen (uri));
	}
	gnome_desktop_item_unref (ditem);

	g_free (uri);
	g_free (path);
	g_free (disk);
	g_free (scheme);
}

void
show_file_selector_cb (GtkAction * action,
		       gpointer data)
{
	GSearchWindow * gsearch = data;
	GtkWidget * file_chooser;

	file_chooser = gtk_file_chooser_dialog_new (_("Save Search Results As..."),
						    GTK_WINDOW (gsearch->window),
						    GTK_FILE_CHOOSER_ACTION_SAVE,
						    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						    GTK_STOCK_SAVE, GTK_RESPONSE_OK,
						    NULL);

#if GTK_CHECK_VERSION(2,7,3)
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (file_chooser), TRUE);
#endif

	if (gsearch->save_results_as_default_filename != NULL) {
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (file_chooser),
					       gsearch->save_results_as_default_filename);
	} else {
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (file_chooser),
					       g_get_home_dir());
	}

	g_signal_connect (G_OBJECT (file_chooser), "response",
			  G_CALLBACK (save_results_cb), gsearch);

	gtk_window_set_modal (GTK_WINDOW (file_chooser), TRUE);
	gtk_window_set_position (GTK_WINDOW (file_chooser), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show (GTK_WIDGET (file_chooser));
}

static void
display_dialog_could_not_save_no_name (GtkWidget * window)
{
	GtkWidget * dialog;
	gchar * primary;
	gchar * secondary;

	primary = g_strdup (_("Could not save document."));
	secondary = g_strdup (_("You did not select a document name."));

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 primary);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  secondary);

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	gtk_widget_show (dialog);
	g_free (primary);
	g_free (secondary);
}

static void
display_dialog_could_not_save_to (GtkWidget * window,
				  const gchar * file,
				  const gchar * message)
{
	GtkWidget * dialog;
	gchar * primary;

	primary = g_strdup_printf (_("Could not save \"%s\" document to \"%s\"."),
				   g_path_get_basename (file),
				   g_path_get_dirname (file));

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 primary);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  message);

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	gtk_widget_show (dialog);
	g_free (primary);
}

#if !GTK_CHECK_VERSION(2,7,3)
static gint
display_dialog_could_not_save_exists (GtkWidget * window,
				      const gchar * file)
{
	GtkWidget * dialog;
	GtkWidget * button;
	gchar * primary;
	gchar * secondary;
	gint response;

	primary = g_strdup_printf (_("The document \"%s\" already exists.  "
				     "Would you like to replace it?"),
				   g_path_get_basename (file));

	secondary = g_strdup (_("If you replace an existing file, "
				"its contents will be overwritten."));

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_CANCEL,
					 primary);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  secondary);

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	button = tracker_search_button_new_with_stock_icon (_("_Replace"), GTK_STOCK_OK);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_show (button);

	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (GTK_WIDGET(dialog));
	g_free (primary);
	g_free (secondary);

	return response;
}
#endif

void
save_results_cb (GtkWidget * chooser,
		 gint response,
		 gpointer data)
{
	GSearchWindow * gsearch = data;
	GtkListStore * store;
	GtkTreeIter iter;
	FILE * fp;
	gchar * utf8 = NULL;

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (GTK_WIDGET (chooser));
		return;
	}

	store = gsearch->search_results_list_store;
	g_free (gsearch->save_results_as_default_filename);

	gsearch->save_results_as_default_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
	gtk_widget_destroy (chooser);

	if (gsearch->save_results_as_default_filename != NULL) {
		utf8 = g_filename_to_utf8 (gsearch->save_results_as_default_filename, -1, NULL, NULL, NULL);
	}

	if (utf8 == NULL) {
		display_dialog_could_not_save_no_name (gsearch->window);
		return;
	}

	if (g_file_test (gsearch->save_results_as_default_filename, G_FILE_TEST_IS_DIR)) {
		display_dialog_could_not_save_to (gsearch->window, utf8,
						  _("The document name you selected is a folder."));
		g_free (utf8);
		return;
	}

#if !GTK_CHECK_VERSION(2,7,3)
	if (g_file_test (gsearch->save_results_as_default_filename, G_FILE_TEST_EXISTS)) {

		gint response;

		response = display_dialog_could_not_save_exists (gsearch->window, utf8);

		if (response != GTK_RESPONSE_OK) {
			g_free (utf8);
			return;
		}
	}
#endif

	if ((fp = g_fopen (gsearch->save_results_as_default_filename, "w")) != NULL) {

		gint index;

		for (index = 0; index < gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL); index++)
		{
			if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter, NULL, index) == TRUE) {

				gchar * utf8_path;
				gchar * utf8_name;
				gchar * utf8_file;
				gchar * locale_file;

				gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COLUMN_PATH, &utf8_path, -1);
				gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COLUMN_NAME, &utf8_name, -1);

				utf8_file = g_build_filename (utf8_path, utf8_name, NULL);
				locale_file = g_filename_from_utf8 (utf8_file, -1, NULL, NULL, NULL);
				fprintf (fp, "%s\n", locale_file);

				g_free (utf8_path);
				g_free (utf8_name);
				g_free (utf8_file);
				g_free (locale_file);
			}
		}
		fclose (fp);
	}
	else {
		display_dialog_could_not_save_to (gsearch->window, utf8,
						  _("You may not have write permissions to the document."));
	}
	g_free (utf8);
}

void
save_session_cb (GnomeClient * client,
		 gint phase,
		 GnomeRestartStyle save_style,
		 gint shutdown,
		 GnomeInteractStyle interact_style,
		 gint fast,
		 gpointer client_data)
{
	GSearchWindow * gsearch = client_data;
	char ** argv;
	gint argc;

	set_clone_command (gsearch, &argc, &argv, "tracker-search-tool", FALSE);
	gnome_client_set_clone_command (client, argc, argv);
	gnome_client_set_restart_command (client, argc, argv);
}

gboolean
key_press_cb (GtkWidget * widget,
	      GdkEventKey * event,
	      gpointer data)
{
	GSearchWindow * gsearch = data;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	if (event->keyval == GDK_Escape) {
		if (gsearch->command_details->command_status == RUNNING) {
			click_stop_cb (widget, data);
		}
		else if (gsearch->command_details->is_command_timeout_enabled == FALSE) {
			quit_cb (widget, (GdkEvent *) NULL, data);
		}
	}
	else if (event->keyval == GDK_F10) {
		if (event->state & GDK_SHIFT_MASK) {
			gboolean no_files_found = FALSE;
			GtkTreeModel * model;
			GtkTreeIter iter;
			GList * list;

			if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection)) == 0) {
				return FALSE;
			}

			list = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION (gsearch->search_results_selection),
								     &model);

			gtk_tree_model_get_iter (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
						 g_list_first (list)->data);

			gtk_tree_model_get (GTK_TREE_MODEL (gsearch->search_results_list_store), &iter,
					    COLUMN_NO_FILES_FOUND, &no_files_found, -1);

			g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
			g_list_free (list);

			if (!no_files_found) {
				gtk_menu_popup (GTK_MENU (gsearch->search_results_popup_menu), NULL, NULL, NULL, NULL,
						event->keyval, event->time);
				return TRUE;
			}
		}
	}
	return FALSE;
}

gboolean
not_running_timeout_cb (gpointer data)
{
	GSearchWindow * gsearch = data;

	gsearch->command_details->is_command_timeout_enabled = FALSE;
	return FALSE;
}

void
disable_quick_search_cb (GtkWidget * dialog,
			 gint response,
			 gpointer data)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));

	if (response == GTK_RESPONSE_OK) {
		tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/disable_quick_search", TRUE);
	}
}

void
single_click_to_activate_key_changed_cb (GConfClient * client,
					 guint cnxn_id,
					 GConfEntry * entry,
					 gpointer user_data)
{
	GSearchWindow * gsearch = user_data;
	GConfValue * value = gconf_entry_get_value (entry);

	g_return_if_fail (value->type == GCONF_VALUE_STRING);

	gsearch->is_search_results_single_click_to_activate =
		(strncmp (gconf_value_get_string (value), "single", 6) == 0);
}

void
columns_changed_cb (GtkTreeView * treeview,
		    gpointer user_data)
{
	GSList * order = tracker_search_get_columns_order (treeview);

	if (g_slist_length (order) == NUM_VISIBLE_COLUMNS) {
		tracker_search_gconf_set_list ("/apps/tracker-search-tool/columns_order", order, GCONF_VALUE_INT);
	}
	g_slist_free (order);
}

gboolean
window_state_event_cb (GtkWidget * widget,
		       GdkEventWindowState * event,
		       gpointer data)
{
	GSearchWindow * gsearch = data;

	if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) {
		gsearch->is_window_maximized = TRUE;
	} else {
		gsearch->is_window_maximized = FALSE;
	}
	return FALSE;
}

void
suggest_search_cb (GtkWidget * widget,
		   gpointer data)
{
	GSearchWindow * gsearch = data;
	gchar	      * suggest = g_object_get_data (G_OBJECT (widget), "suggestion");

	gtk_entry_set_text (GTK_ENTRY (gsearch->search_entry), suggest);
	gtk_button_clicked (GTK_BUTTON (gsearch->find_button));
}
