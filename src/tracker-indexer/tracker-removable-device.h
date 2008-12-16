/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 *
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __TRACKER_REMOVABLE_DEVICE_H__
#define __TRACKER_REMOVABLE_DEVICE_H__

#include <libtracker-data/tracker-data-metadata.h>

#include "tracker-module-metadata.h"
#include "tracker-indexer.h"

G_BEGIN_DECLS

void    tracker_removable_device_load         (TrackerIndexer *indexer, 
					       const gchar *mount_point);
void    tracker_removable_device_optimize     (TrackerIndexer *indexer, 
					       const gchar *mount_point);
void    tracker_removable_device_add_metadata (TrackerIndexer *indexer, 
					       const gchar *mount_point,
					       const gchar *path,
					       const gchar *rdf_type,
					       TrackerModuleMetadata *metadata);
void    tracker_removable_device_add_removal  (TrackerIndexer *indexer, 
					       const gchar *mount_point, 
					       const gchar *path,
					       const gchar *rdf_type);
void    tracker_removable_device_add_move     (TrackerIndexer *indexer, 
					       const gchar *mount_point, 
					       const gchar *from_path, 
					       const gchar *to_path,
					       const gchar *rdf_type);

G_END_DECLS

#endif /* __TRACKER_REMOVABLE_DEVICE_H__ */



