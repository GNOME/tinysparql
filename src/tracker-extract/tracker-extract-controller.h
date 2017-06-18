/*
 * Copyright (C) 2014 - Collabora Ltd.
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

#include "tracker-extract-decorator.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_EXTRACT_CONTROLLER         (tracker_extract_controller_get_type ())
#define TRACKER_EXTRACT_CONTROLLER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_EXTRACT_CONTROLLER, TrackerExtractController))
#define TRACKER_EXTRACT_CONTROLLER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_EXTRACT_CONTROLLER, TrackerExtractControllerClass))
#define TRACKER_IS_EXTRACT_CONTROLLER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_EXTRACT_CONTROLLER))
#define TRACKER_IS_EXTRACT_CONTROLLER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TRACKER_TYPE_EXTRACT_CONTROLLER))
#define TRACKER_EXTRACT_CONTROLLER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_EXTRACT_CONTROLLER, TrackerExtractControllerClass))

typedef struct TrackerExtractController TrackerExtractController;
typedef struct TrackerExtractControllerClass TrackerExtractControllerClass;
typedef struct TrackerExtractControllerPrivate TrackerExtractControllerPrivate;

struct TrackerExtractController {
	GObject parent_instance;
	TrackerExtractControllerPrivate *priv;
};

struct TrackerExtractControllerClass {
	GObjectClass parent_class;
};

GType                      tracker_extract_controller_get_type (void) G_GNUC_CONST;
TrackerExtractController * tracker_extract_controller_new      (TrackerDecorator *decorator,
                                                                GDBusConnection  *connection);

G_END_DECLS

#endif /* __TRACKER_EXTRACT_CONTROLLER_H__ */
