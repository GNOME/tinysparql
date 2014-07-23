/*
 * Copyright (C) 2014 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __TRACKER_EXTRACT_PERSISTENCE_H__
#define __TRACKER_EXTRACT_PERSISTENCE_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_EXTRACT_PERSISTENCE         (tracker_extract_persistence_get_type ())
#define TRACKER_EXTRACT_PERSISTENCE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_EXTRACT_PERSISTENCE, TrackerExtractPersistence))
#define TRACKER_EXTRACT_PERSISTENCE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_EXTRACT_PERSISTENCE, TrackerExtractPersistenceClass))
#define TRACKER_IS_EXTRACT_PERSISTENCE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_EXTRACT_PERSISTENCE))
#define TRACKER_IS_EXTRACT_PERSISTENCE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TRACKER_TYPE_EXTRACT_PERSISTENCE))
#define TRACKER_EXTRACT_PERSISTENCE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_EXTRACT_PERSISTENCE, TrackerExtractPersistenceClass))

typedef struct _TrackerExtractPersistence TrackerExtractPersistence;
typedef struct _TrackerExtractPersistenceClass TrackerExtractPersistenceClass;

typedef void (* TrackerFileRecoveryFunc) (GFile    *file,
                                          gpointer  user_data);

struct _TrackerExtractPersistence
{
	GObject parent_instance;
};

struct _TrackerExtractPersistenceClass
{
	GObjectClass parent_class;
};

GType tracker_extract_persistence_get_type (void) G_GNUC_CONST;

TrackerExtractPersistence *
     tracker_extract_persistence_initialize (TrackerFileRecoveryFunc     retry_func,
                                             TrackerFileRecoveryFunc     ignore_func,
                                             gpointer                    user_data);

void tracker_extract_persistence_add_file    (TrackerExtractPersistence *persistence,
                                              GFile                     *file);
void tracker_extract_persistence_remove_file (TrackerExtractPersistence *persistence,
                                              GFile                     *file);

G_END_DECLS

#endif /* __TRACKER_EXTRACT_PERSISTENCE_H__ */
