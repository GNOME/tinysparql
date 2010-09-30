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

#ifndef __LIBTRACKER_DB_INTERFACE_H__
#define __LIBTRACKER_DB_INTERFACE_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-property.h"

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_DATA_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-data/tracker-data.h> must be included directly."
#endif

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

#define TRACKER_TYPE_DB_RESULT_SET          (tracker_db_result_set_get_type ())
#define TRACKER_DB_RESULT_SET(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DB_RESULT_SET, TrackerDBResultSet))
#define TRACKER_DB_RESULT_SET_CLASS(c)      (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_DB_RESULT_SET, TrackerDBResultSetClass))
#define TRACKER_IS_DB_RESULT_SET(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DB_RESULT_SET))
#define TRACKER_IS_DB_RESULT_SET_CLASS(c)   (G_TYPE_CHECK_CLASS_TYPE ((o),    TRACKER_TYPE_DB_RESULT_SET))
#define TRACKER_DB_RESULT_SET_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_DB_RESULT_SET, TrackerDBResultSetClass))

#define TRACKER_DB_INTERFACE_ERROR          (tracker_db_interface_error_quark ())

typedef void (*TrackerBusyCallback)      (const gchar *status,
                                          gdouble      progress,
                                          gpointer     user_data);

typedef enum {
	TRACKER_DB_QUERY_ERROR,
	TRACKER_DB_CORRUPT,
	TRACKER_DB_INTERRUPTED
} TrackerDBInterfaceError;

typedef enum {
	TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT,
	TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE,
	TRACKER_DB_STATEMENT_CACHE_TYPE_NONE
} TrackerDBStatementCacheType;

typedef struct TrackerDBInterface      TrackerDBInterface;
typedef struct TrackerDBInterfaceClass TrackerDBInterfaceClass;
typedef struct TrackerDBStatement      TrackerDBStatement;
typedef struct TrackerDBStatementClass TrackerDBStatementClass;
typedef struct TrackerDBResultSet      TrackerDBResultSet;
typedef struct TrackerDBResultSetClass TrackerDBResultSetClass;
typedef struct TrackerDBCursor         TrackerDBCursor;
typedef struct TrackerDBCursorClass    TrackerDBCursorClass;
typedef struct TrackerDBResultSetPrivate TrackerDBResultSetPrivate;

GQuark                  tracker_db_interface_error_quark             (void);

GType                   tracker_db_interface_get_type                (void);
GType                   tracker_db_statement_get_type                (void);
GType                   tracker_db_cursor_get_type                   (void);
GType                   tracker_db_result_set_get_type               (void);

void                    tracker_db_interface_set_max_stmt_cache_size (TrackerDBInterface         *db_interface,
                                                                      TrackerDBStatementCacheType cache_type,
                                                                      guint                       max_size);

/* Functions to create queries/procedures */
TrackerDBStatement *    tracker_db_interface_create_statement        (TrackerDBInterface          *interface,
                                                                      TrackerDBStatementCacheType  cache_type,
                                                                      GError                     **error,
                                                                      const gchar                 *query,
                                                                      ...) G_GNUC_PRINTF (4, 5);
TrackerDBResultSet *    tracker_db_interface_execute_vquery          (TrackerDBInterface          *interface,
                                                                      GError                     **error,
                                                                      const gchar                 *query,
                                                                      va_list                      args);
TrackerDBResultSet *    tracker_db_interface_execute_query           (TrackerDBInterface          *interface,
                                                                      GError                     **error,
                                                                      const gchar                 *query,
                                                                       ...) G_GNUC_PRINTF (3, 4);

gboolean                tracker_db_interface_start_transaction       (TrackerDBInterface         *interface);
gboolean                tracker_db_interface_end_db_transaction      (TrackerDBInterface         *interface);
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
TrackerDBResultSet *    tracker_db_statement_execute                 (TrackerDBStatement         *stmt,
                                                                      GError                    **error);
TrackerDBCursor *       tracker_db_statement_start_cursor            (TrackerDBStatement         *stmt,
                                                                      GError                    **error);
TrackerDBCursor *       tracker_db_statement_start_sparql_cursor     (TrackerDBStatement         *stmt,
                                                                      TrackerPropertyType        *types,
                                                                      gint                        n_types,
                                                                      const gchar               **variable_names,
                                                                      gint                        n_variable_names,
                                                                      GError                    **error);
void                    tracker_db_interface_set_busy_handler        (TrackerDBInterface         *db_interface,
                                                                      TrackerBusyCallback         busy_callback,
                                                                      const gchar                *busy_status,
                                                                      gpointer                    busy_user_data);

/* Semi private TrackerDBResultSet functions */
TrackerDBResultSet *    _tracker_db_result_set_new                   (guint                       cols);
void                    _tracker_db_result_set_append                (TrackerDBResultSet         *result_set);
void                    _tracker_db_result_set_set_value             (TrackerDBResultSet         *result_set,
                                                                      guint                       column,
                                                                      const GValue               *value);
void                    _tracker_db_result_set_get_value             (TrackerDBResultSet         *result_set,
                                                                      guint                       column,
                                                                      GValue                     *value);

/* Functions to deal with the resultset */
void                    tracker_db_result_set_get                    (TrackerDBResultSet         *result_set,
                                                                      ...);
void                    tracker_db_result_set_rewind                 (TrackerDBResultSet         *result_set);
gboolean                tracker_db_result_set_iter_next              (TrackerDBResultSet         *result_set);
guint                   tracker_db_result_set_get_n_columns          (TrackerDBResultSet         *result_set);
guint                   tracker_db_result_set_get_n_rows             (TrackerDBResultSet         *result_set);

/* Functions to deal with a cursor */
void                    tracker_db_cursor_rewind                     (TrackerDBCursor            *cursor);
gboolean                tracker_db_cursor_iter_next                  (TrackerDBCursor            *cursor,
                                                                      GCancellable               *cancellable,
                                                                      GError                    **error);
guint                   tracker_db_cursor_get_n_columns              (TrackerDBCursor            *cursor);
const gchar*            tracker_db_cursor_get_variable_name          (TrackerDBCursor            *cursor,
                                                                      guint                       column);
TrackerSparqlValueType  tracker_db_cursor_get_value_type             (TrackerDBCursor            *cursor,
                                                                      guint                       column);
void                    tracker_db_cursor_get_value                  (TrackerDBCursor            *cursor,
                                                                      guint                       column,
                                                                      GValue                     *value);
const gchar*            tracker_db_cursor_get_string                 (TrackerDBCursor            *cursor,
                                                                      guint                       column,
                                                                      glong                      *length);
gint64                  tracker_db_cursor_get_int                    (TrackerDBCursor            *cursor,
                                                                      guint                       column);
gdouble                 tracker_db_cursor_get_double                 (TrackerDBCursor            *cursor,
                                                                      guint                       column);

G_END_DECLS

#endif /* __LIBTRACKER_DB_INTERFACE_H__ */
