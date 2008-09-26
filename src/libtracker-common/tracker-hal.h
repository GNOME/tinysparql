/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#ifndef __LIBTRACKER_HAL_H__
#define __LIBTRACKER_HAL_H__

#include <glib-object.h>

G_BEGIN_DECLS

#ifdef HAVE_HAL

#define TRACKER_TYPE_HAL	 (tracker_hal_get_type ())
#define TRACKER_HAL(o)		 (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_HAL, TrackerHal))
#define TRACKER_HAL_CLASS(k)	 (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_HAL, TrackerHalClass))
#define TRACKER_IS_HAL(o)	 (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_HAL))
#define TRACKER_IS_HAL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_HAL))
#define TRACKER_HAL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_HAL, TrackerHalClass))

typedef struct _TrackerHal	TrackerHal;
typedef struct _TrackerHalClass TrackerHalClass;

struct _TrackerHal {
	GObject      parent;
};

struct _TrackerHalClass {
	GObjectClass parent_class;
};

GType	    tracker_hal_get_type		    (void) G_GNUC_CONST;

TrackerHal *tracker_hal_new			    (void);
gboolean    tracker_hal_get_battery_in_use	    (TrackerHal *hal);
gboolean    tracker_hal_get_battery_exists	    (TrackerHal *hal);
GSList *    tracker_hal_get_mounted_directory_roots (TrackerHal *hal);
GSList *    tracker_hal_get_removable_device_roots  (TrackerHal *hal);

#else  /* HAVE_HAL */

typedef void TrackerHal;

#endif /* HAVE_HAL */

G_END_DECLS

#endif /* __LIBTRACKER_HAL_H__ */
