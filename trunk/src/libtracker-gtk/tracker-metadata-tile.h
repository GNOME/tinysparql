/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2007 Neil Jagdish Patel <njpatel@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author : Neil Jagdish Patel <njpatel@gmail.com>
 */

#ifndef TRACKER_METADATA_TILE_H
#define TRACKER_METADATA_TILE_H

#include <gtk/gtk.h>
#include <tracker.h>


#define TRACKER_TYPE_METADATA_TILE		(tracker_metadata_tile_get_type ())
#define TRACKER_METADATA_TILE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_METADATA_TILE, TrackerMetadataTile))
#define TRACKER_METADATA_TILE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_METADATA_TILE, TrackerMetadataTileClass))
#define TRACKER_IS_METADATA_TILE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_METADATA_TILE))
#define TRACKER_IS_METADATA_TILE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_METADATA_TILE))
#define TRACKER_METADATA_TILE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_METADATA_TILE, TrackerMetadataTileClass))

typedef struct TrackerMetadataTilePrivate TrackerMetadataTilePrivate;

typedef struct TrackerMetadataTile {
	GtkEventBox parent;

} TrackerMetadataTile;

typedef struct {
	GtkEventBoxClass parent_class;

} TrackerMetadataTileClass;

GType	   tracker_metadata_tile_get_type  (void);

GtkWidget* tracker_metadata_tile_new	   (void);

void	   tracker_metadata_tile_set_uri (TrackerMetadataTile		*tile,
					  const gchar			*uri,
					  ServiceType			service_type,
					  const gchar			*type,
					  GdkPixbuf			*icon);

#endif /* TRACKER_METADATA_TILE_H */
