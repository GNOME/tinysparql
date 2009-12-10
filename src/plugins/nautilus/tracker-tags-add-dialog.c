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

struct _TrackerTagsAddDialogPrivate {
	GtkWidget      *entry;
};

G_DEFINE_TYPE (TrackerTagsAddDialog, tracker_tags_add_dialog, GTK_TYPE_DIALOG)

#define TRACKER_TAGS_ADD_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TRACKER_TYPE_TAGS_ADD_DIALOG, TrackerTagsAddDialogPrivate))

static void
tracker_tags_add_dialog_entry_changed_cb (GtkEditable *editable, gpointer user_data)
{
	GtkDialog *const add_dialog = GTK_DIALOG (user_data);
	GtkEntry *const entry = GTK_ENTRY (editable);

	if (0 == gtk_entry_get_text_length (entry))
		gtk_dialog_set_response_sensitive (add_dialog, GTK_RESPONSE_OK, FALSE);
	else
		gtk_dialog_set_response_sensitive (add_dialog, GTK_RESPONSE_OK, TRUE);
}

void
tracker_tags_add_dialog_register_type (GTypeModule *module)
{
	tracker_tags_add_dialog_get_type ();
}

static void
tracker_tags_add_dialog_finalize (GObject *object)
{
	G_OBJECT_CLASS (tracker_tags_add_dialog_parent_class)->finalize (object);
}

static void
tracker_tags_add_dialog_response (GtkDialog *dialog, gint response_id)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
tracker_tags_add_dialog_class_init (TrackerTagsAddDialogClass *klass)
{
	GObjectClass *const gobject_class = G_OBJECT_CLASS (klass);
	GtkDialogClass *const dialog_class = GTK_DIALOG_CLASS (klass);

	gobject_class->finalize = tracker_tags_add_dialog_finalize;
	dialog_class->response = tracker_tags_add_dialog_response;

	g_type_class_add_private (gobject_class, sizeof (TrackerTagsAddDialogPrivate));

	tracker_tags_add_dialog_parent_class = g_type_class_peek_parent (klass);
}

static void
tracker_tags_add_dialog_init (TrackerTagsAddDialog *add_dialog)
{
	TrackerTagsAddDialogPrivate *const priv = TRACKER_TAGS_ADD_DIALOG_GET_PRIVATE (add_dialog);
	GtkWidget *action_area;
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vbox;

	add_dialog->priv = priv;

	gtk_container_set_border_width (GTK_CONTAINER (add_dialog), 5);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (add_dialog), TRUE);
	gtk_window_set_modal (GTK_WINDOW (add_dialog), TRUE);
	gtk_window_set_title (GTK_WINDOW (add_dialog), N_("Add Tag"));
	gtk_window_set_resizable (GTK_WINDOW (add_dialog), FALSE);
	gtk_dialog_set_has_separator (GTK_DIALOG (add_dialog), FALSE);
	gtk_dialog_add_button (GTK_DIALOG (add_dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (add_dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (add_dialog), GTK_RESPONSE_OK);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (add_dialog), GTK_RESPONSE_OK, FALSE);

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (add_dialog));
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);
	gtk_box_set_spacing (GTK_BOX (action_area), 6);

	vbox = gtk_dialog_get_content_area (GTK_DIALOG (add_dialog));
	gtk_box_set_spacing (GTK_BOX (vbox), 2);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

	label = gtk_label_new_with_mnemonic (N_("_Tag label:"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	add_dialog->priv->entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), add_dialog->priv->entry, TRUE, TRUE, 0);
	g_signal_connect (add_dialog->priv->entry,
                          "changed",
                          G_CALLBACK (tracker_tags_add_dialog_entry_changed_cb),
                          add_dialog);

	gtk_widget_show_all (vbox);
}

GtkWidget *
tracker_tags_add_dialog_new (void)
{
	return GTK_WIDGET (g_object_new (TRACKER_TYPE_TAGS_ADD_DIALOG, NULL));
}

const gchar *
tracker_tags_add_dialog_get_text (TrackerTagsAddDialog *add_dialog)
{
	return gtk_entry_get_text (GTK_ENTRY (add_dialog->priv->entry));
}
