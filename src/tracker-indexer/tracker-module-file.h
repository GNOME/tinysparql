/* Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef __TRACKER_MODULE_FILE_H__
#define __TRACKER_MODULE_FILE_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "tracker-module-metadata.h"

G_BEGIN_DECLS

#if !defined (__TRACKER_MODULE_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-module/tracker-module.h> must be included directly."
#endif


#define TRACKER_TYPE_MODULE_FILE         (tracker_module_file_get_type())
#define TRACKER_MODULE_FILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MODULE_FILE, TrackerModuleFile))
#define TRACKER_MODULE_FILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_MODULE_FILE, TrackerModuleFileClass))
#define TRACKER_IS_MODULE_FILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MODULE_FILE))
#define TRACKER_IS_MODULE_FILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_MODULE_FILE))
#define TRACKER_MODULE_FILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MODULE_FILE, TrackerModuleFileClass))


typedef struct TrackerModuleFile TrackerModuleFile;
typedef struct TrackerModuleFileClass TrackerModuleFileClass;
typedef enum TrackerModuleFlags TrackerModuleFlags;

enum TrackerModuleFlags {
        TRACKER_FILE_CONTENTS_STATIC = 1 << 0
};

struct TrackerModuleFile {
        GObject parent_instance;
};

struct TrackerModuleFileClass {
        GObjectClass parent_class;

        void (* initialize) (TrackerModuleFile *file);
        G_CONST_RETURN gchar * (* get_service_type) (TrackerModuleFile *file);
        gchar * (* get_uri) (TrackerModuleFile *file);
        gchar * (* get_text) (TrackerModuleFile *file);
        TrackerModuleMetadata * (* get_metadata) (TrackerModuleFile *file);
        TrackerModuleFlags (* get_flags) (TrackerModuleFile *file);
        void (* cancel) (TrackerModuleFile *file);
};

GType tracker_module_flags_get_type (void) G_GNUC_CONST;
GType tracker_module_file_get_type (void) G_GNUC_CONST;

GFile *                 tracker_module_file_get_file         (TrackerModuleFile *file);
G_CONST_RETURN gchar *  tracker_module_file_get_service_type (TrackerModuleFile *file);
gchar *                 tracker_module_file_get_uri          (TrackerModuleFile *file);
gchar *                 tracker_module_file_get_text         (TrackerModuleFile *file);
TrackerModuleMetadata * tracker_module_file_get_metadata     (TrackerModuleFile *file);
TrackerModuleFlags      tracker_module_file_get_flags        (TrackerModuleFile *file);

void                    tracker_module_file_cancel           (TrackerModuleFile *file);
gboolean                tracker_module_file_is_cancelled     (TrackerModuleFile *file);


G_END_DECLS

#endif /* __TRACKER_MODULE_FILE_H__ */
