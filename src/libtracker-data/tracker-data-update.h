/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#ifndef __LIBTRACKER_DATA_UPDATE_H__
#define __LIBTRACKER_DATA_UPDATE_H__

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-ontologies.h>

#include "tracker-db-interface.h"

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_DATA_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-data/tracker-data.h> must be included directly."
#endif

typedef void (*TrackerStatementCallback) (gint         graph_id,
                                          const gchar *graph,
                                          gint         subject_id,
                                          const gchar *subject,
                                          gint         predicate_id,
                                          gint         object_id,
                                          const gchar *object,
                                          GPtrArray   *rdf_types,
                                          gpointer     user_data);
typedef void (*TrackerCommitCallback)    (gboolean     start_timer,
                                          gpointer     user_data);

GQuark   tracker_data_error_quark                   (void);

/* Metadata */
void     tracker_data_delete_resource_description   (const gchar               *graph,
                                                     const gchar               *url,
                                                     GError                   **error);
void     tracker_data_delete_statement              (const gchar               *graph,
                                                     const gchar               *subject,
                                                     const gchar               *predicate,
                                                     const gchar               *object,
                                                     GError                   **error);
void     tracker_data_insert_statement              (const gchar               *graph,
                                                     const gchar               *subject,
                                                     const gchar               *predicate,
                                                     const gchar               *object,
                                                     GError                   **error);
void     tracker_data_insert_statement_with_uri     (const gchar               *graph,
                                                     const gchar               *subject,
                                                     const gchar               *predicate,
                                                     const gchar               *object,
                                                     GError                   **error);
void     tracker_data_insert_statement_with_string  (const gchar               *graph,
                                                     const gchar               *subject,
                                                     const gchar               *predicate,
                                                     const gchar               *object,
                                                     GError                   **error);
void     tracker_data_begin_transaction             (GError                   **error);
void     tracker_data_begin_ontology_transaction    (GError                   **error);
void     tracker_data_begin_transaction_for_replay  (time_t                     time,
                                                     GError                   **error);
void     tracker_data_commit_transaction            (GError                   **error);
void     tracker_data_notify_transaction            (gboolean                   start_timer);
void     tracker_data_rollback_transaction          (void);
void     tracker_data_update_sparql                 (const gchar               *update,
                                                     GError                   **error);
GPtrArray *
         tracker_data_update_sparql_blank           (const gchar               *update,
                                                     GError                   **error);
void     tracker_data_update_buffer_flush           (GError                   **error);
void     tracker_data_update_buffer_might_flush     (GError                   **error);
void     tracker_data_load_turtle_file              (GFile                     *file,
                                                     GError                   **error);

void     tracker_data_sync                          (void);
void     tracker_data_replay_journal                (TrackerBusyCallback        busy_callback,
                                                     gpointer                   busy_user_data,
                                                     const gchar               *busy_status);

/* Calling back */
void     tracker_data_add_insert_statement_callback      (TrackerStatementCallback   callback,
                                                          gpointer                   user_data);
void     tracker_data_add_delete_statement_callback      (TrackerStatementCallback   callback,
                                                          gpointer                   user_data);
void     tracker_data_add_commit_statement_callback      (TrackerCommitCallback      callback,
                                                          gpointer                   user_data);
void     tracker_data_add_rollback_statement_callback    (TrackerCommitCallback      callback,
                                                          gpointer                   user_data);
void     tracker_data_remove_insert_statement_callback   (TrackerStatementCallback   callback,
                                                          gpointer                   user_data);
void     tracker_data_remove_delete_statement_callback   (TrackerStatementCallback   callback,
                                                          gpointer                   user_data);
void     tracker_data_remove_commit_statement_callback   (TrackerCommitCallback      callback,
                                                          gpointer                   user_data);
void     tracker_data_remove_rollback_statement_callback (TrackerCommitCallback      callback,
                                                          gpointer                   user_data);

void     tracker_data_update_shutdown                 (void);
#define  tracker_data_update_init                     tracker_data_update_shutdown

G_END_DECLS

#endif /* __LIBTRACKER_DATA_UPDATE_H__ */
