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

#ifndef __LIBTRACKER_DB_INTERFACE_SQLITE_H__
#define __LIBTRACKER_DB_INTERFACE_SQLITE_H__

#include "config.h"

#include "tracker-db-interface.h"

G_BEGIN_DECLS

#define TRACKER_COLLATION_NAME "TRACKER"
#define TRACKER_TITLE_COLLATION_NAME "TRACKER_TITLE"

typedef void (*TrackerDBWalCallback) (TrackerDBInterface *iface,
                                      gint                n_pages,
                                      gpointer            user_data);

typedef enum {
	TRACKER_DB_INTERFACE_READONLY  = 1 << 0,
	TRACKER_DB_INTERFACE_USE_MUTEX = 1 << 1,
	TRACKER_DB_INTERFACE_IN_MEMORY = 1 << 2,
} TrackerDBInterfaceFlags;

TrackerDBInterface *tracker_db_interface_sqlite_new                    (const gchar              *filename,
                                                                        const gchar              *shared_cache_key,
                                                                        TrackerDBInterfaceFlags   flags,
                                                                        GError                  **error);
gint64              tracker_db_interface_sqlite_get_last_insert_id     (TrackerDBInterface       *interface);
void                tracker_db_interface_sqlite_enable_shared_cache    (void);
gboolean            tracker_db_interface_sqlite_fts_init               (TrackerDBInterface       *interface,
                                                                        const gchar              *database,
                                                                        GHashTable               *properties,
                                                                        GHashTable               *multivalued,
                                                                        gboolean                  create,
                                                                        GError                  **error);
void                tracker_db_interface_sqlite_reset_collator         (TrackerDBInterface       *interface);
gboolean            tracker_db_interface_sqlite_wal_checkpoint         (TrackerDBInterface       *interface,
                                                                        gboolean                  blocking,
                                                                        GError                  **error);
gboolean            tracker_db_interface_init_vtabs                    (TrackerDBInterface       *interface,
                                                                        gpointer                  vtab_data);

gboolean            tracker_db_interface_sqlite_fts_delete_table       (TrackerDBInterface       *interface,
                                                                        const gchar              *database,
                                                                        GError                  **error);

gboolean            tracker_db_interface_sqlite_fts_alter_table        (TrackerDBInterface       *interface,
                                                                        const gchar              *database,
                                                                        GHashTable               *properties,
                                                                        GHashTable               *multivalued,
                                                                        GError                  **error);
gboolean            tracker_db_interface_sqlite_fts_update_text        (TrackerDBInterface       *db_interface,
                                                                        const gchar              *database,
                                                                        TrackerRowid              id,
                                                                        const gchar             **properties,
                                                                        GError                  **error);

gboolean            tracker_db_interface_sqlite_fts_delete_text        (TrackerDBInterface       *interface,
                                                                        const gchar              *database,
                                                                        TrackerRowid              rowid,
                                                                        const gchar             **properties,
                                                                        GError                  **error);
gboolean            tracker_db_interface_sqlite_fts_rebuild_tokens     (TrackerDBInterface       *interface,
                                                                        const gchar              *database,
                                                                        GError                  **error);

gboolean            tracker_db_interface_attach_database               (TrackerDBInterface       *db_interface,
                                                                        GFile                    *file,
                                                                        const gchar              *name,
                                                                        GError                  **error);
gboolean            tracker_db_interface_detach_database               (TrackerDBInterface       *db_interface,
                                                                        const gchar              *name,
                                                                        GError                  **error);
gssize              tracker_db_interface_sqlite_release_memory         (TrackerDBInterface       *db_interface);

void                tracker_db_interface_ref_use   (TrackerDBInterface *db_interface);
gboolean            tracker_db_interface_unref_use (TrackerDBInterface *db_interface);

GArray * tracker_db_statement_get_values (TrackerDBStatement   *stmt,
                                          TrackerPropertyType   type,
                                          GError              **error);

G_END_DECLS

#endif /* __LIBTRACKER_DB_INTERFACE_SQLITE_H__ */
