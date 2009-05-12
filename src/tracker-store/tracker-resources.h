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

#ifndef __TRACKERD_RESOURCES_H__
#define __TRACKERD_RESOURCES_H__

#include <glib-object.h>

#define TRACKER_RESOURCES_SERVICE	 "org.freedesktop.Tracker"
#define TRACKER_RESOURCES_PATH		 "/org/freedesktop/Tracker/Resources"
#define TRACKER_RESOURCES_INTERFACE	 "org.freedesktop.Tracker.Resources"

G_BEGIN_DECLS

#define TRACKER_TYPE_RESOURCES		 (tracker_resources_get_type ())
#define TRACKER_RESOURCES(object)	 (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_RESOURCES, TrackerResources))
#define TRACKER_RESOURCES_CLASS(klass)	 (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_RESOURCES, TrackerResourcesClass))
#define TRACKER_IS_RESOURCES(object)	 (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_RESOURCES))
#define TRACKER_IS_RESOURCES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_RESOURCES))
#define TRACKER_RESOURCES_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_RESOURCES, TrackerResourcesClass))

typedef struct TrackerResources	    TrackerResources;
typedef struct TrackerResourcesClass TrackerResourcesClass;

struct TrackerResources {
	GObject parent;
};

struct TrackerResourcesClass {
	GObjectClass parent;
};

GType		 tracker_resources_get_type		 (void);
TrackerResources *tracker_resources_new			 (void);

void		 tracker_resources_prepare	 (TrackerResources       *object,
						  GSList                 *event_sources);

/* DBus methods */
void		 tracker_resources_insert		 (TrackerResources	 *self,
							  const gchar		 *subject,
							  const gchar		 *predicate,
							  const gchar		 *object,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_resources_delete		 (TrackerResources	 *self,
							  const gchar		 *subject,
							  const gchar		 *predicate,
							  const gchar		 *object,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_resources_load			 (TrackerResources	 *object,
							  const gchar		 *uri,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_resources_sparql_query		 (TrackerResources       *object,
							  const gchar		 *query,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_resources_sparql_update	 (TrackerResources       *object,
							  const gchar		 *update,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_resources_batch_sparql_update	 (TrackerResources       *object,
							  const gchar		 *update,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_resources_batch_commit		 (TrackerResources       *object,
							  DBusGMethodInvocation  *context,
							  GError		**error);


G_END_DECLS

#endif /* __TRACKERD_RESOURCES_H__ */
