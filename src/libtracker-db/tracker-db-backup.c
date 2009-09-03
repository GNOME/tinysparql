/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia
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
 *
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <sqlite3.h>

#include <libtracker-common/tracker-ontology.h>
#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-interface-sqlite.h>

#include <tracker-fts/tracker-fts.h>

#include "tracker-db-backup.h"

#define TRACKER_DB_BACKUP_META_FILENAME_T	"meta-backup.db.tmp"

typedef struct {
	TrackerDBBackupFinished callback;
	GDestroyNotify destroy;
	gpointer user_data;
	GError *error;
	sqlite3_stmt *stmt;
	sqlite3 *db, *backup_temp;
	sqlite3_backup *backup_db;
	gchar *backup_fname;
	int result;
	GFile *parent;
} BackupInfo;

GQuark
tracker_db_backup_error_quark (void)
{
	return g_quark_from_static_string ("tracker-db-backup-error-quark");
}

static gboolean
perform_callback (gpointer user_data)
{
	BackupInfo *info = user_data;

	if (info->callback) {
		info->callback (info->error, info->user_data);
	}

	return FALSE;
}

static void
backup_info_free (gpointer user_data)
{
	BackupInfo *info = user_data;

	if (info->parent) {
		g_object_unref (info->parent);
	}

	if (info->destroy) {
		info->destroy (info->user_data);
	}

	g_clear_error (&info->error);

	if (info->stmt) {
		sqlite3_finalize (info->stmt);
	}

	if (info->backup_db) {
		sqlite3_backup_finish (info->backup_db);
		info->backup_db = NULL;
	}

	if (info->backup_temp) {
		sqlite3_close (info->backup_temp);
	}

	if (info->db) {
		sqlite3_close (info->db);
	}

	if (info->backup_fname) {
		g_free (info->backup_fname);
	}

	g_free (info);
}


static gboolean
backup_file_step (gpointer user_data)
{
	BackupInfo *info = user_data;
	guint cnt = 0;
	gboolean cont = TRUE;

	while (cont && info->result == SQLITE_OK) {

		info->result = sqlite3_backup_step(info->backup_db, 5);

		switch (info->result) {
			case SQLITE_OK:
			break;

			case SQLITE_ERROR:
			default:
			cont = FALSE;
			break;
		}

		if (cnt > 100) {
			break;
		}

		cnt++;
	}

	return cont;
}

static void
on_backup_temp_finished (gpointer user_data)
{
	BackupInfo *info = user_data;

	if (info->backup_db) {
		sqlite3_backup_finish (info->backup_db);
		info->backup_db = NULL;
	}

	if (info->db) {
		sqlite3_close (info->db);
		info->db = NULL;
	}


	if (!info->error && info->result != SQLITE_DONE) {
		g_set_error (&info->error, TRACKER_DB_BACKUP_ERROR, 
		             TRACKER_DB_BACKUP_ERROR_UNKNOWN,
		             "%s", sqlite3_errmsg (info->backup_temp));
	}

	if (!info->error) {
		GFile *from_file, *to_file;

		from_file = g_file_get_child (info->parent, TRACKER_DB_BACKUP_META_FILENAME_T);
		to_file = g_file_get_child (info->parent, TRACKER_DB_BACKUP_META_FILENAME);

		g_file_move (from_file, to_file, 
			     G_FILE_COPY_OVERWRITE, 
			     NULL, NULL, NULL,
			     &info->error);

		g_object_unref (from_file);
		g_object_unref (to_file);
	}

	perform_callback (info);

	backup_info_free (info);

	return;
}

GFile *
tracker_db_backup_file (GFile **parent_out, const gchar *type)
{
	GFile *file, *parent;
	gchar *parent_path;

	parent_path = g_build_filename (g_get_user_data_dir (),
	                                "tracker",
	                                "data",
	                                NULL);

	parent = g_file_new_for_path (parent_path);
	file = g_file_get_child (parent, type);

	if (parent_out) {
		*parent_out = parent;
	} else {
		g_object_unref (parent);
	}

	g_free (parent_path);

	return file;
}


