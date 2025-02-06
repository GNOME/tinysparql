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

#include <glib-object.h>
#include <gio/gio.h>

#include "tracker-cursor.h"

#include "core/tracker-property.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_DB_INTERFACE           (tracker_db_interface_get_type ())
#define TRACKER_DB_INTERFACE(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_DB_INTERFACE, TrackerDBInterface))
#define TRACKER_DB_INTERFACE_CLASS(c)       (G_TYPE_CHECK_CLASS_CAST ((c),      TRACKER_TYPE_DB_INTERFACE, TrackerDBInterfaceClass))
#define TRACKER_IS_DB_INTERFACE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_DB_INTERFACE))
#define TRACKER_IS_DB_INTERFACE_CLASS(c)    (G_TYPE_CHECK_CLASS_TYPE ((o),      TRACKER_TYPE_DB_INTERFACE))
#define TRACKER_DB_INTERFACE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_DB_INTERFACE, TrackerDBInterfaceClass))

#define TRACKER_TYPE_DB_STATEMENT           (tracker_db_statement_get_type ())
#define TRACKER_DB_STATEMENT(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_DB_STATEMENT, TrackerDBStatement))
#define TRACKER_DB_STATEMENT_CLASS(c)       (G_TYPE_CHECK_CLASS_CAST ((c),      TRACKER_TYPE_DB_STATEMENT, TrackerDBStatementClass))
#define TRACKER_IS_DB_STATEMENT(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_DB_STATEMENT))
#define TRACKER_IS_DB_STATEMENT_CLASS(c)    (G_TYPE_CHECK_CLASS_TYPE ((o),      TRACKER_TYPE_DB_STATEMENT))
#define TRACKER_DB_STATEMENT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_DB_STATEMENT, TrackerDBStatementClass))

#define TRACKER_TYPE_DB_CURSOR              (tracker_db_cursor_get_type ())
#define TRACKER_DB_CURSOR(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_DB_CURSOR, TrackerDBCursor))
#define TRACKER_DB_CURSOR_CLASS(c)          (G_TYPE_CHECK_CLASS_CAST ((c),      TRACKER_TYPE_DB_CURSOR, TrackerDBCursorClass))
#define TRACKER_IS_DB_CURSOR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_DB_CURSOR))
#define TRACKER_IS_DB_CURSOR_CLASS(c)       (G_TYPE_CHECK_CLASS_TYPE ((o),      TRACKER_TYPE_DB_CURSOR))
#define TRACKER_DB_CURSOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj),  TRACKER_TYPE_DB_CURSOR, TrackerDBCursorClass))

#define TRACKER_DB_INTERFACE_ERROR          (tracker_db_interface_error_quark ())

typedef enum {
	TRACKER_DB_QUERY_ERROR,
	TRACKER_DB_INTERRUPTED,
	TRACKER_DB_OPEN_ERROR,
	TRACKER_DB_NO_SPACE,
	TRACKER_DB_CONSTRAINT,
	TRACKER_DB_CORRUPT,
} TrackerDBInterfaceError;

typedef enum {
	TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT,
	TRACKER_DB_STATEMENT_CACHE_TYPE_NONE
} TrackerDBStatementCacheType;

typedef struct TrackerDBInterface      TrackerDBInterface;
typedef struct TrackerDBInterfaceClass TrackerDBInterfaceClass;
typedef struct TrackerDBStatement      TrackerDBStatement;
typedef struct TrackerDBStatementClass TrackerDBStatementClass;
typedef struct TrackerDBCursor         TrackerDBCursor;
typedef struct TrackerDBCursorClass    TrackerDBCursorClass;
typedef struct TrackerDBStatementMru   TrackerDBStatementMru;

struct TrackerDBStatementMru {
	TrackerDBStatement *head;
	TrackerDBStatement *tail;
	GHashTable *stmts;
	guint size;
	guint max;
};

GQuark                  tracker_db_interface_error_quark             (void);

GType                   tracker_db_interface_get_type                (void);
GType                   tracker_db_statement_get_type                (void);
GType                   tracker_db_cursor_get_type                   (void);

