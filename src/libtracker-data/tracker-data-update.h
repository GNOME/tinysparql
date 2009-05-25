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

#ifndef __TRACKER_DATA_UPDATE_H__
#define __TRACKER_DATA_UPDATE_H__

#include <glib.h>

#include <libtracker-common/tracker-ontology.h>

#include <libtracker-db/tracker-db-interface.h>

#include "tracker-data-metadata.h"

G_BEGIN_DECLS

typedef struct TrackerDataUpdateMetadataContext TrackerDataUpdateMetadataContext;
typedef enum TrackerDataUpdateMetadataContextType TrackerDataUpdateMetadataContextType;

enum TrackerDataUpdateMetadataContextType {
	TRACKER_CONTEXT_TYPE_INSERT,
	TRACKER_CONTEXT_TYPE_UPDATE
};

struct TrackerDataUpdateMetadataContext {
	TrackerDataUpdateMetadataContextType type;
	TrackerService *service;
	guint id;
	GHashTable *data;
};

guint32  tracker_data_update_get_new_service_id         (TrackerDBInterface  *iface);

/* Services  */
gboolean tracker_data_update_create_service             (TrackerDataUpdateMetadataContext *context,
							 TrackerService      *service,
							 guint32              service_id,
							 const gchar         *udi,
							 const gchar         *dirname,
							 const gchar         *basename,
							 GHashTable          *metadata);
void     tracker_data_update_disable_service            (TrackerService      *service,
							 guint32              service_id);
void     tracker_data_update_delete_service             (TrackerService      *service,
							 guint32              service_id);
void     tracker_data_update_delete_service_recursively (TrackerService      *service,
							 const gchar         *service_path);
gboolean tracker_data_update_move_service               (TrackerService      *service,
							 const gchar         *from,
							 const gchar         *to);

/* Turtle importing */
void     tracker_data_update_replace_service            (const gchar         *udi,
							 const gchar         *path,
							 const gchar         *rdf_type,
							 GHashTable          *metadata);
void     tracker_data_update_delete_service_by_path     (const gchar         *path,
							 const gchar         *rdf_type);
void     tracker_data_update_delete_service_all         (const gchar *rdf_type);


/* Metadata */
void     tracker_data_update_set_metadata               (TrackerDataUpdateMetadataContext *context,
							 TrackerService      *service,
							 guint32              service_id,
							 TrackerField        *field,
							 const gchar         *value,
							 const gchar         *parsed_value);
void     tracker_data_update_delete_all_metadata        (TrackerService      *service,
							 guint32              service_id);
void     tracker_data_update_delete_metadata            (TrackerService      *service,
							 guint32              service_id,
							 TrackerField        *field,
							 const gchar         *value);

/* Contents */
void     tracker_data_update_set_content                (TrackerService      *service,
							 guint32              service_id,
							 const gchar         *text);
void     tracker_data_update_delete_content             (TrackerService      *service,
							 guint32              service_id);


/* Volume handling */
void tracker_data_update_enable_volume                  (const gchar         *udi,
                                                         const gchar         *mount_path);
void tracker_data_update_disable_volume                 (const gchar         *udi);
void tracker_data_update_disable_all_volumes            (void);
void tracker_data_update_reset_volume                   (guint32              volume_id);

/* Metadata context */
TrackerDataUpdateMetadataContext *
     tracker_data_update_metadata_context_new           (TrackerDataUpdateMetadataContextType  type,
							 TrackerService                       *service,
							 guint                                 id);
void tracker_data_update_metadata_context_add           (TrackerDataUpdateMetadataContext     *context,
							 const gchar                          *column,
							 const gchar                          *value,
							 const gchar                          *function);
void tracker_data_update_metadata_context_close         (TrackerDataUpdateMetadataContext     *context);
void tracker_data_update_metadata_context_free          (TrackerDataUpdateMetadataContext     *context);

G_END_DECLS

#endif /* __TRACKER_DATA_UPDATE_H__ */
