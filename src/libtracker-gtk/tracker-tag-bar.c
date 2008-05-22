/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2007 Neil Jagdish Patel <njpatel@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author : Neil Jagdish Patel <njpatel@gmail.com>
 */

#include <config.h>

#include <string.h>

#include <glib/gi18n-lib.h>

#include "tracker-tag-bar.h"
#include "tracker-utils.h"


#define TRACKER_TAG_BAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_TAG_BAR, TrackerTagBarPrivate))

G_DEFINE_TYPE (TrackerTagBar, tracker_tag_bar, GTK_TYPE_HBOX);

/* FORWARD DECLARATIONS */

static void _tag_bar_add_tag (TrackerTagBar *bar, GtkWidget *box, const char *tag);
static void _tag_launch_search (const gchar *tag);

/* STRUCTS & ENUMS */
typedef struct _TrackerTagBarPrivate TrackerTagBarPrivate;

struct _TrackerTagBarPrivate
{
	TrackerClient *client;
	
	gchar *uri;
	const gchar *active_tag;

	ServiceType type;
	
	GtkWidget *tag_box;
	GtkWidget *add_button;
	GtkWidget *menu;

	GtkWidget *entry_box;
	GtkWidget *entry;
};

/* CALLBACKS */
static void 
_keywords_reply (char **array, GError *error, TrackerTagBar *bar)
{
	TrackerTagBarPrivate *priv;
	gchar **meta = NULL;
	gchar * tag = NULL;
	gint i = 0;
	GtkWidget *hbox;
	
	if (error) {
		g_print ("%s\n", error->message);
		g_clear_error(&error);
		return;
	}
	if (array == NULL) {
		return;
	}
	
	priv = TRACKER_TAG_BAR_GET_PRIVATE (bar);

	hbox = gtk_hbox_new (FALSE, 5);
	for (meta = array; *meta; meta++) {
		tag = meta[0];
		if (strlen (tag) > 0) {
			_tag_bar_add_tag (bar, hbox, tag);
			i++;
		}

	}
	if (priv->tag_box) {
		gtk_widget_destroy(priv->tag_box);
	}
	priv->tag_box = hbox;
	gtk_box_pack_start (GTK_BOX(bar), hbox, FALSE, FALSE, 0);
	
	gtk_widget_show_all (hbox);
	
	g_strfreev (array);
}

static gboolean
_on_tag_button_press_event (GtkWidget 			*button, 
			    GdkEventButton		*event, 
			    TrackerTagBar 		*bar)
{
	TrackerTagBarPrivate *priv;
	GtkWidget *label;
	const gchar *tag;
	
	priv = TRACKER_TAG_BAR_GET_PRIVATE (bar);
	
	label = gtk_bin_get_child (GTK_BIN (button));
	tag = gtk_label_get_text (GTK_LABEL (label));
	
	switch (event->button) {
		case 1:
			_tag_launch_search (tag);
			break;
		case 3:
			priv->active_tag = tag;
			gtk_menu_popup (GTK_MENU (priv->menu),
					NULL, NULL, NULL, bar, 3, event->time);
					
			break;
		default:
			break;
	
	}	
	return FALSE;
}

static void 
_tag_launch_search (const gchar *tag)
{
	GdkScreen *screen;
	gchar *command;
	GError *error = NULL;
	
	screen = gdk_screen_get_default ();
	command = g_strdup_printf ("tracker-search-tool %s", tag);
	
	if (! gdk_spawn_command_line_on_screen (screen, command, &error)) {
		if (error) {
			g_print ("Error : %s", error->message);
			g_error_free (error);
		}
	}
	g_free (command);
}

static void
search_tag_activate_cb(GtkMenuItem *menu_item, TrackerTagBar *bar)
{
	TrackerTagBarPrivate *priv;
	
	priv = TRACKER_TAG_BAR_GET_PRIVATE (bar);
	
	_tag_launch_search (priv->active_tag);
}

static void
remove_tag_activate_cb(GtkMenuItem *menu_item, TrackerTagBar *bar)
{
	TrackerTagBarPrivate *priv;
	GError *error = NULL;
	char *args[1];
	
	priv = TRACKER_TAG_BAR_GET_PRIVATE (bar);
	
	args[0] = g_strdup (priv->active_tag);
	
	tracker_keywords_remove(priv->client, priv->type, priv->uri, 
				 args, &error);
	if (error) {
		g_print ("Tag Removal Error : %s", error->message);
		return;
	}
	gchar *temp = g_strdup (priv->uri);
	tracker_tag_bar_set_uri (bar, priv->type, temp);
	g_free (temp);
}

static void
_on_close_add_tag (GtkButton *but, TrackerTagBar *bar)
{
	TrackerTagBarPrivate *priv;
	priv = TRACKER_TAG_BAR_GET_PRIVATE (bar);
	
	gtk_widget_destroy (priv->entry_box);
	priv->entry_box = priv->entry = NULL;
	
	gtk_widget_show (priv->tag_box);
	gtk_widget_show (priv->add_button);
}

static void
_on_apply_add_tag (GtkButton *but, TrackerTagBar *bar)
{
	TrackerTagBarPrivate *priv;
	const gchar *text;
	gchar **tags;
	GError *error = NULL;
	
	priv = TRACKER_TAG_BAR_GET_PRIVATE (bar);
	
	text = gtk_entry_get_text (GTK_ENTRY (priv->entry));

	if (strcmp (text, "Type tags you want to add here, separated by commas") != 0) {
		
		tags = g_strsplit (text, ",", 0);
	
		tracker_keywords_add(priv->client, priv->type, priv->uri, 
				 tags, &error);
		if (error) {
			g_print ("Tag Addition Error : %s", error->message);
			return;
		}	
	}

	_on_close_add_tag (but, bar);
	gchar *temp = g_strdup (priv->uri);
	tracker_tag_bar_set_uri (bar, priv->type, temp);
	g_free (temp);
}

