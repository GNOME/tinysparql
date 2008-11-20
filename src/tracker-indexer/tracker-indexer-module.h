/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef __TRACKER_INDEXER_MODULE_H__
#define __TRACKER_INDEXER_MODULE_H__

#include <glib.h>
#include "tracker-module-file.h"
#include "tracker-module-iteratable.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_INDEXER_MODULE    (tracker_indexer_module_get_type ())
#define TRACKER_INDEXER_MODULE(module) (G_TYPE_CHECK_INSTANCE_CAST ((module), TRACKER_TYPE_INDEXER_MODULE, TrackerIndexerModule))

typedef struct TrackerIndexerModule TrackerIndexerModule;
typedef struct TrackerIndexerModuleClass TrackerIndexerModuleClass;

struct TrackerIndexerModule {
	GTypeModule parent_instance;

	GModule *module;
	gchar *name;

	void (* initialize) (GTypeModule *module);
	void (* shutdown) (void);
	TrackerModuleFile * (* create_file) (GFile *file);
};

struct TrackerIndexerModuleClass {
	GTypeModuleClass parent_class;
};


GType                   tracker_indexer_module_get_type               (void) G_GNUC_CONST;

TrackerIndexerModule *  tracker_indexer_module_get                    (const gchar          *name);
TrackerModuleFile *     tracker_indexer_module_create_file            (TrackerIndexerModule *module,
								       GFile                *file);


G_END_DECLS

#endif /* __TRACKER_INDEXER_MODULE_H__ */
