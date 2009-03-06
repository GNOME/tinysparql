/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
 * Copyright (C) 2007, Creative Commons (http://creativecommons.org)
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

#ifndef __TRACKER_DATA_QUERY_H__
#define __TRACKER_DATA_QUERY_H__

#include <glib.h>

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-field.h>
#include <libtracker-common/tracker-language.h>
#include <libtracker-common/tracker-ontology.h>

#include <libtracker-db/tracker-db-interface.h>
#include <libtracker-db/tracker-db-file-info.h>
#include <libtracker-db/tracker-db-index.h>

#include "tracker-data-metadata.h"
#include "tracker-field-data.h"

G_BEGIN_DECLS

/* Metadata API */
TrackerDBResultSet * tracker_data_query_metadata_field        (TrackerDBInterface  *iface,
							       const gchar         *service_id,
							       const gchar         *field);
GPtrArray *          tracker_data_query_all_metadata          (const gchar         *service_type,
							       const gchar         *service_id);
TrackerDBResultSet * tracker_data_query_metadata_fields       (TrackerDBInterface  *iface,
							       const gchar         *service_type,
							       const gchar         *service_id,
							       gchar              **fields);
TrackerDataMetadata *tracker_data_query_metadata              (TrackerService      *service,
							       guint32              service_id,
							       gboolean             embedded);
TrackerDBResultSet  *tracker_data_query_backup_metadata       (TrackerService      *service);
gchar *              tracker_data_query_parsed_metadata       (TrackerService      *service,
							       guint32              service_id);
gchar *              tracker_data_query_unparsed_metadata     (TrackerService      *service,
							       guint32              service_id);
gchar **             tracker_data_query_metadata_field_values (TrackerService      *service_def,
							       guint32              service_id,
							       TrackerField        *field_def);

/* Using path */
gboolean             tracker_data_query_service_exists        (TrackerService      *service,
							       const gchar         *dirname,
							       const gchar         *basename,
							       guint32             *service_id,
							       time_t              *mtime);
guint                tracker_data_query_service_type_id       (const gchar         *dirname,
							       const gchar         *basename);
GHashTable *         tracker_data_query_service_children      (TrackerService      *service,
							       const gchar         *dirname);

/* Deleted files */
gboolean             tracker_data_query_first_removed_service (TrackerDBInterface  *iface,
							       guint32             *service_id);


/* Service API */
G_CONST_RETURN gchar *
                     tracker_data_query_service_type_by_id    (TrackerDBInterface  *iface,
							       guint32              service_id);

/* Files API */
guint32              tracker_data_query_file_id               (const gchar         *service_type,
							       const gchar         *path);
gchar *              tracker_data_query_file_id_as_string     (const gchar         *service_type,
							       const gchar         *path);
gchar *              tracker_data_query_content               (TrackerService      *service,
							       guint32              service_id);

G_END_DECLS

#endif /* __TRACKER_DATA_QUERY_H__ */
