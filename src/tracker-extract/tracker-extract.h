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
 */

#ifndef __TRACKERD_EXTRACT_H__
#define __TRACKERD_EXTRACT_H__

#include <glib-object.h>

#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus.h>

#define TRACKER_EXTRACT_SERVICE        "org.freedesktop.Tracker1.Extract"
#define TRACKER_EXTRACT_PATH           "/org/freedesktop/Tracker1/Extract"
#define TRACKER_EXTRACT_INTERFACE      "org.freedesktop.Tracker1.Extract"

G_BEGIN_DECLS

#define TRACKER_TYPE_EXTRACT           (tracker_extract_get_type ())
#define TRACKER_EXTRACT(object)        (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_EXTRACT, TrackerExtract))
#define TRACKER_EXTRACT_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_EXTRACT, TrackerExtractClass))
#define TRACKER_IS_EXTRACT(object)     (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_EXTRACT))
#define TRACKER_IS_EXTRACT_CLASS(klass)(G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_EXTRACT))
#define TRACKER_EXTRACT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_EXTRACT, TrackerExtractClass))

typedef struct TrackerExtract      TrackerExtract;
typedef struct TrackerExtractClass TrackerExtractClass;

struct TrackerExtract {
	GObject parent;
};

struct TrackerExtractClass {
	GObjectClass parent;
};

GType           tracker_extract_get_type                (void);
TrackerExtract *tracker_extract_new                     (gboolean                disable_shutdown,
                                                         gboolean                force_internal_extractors,
                                                         const gchar            *force_module);
void            tracker_extract_get_pid                 (TrackerExtract         *object,
                                                         DBusGMethodInvocation  *context,
                                                         GError                **error);
void            tracker_extract_get_metadata            (TrackerExtract         *object,
                                                         const gchar            *uri,
                                                         const gchar            *mime,
                                                         DBusGMethodInvocation  *context,
                                                         GError                **error);
#ifdef HAVE_DBUS_FD_PASSING
DBusHandlerResult tracker_extract_connection_filter     (DBusConnection         *connection,
                                                         DBusMessage            *message,
                                                         void                   *user_data);
#endif /* HAVE_DBUS_FD_PASSING */

/* Not DBus API */
void            tracker_extract_get_metadata_by_cmdline (TrackerExtract         *object,
                                                         const gchar            *path,
                                                         const gchar            *mime);

G_END_DECLS

#endif /* __TRACKERD_EXTRACT_H__ */
