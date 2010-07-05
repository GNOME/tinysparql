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
 *
 * Authors:
 * Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __TRACKER_DB_CONFIG_H__
#define __TRACKER_DB_CONFIG_H__

#if !defined (__LIBTRACKER_DB_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-db/tracker-db.h> must be included directly."
#endif

#include <glib-object.h>

#include <libtracker-common/tracker-config-file.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_DB_CONFIG         (tracker_db_config_get_type ())
#define TRACKER_DB_CONFIG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DB_CONFIG, TrackerDBConfig))
#define TRACKER_DB_CONFIG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_DB_CONFIG, TrackerDBConfigClass))
#define TRACKER_IS_DB_CONFIG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DB_CONFIG))
#define TRACKER_IS_DB_CONFIG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_DB_CONFIG))
#define TRACKER_DB_CONFIG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_DB_CONFIG, TrackerDBConfigClass))

typedef struct TrackerDBConfig              TrackerDBConfig;
typedef struct TrackerDBConfigClass TrackerDBConfigClass;

struct TrackerDBConfig {
	TrackerConfigFile parent;
};

struct TrackerDBConfigClass {
	TrackerConfigFileClass parent_class;
};

GType            tracker_db_config_get_type                       (void) G_GNUC_CONST;

TrackerDBConfig *tracker_db_config_new                            (void);
gboolean         tracker_db_config_save                           (TrackerDBConfig *config);

gint             tracker_db_config_get_journal_chunk_size         (TrackerDBConfig *config);
const gchar *    tracker_db_config_get_journal_rotate_destination (TrackerDBConfig *config);

void             tracker_db_config_set_journal_chunk_size         (TrackerDBConfig *config,
                                                                   gint             value);
void             tracker_db_config_set_journal_rotate_destination (TrackerDBConfig *config,
                                                                   const gchar     *value);

G_END_DECLS

#endif /* __TRACKER_DB_CONFIG_H__ */

