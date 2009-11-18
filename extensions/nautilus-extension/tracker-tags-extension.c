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

#include <gtk/gtk.h>

#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-extension/nautilus-property-page-provider.h>
#include <libtracker-client/tracker.h>

#include "tracker-tags-add-dialog.h"
#include "tracker-tags-utils.h"
#include "tracker-tags-view.h"

#define TRACKER_TYPE_TAGS_PLUGIN (tracker_tags_plugin_get_type ())
#define TRACKER_TAGS_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_TAGS_PLUGIN, TrackerTagsPlugin))
#define TRACKER_TAGS_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_TAGS_PLUGIN, TrackerTagsPluginClass))
#define TRACKER_IS_TAGS_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_TAGS_PLUGIN))
#define TRACKER_IS_TAGS_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_TAGS_PLUGIN))

typedef struct _TrackerTagsPluginPrivate	TrackerTagsPluginPrivate;

typedef struct _TrackerTagsPlugin		TrackerTagsPlugin;
typedef struct _TrackerTagsPluginClass		TrackerTagsPluginClass;

struct _TrackerTagsPlugin
{
	GObject parent;
	TrackerTagsPluginPrivate *priv;
};

struct _TrackerTagsPluginClass
{
	GObjectClass parent;
};

struct _TrackerTagsPluginPrivate
{
	TrackerClient	*tracker_client;
};

static void	tracker_tags_plugin_menu_provider_iface_init	(NautilusMenuProviderIface *iface);
static void	tracker_tags_plugin_property_page_provider_iface_init	(NautilusPropertyPageProviderIface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (TrackerTagsPlugin, tracker_tags_plugin, G_TYPE_OBJECT, 0,
				G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_MENU_PROVIDER,
						       tracker_tags_plugin_menu_provider_iface_init)
				G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER,
						       tracker_tags_plugin_property_page_provider_iface_init));

#define TRACKER_TAGS_PLUGIN_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TRACKER_TYPE_TAGS_PLUGIN, TrackerTagsPluginPrivate))

static void
tracker_tags_plugin_update_finished (GError *error, gpointer user_data)
{
	if (NULL != error)
	{
		GtkWidget *error_dialog;

		error_dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_NO_SEPARATOR, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, error->message);
		g_signal_connect (error_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_dialog_run (GTK_DIALOG (error_dialog));
		g_error_free (error);
	}
}

static void
tracker_tags_plugin_add_dialog_response_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	TrackerTagsPlugin *const plugin = TRACKER_TAGS_PLUGIN (user_data);

	switch (response_id)
	{
	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_DELETE_EVENT:
		break;

	case GTK_RESPONSE_OK:
	{
		const gchar *query;
		const gchar *tag_label;

		tag_label = tracker_tags_add_dialog_get_text (TRACKER_TAGS_ADD_DIALOG (dialog));
		query = tracker_tags_utils_add_query (tag_label);
		tracker_resources_sparql_update_async (plugin->priv->tracker_client, query, tracker_tags_plugin_update_finished, NULL);
		g_free ((gpointer) query);
		break;
	}

	default:
		g_assert_not_reached ();
		break;
	}
}

static void
tracker_tags_plugin_add_activate_cb (NautilusMenuItem *menu_item, gpointer user_data)
{
	void **const arg = user_data;
	TrackerTagsPlugin *const plugin = TRACKER_TAGS_PLUGIN (arg[0]);
	GtkWindow *const window = GTK_WINDOW (arg[1]);
	GtkWidget *add_dialog;

	/* g_free (arg); */

	add_dialog = tracker_tags_add_dialog_new ();
	gtk_window_set_transient_for (GTK_WINDOW (add_dialog), window);
	g_signal_connect (add_dialog, "response", G_CALLBACK (tracker_tags_plugin_add_dialog_response_cb), plugin);
	gtk_widget_show_all (add_dialog);
}

static void
tracker_tags_plugin_manage_activate_cb (NautilusMenuItem *menu_item, gpointer user_data)
{
	void **const arg = user_data;
	GList *const files = arg[0];
	GtkWindow *const window = GTK_WINDOW (arg[1]);
	GtkWidget *action_area;
	GtkWidget *manage_dialog;
	GtkWidget *vbox;
	GtkWidget *view;

	/* g_free (arg); */

	manage_dialog = gtk_dialog_new_with_buttons (N_("Manage Tags"), window, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR, GTK_STOCK_CLOSE, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (manage_dialog), 5);
	gtk_window_set_default_size (GTK_WINDOW (manage_dialog), 250, 375);
	gtk_window_set_resizable (GTK_WINDOW (manage_dialog), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (manage_dialog), window);
	g_signal_connect (manage_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (manage_dialog));
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);

	vbox = gtk_dialog_get_content_area (GTK_DIALOG (manage_dialog));
	gtk_box_set_spacing (GTK_BOX (vbox), 2);

	view = tracker_tags_view_new (files);
	gtk_box_pack_start (GTK_BOX (vbox), view, TRUE, TRUE, 0);

	gtk_widget_show_all (manage_dialog);
}

