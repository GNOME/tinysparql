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

#ifndef __TRACKER_SERVICE_MANAGER_H__
#define __TRACKER_SERVICE_MANAGER_H__

#include <glib-object.h>

#include "tracker-service.h"

G_BEGIN_DECLS

void            tracker_service_manager_init                         (void);
void            tracker_service_manager_term                         (void);
void            tracker_service_manager_add_service                  (TrackerService *service,
								      GSList         *mimes,
								      GSList         *mime_prefixes);
TrackerService *tracker_service_manager_get_service                  (const gchar    *service_str);
gchar *         tracker_service_manager_get_service_by_id            (gint            id);
gchar *         tracker_service_manager_get_service_type_for_mime    (const gchar    *mime);
gint            tracker_service_manager_get_id_for_service           (const gchar    *service_str);
gint            tracker_service_manager_get_id_for_parent_service    (const gchar    *service_str);
gchar *         tracker_service_manager_get_parent_service           (const gchar    *service_str);
gchar *         tracker_service_manager_get_parent_service_by_id     (gint            id);
gint            tracker_service_manager_get_parent_id_for_service_id (gint            id);
TrackerDBType   tracker_service_manager_get_db_for_service           (const gchar    *service_str);
gboolean        tracker_service_manager_is_service_embedded          (const gchar    *service_str);
gboolean        tracker_service_manager_is_valid_service             (const gchar    *service_str);
gboolean        tracker_service_manager_has_metadata                 (const gchar    *service_str);
gboolean        tracker_service_manager_has_thumbnails               (const gchar    *service_str);
gboolean        tracker_service_manager_has_text                     (const gchar    *service_str);
gint            tracker_service_manager_metadata_in_service          (const gchar    *service_str,
								      const gchar    *meta_name);
gboolean        tracker_service_manager_show_service_directories     (const gchar    *service_str);
gboolean        tracker_service_manager_show_service_files           (const gchar    *service_str);

G_END_DECLS

#endif /* __TRACKER_SERVICE_MANAGER_H__ */

