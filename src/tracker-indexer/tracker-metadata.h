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

#ifndef __TRACKER_METADATA_H__
#define __TRACKER_METADATA_H__

#include <glib.h>
#include <libtracker-common/tracker-field.h>

typedef struct TrackerMetadata TrackerMetadata;

typedef void (* TrackerMetadataForeach) (TrackerField *field,
					 gpointer      value,
					 gpointer      user_data);

TrackerMetadata *      tracker_metadata_new    (void);
void		       tracker_metadata_free   (TrackerMetadata *metadata);

void		       tracker_metadata_insert		       (TrackerMetadata *metadata,
								const gchar	*field_name,
								gchar		*value);
void		       tracker_metadata_insert_multiple_values (TrackerMetadata *metadata,
								const gchar	*field_name,
								GList		*list);

G_CONST_RETURN gchar * tracker_metadata_lookup		       (TrackerMetadata *metadata,
								const gchar	*field_name);
G_CONST_RETURN GList * tracker_metadata_lookup_multiple_values (TrackerMetadata *metadata,
								const gchar	*field_name);

void		       tracker_metadata_foreach (TrackerMetadata	*metadata,
						 TrackerMetadataForeach  func,
						 gpointer		 user_data);

G_END_DECLS

#endif /* __TRACKER_METADATA_H__*/
