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

#ifndef __TRACKER_STORE_BUSY_NOTIFIER_H__
#define __TRACKER_STORE_BUSY_NOTIFIER_H__

#include <glib-object.h>
#include <libtracker-common/tracker-dbus.h>
#include <libtracker-data/tracker-data-update.h>

#define TRACKER_BUSY_NOTIFIER_SERVICE         "org.freedesktop.Tracker1"
#define TRACKER_BUSY_NOTIFIER_PATH            "/org/freedesktop/Tracker1/BusyNotifier"
#define TRACKER_BUSY_NOTIFIER_INTERFACE       "org.freedesktop.Tracker1.BusyNotifier"

G_BEGIN_DECLS

#define TRACKER_TYPE_BUSY_NOTIFIER            (tracker_busy_notifier_get_type ())
#define TRACKER_BUSY_NOTIFIER(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_BUSY_NOTIFIER, TrackerBusyNotifier))
#define TRACKER_BUSY_NOTIFIER_CLASS(klass)    (G_TYPE_CHECK_NOTIFIER_CAST ((klass), TRACKER_TYPE_BUSY_NOTIFIER, TrackerBusyNotifierClass))
#define TRACKER_IS_BUSY_NOTIFIER(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_BUSY_NOTIFIER))
#define TRACKER_IS_BUSY_NOTIFIER_CLASS(klass) (G_TYPE_CHECK_NOTIFIER_TYPE ((klass), TRACKER_TYPE_BUSY_NOTIFIER))
#define TRACKER_BUSY_NOTIFIER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_NOTIFIER ((obj), TRACKER_TYPE_BUSY_NOTIFIER, TrackerBusyNotifierClass))

typedef struct TrackerBusyNotifier       TrackerBusyNotifier;
typedef struct TrackerBusyNotifierClass TrackerBusyNotifierClass;

struct TrackerBusyNotifier {
	GObject parent;
};

struct TrackerBusyNotifierClass {
	GObjectClass parent;

	void (* progress) (TrackerBusyNotifier *notifier,
	                   const gchar         *status,
	                   gdouble              progress);
};

GType                 tracker_busy_notifier_get_type      (void);
TrackerBusyNotifier  *tracker_busy_notifier_new           (void);
TrackerBusyCallback   tracker_busy_notifier_get_callback  (TrackerBusyNotifier    *object,
                                                           gpointer               *user_data);

/* DBus methods */
void                  tracker_busy_notifier_get_progress  (TrackerBusyNotifier    *object,
                                                           DBusGMethodInvocation  *context,
                                                           GError                **error);
void                  tracker_busy_notifier_get_status    (TrackerBusyNotifier    *object,
                                                           DBusGMethodInvocation  *context,
                                                           GError                **error);


G_END_DECLS

#endif /* __TRACKER_STORE_BUSY_NOTIFIER_H__ */
