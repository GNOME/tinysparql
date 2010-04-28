/*
 * Copyright (C) 2009%, Nokia <ivan.frade@nokia.com>
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

#define TRACKER_TYPE_CONFIG         (tracker_config_get_type ())
#define TRACKER_CONFIG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_CONFIG, TrackerConfig))
#define TRACKER_CONFIG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_CONFIG, TrackerConfigClass))
#define TRACKER_IS_CONFIG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_CONFIG))
#define TRACKER_IS_CONFIG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_CONFIG))
#define TRACKER_CONFIG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_CONFIG, TrackerConfigClass))

typedef struct TrackerConfig      TrackerConfig;
typedef struct TrackerConfigClass TrackerConfigClass;

struct TrackerConfig {
	TrackerConfigFile parent;
};

struct TrackerConfigClass {
	TrackerConfigFileClass parent_class;
};

GType          tracker_config_get_type                             (void) G_GNUC_CONST;

TrackerConfig *tracker_config_new                                  (void);
TrackerConfig *tracker_config_new_with_domain                      (const gchar *domain);

gboolean       tracker_config_save                                 (TrackerConfig *config);

gint           tracker_config_get_verbosity                        (TrackerConfig *config);
gint           tracker_config_get_initial_sleep                    (TrackerConfig *config);
gboolean       tracker_config_get_enable_monitors                  (TrackerConfig *config);
gint           tracker_config_get_scan_timeout                     (TrackerConfig *config);
gint           tracker_config_get_cache_timeout                    (TrackerConfig *config);
gint           tracker_config_get_throttle                         (TrackerConfig *config);
gboolean       tracker_config_get_index_on_battery                 (TrackerConfig *config);
gboolean       tracker_config_get_index_on_battery_first_time      (TrackerConfig *config);
gboolean       tracker_config_get_index_removable_devices          (TrackerConfig *config);
gboolean       tracker_config_get_index_optical_discs              (TrackerConfig *config);
gboolean       tracker_config_get_index_mounted_directories        (TrackerConfig *config);
gint           tracker_config_get_low_disk_space_limit             (TrackerConfig *config);
GSList *       tracker_config_get_index_recursive_directories      (TrackerConfig *config);
GSList *       tracker_config_get_index_single_directories         (TrackerConfig *config);
GSList *       tracker_config_get_index_recursive_directories_unfiltered (TrackerConfig *config);
GSList *       tracker_config_get_index_single_directories_unfiltered    (TrackerConfig *config);
GSList *       tracker_config_get_ignored_directories              (TrackerConfig *config);
GSList *       tracker_config_get_ignored_directories_with_content (TrackerConfig *config);
GSList *       tracker_config_get_ignored_files                    (TrackerConfig *config);
gint           tracker_config_get_crawling_interval                (TrackerConfig *config);

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
void           tracker_config_set_throttle                         (TrackerConfig *config,
                                                                    gint           value);
void           tracker_config_set_index_on_battery                 (TrackerConfig *config,
                                                                    gboolean       value);
void           tracker_config_set_index_on_battery_first_time      (TrackerConfig *config,
                                                                    gboolean       value);
void           tracker_config_set_index_removable_devices          (TrackerConfig *config,
                                                                    gboolean       value);
void           tracker_config_set_index_optical_discs              (TrackerConfig *config,
                                                                    gboolean       value);
void           tracker_config_set_index_mounted_directories        (TrackerConfig *config,
                                                                    gboolean       value);
void           tracker_config_set_low_disk_space_limit             (TrackerConfig *config,
                                                                    gint           value);
void           tracker_config_set_index_recursive_directories      (TrackerConfig *config,
                                                                    GSList        *files);
void           tracker_config_set_index_single_directories         (TrackerConfig *config,
                                                                    GSList        *files);
void           tracker_config_set_ignored_directories              (TrackerConfig *config,
                                                                    GSList        *files);
void           tracker_config_set_ignored_directories_with_content (TrackerConfig *config,
                                                                    GSList        *files);
void           tracker_config_set_ignored_files                    (TrackerConfig *config,
                                                                    GSList        *files);
void           tracker_config_set_crawling_interval                (TrackerConfig *config,
                                                                    gint           interval);

/*
 * Convenience functions:
 */

/* The _patterns() APIs return GPatternSpec pointers for basename
 * pattern matching.
 */
GSList *       tracker_config_get_ignored_directory_patterns        (TrackerConfig *config);
GSList *       tracker_config_get_ignored_file_patterns             (TrackerConfig *config);

/* The _paths() APIs return string pointers for full paths matching */
GSList *       tracker_config_get_ignored_directory_paths           (TrackerConfig *config);
GSList *       tracker_config_get_ignored_file_paths                (TrackerConfig *config);

G_END_DECLS

#endif /* __TRACKER_MINER_FS_CONFIG_H__ */
