/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009%, Nokia (urho.konttori@nokia.com)
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

#ifndef __TRACKER_MINER_FS_CONFIG_H__
#define __TRACKER_MINER_FS_CONFIG_H__

#include <glib-object.h>

#include <libtracker-common/tracker-config-file.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_CONFIG	    (tracker_config_get_type ())
#define TRACKER_CONFIG(o)	    (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_CONFIG, TrackerConfig))
#define TRACKER_CONFIG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_CONFIG, TrackerConfigClass))
#define TRACKER_IS_CONFIG(o)	    (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_CONFIG))
#define TRACKER_IS_CONFIG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_CONFIG))
#define TRACKER_CONFIG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_CONFIG, TrackerConfigClass))

typedef struct TrackerConfig	  TrackerConfig;
typedef struct TrackerConfigClass TrackerConfigClass;

struct TrackerConfig {
	TrackerConfigFile parent;
};

struct TrackerConfigClass {
	TrackerConfigFileClass parent_class;
};

GType	       tracker_config_get_type				   (void) G_GNUC_CONST;

TrackerConfig *tracker_config_new                                  (void);
gboolean       tracker_config_save                                 (TrackerConfig *config);

gint           tracker_config_get_verbosity                        (TrackerConfig *config);
gint           tracker_config_get_initial_sleep                    (TrackerConfig *config);
gboolean       tracker_config_get_enable_monitors                  (TrackerConfig *config);
GSList *       tracker_config_get_monitor_directories              (TrackerConfig *config);
GSList *       tracker_config_get_monitor_directories_ignored      (TrackerConfig *config);
GSList *       tracker_config_get_monitor_recurse_directories      (TrackerConfig *config);
gint           tracker_config_get_scan_timeout                     (TrackerConfig *config);
gint           tracker_config_get_cache_timeout                    (TrackerConfig *config);
gint           tracker_config_get_throttle                         (TrackerConfig *config);
gboolean       tracker_config_get_enable_thumbnails                (TrackerConfig *config);
gboolean       tracker_config_get_disable_indexing_on_battery      (TrackerConfig *config);
gboolean       tracker_config_get_disable_indexing_on_battery_init (TrackerConfig *config);
gint           tracker_config_get_low_disk_space_limit             (TrackerConfig *config);
gboolean       tracker_config_get_index_removable_devices          (TrackerConfig *config);
gboolean       tracker_config_get_index_mounted_directories        (TrackerConfig *config);

void           tracker_config_set_verbosity                        (TrackerConfig *config,
								    gint           value);
void           tracker_config_set_initial_sleep                    (TrackerConfig *config,
								    gint           value);
void           tracker_config_set_enable_monitors                  (TrackerConfig *config,
								    gboolean       value);
void           tracker_config_set_scan_timeout                     (TrackerConfig *config,
								    gint           value);
void           tracker_config_set_cache_timeout                    (TrackerConfig *config,
								    gint           value);
void	       tracker_config_set_monitor_directories              (TrackerConfig *config,
								    GSList        *roots);
void	       tracker_config_set_monitor_directories_ignored      (TrackerConfig *config,
								    GSList        *roots);
void	       tracker_config_set_monitor_recurse_directories      (TrackerConfig *config,
								    GSList        *roots);
void           tracker_config_set_throttle                         (TrackerConfig *config,
								    gint           value);
void           tracker_config_set_enable_thumbnails                (TrackerConfig *config,
								    gboolean       value);
void           tracker_config_set_disable_indexing_on_battery      (TrackerConfig *config,
								    gboolean       value);
void           tracker_config_set_disable_indexing_on_battery_init (TrackerConfig *config,
								    gboolean       value);
void           tracker_config_set_low_disk_space_limit             (TrackerConfig *config,
								    gint           value);
void	       tracker_config_set_index_directories                (TrackerConfig *config,
								    GSList        *files);
void	       tracker_config_set_ignored_directories              (TrackerConfig *config,
								    GSList        *files);
void	       tracker_config_set_ignored_directories_with_content (TrackerConfig *config,
								    GSList        *files);
void	       tracker_config_set_ignored_files                    (TrackerConfig *config,
								    GSList        *files);
void           tracker_config_set_index_removable_devices          (TrackerConfig *config,
								    gboolean       value);
void           tracker_config_set_index_mounted_directories        (TrackerConfig *config,
								    gboolean       value);

G_END_DECLS

#endif /* __TRACKER_MINER_FS_CONFIG_H__ */