void
tracker_db_backup_save (TrackerDBBackupFinished callback,
                        gpointer user_data,
                        GDestroyNotify destroy)
{
	const gchar *db_file = tracker_db_manager_get_file (TRACKER_DB_METADATA);
	BackupInfo *info = g_new0 (BackupInfo, 1);
	GFile *file;

	/* This procedure makes a meta-backup.db using sqlite3_backup API */

	info->callback = callback;
	info->user_data = user_data;
	info->destroy = destroy;

	file = tracker_db_backup_file (&info->parent, TRACKER_DB_BACKUP_META_FILENAME_T);
	info->backup_fname = g_file_get_path (file);
	g_object_unref (file);

	if (sqlite3_open_v2 (db_file, &info->db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
		g_set_error (&info->error, TRACKER_DB_BACKUP_ERROR, TRACKER_DB_BACKUP_ERROR_UNKNOWN,
		             "Could not open sqlite3 database:'%s'", db_file);

		g_idle_add_full (G_PRIORITY_DEFAULT, perform_callback, info, 
		                 backup_info_free);

		return;
	}

	if (sqlite3_open (info->backup_fname, &info->backup_temp) != SQLITE_OK) {
		g_set_error (&info->error, TRACKER_DB_BACKUP_ERROR, TRACKER_DB_BACKUP_ERROR_UNKNOWN,
		             "Could not open sqlite3 database:'%s'", info->backup_fname);

		g_idle_add_full (G_PRIORITY_DEFAULT, perform_callback, info, 
		                 backup_info_free);

		return;
	}

	info->backup_db = sqlite3_backup_init (info->backup_temp, "main", 
	                                       info->db, "main");

	if (!info->backup_db) {
		g_set_error (&info->error, TRACKER_DB_BACKUP_ERROR, TRACKER_DB_BACKUP_ERROR_UNKNOWN,
		             "Unknown error creating backup db: '%s'", info->backup_fname);

		g_idle_add_full (G_PRIORITY_DEFAULT, perform_callback, info, 
		                 backup_info_free);

		return;
	}

	g_idle_add_full (G_PRIORITY_DEFAULT, backup_file_step, info, 
	                 on_backup_temp_finished);
}


void
tracker_db_backup_sync_fts (void)
{
	TrackerProperty   **properties, **property;
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBCursor    *cursor;
	TrackerClass       *prop_class;
	gchar              *query;

	iface = tracker_db_manager_get_db_interface ();

	query = tracker_fts_get_drop_fts_table_query ();
	tracker_db_interface_execute_query (iface, NULL, "%s", query);
	g_free (query);

	query = tracker_fts_get_create_fts_table_query ();
	tracker_db_interface_execute_query (iface, NULL, "%s", query);
	g_free (query);

	properties = tracker_ontology_get_properties ();
	for (property = properties; *property; property++) {
		if (tracker_property_get_data_type (*property) == TRACKER_PROPERTY_TYPE_STRING &&
		    tracker_property_get_fulltext_indexed (*property)) {

			prop_class  = tracker_property_get_domain (*property);

			if (tracker_property_get_multiple_values (*property)) {
				query = g_strdup_printf ("SELECT ID, \"%s\" FROM \"%s_%s\"", 
				                         tracker_property_get_name (*property), 
				                         tracker_class_get_name (prop_class),
				                         tracker_property_get_name (*property));
			} else {
				query = g_strdup_printf ("SELECT ID, \"%s\" FROM \"%s\" WHERE \"%s\" IS NOT NULL", 
				                         tracker_property_get_name (*property), 
				                         tracker_class_get_name (prop_class),
				                         tracker_property_get_name (*property));
			}

			stmt = tracker_db_interface_create_statement (iface, "%s", query);
			cursor = tracker_db_statement_start_cursor (stmt, NULL);
			g_object_unref (stmt);

			if (cursor) {
				while (tracker_db_cursor_iter_next (cursor)) {
					guint32 id;
					const gchar *text;

					id = tracker_db_cursor_get_int (cursor, 0);
					text = tracker_db_cursor_get_string (cursor, 1);

					// TODO we need to retrieve all existing (FTS indexed) property values for
					// this resource to properly support incremental FTS updates
					// (like calling deleteTerms and then calling insertTerms)

					tracker_fts_update_init (id);
					tracker_fts_update_text (id, 0,  text);
				}

				g_object_unref (cursor);
			}

			g_free (query);
		}
	}
	g_free (properties);

	tracker_fts_update_commit ();
}
