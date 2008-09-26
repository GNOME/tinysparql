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

#include <gtk/gtk.h>
#include "tracker-preferences-dialogs.h"

static void
activate_dialog_entry (GtkEntry * entry, gpointer user_data)
{
	gtk_dialog_response (GTK_DIALOG (user_data), GTK_RESPONSE_ACCEPT);
}

gchar *
tracker_preferences_select_folder (void)
{
	gchar *folder = NULL;
	GtkWidget *dialog =
		gtk_file_chooser_dialog_new
		("Tracker Preferences - Choose a folder",
		 NULL,
		 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		 GTK_STOCK_CANCEL,
		 GTK_RESPONSE_CANCEL,
		 GTK_STOCK_OPEN,
		 GTK_RESPONSE_ACCEPT,
		 NULL);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		folder = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER
							(dialog));

	gtk_widget_destroy (dialog);
	return folder;
}

gchar *
tracker_preferences_select_pattern (void)
{
	gchar *pattern = NULL;
	GtkWidget *dialog =
		gtk_dialog_new_with_buttons
		("Tracker Preferences - Enter a file glob",
		 NULL,
		 GTK_DIALOG_MODAL,
		 GTK_STOCK_OK,
		 GTK_RESPONSE_ACCEPT,
		 GTK_STOCK_CANCEL,
		 GTK_RESPONSE_CANCEL,
		 NULL);

	GtkWidget *entry = gtk_entry_new ();
	GtkWidget *label =
		gtk_label_new ("Enter a file mask that you wish to ignore:");

	g_signal_connect (G_OBJECT (entry), "activate",
			  G_CALLBACK (activate_dialog_entry), dialog);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, TRUE,
			    TRUE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), entry, TRUE,
			    TRUE, 0);
	gtk_widget_show_all (dialog);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		pattern = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

	gtk_widget_destroy (dialog);
	return pattern;
}
