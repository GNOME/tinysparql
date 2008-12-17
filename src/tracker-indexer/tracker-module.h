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

#ifndef __TRACKER_MODULE_H__
#define __TRACKER_MODULE_H__

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define __TRACKER_MODULE_INSIDE__

#include "tracker-module-file.h"
#include "tracker-module-iteratable.h"
#include "tracker-module-metadata.h"
#include "tracker-module-metadata-utils.h"

void                indexer_module_initialize  (GTypeModule *module);
void                indexer_module_shutdown    (void);
TrackerModuleFile * indexer_module_create_file (GFile *file);


#undef __TRACKER_MODULE_INSIDE__

#endif /* __TRACKER_MODULE_H__ */
