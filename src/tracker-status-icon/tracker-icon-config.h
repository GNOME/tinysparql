/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __TRACKER_STATUS_ICON_CONFIG_H__
#define __TRACKER_STATUS_ICON_CONFIG_H__

#include <glib-object.h>

#include <libtracker-common/tracker-config-file.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_ICON_CONFIG         (tracker_icon_config_get_type ())
#define TRACKER_ICON_CONFIG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_ICON_CONFIG, TrackerIconConfig))
#define TRACKER_ICON_CONFIG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_ICON_CONFIG, TrackerIconConfigClass))
#define TRACKER_IS_ICON_CONFIG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_ICON_CONFIG))
#define TRACKER_IS_ICON_CONFIG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_ICON_CONFIG))
#define TRACKER_ICON_CONFIG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_ICON_CONFIG, TrackerIconConfigClass))

typedef struct TrackerIconConfig          TrackerIconConfig;
typedef struct TrackerIconConfigClass TrackerIconConfigClass;

typedef enum {
	TRACKER_SHOW_NEVER,
	TRACKER_SHOW_ACTIVE,
	TRACKER_SHOW_ALWAYS
} TrackerVisibility;

struct TrackerIconConfig {
	TrackerConfigFile parent;
};

struct TrackerIconConfigClass {
	TrackerConfigFileClass parent_class;
};

GType              tracker_icon_config_get_type                                    (void) G_GNUC_CONST;

TrackerIconConfig *tracker_icon_config_new                                  (void);
TrackerIconConfig *tracker_icon_config_new_with_domain                      (const gchar *domain);

gboolean           tracker_icon_config_save                                 (TrackerIconConfig *config);

TrackerVisibility  tracker_icon_config_get_visibility                       (TrackerIconConfig *config);
void               tracker_icon_config_set_visibility                       (TrackerIconConfig *config,
                                                                             TrackerVisibility  value);

G_END_DECLS

#endif /* __TRACKER_STATUS_ICON_CONFIG_H__ */
