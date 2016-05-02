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

struct _TrackerFileSystem {
	GObject parent_instance;
	gpointer priv;
};

struct _TrackerFileSystemClass {
	GObjectClass parent_class;
};

typedef gboolean (* TrackerFileSystemTraverseFunc) (GFile    *file,
                                                    gpointer  user_data);

GType      tracker_file_system_get_type      (void) G_GNUC_CONST;

TrackerFileSystem *
              tracker_file_system_new            (GFile              *root);

GFile *       tracker_file_system_get_file       (TrackerFileSystem  *file_system,
                                                  GFile              *file,
                                                  GFileType           file_type,
                                                  GFile              *parent);
GFile *       tracker_file_system_peek_file      (TrackerFileSystem  *file_system,
                                                  GFile              *file);
GFile *       tracker_file_system_peek_parent    (TrackerFileSystem  *file_system,
                                                  GFile              *file);

void          tracker_file_system_traverse       (TrackerFileSystem             *file_system,
                                                  GFile                         *root,
                                                  GTraverseType                  order,
                                                  TrackerFileSystemTraverseFunc  func,
                                                  gint                           max_depth,
                                                  gpointer                       user_data);

void          tracker_file_system_forget_files   (TrackerFileSystem *file_system,
						  GFile             *root,
						  GFileType          file_type);

GFileType     tracker_file_system_get_file_type  (TrackerFileSystem  *file_system,
                                                  GFile              *file);
/* properties */
void      tracker_file_system_register_property (GQuark             prop,
                                                 GDestroyNotify     destroy_notify);

void      tracker_file_system_set_property   (TrackerFileSystem  *file_system,
                                              GFile              *file,
                                              GQuark              prop,
                                              gpointer            prop_data);
gpointer  tracker_file_system_get_property   (TrackerFileSystem  *file_system,
                                              GFile              *file,
                                              GQuark              prop);
void      tracker_file_system_unset_property (TrackerFileSystem  *file_system,
                                              GFile              *file,
                                              GQuark              prop);
gpointer  tracker_file_system_steal_property (TrackerFileSystem *file_system,
                                              GFile             *file,
                                              GQuark             prop);

gboolean  tracker_file_system_get_property_full (TrackerFileSystem *file_system,
                                                 GFile             *file,
                                                 GQuark             prop,
                                                 gpointer          *data);

G_END_DECLS

#endif /* __TRACKER_FILE_SYSTEM_H__ */
