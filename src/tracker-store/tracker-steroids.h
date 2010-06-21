/* 
 * Copyright (C) 2010, Codeminded BVBA <abustany@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __TRACKER_STEROIDS_H__
#define __TRACKER_STEROIDS_H__

#include <glib-object.h>

#define TRACKER_STEROIDS_SERVICE   "org.freedesktop.Tracker1"
#define TRACKER_STEROIDS_PATH      "/org/freedesktop/Tracker1/Steroids"
#define TRACKER_STEROIDS_INTERFACE "org.freedesktop.Tracker1.Steroids"

G_BEGIN_DECLS

#define TRACKER_TYPE_STEROIDS            (tracker_steroids_get_type ())
#define TRACKER_STEROIDS(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_STEROIDS, TrackerSteroids))
#define TRACKER_STEROIDS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_DBUS_STEROIDS, TrackerSteroidsClass))
#define TRACKER_IS_STEROIDS(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_STEROIDS))
#define TRACKER_IS_STEROIDS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_STEROIDS))
#define TRACKER_STEROIDS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_STEROIDS, TrackerSteroidsClass))

#define TRACKER_STEROIDS_BUFFER_SIZE 65536

typedef struct TrackerSteroids      TrackerSteroids;
typedef struct TrackerSteroidsClass TrackerSteroidsClass;

struct TrackerSteroids {
	GObject parent;
};

struct TrackerSteroidsClass {
	GObjectClass parent;
};

GType             tracker_steroids_get_type          (void) G_GNUC_CONST;

TrackerSteroids*  tracker_steroids_new               (void);
DBusHandlerResult tracker_steroids_connection_filter (DBusConnection *connection,
                                                      DBusMessage    *message,
                                                      void           *user_data);
#endif /* __TRACKER_STEROIDS_H__ */
