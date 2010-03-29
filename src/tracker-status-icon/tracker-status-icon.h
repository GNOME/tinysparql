/*
 * Copyright (C) 2009, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __TRACKER_STATUS_ICON_H__
#define __TRACKER_STATUS_ICON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_STATUS_ICON         (tracker_status_icon_get_type())
#define TRACKER_STATUS_ICON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_STATUS_ICON, TrackerStatusIcon))
#define TRACKER_STATUS_ICON_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_STATUS_ICON, TrackerStatusIconClass))
#define TRACKER_IS_STATUS_ICON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_STATUS_ICON))
#define TRACKER_IS_STATUS_ICON_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_STATUS_ICON))
#define TRACKER_STATUS_ICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_STATUS_ICON, TrackerStatusIconClass))

typedef struct TrackerStatusIcon TrackerStatusIcon;
typedef struct TrackerStatusIconClass TrackerStatusIconClass;

struct TrackerStatusIcon {
	GtkStatusIcon parent_object;
};

struct TrackerStatusIconClass {
	GtkStatusIconClass parent_class;
};

GType           tracker_status_icon_get_type (void) G_GNUC_CONST;

GtkStatusIcon * tracker_status_icon_new      (void);


G_END_DECLS

#endif /* __TRACKER_STATUS_ICON_H__ */