void                    tracker_db_interface_set_max_stmt_cache_size (TrackerDBInterface         *db_interface,
                                                                      TrackerDBStatementCacheType cache_type,
                                                                      guint                       max_size);

/* User data functions, mainly to attach the data manager */
void                    tracker_db_interface_set_user_data           (TrackerDBInterface         *interface,
                                                                      GObject                    *user_data);

/* Functions to create queries/procedures */
TrackerDBStatement *    tracker_db_interface_create_statement        (TrackerDBInterface           *db_interface,
                                                                      TrackerDBStatementCacheType   cache_type,
                                                                      GError                      **error,
                                                                      const gchar                  *query);
TrackerDBStatement *    tracker_db_interface_create_vstatement       (TrackerDBInterface          *interface,
                                                                      TrackerDBStatementCacheType  cache_type,
                                                                      GError                     **error,
                                                                      const gchar                 *query,
                                                                      ...) G_GNUC_PRINTF (4, 5);
gboolean                tracker_db_interface_execute_vquery          (TrackerDBInterface          *interface,
                                                                      GError                     **error,
                                                                      const gchar                 *query,
                                                                      va_list                      args);
gboolean                tracker_db_interface_execute_query           (TrackerDBInterface          *interface,
                                                                      GError                     **error,
                                                                      const gchar                 *query,
                                                                      ...) G_GNUC_PRINTF (3, 4);

gboolean                tracker_db_interface_start_transaction       (TrackerDBInterface         *interface);
gboolean                tracker_db_interface_end_db_transaction      (TrackerDBInterface         *interface,
                                                                      GError                    **error);
gboolean                tracker_db_interface_get_is_used             (TrackerDBInterface         *interface);

/* Statements */
void                    tracker_db_statement_bind_double             (TrackerDBStatement         *stmt,
                                                                      int                         index,
                                                                      double                      value);
void                    tracker_db_statement_bind_int                (TrackerDBStatement         *stmt,
                                                                      int                         index,
                                                                      gint64                      value);
void                    tracker_db_statement_bind_null               (TrackerDBStatement         *stmt,
                                                                      int                         index);
void                    tracker_db_statement_bind_text               (TrackerDBStatement         *stmt,
                                                                      int                         index,
                                                                      const gchar                *value);
void                    tracker_db_statement_bind_bytes              (TrackerDBStatement         *stmt,
                                                                      int                         index,
                                                                      GBytes                     *value);
void                    tracker_db_statement_bind_value              (TrackerDBStatement         *stmt,
                                                                      int                         index,
								      const GValue               *value);
gboolean                tracker_db_statement_execute                 (TrackerDBStatement         *stmt,
                                                                      GError                    **error);
TrackerDBCursor *       tracker_db_statement_start_cursor            (TrackerDBStatement         *stmt,
                                                                      GError                    **error);
TrackerDBCursor *       tracker_db_statement_start_sparql_cursor     (TrackerDBStatement         *stmt,
                                                                      guint                       n_columns,
                                                                      GError                    **error);

/* Statement caches */
void tracker_db_statement_mru_init (TrackerDBStatementMru *mru,
                                    guint                  size,
                                    GHashFunc              hash_func,
                                    GEqualFunc             equal_func,
                                    GDestroyNotify         key_destroy);

void tracker_db_statement_mru_finish (TrackerDBStatementMru *mru);

void tracker_db_statement_mru_clear (TrackerDBStatementMru *mru);

TrackerDBStatement * tracker_db_statement_mru_lookup (TrackerDBStatementMru *mru,
                                                      gconstpointer          key);

void tracker_db_statement_mru_insert (TrackerDBStatementMru *mru,
                                      gpointer               key,
                                      TrackerDBStatement    *stmt);

void tracker_db_statement_mru_update (TrackerDBStatementMru *mru,
                                      TrackerDBStatement    *stmt);

void tracker_db_cursor_get_value (TrackerDBCursor            *cursor,
                                  guint                       column,
                                  GValue                     *value);

G_END_DECLS
