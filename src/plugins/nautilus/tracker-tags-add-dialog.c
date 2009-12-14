/*
 * Copyright (C) 2009  Debarshi Ray <debarshir@src.gnome.org>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <glib/gi18n.h>

#include "tracker-tags-add-dialog.h"

#define TRACKER_TAGS_ADD_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TRACKER_TYPE_TAGS_ADD_DIALOG, TrackerTagsAddDialogPrivate))

struct _TrackerTagsAddDialogPrivate {
	GtkWidget *entry;
};

static void dialog_finalize         (GObject     *object);
static void dialog_response         (GtkDialog   *dialog,
                                     gint         response_id);
static void dialog_entry_changed_cb (GtkEditable *editable,
                                     gpointer     user_data);

G_DEFINE_TYPE (TrackerTagsAddDialog, tracker_tags_add_dialog, GTK_TYPE_DIALOG)

static void
tracker_tags_add_dialog_class_init (TrackerTagsAddDialogClass *klass)
{
	GObjectClass *const gobject_class = G_OBJECT_CLASS (klass);
	GtkDialogClass *const dialog_class = GTK_DIALOG_CLASS (klass);

	gobject_class->finalize = dialog_finalize;
	dialog_class->response = dialog_response;

	g_type_class_add_private (gobject_class, sizeof (TrackerTagsAddDialogPrivate));

	/* tracker_tags_add_dialog_parent_class = g_type_class_peek_parent (klass); */
}

static void
tracker_tags_add_dialog_init (TrackerTagsAddDialog *dialog)
{
	GtkWidget *action_area;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vbox;

	dialog->private = TRACKER_TAGS_ADD_DIALOG_GET_PRIVATE (dialog);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_title (GTK_WINDOW (dialog), N_("Add Tag"));
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);
	gtk_box_set_spacing (GTK_BOX (action_area), 6);

	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_set_spacing (GTK_BOX (vbox), 2);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

	label = gtk_label_new_with_mnemonic (N_("_Tag label:"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	dialog->private->entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->private->entry), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), dialog->private->entry, TRUE, TRUE, 0);
	g_signal_connect (dialog->private->entry, "changed",
                          G_CALLBACK (dialog_entry_changed_cb),
                          dialog);

	gtk_widget_show_all (vbox);
}

static void
dialog_finalize (GObject *object)
{
	G_OBJECT_CLASS (tracker_tags_add_dialog_parent_class)->finalize (object);
}

static void
dialog_response (GtkDialog *dialog,
                 gint       response_id)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
dialog_entry_changed_cb (GtkEditable *editable,
                         gpointer     user_data)
{
	GtkDialog *dialog = GTK_DIALOG (user_data);
	GtkEntry *entry = GTK_ENTRY (editable);

	if (gtk_entry_get_text_length (entry) < 1) {
		gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, FALSE);
	} else {
		gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, TRUE);
	}
}

void
tracker_tags_add_dialog_register_type (GTypeModule *module)
{
	tracker_tags_add_dialog_get_type ();
}

GtkWidget *
tracker_tags_add_dialog_new (void)
{
	return GTK_WIDGET (g_object_new (TRACKER_TYPE_TAGS_ADD_DIALOG, NULL));
}

const gchar *
tracker_tags_add_dialog_get_text (TrackerTagsAddDialog *add_dialog)
{
	return gtk_entry_get_text (GTK_ENTRY (add_dialog->private->entry));
}
