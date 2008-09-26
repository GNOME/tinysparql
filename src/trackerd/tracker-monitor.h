/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#ifndef __TRACKERD_MONITOR_H__
#define __TRACKERD_MONITOR_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-config.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_MONITOR		(tracker_monitor_get_type ())
#define TRACKER_MONITOR(object)		(G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_MONITOR, TrackerMonitor))
#define TRACKER_MONITOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_MONITOR, TrackerMonitorClass))
#define TRACKER_IS_MONITOR(object)	(G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_MONITOR))
#define TRACKER_IS_MONITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_MONITOR))
#define TRACKER_MONITOR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_MONITOR, TrackerMonitorClass))

typedef struct _TrackerMonitor	       TrackerMonitor;
typedef struct _TrackerMonitorClass    TrackerMonitorClass;
typedef struct _TrackerMonitorPrivate  TrackerMonitorPrivate;

struct _TrackerMonitor {
	GObject		       parent;
	TrackerMonitorPrivate *private;
};

struct _TrackerMonitorClass {
	GObjectClass	       parent;
};

GType		tracker_monitor_get_type	     (void);
TrackerMonitor *tracker_monitor_new		     (TrackerConfig  *config);
gboolean	tracker_monitor_get_enabled	     (TrackerMonitor *monitor);
void		tracker_monitor_set_enabled	     (TrackerMonitor *monitor,
						      gboolean	      enabled);
gboolean	tracker_monitor_add		     (TrackerMonitor *monitor,
						      const gchar    *module_name,
						      GFile	     *file);
gboolean	tracker_monitor_remove		     (TrackerMonitor *monitor,
						      const gchar    *module_name,
						      GFile	     *file);
gboolean	tracker_monitor_is_watched	     (TrackerMonitor *monitor,
						      const gchar    *module_name,
						      GFile	     *file);
gboolean	tracker_monitor_is_watched_by_string (TrackerMonitor *monitor,
						      const gchar    *module_name,
						      const gchar    *path);
guint		tracker_monitor_get_count	     (TrackerMonitor *monitor,
						      const gchar    *module_name);
guint		tracker_monitor_get_ignored	     (TrackerMonitor *monitor);

G_END_DECLS

#endif /* __TRACKERD_MONITOR_H__ */
