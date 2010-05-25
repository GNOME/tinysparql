/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#ifndef __LIBTRACKER_DB_MANAGER_H__
#define __LIBTRACKER_DB_MANAGER_H__

#include <glib-object.h>

#include "tracker-db-interface.h"

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_DB_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-db/tracker-db.h> must be included directly."
#endif

#define TRACKER_TYPE_DB (tracker_db_get_type ())

typedef enum {
	TRACKER_DB_UNKNOWN,
	TRACKER_DB_METADATA,
	TRACKER_DB_CONTENTS,
	TRACKER_DB_FULLTEXT,
} TrackerDB;

typedef enum {
	TRACKER_DB_MANAGER_FORCE_REINDEX    = 1 << 1,
	TRACKER_DB_MANAGER_REMOVE_CACHE     = 1 << 2,
	/* 1 << 3 Was low mem mode */
	TRACKER_DB_MANAGER_REMOVE_ALL       = 1 << 4,
	TRACKER_DB_MANAGER_READONLY         = 1 << 5
} TrackerDBManagerFlags;

GType               tracker_db_get_type                       (void) G_GNUC_CONST;
gboolean            tracker_db_manager_init                   (TrackerDBManagerFlags  flags,
                                                               gboolean              *first_time,
                                                               gboolean               shared_cache);
void                tracker_db_manager_shutdown               (void);
void                tracker_db_manager_remove_all             (gboolean               rm_journal);
void                tracker_db_manager_optimize               (void);
const gchar *       tracker_db_manager_get_file               (TrackerDB              db);
TrackerDBInterface *tracker_db_manager_get_db_interface       (void);
void                tracker_db_manager_remove_temp            (void);
void                tracker_db_manager_move_to_temp           (void);
void                tracker_db_manager_restore_from_temp      (void);
void                tracker_db_manager_init_locations         (void);
gboolean            tracker_db_manager_has_enough_space       (void);

TrackerDBManagerFlags
                    tracker_db_manager_get_flags              (void);

gboolean            tracker_db_manager_interrupt_thread       (GThread *thread);
void                tracker_db_manager_interrupt_thread_reset (GThread *thread);

gboolean            tracker_db_manager_get_first_index_done (void);
void                tracker_db_manager_set_first_index_done (gboolean done);


G_END_DECLS

#endif /* __LIBTRACKER_DB_MANAGER_H__ */
