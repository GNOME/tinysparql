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

#ifndef __TRACKERD_DBUS_DAEMON_H__
#define __TRACKERD_DBUS_DAEMON_H__

#include <glib-object.h>

#include <libtracker-common/tracker-config.h>

#include "tracker-processor.h"

#define TRACKER_DAEMON_SERVICE	       "org.freedesktop.Tracker"
#define TRACKER_DAEMON_PATH	       "/org/freedesktop/Tracker"
#define TRACKER_DAEMON_INTERFACE       "org.freedesktop.Tracker"

G_BEGIN_DECLS

#define TRACKER_TYPE_DAEMON	       (tracker_daemon_get_type ())
#define TRACKER_DAEMON(object)	       (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_DAEMON, TrackerDaemon))
#define TRACKER_DAEMON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_DBUS_DAEMON, TrackerDaemonClass))
#define TRACKER_IS_DAEMON(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_DAEMON))
#define TRACKER_IS_DAEMON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_DAEMON))
#define TRACKER_DAEMON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_DAEMON, TrackerDaemonClass))

typedef struct TrackerDaemon	  TrackerDaemon;
typedef struct TrackerDaemonClass TrackerDaemonClass;

struct TrackerDaemon {
	GObject parent;
};

struct TrackerDaemonClass {
	GObjectClass parent;
};

GType	       tracker_daemon_get_type		   (void);
TrackerDaemon *tracker_daemon_new		   (TrackerConfig	  *config,
						    TrackerProcessor	  *processor);
void	       tracker_daemon_get_version	   (TrackerDaemon	  *object,
						    DBusGMethodInvocation *context,
						    GError **error);
void	       tracker_daemon_get_status	   (TrackerDaemon	  *object,
						    DBusGMethodInvocation *context,
						    GError **error);
void	       tracker_daemon_get_services	   (TrackerDaemon	  *object,
						    gboolean		   main_services_only,
						    DBusGMethodInvocation *context,
						    GError **error);
void	       tracker_daemon_get_stats		   (TrackerDaemon	  *object,
						    DBusGMethodInvocation *context,
						    GError **error);
void	       tracker_daemon_set_bool_option	   (TrackerDaemon	  *object,
						    const gchar		  *option,
						    gboolean		   value,
						    DBusGMethodInvocation *context,
						    GError **error);
void	       tracker_daemon_set_int_option	   (TrackerDaemon	  *object,
						    const gchar		  *option,
						    gint		   value,
						    DBusGMethodInvocation *context,
						    GError **error);
void	       tracker_daemon_shutdown		   (TrackerDaemon	  *object,
						    gboolean		   reindex,
						    DBusGMethodInvocation *context,
						    GError **error);
void	       tracker_daemon_prompt_index_signals (TrackerDaemon	  *object,
						    DBusGMethodInvocation *context,
						    GError **error);


G_END_DECLS

#endif /* __TRACKERD_DAEMON_H__ */
