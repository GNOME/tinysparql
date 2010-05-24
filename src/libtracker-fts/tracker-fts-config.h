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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef __TRACKER_FTS_CONFIG_H__
#define __TRACKER_FTS_CONFIG_H__

#include <glib-object.h>

#include <libtracker-common/tracker-config-file.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_FTS_CONFIG                 (tracker_fts_config_get_type ())
#define TRACKER_FTS_CONFIG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_FTS_CONFIG, TrackerFTSConfig))
#define TRACKER_FTS_CONFIG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_FTS_CONFIG, TrackerFTSConfigClass))
#define TRACKER_IS_FTS_CONFIG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_FTS_CONFIG))
#define TRACKER_IS_FTS_CONFIG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_FTS_CONFIG))
#define TRACKER_FTS_CONFIG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_FTS_CONFIG, TrackerFTSConfigClass))

typedef struct TrackerFTSConfig              TrackerFTSConfig;
typedef struct TrackerFTSConfigClass TrackerFTSConfigClass;

struct TrackerFTSConfig {
	TrackerConfigFile parent;
};

struct TrackerFTSConfigClass {
	TrackerConfigFileClass parent_class;
};

GType             tracker_fts_config_get_type               (void) G_GNUC_CONST;

TrackerFTSConfig *tracker_fts_config_new                    (void);
gboolean          tracker_fts_config_save                   (TrackerFTSConfig *config);
gint              tracker_fts_config_get_min_word_length    (TrackerFTSConfig *config);
gint              tracker_fts_config_get_max_word_length    (TrackerFTSConfig *config);
gboolean          tracker_fts_config_get_enable_stemmer     (TrackerFTSConfig *config);
gboolean          tracker_fts_config_get_enable_unaccent    (TrackerFTSConfig *config);
gboolean          tracker_fts_config_get_ignore_numbers     (TrackerFTSConfig *config);
gboolean          tracker_fts_config_get_ignore_stop_words  (TrackerFTSConfig *config);
gint              tracker_fts_config_get_max_words_to_index (TrackerFTSConfig *config);
void              tracker_fts_config_set_min_word_length    (TrackerFTSConfig *config,
                                                             gint              value);
void              tracker_fts_config_set_max_word_length    (TrackerFTSConfig *config,
                                                             gint              value);
void              tracker_fts_config_set_enable_stemmer     (TrackerFTSConfig *config,
                                                             gboolean          value);
void              tracker_fts_config_set_enable_unaccent    (TrackerFTSConfig *config,
                                                             gboolean          value);
void              tracker_fts_config_set_ignore_numbers     (TrackerFTSConfig *config,
                                                             gboolean          value);
void              tracker_fts_config_set_ignore_stop_words  (TrackerFTSConfig *config,
                                                             gboolean          value);
void              tracker_fts_config_set_max_words_to_index (TrackerFTSConfig *config,
                                                             gint              value);

G_END_DECLS

#endif /* __TRACKER_FTS_CONFIG_H__ */

