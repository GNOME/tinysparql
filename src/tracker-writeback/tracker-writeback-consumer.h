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
 */

#ifndef __TRACKER_WRITEBACK_CONSUMER_H__
#define __TRACKER_WRITEBACK_CONSUMER_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_WRITEBACK_CONSUMER         (tracker_writeback_consumer_get_type())
#define TRACKER_WRITEBACK_CONSUMER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_WRITEBACK_CONSUMER, TrackerWritebackConsumer))
#define TRACKER_WRITEBACK_CONSUMER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_WRITEBACK_CONSUMER, TrackerWritebackConsumerClass))
#define TRACKER_IS_WRITEBACK_CONSUMER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_WRITEBACK_CONSUMER))
#define TRACKER_IS_WRITEBACK_CONSUMER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_WRITEBACK_CONSUMER))
#define TRACKER_WRITEBACK_CONSUMER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_WRITEBACK_CONSUMER, TrackerWritebackConsumerClass))

typedef struct TrackerWritebackConsumer TrackerWritebackConsumer;
typedef struct TrackerWritebackConsumerClass TrackerWritebackConsumerClass;

struct TrackerWritebackConsumer {
	GObject parent_instance;
};

struct TrackerWritebackConsumerClass {
	GObjectClass parent_class;
};

GType                      tracker_writeback_consumer_get_type (void) G_GNUC_CONST;
TrackerWritebackConsumer * tracker_writeback_consumer_new      (void);

void tracker_writeback_consumer_add_subject (TrackerWritebackConsumer *consumer,
                                             gint                      subject,
                                             GArray                   *rdf_types);

G_END_DECLS

#endif /* __TRACKER_WRITEBACK_CONSUMER_H__ */
