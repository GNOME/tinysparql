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

#ifndef __TRACKER_WRITEBACK_DISPATCHER_H__
#define __TRACKER_WRITEBACK_DISPATCHER_H__


#include <glib-object.h>
#include <gio/gio.h>
#include <libtracker-client/tracker.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_WRITEBACK_DISPATCHER         (tracker_writeback_dispatcher_get_type())
#define TRACKER_WRITEBACK_DISPATCHER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_WRITEBACK_DISPATCHER, TrackerWritebackDispatcher))
#define TRACKER_WRITEBACK_DISPATCHER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_WRITEBACK_DISPATCHER, TrackerWritebackDispatcherClass))
#define TRACKER_IS_WRITEBACK_DISPATCHER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_WRITEBACK_DISPATCHER))
#define TRACKER_IS_WRITEBACK_DISPATCHER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_WRITEBACK_DISPATCHER))
#define TRACKER_WRITEBACK_DISPATCHER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_WRITEBACK_DISPATCHER, TrackerWritebackDispatcherClass))

typedef struct TrackerWritebackDispatcher TrackerWritebackDispatcher;
typedef struct TrackerWritebackDispatcherClass TrackerWritebackDispatcherClass;

struct TrackerWritebackDispatcher {
	GObject parent_instance;
};

struct TrackerWritebackDispatcherClass {
	GObjectClass parent_class;

	void (* writeback) (TrackerWritebackDispatcher *dispatcher,
	                    const gchar                *subject,
	                    const GStrv                 rdf_types);
};

GType                        tracker_writeback_dispatcher_get_type (void) G_GNUC_CONST;

TrackerWritebackDispatcher * tracker_writeback_dispatcher_new (GMainContext *context);

G_END_DECLS

#endif /* __TRACKER_WRITEBACK_DISPATCHER_H__ */
