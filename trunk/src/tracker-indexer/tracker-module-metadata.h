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

#ifndef __TRACKER_MODULE_METADATA_H__
#define __TRACKER_MODULE_METADATA_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#if !defined (__TRACKER_MODULE_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-module/tracker-module.h> must be included directly."
#endif


#define TRACKER_TYPE_MODULE_METADATA	     (tracker_module_metadata_get_type())
#define TRACKER_MODULE_METADATA(o)	     (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MODULE_METADATA, TrackerModuleMetadata))
#define TRACKER_MODULE_METADATA_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_MODULE_METADATA, TrackerModuleMetadataClass))
#define TRACKER_IS_MODULE_METADATA(o)	     (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MODULE_METADATA))
#define TRACKER_IS_MODULE_METADATA_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_MODULE_METADATA))
#define TRACKER_MODULE_METADATA_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MODULE_METADATA, TrackerModuleMetadataClass))

typedef struct TrackerModuleMetadata TrackerModuleMetadata;           /* dummy typedef */
typedef struct TrackerModuleMetadataClass TrackerModuleMetadataClass; /* dummy typedef */


GType                  tracker_module_metadata_get_type         (void) G_GNUC_CONST;

TrackerModuleMetadata *tracker_module_metadata_new              (void);

void                   tracker_module_metadata_clear_field      (TrackerModuleMetadata *metadata,
								 const gchar           *field_name);

gboolean               tracker_module_metadata_add_take_string  (TrackerModuleMetadata *metadata,
								 const gchar           *field_name,
								 gchar                 *value);
void                   tracker_module_metadata_add_string       (TrackerModuleMetadata *metadata,
								 const gchar           *field_name,
								 const gchar           *value);
void                   tracker_module_metadata_add_int          (TrackerModuleMetadata *metadata,
								 const gchar           *field_name,
								 gint                   value);
void                   tracker_module_metadata_add_uint         (TrackerModuleMetadata *metadata,
								 const gchar           *field_name,
								 guint                  value);
void                   tracker_module_metadata_add_double       (TrackerModuleMetadata *metadata,
								 const gchar           *field_name,
								 gdouble                value);
void                   tracker_module_metadata_add_float        (TrackerModuleMetadata *metadata,
								 const gchar           *field_name,
								 gfloat                 value);
void                   tracker_module_metadata_add_date         (TrackerModuleMetadata *metadata,
								 const gchar           *field_name,
								 time_t                 value);

G_END_DECLS

#endif /* __TRACKER_MODULE_METADATA_H__*/
