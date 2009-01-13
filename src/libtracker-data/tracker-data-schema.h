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

#ifndef __TRACKER_DATA_SCHEMA_H__
#define __TRACKER_DATA_SCHEMA_H__

#include <glib.h>

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-field.h>
#include <libtracker-common/tracker-language.h>
#include <libtracker-common/tracker-ontology.h>

#include <libtracker-db/tracker-db-interface.h>
#include <libtracker-db/tracker-db-file-info.h>
#include <libtracker-db/tracker-db-index.h>

#include "tracker-field-data.h"

G_BEGIN_DECLS

GArray *            tracker_data_schema_create_service_array             (const gchar        *service,
									  gboolean            basic_services);

/* Metadata API */
gchar *             tracker_data_schema_metadata_field_get_related_names (TrackerDBInterface *iface,
									  const gchar        *name);
const gchar *       tracker_data_schema_metadata_field_get_db_table      (TrackerFieldType    type);

/* Miscellaneous API */
gchar *             tracker_data_schema_get_field_name                   (const gchar        *service,
									  const gchar        *meta_name);
TrackerFieldData *  tracker_data_schema_get_metadata_field               (TrackerDBInterface *iface,
									  const gchar        *service,
									  const gchar        *field_name,
									  gint                field_count,
									  gboolean            is_select,
									  gboolean            is_condition);

/* XESAM API */
TrackerDBResultSet *tracker_data_schema_xesam_get_metadata_names         (TrackerDBInterface *iface,
									  const char         *name);
TrackerDBResultSet *tracker_data_schema_xesam_get_text_metadata_names    (TrackerDBInterface *iface);
TrackerDBResultSet *tracker_data_schema_xesam_get_service_names          (TrackerDBInterface *iface,
									  const char         *name);

G_END_DECLS

#endif /* __TRACKER_DATA_SCHEMA_H__ */
