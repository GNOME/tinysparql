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

#include <glib.h>
#include <string.h>
#include <time.h>
#include <libtracker-common/tracker-sparql-builder.h>
#include <libtracker-common/tracker-type-utils.h>
#include "tracker-module-metadata-private.h"

struct TrackerModuleMetadataClass {
	GObjectClass parent_class;
};


static void   tracker_module_metadata_finalize   (GObject *object);


G_DEFINE_TYPE (TrackerModuleMetadata, tracker_module_metadata, G_TYPE_OBJECT)

static void
tracker_module_metadata_class_init (TrackerModuleMetadataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_module_metadata_finalize;
}

static void
tracker_module_metadata_init (TrackerModuleMetadata *metadata)
{
	metadata->sparql = tracker_sparql_builder_new_update ();
	tracker_sparql_builder_insert_open (metadata->sparql);
}

static void
tracker_module_metadata_finalize (GObject *object)
{
	TrackerModuleMetadata *metadata;

	metadata = TRACKER_MODULE_METADATA (object);

	g_object_unref (metadata->sparql);

	G_OBJECT_CLASS (tracker_module_metadata_parent_class)->finalize (object);
}

gchar *
tracker_module_metadata_get_sparql (TrackerModuleMetadata        *metadata)
{
	if (!metadata->sparql_closed) {
		tracker_sparql_builder_insert_close (metadata->sparql);
		metadata->sparql_closed = TRUE;
	}

	return g_strdup (tracker_sparql_builder_get_result (metadata->sparql));
}

/**
 * tracker_module_metadata_new:
 *
 * Creates a new #TrackerModuleMetadata
 *
 * Returns: A newly created #TrackerModuleMetadata
 **/
TrackerModuleMetadata *
tracker_module_metadata_new (void)
{
	return g_object_new (TRACKER_TYPE_MODULE_METADATA, NULL);
}
