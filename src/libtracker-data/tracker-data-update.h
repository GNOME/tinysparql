/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __TRACKER_DATA_UPDATE_H__
#define __TRACKER_DATA_UPDATE_H__

#include <glib.h>

#include <libtracker-common/tracker-ontology.h>

#include <libtracker-db/tracker-db-interface.h>

G_BEGIN_DECLS

typedef enum  {
	TRACKER_DATA_ERROR_UNKNOWN_CLASS,
	TRACKER_DATA_ERROR_UNKNOWN_PROPERTY,
	TRACKER_DATA_ERROR_INVALID_TYPE
} TrackerDataError;

#define TRACKER_DATA_ERROR tracker_data_error_quark ()

GQuark   tracker_data_error_quark (void);

/* Services  */
guint32  tracker_data_insert_resource                   (const gchar         *uri);
void     tracker_data_delete_resource                   (const gchar         *uri);
gboolean tracker_data_update_resource_uri               (const gchar         *old_uri,
							 const gchar         *new_uri);
/* Metadata */
void     tracker_data_delete_resource_description       (const gchar         *uri);
void     tracker_data_delete_statement			(const gchar	     *subject,
							 const gchar         *predicate,
							 const gchar         *object);

void     tracker_data_insert_statement			(const gchar	     *subject,
							 const gchar         *predicate,
							 const gchar         *object,
							 GError             **error);
void     tracker_data_insert_statement_with_uri		(const gchar	     *subject,
							 const gchar         *predicate,
							 const gchar         *object,
							 GError             **error);
void     tracker_data_insert_statement_with_string	(const gchar	     *subject,
							 const gchar         *predicate,
							 const gchar         *object,
							 GError             **error);
void     tracker_data_begin_transaction			(void);
void     tracker_data_commit_transaction		(void);

void     tracker_data_update_sparql			(const gchar       *update,
							 GError	          **error);

gint64    tracker_data_get_modification_sequence         (void);
void      tracker_data_set_modification_sequence        (gint64 modseq);

/* Volume handling */
void tracker_data_update_enable_volume                  (const gchar         *udi,
                                                         const gchar         *mount_path);
void tracker_data_update_disable_volume                 (const gchar         *udi);
void tracker_data_update_disable_all_volumes            (void);
void tracker_data_update_reset_volume                   (const gchar         *uri);

/* Calling back */
typedef void (*TrackerStatementCallback)                (const gchar *subject, 
							 const gchar *predicate, 
							 const gchar *object,
							 GPtrArray   *rdf_types,
							 gpointer user_data);
typedef void (*TrackerCommitCallback)                   (gpointer user_data);

void tracker_data_set_insert_statement_callback         (TrackerStatementCallback callback,
							 gpointer                 user_data);
void tracker_data_set_delete_statement_callback         (TrackerStatementCallback callback,
							 gpointer                 user_data);
void tracker_data_set_commit_statement_callback         (TrackerCommitCallback    callback,
							 gpointer                 user_data);

G_END_DECLS

#endif /* __TRACKER_DATA_UPDATE_H__ */
