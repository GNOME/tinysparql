/*
 * Copyright (C) 2016 Red Hat Inc.
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

#ifndef __TRACKER_NOTIFIER_H__
#define __TRACKER_NOTIFIER_H__

#if !defined (__LIBTRACKER_SPARQL_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-sparql/tracker-sparql.h> must be included directly."
#endif

#include <gio/gio.h>
#include <libtracker-sparql/tracker-version.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_NOTIFIER (tracker_notifier_get_type ())
#define TRACKER_TYPE_NOTIFIER_EVENT (tracker_notifier_event_get_type ())

/**
 * TrackerNotifier:
 *
 * The <structname>TrackerNotifier</structname> object allows subscribing
 * to changes in the stored data.
 */
TRACKER_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (TrackerNotifier, tracker_notifier, TRACKER, NOTIFIER, GObject)

/**
 * TrackerNotifierEvent:
 *
 * The <structname>TrackerNotifierEvent</structname> struct represents a
 * change event in the stored data.
 */
typedef struct _TrackerNotifierEvent TrackerNotifierEvent;

/**
 * TrackerNotifierEventType:
 * @TRACKER_NOTIFIER_EVENT_CREATE: An element was created.
 * @TRACKER_NOTIFIER_EVENT_DELETE: An element was deleted.
 * @TRACKER_NOTIFIER_EVENT_UPDATE: An element was updated.
 *
 * Notifier event types.
 */
typedef enum {
	TRACKER_NOTIFIER_EVENT_CREATE,
	TRACKER_NOTIFIER_EVENT_DELETE,
	TRACKER_NOTIFIER_EVENT_UPDATE
} TrackerNotifierEventType;

TRACKER_AVAILABLE_IN_ALL
guint tracker_notifier_signal_subscribe   (TrackerNotifier *notifier,
                                           GDBusConnection *connection,
                                           const gchar     *service,
                                           const gchar     *object_path,
                                           const gchar     *graph);
TRACKER_AVAILABLE_IN_ALL
void  tracker_notifier_signal_unsubscribe (TrackerNotifier *notifier,
                                           guint            handler_id);

TRACKER_AVAILABLE_IN_ALL
GType tracker_notifier_event_get_type (void) G_GNUC_CONST;

TRACKER_AVAILABLE_IN_ALL
TrackerNotifierEventType
              tracker_notifier_event_get_event_type (TrackerNotifierEvent *event);
TRACKER_AVAILABLE_IN_ALL
gint64        tracker_notifier_event_get_id         (TrackerNotifierEvent *event);
TRACKER_AVAILABLE_IN_ALL
const gchar * tracker_notifier_event_get_urn        (TrackerNotifierEvent *event);

G_END_DECLS

#endif /* __TRACKER_NOTIFIER_H__ */