static GList *
tracker_tags_plugin_get_background_items (NautilusMenuProvider	*provider,
					  GtkWidget		*window,
					  NautilusFileInfo	*current_folder)
{
	GList *menu_items = NULL;
	NautilusMenuItem *menu_item;
	void **arg;

	if (NULL == current_folder)
		return NULL;

	menu_item = nautilus_menu_item_new ("tracker-tags-new", N_("Create Tag..."), N_("Create a new tag for this desktop"), NULL);
	menu_items = g_list_append (menu_items, menu_item);
	arg = g_new (void *, 2);
	arg[0] = provider;
	arg[1] = window;
	g_signal_connect (menu_item, "activate", G_CALLBACK (tracker_tags_plugin_add_activate_cb), arg);

	return menu_items;
}

static GList *
tracker_tags_plugin_get_file_items (NautilusMenuProvider	*provider,
				    GtkWidget			*window,
				    GList			*files)
{
	GList *menu_items = NULL;
	NautilusMenuItem *menu_item;
	void **arg;

	if (NULL == files)
		return NULL;

	menu_item = nautilus_menu_item_new ("tracker-tags-new", N_("Create Tag..."), N_("Create a new tag for this desktop"), NULL);
	menu_items = g_list_append (menu_items, menu_item);
	arg = g_new (void *, 2);
	arg[0] = provider;
	arg[1] = window;
	g_signal_connect (menu_item, "activate", G_CALLBACK (tracker_tags_plugin_add_activate_cb), arg);

	menu_item = nautilus_menu_item_new ("tracker-tags-manage", N_("Manage Tags..."), N_("Change the tags attached to the selected objects"), NULL);
	menu_items = g_list_append (menu_items, menu_item);
	arg = g_new (void *, 2);
	arg[0] = g_list_copy (files);
	arg[1] = window;
	g_signal_connect (menu_item, "activate", G_CALLBACK (tracker_tags_plugin_manage_activate_cb), arg);

	return menu_items;
}

static GList *
tracker_tags_plugin_get_pages (NautilusPropertyPageProvider	*provider,
			       GList				*files)
{
	GList *property_pages = NULL;
	GtkWidget *label;
	GtkWidget *view;
	NautilusPropertyPage *property_page;

	if (NULL == files)
		return NULL;

	label = gtk_label_new ("Tags");
	view = tracker_tags_view_new (files);
	property_page = nautilus_property_page_new ("tracker-tags", label, view);

	property_pages = g_list_prepend (property_pages, property_page);
	return property_pages;
}

static GList *
tracker_tags_plugin_get_toolbar_items (NautilusMenuProvider	*provider,
				       GtkWidget		*window,
				       NautilusFileInfo		*current_folder)
{
	return NULL;
}

static void
tracker_tags_plugin_menu_provider_iface_init (NautilusMenuProviderIface *iface)
{
        iface->get_file_items = tracker_tags_plugin_get_file_items;
        iface->get_background_items = tracker_tags_plugin_get_background_items;
        iface->get_toolbar_items = tracker_tags_plugin_get_toolbar_items;
}

static void
tracker_tags_plugin_property_page_provider_iface_init (NautilusPropertyPageProviderIface *iface)
{
	iface->get_pages = tracker_tags_plugin_get_pages;
}

static void
tracker_tags_plugin_class_finalize (TrackerTagsPluginClass *klass)
{
}

static void
tracker_tags_plugin_finalize (GObject *object)
{
	TrackerTagsPlugin *const plugin = TRACKER_TAGS_PLUGIN (object);

	tracker_disconnect (plugin->priv->tracker_client);
	G_OBJECT_CLASS (tracker_tags_plugin_parent_class)->finalize (object);
}

static void
tracker_tags_plugin_class_init (TrackerTagsPluginClass *klass)
{
	GObjectClass *const gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = tracker_tags_plugin_finalize;
	g_type_class_add_private (gobject_class, sizeof (TrackerTagsPluginPrivate));
}

static void
tracker_tags_plugin_init (TrackerTagsPlugin *self)
{
	TrackerTagsPluginPrivate *const priv = TRACKER_TAGS_PLUGIN_GET_PRIVATE (self);

	self->priv = priv;
	self->priv->tracker_client = tracker_connect (TRUE, G_MAXINT);
}

void
nautilus_module_initialize (GTypeModule *module)
{
	tracker_tags_plugin_register_type (module);
	tracker_tags_view_register_type (module);
}

void
nautilus_module_shutdown (void)
{
}

void
nautilus_module_list_types (const GType **types,
                            int          *num_types)
{
	static GType type_list[1];

	type_list[0] = tracker_tags_plugin_type_id;
	*types = type_list;
	*num_types = G_N_ELEMENTS (type_list);
}
