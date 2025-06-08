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

#pragma once

#include "tracker-db-interface.h"
#include "tracker-db-manager.h"

G_BEGIN_DECLS

#define TRACKER_COLLATION_NAME "TRACKER"
#define TRACKER_TITLE_COLLATION_NAME "TRACKER_TITLE"

typedef void (*TrackerDBWalCallback) (TrackerDBInterface *iface,
                                      gint                n_pages,
                                      gpointer            user_data);

typedef enum {
	TRACKER_DB_INTERFACE_READONLY  = 1 << 0,
	TRACKER_DB_INTERFACE_IN_MEMORY = 1 << 2,
} TrackerDBInterfaceFlags;

typedef enum {
	TRACKER_OP_INSERT,
	TRACKER_OP_INSERT_FAILABLE,
	TRACKER_OP_DELETE,
	TRACKER_OP_RESET,
} TrackerPropertyOp;

TrackerDBInterface *tracker_db_interface_sqlite_new                    (const gchar              *filename,
                                                                        TrackerDBInterfaceFlags   flags,
                                                                        GError                  **error);
gint64              tracker_db_interface_sqlite_get_last_insert_id     (TrackerDBInterface       *interface);
gboolean            tracker_db_interface_sqlite_fts_init               (TrackerDBInterface       *interface,
                                                                        TrackerDBManagerFlags     fts_flags,
                                                                        GError                  **error);
void                tracker_db_interface_sqlite_reset_collator         (TrackerDBInterface       *interface);
gboolean            tracker_db_interface_sqlite_wal_checkpoint         (TrackerDBInterface       *interface,
                                                                        gboolean                  blocking,
                                                                        GError                  **error);
void                tracker_db_interface_init_vtabs                    (TrackerDBInterface       *interface);

gboolean            tracker_db_interface_attach_database               (TrackerDBInterface       *db_interface,
                                                                        GFile                    *file,
                                                                        const gchar              *name,
                                                                        GError                  **error);
gssize              tracker_db_interface_sqlite_release_memory         (TrackerDBInterface       *db_interface);

void                tracker_db_interface_ref_use   (TrackerDBInterface *db_interface);
gboolean            tracker_db_interface_unref_use (TrackerDBInterface *db_interface);

gboolean tracker_db_interface_found_corruption (TrackerDBInterface *db_interface);

gboolean tracker_db_statement_next_integer (TrackerDBStatement  *stmt,
                                            gboolean            *first,
                                            gint64              *value,
                                            GError             **error);

gboolean tracker_db_statement_next_string (TrackerDBStatement  *stmt,
                                           gboolean            *first,
                                           const char         **value,
                                           GError             **error);

G_END_DECLS
