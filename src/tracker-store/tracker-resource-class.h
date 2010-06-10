/*
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
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __TRACKER_STORE_RESOURCES_CLASS_H__
#define __TRACKER_STORE_RESOURCES_CLASS_H__

#include <glib-object.h>
#include <libtracker-common/tracker-dbus.h>
#include <libtracker-data/tracker-property.h>

#define TRACKER_RESOURCES_CLASS_SERVICE                "org.freedesktop.Tracker1"
#define TRACKER_RESOURCES_CLASS_PATH           "/org/freedesktop/Tracker1/Resources/Classes/%s"
#define TRACKER_RESOURCES_CLASS_INTERFACE       "org.freedesktop.Tracker1.Resources.Class"

G_BEGIN_DECLS

#define TRACKER_TYPE_RESOURCE_CLASS            (tracker_resource_class_get_type ())
#define TRACKER_RESOURCE_CLASS(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_RESOURCE_CLASS, TrackerResourceClass))
#define TRACKER_RESOURCE_CLASS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_RESOURCE_CLASS, TrackerResourceClassClass))
#define TRACKER_IS_RESOURCE_CLASS(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_RESOURCE_CLASS))
#define TRACKER_IS_RESOURCE_CLASS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_RESOURCE_CLASS))
#define TRACKER_RESOURCE_CLASS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_RESOURCE_CLASS, TrackerResourceClassClass))

typedef struct TrackerResourceClass       TrackerResourceClass;
typedef struct TrackerResourceClassClass TrackerResourceClassClass;

struct TrackerResourceClass {
	GObject parent;
};

struct TrackerResourceClassClass {
	GObjectClass parent;
};

GType                  tracker_resource_class_get_type      (void);
TrackerResourceClass  *tracker_resource_class_new           (const gchar *rdf_class);

const gchar *          tracker_resource_class_get_rdf_class (TrackerResourceClass  *object);

void                   tracker_resource_class_add_event     (TrackerResourceClass  *object,
                                                             const gchar           *uri,
                                                             TrackerProperty       *predicate,
                                                             TrackerDBusEventsType  type);
void                   tracker_resource_class_emit_events   (TrackerResourceClass  *object);

G_END_DECLS

#endif /* __TRACKER_STORE_RESOURCES_CLASS_H__ */
