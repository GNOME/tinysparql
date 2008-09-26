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

#ifndef __TRACKERD_METADATA_H__
#define __TRACKERD_METADATA_H__

#include <glib-object.h>

#include <libtracker-db/tracker-db-index.h>

#define TRACKER_METADATA_SERVICE	 "org.freedesktop.Tracker"
#define TRACKER_METADATA_PATH		 "/org/freedesktop/Tracker/Metadata"
#define TRACKER_METADATA_INTERFACE	 "org.freedesktop.Tracker.Metadata"

G_BEGIN_DECLS

#define TRACKER_TYPE_METADATA		 (tracker_metadata_get_type ())
#define TRACKER_METADATA(object)	 (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_METADATA, TrackerMetadata))
#define TRACKER_METADATA_CLASS(klass)	 (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_METADATA, TrackerMetadataClass))
#define TRACKER_IS_METADATA(object)	 (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_METADATA))
#define TRACKER_IS_METADATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_METADATA))
#define TRACKER_METADATA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_METADATA, TrackerMetadataClass))

typedef struct TrackerMetadata	    TrackerMetadata;
typedef struct TrackerMetadataClass TrackerMetadataClass;

struct TrackerMetadata {
	GObject parent;
};

struct TrackerMetadataClass {
	GObjectClass parent;
};

GType		 tracker_metadata_get_type		 (void);
TrackerMetadata *tracker_metadata_new			 (void);
void		 tracker_metadata_get			 (TrackerMetadata	 *object,
							  const gchar		 *service_type,
							  const gchar		 *uri,
							  gchar			**keys,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_metadata_set			 (TrackerMetadata	 *object,
							  const gchar		 *service_type,
							  const gchar		 *uri,
							  gchar			**keys,
							  gchar			**metadata,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_metadata_get_type_details	 (TrackerMetadata	 *object,
							  const gchar		 *metadata,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_metadata_get_registered_types	 (TrackerMetadata	 *object,
							  const gchar		 *service_type,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_metadata_get_registered_classes (TrackerMetadata	 *object,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_metadata_get_unique_values	 (TrackerMetadata	 *object,
							  const gchar		 *service_type,
							  gchar			**fields,
							  const gchar		 *query_condition,
							  gboolean		  order_desc,
							  gint			  offset,
							  gint			  max_hits,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_metadata_get_sum		 (TrackerMetadata	 *object,
							  const gchar		 *service_type,
							  const gchar		 *field,
							  const gchar		 *query_condition,
							  DBusGMethodInvocation  *context,
							  GError		**error);
void		 tracker_metadata_get_count		 (TrackerMetadata	 *object,
							  const gchar		 *service_type,
							  const gchar		 *field,
							  const gchar		 *query_condition,
							  DBusGMethodInvocation  *context,
							  GError		**error);

void		 tracker_metadata_get_unique_values_with_count (TrackerMetadata        *object,
								const gchar	       *service_type,
								gchar		      **fields,
								const gchar	       *query_condition,
								const gchar	       *count,
								gboolean		order_desc,
								gint			offset,
								gint			max_hits,
								DBusGMethodInvocation  *context,
								GError		      **error);

G_END_DECLS

#endif /* __TRACKERD_METADATA_H__ */
