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

#ifndef __TRACKERD_ONTOLOGY_H__
#define __TRACKERD_ONTOLOGY_H__

#include <glib-object.h>

#include "tracker-field.h"
#include "tracker-service.h"

G_BEGIN_DECLS

void		tracker_ontology_init				(void);
void		tracker_ontology_shutdown			(void);

/* Service mechanics */
void		tracker_ontology_service_add			(TrackerService *service,
								 GSList		*mimes,
								 GSList		*mime_prefixes);
TrackerService *tracker_ontology_get_service_by_name		(const gchar	*service_str);
gchar *		tracker_ontology_get_service_by_id		(gint		 id);
gchar *		tracker_ontology_get_service_by_mime		(const gchar	*mime);
gint		tracker_ontology_get_service_id_by_name		(const gchar	*service_str);
TrackerDBType	tracker_ontology_get_service_db_by_name		(const gchar	*service_str);
gchar *		tracker_ontology_get_service_parent		(const gchar	*service_str);
gchar *		tracker_ontology_get_service_parent_by_id	(gint		 id);
gint		tracker_ontology_get_service_parent_id_by_id	(gint		 id);
GSList *	tracker_ontology_get_service_names_registered	(void);
GSList *	tracker_ontology_get_field_names_registered	(const gchar	*service_str);

/* Service data */
gboolean	tracker_ontology_service_is_valid		(const gchar	*service_str);
gboolean	tracker_ontology_service_has_embedded		(const gchar	*service_str);
gboolean	tracker_ontology_service_has_metadata		(const gchar	*service_str);
gboolean	tracker_ontology_service_has_thumbnails		(const gchar	*service_str);
gboolean	tracker_ontology_service_has_text		(const gchar	*service_str);
gint		tracker_ontology_service_get_key_metadata	(const gchar	*service_str,
								 const gchar	*meta_name);
gboolean	tracker_ontology_service_get_show_directories	(const gchar	*service_str);
gboolean	tracker_ontology_service_get_show_files		(const gchar	*service_str);

/* Field mechanics */
void		tracker_ontology_field_add			(TrackerField	*field);
TrackerField *	tracker_ontology_get_field_by_name		(const gchar	*name);
TrackerField *	tracker_ontology_get_field_by_id		(gint		 id);
gchar *		tracker_ontology_get_field_name_by_service_name (TrackerField	*field,
								 const gchar	*service_str);

/* Field data */
gchar *		tracker_ontology_field_get_display_name		(TrackerField	*field);
const gchar *	tracker_ontology_field_get_id			(const gchar	*name);
gboolean	tracker_ontology_field_is_child_of		(const gchar	*child,
								 const gchar	*parent);

G_END_DECLS

#endif /* __TRACKERD_ONTOLOGY_H__ */

