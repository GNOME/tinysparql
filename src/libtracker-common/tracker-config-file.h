/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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
 */

#ifndef __LIBTRACKER_CONFIG_FILE_H__
#define __LIBTRACKER_CONFIG_FILE_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

#define TRACKER_TYPE_CONFIG_FILE         (tracker_config_file_get_type ())
#define TRACKER_CONFIG_FILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_CONFIG_FILE, TrackerConfigFile))
#define TRACKER_CONFIG_FILE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_CONFIG_FILE, TrackerConfigFileClass))
#define TRACKER_IS_CONFIG_FILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_CONFIG_FILE))
#define TRACKER_IS_CONFIG_FILE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_CONFIG_FILE))
#define TRACKER_CONFIG_FILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_CONFIG_FILE, TrackerConfigFileClass))

typedef struct _TrackerConfigFile TrackerConfigFile;
typedef struct _TrackerConfigFileClass TrackerConfigFileClass;
typedef struct _TrackerConfigMigrationEntry TrackerConfigMigrationEntry;

struct _TrackerConfigFile {
	GObject parent;

	GFile *file;
	GFileMonitor *monitor;

	gboolean file_exists;

	GKeyFile *key_file;
};

struct _TrackerConfigFileClass {
	GObjectClass parent_class;

	void (* changed) (TrackerConfigFile *file);
};

struct _TrackerConfigMigrationEntry {
	GType type;
	const gchar *file_section;
	const gchar *file_key;
	const gchar *settings_key;
};

GType              tracker_config_file_get_type (void) G_GNUC_CONST;

TrackerConfigFile* tracker_config_file_new     (void);
gboolean           tracker_config_file_migrate (TrackerConfigFile           *config,
						GSettings                   *settings,
						TrackerConfigMigrationEntry *entries);

gboolean           tracker_config_file_save     (TrackerConfigFile *config);

G_END_DECLS

#endif /* __LIBTRACKER_CONFIG_FILE_H__ */
