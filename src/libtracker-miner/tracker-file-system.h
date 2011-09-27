/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Carlos Garnacho  <carlos@lanedo.com>
 */

#ifndef __TRACKER_FILE_SYSTEM_H__
#define __TRACKER_FILE_SYSTEM_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_FILE_SYSTEM         (tracker_file_system_get_type())
#define TRACKER_FILE_SYSTEM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_FILE_SYSTEM, TrackerFileSystem))
#define TRACKER_FILE_SYSTEM_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_FILE_SYSTEM, TrackerFileSystemClass))
#define TRACKER_IS_FILE_SYSTEM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_FILE_SYSTEM))
#define TRACKER_IS_FILE_SYSTEM_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_FILE_SYSTEM))
#define TRACKER_FILE_SYSTEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_FILE_SYSTEM, TrackerFileSystemClass))

typedef struct _TrackerFileSystem TrackerFileSystem;
typedef struct _TrackerFileSystemClass TrackerFileSystemClass;
typedef struct _TrackerFile TrackerFile;

struct _TrackerFileSystem {
	GObject parent_instance;
	gpointer priv;
};

struct _TrackerFileSystemClass {
	GObjectClass parent_class;
};

GType      tracker_file_system_get_type      (void) G_GNUC_CONST;

TrackerFileSystem * tracker_file_system_new  (void);

/* GFile -> TrackerFile */
TrackerFile * tracker_file_system_get_file       (TrackerFileSystem  *file_system,
						  GFile              *file,
						  GFileType           file_type,
						  TrackerFile        *parent);
TrackerFile * tracker_file_system_ref_file       (TrackerFileSystem  *file_system,
                                                  TrackerFile        *file);
void          tracker_file_system_unref_file     (TrackerFileSystem  *file_system,
						  TrackerFile        *file);
TrackerFile * tracker_file_system_peek_file      (TrackerFileSystem  *file_system,
						  GFile              *file);

/* TrackerFile -> GFile */
GFile *       tracker_file_system_resolve_file   (TrackerFileSystem  *file_system,
                                                  TrackerFile        *file);

/* properties */
void      tracker_file_system_register_property (TrackerFileSystem *file_system,
                                                 GQuark             prop,
                                                 GDestroyNotify     destroy_notify);

void      tracker_file_system_set_property   (TrackerFileSystem  *file_system,
					      TrackerFile        *file,
                                              GQuark              prop,
                                              gpointer            prop_data);
gpointer  tracker_file_system_get_property   (TrackerFileSystem  *file_system,
                                              TrackerFile        *file,
                                              GQuark              prop);
void      tracker_file_system_unset_property (TrackerFileSystem  *file_system,
					      TrackerFile        *file,
                                              GQuark              prop);

G_END_DECLS

#endif /* __TRACKER_FILE_SYSTEM_H__ */
