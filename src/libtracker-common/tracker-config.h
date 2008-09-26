/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Michal Pryc (Michal.Pryc@Sun.Com)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#ifndef __LIBTRACKER_COMMON_CONFIG_H__
#define __LIBTRACKER_COMMON_CONFIG_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_CONFIG	    (tracker_config_get_type ())
#define TRACKER_CONFIG(o)	    (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_CONFIG, TrackerConfig))
#define TRACKER_CONFIG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_CONFIG, TrackerConfigClass))
#define TRACKER_IS_CONFIG(o)	    (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_CONFIG))
#define TRACKER_IS_CONFIG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_CONFIG))
#define TRACKER_CONFIG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_CONFIG, TrackerConfigClass))

typedef struct _TrackerConfig	   TrackerConfig;
typedef struct _TrackerConfigClass TrackerConfigClass;

struct _TrackerConfig {
	GObject      parent;
};

struct _TrackerConfigClass {
	GObjectClass parent_class;
};

GType	       tracker_config_get_type				   (void) G_GNUC_CONST;

void	       tracker_config_load_file				   (void);

TrackerConfig *tracker_config_new				   (void);
gint	       tracker_config_get_verbosity			   (TrackerConfig *config);
gint	       tracker_config_get_initial_sleep			   (TrackerConfig *config);
gboolean       tracker_config_get_low_memory_mode		   (TrackerConfig *config);
gboolean       tracker_config_get_nfs_locking			   (TrackerConfig *config);
GSList *       tracker_config_get_watch_directory_roots		   (TrackerConfig *config);
GSList *       tracker_config_get_crawl_directory_roots		   (TrackerConfig *config);
GSList *       tracker_config_get_no_watch_directory_roots	   (TrackerConfig *config);
gboolean       tracker_config_get_enable_watches		   (TrackerConfig *config);
gint	       tracker_config_get_throttle			   (TrackerConfig *config);
gboolean       tracker_config_get_enable_indexing		   (TrackerConfig *config);
gboolean       tracker_config_get_enable_xesam			   (TrackerConfig *config);
gboolean       tracker_config_get_enable_content_indexing	   (TrackerConfig *config);
gboolean       tracker_config_get_enable_thumbnails		   (TrackerConfig *config);
GSList *       tracker_config_get_disabled_modules		   (TrackerConfig *config);
gboolean       tracker_config_get_fast_merges			   (TrackerConfig *config);
GSList *       tracker_config_get_no_index_file_types		   (TrackerConfig *config);
gint	       tracker_config_get_min_word_length		   (TrackerConfig *config);
gint	       tracker_config_get_max_word_length		   (TrackerConfig *config);
const gchar *  tracker_config_get_language			   (TrackerConfig *config);
gboolean       tracker_config_get_enable_stemmer		   (TrackerConfig *config);
gboolean       tracker_config_get_disable_indexing_on_battery	   (TrackerConfig *config);
gboolean       tracker_config_get_disable_indexing_on_battery_init (TrackerConfig *config);
gint	       tracker_config_get_low_disk_space_limit		   (TrackerConfig *config);
gboolean       tracker_config_get_index_removable_devices	   (TrackerConfig *config);
gboolean       tracker_config_get_index_mounted_directories	   (TrackerConfig *config);
gint	       tracker_config_get_max_text_to_index		   (TrackerConfig *config);
gint	       tracker_config_get_max_words_to_index		   (TrackerConfig *config);
gint	       tracker_config_get_max_bucket_count		   (TrackerConfig *config);
gint	       tracker_config_get_min_bucket_count		   (TrackerConfig *config);
void	       tracker_config_set_verbosity			   (TrackerConfig *config,
								    gint	   value);
void	       tracker_config_set_initial_sleep			   (TrackerConfig *config,
								    gint	   value);
void	       tracker_config_set_low_memory_mode		   (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_nfs_locking			   (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_enable_watches		   (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_throttle			   (TrackerConfig *config,
								    gint	   value);
void	       tracker_config_set_enable_indexing		   (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_enable_xesam			   (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_enable_content_indexing	   (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_enable_thumbnails		   (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_fast_merges			   (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_min_word_length		   (TrackerConfig *config,
								    gint	   value);
void	       tracker_config_set_max_word_length		   (TrackerConfig *config,
								    gint	   value);
void	       tracker_config_set_language			   (TrackerConfig *config,
								    const gchar   *value);
void	       tracker_config_set_enable_stemmer		   (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_disable_indexing_on_battery	   (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_disable_indexing_on_battery_init (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_low_disk_space_limit		   (TrackerConfig *config,
								    gint	   value);
void	       tracker_config_set_index_removable_devices	   (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_index_mounted_directories	   (TrackerConfig *config,
								    gboolean	   value);
void	       tracker_config_set_max_text_to_index		   (TrackerConfig *config,
								    gint	   value);
void	       tracker_config_set_max_words_to_index		   (TrackerConfig *config,
								    gint	   value);
void	       tracker_config_set_max_bucket_count		   (TrackerConfig *config,
								    gint	   value);
void	       tracker_config_set_min_bucket_count		   (TrackerConfig *config,
								    gint	   value);

/* Directory root APIs*/
void	       tracker_config_add_watch_directory_roots		   (TrackerConfig *config,
								    gchar * const *roots);
void	       tracker_config_add_crawl_directory_roots		   (TrackerConfig *config,
								    gchar * const *roots);
void	       tracker_config_add_no_watch_directory_roots	   (TrackerConfig *config,
								    gchar * const *roots);
void	       tracker_config_add_disabled_modules		   (TrackerConfig *config,
								    gchar * const *modules);
void	       tracker_config_remove_disabled_modules		   (TrackerConfig *config,
								    const gchar   *module);

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_CONFIG_H__ */