static void
_on_entry_activate (GtkEntry *entry, TrackerTagBar *bar)
{
	_on_apply_add_tag (NULL, bar);
}

static void
_on_add_tag_clicked (GtkButton *but, TrackerTagBar *bar)
{
	TrackerTagBarPrivate *priv;
	GtkWidget *hbox;
	GtkWidget *entry;
	GtkWidget *image;
	GtkWidget *button;
	
	priv = TRACKER_TAG_BAR_GET_PRIVATE (bar);

	hbox = gtk_hbox_new (FALSE, 4);
	priv->entry_box = hbox;
	
	entry = gtk_entry_new ();
	priv->entry = entry;
	gtk_entry_set_text (GTK_ENTRY (entry), _("Type tags you want to add here, separated by commas"));
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
	gtk_box_pack_start (GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	
	g_signal_connect (G_OBJECT (entry), "activate",
			  G_CALLBACK (_on_entry_activate), bar);
	
	image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	
	button = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_button_set_image (GTK_BUTTON (button), image);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (_on_close_add_tag), bar);
			  
	image = gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU);
	
	button = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_button_set_image (GTK_BUTTON (button), image);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	
	
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (_on_apply_add_tag), bar);	
	
	gtk_box_pack_start (GTK_BOX (bar), hbox, TRUE, TRUE, 0);
	gtk_widget_show_all (hbox);
	gtk_widget_hide (priv->tag_box);
	gtk_widget_hide (priv->add_button);	
	gtk_widget_grab_focus (entry);
}

/* UTILS */

static void 
_tag_bar_add_tag (TrackerTagBar *bar, GtkWidget *box, const char *tag)
{
	GtkWidget *button;
	GtkWidget *label;
	gchar *temp;
	
	temp = g_strdup_printf ("<b><u>%s</u></b>", tag);
	
	label = gtk_label_new (" ");
	gtk_label_set_markup (GTK_LABEL (label), temp);
	
	button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (button), label);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
	gtk_box_pack_start (GTK_BOX(box), button, FALSE, FALSE, 0);

	tracker_set_atk_relationship(button, ATK_RELATION_LABELLED_BY,
				     label);
        tracker_set_atk_relationship(label, ATK_RELATION_LABEL_FOR,
				     button);

	g_signal_connect (G_OBJECT (button), "button-press-event",
			  G_CALLBACK (_on_tag_button_press_event), bar);

	g_free (temp);	
}

/* HEADER FUNCTIONS */
void 	   
tracker_tag_bar_set_uri (TrackerTagBar *bar, ServiceType type, const gchar *uri)
{
	TrackerTagBarPrivate *priv;
	
	priv = TRACKER_TAG_BAR_GET_PRIVATE (bar);
		
	if (priv->uri)
		g_free (priv->uri);
	priv->uri = g_strdup (uri);
	priv->type = type;
	
	tracker_keywords_get_async (priv->client, priv->type, uri, 
				    (TrackerArrayReply)_keywords_reply, 
				    bar);
}

/* TRACKER TAG BAR NEW */
static void
tracker_tag_bar_class_init (TrackerTagBarClass *class)
{
	GObjectClass *obj_class;
	GtkWidgetClass *widget_class;

	obj_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	g_type_class_add_private (obj_class, sizeof (TrackerTagBarPrivate));
}

static void
tracker_tag_bar_init (TrackerTagBar *tag_bar)
{
	TrackerTagBarPrivate *priv;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *image;
	GtkWidget *menu;
	GtkWidget *item;
	
	priv = TRACKER_TAG_BAR_GET_PRIVATE (tag_bar);
	
	priv->uri = NULL;
	priv->active_tag = NULL;
	
	gtk_container_set_border_width (GTK_CONTAINER(tag_bar), 0);
	
	label = gtk_label_new (_("Tags :"));
	gtk_box_pack_start (GTK_BOX(tag_bar), label, FALSE, TRUE, 0);
	
	hbox = gtk_hbox_new (FALSE, 4);
	priv->tag_box = hbox;
	gtk_box_pack_start (GTK_BOX(tag_bar), hbox, FALSE, TRUE, 0);
	
	image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
	
	button = gtk_button_new ();
	priv->add_button = button;
	gtk_box_pack_start (GTK_BOX(tag_bar), button, FALSE, FALSE, 0);
	gtk_button_set_image (GTK_BUTTON (button), image);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

        tracker_set_atk_relationship(button, ATK_RELATION_LABELLED_BY,
				     label);
        tracker_set_atk_relationship(label, ATK_RELATION_LABEL_FOR,
				     button);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (_on_add_tag_clicked), tag_bar);
	
	menu = gtk_menu_new();
	priv->menu = menu;
	
	/* Search For Tag */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Search For Tag"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(search_tag_activate_cb), tag_bar);

	image = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(image);

	/* Remove Tag */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Remove Tag"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(remove_tag_activate_cb), tag_bar);

	image = gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(image);	
}

GtkWidget *
tracker_tag_bar_new (void)
{
	TrackerClient *client;
	GtkWidget *tag_bar;
	TrackerTagBarPrivate *priv;
	
	tag_bar = g_object_new (TRACKER_TYPE_TAG_BAR, 
			        "homogeneous", FALSE,
			        "spacing", 0 ,
			        NULL);
	priv = TRACKER_TAG_BAR_GET_PRIVATE (tag_bar);
	
	client = tracker_connect (TRUE);
	priv->client = client;
	return tag_bar;
}
