/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2007, Jason Kivlighn <jkivlighn@gmail.com>
 * Copyright (C) 2007, Creative Commons <http://creativecommons.org>
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

#ifndef __LIBTRACKER_DATA_MANAGER_H__
#define __LIBTRACKER_DATA_MANAGER_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _TrackerDataManager TrackerDataManager;
typedef struct _TrackerDataManagerClass TrackerDataManagerClass;

#include <libtracker-common/tracker-common.h>
#include <libtracker-sparql/tracker-ontologies.h>

#include "tracker-rowid.h"
#include "tracker-data-update.h"
#include "tracker-db-interface.h"
#include "tracker-db-manager.h"

#define TRACKER_DATA_ONTOLOGY_ERROR                  (tracker_data_ontology_error_quark ())

#define TRACKER_TYPE_DATA_MANAGER         (tracker_data_manager_get_type ())
#define TRACKER_DATA_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DATA_MANAGER, TrackerDataManager))
#define TRACKER_DATA_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_DATA_MANAGER, TrackerDataManagerClass))
#define TRACKER_IS_DATA_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DATA_MANAGER))
#define TRACKER_IS_DATA_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_DATA_MANAGER))
#define TRACKER_DATA_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_DATA_MANAGER, TrackerDataManagerClass))

typedef enum {
	TRACKER_DATA_UNSUPPORTED_ONTOLOGY_CHANGE,
	TRACKER_DATA_ONTOLOGY_NOT_FOUND,
	TRACKER_DATA_UNSUPPORTED_LOCATION
} TrackerDataOntologyError;

GType    tracker_data_manager_get_type               (void) G_GNUC_CONST;

GQuark   tracker_data_ontology_error_quark           (void);

TrackerDataManager * tracker_data_manager_new        (TrackerDBManagerFlags   flags,
                                                      GFile                  *cache_location,
                                                      GFile                  *ontology_location,
                                                      guint                   select_cache_size,
                                                      guint                   update_cache_size);

void                 tracker_data_manager_shutdown            (TrackerDataManager *manager);

GFile *              tracker_data_manager_get_cache_location  (TrackerDataManager *manager);
TrackerOntologies *  tracker_data_manager_get_ontologies      (TrackerDataManager *manager);

TrackerDBManager *   tracker_data_manager_get_db_manager      (TrackerDataManager *manager);
TrackerDBInterface * tracker_data_manager_get_db_interface    (TrackerDataManager  *manager,
                                                               GError             **error);
TrackerDBInterface * tracker_data_manager_get_writable_db_interface (TrackerDataManager *manager);
TrackerData *        tracker_data_manager_get_data            (TrackerDataManager *manager);

GHashTable *         tracker_data_manager_get_namespaces      (TrackerDataManager *manager);

gboolean             tracker_data_manager_create_graph (TrackerDataManager  *manager,
                                                        const gchar         *name,
                                                        GError             **error);

gboolean             tracker_data_manager_drop_graph (TrackerDataManager  *manager,
                                                      const gchar         *name,
                                                      GError             **error);

gboolean             tracker_data_manager_clear_graph (TrackerDataManager  *manager,
                                                       const gchar         *graph,
                                                       GError             **error);
gboolean             tracker_data_manager_copy_graph  (TrackerDataManager  *manager,
                                                       const gchar         *source,
                                                       const gchar         *destination,
                                                       GError             **error);

GHashTable *         tracker_data_manager_get_graphs       (TrackerDataManager *manager,
                                                            gboolean            in_transaction);

TrackerRowid         tracker_data_manager_find_graph       (TrackerDataManager *manager,
                                                            const gchar        *name,
                                                            gboolean            in_transaction);

guint                tracker_data_manager_get_generation   (TrackerDataManager *manager);
void                 tracker_data_manager_rollback_graphs (TrackerDataManager *manager);
void                 tracker_data_manager_commit_graphs (TrackerDataManager *manager);

void                 tracker_data_manager_release_memory (TrackerDataManager *manager);

gboolean             tracker_data_manager_expand_prefix (TrackerDataManager  *manager,
                                                         const gchar         *term,
                                                         GHashTable          *prefix_map,
                                                         gchar              **prefix,
                                                         gchar              **expanded);
TrackerSparqlConnection * tracker_data_manager_get_remote_connection (TrackerDataManager  *data_manager,
                                                                      const gchar         *uri,
                                                                      GError             **error);
void tracker_data_manager_map_connection (TrackerDataManager      *data_manager,
                                          const gchar             *handle_name,
                                          TrackerSparqlConnection *connection);

G_END_DECLS

#endif /* __LIBTRACKER_DATA_MANAGER_H__ */
