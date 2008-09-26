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
#include "tracker-module.h"
#include "tracker-metadata.h"

G_BEGIN_DECLS

GModule *		tracker_indexer_module_load		      (const gchar  *module_name);

void			tracker_indexer_module_init		      (GModule	    *module);
void			tracker_indexer_module_shutdown		      (GModule	    *module);

G_CONST_RETURN gchar *	tracker_indexer_module_get_name		      (GModule	    *module);

TrackerFile *		tracker_indexer_module_file_new		      (GModule	    *module,
								       const gchar  *path);
void			tracker_indexer_module_file_free	      (GModule	    *module,
								       TrackerFile  *file);

gboolean		tracker_indexer_module_file_get_uri	      (GModule	    *module,
								       TrackerFile  *file,
								       gchar	   **dirname,
								       gchar	   **basename);
gchar *			tracker_indexer_module_file_get_service_type  (GModule	    *module,
								       TrackerFile  *file);
TrackerMetadata *	tracker_indexer_module_file_get_metadata      (GModule	    *module,
								       TrackerFile  *file);
gchar *			tracker_indexer_module_file_get_text	      (GModule	    *module,
								       TrackerFile  *file);

gboolean		tracker_indexer_module_file_iter_contents     (GModule	    *module,
								       TrackerFile  *file);

G_END_DECLS

#endif /* __TRACKER_INDEXER_MODULE_H__ */
