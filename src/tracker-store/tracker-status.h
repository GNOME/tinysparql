/*
 * Copyright (C) 2008, Nokia
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

#ifndef __TRACKER_STORE_STATUS_H__
#define __TRACKER_STORE_STATUS_H__

#include <glib-object.h>
#include <libtracker-common/tracker-dbus.h>
#include <libtracker-data/tracker-data-update.h>

#define TRACKER_STATUS_SERVICE         "org.freedesktop.Tracker1"
#define TRACKER_STATUS_PATH            "/org/freedesktop/Tracker1/Status"
#define TRACKER_STATUS_INTERFACE       "org.freedesktop.Tracker1.Status"

G_BEGIN_DECLS

#define TRACKER_TYPE_STATUS            (tracker_status_get_type ())
#define TRACKER_STATUS(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_STATUS, TrackerStatus))
#define TRACKER_STATUS_CLASS(klass)    (G_TYPE_CHECK_NOTIFIER_CAST ((klass), TRACKER_TYPE_STATUS, TrackerStatusClass))
#define TRACKER_IS_STATUS(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_STATUS))
#define TRACKER_IS_STATUS_CLASS(klass) (G_TYPE_CHECK_NOTIFIER_TYPE ((klass), TRACKER_TYPE_STATUS))
#define TRACKER_STATUS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_NOTIFIER ((obj), TRACKER_TYPE_STATUS, TrackerStatusClass))

typedef struct TrackerStatus       TrackerStatus;
typedef struct TrackerStatusClass TrackerStatusClass;

struct TrackerStatus {
	GObject parent;
};

struct TrackerStatusClass {
	GObjectClass parent;

	void (* progress) (TrackerStatus       *notifier,
	                   const gchar         *status,
	                   gdouble              progress);
};

GType                 tracker_status_get_type      (void);
TrackerStatus        *tracker_status_new           (void);
TrackerBusyCallback   tracker_status_get_callback  (TrackerStatus          *object,
                                                    gpointer               *user_data);

/* DBus methods */
void                  tracker_status_get_progress  (TrackerStatus          *object,
                                                    DBusGMethodInvocation  *context,
                                                    GError                **error);
void                  tracker_status_get_status    (TrackerStatus          *object,
                                                    DBusGMethodInvocation  *context,
                                                    GError                **error);
void                  tracker_status_wait          (TrackerStatus          *object,
                                                    DBusGMethodInvocation  *context,
                                                    GError                **error);


G_END_DECLS

#endif /* __TRACKER_STORE_STATUS_H__ */
