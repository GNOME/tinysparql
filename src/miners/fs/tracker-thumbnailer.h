/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef __LIBTRACKER_MINER_THUMBNAILER_H__
#define __LIBTRACKER_MINER_THUMBNAILER_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_THUMBNAILER         (tracker_thumbnailer_get_type())
#define TRACKER_THUMBNAILER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_THUMBNAILER, TrackerThumbnailer))
#define TRACKER_THUMBNAILER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_THUMBNAILER, TrackerThumbnailerClass))
#define TRACKER_IS_THUMBNAILER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_THUMBNAILER))
#define TRACKER_IS_THUMBNAILER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_THUMBNAILER))
#define TRACKER_THUMBNAILER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_THUMBNAILER, TrackerThumbnailerClass))

typedef struct _TrackerThumbnailer TrackerThumbnailer;
typedef struct _TrackerThumbnailerClass TrackerThumbnailerClass;

/**
 * TrackerThumbnailer:
 * @parent: parent object
 *
 * A class implementation for managing thumbnails when mining content.
 **/
struct _TrackerThumbnailer {
	GObject parent;
};

/**
 * TrackerThumbnailerClass:
 * @parent: parent object class
 *
 * Prototype for the class.
 **/
struct _TrackerThumbnailerClass {
	GObjectClass parent;
};


GType    tracker_thumbnailer_get_type   (void) G_GNUC_CONST;
TrackerThumbnailer *
         tracker_thumbnailer_new        (void);

void     tracker_thumbnailer_send       (TrackerThumbnailer *thumbnailer);
gboolean tracker_thumbnailer_move_add   (TrackerThumbnailer *thumbnailer,
					 const gchar        *from_uri,
                                         const gchar        *mime_type,
                                         const gchar        *to_uri);
gboolean tracker_thumbnailer_remove_add (TrackerThumbnailer *thumbnailer,
					 const gchar        *uri,
                                         const gchar        *mime_type);
gboolean tracker_thumbnailer_cleanup    (TrackerThumbnailer *thumbnailer,
					 const gchar        *uri_prefix);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_THUMBNAILER_H__ */
