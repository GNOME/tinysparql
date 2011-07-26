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
 */

#ifndef __TRACKER_EXTRACT_CONTROLLER_H__
#define __TRACKER_EXTRACT_CONTROLLER_H__

#include <gio/gio.h>

#include "tracker-extract.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_CONTROLLER         (tracker_controller_get_type ())
#define TRACKER_CONTROLLER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_CONTROLLER, TrackerController))
#define TRACKER_CONTROLLER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_CONTROLLER, TrackerControllerClass))
#define TRACKER_IS_CONTROLLER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_CONTROLLER))
#define TRACKER_IS_CONTROLLER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TRACKER_TYPE_CONTROLLER))
#define TRACKER_CONTROLLER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_CONTROLLER, TrackerControllerClass))

typedef struct TrackerController TrackerController;
typedef struct TrackerControllerClass TrackerControllerClass;

struct TrackerController {
        GObject parent_instance;
        gpointer priv;
};

struct TrackerControllerClass {
        GObjectClass parent_class;
};

GType               tracker_controller_get_type (void) G_GNUC_CONST;

TrackerController * tracker_controller_new      (TrackerExtract     *extractor,
                                                 guint               shutdown_timeout,
                                                 GError            **error);
gboolean            tracker_controller_start    (TrackerController  *controller,
                                                 GError            **error);

G_END_DECLS

#endif /* __TRACKER_EXTRACT_CONTROLLER_H__ */
