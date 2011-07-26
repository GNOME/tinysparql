/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
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
 *
 * Author: Carlos Garnacho <carlos@lanedo.com>
 */

#ifndef __LIBTRACKER_MINER_SPARQL_BUFFER_H__
#define __LIBTRACKER_MINER_SPARQL_BUFFER_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-task-pool.h"

G_BEGIN_DECLS

/* Task pool */
#define TRACKER_TYPE_SPARQL_BUFFER         (tracker_sparql_buffer_get_type())
#define TRACKER_SPARQL_BUFFER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_SPARQL_BUFFER, TrackerSparqlBuffer))
#define TRACKER_SPARQL_BUFFER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_SPARQL_BUFFER, TrackerSparqlBufferClass))
#define TRACKER_IS_SPARQL_BUFFER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_SPARQL_BUFFER))
#define TRACKER_IS_SPARQL_BUFFER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_SPARQL_BUFFER))
#define TRACKER_SPARQL_BUFFER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_SPARQL_BUFFER, TrackerSparqlBufferClass))

typedef struct _TrackerSparqlBuffer TrackerSparqlBuffer;
typedef struct _TrackerSparqlBufferClass TrackerSparqlBufferClass;

typedef enum {
	TRACKER_BULK_MATCH_EQUALS   = 1 << 0,
	TRACKER_BULK_MATCH_CHILDREN = 1 << 1
} TrackerBulkTaskFlags;

struct _TrackerSparqlBuffer
{
	TrackerTaskPool parent_instance;
	gpointer priv;
};

struct _TrackerSparqlBufferClass
{
	TrackerTaskPoolClass parent_class;
};

GType                tracker_sparql_buffer_get_type (void) G_GNUC_CONST;

TrackerSparqlBuffer *tracker_sparql_buffer_new   (TrackerSparqlConnection *connection,
                                                  guint                    limit);

gboolean             tracker_sparql_buffer_flush (TrackerSparqlBuffer *buffer,
                                                  const gchar         *reason);

void                 tracker_sparql_buffer_push  (TrackerSparqlBuffer *buffer,
                                                  TrackerTask         *task,
                                                  gint                 priority,
                                                  GAsyncReadyCallback  cb,
                                                  gpointer             user_data);

TrackerTask *        tracker_sparql_task_new_take_sparql_str (GFile                *file,
                                                              gchar                *sparql_str);
TrackerTask *        tracker_sparql_task_new_with_sparql_str (GFile                *file,
                                                              const gchar          *sparql_str);
TrackerTask *        tracker_sparql_task_new_with_sparql     (GFile                *file,
                                                              TrackerSparqlBuilder *builder);
TrackerTask *        tracker_sparql_task_new_bulk            (GFile                *file,
                                                              const gchar          *sparql_str,
                                                              TrackerBulkTaskFlags  flags);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_SPARQL_BUFFER_H__ */
