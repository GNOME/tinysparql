/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 * Authors: Philip Van Hoof (pvanhoof@gnome.org)
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

#ifndef __TRACKERD_XESAM_H__
#define __TRACKERD_XESAM_H__

#include <glib-object.h>

#include <dbus/dbus-glib-bindings.h>

#include <libtracker-db/tracker-db-index.h>

#include "tracker-db.h"

#define TRACKER_XESAM_SERVICE	      "org.freedesktop.xesam.searcher"
#define TRACKER_XESAM_PATH	      "/org/freedesktop/xesam/searcher/main"
#define TRACKER_XESAM_INTERFACE       "org.freedesktop.xesam.Search"

G_BEGIN_DECLS

#define TRACKER_TYPE_XESAM	      (tracker_xesam_get_type ())
#define TRACKER_XESAM(object)	      (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_XESAM, TrackerXesam))
#define TRACKER_XESAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_XESAM, TrackerXesamClass))
#define TRACKER_IS_XESAM(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_XESAM))
#define TRACKER_IS_XESAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_XESAM))
#define TRACKER_XESAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_XESAM, TrackerXesamClass))

typedef struct TrackerXesam	 TrackerXesam;
typedef struct TrackerXesamClass TrackerXesamClass;

struct TrackerXesam {
	GObject parent;
};

struct TrackerXesamClass {
	GObjectClass parent;
};

GType	      tracker_xesam_get_type	       (void);
TrackerXesam *tracker_xesam_new		       (void);
void	      tracker_xesam_new_session        (TrackerXesam	      *object,
						DBusGMethodInvocation *context);
void	      tracker_xesam_set_property       (TrackerXesam	      *object,
						const gchar	      *session_id,
						const gchar	      *prop,
						GValue		      *val,
						DBusGMethodInvocation *context);
void	      tracker_xesam_get_property       (TrackerXesam	      *object,
						const gchar	      *session_id,
						const gchar	      *prop,
						DBusGMethodInvocation *context);
void	      tracker_xesam_close_session      (TrackerXesam	      *object,
						const gchar	      *session_id,
						DBusGMethodInvocation *context);
void	      tracker_xesam_new_search	       (TrackerXesam	      *object,
						const gchar	      *session_id,
						const gchar	      *query_xml,
						DBusGMethodInvocation *context);
void	      tracker_xesam_start_search       (TrackerXesam	      *object,
						const gchar	      *search_id,
						DBusGMethodInvocation *context);
void	      tracker_xesam_get_hit_count      (TrackerXesam	      *object,
						const gchar	      *search_id,
						DBusGMethodInvocation *context);
void	      tracker_xesam_get_hits	       (TrackerXesam	      *object,
						const gchar	      *search_id,
						guint		       count,
						DBusGMethodInvocation *context);
void	      tracker_xesam_get_range_hits     (TrackerXesam	      *object,
						const gchar	      *search_id,
						guint		       a,
						guint		       b,
						DBusGMethodInvocation *context);
void	      tracker_xesam_get_hit_data       (TrackerXesam	      *object,
						const gchar	      *search_id,
						GArray		      *hit_ids,
						GStrv		       fields,
						DBusGMethodInvocation *context);
void	      tracker_xesam_get_range_hit_data (TrackerXesam	      *object,
						const gchar	      *search_id,
						guint		       a,
						guint		       b,
						GStrv		       fields,
						DBusGMethodInvocation *context);
void	      tracker_xesam_close_search       (TrackerXesam	      *object,
						const gchar	      *search_id,
						DBusGMethodInvocation *context);
void	      tracker_xesam_get_state	       (TrackerXesam	      *object,
						DBusGMethodInvocation *context);
void	      tracker_xesam_emit_state_changed (TrackerXesam	      *self,
						GStrv		       state_info);
void	      tracker_xesam_name_owner_changed (DBusGProxy	      *proxy,
						const char	      *name,
						const char	      *prev_owner,
						const char	      *new_owner,
						TrackerXesam	      *self);

G_END_DECLS

#endif /* __TRACKERD_XESAM_H__ */
