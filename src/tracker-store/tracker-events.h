/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#ifndef __TRACKER_STORE_EVENTS_H__
#define __TRACKER_STORE_EVENTS_H__

#include <libtracker-common/tracker-common.h>
#include <libtracker-data/tracker-data.h>

G_BEGIN_DECLS

typedef struct TrackerEvent TrackerEvent;

struct TrackerEvent {
	TrackerDBusEventsType type;
	TrackerClass *class;
	TrackerProperty *predicate;
	gchar *subject;
};

typedef GStrv (*TrackerNotifyClassGetter) (void);

void       tracker_events_init        (TrackerNotifyClassGetter  callback);
void       tracker_events_shutdown    (void);
void       tracker_events_insert      (const gchar              *uri,
                                       const gchar              *predicate,
                                       const gchar              *object,
                                       GPtrArray                *rdf_types,
                                       TrackerDBusEventsType     type);
GArray    *tracker_events_get_pending (void);
void       tracker_events_reset       (void);
void       tracker_events_freeze      (void);

G_END_DECLS

#endif /* __TRACKER_STORE_EVENTS_H__ */
