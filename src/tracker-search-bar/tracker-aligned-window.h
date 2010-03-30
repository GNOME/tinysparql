/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#ifndef __TRACKER_ALIGNED_WINDOW_H__
#define __TRACKER_ALIGNED_WINDOW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_ALIGNED_WINDOW             (tracker_aligned_window_get_type ())
#define TRACKER_ALIGNED_WINDOW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_ALIGNED_WINDOW, TrackerAlignedWindow))
#define TRACKER_IS_ALIGNED_WINDOW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_ALIGNED_WINDOW))
#define TRACKER_ALIGNED_WINDOW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_ALIGNED_WINDOW, TrackerAlignedWindowClass))
#define TRACKER_IS_ALIGNED_WINDOW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_ALIGNED_WINDOW))
#define TRACKER_ALIGNED_WINDOW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_ALIGNED_WINDOW, TrackerAlignedWindowClass))

typedef struct _TrackerAlignedWindow        TrackerAlignedWindow;
typedef struct _TrackerAlignedWindowClass   TrackerAlignedWindowClass;
typedef struct _TrackerAlignedWindowPrivate TrackerAlignedWindowPrivate;

struct _TrackerAlignedWindow {
	GtkWindow parent_instance;
 
	TrackerAlignedWindowPrivate *private;
};

struct _TrackerAlignedWindowClass {
	GtkWindowClass parent_class;
};

GType      tracker_aligned_window_get_type   (void) G_GNUC_CONST;
GtkWidget *tracker_aligned_window_new        (GtkWidget            *align_widget);
void       tracker_aligned_window_set_widget (TrackerAlignedWindow *aligned_window,
                                              GtkWidget            *align_widget);
GtkWidget *tracker_aligned_window_get_widget (TrackerAlignedWindow *aligned_window);

G_END_DECLS

#endif /* __TRACKER_ALIGNED_WINDOW_H__ */
