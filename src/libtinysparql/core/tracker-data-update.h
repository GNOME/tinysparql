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

#pragma once

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _TrackerData TrackerData;
typedef struct _TrackerDataClass TrackerDataClass;

#include "core/tracker-db-interface.h"
#include "core/tracker-data-manager.h"

#include "tracker-deserializer.h"

#define TRACKER_TYPE_DATA         (tracker_data_get_type ())
#define TRACKER_DATA(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DATA, TrackerData))
#define TRACKER_DATA_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_DATA, TrackerDataClass))
#define TRACKER_IS_DATA(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DATA))
#define TRACKER_IS_DATA_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_DATA))
#define TRACKER_DATA_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_DATA, TrackerDataClass))

typedef struct _TrackerData TrackerDataUpdate;

typedef enum {
	TRACKER_DATA_INSERT,
	TRACKER_DATA_DELETE,
} TrackerDataUpdateType;

typedef void (*TrackerStatementCallback) (TrackerDataUpdateType  type,
                                          const gchar           *graph,
                                          TrackerRowid           subject_id,
                                          TrackerRowid           predicate_id,
                                          TrackerRowid           object_id,
                                          GPtrArray             *rdf_types,
                                          gpointer               user_data);

typedef enum {
	TRACKER_DATA_ROLLBACK,
	TRACKER_DATA_COMMIT,
} TrackerDataTransactionType;

typedef void (*TrackerTransactionCallback) (TrackerDataTransactionType type,
                                            gpointer                   user_data);

typedef enum {
	TRACKER_SAVEPOINT_SET,
	TRACKER_SAVEPOINT_RELEASE,
	TRACKER_SAVEPOINT_ROLLBACK,
} TrackerSavepointOp;

GQuark   tracker_data_error_quark                   (void);

/* Metadata */
void     tracker_data_delete_statement              (TrackerData               *data,
                                                     const gchar               *graph,
                                                     TrackerRowid               subject,
                                                     TrackerProperty           *predicate,
                                                     const GValue              *object,
                                                     GError                   **error);
void     tracker_data_insert_statement              (TrackerData               *data,
                                                     const gchar               *graph,
                                                     TrackerRowid               subject,
                                                     TrackerProperty           *predicate,
                                                     const GValue              *object,
                                                     GError                   **error);
void     tracker_data_update_statement              (TrackerData               *data,
                                                     const gchar               *graph,
                                                     TrackerRowid               subject,
                                                     TrackerProperty           *predicate,
                                                     const GValue              *object,
                                                     GError                   **error);
gboolean tracker_data_begin_transaction             (TrackerData               *data,
                                                     GError                   **error);
gboolean tracker_data_begin_ontology_transaction    (TrackerData               *data,
                                                     GError                   **error);
gboolean tracker_data_commit_transaction            (TrackerData               *data,
                                                     GError                   **error);
void     tracker_data_rollback_transaction          (TrackerData               *data);
void     tracker_data_update_sparql                 (TrackerData               *data,
                                                     const gchar               *update,
                                                     GError                   **error);
GVariant *
         tracker_data_update_sparql_blank           (TrackerData               *data,
                                                     const gchar               *update,
                                                     GError                   **error);
void     tracker_data_update_buffer_flush           (TrackerData               *data,
                                                     GError                   **error);
void     tracker_data_update_buffer_might_flush     (TrackerData               *data,
                                                     GError                   **error);

gboolean tracker_data_load_from_deserializer        (TrackerData               *data,
                                                     TrackerDeserializer       *deserializer,
                                                     const gchar               *graph,
                                                     const gchar               *location,
                                                     GHashTable                *bnodes,
                                                     GError                   **error);

gboolean tracker_data_load_from_dbus_fd (TrackerData   *data,
                                         GInputStream  *istream,
                                         GHashTable    *bnodes,
                                         GCancellable  *cancellable,
                                         GError       **error);

void     tracker_data_load_rdf_file                 (TrackerData               *data,
                                                     GFile                     *file,
                                                     const gchar               *graph,
                                                     GError                   **error);

TrackerRowid tracker_data_ensure_graph              (TrackerData               *data,
                                                     const gchar               *name,
                                                     GError                   **error);
gboolean tracker_data_delete_graph                  (TrackerData               *data,
                                                     const gchar               *uri,
                                                     GError                   **error);

/* Calling back */
void tracker_data_add_statement_callback (TrackerData              *data,
                                          TrackerStatementCallback  callback,
                                          gpointer                  user_data);

void tracker_data_remove_statement_callback (TrackerData              *data,
                                             TrackerStatementCallback  callback,
                                             gpointer                  user_data);

void tracker_data_add_transaction_callback (TrackerData                *data,
                                            TrackerTransactionCallback  callback,
                                            gpointer                    user_data);

void tracker_data_remove_transaction_callback (TrackerData                *data,
                                               TrackerTransactionCallback  callback,
                                               gpointer                    user_data);

gboolean tracker_data_update_resource (TrackerData      *data,
                                       const gchar      *graph,
                                       TrackerResource  *resource,
                                       GHashTable       *bnodes,
                                       GHashTable       *visited,
                                       GError          **error);

TrackerRowid tracker_data_update_ensure_resource (TrackerData  *data,
                                                  const gchar  *uri,
                                                  GError      **error);
TrackerRowid tracker_data_generate_bnode (TrackerData  *data,
                                          GError      **error);

GTimeZone * tracker_data_get_time_zone (TrackerData *data);

gboolean tracker_data_savepoint (TrackerData         *data,
                                 TrackerSavepointOp   op,
                                 const char          *name,
                                 GError             **error);

GType         tracker_data_get_type (void) G_GNUC_CONST;
TrackerData * tracker_data_new      (TrackerDataManager *manager);

G_END_DECLS
