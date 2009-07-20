/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Nokia
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

#ifndef __TRACKER_CONFIG_MANAGER_H__
#define __TRACKER_CONFIG_MANAGER_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_CONFIG_MANAGER	    (tracker_config_manager_get_type ())
#define TRACKER_CONFIG_MANAGER(o)	    (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_CONFIG_MANAGER, TrackerConfigManager))
#define TRACKER_CONFIG_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_CONFIG_MANAGER, TrackerConfigManagerClass))
#define TRACKER_IS_CONFIG_MANAGER(o)	    (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_CONFIG_MANAGER))
#define TRACKER_IS_CONFIG_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_CONFIG_MANAGER))
#define TRACKER_CONFIG_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_CONFIG_MANAGER, TrackerConfigManagerClass))

typedef struct _TrackerConfigManager	  TrackerConfigManager;
typedef struct _TrackerConfigManagerClass TrackerConfigManagerClass;

struct _TrackerConfigManager {
	GObject       parent;

	GFile	     *file;
	GFileMonitor *monitor;

	gboolean      file_exists;

	GKeyFile     *key_file;
};

struct _TrackerConfigManagerClass {
	GObjectClass parent_class;
};

GType	              tracker_config_manager_get_type              (void) G_GNUC_CONST;

TrackerConfigManager *tracker_config_manager_new                   (const gchar           *domain);
gboolean              tracker_config_manager_save                  (TrackerConfigManager  *config);

const gchar *         tracker_config_manager_get_property_blurb    (TrackerConfigManager  *config,
								    const gchar           *property);
gboolean              tracker_config_manager_boolean_default       (TrackerConfigManager  *config,
								    const gchar           *property);

gint                  tracker_config_manager_int_default           (TrackerConfigManager  *config,
								    const gchar           *property);

gboolean              tracker_config_manager_int_validate          (TrackerConfigManager  *config,
								    const gchar           *property,
								    gint                   value);

GSList *              tracker_config_manager_string_list_to_gslist (const gchar          **value,
								    gboolean               is_directory_list);

void                  tracker_config_manager_load_int              (TrackerConfigManager  *config,
								    const gchar           *property,
								    GKeyFile              *key_file,
								    const gchar           *group,
								    const gchar           *key);
void                  tracker_config_manager_load_boolean          (TrackerConfigManager  *config,
								    const gchar           *property,
								    GKeyFile              *key_file,
								    const gchar           *group,
								    const gchar           *key);
void                  tracker_config_manager_load_string           (TrackerConfigManager  *config,
								    const gchar           *property,
								    GKeyFile              *key_file,
								    const gchar           *group,
								    const gchar           *key);
void                  tracker_config_manager_load_string_list      (TrackerConfigManager  *config,
								    const gchar           *property,
								    GKeyFile              *key_file,
								    const gchar           *group,
								    const gchar           *key);

void                  tracker_config_manager_save_int              (TrackerConfigManager  *config,
								    const gchar           *property,
								    GKeyFile              *key_file,
								    const gchar           *group,
								    const gchar           *key);
void                  tracker_config_manager_save_boolean          (TrackerConfigManager  *config,
								    const gchar           *property,
								    GKeyFile              *key_file,
								    const gchar           *group,
								    const gchar           *key);
void                  tracker_config_manager_save_string           (TrackerConfigManager  *config,
								    const gchar           *property,
								    GKeyFile              *key_file,
								    const gchar           *group,
								    const gchar           *key);
void                  tracker_config_manager_save_string_list      (TrackerConfigManager  *config,
								    const gchar           *property,
								    GKeyFile              *key_file,
								    const gchar           *group,
								    const gchar           *key);
	
G_END_DECLS

#endif /* __TRACKER_CONFIG_MANAGER_H__ */
