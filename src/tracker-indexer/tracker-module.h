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

#ifndef __TRACKER_MODULE_H__
#define __TRACKER_MODULE_H__

G_BEGIN_DECLS

#include <glib.h>
#include <tracker-indexer/tracker-metadata.h>

typedef struct TrackerFile TrackerFile;

struct TrackerFile {
	gchar	 *path;
	gpointer  data;
};

typedef void		  (* TrackerModuleInit)			  (void);
typedef void		  (* TrackerModuleShutdown)		  (void);

typedef const gchar *	  (* TrackerModuleGetNameFunc)		  (void);
typedef gchar **	  (* TrackerModuleGetDirectoriesFunc)	  (void);

typedef gpointer	  (* TrackerModuleFileGetDataFunc)	  (const gchar	*path);
typedef void		  (* TrackerModuleFileFreeDataFunc)	  (gpointer	 data);

typedef gchar *		  (* TrackerModuleFileGetServiceTypeFunc) (TrackerFile	*file);
typedef void		  (* TrackerModuleFileGetUriFunc)	  (TrackerFile	*file,
								   gchar       **dirname,
								   gchar       **basename);

typedef TrackerMetadata * (* TrackerModuleFileGetMetadataFunc)	  (TrackerFile	*file);
typedef gchar *		  (* TrackerModuleFileGetText)		  (TrackerFile	*path);
typedef gboolean	  (* TrackerModuleFileIterContents)	  (TrackerFile	*path);


void		      tracker_module_init		   (void);
void		      tracker_module_shutdown		   (void);
G_CONST_RETURN gchar *tracker_module_get_name		   (void);
gpointer	      tracker_module_file_get_data	   (const gchar  *path);
void		      tracker_module_file_free_data	   (gpointer	  file_data);
gchar *		      tracker_module_file_get_service_type (TrackerFile  *file);
void		      tracker_module_file_get_uri	   (TrackerFile  *file,
							    gchar	**dirname,
							    gchar	**basename);
TrackerMetadata *     tracker_module_file_get_metadata	   (TrackerFile  *file);
gchar *		      tracker_module_file_get_text	   (TrackerFile  *file);
gboolean	      tracker_module_file_iter_contents    (TrackerFile  *file);

G_END_DECLS

#endif /* __TRACKER_MODULE_H__ */
