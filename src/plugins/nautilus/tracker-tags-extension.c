/*
 * Copyright (C) 2009, Debarshi Ray <debarshir@src.gnome.org>
 * Copyright (C) 2009, Martyn Russell <martyn@lanedo.com>
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libnautilus-extension/nautilus-property-page-provider.h>

#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-tags-utils.h"
#include "tracker-tags-view.h"

#define TRACKER_TYPE_TAGS_EXTENSION (tracker_tags_extension_get_type ())
#define TRACKER_TAGS_EXTENSION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_TAGS_EXTENSION, TrackerTagsExtension))
#define TRACKER_TAGS_EXTENSION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_TAGS_EXTENSION, TrackerTagsExtensionClass))

#define TRACKER_IS_TAGS_EXTENSION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_TAGS_EXTENSION))
#define TRACKER_IS_TAGS_EXTENSION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_TAGS_EXTENSION))

typedef struct _TrackerTagsExtension TrackerTagsExtension;
typedef struct _TrackerTagsExtensionClass TrackerTagsExtensionClass;

struct _TrackerTagsExtension {
	GObject parent;
};

struct _TrackerTagsExtensionClass {
	GObjectClass parent;
};

GType        tracker_tags_extension_get_type                          (void);
static void  tracker_tags_extension_property_page_provider_iface_init (NautilusPropertyPageProviderIface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (TrackerTagsExtension, tracker_tags_extension, G_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER,
                                                       tracker_tags_extension_property_page_provider_iface_init));

static GList *
extension_get_pages (NautilusPropertyPageProvider *provider,
                     GList                        *files)
{
	GList *property_pages = NULL;
	GtkWidget *label;
	GtkWidget *view;
	NautilusPropertyPage *property_page;

	if (files == NULL) {
		return NULL;
	}

	label = gtk_label_new (_("Tags"));
	view = tracker_tags_view_new (files);
	gtk_widget_show (view);
	property_page = nautilus_property_page_new ("tracker-tags", label, view);
	property_pages = g_list_prepend (property_pages, property_page);

	return property_pages;
}

static void
tracker_tags_extension_property_page_provider_iface_init (NautilusPropertyPageProviderIface *iface)
{
	iface->get_pages = extension_get_pages;
}

static void
tracker_tags_extension_class_init (TrackerTagsExtensionClass *klass)
{
}

static void
tracker_tags_extension_class_finalize (TrackerTagsExtensionClass *klass)
{
}

static void
tracker_tags_extension_init (TrackerTagsExtension *self)
{
}

void
nautilus_module_initialize (GTypeModule *module)
{
	g_debug ("Initializing tracker-tags extension\n");
	tracker_tags_extension_register_type (module);
	tracker_tags_view_register_types (module);
}

void
nautilus_module_shutdown (void)
{
	g_debug ("Shutting down tracker-tags extension\n");
}

void
nautilus_module_list_types (const GType **types,
                            gint         *num_types)
{
	static GType type_list[1];

	type_list[0] = tracker_tags_extension_type_id;
	*types = type_list;
	*num_types = G_N_ELEMENTS (type_list);
}
