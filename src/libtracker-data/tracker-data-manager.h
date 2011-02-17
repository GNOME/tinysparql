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
#include <libtracker-data/tracker-db-interface.h>
#include <libtracker-data/tracker-db-manager.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_DATA_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-data/tracker-data.h> must be included directly."
#endif

#define TRACKER_DATA_ONTOLOGY_ERROR                  (tracker_data_ontology_error_quark ())

typedef enum {
	TRACKER_DATA_UNSUPPORTED_ONTOLOGY_CHANGE
} TrackerDataOntologyError;

GQuark   tracker_data_ontology_error_quark           (void);
gboolean tracker_data_manager_init                   (TrackerDBManagerFlags   flags,
                                                      const gchar           **test_schema,
                                                      gboolean               *first_time,
                                                      gboolean                journal_check,
                                                      guint                   select_cache_size,
                                                      guint                   update_cache_size,
                                                      TrackerBusyCallback     busy_callback,
                                                      gpointer                busy_user_data,
                                                      const gchar            *busy_operation,
                                                      GError                **error);

void     tracker_data_manager_init_async             (TrackerDBManagerFlags   flags,
                                                      const gchar           **test_schema,
                                                      gboolean                journal_check,
                                                      guint                   select_cache_size,
                                                      guint                   update_cache_size,
                                                      TrackerBusyCallback     busy_callback,
                                                      gpointer                busy_user_data,
                                                      const gchar            *busy_operation,
                                                      GAsyncReadyCallback     callback,
                                                      gpointer                user_data);
gboolean tracker_data_manager_init_finish            (GAsyncResult           *result,
                                                      GError                **error);

void     tracker_data_manager_shutdown               (void);
gboolean tracker_data_manager_reload                 (TrackerBusyCallback     busy_callback,
                                                      gpointer                busy_user_data,
                                                      const gchar            *busy_operation,
                                                      GError                **error);

G_END_DECLS

#endif /* __LIBTRACKER_DATA_MANAGER_H__ */
