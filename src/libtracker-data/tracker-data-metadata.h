/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef __TRACKER_DATA_METADATA_H__
#define __TRACKER_DATA_METADATA_H__

#include <glib.h>

#include <libtracker-common/tracker-common.h>

#define TRACKER_TYPE_DATA_METADATA	   (tracker_data_metadata_get_type())
#define TRACKER_DATA_METADATA(o)	   (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DATA_METADATA, TrackerDataMetadata))
#define TRACKER_DATA_METADATA_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_DATA_METADATA, TrackerDataMetadataClass))
#define TRACKER_IS_DATA_METADATA(o)	   (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DATA_METADATA))
#define TRACKER_IS_DATA_METADATA_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_DATA_METADATA))
#define TRACKER_DATA_METADATA_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_DATA_METADATA, TrackerDataMetadataClass))

typedef struct TrackerDataMetadata TrackerDataMetadata;
typedef struct TrackerDataMetadataClass TrackerDataMetadataClass;

struct TrackerDataMetadata {
	GObject parent_instance;
};

struct TrackerDataMetadataClass {
	GObjectClass parent_class;
};

typedef void (* TrackerDataMetadataForeach) (TrackerField *field,
					     gpointer      value,
					     gpointer      user_data);
typedef gboolean (* TrackerDataMetadataRemove) (TrackerField *field,
						gpointer      value,
						gpointer      user_data);

GType                 tracker_data_metadata_get_type       (void) G_GNUC_CONST;

TrackerDataMetadata * tracker_data_metadata_new            (void);

void                  tracker_data_metadata_clear_field    (TrackerDataMetadata        *metadata,
							    const gchar                *field_name);
gboolean              tracker_data_metadata_insert_take_ownership
                                                           (TrackerDataMetadata        *metadata,
							    const gchar                *field_name,
							    gchar                      *value);
void                  tracker_data_metadata_insert         (TrackerDataMetadata        *metadata,
							    const gchar                *field_name,
							    const gchar                *value);
void                  tracker_data_metadata_insert_values  (TrackerDataMetadata        *metadata,
							    const gchar                *field_name,
							    const GList                *list);
G_CONST_RETURN gchar *tracker_data_metadata_lookup         (TrackerDataMetadata        *metadata,
							    const gchar                *field_name);
G_CONST_RETURN GList *tracker_data_metadata_lookup_values  (TrackerDataMetadata        *metadata,
							    const gchar                *field_name);
void                  tracker_data_metadata_foreach        (TrackerDataMetadata        *metadata,
							    TrackerDataMetadataForeach  func,
							    gpointer                    user_data);
void                  tracker_data_metadata_foreach_remove (TrackerDataMetadata        *metadata,
							    TrackerDataMetadataRemove   func,
							    gpointer                    user_data);

G_END_DECLS

#endif /* __TRACKER_DATA_METADATA_H__*/
