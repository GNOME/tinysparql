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

#ifndef __TRACKER_MODULE_METADATA_PRIVATE_H__
#define __TRACKER_MODULE_METADATA_PRIVATE_H__

#include <glib.h>
#include <libtracker-common/tracker-ontology.h>
#include "tracker-module-metadata.h"

G_BEGIN_DECLS

typedef void (* TrackerModuleMetadataForeach) (TrackerField *field,
					       gpointer      value,
					       gpointer      user_data);
typedef gboolean (* TrackerModuleMetadataRemove) (TrackerField *field,
						  gpointer      value,
						  gpointer      user_data);

gconstpointer          tracker_module_metadata_lookup         (TrackerModuleMetadata        *metadata,
							       const gchar                  *field_name,
							       gboolean                     *multiple_values);

void		       tracker_module_metadata_foreach        (TrackerModuleMetadata        *metadata,
							       TrackerModuleMetadataForeach  func,
							       gpointer	                     user_data);

void		       tracker_module_metadata_foreach_remove (TrackerModuleMetadata        *metadata,
							       TrackerModuleMetadataRemove   func,
							       gpointer	                     user_data);

GHashTable *           tracker_module_metadata_get_hash_table (TrackerModuleMetadata        *metadata);


G_END_DECLS

#endif /* __TRACKER_MODULE_METADATA_PRIVATE_H__ */
