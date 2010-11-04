/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#ifndef __TRACKER_STORE_RESOURCES_H__
#define __TRACKER_STORE_RESOURCES_H__

#include <glib-object.h>

#define TRACKER_RESOURCES_SERVICE        "org.freedesktop.Tracker1"
#define TRACKER_RESOURCES_PATH           "/org/freedesktop/Tracker1/Resources"
#define TRACKER_RESOURCES_INTERFACE      "org.freedesktop.Tracker1.Resources"

G_BEGIN_DECLS

#define TRACKER_TYPE_RESOURCES           (tracker_resources_get_type ())
#define TRACKER_RESOURCES(object)        (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_RESOURCES, TrackerResources))
#define TRACKER_RESOURCES_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_RESOURCES, TrackerResourcesClass))
#define TRACKER_IS_RESOURCES(object)     (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_RESOURCES))
#define TRACKER_IS_RESOURCES_CLASS(klass)(G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_RESOURCES))
#define TRACKER_RESOURCES_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_RESOURCES, TrackerResourcesClass))

typedef struct TrackerResources         TrackerResources;
typedef struct TrackerResourcesClass TrackerResourcesClass;

struct TrackerResources {
	GObject parent;
};

struct TrackerResourcesClass {
	GObjectClass parent;

	void     (*writeback)                               (TrackerResources *resources,
	                                                     GStrv subjects);
	void     (*graph_updated)                           (TrackerResources *resources,
	                                                     const gchar      *classname,
	                                                     GPtrArray        *deletes,
	                                                     GPtrArray        *inserts);
};

GType             tracker_resources_get_type            (void);
TrackerResources *tracker_resources_new                 (DBusGConnection        *connection);
void              tracker_resources_enable_signals      (TrackerResources       *object);
void              tracker_resources_disable_signals     (TrackerResources       *object);

void              tracker_resources_unreg_batches       (TrackerResources       *object,
                                                         const gchar            *old_owner);

/* DBus methods */
void              tracker_resources_load                (TrackerResources       *object,
                                                         const gchar            *uri,
                                                         DBusGMethodInvocation  *context,
                                                         GError                **error);
void              tracker_resources_sparql_query        (TrackerResources       *object,
                                                         const gchar            *query,
                                                         DBusGMethodInvocation  *context,
                                                         GError                **error);
void              tracker_resources_sparql_update       (TrackerResources       *object,
                                                         const gchar            *update,
                                                         DBusGMethodInvocation  *context,
                                                         GError                **error);
void              tracker_resources_sparql_update_blank (TrackerResources       *object,
                                                         const gchar            *update,
                                                         DBusGMethodInvocation  *context,
                                                         GError                **error);
void              tracker_resources_sync                (TrackerResources       *object,
                                                         DBusGMethodInvocation  *context,
                                                         GError                **error);
void              tracker_resources_batch_sparql_update (TrackerResources       *object,
                                                         const gchar            *update,
                                                         DBusGMethodInvocation  *context,
                                                         GError                **error);
void              tracker_resources_batch_commit        (TrackerResources       *object,
                                                         DBusGMethodInvocation  *context,
                                                         GError                **error);

G_END_DECLS

#endif /* __TRACKER_STORE_RESOURCES_H__ */
