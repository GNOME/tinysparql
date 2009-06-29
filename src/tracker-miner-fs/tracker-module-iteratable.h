/* Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

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

#ifndef __TRACKER_MODULE_ITERATABLE_H__
#define __TRACKER_MODULE_ITERATABLE_H__

#include <glib-object.h>
#include "tracker-module-file.h"

G_BEGIN_DECLS

#if !defined (__TRACKER_MODULE_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-module/tracker-module.h> must be included directly."
#endif


#define TRACKER_TYPE_MODULE_ITERATABLE         (tracker_module_iteratable_get_type ())
#define TRACKER_MODULE_ITERATABLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MODULE_ITERATABLE, TrackerModuleIteratable))
#define TRACKER_IS_MODULE_ITERATABLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MODULE_ITERATABLE))
#define TRACKER_MODULE_ITERATABLE_GET_IFACE(i) (G_TYPE_INSTANCE_GET_INTERFACE ((i), TRACKER_TYPE_MODULE_ITERATABLE, TrackerModuleIteratableIface))

typedef struct TrackerModuleIteratable TrackerModuleIteratable; /* dummy typedef */
typedef struct TrackerModuleIteratableIface TrackerModuleIteratableIface;

struct TrackerModuleIteratableIface {
        GTypeInterface parent_iface;

        gboolean (* iter_contents) (TrackerModuleIteratable *iteratable);
        guint (* get_count) (TrackerModuleIteratable *iteratable);
};


GType tracker_module_iteratable_get_type (void) G_GNUC_CONST;

gboolean tracker_module_iteratable_iter_contents (TrackerModuleIteratable *iteratable);
guint    tracker_module_iteratable_get_count     (TrackerModuleIteratable *iteratable);


G_END_DECLS

#endif /* __TRACKER_MODULE_ITERATABLE_H__ */
