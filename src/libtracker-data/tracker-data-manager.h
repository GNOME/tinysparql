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

#include <libtracker-common/tracker-language.h>
#include <libtracker-common/tracker-ontologies.h>

#include <libtracker-data/tracker-data-update.h>

#include <libtracker-db/tracker-db-interface.h>
#include <libtracker-db/tracker-db-manager.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_DATA_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-data/tracker-data.h> must be included directly."
#endif

gboolean tracker_data_manager_init                   (TrackerDBManagerFlags  flags,
                                                      const gchar          **test_schema,
                                                      gboolean              *first_time,
                                                      gboolean               journal_check,
                                                      TrackerBusyCallback    busy_callback,
                                                      gpointer               busy_user_data,
                                                      const gchar           *busy_status);
void     tracker_data_manager_shutdown               (void);
gboolean tracker_data_manager_interrupt_thread       (GThread               *thread);
void     tracker_data_manager_interrupt_thread_reset (GThread               *thread);

gint64   tracker_data_manager_get_db_option_int64    (const gchar           *option);
void     tracker_data_manager_set_db_option_int64    (const gchar           *option,
                                                      gint64                 value);
void     tracker_data_ontology_load_statement        (const gchar           *ontology_file,
                                                      gint                   subject_id,
                                                      const gchar           *subject,
                                                      const gchar           *predicate,
                                                      const gchar           *object,
                                                      gint                  *max_id,
                                                      gboolean               in_update,
                                                      GHashTable            *classes,
                                                      GHashTable            *properties,
                                                      GPtrArray             *seen_classes,
                                                      GPtrArray             *seen_properties);
void     tracker_data_ontology_import_into_db        (gboolean               is_new);
void     tracker_data_ontology_process_statement     (const gchar           *graph,
                                                      const gchar           *subject,
                                                      const gchar           *predicate,
                                                      const gchar           *object,
                                                      gboolean               is_uri,
                                                      gboolean               in_update,
                                                      gboolean               ignore_nao_last_modified);
void    tracker_data_ontology_import_finished        (void);
void    tracker_data_ontology_process_changes        (GPtrArray             *seen_classes,
                                                      GPtrArray             *seen_properties);
void    tracker_data_ontology_free_seen              (GPtrArray             *seen);

G_END_DECLS

#endif /* __LIBTRACKER_DATA_MANAGER_H__ */
